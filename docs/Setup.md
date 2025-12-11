# Setup
weBIGeo can be deployed to the web via emscripten and additionally we support native builds on Windows, using [Dawn](https://dawn.googlesource.com/) and [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2). This allows for faster development as emscripten linking is quite slow and GPU debugging is not easily possible in the web.

## Building the web version

### Dependencies
* Qt 6.10.1 with
  * WebAssembly (multi-threaded) pre-built binaries
* Python 3
* cmake and ninja (come with Qt)
* emsdk 4.0.7 ([emscripten](https://emscripten.org/docs/getting_started/downloads.html))
```
emsdk install 4.0.7 & emsdk activate 4.0.7
```

### Qt-Creator Setup
This step is specifically tailored to the Qt-Creator IDE.

1. `Preferences -> Devices -> WebAssembly`: Set path to the emsdk git repository

[![Emscripten Path Qt Creator](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator.jpg?raw=true) 


## Building the native version

### Dependencies
* Windows
* Qt 6.10.1 with
  * MSVC2022 pre-built binaries
* Python 3
* Microsoft Visual C++ Compiler 17.6 (aka. MSVC2022, comes with Visual Studio 2022)
* cmake and ninja (come with Qt)

> [!IMPORTANT]
> Dawn (in 2024) does not compile with MinGW! GCC might be possible, but is not tested (hence the Windows Requirement).

### Troubleshoot
- MY CONFIGURATION TAKES FOREVER: Upon first cmake configuration DAWN as well as SDL is being pulled, build and installed. This might take a while. (~10-40 min)
- Dawn and SDL installation as well as fetching the custom dawn port for emscripten now happens in the python scripts inside the respective folder. They get executed by the CMAKE-Setup. A change requires a reconfiguration of CMAKE as well as the deletion of the directory in the `extern` directory.
- If you have issues with your currently installed Vulkan SDK you may try one or all of the following:
  - disable `DDAWN_FORCE_SYSTEM_COMPONENT_LOAD`
  - Try with a different DAWN backend
  - copying the include files from your sdk into the respective folders in the dawn binaries `dawn\third_party\vulkan-utility-libraries\src\include\vulkan\utility\` and `dawn\third_party\vulkan-headers\src\include\vulkan\` and rebuild dawn.

### About DAWN Backends
Per default we opt for an only Vulkan-Backend Build for two reasons:
- Vulkan is probably the most supported Backend running on most devices
- We have more knowledge about Vulkan which comes to play when we use GPU debuggers

That being said you may enable different Backends in the `install_dawn.py` script.