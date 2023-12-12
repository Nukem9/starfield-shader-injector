#pragma once

#include "RE/CreationRenderer.h"
#include "CComPtr.h"
#include "D3DPipelineStateStream.h"

namespace CRHooks
{
	void TrackDevice(CComPtr<ID3D12Device2> Device);

	void TrackCompiledTechnique(
		CComPtr<ID3D12Device2> Device,
		CreationRenderer::TechniqueData *Technique,
		D3DPipelineStateStream::Copy&& StreamCopy,
		bool WasPatchedUpfront);
}
