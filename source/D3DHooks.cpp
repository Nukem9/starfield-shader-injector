#include <xbyak/xbyak.h>
#include "RE/CreationRenderer.h"
#include "CComPtr.h"
#include "CRHooks.h"
#include "D3DShaderReplacement.h"
#include "DebuggingUtil.h"

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

	HRESULT LoadPipelineForTechnique(
		ID3D12PipelineLibrary1 *Thisptr,
		LPCWSTR Name,
		const D3D12_PIPELINE_STATE_STREAM_DESC *Desc,
		REFIID Riid,
		void **PipelineState,
		CreationRenderer::TechniqueData *Tech)
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

	HRESULT StorePipelineForTechnique(
		ID3D12PipelineLibrary1 *Thisptr,
		LPCWSTR Name,
		ID3D12PipelineState *Pipeline,
		CreationRenderer::TechniqueData *Tech)
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
			mov(ebp, eax);

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
	HRESULT CreatePipelineStateForTechnique(
		ID3D12Device2 *Thisptr,
		const D3D12_PIPELINE_STATE_STREAM_DESC *Desc,
		REFIID Riid,
		void **PipelineState,
		CreationRenderer::TechniqueData *Tech)
	{
		CRHooks::TrackDevice(Thisptr);

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
				if (SUCCEEDED(TLLastRequestedPipelineLibrary->LoadPipeline(
					TLLastRequestedPipelineName,
					streamCopy.GetDesc(),
					Riid,
					PipelineState)))
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
				spdlog::error(
					"CreatePipelineState failed and returned {:X}. Shader technique: {:X}.",
					static_cast<uint32_t>(hr),
					Tech->m_Id);

				if (hr == E_INVALIDARG)
					spdlog::error("Please check that all custom shaders have matching input semantics, root signatures, and are digitally "
								  "signed by dxc.exe.");

				return hr;
			}
		}

		// Tech can't be used because it's allocated on the stack and quickly discarded. PipelineState is a
		// pointer within another TechniqueData struct that's stored in a global array - a suitable alternative.
		auto globalTech = reinterpret_cast<ptrdiff_t>(PipelineState) - offsetof(CreationRenderer::TechniqueData, m_PipelineState);

		CRHooks::TrackCompiledTechnique(
			Thisptr,
			reinterpret_cast<CreationRenderer::TechniqueData *>(globalTech),
			std::move(streamCopy),
			shaderWasPatched);

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

	//
	// Identical to CreatePipelineStateForTechnique but intercepts the call to CreateGraphicsPipelineState that
	// FidelityFX's SDK uses.
	//
	HRESULT FFXCreateGraphicsPipelineStateForTechnique(
		ID3D12Device2 *Thisptr,
		const D3D12_GRAPHICS_PIPELINE_STATE_DESC *Desc,
		REFIID Riid,
		void **PipelineState)
	{
		CRHooks::TrackDevice(Thisptr);

		if (Riid != __uuidof(ID3D12PipelineState))
			return E_NOINTERFACE;

		*PipelineState = nullptr;

		// FFX doesn't have debug names so we have to fake one
		const auto fakeTechniqueId = static_cast<uint64_t>(DebuggingUtil::FNV1A32(Desc->VS.pShaderBytecode, Desc->VS.BytecodeLength)) << 32ull |
									 static_cast<uint64_t>(DebuggingUtil::FNV1A32(Desc->PS.pShaderBytecode, Desc->PS.BytecodeLength));

		char fakeTechniqueName[128];
		sprintf_s(fakeTechniqueName, "FidelityFX3FI- (%llX)", fakeTechniqueId);

		// Upgrade CreateGraphicsPipelineState's legacy structure to CreatePipelineState's bytestream description
		struct
		{
#define MAKE_PSS_ENTRY(HeaderType, FullType)                                                                        \
	struct alignas(void *)                                                                                          \
	{                                                                                                               \
		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE m_Type_##HeaderType = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_##HeaderType; \
		FullType = {};                                                                                              \
	}

			MAKE_PSS_ENTRY(ROOT_SIGNATURE, ID3D12RootSignature *m_RootSignature);
			MAKE_PSS_ENTRY(VS, D3D12_SHADER_BYTECODE m_VS);
			MAKE_PSS_ENTRY(PS, D3D12_SHADER_BYTECODE m_PS);
			MAKE_PSS_ENTRY(DS, D3D12_SHADER_BYTECODE m_DS);
			MAKE_PSS_ENTRY(HS, D3D12_SHADER_BYTECODE m_HS);
			MAKE_PSS_ENTRY(GS, D3D12_SHADER_BYTECODE m_GS);
			MAKE_PSS_ENTRY(STREAM_OUTPUT, D3D12_STREAM_OUTPUT_DESC m_StreamOutput);
			MAKE_PSS_ENTRY(BLEND, D3D12_BLEND_DESC m_BlendState);
			MAKE_PSS_ENTRY(SAMPLE_MASK, UINT m_SampleMask);
			MAKE_PSS_ENTRY(RASTERIZER, D3D12_RASTERIZER_DESC m_RasterizerState);
			MAKE_PSS_ENTRY(DEPTH_STENCIL, D3D12_DEPTH_STENCIL_DESC m_DepthStencilState);
			MAKE_PSS_ENTRY(INPUT_LAYOUT, D3D12_INPUT_LAYOUT_DESC m_InputLayout);
			MAKE_PSS_ENTRY(IB_STRIP_CUT_VALUE, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE m_IBStripCutValue);
			MAKE_PSS_ENTRY(PRIMITIVE_TOPOLOGY, D3D12_PRIMITIVE_TOPOLOGY_TYPE m_PrimitiveTopologyType);
			MAKE_PSS_ENTRY(RENDER_TARGET_FORMATS, D3D12_RT_FORMAT_ARRAY m_RTVFormats);
			MAKE_PSS_ENTRY(DEPTH_STENCIL_FORMAT, DXGI_FORMAT m_DSVFormat);
			MAKE_PSS_ENTRY(SAMPLE_DESC, DXGI_SAMPLE_DESC m_SampleDesc);
			MAKE_PSS_ENTRY(NODE_MASK, UINT m_NodeMask);
			MAKE_PSS_ENTRY(CACHED_PSO, D3D12_CACHED_PIPELINE_STATE m_CachedPSO);
			MAKE_PSS_ENTRY(FLAGS, D3D12_PIPELINE_STATE_FLAGS m_Flags);

#undef MAKE_PSS_ENTRY
		} upgradedStreamData;
		// Designated initializers generate an internal compiler error
		upgradedStreamData.m_RootSignature = Desc->pRootSignature;
		upgradedStreamData.m_VS = Desc->VS;
		upgradedStreamData.m_PS = Desc->PS;
		upgradedStreamData.m_DS = Desc->DS;
		upgradedStreamData.m_HS = Desc->HS;
		upgradedStreamData.m_GS = Desc->GS;
		upgradedStreamData.m_StreamOutput = Desc->StreamOutput;
		upgradedStreamData.m_BlendState = Desc->BlendState;
		upgradedStreamData.m_SampleMask = Desc->SampleMask;
		upgradedStreamData.m_RasterizerState = Desc->RasterizerState;
		upgradedStreamData.m_DepthStencilState = Desc->DepthStencilState;
		upgradedStreamData.m_InputLayout = Desc->InputLayout;
		upgradedStreamData.m_IBStripCutValue = Desc->IBStripCutValue;
		upgradedStreamData.m_PrimitiveTopologyType = Desc->PrimitiveTopologyType;
		upgradedStreamData.m_RTVFormats.NumRenderTargets = Desc->NumRenderTargets;
		memcpy(upgradedStreamData.m_RTVFormats.RTFormats, Desc->RTVFormats, sizeof(Desc->RTVFormats));
		upgradedStreamData.m_DSVFormat = Desc->DSVFormat;
		upgradedStreamData.m_SampleDesc = Desc->SampleDesc;
		upgradedStreamData.m_NodeMask = Desc->NodeMask;
		upgradedStreamData.m_CachedPSO = Desc->CachedPSO;
		upgradedStreamData.m_Flags = Desc->Flags;

		const D3D12_PIPELINE_STATE_STREAM_DESC upgradedStreamDesc = {
			.SizeInBytes = sizeof(upgradedStreamData),
			.pPipelineStateSubobjectStream = &upgradedStreamData,
		};

		D3DPipelineStateStream::Copy streamCopy(&upgradedStreamDesc);
		D3DShaderReplacement::PatchPipelineStateStream(streamCopy, Thisptr, nullptr, fakeTechniqueName, fakeTechniqueId);

		const auto hr = Thisptr->CreatePipelineState(streamCopy.GetDesc(), Riid, PipelineState);

		if (FAILED(hr))
		{
			spdlog::error(
				"CreatePipelineState failed and returned {:X}. Shader technique: {:X}.",
				static_cast<uint32_t>(hr),
				fakeTechniqueId);

			if (hr == E_INVALIDARG)
				spdlog::error("Please check that all custom shaders have matching input semantics, root signatures, and are digitally "
							  "signed by dxc.exe.");

			return hr;
		}

		return S_OK;
	}

	class FFXCreateGraphicsPipelineStateHookGen : Xbyak::CodeGenerator
	{
	private:
		const uintptr_t m_TargetAddress;

	public:
		FFXCreateGraphicsPipelineStateHookGen(uintptr_t TargetAddress) : m_TargetAddress(TargetAddress)
		{
			mov(rax, reinterpret_cast<uintptr_t>(&FFXCreateGraphicsPipelineStateForTechnique));
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

	DECLARE_HOOK_TRANSACTION(D3DHooks)
	{
		static LoadPipelineHookGen loadPipelineHook(
			Offsets::Signature("FF 50 68 85 C0 0F 89 ? ? 00 00 49 8B 8D 18 04 00 00 48 8B 01"));
		loadPipelineHook.Patch();

		static StorePipelineHookGen storePipelineHook(
			Offsets::Signature("FF 50 40 8B E8 85 C0 0F 89 ? ? 00 00 45 33 FF 4C 89")); // WARNING: Banking on r12 being preserved across function call
		storePipelineHook.Patch();

		static CreatePipelineStateHookGen createPipelineStateHook1(
			Offsets::Signature("FF 90 78 01 00 00 8B D8 85 C0 0F 89 8A 02 00 00 4C 89"));
		createPipelineStateHook1.Patch();

		static CreatePipelineStateHookGen createPipelineStateHook2(
			Offsets::Signature("FF 90 78 01 00 00 8B D8 85 C0 0F 89 8F 01 00 00 4C 89"));
		createPipelineStateHook2.Patch();

		static FFXCreateGraphicsPipelineStateHookGen createGraphicsPipelineStateHook(
			Offsets::Signature("FF 50 50 85 C0 78 04 33 C0 EB 05 B8 0D 00 00 80"));
		createGraphicsPipelineStateHook.Patch();
	};
}
