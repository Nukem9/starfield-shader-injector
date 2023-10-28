#include <d3d12.h>
#include "D3DPipelineStateStream.h"
#include "D3DShaderReplacement.h"
#include "Plugin.h"

#pragma comment(lib, "d3d12.lib")

namespace D3DShaderReplacement
{
	uint32_t FNV1A32(const void *Input, size_t Length)
	{
		constexpr uint32_t FNV1_PRIME_32 = 0x01000193;
		constexpr uint32_t FNV1_BASE_32 = 2166136261U;

		auto data = reinterpret_cast<const unsigned char *>(Input);
		auto end = data + Length;

		auto hash = FNV1_BASE_32;

		for (; data != end; data++)
		{
			hash ^= *data;
			hash *= FNV1_PRIME_32;
		}

		return hash;
	}

	const std::filesystem::path& GetShaderBinDirectory()
	{
		const static auto path = []()
		{
			auto temp = std::filesystem::current_path() / "Data" / "shadersfx";
			spdlog::info("Using custom shader root directory: {}", temp.string());

			return temp;
		}();

		return path;
	}

	const char *GetShaderTypePrefix(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type)
	{
		switch (Type)
		{
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS: return "vs";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS: return "ps";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS: return "hs";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS: return "ds";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS: return "gs";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS: return "cs";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS: return "as";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS: return "ms";
		case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE: return "rsg";
		}

		return "unknown";
	}

	bool ExtractOrReplaceShader(
		D3DPipelineStateStream::Copy& StreamCopy,
		D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type,
		D3D12_SHADER_BYTECODE *Bytecode,
		const char *TechniqueName,
		uint64_t TechniqueId)
	{
		// Techniques have to be trimmed as they're too long to be used in file names
		const auto prefix = GetShaderTypePrefix(Type);

		char techniqueShortName[512] = {};
		strncpy_s(techniqueShortName, TechniqueName, _TRUNCATE);

		if (auto s = strchr(techniqueShortName, '-'))
			*s = '\0';

		char shaderBinFileName[512];
		sprintf_s(shaderBinFileName, "%s_%llX_%s.bin", techniqueShortName, TechniqueId, prefix);

		if (!Plugin::ShaderDumpBinPath.empty())
		{
			// Extract it
			const auto shaderBinFullPath = Plugin::ShaderDumpBinPath / techniqueShortName / shaderBinFileName;

			if (Bytecode->pShaderBytecode && Bytecode->BytecodeLength != 0)
			{
				// Calculate the shader data hash, dump it, then map it to a technique name in a dedicated CSV file. It's
				// far from efficient but it's usually a one-time operation.
				std::filesystem::create_directories(shaderBinFullPath.parent_path());

				const auto hash = FNV1A32(Bytecode->pShaderBytecode, Bytecode->BytecodeLength);
				spdlog::info("Dumping shader with hash {} to {}", hash, shaderBinFullPath.string());

				static std::mutex fileDumpMutex;
				fileDumpMutex.lock();
				{
					// Dump binary
					if (std::ofstream f(shaderBinFullPath, std::ios::binary); f.good())
						f.write(reinterpret_cast<const char *>(Bytecode->pShaderBytecode), Bytecode->BytecodeLength);

					// Append to CSV
					const static auto csvPath = Plugin::ShaderDumpBinPath / "ShaderTechniqueMap.csv";

					if (std::ofstream f(csvPath, std::ios::app); f.good())
					{
						char csvLine[2048];
						auto length = sprintf_s(csvLine, "%s,%s,%u,%llX,\"%s\"\n", techniqueShortName, prefix, hash, TechniqueId, TechniqueName);

						f.write(csvLine, length);
					}
				}
				fileDumpMutex.unlock();
			}
		}
		else
		{
			// Replace it
			const auto shaderBinFullPath = GetShaderBinDirectory() / techniqueShortName / shaderBinFileName;

			if (std::ifstream f(shaderBinFullPath, std::ios::binary | std::ios::ate); f.good())
			{
				static bool once = [&]()
				{
					spdlog::info("Trying to replace at least one shader: {}", shaderBinFullPath.string());
					return true;
				}();

				auto fileSize = static_cast<uint64_t>(f.tellg());
				auto fileData = std::make_unique<uint8_t[]>(fileSize);

				f.seekg(0, std::ios::beg);
				f.read(reinterpret_cast<char *>(fileData.get()), fileSize);

				// Only replace if the on-disk data is different
				if (fileSize != Bytecode->BytecodeLength || memcmp(fileData.get(), Bytecode->pShaderBytecode, fileSize) != 0)
				{
					Bytecode->BytecodeLength = fileSize;
					Bytecode->pShaderBytecode = fileData.get();
					StreamCopy.TrackAllocation(std::move(fileData));

					spdlog::trace("Used file replacement: {}", shaderBinFullPath.string());
					return true;
				}
			}
		}

		return false;
	}

