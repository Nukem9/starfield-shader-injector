#pragma once

struct ID3D12RootSignature;
struct ID3D12PipelineState;

namespace CreationRenderer
{
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

	public:
		void *m_LayoutConfigurationData;	  // 0x0
		char _pad0[0x58];					  // 0x8
		ID3D12RootSignature *m_RootSignature; // 0x60 Ref counted
	};
	static_assert(offsetof(PipelineLayoutDx12, m_RootSignature) == 0x60);

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
		char _pad0[0x8];					  // 0x0
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

	struct Dx12Resource;

	struct Dx12Unknown
	{
		char _pad0[0x8];								  // 00
		D3D12_CPU_DESCRIPTOR_HANDLE *m_RTVCpuDescriptors; // 08
		char _pad1[0x48];								  // 10
		Dx12Resource *m_Resource;						  // 58
	};

	ID3D12CommandList *GetRenderGraphCommandList(void *RenderGraphData);
	Dx12Unknown *AcquireRenderPassIO(void *RenderPassData, uint32_t IOIndex);
}
