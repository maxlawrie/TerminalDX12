# TerminalDX12 Development Guidelines

Auto-generated from all feature plans. Last updated: 2025-12-22

## Active Technologies

- C++20, DirectX 12, FreeType, ConPTY, spdlog (master)

## Project Structure

```text
src/
  core/       - Application lifecycle, window management
  pty/        - ConPTY process spawning and I/O
  terminal/   - VT parsing, screen buffer, scrollback
  rendering/  - DirectX 12 renderer, glyph atlas, shaders
  ui/         - Input handling, selection, clipboard, tabs
tests/
  unit/       - Google Test unit tests
  integration/- Python integration tests
```

## Commands

```bash
# Configure (first time or after CMakeLists.txt changes)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# Build Release
cmake --build build --config Release

# Build Debug
cmake --build build --config Debug

# Run C++ unit tests
./build/tests/Release/TerminalDX12Tests.exe

# Run Python integration tests
python -m pytest tests/ -v

# Launch application
./build/bin/Release/TerminalDX12.exe
```

## Code Style

- C++20 standard with `/W4` warning level
- Follow existing patterns in codebase
- Doxygen comments for public APIs (@brief, @param, @return)
- Test-first development per constitution

## Key Files

- `src/terminal/VTStateMachine.cpp` - VT escape sequence parser
- `src/rendering/DX12Renderer.cpp` - GPU rendering pipeline
- `src/core/Application.cpp` - Main application loop
- `include/core/Config.h` - Configuration structure

## Recent Changes

- master: Added C++20, DirectX 12, FreeType, ConPTY, spdlog

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
