#include "DebuggingUtil.h"
#include "Plugin.h"

namespace DebuggingUtil
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

	void SetObjectDebugName(ID3D12Object *Object, const char *Name)
	{
		if (!Plugin::InsertDebugMarkers)
			return;

		if (!Object || !Name || strlen(Name) <= 0)
			return;

		wchar_t tempOut[1024];
		if (mbstowcs_s(nullptr, tempOut, Name, _TRUNCATE) == 0)
			Object->SetName(tempOut);
	}

	void (*OriginalCmdBeginProfilingMarker)(void *, void *, const char *);
	void HookedCmdBeginProfilingMarker(void *a1, void *a2, const char *MarkerText)
	{
		auto commandList = *reinterpret_cast<ID3D12GraphicsCommandList **>(reinterpret_cast<uintptr_t>(a1) + 0x60);
		commandList->BeginEvent(1, MarkerText, static_cast<uint32_t>(strlen(MarkerText) + 1));

		OriginalCmdBeginProfilingMarker(a1, a2, MarkerText);
	}

	void (*OriginalCmdEndProfilingMarker)(void *);
	void HookedCmdEndProfilingMarker(void *a1)
	{
		OriginalCmdEndProfilingMarker(a1);

		auto commandList = *reinterpret_cast<ID3D12GraphicsCommandList **>(reinterpret_cast<uintptr_t>(a1) + 0x60);
		commandList->EndEvent();
	}

	DECLARE_HOOK_TRANSACTION(DebuggingUtil)
	{
		if (!Plugin::InsertDebugMarkers)
			return;

		Hooks::WriteJump(
			Offsets::Signature("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 44 88 4C 24 20 57 41 56 41 57 48 83 EC 20"),
			&HookedCmdBeginProfilingMarker,
			&OriginalCmdBeginProfilingMarker);

		Hooks::WriteJump(
			Offsets::Signature("48 89 5C 24 08 48 89 74 24 18 88 54 24 10 57 41 56 41 57 48 83 EC 20 48 8B F1"),
			&HookedCmdEndProfilingMarker,
			&OriginalCmdEndProfilingMarker);
	};
}
