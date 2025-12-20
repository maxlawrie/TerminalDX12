cmake_minimum_required(VERSION 3.21)
project(TerminalDX12 VERSION 0.1.0 LANGUAGES CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable vcpkg integration
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# Windows-specific settings
if(WIN32)
    add_compile_definitions(
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
    )
endif()

# Find required packages
find_package(directx-headers CONFIG REQUIRED)
find_package(freetype CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Source files
set(CORE_SOURCES
    src/core/Application.cpp
    src/core/Window.cpp
)

set(RENDERING_SOURCES
    src/rendering/DX12Renderer.cpp
    src/rendering/GlyphAtlas.cpp
    src/rendering/TextRenderer.cpp
)

set(PTY_SOURCES
    src/pty/ConPtySession.cpp
)

set(TERMINAL_SOURCES
    src/terminal/ScreenBuffer.cpp
    src/terminal/VTStateMachine.cpp
    src/terminal/TerminalDispatcher.cpp
)

set(UI_SOURCES
    src/ui/InputHandler.cpp
    src/ui/TabManager.cpp
)

set(ALL_SOURCES
    ${CORE_SOURCES}
    ${RENDERING_SOURCES}
    ${PTY_SOURCES}
    ${TERMINAL_SOURCES}
    ${UI_SOURCES}
    src/main.cpp
)

# Main executable
add_executable(TerminalDX12 WIN32 ${ALL_SOURCES})

# Include directories
target_include_directories(TerminalDX12 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(TerminalDX12 PRIVATE
    Microsoft::DirectX-Headers
    Microsoft::DirectX-Guids
    freetype
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    d3d12.lib
    dxgi.lib
    dxguid.lib
    d3dcompiler.lib
)

# Compiler warnings
if(MSVC)
    target_compile_options(TerminalDX12 PRIVATE /W4 /WX-)
else()
    target_compile_options(TerminalDX12 PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Shader compilation
add_subdirectory(shaders)

# Tests (optional)
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Set output directories
set_target_properties(TerminalDX12 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
