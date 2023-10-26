#pragma once

#include <wrl/client.h>
#include "RE/CreationRenderer.h"
#include "D3DPipelineStateStream.h"

struct ID3D12Device2;

namespace CRHooks
{
	void TrackCompiledTechnique(Microsoft::WRL::ComPtr<ID3D12Device2> Device, CreationRenderer::TechniqueData *Technique, D3D12PipelineStateStream::Copy&& StreamCopy, bool WasPatchedUpfront);
}
