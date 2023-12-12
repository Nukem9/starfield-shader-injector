#pragma once

#include "D3DPipelineStateStream.h"

namespace D3DShaderReplacement
{
	const std::filesystem::path& GetShaderBinDirectory();

	bool PatchPipelineStateStream(
		D3DPipelineStateStream::Copy& StreamCopy,
		ID3D12Device2 *Device,
		const std::span<const uint8_t> *RootSignatureData,
		const char *TechniqueName,
		uint64_t TechniqueId);
}
