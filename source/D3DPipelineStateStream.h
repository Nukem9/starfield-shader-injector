#pragma once

#include <d3d12.h>

namespace D3D12PipelineStateStream
{
	// Thanks to RenderDoc source code for providing some insight. What a mess this was...
	//
	// NOTE: D3DX12ParsePipelineStream is close to what I want. However, what I don't want
	// is to have to implement five billion interface callbacks.
	class Iter
	{
	private:
		struct D3D12_PSO_SUBOBJECT
		{
			D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type;
		};

		struct D3D12_PTR_PSO_SUBOBJECT
		{
			D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type;
			UINT Padding;

			union
			{
				ID3D12RootSignature *RootSignature;
				D3D12_SHADER_BYTECODE Shader;
				D3D12_STREAM_OUTPUT_DESC StreamOutput;
				D3D12_INPUT_LAYOUT_DESC InputLayout;
				D3D12_CACHED_PIPELINE_STATE CachedPSO;
				D3D12_VIEW_INSTANCING_DESC ViewInstancing;
			};
		};

		uint8_t *m_Start = nullptr;
		uint8_t *m_End = nullptr;

	public:
		Iter(const D3D12_PIPELINE_STATE_STREAM_DESC *Description)
		{
			m_Start = reinterpret_cast<uint8_t *>(Description->pPipelineStateSubobjectStream);
			m_End = m_Start + Description->SizeInBytes;
		}

		Iter(const Iter& Other) = delete;
		Iter& operator=(const Iter& Other) = delete;

		bool AtEnd() const
		{
			return m_Start >= m_End;
		}

		void Advance()
		{
			const auto type = GetObj()->Type;

			m_Start += sizeof(D3D12_PTR_PSO_SUBOBJECT::Type);
			m_Start = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(m_Start), GetAlignmentForType(type)));

			m_Start += GetSizeForType(type);
			m_Start = reinterpret_cast<uint8_t *>(AlignUp(reinterpret_cast<uintptr_t>(m_Start), alignof(void *)));
		}

		D3D12_PTR_PSO_SUBOBJECT *GetObj() const
		{
			return reinterpret_cast<D3D12_PTR_PSO_SUBOBJECT *>(m_Start);
		}

		size_t GetSizeForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const
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

			__debugbreak();
			return 0;
		}

		size_t GetAlignmentForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const
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

			__debugbreak();
			return 0;
		}

	private:
		template <typename T>
		static T AlignUp(T x, T a)
		{
			return (x + (a - 1)) & (~(a - 1));
		}
	};

	class Copy
	{
	private:
		std::vector<std::unique_ptr<uint8_t[]>> m_TempBuffers;
		std::vector<IUnknown *> m_RefCountedObjects;
		D3D12_PIPELINE_STATE_STREAM_DESC m_CopiedDesc = {};

	public:
		Copy(const D3D12_PIPELINE_STATE_STREAM_DESC *Description)
		{
			CreateCopy(Description);
		}

		Copy(const Copy& Other) = delete;

		Copy(Copy&& Other)
		{
			m_TempBuffers = std::move(Other.m_TempBuffers);
			m_RefCountedObjects = std::move(Other.m_RefCountedObjects);

			m_CopiedDesc = Other.m_CopiedDesc;
			Other.m_CopiedDesc = {};
		}

		~Copy()
		{
			for (auto& obj : m_RefCountedObjects)
				obj->Release();
		}

		void TrackAllocation(std::unique_ptr<uint8_t[]>&& Allocation)
		{
			m_TempBuffers.emplace_back(std::move(Allocation));
		}

		void TrackObject(IUnknown*&& Object)
		{
			m_RefCountedObjects.emplace_back(std::move(Object));
		}

		const D3D12_PIPELINE_STATE_STREAM_DESC *GetDesc() const
		{
			return &m_CopiedDesc;
		}

	private:
		template<typename T>
		T *memdup(const T *Data, size_t Size)
		{
			if (!Data)
				return nullptr;

			auto& ptr = m_TempBuffers.emplace_back(std::make_unique<uint8_t[]>(Size));
			return reinterpret_cast<T *>(memcpy(ptr.get(), Data, Size));
		}

		void CreateCopy(const D3D12_PIPELINE_STATE_STREAM_DESC *InputDesc)
		{
			// Do a memcpy up front and then patch the pointers as needed
			m_CopiedDesc.pPipelineStateSubobjectStream = memdup(InputDesc->pPipelineStateSubobjectStream, InputDesc->SizeInBytes);
			m_CopiedDesc.SizeInBytes = InputDesc->SizeInBytes;

			for (Iter iter(GetDesc()); !iter.AtEnd(); iter.Advance())
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
					if (auto copy = obj->RootSignature; copy)
					{
						copy->AddRef(); // Bare pointer copy
						TrackObject(std::move(copy));
					}
					break;

				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
					obj->StreamOutput.pSODeclaration = memdup(obj->StreamOutput.pSODeclaration, obj->StreamOutput.NumEntries * sizeof(D3D12_SO_DECLARATION_ENTRY));
					obj->StreamOutput.pBufferStrides = memdup(obj->StreamOutput.pBufferStrides, obj->StreamOutput.NumStrides * sizeof(UINT));
					break;

				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
					obj->InputLayout.pInputElementDescs = memdup(obj->InputLayout.pInputElementDescs, obj->InputLayout.NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
					break;

				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
					obj->ViewInstancing.pViewInstanceLocations = memdup(obj->ViewInstancing.pViewInstanceLocations, obj->ViewInstancing.ViewInstanceCount * sizeof(D3D12_VIEW_INSTANCE_LOCATION));
					break;
				}
			}
		}
	};
}
