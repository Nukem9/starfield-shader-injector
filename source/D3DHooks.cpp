#include <xbyak/xbyak.h>
#include "RE/CreationRenderer.h"
#include "CComPtr.h"
#include "CRHooks.h"
#include "D3DShaderReplacement.h"
#include "DebuggingUtil.h"
#include "D3Dhooks.h"

namespace D3DHooks
{
	//
	// Skip the early LoadPipeline() call and move it down into CreatePipelineStateForTechnique.
	//
	// Starfield calls CreatePipelineState with the same parameters as here if we return an error code. There's not
	// many choices since we have to query multiple files on disk each time. 7000 pipelines * 8 shaders * 3 calls.
	//
	// Instead we get 7000 pipelines * 8 shaders * 1 call.
	// 
	// 
	// This method returns an HRESULT success or error code, which can include E_INVALIDARG if the name doesn't
	// exist or the stream description doesn't match the data in the library, and E_OUTOFMEMORY if the function
	// is unable to allocate the resulting PSO.
	//
	thread_local CComPtr<ID3D12PipelineLibrary1> TLLastRequestedPipelineLibrary;
	thread_local CreationRenderer::TechniqueData *TLLastRequestedShaderTechnique;
	thread_local wchar_t TLLastRequestedPipelineName[64];

	HRESULT LoadPipelineForTechnique(ID3D12PipelineLibrary1 *Thisptr, LPCWSTR Name, const D3D12_PIPELINE_STATE_STREAM_DESC *Desc, REFIID Riid, void **PipelineState, CreationRenderer::TechniqueData *Tech)
	{
		TLLastRequestedPipelineLibrary = Thisptr;
		TLLastRequestedShaderTechnique = Tech;
		wcscpy_s(TLLastRequestedPipelineName, Name);

		return E_INVALIDARG;
	}

	class LoadPipelineHookGen : Xbyak::CodeGenerator
	{
	private:
		const uintptr_t m_TargetAddress;

	public:
		LoadPipelineHookGen(uintptr_t TargetAddress) : m_TargetAddress(TargetAddress)
		{
			mov(ptr[rsp + 0x28], r12); // a6: Technique pointer
			mov(rax, reinterpret_cast<uintptr_t>(&LoadPipelineForTechnique));
			call(rax);
			test(eax, eax);

			jmp(ptr[rip]);
			dq(m_TargetAddress + 0x5);
		}

		void Patch()
		{
			Hooks::WriteJump(m_TargetAddress, getCode());
		}
	};

	//
	// Similar to LoadPipeline above, but for storing pipeline state. CreatePipelineStateForTechnique determines
	// whether this gets called by setting TLNextShaderTechniqueToSkipCaching. We don't want modified shaders to
	// be cached.
	// 
	// 
	// This method returns an HRESULT success or error code, including E_INVALIDARG if the name already exists,
	// E_OUTOFMEMORY if unable to allocate storage in the library.
	//
	thread_local CreationRenderer::TechniqueData *TLNextShaderTechniqueToSkipCaching;

	HRESULT StorePipelineForTechnique(ID3D12PipelineLibrary1 *Thisptr, LPCWSTR Name, ID3D12PipelineState *Pipeline, CreationRenderer::TechniqueData *Tech)
	{
		if (TLNextShaderTechniqueToSkipCaching == Tech)
		{
			TLNextShaderTechniqueToSkipCaching = nullptr;
			return S_OK;
		}

		return Thisptr->StorePipeline(Name, Pipeline);
	}

	class StorePipelineHookGen : Xbyak::CodeGenerator
	{
	private:
		const uintptr_t m_TargetAddress;

	public:
		StorePipelineHookGen(uintptr_t TargetAddress) : m_TargetAddress(TargetAddress)
		{
			mov(r9, r12); // a4: Technique pointer
			mov(rax, reinterpret_cast<uintptr_t>(&StorePipelineForTechnique));
			call(rax);
			mov(ebx, eax);

			jmp(ptr[rip]);
			dq(m_TargetAddress + 0x5);
		}

		void Patch()
		{
			Hooks::WriteJump(m_TargetAddress, getCode());
		}
	};