	std::string TextureFilterModeToString(D3D12_FILTER Mode)
	{
		switch (Mode)
		{
		case D3D12_FILTER_MIN_MAG_MIP_POINT:
			return "FILTER_MIN_MAG_MIP_POINT";
		case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR:
			return "FILTER_MIN_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return "FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR:
			return "FILTER_MIN_POINT_MAG_MIP_LINEAR";
		case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT:
			return "FILTER_MIN_LINEAR_MAG_MIP_POINT";
		case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return "FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:
			return "FILTER_MIN_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MIN_MAG_MIP_LINEAR:
			return "FILTER_MIN_MAG_MIP_LINEAR";
		case D3D12_FILTER_ANISOTROPIC:
			return "FILTER_ANISOTROPIC";
		case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT:
			return "FILTER_COMPARISON_MIN_MAG_MIP_POINT";
		case D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR:
			return "FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return "FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR:
			return "FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR";
		case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT:
			return "FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT";
		case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return "FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT:
			return "FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR:
			return "FILTER_COMPARISON_MIN_MAG_MIP_LINEAR";
		case D3D12_FILTER_COMPARISON_ANISOTROPIC:
			return "FILTER_COMPARISON_ANISOTROPIC";
		case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT:
			return "FILTER_MINIMUM_MIN_MAG_MIP_POINT";
		case D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR:
			return "FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return "FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR:
			return "FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR";
		case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT:
			return "FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT";
		case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return "FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT:
			return "FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR:
			return "FILTER_MINIMUM_MIN_MAG_MIP_LINEAR";
		case D3D12_FILTER_MINIMUM_ANISOTROPIC:
			return "FILTER_MINIMUM_ANISOTROPIC";
		case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT:
			return "FILTER_MAXIMUM_MIN_MAG_MIP_POINT";
		case D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR:
			return "FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
			return "FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR:
			return "FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR";
		case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT:
			return "FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT";
		case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
			return "FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR";
		case D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT:
			return "FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT";
		case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR:
			return "FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR";
		case D3D12_FILTER_MAXIMUM_ANISOTROPIC:
			return "FILTER_MAXIMUM_ANISOTROPIC";
		}

		return "UNKNOWN";
	}

	std::string TextureAddressModeToString(D3D12_TEXTURE_ADDRESS_MODE Mode)
	{
		switch (Mode)
		{
			case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return "TEXTURE_ADDRESS_WRAP";
			case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return "TEXTURE_ADDRESS_MIRROR";
			case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return "TEXTURE_ADDRESS_CLAMP";
			case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return "TEXTURE_ADDRESS_BORDER";
			case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return "TEXTURE_ADDRESS_MIRROR_ONCE";
		}

		return "UNKNOWN";
	}

	std::string ComparisonFuncToString(D3D12_COMPARISON_FUNC Func)
	{
		switch (Func)
		{
			case D3D12_COMPARISON_FUNC_NEVER: return "COMPARISON_NEVER";
			case D3D12_COMPARISON_FUNC_LESS: return "COMPARISON_LESS";
			case D3D12_COMPARISON_FUNC_EQUAL: return "COMPARISON_EQUAL";
			case D3D12_COMPARISON_FUNC_LESS_EQUAL: return "COMPARISON_LESS_EQUAL";
			case D3D12_COMPARISON_FUNC_GREATER: return "COMPARISON_GREATER";
			case D3D12_COMPARISON_FUNC_NOT_EQUAL: return "COMPARISON_NOT_EQUAL";
			case D3D12_COMPARISON_FUNC_GREATER_EQUAL: return "COMPARISON_GREATER_EQUAL";
			case D3D12_COMPARISON_FUNC_ALWAYS: return "COMPARISON_ALWAYS";
		}

		return "UNKNOWN";
	}

