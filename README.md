# TerminalDX12

[![Tests](https://github.com/maxlawrie/TerminalDX12/actions/workflows/tests.yml/badge.svg)](https://github.com/maxlawrie/TerminalDX12/actions/workflows/tests.yml)
[![codecov](https://codecov.io/gh/maxlawrie/TerminalDX12/graph/badge.svg)](https://codecov.io/gh/maxlawrie/TerminalDX12)

A GPU-accelerated terminal emulator for Windows using DirectX 12.

## Features

- **GPU-Accelerated Rendering** - DirectX 12 instanced rendering with glyph atlas
- **VT100/ANSI Support** - Colors (16, 256, true color), bold, underline, inverse
- **ConPTY Integration** - Native Windows pseudo console for shell processes
- **Win32 Input Mode** - Full support for ConPTY's Win32 input mode for proper key handling
- **Copy/Paste** - Mouse text selection with Ctrl+C/V clipboard support
- **Scrollback Buffer** - 10,000 lines of history with mouse wheel scrolling
- **Unicode Support** - Full UTF-8/UTF-32 character handling via FreeType
- **Multi-Shell Support** - Works with PowerShell, cmd.exe, and WSL/Ubuntu

## Screenshots

The terminal features GPU-accelerated rendering with full color and attribute support:

| Feature | Description |
|---------|-------------|
| **True Color** | Full 24-bit RGB color support (16.7M colors) |
| **Text Styles** | Bold, italic, underline, strikethrough, dim |
| **Selection** | Mouse text selection with blue highlight |
| **Tabs** | Multiple terminal sessions in a single window |
| **Split Panes** | Horizontal and vertical pane splitting |
| **Scrollback** | 10,000 lines of history with smooth scrolling |

## Requirements

- Windows 10 1809 or later (for ConPTY support)
- DirectX 12 capable GPU
- Visual Studio 2026 or later with C++20 support
- CMake 3.20+
- vcpkg package manager

## Building

### 1. Install vcpkg

```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.bat
```

### 2. Set environment variable

```bash
set VCPKG_ROOT=C:\path\to\vcpkg
```

### 3. Build the project

```bash
cd TerminalDX12
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### 4. Run

```bash
bin\Release\TerminalDX12.exe
```

By default, PowerShell is used. To run a different shell:

```bash
bin\Release\TerminalDX12.exe cmd.exe
bin\Release\TerminalDX12.exe wsl.exe
```

## Usage

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+C | Copy selected text (or send SIGINT if nothing selected) |
| Ctrl+V | Paste from clipboard |
| Shift+Page Up | Scroll up in history |
| Shift+Page Down | Scroll down in history |

### Mouse

| Action | Result |
|--------|--------|
| Click and drag | Select text |
| Single click | Clear selection |
| Mouse wheel | Scroll through buffer |

## Project Structure

```
TerminalDX12/
├── include/          # Header files
│   ├── core/         # Application, Window
│   ├── pty/          # ConPTY integration
│   ├── terminal/     # VT parser, screen buffer
│   ├── rendering/    # DirectX 12 renderer
│   └── ui/           # Input handling
├── src/              # Implementation files
├── shaders/          # HLSL shaders for text rendering
├── tests/            # Test suites
│   ├── unit/         # C++ unit tests (GTest)
│   ├── *.py          # Python integration tests
│   └── README.md     # Test documentation
└── run_tests.bat     # Test runner script
```

## Architecture

### Rendering Pipeline

1. **Glyph Atlas** - FreeType rasterizes glyphs into 2048x2048 RGBA texture
2. **Instance Buffer** - Per-glyph position, UV, and color data
3. **GPU Rendering** - Single instanced draw call renders all visible glyphs
4. **Selection Highlight** - Blue overlay for selected text regions

### Terminal Emulation

1. **ConPTY** - Native Windows pseudo console spawns cmd.exe/powershell
2. **VT Parser** - State machine processes ANSI/VT escape sequences
3. **Screen Buffer** - 80x24 grid of cells with Unicode + attributes
4. **Scrollback** - Circular buffer storing 10,000 lines of history

## Testing

### C++ Unit Tests

Build with tests enabled:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake -DBUILD_TESTS=ON
cmake --build . --config Release
```

Run tests using the helper script:

```bash
run_tests.bat
```

Or run the test executable directly:

```bash
build\tests\Release\TerminalDX12Tests.exe
```

**Test Coverage:** 333 unit tests covering VT parser, screen buffer, tab management, ConPTY contracts, configuration, Unicode handling, and performance benchmarks.

### Python Integration Tests

Visual validation tests using screenshots and OCR:

```bash
python -m pytest tests/ -v
```

**Requirements:** Python 3.x, Pillow, pywin32, numpy, winocr

**Test Coverage:** 72 integration tests covering clipboard, keyboard shortcuts, ANSI colors, text attributes, Unicode rendering, mouse input, scrolling, window resize, E2E workflows, and stress testing.

### PowerShell Smoke Tests

Quick validation of build artifacts and stability:

```powershell
powershell -ExecutionPolicy Bypass -File tests/test_basic.ps1
```

**Test Coverage:** 4 smoke tests covering executable existence, dependencies, multiple instances, and rapid start/stop.

For detailed test documentation, see [tests/README.md](tests/README.md).


## Feature Specification

See [SPECIFICATION.md](SPECIFICATION.md) for a complete list of supported VT escape sequences, text attributes, and planned features with implementation status.

## Dependencies (via vcpkg)

**Runtime:**
- directx-headers
- freetype
- spdlog
- nlohmann-json

**Testing (optional):**
- gtest

## Known Issues

- **Intermittent red ">" character** - A red ">" character occasionally appears unexpectedly in the terminal output. This appears to be related to escape sequence parsing.

## License

GPL v2 License - See [LICENSE.md](LICENSE.md)

## Contributing

Contributions and suggestions are welcome\! See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.



