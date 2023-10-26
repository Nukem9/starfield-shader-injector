#pragma once

namespace Plugin
{
	extern bool AllowLiveUpdates;
	extern bool InsertDebugMarkers;
	extern std::filesystem::path ShaderDumpBinPath;

	bool Initialize(bool UseASI);
	bool InitializeLog(bool UseASI);
	bool InitializeSettings();
}
