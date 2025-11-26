# Setup
weBIGeo's primary target is the web. Additionally we support native builds on Windows, using [Dawn](https://dawn.googlesource.com/) and [SDL2](https://github.com/libsdl-org/SDL/tree/SDL2). This allows for faster development as emscripten linking is quite slow and GPU debugging is not easily possible in the web. Both setups require significant setup time as we need to compile Qt by source. For the native workflow also Dawn and SDL needs to be compiled. The rest of the dependencies should get automatically pulled by the cmake setup.

## Building the web version

### Dependencies
* Qt 6.9.2 with
  * MinGW (GCC on linux might work but not tested)
  * Qt Shader Tools (otherwise Qt configure fails)
  * Sources
* [emscripten](https://emscripten.org/docs/getting_started/downloads.html)
* To exactly follow along with build instructions you need `ninja`, `cmake` and `emsdk` in your PATH

### Building Qt with emscripten (singlethreaded)
For WebGPU to work we need to compile with emscripten version > 4.0.10. The default emscripten version, supported by Qt 6.9.2 is 3.1.70 (https://doc.qt.io/qt-6/wasm.html#installing-emscripten). However, newer emscripten releases include important features needed by Dawn and its implementation of webgpu.h. Therefore we need to recompile the Qt sources with the appropriate version by ourselves. (This step might be unnecessary in the near future with the release of [Qt 6.10](https://wiki.qt.io/Qt_6.10_Release)).

1. Open new CMD Terminal

2. Navigate to Qt Path:
   ```
   cd "C:\Qt\6.9.2"
   ```

3. Generate build and install path (and make sure they are empty)
   ```
   mkdir build & mkdir wasm_singlethread_emsdk_4_0_14_custom & cd build
   ```

4. Install and activate specific emsdk version
   ```
   emsdk install 4.0.14 & emsdk activate 4.0.14
   ```

5. Configure Qt with a minimal setup (takes ~ 4min)
   ```
   "../Src/configure" -debug-and-release -qt-host-path C:\Qt\6.9.2\msvc2022_64 -make libs -no-widgets -optimize-size -no-warnings-are-errors -platform wasm-emscripten -submodules qtdeclarative -no-dbus -no-sql-sqlite -feature-wasm-simd128 -no-feature-cssparser -no-feature-quick-treeview -no-feature-quick-pathview -no-feature-texthtmlparser -no-feature-textodfwriter -no-feature-quickcontrols2-windows -no-feature-quickcontrols2-macos -no-feature-quickcontrols2-ios -no-feature-quickcontrols2-imagine -no-feature-quickcontrols2-universal -no-feature-quickcontrols2-fusion -no-feature-qtprotobufgen -no-feature-pdf -no-feature-printer -no-feature-sqlmodel -no-feature-quick-pixmap-cache-threaded-download -feature-quick-canvas -no-feature-quick-designer -no-feature-quick-particles -no-feature-quick-sprite -no-feature-raster-64bit -no-feature-raster-fp -prefix ../wasm_singlethread_emscripten_4_0_14_custom
   ```

6. Build Qt (takes ~ 8min)
   ```
   cmake --build . --parallel
   ```

7. Install Qt (takes ~ 1min)
   ```
   cmake --install .
   ```
   
8. Cleanup
   ```
   cd .. & rmdir /s /q build
   ```

### Building Qt with emscripten (multithreaded)
For multithreading to work Qt and emscripten properly, we have to yet again compile a custom version of Qt with an extra flag.

1. Open new CMD Terminal

2. Navigate to Qt Path:
   ```
   cd "C:\Qt\6.9.2"
   ```

3. Generate build and install path (and make sure they are empty)
   ```
   mkdir build & mkdir wasm_multithread_emsdk_4_0_14_custom & cd build
   ```

4. Install and activate specific emsdk version
   ```
   emsdk install 4.0.14 & emsdk activate 4.0.14
   ```

5. Configure Qt with a minimal setup
   ```
   "../Src/configure" -debug-and-release -qt-host-path C:\Qt\6.9.2\msvc2022_64 -make libs -no-widgets -optimize-size -feature-thread -no-warnings-are-errors -platform wasm-emscripten -submodules qtdeclarative -no-dbus -no-sql-sqlite -feature-wasm-simd128 -no-feature-cssparser -no-feature-quick-treeview -no-feature-quick-pathview -no-feature-texthtmlparser -no-feature-textodfwriter -no-feature-quickcontrols2-windows -no-feature-quickcontrols2-macos -no-feature-quickcontrols2-ios -no-feature-quickcontrols2-imagine -no-feature-quickcontrols2-universal -no-feature-quickcontrols2-fusion -no-feature-qtprotobufgen -no-feature-pdf -no-feature-printer -no-feature-sqlmodel -no-feature-quick-pixmap-cache-threaded-download -feature-quick-canvas -no-feature-quick-designer -no-feature-quick-particles -no-feature-quick-sprite -no-feature-raster-64bit -no-feature-raster-fp -prefix ../wasm_multithread_emscripten_4_0_14_custom
   ```

6. Build Qt
   ```
   cmake --build . --parallel
   ```

7. Install Qt
   ```
   cmake --install .
   ```
   
8. Cleanup
   ```
   cd .. & rmdir /s /q build
   ```

### Create Custom Kit for Qt Creator
This step is specifically tailored to the Qt-Creator IDE.

1. `Preferences -> Devices -> WebAssembly`: Set path to the emsdk git repository
2. `Preferences -> Kits -> Qt Versions`: Add the newly built Qt-Version(s).
3. `Preferences -> Kits -> Kits`: Add the new kit(s) and set them to use the custom Qt-Version(s) respectively.

[![Emscripten Path Qt Creator](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/emscripten_path_qt_creator.jpg?raw=true) [![Custom Qt Version for emsdk](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_version.jpg?raw=true) [![Custom Qt Kit for emsdk](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit_thumb.jpg?raw=true)](https://github.com/weBIGeo/ressources/blob/main/for_md/custom_qt_kit.jpg?raw=true)

### Set EMSDK environment variable
[TODO] find better way of handling that, the IDE should set this

Usually, the Qt-IDE (Qt Creator) sets emscripten environment variables right before building after configuring the correct path to emsdk. However, for the current combination of versions, the variable is not set properly by the IDE. Therefore, in order to build the WebAssmebly version, manually set the environment variable EMSDK to emsdk's root directory.

## Building the native version<a name="native"></a>

### Dependencies
* Windows
* Qt 6.10.0 with
  * Qt Shader Tools (otherwise Qt configure fails)
  * MSVC2022 pre-built binaries
* Python
* Microsoft Visual C++ Compiler 17.6 (aka. MSVC2022, comes with Visual Studio 2022)
* To exactly follow along with build instructions you need `ninja` and `cmake` in your PATH

> [!IMPORTANT]
> Dawn does not compile with MinGW! GCC might be possible, but is not tested (hence the Windows Requirement). If compiling on Linux is necessary it is also required to change the way we link to the precompiled Dawn libraries inside of `cmake/alp_target_add_dawn.cmake`.

### Building Dawn
[Dawn](https://dawn.googlesource.com/dawn/) is the webGPU implementation used in Chromium, which is the open-source engine powering Google Chrome. Compiling Dawn ourselves allows us to deploy weBIGeo natively such that we don't have to work in the browser sandbox during development. Building Dawn will take some time and memory as we need Debug and Release builds.

1. We need the compiler env variables, so choose either (or do manually :P)

   1. Start the `x64 Native Tools Command Prompt for VS 2022`.

   2. Start a new CMD and run: (you might have to adjust the link depending on your vs version)
      ```
      "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
      ```

2. Navigate to the parent folder of where the weBIGeo renderer is located. (Not strictly necessary as DAWN location can be set with CMAKE-Variable `ALP_DAWN_DIR`)
   ```bash
   cd "/path/to/the/parent/directory/of/webigeo/renderer"
   ```

3. Clone Dawn and step into directory
   ```bash
   git clone -b chromium/7548 https://dawn.googlesource.com/dawn && cd dawn
   ```

3.  Debug Build (minimal vulkan only build):
    ```bash
    cmake -GNinja -S . -B out/Debug -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF -DDAWN_BUILD_SAMPLES=OFF -DDAWN_ENABLE_D3D11=OFF -DDAWN_ENABLE_D3D12=OFF -DDAWN_ENABLE_METAL=OFF -DDAWN_ENABLE_NULL=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=OFF -DDAWN_ENABLE_VULKAN=ON -DDAWN_USE_WINDOWS_UI=OFF -DDAWN_USE_GLFW=OFF && cmake --build out/Debug && cmake --install out/Debug --prefix install/Debug
    ```

4. Configure, build and install Release build
   ```bash
   cmake -GNinja -S . -B out/Release -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF -DDAWN_BUILD_SAMPLES=OFF -DDAWN_ENABLE_D3D11=OFF -DDAWN_ENABLE_D3D12=OFF -DDAWN_ENABLE_METAL=OFF -DDAWN_ENABLE_NULL=OFF -DDAWN_ENABLE_DESKTOP_GL=OFF -DDAWN_ENABLE_OPENGLES=OFF -DDAWN_ENABLE_VULKAN=ON -DDAWN_USE_WINDOWS_UI=OFF -DDAWN_USE_GLFW=OFF && cmake --build out/Release && cmake --install out/Release --prefix install/Release
   ```

5. Cleanup (Optional):
   Delete everything within the Dawn director except the `install/` directory.

### About DAWN Backends
Per default we opt for an only Vulkan-Backend Build for two reasons:
- Vulkan is probably the most supported Backend running on most devices
- We have more knowledge about Vulkan which comes to play when we use GPU debugers

That being said it is possible to compile DAWN with all Backends by using the following commands *instead of step 3 and 4*:

```bash
cmake -GNinja -S . -B out/Debug -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF -DDAWN_BUILD_SAMPLES=OFF  -DDAWN_USE_WINDOWS_UI=OFF -DDAWN_USE_GLFW=OFF && cmake --build out/Debug && cmake --install out/Debug --prefix install/Debug
```

```bash
cmake -GNinja -S . -B out/Release -DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC -DDAWN_FETCH_DEPENDENCIES=ON -DDAWN_ENABLE_INSTALL=ON -DCMAKE_BUILD_TYPE=Release -DTINT_BUILD_SPV_READER=OFF -DTINT_BUILD_TESTS=OFF -DTINT_BUILD_FUZZERS=OFF -DTINT_BUILD_BENCHMARKS=OFF -DTINT_BUILD_AS_OTHER_OS=OFF -DDAWN_BUILD_SAMPLES=OFF  -DDAWN_USE_WINDOWS_UI=OFF -DDAWN_USE_GLFW=OFF && cmake --build out/Release && cmake --install out/Release --prefix install/Release
```

### Building SDL
SDL is the library in use for window management. Given its big size we also build it seperately from weBIGeo

1. We need the compiler env variables, so choose either (or do manually :P)

   1. Start the `x64 Native Tools Command Prompt for VS 2022`.

   2. Start a new CMD and run: (you might have to adjust the link depending on your vs version)
      ```
      "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
      ```

2.  Set install location. (like DAWN it should be the parent of the weBIGeo renderer location)
    ```
    set CMAKE_INSTALL_PREFIX=C:\tmp\webigeo\SDL
    mkdir %CMAKE_INSTALL_PREFIX% && cd %CMAKE_INSTALL_PREFIX%
    ```

3.  Clone SDL, step into directory and checkout SDL2 branch. (The location doesnt matter, it's only for the temporary build files)
    ```
    git clone https://github.com/libsdl-org/SDL.git && cd SDL && git checkout SDL2
    ```

4.  Create and step into build directory
    ```
    mkdir build & cd build
    ```

5.  Configure and compile Release-Build (Debug not necessary)
    ```
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja
    ```

6.  Install SDL files
    ```
    ninja install
    ```

7.  Delete Build and Source Files
    ```
    cd ../.. && rmdir /S /Q SDL
    ```    