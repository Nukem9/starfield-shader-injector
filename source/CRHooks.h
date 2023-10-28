#pragma once

#include "RE/CreationRenderer.h"
#include "CComPtr.h"
#include "D3DPipelineStateStream.h"

struct ID3D12Device2;

namespace CRHooks
{
	void TrackCompiledTechnique(
		CComPtr<ID3D12Device2> Device,
		CreationRenderer::TechniqueData *Technique,
		D3DPipelineStateStream::Copy&& StreamCopy,
		bool WasPatchedUpfront);
}