	std::string ShaderVisibilityToString(D3D12_SHADER_VISIBILITY Visibility)
	{
		switch (Visibility)
		{
			case D3D12_SHADER_VISIBILITY_ALL: return "SHADER_VISIBILITY_ALL";
			case D3D12_SHADER_VISIBILITY_VERTEX: return "SHADER_VISIBILITY_VERTEX";
			case D3D12_SHADER_VISIBILITY_HULL: return "SHADER_VISIBILITY_HULL";
			case D3D12_SHADER_VISIBILITY_DOMAIN: return "SHADER_VISIBILITY_DOMAIN";
			case D3D12_SHADER_VISIBILITY_GEOMETRY: return "SHADER_VISIBILITY_GEOMETRY";
			case D3D12_SHADER_VISIBILITY_PIXEL: return "SHADER_VISIBILITY_PIXEL";
			case D3D12_SHADER_VISIBILITY_AMPLIFICATION: return "SHADER_VISIBILITY_AMPLIFICATION";
			case D3D12_SHADER_VISIBILITY_MESH: return "SHADER_VISIBILITY_MESH";
		}

		return "UNKNOWN";
	}

	std::string RootDescriptorFlagsToString(D3D12_ROOT_DESCRIPTOR_FLAGS Flags)
	{
		std::string s;

		auto add = [&](const std::string& Value)
		{
			if (s.empty())
				s = Value;
			else
				s += " | " + Value;
		};

		if (Flags == D3D12_ROOT_DESCRIPTOR_FLAG_NONE)
			add("0");

		if (Flags & D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE)
			add("DATA_VOLATILE");

		if (Flags & D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE)
			add("DATA_STATIC_WHILE_SET_AT_EXECUTE");

		if (Flags & D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC)
			add("DATA_STATIC");

		return s;
	}

	std::string RootDescriptorRangeFlagsToString(D3D12_DESCRIPTOR_RANGE_FLAGS Flags)
	{
		std::string s;

		auto add = [&](const std::string& Value)
		{
			if (s.empty())
				s = Value;
			else
				s += " | " + Value;
		};

		if (Flags == D3D12_DESCRIPTOR_RANGE_FLAG_NONE)
			add("0");

		if (Flags & D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE)
			add("DESCRIPTORS_VOLATILE");

		if (Flags & D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE)
			add("DATA_VOLATILE");

		if (Flags & D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE)
			add("DATA_STATIC_WHILE_SET_AT_EXECUTE");

		if (Flags & D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC)
			add("DATA_STATIC");

		if (Flags & D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS)
			add("DESCRIPTORS_STATIC_KEEPING_BUFFER_BOUNDS_CHECKS");

		return s;
	}

