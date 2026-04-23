#include <xbyak/xbyak.h>
#include "D3DShaderReplacement.h"
#include "DebuggingUtil.h"
#include "CRHooks.h"
#include "Plugin.h"
#include "ReShadeHelper.h"

namespace CRHooks
{
	struct TrackedDataEntry
	{
		CreationRenderer::TechniqueData *Technique;
		D3DPipelineStateStream::Copy StreamCopy;
	};

	std::mutex TrackedShaderDataLock;
	std::vector<TrackedDataEntry> TrackedPipelineData;
	std::unordered_map<uint64_t, CComPtr<ID3D12RootSignature>> TrackedTechniqueIdToRootSignature;

	void LiveUpdateFilesystemWatcherThread(CComPtr<ID3D12Device2> Device)
	{
		const auto changeHandle = FindFirstChangeNotificationW(
			D3DShaderReplacement::GetShaderBinDirectory().c_str(),
			true,
			FILE_NOTIFY_CHANGE_LAST_WRITE);

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
					const bool newPipelineRequired = D3DShaderReplacement::PatchPipelineStateStream(
						data.StreamCopy,
						Device.Get(),
						nullptr,
						data.Technique->m_Name,
						data.Technique->m_Id);

					if (!newPipelineRequired)
						continue;

					CComPtr<ID3D12PipelineState> pipelineState;
					if (auto hr = Device->CreatePipelineState(data.StreamCopy.GetDesc(), IID_PPV_ARGS(&pipelineState)); FAILED(hr))
					{
						spdlog::error(
							"Live update: Failed to compile pipeline: {:X}. Shader technique: {:X}.",
							static_cast<uint32_t>(hr),
							data.Technique->m_Id);

						continue;
					}

					DebuggingUtil::SetObjectDebugName(pipelineState.Get(), data.Technique->m_Name);

					// pipelineState->AddRef() is needed due to CComPtr's destructor. Luckily for us, the game keeps
					// exactly 1 reference to the old state so we don't have to fix mismatched reference counts.
					//
					// WARNING: This'll never be thread safe. It's meant as a developer tool, not for production.
					//
					// HACK: oldValue is never released. It's not stable and leaks memory for now.
					pipelineState->AddRef();

					auto targetPointer = reinterpret_cast<void **>(&data.Technique->m_PipelineState);
					auto oldValue = InterlockedExchangePointer(targetPointer, pipelineState.Get());
					(void)oldValue; // ->Release();

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

	void TrackDevice(CComPtr<ID3D12Device2> Device)
	{
		const static bool once = [&]
		{
			if (Plugin::AllowLiveUpdates)
				std::thread(LiveUpdateFilesystemWatcherThread, Device).detach();

			ReShadeHelper::Initialize();
			return true;
		}();
	}

	void TrackCompiledTechnique(
		CComPtr<ID3D12Device2> Device,
		CreationRenderer::TechniqueData *Technique,
		D3DPipelineStateStream::Copy&& StreamCopy,
		bool WasPatchedUpfront)
	{
		// Root signature override has to be tracked
		if (WasPatchedUpfront)
		{
			for (D3DPipelineStateStream::Iterator iter(StreamCopy.GetDesc()); !iter.AtEnd(); iter.Advance())
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
			std::scoped_lock lock(TrackedShaderDataLock);
			TrackedPipelineData.emplace_back(TrackedDataEntry {
				.Technique = Technique,
				.StreamCopy = std::move(StreamCopy),
			});
		}
	}

	bool OverridePipelineLayoutDx12(
		ID3D12GraphicsCommandList4 *CommandList,
		CreationRenderer::PipelineLayoutDx12 *CurrentLayout,
		CreationRenderer::PipelineLayoutDx12 *TargetLayout,
		CreationRenderer::TechniqueData *CurrentTech,
		CreationRenderer::TechniqueData *TargetTech)
	{
		// Nasty hack since these structures aren't actually the same
		CurrentTech = CurrentTech ? reinterpret_cast<decltype(CurrentTech)>(reinterpret_cast<uintptr_t>(CurrentTech) + 0x8) : CurrentTech;
		TargetTech = TargetTech ? reinterpret_cast<decltype(TargetTech)>(reinterpret_cast<uintptr_t>(TargetTech) + 0x8) : TargetTech;

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
		if (auto itr = TargetTech ? TrackedTechniqueIdToRootSignature.find(TargetTech->m_Id) : TrackedTechniqueIdToRootSignature.end();
			itr != TrackedTechniqueIdToRootSignature.end())
		{
			updateRequired = true;
			rootSignature = itr->second.Get();
		}
		else if (!updateRequired && CurrentTech)
		{
			updateRequired = TrackedTechniqueIdToRootSignature.contains(CurrentTech->m_Id);
		}

		if (updateRequired)
		{
			const auto type = *reinterpret_cast<CreationRenderer::ShaderType *>(
				reinterpret_cast<uintptr_t>(TargetLayout->m_LayoutConfigurationData) + 0x4);

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

			mov(ptr[rsp + 0x20], rbx); // a5: Target Technique
			mov(r9, ptr[rcx + 0x8]);   // a4: Current Technique
			mov(r8, r12);			   // a3: Target PipelineLayoutDx12
			mov(rdx, ptr[rcx]);		   // a2: Current PipelineLayoutDx12
			mov(rcx, ptr[r14 + 0x60]); // a1: ID3D12GraphicsCommandList
			mov(rax, reinterpret_cast<uintptr_t>(&OverridePipelineLayoutDx12));
			call(rax);
			mov(rcx, r14);

			test(al, al);
			jnz(emulateSetNewSignature);

			// Run the original code
			mov(rax, m_TargetAddress + 0x4D);
			jmp(rax);

			// New signature required. OverridePipelineLayoutDx12() is expected to pass a signature to the D3D12 API
			// before we get here. This bypasses Starfield's calls to ID3D12CommandList::SetXXXRootSignature().
			L(emulateSetNewSignature);
			mov(rax, m_TargetAddress + 0x3A);
			jmp(rax);
		}

		void Patch()
		{
			Hooks::WriteJump(m_TargetAddress, getCode());
		}
	};

	DECLARE_HOOK_TRANSACTION(CRHooks)
	{
		static SetPipelineLayoutDx12HookGen setPipelineLayoutDx12Hook(Offsets::Signature("74 4B 49 8B 04 24 49 8B 54 24 60"));
		setPipelineLayoutDx12Hook.Patch();
	};
}
