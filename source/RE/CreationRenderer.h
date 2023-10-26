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
	public:
		PipelineLayoutDx12() = delete; // 1433D3670 Steam 1.7.29
		virtual ~PipelineLayoutDx12();

		void *m_LayoutConfigurationData;		// 0x8
		char _pad0[0x68];						// 0x10
		ID3D12RootSignature *m_RootSignature;	// 0x78 Ref counted
		char _pad1[0x8];						// 0x80
	};
	static_assert(sizeof(PipelineLayoutDx12) == 0x88);
	static_assert(offsetof(PipelineLayoutDx12, m_RootSignature) == 0x78);

	class ShaderInputsContainerDx12
	{
	public:
		ShaderInputsContainerDx12() = delete; // 1432E6F30 Steam 1.7.29

		void *m_Unknown1;					// 0x0
		void *m_ConstantDataBuffers[10];	// 0x8 Guessed
		uint32_t m_ConstantDataSizes[10];	// 0x48
		const uint8_t *m_RootSignatureBlob;	// 0x80
		void *m_Unknown2;					// 0x88 Ref counted
		uint32_t m_RootSignatureBlobSize;	// 0x90
	};
	static_assert(sizeof(ShaderInputsContainerDx12) == 0x98);

	class TechniqueData
	{
	public:
		uint32_t m_Type;								// 0x0 ShaderType as uint32
		ShaderInputsContainerDx12 *m_Inputs;			// 0x8
		char _pad1[0x20];								// 0x10
		PipelineLayoutDx12 *m_PipelineLayout;			// 0x30
		char _pad2[0x40];								// 0x38 std::variant<> at 0x70
		uint64_t m_Id;									// 0x78
		char _pad3[0x8];								// 0x80
		const char *m_Name;								// 0x88
		ID3D12PipelineState *m_PipelineState;			// 0x90 Ref counted?
		ID3D12PipelineState *m_RayTracingPipelineState;	// 0x98 Ref counted?
	};
	static_assert(offsetof(TechniqueData, m_Inputs) == 0x8);
	static_assert(offsetof(TechniqueData, m_PipelineLayout) == 0x30);
	static_assert(offsetof(TechniqueData, m_Id) == 0x78);
	static_assert(offsetof(TechniqueData, m_Name) == 0x88);

	class TechniqueInfoTable
	{
	public:
		struct ConfigurationEntry
		{
			uint64_t m_Unknown1;	// 0x0 Contains indices for tertiary arrays
			uint64_t m_Unknown2;	// 0x8
			const char *m_FullName;	// 0x10
		};
		static_assert(sizeof(ConfigurationEntry) == 0x18);

		uint8_t m_BaseTypeIndex;					// 0x0
		uint8_t m_Unknown1;							// 0x1
		uint16_t m_TechniqueCount;					// 0x2
		uint8_t m_Unknown2;							// 0x4
		bool m_Unknown3;							// 0x5
		bool m_IsPrecompiled;						// 0x6
		uint8_t m_Unknown4;							// 0x7
		uint64_t *m_TechniqueIds;					// 0x8
		ConfigurationEntry *m_ConfigurationData;	// 0x10
		const char *m_BaseTypeName;					// 0x18

#if 0
		static TechniqueInfoTable *LookupTable(uint32_t TechniqueTypeIndex); // 14334BC70 Steam 1.7.29
#endif
	};
}
