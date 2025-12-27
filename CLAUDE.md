# TerminalDX12 Development Guidelines

## Quick Reference

| Item | Value |
|------|-------|
| **Language** | C++20 |
| **IDE** | Visual Studio 2026 |
| **Graphics** | DirectX 12 |
| **Platform** | Windows 10 1809+ (ConPTY requirement) |
| **Tests** | 333 unit tests must pass before commits |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Application                             │
│                   (src/core/Application.cpp)                 │
└─────────────────────────────────────────────────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│   TabManager    │  │  InputHandler   │  │  DX12Renderer   │
│  (src/ui/)      │  │  (src/ui/)      │  │ (src/rendering/)│
└─────────────────┘  └─────────────────┘  └─────────────────┘
         │                    │                    │
         ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│      Tab        │  │  ScreenBuffer   │  │   GlyphAtlas    │
│  (src/ui/)      │  │ (src/terminal/) │  │ (src/rendering/)│
└─────────────────┘  └─────────────────┘  └─────────────────┘
         │                    │
         ▼                    ▼
┌─────────────────┐  ┌─────────────────┐
│ ConPtySession   │  │ VTStateMachine  │
│  (src/pty/)     │  │ (src/terminal/) │
└─────────────────┘  └─────────────────┘
```

**Data Flow:** ConPTY → VTStateMachine → ScreenBuffer → DX12Renderer → Display

## Thread Safety (Critical)

These invariants **must** be maintained:

1. **Buffer swap BEFORE dimension update** - During resize, swap the buffer pointer before updating dimensions to prevent other threads from accessing invalid memory
2. **Mutex-protected scroll region** - `SetScrollRegion`, `ResetScrollRegion`, `ScrollRegionUp`, `ScrollRegionDown` all require mutex locks
3. **Return cells by value** - `GetCellWithScrollback()` returns by value to prevent dangling references

## Key Abstractions

| Type | Location | Purpose |
|------|----------|---------|
| `Cell` | ScreenBuffer.h | Single character + attributes |
| `CellAttributes` | ScreenBuffer.h | Color, bold, underline, etc. |
| `ScreenBuffer` | terminal/ | 2D grid of cells + scrollback |
| `VTStateMachine` | terminal/ | Parses VT/ANSI escape sequences |
| `Tab` | ui/ | Single terminal session |
| `TabManager` | ui/ | Multiple tabs in one window |
| `ConPtySession` | pty/ | Windows pseudo-console interface |

## Common Tasks

### Adding a VT Escape Sequence
See [docs/ADDING_VT_SEQUENCES.md](docs/ADDING_VT_SEQUENCES.md) for step-by-step guide.

### Adding Unit Tests
- C++ tests: Add to `tests/unit/` using GoogleTest
- Python visual tests: Add to `tests/*.py`
- Run tests: `./build/tests/Release/TerminalDX12Tests.exe`

### Fixing Bugs
1. Check if issue is in VT parsing (`VTStateMachine.cpp`) or rendering (`DX12Renderer.cpp`)
2. Add a failing test first
3. Fix the bug
4. Verify all 333 tests pass

## Project Structure

```text
src/
  core/       - Application lifecycle, window management
  pty/        - ConPTY process spawning and I/O
  terminal/   - VT parsing, screen buffer, scrollback
  rendering/  - DirectX 12 renderer, glyph atlas, shaders
  ui/         - Input handling, selection, clipboard, tabs
include/      - Header files (mirrors src/ structure)
tests/
  unit/       - Google Test unit tests (333 tests)
  *.py        - Python integration tests (72 tests)
docs/         - Architecture, configuration, guides
specs/        - Feature specifications (speckit)
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

# Run contract tests
./build/tests/Release/TerminalDX12ContractTests.exe

# Run Python integration tests
python -m pytest tests/ -v

# Launch application
./build/bin/Release/TerminalDX12.exe
```

## Code Style

- C++20 standard with `/W4` warning level
- Follow existing patterns in codebase
- Doxygen comments for public APIs (`@brief`, `@param`, `@return`)
- Test-first development per constitution
- No raw `new`/`delete` - use smart pointers

## Key Files

| File | Purpose |
|------|---------|
| `src/terminal/VTStateMachine.cpp` | VT escape sequence parser |
| `src/terminal/ScreenBuffer.cpp` | Cell grid and scrollback |
| `src/rendering/DX12Renderer.cpp` | GPU rendering pipeline |
| `src/core/Application.cpp` | Main loop, window, input |
| `src/pty/ConPtySession.cpp` | Windows ConPTY interface |
| `include/core/Config.h` | Configuration structure |

## Debugging

### Logging
```cpp
#include <spdlog/spdlog.h>
spdlog::debug("Message: {}", value);
spdlog::info("Info message");
spdlog::warn("Warning");
spdlog::error("Error");
```

### Common Issues
| Symptom | Likely Cause | Check |
|---------|--------------|-------|
| Crash on resize | Thread safety | Buffer swap ordering |
| Garbled output | VT parsing | `VTStateMachine.cpp` state |
| Missing characters | Glyph atlas | `GlyphAtlas.cpp` cache |
| Wrong colors | SGR handling | `HandleSGR()` in VTStateMachine |

## Performance Constraints

- **60 FPS** rendering target
- **<100MB** memory for 10K line scrollback
- **Sub-ms** input latency

## Documentation Links

- [ARCHITECTURE.md](docs/ARCHITECTURE.md) - Detailed system design
- [SPECIFICATION.md](SPECIFICATION.md) - VT sequence support matrix
- [CONFIGURATION.md](docs/CONFIGURATION.md) - Config file reference
- [CONTRIBUTING.md](CONTRIBUTING.md) - Contribution guidelines

<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
