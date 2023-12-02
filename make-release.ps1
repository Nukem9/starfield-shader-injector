# Set up powershell equivalent of vcvarsall.bat
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationpath

Import-Module (Get-ChildItem $vsPath -Recurse -File -Filter Microsoft.VisualStudio.DevShell.dll).FullName
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64'

# Then build with VS
& cmake --preset asiloader
& cmake --build --preset asiloader-release
& cpack --preset asiloader

& cmake --preset sfse
& cmake --build --preset sfse-release
& cpack --preset sfse