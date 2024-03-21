#pragma once

struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace CreationRenderer
{
	constexpr uint32_t TotalTechniqueTypeCount = 234;

	enum class ShaderType : uint8_t
	{
		Invalid = 0,
		Graphics = 1,
		Compute = 2,
		RayTracing = 3,
	};

	class PipelineLayoutDx12
	{
	private:
		PipelineLayoutDx12() = delete;
		virtual ~PipelineLayoutDx12();

	public:
		void *m_LayoutConfigurationData;	  // 0x8
		char _pad0[0x68];					  // 0x10
		ID3D12RootSignature *m_RootSignature; // 0x78 Ref counted
		char _pad1[0x8];					  // 0x80
	};
	static_assert(sizeof(PipelineLayoutDx12) == 0x88);
	static_assert(offsetof(PipelineLayoutDx12, m_RootSignature) == 0x78);

	class ShaderInputsContainerDx12
	{
	private:
		ShaderInputsContainerDx12() = delete;

	public:
		char _pad0[0x38];					// 0x0
		const uint8_t *m_RootSignatureBlob; // 0x38
		char _pad1[0x8];					// 0x40
		uint32_t m_RootSignatureBlobSize;	// 0x48
	};
	static_assert(offsetof(ShaderInputsContainerDx12, m_RootSignatureBlob) == 0x38);
	static_assert(offsetof(ShaderInputsContainerDx12, m_RootSignatureBlobSize) == 0x48);

	class TechniqueData
	{
	private:
		TechniqueData() = delete;

	public:
		uint32_t m_Type;					  // 0x0 ShaderType
		ShaderInputsContainerDx12 *m_Inputs;  // 0x8
		char _pad1[0x50];					  // 0x10
		uint64_t m_Id;						  // 0x60
		char _pad4[0x8];					  // 0x68
		const char *m_Name;					  // 0x70
		ID3D12PipelineState *m_PipelineState; // 0x78
	};
	static_assert(offsetof(TechniqueData, m_Inputs) == 0x8);
	static_assert(offsetof(TechniqueData, m_Id) == 0x60);
	static_assert(offsetof(TechniqueData, m_Name) == 0x70);

	class TechniqueInfoTable
	{
	public:
		struct ConfigurationEntry
		{
			uint64_t m_Unknown1;	// 0x0 Contains indices for tertiary arrays
			uint64_t m_Unknown2;	// 0x8
			const char *m_FullName; // 0x10
		};
		static_assert(sizeof(ConfigurationEntry) == 0x18);

		uint8_t m_BaseTypeIndex;				 // 0x0
		uint8_t m_Unknown1;						 // 0x1
		uint16_t m_TechniqueCount;				 // 0x2
		uint8_t m_Unknown2;						 // 0x4
		bool m_Unknown3;						 // 0x5
		bool m_IsPrecompiled;					 // 0x6
		uint8_t m_Unknown4;						 // 0x7
		uint64_t *m_TechniqueIds;				 // 0x8
		ConfigurationEntry *m_ConfigurationData; // 0x10
		const char *m_BaseTypeName;				 // 0x18

#if 0
		static TechniqueInfoTable *LookupTable(uint32_t TechniqueTypeIndex);
#endif
	};

	struct Dx12Unknown
	{
		char _pad0[0x48];								  // 00
		class Dx12Resource *m_Resource;					  // 48
		char _pad1[0x18];								  // 50
		D3D12_CPU_DESCRIPTOR_HANDLE *m_RTVCpuDescriptors; // 68
	};

	ID3D12CommandList *GetRenderGraphCommandList(void *RenderGraphData);
	Dx12Unknown *AcquireRenderPassRenderTarget(void *RenderPassData, uint32_t RenderTargetId);
	Dx12Unknown *AcquireRenderPassSingleInput(void *RenderPassData);
	Dx12Unknown *AcquireRenderPassSingleOutput(void *RenderPassData);
}
