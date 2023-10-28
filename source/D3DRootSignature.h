#pragma once

#include <string>
#include <d3d12.h>

namespace D3DRootSignature
{
	std::string TextureFilterModeToString(D3D12_FILTER Mode);
	std::string TextureAddressModeToString(D3D12_TEXTURE_ADDRESS_MODE Mode);
	std::string ComparisonFuncToString(D3D12_COMPARISON_FUNC Func);
	std::string ShaderVisibilityToString(D3D12_SHADER_VISIBILITY Visibility);
	std::string RootDescriptorFlagsToString(D3D12_ROOT_DESCRIPTOR_FLAGS Flags);
	std::string RootDescriptorRangeFlagsToString(D3D12_DESCRIPTOR_RANGE_FLAGS Flags);
	std::string RootFlagsToString(D3D12_ROOT_SIGNATURE_FLAGS Flags);
	std::string RootConstantsToString(const D3D12_ROOT_CONSTANTS& Constants, D3D12_SHADER_VISIBILITY Visibility);
	std::string RootParameterToString(
		D3D12_ROOT_PARAMETER_TYPE Type,
		const D3D12_ROOT_DESCRIPTOR1& Param,
		D3D12_SHADER_VISIBILITY Visibility);
	std::string RootDescriptorTableToString(const D3D12_ROOT_DESCRIPTOR_TABLE1& Table, D3D12_SHADER_VISIBILITY Visibility);
	std::string StaticSamplerToString(const D3D12_STATIC_SAMPLER_DESC& Sampler);
	std::string RootSignatureToString(const D3D12_ROOT_SIGNATURE_DESC1& RootSig);
}
