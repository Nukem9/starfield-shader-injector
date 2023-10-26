#include <xbyak/xbyak.h>
#include <d3d12.h>
#include "D3DShaderReplacement.h"
#include "DebuggingUtil.h"
#include "CRHooks.h"
#include "Plugin.h"

namespace CRHooks
{
	struct TrackedDataEntry
	{
		CreationRenderer::TechniqueData *Technique;
		D3D12PipelineStateStream::Copy StreamCopy;
	};

	std::mutex TrackedShaderDataLock;
	std::vector<TrackedDataEntry> TrackedPipelineData;
	std::unordered_map<uint64_t, Microsoft::WRL::ComPtr<ID3D12RootSignature>> TrackedTechniqueIdToRootSignature;

	void LiveUpdateFilesystemWatcherThread(Microsoft::WRL::ComPtr<ID3D12Device2> Device)
	{
		const auto changeHandle = FindFirstChangeNotificationW(D3DShaderReplacement::GetShaderBinDirectory().c_str(), true, FILE_NOTIFY_CHANGE_LAST_WRITE);

		if (changeHandle == INVALID_HANDLE_VALUE)
		{
			spdlog::error("Live update: FindFirstChangeNotification failed with error code {:X}.", GetLastError());
			return;
		}

		spdlog::info("Live update: Initialized.");

		while (true)
		{
			const auto status = WaitForSingleObject(changeHandle, INFINITE);

			if (status != WAIT_OBJECT_0)
				break;

			// Update all known shaders in the directory. The loop might run multiple times if multiple files are
			// changed but that's okay.
			TrackedShaderDataLock.lock();
			{
				size_t patchCounter = 0;

				for (auto& data : TrackedPipelineData)
				{
					const bool newPipelineRequired = D3DShaderReplacement::PatchPipelineStateStream(data.StreamCopy, Device.Get(), nullptr, data.Technique->m_Name, data.Technique->m_Id);
					ID3D12PipelineState *pipelineState = nullptr;

					if (!newPipelineRequired)
						continue;

					if (auto hr = Device->CreatePipelineState(data.StreamCopy.GetDesc(), IID_PPV_ARGS(&pipelineState)); FAILED(hr))
					{
						spdlog::error("Live update: Failed to compile pipeline: {:X}. Shader technique: {:X}.", static_cast<uint32_t>(hr), data.Technique->m_Id);
						continue;
					}

					DebuggingUtil::SetObjectDebugName(pipelineState, data.Technique->m_Name);

					// Don't need pipelineState->AddRef() since we're swapping pointers. Luckily for us, the game keeps
					// exactly 1 reference to the old state so we don't have to fix mismatched reference counts either.
					//
					// WARNING: This'll never be thread safe. It's meant as a developer tool, not for production.
					auto old = InterlockedExchangePointer(reinterpret_cast<void **>(&data.Technique->m_PipelineState), pipelineState);
					(void)old;
					//static_cast<ID3D12PipelineState *>(old)->Release(); -- HACK: Not stable. Leaks memory for now.

					patchCounter++;
				}

				if (patchCounter > 0)
					spdlog::info("Live update: Created pipelines for {} technique(s).", patchCounter);
			}
			TrackedShaderDataLock.unlock();

			FindNextChangeNotification(changeHandle);
		}

		FindCloseChangeNotification(changeHandle);
	}

	void TrackCompiledTechnique(
		Microsoft::WRL::ComPtr<ID3D12Device2> Device,
		CreationRenderer::TechniqueData *Technique,
		D3D12PipelineStateStream::Copy&& StreamCopy,
		bool WasPatchedUpfront)
	{
		// Root signature override has to be tracked
		if (WasPatchedUpfront)
		{
			for (D3D12PipelineStateStream::Iter iter(StreamCopy.GetDesc()); !iter.AtEnd(); iter.Advance())
			{
				switch (auto obj = iter.GetObj(); obj->Type)
				{
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
					std::scoped_lock lock(TrackedShaderDataLock);
					TrackedTechniqueIdToRootSignature.emplace(Technique->m_Id, obj->RootSignature);
					break;
				}
			}
		}

		if (Plugin::AllowLiveUpdates)
		{
			static bool once = [&]()
			{
				std::thread t(LiveUpdateFilesystemWatcherThread, Device);
				t.detach();

				return true;
			}();

			std::scoped_lock lock(TrackedShaderDataLock);
			TrackedPipelineData.emplace_back(TrackedDataEntry
			{
				.Technique = Technique,
				.StreamCopy = std::move(StreamCopy),
			});
		}
	}

