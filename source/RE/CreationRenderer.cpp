#include "CreationRenderer.h"

namespace CreationRenderer
{
	ID3D12CommandList *GetRenderGraphCommandList(void *RenderGraphData)
	{
		auto addr = Offsets::Signature("48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 99 38 01 00 00");
		auto func = reinterpret_cast<void *(*)(void *)>(addr.operator size_t());

		return *reinterpret_cast<ID3D12CommandList **>(reinterpret_cast<uintptr_t>(func(RenderGraphData)) + 0x60);
	}

	Dx12Unknown *AcquireRenderPassIO(void *RenderPassData, uint32_t IOIndex)
	{
		auto addr = Offsets::Signature("48 83 EC 08 8B 41 08 4C 8B D2 85 C0 0F 84 89 00 00 00 48 89 74 24 18");
		auto func = reinterpret_cast<Dx12Unknown *(*)(uint64_t, void *)>(addr.operator size_t());

		auto v19 = *(uint64_t *)RenderPassData + 16LL;
		if (*(int *)(*(uint64_t *)RenderPassData + 8LL) >= 0)
			v19 = *(uint64_t *)v19;

		auto v20 = *(uint64_t *)((uint64_t)RenderPassData + 8);
		auto v21 = (uint64_t)*(uint32_t *)(v19 + 4 + (IOIndex * 32)) << 32;

		struct
		{
			uint64_t arg0;
			uint64_t arg1;
		} tempdata =
		{
			.arg0 = *(uint32_t *)(v19 + (IOIndex * 32)) | v21,
			.arg1 = *(uint32_t *)(v19 + 8 + (IOIndex * 32)),
		};

		return func(v20, &tempdata);
	}
}