	//
	// Pipeline state object creation. This is the main hook where shader bytecode gets replaced.
	// 
	//
	// This method returns E_OUTOFMEMORY if there is insufficient memory to create the pipeline state object.
	// See Direct3D 12 Return Codes for other possible return values.
	//
	HRESULT CreatePipelineStateForTechnique(ID3D12Device2 *Thisptr, const D3D12_PIPELINE_STATE_STREAM_DESC *Desc, REFIID Riid, void **PipelineState, CreationRenderer::TechniqueData *Tech)
	{
		if (Riid != __uuidof(ID3D12PipelineState))
			return E_NOINTERFACE;

		*PipelineState = nullptr;

		// Note that streamCopy is initially a 1:1 copy since Desc is const. We don't know if a modification
		// is applied until PatchPipelineStateStream returns.
		D3DPipelineStateStream::Copy streamCopy(Desc);
		const std::span rootSignatureData(Tech->m_Inputs->m_RootSignatureBlob, Tech->m_Inputs->m_RootSignatureBlobSize);

		// shaderWasPatched will be true if ANY part of the pipeline state stream is modified by code. If so,
		// the pipeline state has to be created from scratch. Otherwise ask the pipeline library interface for
		// a precompiled copy.
		bool shaderWasPatched = false;
		bool shaderWasLoadedFromCache = false;

		if (D3DShaderReplacement::PatchPipelineStateStream(streamCopy, Thisptr, &rootSignatureData, Tech->m_Name, Tech->m_Id))
		{
			shaderWasPatched = true;
		}
		else
		{
			if (TLLastRequestedPipelineLibrary && TLLastRequestedShaderTechnique == Tech)
			{
				if (SUCCEEDED(TLLastRequestedPipelineLibrary->LoadPipeline(TLLastRequestedPipelineName, streamCopy.GetDesc(), Riid, PipelineState)))
					shaderWasLoadedFromCache = true;
			}
		}

		TLLastRequestedPipelineLibrary = nullptr;
		TLLastRequestedShaderTechnique = nullptr;
		TLNextShaderTechniqueToSkipCaching = (shaderWasLoadedFromCache || shaderWasPatched) ? Tech : nullptr;

		if (!shaderWasLoadedFromCache)
		{
			const auto hr = Thisptr->CreatePipelineState(streamCopy.GetDesc(), Riid, PipelineState);

			if (FAILED(hr))
			{
				spdlog::error("CreatePipelineState failed and returned {:X}. Shader technique: {:X}.", static_cast<uint32_t>(hr), Tech->m_Id);

				if (hr == E_INVALIDARG)
					spdlog::error("Please check that all custom shaders have matching input semantics, root signatures, and are digitally signed by dxc.exe.");

				return hr;
			}
		}

		// Tech can't be used because it's allocated on the stack and quickly discarded. PipelineState is a
		// pointer within another TechniqueData struct that's stored in a global array - a suitable alternative.
		auto globalTech = reinterpret_cast<ptrdiff_t>(PipelineState) - offsetof(CreationRenderer::TechniqueData, m_PipelineState);
		CRHooks::TrackCompiledTechnique(Thisptr, reinterpret_cast<CreationRenderer::TechniqueData *>(globalTech), std::move(streamCopy), shaderWasPatched);

		DebuggingUtil::SetObjectDebugName(static_cast<ID3D12PipelineState *>(*PipelineState), Tech->m_Name);
		return S_OK;
	}

	class CreatePipelineStateHookGen : Xbyak::CodeGenerator
	{
	private:
		const uintptr_t m_TargetAddress;

	public:
		CreatePipelineStateHookGen(uintptr_t TargetAddress) : m_TargetAddress(TargetAddress)
		{
			mov(ptr[rsp + 0x20], r12); // a5: Technique pointer
			mov(rax, reinterpret_cast<uintptr_t>(&CreatePipelineStateForTechnique));
			call(rax);

			jmp(ptr[rip]);
			dq(m_TargetAddress + 0x6);
		}

		void Patch()
		{
			Hooks::WriteJump(m_TargetAddress, getCode());
		}
	};

	DECLARE_HOOK_TRANSACTION(D3DHooks)
	{
		static LoadPipelineHookGen loadPipelineHook(Offsets::Signature("FF 50 68 85 C0 0F 89 ? ? ? ? 49 8B 8F A0 03 00 00 48 8B 01 4C 8B CF 4C 8D"));
		loadPipelineHook.Patch();

		static StorePipelineHookGen storePipelineHook(Offsets::Signature("FF 50 40 8B D8 85 C0 0F 89 ? ? ? ? 45 33 E4 4C 89 64 24 58"));
		storePipelineHook.Patch();

		static CreatePipelineStateHookGen createPipelineStateHook1(Offsets::Signature("FF 90 78 01 00 00 8B D8 41 BD FF FF FF FF 85 C0 0F 89 ? ? ? ? 33 C0"));
		createPipelineStateHook1.Patch();

		static CreatePipelineStateHookGen createPipelineStateHook2(Offsets::Signature("FF 90 78 01 00 00 8B D8 85 C0 0F 89 ? ? ? ? 4C 89 6C 24 68"));
		createPipelineStateHook2.Patch();
	};
}
