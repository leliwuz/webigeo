#############################################################################
# weBIGeo
# Copyright (C) 2024 Gerald Kimmersdorfer
# Copyright (C) 2025 Patrick Komon
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#############################################################################

project(alpine-renderer-webgpu LANGUAGES C CXX)

# NOTE: The idea behind this target is to accumulate all the webgpu related things that are platform
# dependent such that we only have the webgpu cmake target for the other projects to link against.

set(ALP_DAWN_DIR "${CMAKE_SOURCE_DIR}/../dawn" CACHE PATH "path to the dawn repo with debug and release builds. (only necessary for native build)")

set(ALP_SDL_INSTALL_DIR "${CMAKE_SOURCE_DIR}/../SDL" CACHE PATH "Path to the SDL installation directory")

set(ALP_DAWN_PORT_DIR "${CMAKE_SOURCE_DIR}/../emdawnwebgpu_pkg/emdawnwebgpu.port.py" CACHE FILEPATH "Path to the emdawnwebgpu Emscripten port file")

set(SOURCES

    raii/base_types.h
    raii/Texture.h raii/Texture.cpp
    raii/TextureWithSampler.h raii/TextureWithSampler.cpp
    raii/RawBuffer.h
    raii/TextureView.h raii/TextureView.cpp
    raii/Sampler.h raii/Sampler.cpp
    raii/BindGroup.h raii/BindGroup.cpp
    raii/BindGroupLayout.h raii/BindGroupLayout.cpp
    raii/Pipeline.h raii/Pipeline.cpp
    raii/PipelineLayout.h raii/PipelineLayout.cpp
    raii/CombinedComputePipeline.h raii/CombinedComputePipeline.cpp
    raii/RenderPassEncoder.h raii/RenderPassEncoder.cpp

    util/VertexBufferInfo.h util/VertexBufferInfo.cpp
    util/VertexFormat.h
    util/string_cast.h util/string_cast.cpp

    timing/GuiTimerManager.h timing/GuiTimerManager.cpp
    timing/TimerInterface.h timing/TimerInterface.cpp
    timing/CpuTimer.h timing/CpuTimer.cpp
    timing/WebGpuTimer.h timing/WebGpuTimer.cpp

    Framebuffer.h Framebuffer.cpp

    webgpu_interface.hpp webgpu_interface.cpp)

add_library(webgpu STATIC ${SOURCES})

if (EMSCRIPTEN)
    if (EXISTS "${ALP_DAWN_PORT_DIR}")
        message(STATUS "Using custom Dawn port: ${ALP_DAWN_PORT_DIR}")
        set(ALP_DAWN_PORT_ARG "--use-port=${ALP_DAWN_PORT_DIR}")
    else()
        message(WARNING
            "ALP_DAWN_PORT_DIR not found at '${ALP_DAWN_PORT_DIR}'. "
            "Falling back to default Emscripten port emdawnwebgpu"
        )
        set(ALP_DAWN_PORT_ARG "--use-port=emdawnwebgpu")
    endif()

    target_compile_options(webgpu PUBLIC
        "--use-port=sdl2"
        "${ALP_DAWN_PORT_ARG}"
    )

    target_link_options(webgpu PUBLIC
        "-sASYNCIFY=1"
        "--use-port=sdl2"
        "${ALP_DAWN_PORT_ARG}"
    )

else()

    # add dawn libraries to webgpu library
    set(Dawn_DIR "${ALP_DAWN_DIR}/install/${CMAKE_BUILD_TYPE}/lib/cmake/Dawn")
    find_package(Dawn REQUIRED)
    target_link_libraries(webgpu PUBLIC dawn::webgpu_dawn)

    # find and add SDL2 (you may install it to the default install folder or ALP_SDL_INSTALL_DIR)
    # Add ALP_SDL_INSTALL_DIR to CMake's search path for find_package
    list(APPEND CMAKE_PREFIX_PATH "${ALP_SDL_INSTALL_DIR}")
    find_package(SDL2 REQUIRED)
    target_link_libraries(webgpu PUBLIC SDL2::SDL2 SDL2::SDL2main)

    # TODO verify whether this is still needed and adapt accordingly
    # If tint_lang_hlsl_writer.lib within ${ALP_DAWN_DIR}/build/debug/src exists DAWN was compiled with DX-Backends.
    file(GLOB_RECURSE TINT_LIB_PATH "${ALP_DAWN_DIR}/out/Debug/src/tint/tint_lang_hlsl_writer.lib")
    if (TINT_LIB_PATH)
        # NOTE: With Direct3D Backends we additionally need to link against dx.
        message(STATUS "Found tint_lang_hlsl_writer.lib, DAWN seems to be compiled with DX-Backends. Will link against dxguid.lib.")
        find_library(DXGUID_LIB dxguid.lib)
        if (DXGUID_LIB)
            target_link_libraries(webgpu PUBLIC ${DXGUID_LIB})
        else()
            message(FATAL_ERROR "dxguid.lib not found.")
        endif()
    endif()
endif()

# set public IMGUI_IMPL_WEBGPU_BACKEND_DAWN flag
target_compile_definitions(webgpu PUBLIC IMGUI_IMPL_WEBGPU_BACKEND_DAWN)

target_link_libraries(webgpu PUBLIC Qt::Core nucleus)
target_include_directories(webgpu PRIVATE .)
