# TerminalDX12

A GPU-accelerated terminal emulator for Windows using DirectX 12.

## Features

- **GPU-Accelerated Rendering** - DirectX 12 instanced rendering with glyph atlas
- **VT100/ANSI Support** - Colors (16, 256, true color), bold, underline, inverse
- **ConPTY Integration** - Native Windows pseudo console for shell processes
- **Copy/Paste** - Mouse text selection with Ctrl+C/V clipboard support
- **Scrollback Buffer** - 10,000 lines of history with mouse wheel scrolling
- **Unicode Support** - Full UTF-8/UTF-32 character handling via FreeType

## Screenshots

The terminal renders text using a GPU-accelerated glyph atlas with proper color and attribute support.

## Requirements

- Windows 10 1809 or later (for ConPTY support)
- DirectX 12 capable GPU
- Visual Studio 2022 or later
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
└── tests/            # Python test suite with screenshots
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

The test suite uses Python with screenshot-based visual validation:

```bash
cd C:\Temp\TerminalDX12Test
python test_terminal.py
```

**Requirements:** Python 3.x, Pillow, pywin32, numpy

**Test Coverage:** 20 tests covering startup, keyboard input, colors, text attributes, scrollback, and more.

## Dependencies (via vcpkg)

- directx-headers
- freetype
- spdlog
- nlohmann-json

## License

MIT License

## Contributing

Contributions and suggestions are welcome!
