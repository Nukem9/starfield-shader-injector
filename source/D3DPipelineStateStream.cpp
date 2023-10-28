#include "D3DPipelineStateStream.h"

namespace D3DPipelineStateStream
{
	Iterator::Iterator(const D3D12_PIPELINE_STATE_STREAM_DESC *Description)
	{
		m_Start = reinterpret_cast<uint8_t *>(Description->pPipelineStateSubobjectStream);
		m_End = m_Start + Description->SizeInBytes;
	}

	void Iterator::Advance()
	{
		const auto type = GetObj()->Type;

		m_Start += sizeof(D3D12_PTR_PSO_SUBOBJECT::Type);
		m_Start = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(m_Start), GetAlignmentForType(type)));

		m_Start += GetSizeForType(type);
		m_Start = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(m_Start), alignof(void *)));
	}

	bool Iterator::AtEnd() const
	{
		return m_Start >= m_End;
	}

	Iterator::D3D12_PTR_PSO_SUBOBJECT *Iterator::GetObj() const
	{
		return reinterpret_cast<D3D12_PTR_PSO_SUBOBJECT *>(m_Start);
	}

	size_t Iterator::GetSizeForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const
	{
		switch (Type)
		{
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
			return sizeof(D3D12_SHADER_BYTECODE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
			return sizeof(D3D12_CACHED_PIPELINE_STATE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
			return sizeof(ID3D12RootSignature *);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
			return sizeof(D3D12_STREAM_OUTPUT_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
			return sizeof(D3D12_BLEND_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
			return sizeof(UINT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
			return sizeof(D3D12_RASTERIZER_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
			return sizeof(D3D12_DEPTH_STENCIL_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
			return sizeof(D3D12_INPUT_LAYOUT_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
			return sizeof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
			return sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
			return sizeof(D3D12_RT_FORMAT_ARRAY);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
			return sizeof(DXGI_FORMAT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
			return sizeof(DXGI_SAMPLE_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
			return sizeof(UINT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
			return sizeof(D3D12_PIPELINE_STATE_FLAGS);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
			return sizeof(D3D12_DEPTH_STENCIL_DESC1);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
			return sizeof(D3D12_VIEW_INSTANCING_DESC);
		}

		std::unreachable();
	}

	size_t Iterator::GetAlignmentForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const
	{
		switch (Type)
		{
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
			return alignof(D3D12_SHADER_BYTECODE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
			return alignof(D3D12_CACHED_PIPELINE_STATE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
			return alignof(ID3D12RootSignature *);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
			return alignof(D3D12_STREAM_OUTPUT_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
			return alignof(D3D12_BLEND_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
			return alignof(UINT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
			return alignof(D3D12_RASTERIZER_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
			return alignof(D3D12_DEPTH_STENCIL_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
			return alignof(D3D12_INPUT_LAYOUT_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
			return alignof(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
			return alignof(D3D12_PRIMITIVE_TOPOLOGY_TYPE);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
			return alignof(D3D12_RT_FORMAT_ARRAY);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
			return alignof(DXGI_FORMAT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
			return alignof(DXGI_SAMPLE_DESC);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
			return alignof(UINT);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
			return alignof(D3D12_PIPELINE_STATE_FLAGS);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
			return alignof(D3D12_DEPTH_STENCIL_DESC1);

		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
			return alignof(D3D12_VIEW_INSTANCING_DESC);
		}

		std::unreachable();
	}

	Copy::Copy(const D3D12_PIPELINE_STATE_STREAM_DESC *Description)
	{
		CreateCopy(Description);
	}

	Copy::Copy(Copy&& Other)
	{
		m_TempBuffers = std::move(Other.m_TempBuffers);
		m_RefCountedObjects = std::move(Other.m_RefCountedObjects);

		m_CopiedDesc = Other.m_CopiedDesc;
		Other.m_CopiedDesc = {};
	}

	void Copy::CreateCopy(const D3D12_PIPELINE_STATE_STREAM_DESC *InputDesc)
	{
		// Do a memcpy up front and then patch the pointers as needed
		m_CopiedDesc.pPipelineStateSubobjectStream = memdup(InputDesc->pPipelineStateSubobjectStream, InputDesc->SizeInBytes);
		m_CopiedDesc.SizeInBytes = InputDesc->SizeInBytes;

		for (Iterator iter(GetDesc()); !iter.AtEnd(); iter.Advance())
		{
			switch (auto obj = iter.GetObj(); obj->Type)
			{
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
				obj->Shader.pShaderBytecode = memdup(obj->Shader.pShaderBytecode, obj->Shader.BytecodeLength);
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
				obj->CachedPSO.pCachedBlob = memdup(obj->CachedPSO.pCachedBlob, obj->CachedPSO.CachedBlobSizeInBytes);
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
				if (obj->RootSignature)
				{
					CComPtr<ID3D12RootSignature> rsCopy(obj->RootSignature);
					TrackObject(std::move(rsCopy));
				}
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
				obj->StreamOutput.pSODeclaration = memdup(
					obj->StreamOutput.pSODeclaration,
					obj->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY));
				obj->StreamOutput.pBufferStrides = memdup(obj->StreamOutput.pBufferStrides, obj->StreamOutput.NumStrides * sizeof(UINT));
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
				obj->InputLayout.pInputElementDescs = memdup(
					obj->InputLayout.pInputElementDescs,
					obj->InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
				obj->ViewInstancing.pViewInstanceLocations = memdup(
					obj->ViewInstancing.pViewInstanceLocations,
					obj->ViewInstancing.ViewInstanceCount * sizeof(D3D12_VIEW_INSTANCE_LOCATION));
				break;
			}
		}
	}
}
