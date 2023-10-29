# Starfield Shader Injector

A plugin that allows people to use custom shaders without needing to rebuild or extract Starfield's BA2 shader package. Live shader updates are also supported for easy debugging at runtime.

## Building

- CMake and vcpkg are expected to be set up beforehand. Visual Studio 2022 is recommended.
- \<preset\> is `asi-rel`, `asi-dbg`, `sfse-rel`, or `sfse-dbg`. 

```
git clone --recurse-submodules https://github.com/Nukem9/sf-shader-injector.git
cmake --preset <preset>
cmake --build bin/<preset>
```

## Installation

- For developers, edit `CMakeUserEnvVars.json` and set `GAME_ROOT_DIRECTORY` to Starfield's root directory. The build script will automatically copy library files to the game folder.

- For manual SFSE installs, place `SFShaderInjector.dll` in the corresponding `Starfield\Data\SFSE\Plugins` folder. An example path is: `C:\steamapps\common\Starfield\Data\SFSE\Plugins\SFShaderInjector.dll`

- For manual ASI loader installs, place `SFShaderInjector.asi` in the game root directory next to `Starfield.exe`. An example path is: `C:\XboxGames\Starfield\Content\SFShaderInjector.asi`


## License

- TBD.
- Dependencies are under their respective licenses.