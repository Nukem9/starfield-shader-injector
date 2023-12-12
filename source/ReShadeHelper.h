#pragma once

#include <reshade-api/reshade.hpp>
#include <atomic>
#include "CComPtr.h"

namespace ReShadeHelper
{
	// Maps a reshade::api::* object to a native D3D12 object via ID3D12Object::GetPrivateData()
	constexpr GUID IID_NativeToReShade = { 0x8e315253, 0x21a9, 0x4309, { 0xbc, 0x17, 0x8e, 0xcc, 0x76, 0x24, 0x33, 0x95 } };

	// ReShade internal https://github.com/crosire/reshade/blob/main/source/d3d12/d3d12_device.hpp#L12 via ID3D12Object::GetPrivateData()
	constexpr GUID IID_ReShadeD3D12DevicePrivateData = { 0x2523aff4, 0x978b, 0x4939, { 0xba, 0x16, 0x8e, 0xe8, 0x76, 0xa4, 0xcb, 0x2a } };

	// Maps a reshade::api::effect_runtime to a native D3D12 device via reshade::api_object::get_private_data()
	constexpr GUID IID_ReShadeEffectRuntime = { 0x358d5ebf, 0x15aa, 0x4bae, { 0x89, 0x50, 0x12, 0x01, 0xa4, 0x25, 0xbc, 0x2f } };

	// Command list submission callback via reshade::api_object::get_private_data()
	constexpr GUID IID_CommandListSubmitCallback = { 0xec8960f3, 0x328c, 0x4091, { 0x7f, 0x17, 0x10, 0x23, 0x22, 0xee, 0x51, 0x9e } };

	struct EffectDepthCopy
	{
		decltype(reshade::api::resource_desc::texture) Format = {};
		reshade::api::resource Resource = {};
		reshade::api::resource_view ResourceView = {};
		uint64_t LastFrameIndex = 0;
	};

	struct __declspec(uuid("fdc21f1f-7bef-418f-8e34-10eb86add2e4")) EffectRuntimeConfiguration
	{
		bool m_DrawEffectsBeforeUI = false;
		bool m_AutomaticDepthBufferSelection = false;

		std::mutex DepthBufferListMutex;
		CComPtr<ID3D12Fence> DepthTrackingFence;
		reshade::api::resource_view UpdateHint = {};
		std::atomic_uint64_t DepthTrackingFrameIndex = 0;
		std::vector<EffectDepthCopy> DepthBufferCopies;

		EffectRuntimeConfiguration(reshade::api::effect_runtime *Runtime);
		void Load(reshade::api::effect_runtime *Runtime);
		void Save(reshade::api::effect_runtime *Runtime);
	};

	class CommandListLock
	{
	private:
		static inline std::atomic_flag GlobalLock;

		CommandListLock(const CommandListLock&) = delete;
		CommandListLock& operator=(const CommandListLock&) = delete;

	public:
		CommandListLock()
		{
			while (GlobalLock.test_and_set(std::memory_order_acquire))
			{
				while (GlobalLock.test(std::memory_order_relaxed))
					_mm_pause();
			}
		}

		~CommandListLock()
		{
			GlobalLock.clear(std::memory_order_release);
		}
	};

	template<typename T>
	requires(sizeof(T) <= sizeof(uint64_t) && std::is_trivially_copyable_v<T>)
	void SetImplData(ID3D12Object *Object, const GUID& Guid, const T& Data)
	{
		if constexpr (std::is_same_v<T, std::nullptr_t>)
			Object->SetPrivateData(Guid, 0, nullptr);
		else
			Object->SetPrivateData(Guid, sizeof(T), std::addressof(Data));
	}

	template<typename T>
	requires(sizeof(T) <= sizeof(uint64_t) && std::is_trivially_copyable_v<T>)
	void SetImplData(reshade::api::api_object *Object, const GUID& Guid, const T& Data)
	{
		uint64_t value = {};

		if constexpr (!std::is_same_v<T, std::nullptr_t>)
			memcpy(&value, std::addressof(Data), sizeof(T));

		Object->set_private_data(reinterpret_cast<const uint8_t *>(&Guid), value);
	}

	template<typename T>
	requires(sizeof(T) <= sizeof(uint64_t) && std::is_trivially_copyable_v<T>)
	T GetImplData(ID3D12Object *Object, const GUID& Guid)
	{
		T value = {};
		uint32_t size = sizeof(T);

		Object->GetPrivateData(Guid, &size, std::addressof(value));
		return value;
	}

	template<typename T>
	requires(sizeof(T) <= sizeof(uint64_t) && std::is_trivially_copyable_v<T>)
	T GetImplData(reshade::api::api_object *Object, const GUID& Guid)
	{
		uint64_t temp = {};
		Object->get_private_data(reinterpret_cast<const uint8_t *>(&Guid), &temp);

		T value = {};
		memcpy(std::addressof(value), &temp, sizeof(T));

		return value;
	}

	void Initialize();
}
