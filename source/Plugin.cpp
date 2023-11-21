#include <spdlog/sinks/basic_file_sink.h>
#include <toml++/toml.h>
#include <Windows.h>
#include <ShlObj.h>
#include "Plugin.h"

namespace Plugin
{
	bool AllowLiveUpdates = false;
	bool InsertDebugMarkers = false;
	std::filesystem::path ShaderDumpBinPath;

	bool Initialize(bool UseASI)
	{
		InitializeSettings();

		if (!InitializeLog(UseASI))
			return false;

		if (!Offsets::Initialize())
			return false;

		if (!Hooks::Initialize())
			return false;

		return true;
	}

	bool InitializeLog(bool UseASI)
	{
		// Initialize logging in the documents folder
		wchar_t *documentsPath = nullptr;

		if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &documentsPath)))
			return false;

		std::filesystem::path logPath(documentsPath);
		logPath.append(UseASI ? L"My Games\\Starfield\\Logs" : L"My Games\\Starfield\\SFSE\\Logs");
		logPath.append(L"SFShaderInjector.log");

		auto logger = spdlog::basic_logger_mt("file_logger", logPath.string(), true);
		logger->set_level(spdlog::level::level_enum::trace);
		logger->set_pattern("[%H:%M:%S] [%l] %v"); // [HH:MM:SS] [Level] Message
		logger->flush_on(logger->level());
		spdlog::set_default_logger(std::move(logger));

		spdlog::info(
			"Starfield Shader Injector {} version {}.{} by Nukem. Mod URL: https://www.nexusmods.com/starfield/mods/5562",
			UseASI ? "ASI" : "SFSE",
			BUILD_VERSION_MAJOR,
			BUILD_VERSION_MINOR);

		CoTaskMemFree(documentsPath);
		return true;
	}

	bool InitializeSettings()
	{
		// Grab the full path of this dll and change the extension to .ini
		HMODULE dllHandle = nullptr;
		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&InitializeSettings),
			&dllHandle);

		wchar_t dllPath[1024] = {};
		if (GetModuleFileNameW(dllHandle, dllPath, static_cast<uint32_t>(std::size(dllPath))) == 0)
			return false;

		std::filesystem::path iniPath(dllPath);
		iniPath.replace_extension(L".ini");

		// Then parse the fake .ini as TOML
		try
		{
			auto toml = toml::parse_file(iniPath.string());

			if (toml.get("Development"))
			{
				AllowLiveUpdates = toml["Development"]["AllowLiveUpdates"].value_or(false);
				InsertDebugMarkers = toml["Development"]["InsertDebugMarkers"].value_or(false);
				ShaderDumpBinPath = toml["Development"]["ShaderDumpBinPath"].value_or(L"");
			}

			if (!ShaderDumpBinPath.empty())
				AllowLiveUpdates = false;
		}
		catch (const toml::parse_error&)
		{
			return false;
		}

		return true;
	}
}

#if BUILD_FOR_SFSE
#include <sfse_common/sfse_version.h>
#include <sfse/PluginAPI.h>

extern "C" __declspec(dllexport) const SFSEPluginVersionData SFSEPlugin_Version {
	SFSEPluginVersionData::kVersion,

	(100 * BUILD_VERSION_MAJOR) + BUILD_VERSION_MINOR,			 // Plugin version
	"SFShaderInjector",											 // Name
	"Nukem",													 // Author

	SFSEPluginVersionData::kAddressIndependence_Signatures,		 // Address independent as of 1.8.86
	SFSEPluginVersionData::kStructureIndependence_InitialLayout, // Structure independent as of game release
																 // Compatible with 1.8.86 and beyond
	{
		RUNTIME_VERSION_1_8_86,
		0,
	},

	0, // Works with any version of the script extender
	0,
	0, // Reserved
};

extern "C" __declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface *Interface)
{
	return true;
}

extern "C" __declspec(dllexport) bool SFSEPlugin_Preload(const SFSEInterface *Interface)
{
	return Plugin::Initialize(false);
}
#endif // BUILD_FOR_SFSE

#if BUILD_FOR_ASILOADER
extern "C" __declspec(dllexport) void InitializeASI()
{
	auto module = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
	auto ntHeaders = reinterpret_cast<const PIMAGE_NT_HEADERS>(module + reinterpret_cast<PIMAGE_DOS_HEADER>(module)->e_lfanew);

	auto& directory = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
	auto descriptor = reinterpret_cast<const PIMAGE_EXPORT_DIRECTORY>(module + directory.VirtualAddress);

	// Plugin dlls can be loaded into non-game processes when people use broken ASI loader setups. The only
	// version-agnostic and file-name-agnostic method to detect Starfield.exe is to check the export directory
	// name.
	if (directory.VirtualAddress == 0 || directory.Size == 0 ||
		memcmp(reinterpret_cast<void *>(module + descriptor->Name), "Starfield.exe", 14) != 0)
		return;

	Plugin::Initialize(true);
}
#endif // BUILD_FOR_ASILOADER
