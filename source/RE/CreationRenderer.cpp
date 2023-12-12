#include "CreationRenderer.h"

namespace CreationRenderer
{
	ID3D12CommandList *GetRenderGraphCommandList(void *RenderGraphData)
	{
		auto addr = Offsets::Signature("48 83 EC 28 48 8B 89 38 01 00 00 33 C0 48 85 C9 74 05 E8");
		auto func = reinterpret_cast<decltype(&GetRenderGraphCommandList)>(addr.operator size_t());

		return *reinterpret_cast<ID3D12CommandList **>(reinterpret_cast<uintptr_t>(func(RenderGraphData)) + 0x10);
	}

	Dx12Unknown *AcquireRenderPassRenderTarget(void *RenderPassData, uint32_t RenderTargetId)
	{
		// Leads to a call instruction
		auto addr = Offsets::Signature("E8 ? ? ? ? 48 8B 0D ? ? ? ? 48 8B D8 8B 97 F0 00 00 00 48 89 84 24 E8 00 00 00").operator size_t();
		addr = addr - 0x5 + *reinterpret_cast<int32_t *>(addr + 0x1);
		auto func = reinterpret_cast<decltype(&AcquireRenderPassRenderTarget)>(addr);

		return func(RenderPassData, RenderTargetId);
	}

	Dx12Unknown *AcquireRenderPassSingleInput(void *RenderPassData)
	{
		auto addr = Offsets::Signature("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 48 8B 01 48 8B 79 08 83 78 08 00 48 8D "
									   "48 10 7C 03 48 8B 09 44 8B 59 24 8B 41 20 8B 5F 08");
		auto func = reinterpret_cast<decltype(&AcquireRenderPassSingleInput)>(addr.operator size_t());

		return func(RenderPassData);
	}

	Dx12Unknown *AcquireRenderPassSingleOutput(void *RenderPassData)
	{
		auto addr = Offsets::Signature("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 48 8B 01 48 8B 79 08 83 78 08 00 48 8D "
									   "48 10 7C 03 48 8B 09 44 8B 59 04 8B 01 8B 5F 08");
		auto func = reinterpret_cast<decltype(&AcquireRenderPassSingleOutput)>(addr.operator size_t());

		return func(RenderPassData);
	}
}
