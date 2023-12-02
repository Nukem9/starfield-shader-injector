# Starfield Shader Injector

A plugin that allows people to use custom shaders without needing to rebuild or extract Starfield's BA2 shader package. Live shader updates are also supported for easy debugging at runtime.

## Building

- CMake and vcpkg are expected to be set up beforehand. Visual Studio 2022 is recommended.
- \<configure_preset\> is `asiloader` or `sfse`.
- \<build_preset\> is `asiloader-release`, `asiloader-debug`, `sfse-release`, or `sfse-debug`.

```
git clone --recurse-submodules https://github.com/Nukem9/sf-shader-injector.git
cmake --preset <configure_preset>
cmake --build --preset <build_preset>
```

## Installation

- For developers, edit `CMakeUserEnvVars.json` and set `GAME_ROOT_DIRECTORY` to Starfield's root directory. The build script will automatically copy library files to the game folder.

- For manual SFSE installs, place `SFShaderInjector.dll` in the corresponding `Starfield\Data\SFSE\Plugins` folder. An example path is: `C:\steamapps\common\Starfield\Data\SFSE\Plugins\SFShaderInjector.dll`

- For manual ASI loader installs, place `SFShaderInjector.asi` in the game root directory next to `Starfield.exe`. An example path is: `C:\XboxGames\Starfield\Content\SFShaderInjector.asi`

## Custom Shader Installation

- Shaders must be installed under the `Data\shadersfx` folder in the game's root directory.

- A Steam edition path looks like this: `C:\steamapps\common\Starfield\Data\shadersfx\ColorGradingMerge\ColorGradingMerge_FF81_cs.bin`

- A Game Pass edition path looks like this: `C:\XboxGames\Starfield\Content\Data\shadersfx\ColorGradingMerge\ColorGradingMerge_FF81_cs.bin`

## License

- TBD.
- Dependencies are under their respective licenses.