	std::string RootFlagsToString(D3D12_ROOT_SIGNATURE_FLAGS Flags)
	{
		std::string s;

		auto add = [&](const std::string& Value)
		{
			if (s.empty())
				s = Value;
			else
				s += " | " + Value;
		};

		if (Flags == D3D12_ROOT_SIGNATURE_FLAG_NONE)
			add("0");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
			add("ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS)
			add("DENY_VERTEX_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS)
			add("DENY_HULL_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS)
			add("DENY_DOMAIN_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS)
			add("DENY_GEOMETRY_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS)
			add("DENY_PIXEL_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT)
			add("ALLOW_STREAM_OUTPUT");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE)
			add("LOCAL_ROOT_SIGNATURE");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_AMPLIFICATION_SHADER_ROOT_ACCESS)
			add("DENY_AMPLIFICATION_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_DENY_MESH_SHADER_ROOT_ACCESS)
			add("DENY_MESH_SHADER_ROOT_ACCESS");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED)
			add("CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED");

		if (Flags & D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED)
			add("SAMPLER_HEAP_DIRECTLY_INDEXED");

		return std::format("RootFlags({})", s);
	}

	std::string RootConstantsToString(const D3D12_ROOT_CONSTANTS& Constants, D3D12_SHADER_VISIBILITY Visibility)
	{
		auto s = std::format("RootConstants(num32BitConstants={}, b{}", Constants.Num32BitValues, Constants.ShaderRegister);

		if (Constants.RegisterSpace != 0)
			s += std::format(", space={}", Constants.RegisterSpace);

		if (Visibility != D3D12_SHADER_VISIBILITY_ALL)
			s += std::format(", visibility={}", ShaderVisibilityToString(Visibility));

		s += ")";
		return s;
	}

	std::string RootParameterToString(D3D12_ROOT_PARAMETER_TYPE Type, const D3D12_ROOT_DESCRIPTOR1& Param, D3D12_SHADER_VISIBILITY Visibility)
	{
		std::string s;
		D3D12_ROOT_DESCRIPTOR_FLAGS defaultFlags = {};

		switch (Type)
		{
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
			s = std::format("CBV(b{}", Param.ShaderRegister);
			defaultFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
			break;

		case D3D12_ROOT_PARAMETER_TYPE_SRV:
			s = std::format("SRV(t{}", Param.ShaderRegister);
			defaultFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
			break;

		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			s = std::format("UAV(u{}", Param.ShaderRegister);
			defaultFlags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
			break;
		}

		if (Param.RegisterSpace != 0)
			s += std::format(", space={}", Param.RegisterSpace);

		if (Visibility != D3D12_SHADER_VISIBILITY_ALL)
			s += std::format(", visibility={}", ShaderVisibilityToString(Visibility));

		if (Param.Flags != defaultFlags)
			s += std::format(", flags={}", RootDescriptorFlagsToString(Param.Flags));

		s += ")";
		return s;
	}

	std::string RootDescriptorTableToString(const D3D12_ROOT_DESCRIPTOR_TABLE1& Table, D3D12_SHADER_VISIBILITY Visibility)
	{
		std::string s = "DescriptorTable(\n";

		for (uint32_t i = 0; i < Table.NumDescriptorRanges; i++)
		{
			auto& descriptor = Table.pDescriptorRanges[i];
			D3D12_DESCRIPTOR_RANGE_FLAGS defaultFlags = {};

			if (i != 0)
				s += ",\n";

			switch (descriptor.RangeType)
			{
			case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				s += std::format("CBV(b{}", descriptor.BaseShaderRegister);
				defaultFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
				break;

			case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
				s += std::format("SRV(t{}", descriptor.BaseShaderRegister);
				defaultFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;
				break;

			case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
				s += std::format("UAV(u{}", descriptor.BaseShaderRegister);
				defaultFlags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
				break;

			case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
				s += std::format("Sampler(s{}", descriptor.BaseShaderRegister);
				defaultFlags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
				break;
			}

			if (descriptor.NumDescriptors != 1)
				s += std::format(", numDescriptors={}", descriptor.NumDescriptors);

			if (descriptor.RegisterSpace != 0)
				s += std::format(", space={}", descriptor.RegisterSpace);

			if (descriptor.OffsetInDescriptorsFromTableStart != D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
				s += std::format(", offset={}", descriptor.OffsetInDescriptorsFromTableStart);

			if (descriptor.Flags != defaultFlags)
				s += std::format(", flags={}", RootDescriptorRangeFlagsToString(descriptor.Flags));

			s += ")";
		}

		if (Visibility != D3D12_SHADER_VISIBILITY_ALL)
			s += std::format(", visibility={}", ShaderVisibilityToString(Visibility));

		s += "\n)";
		return s;
	}

	std::string StaticSamplerToString(const D3D12_STATIC_SAMPLER_DESC& Sampler)
	{
		std::string s = std::format("StaticSampler(s{}", Sampler.ShaderRegister);
		
		if (Sampler.Filter != D3D12_FILTER_ANISOTROPIC)
			s += std::format(", filter={}", TextureFilterModeToString(Sampler.Filter));

		if (Sampler.AddressU != D3D12_TEXTURE_ADDRESS_MODE_WRAP)
			s += std::format(", addressU={}", TextureAddressModeToString(Sampler.AddressU));

		if (Sampler.AddressV != D3D12_TEXTURE_ADDRESS_MODE_WRAP)
			s += std::format(", addressV={}", TextureAddressModeToString(Sampler.AddressV));

		if (Sampler.AddressW != D3D12_TEXTURE_ADDRESS_MODE_WRAP)
			s += std::format(", addressW={}", TextureAddressModeToString(Sampler.AddressW));

		if (Sampler.MipLODBias != 0.0f)
			s += std::format(", mipLODBias={}", Sampler.MipLODBias);

		if (Sampler.MaxAnisotropy != D3D12_MAX_MAXANISOTROPY)
			s += std::format(", maxAnisotropy={}", Sampler.MaxAnisotropy);

		if (Sampler.ComparisonFunc != D3D12_COMPARISON_FUNC_LESS_EQUAL)
			s += std::format(", comparisonFunc={}", ComparisonFuncToString(Sampler.ComparisonFunc));

		if (Sampler.BorderColor != D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
			s += std::format(", borderColor={}", (uint32_t)Sampler.BorderColor);

		if (Sampler.MinLOD != 0.0f)
			s += std::format(", minLOD={}", Sampler.MinLOD);

		if (Sampler.MaxLOD != D3D12_FLOAT32_MAX)
			s += std::format(", maxLOD={}", Sampler.MaxLOD);

		if (Sampler.RegisterSpace != 0)
			s += std::format(", space={}", Sampler.RegisterSpace);

		if (Sampler.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL)
			s += std::format(", visibility={}", ShaderVisibilityToString(Sampler.ShaderVisibility));

		s += ")";
		return s;
	}

	std::string RootSignatureToString(const D3D12_ROOT_SIGNATURE_DESC1& RootSig)
	{
		std::string sig = RootFlagsToString(RootSig.Flags) + ",\n";

		// Decipher the root signature parameters
		for (uint32_t i = 0; i < RootSig.NumParameters; i++)
		{
			const auto& param = RootSig.pParameters[i];

			switch (param.ParameterType)
			{
			case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
				sig += RootDescriptorTableToString(param.DescriptorTable, param.ShaderVisibility);
				break;

			case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
				sig += RootConstantsToString(param.Constants, param.ShaderVisibility);
				break;

			case D3D12_ROOT_PARAMETER_TYPE_CBV:
			case D3D12_ROOT_PARAMETER_TYPE_SRV:
			case D3D12_ROOT_PARAMETER_TYPE_UAV:
				sig += RootParameterToString(param.ParameterType, param.Descriptor, param.ShaderVisibility);
				break;

			default:
				sig += std::format("UNKNOWN_TYPE_{}()", static_cast<uint32_t>(param.ParameterType));
				break;
			}

			sig += std::format(", // Index = {}\n", i);
		}

		// Then embedded samplers
		for (uint32_t i = 0; i < RootSig.NumStaticSamplers; i++)
		{
			const auto& sampler = RootSig.pStaticSamplers[i];

			sig += StaticSamplerToString(sampler);
			sig += ",\n";
		}

		return sig;
	}

	bool PatchPipelineStateStream(
		D3DPipelineStateStream::Copy& StreamCopy,
		ID3D12Device2 *Device,
		const std::span<const uint8_t> *RootSignatureData,
		const char *TechniqueName,
		uint64_t TechniqueId)
	{
		bool modified = false;

#if 0
		if (TechniqueId == 0x2FF7F ||
			TechniqueId == 0x400FF59 ||
			TechniqueId == 0xFF1A ||
			TechniqueId == 0xFF75 ||
			TechniqueId == 0xFF81 ||
			TechniqueId == 0xFF94 ||
			TechniqueId == 0xFF9A ||
			TechniqueId == 0xFFAA ||
			TechniqueId == 0xFFAB)
		{
			ID3D12VersionedRootSignatureDeserializer *itf;
			D3D12CreateVersionedRootSignatureDeserializer(RootSignatureData->data(), RootSignatureData->size(), IID_PPV_ARGS(&itf));

			auto test = &itf->GetUnconvertedRootSignatureDesc()->Desc_1_1;
			auto strTest = RootSignatureToString(*test);

			const auto txtPath = GetShaderBinDirectory() / std::format("{:X}.txt", TechniqueId);

			if (std::ofstream f(txtPath, std::ios::out); f.good())
				f.write(strTest.c_str(), strTest.length());
		}
#endif

		for (D3DPipelineStateStream::Iterator iter(StreamCopy.GetDesc()); !iter.AtEnd(); iter.Advance())
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
				if (ExtractOrReplaceShader(StreamCopy, obj->Type, &obj->Shader, TechniqueName, TechniqueId))
					modified = true;
				break;

			case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
				if (RootSignatureData)
				{
					D3D12_SHADER_BYTECODE bytecode
					{
						.pShaderBytecode = RootSignatureData->data(),
						.BytecodeLength = RootSignatureData->size(),
					};

					if (ExtractOrReplaceShader(StreamCopy, obj->Type, &bytecode, TechniqueName, TechniqueId))
					{
						ID3D12RootSignature *newSignature = nullptr;
						const auto hr = Device->CreateRootSignature(0, bytecode.pShaderBytecode, bytecode.BytecodeLength, IID_PPV_ARGS(&newSignature));

						if (FAILED(hr))
						{
							// Somebody passed in malformed data
							spdlog::error("Failed to create root signature: {:X}. Shader technique: {:X}.", static_cast<uint32_t>(hr), TechniqueId);
						}
						else
						{
							obj->RootSignature = newSignature;
							StreamCopy.TrackObject(std::move(newSignature));

							modified = true;
						}
					}
				}
				break;
			}
		}

		// Loop around once again to disable PSO cache entries
		if (modified)
		{
			for (D3DPipelineStateStream::Iterator iter(StreamCopy.GetDesc()); !iter.AtEnd(); iter.Advance())
			{
				switch (auto obj = iter.GetObj(); obj->Type)
				{
				case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
					obj->CachedPSO.pCachedBlob = nullptr;
					obj->CachedPSO.CachedBlobSizeInBytes = 0;
					break;
				}
			}
		}

		return modified;
	}
}