	bool OverridePipelineLayoutDx12(
		ID3D12GraphicsCommandList4 *CommandList,
		CreationRenderer::PipelineLayoutDx12 *CurrentLayout,
		CreationRenderer::PipelineLayoutDx12 *TargetLayout,
		CreationRenderer::TechniqueData **CurrentTech,
		CreationRenderer::TechniqueData **TargetTech)
	{
		//
		// Return false when absolutely nothing has changed.
		// Return true when a new root signature is required. The command list MUST be updated before returning.
		//
		// Vanilla game code uses the following logic:
		// if (CurrentLayout == TargetLayout)
		//	return false;
		//
		bool updateRequired = CurrentLayout != TargetLayout;
		auto rootSignature = TargetLayout->m_RootSignature;

		// If the target technique requires an override OR the previous technique was overridden, force a flush
		if (auto itr = TrackedTechniqueIdToRootSignature.find((*TargetTech)->m_Id); itr != TrackedTechniqueIdToRootSignature.end())
		{
			updateRequired = true;
			rootSignature = itr->second.Get();
		}
		else if (!updateRequired && CurrentTech)
		{
			updateRequired = TrackedTechniqueIdToRootSignature.contains((*CurrentTech)->m_Id);
		}

		if (updateRequired)
		{
			const auto type = *reinterpret_cast<CreationRenderer::ShaderType *>(reinterpret_cast<uintptr_t>(TargetLayout->m_LayoutConfigurationData) + 0x4);

			switch (type)
			{
			case CreationRenderer::ShaderType::Graphics:
				CommandList->SetGraphicsRootSignature(rootSignature);
				break;

			case CreationRenderer::ShaderType::Compute:
			case CreationRenderer::ShaderType::RayTracing:
				CommandList->SetComputeRootSignature(rootSignature);
				break;
			}
		}

		return updateRequired;
	}

	class SetPipelineLayoutDx12HookGen : Xbyak::CodeGenerator
	{
	private:
		const uintptr_t m_TargetAddress;

	public:
		SetPipelineLayoutDx12HookGen(uintptr_t TargetAddress) : m_TargetAddress(TargetAddress)
		{
			Xbyak::Label emulateSetNewSignature;

			lea(r9, ptr[rsi + 0x8]);
			mov(ptr[rsp + 0x20], r9);	// a5: Target Technique**
			mov(r9, r15);				// a4: Current Technique**
			mov(r8, r13);				// a3: Target PipelineLayoutDx12
			mov(rdx, ptr[rcx + 0x18]);	// a2: Current PipelineLayoutDx12
			mov(rcx, ptr[r14 + 0x10]);	// a1: ID3D12GraphicsCommandList
			mov(rax, reinterpret_cast<uintptr_t>(&OverridePipelineLayoutDx12));
			call(rax);

			test(al, al);
			jnz(emulateSetNewSignature);

			// Run the original code
			mov(rax, m_TargetAddress + 0x73);
			jmp(rax);

			// New signature required. OverridePipelineLayoutDx12() is expected to pass a signature to the D3D12 API
			// before we get here. This bypasses Starfield's calls to ID3D12CommandList::SetXXXRootSignature().
			L(emulateSetNewSignature);
			mov(rax, m_TargetAddress + 0x60);
			jmp(rax);
		}

		void Patch()
		{
			Hooks::WriteJump(m_TargetAddress, getCode());
		}
	};

	DECLARE_HOOK_TRANSACTION(CRHooks)
	{
		static SetPipelineLayoutDx12HookGen setPipelineLayoutDx12Hook(Offsets::Signature("4C 39 69 18 74 6D 41 8B C8 83 E9 01 74 41 83 E9 01 74 29 83 F9 01 74 24 41 8B C8 83 E9 01 74 40"));
		setPipelineLayoutDx12Hook.Patch();
	};
}
