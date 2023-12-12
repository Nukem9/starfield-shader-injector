#pragma once

#include "CComPtr.h"

namespace D3DPipelineStateStream
{
	// Thanks to RenderDoc source code for providing some insight. What a mess this was...
	//
	// NOTE: D3DX12ParsePipelineStream is close to what I want. However, what I don't want
	// is to have to implement five billion interface callbacks.
	class Iterator
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
		Iterator(const D3D12_PIPELINE_STATE_STREAM_DESC *Description);
		Iterator(const Iterator& Other) = delete;
		Iterator& operator=(const Iterator& Other) = delete;

		void Advance();

		bool AtEnd() const;
		D3D12_PTR_PSO_SUBOBJECT *GetObj() const;

		size_t GetSizeForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const;
		size_t GetAlignmentForType(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type) const;

	private:
		template<typename T>
		static T AlignUp(T X, T A)
		{
			return (X + (A - 1)) & (~(A - 1));
		}
	};

	class Copy
	{
	private:
		std::vector<std::unique_ptr<uint8_t[]>> m_TempBuffers;
		std::vector<CComPtr<IUnknown>> m_RefCountedObjects;
		D3D12_PIPELINE_STATE_STREAM_DESC m_CopiedDesc = {};

	public:
		Copy(const D3D12_PIPELINE_STATE_STREAM_DESC *Description);
		Copy(const Copy& Other) = delete;
		Copy(Copy&& Other);

		void TrackAllocation(std::unique_ptr<uint8_t[]>&& Allocation)
		{
			m_TempBuffers.emplace_back(std::forward<std::unique_ptr<uint8_t[]>>(Allocation));
		}

		template<typename T>
		void TrackObject(CComPtr<T>&& Object)
		{
			m_RefCountedObjects.emplace_back(std::forward<CComPtr<T>>(Object));
		}

		const D3D12_PIPELINE_STATE_STREAM_DESC *GetDesc() const
		{
			return &m_CopiedDesc;
		}

	private:
		void CreateCopy(const D3D12_PIPELINE_STATE_STREAM_DESC *InputDesc);

		template<typename T>
		T *memdup(const T *Data, size_t Size)
		{
			if (!Data)
				return nullptr;

			auto& ptr = m_TempBuffers.emplace_back(std::make_unique<uint8_t[]>(Size));
			return reinterpret_cast<T *>(memcpy(ptr.get(), Data, Size));
		}
	};
}
