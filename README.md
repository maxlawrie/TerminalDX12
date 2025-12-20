# TerminalDX12

A modern terminal emulator for Windows with GPU-accelerated text rendering using DirectX 12.

## Features

- **GPU-Accelerated Rendering**: DirectX 12 instanced rendering with glyph atlases
- **Modern Text Rendering**: FreeType and HarfBuzz for ligatures and complex scripts
- **ConPTY Integration**: Native Windows pseudo console for shell processes
- **Complete VT Emulation**: Full VT sequence support for modern terminal apps
- **Tabs and Splits**: Tree-based pane management
- **Custom Shaders**: Post-processing effects (CRT, blur, custom effects)
- **Configurable**: JSON-based configuration for profiles, colors, and keybindings

## Requirements

- Windows 10 1809 or later (for ConPTY support)
- DirectX 12 capable GPU
- Visual Studio 2022 or later (MSVC v143)
- CMake 3.21 or later
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
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

### 4. Run

```bash
bin\Release\TerminalDX12.exe
```

## Project Structure

```
TerminalDX12/
├── include/          # Header files
│   ├── core/        # Application, Window
│   ├── pty/         # ConPTY integration
│   ├── terminal/    # VT parser, screen buffer
│   ├── rendering/   # DirectX 12 renderer
│   └── ui/          # Tabs, splits, input
├── src/             # Implementation files
├── shaders/         # HLSL shaders
├── resources/       # Fonts, icons
└── tests/           # Unit and integration tests
```

## Architecture

### Rendering Pipeline

1. **Glyph Atlas**: FreeType rasterizes glyphs into 2048x2048 texture
2. **Text Shaping**: HarfBuzz converts text to glyph indices + positions
3. **Instance Buffer**: Build per-glyph data for visible cells
4. **GPU Rendering**: Single instanced draw call renders all glyphs
5. **Post-Processing**: Optional custom shaders for effects

### Terminal Emulation

1. **ConPTY**: Native Windows pseudo console spawns shell processes
2. **VT Parser**: State machine processes ANSI/VT sequences
3. **Screen Buffer**: Grid of cells with text attributes
4. **Dispatcher**: Translates VT commands to screen buffer operations

## Configuration

Create `config.json` in the application directory:

```json
{
  "profiles": [
    {
      "name": "PowerShell",
      "commandline": "pwsh.exe",
      "font": {
        "face": "Cascadia Code",
        "size": 11
      },
      "colors": {
        "foreground": "#CCCCCC",
        "background": "#0C0C0C"
      }
    }
  ],
  "keybindings": [
    {"command": "newTab", "keys": "ctrl+shift+t"},
    {"command": "splitHorizontal", "keys": "ctrl+shift+d"}
  ]
}
```

## Development Roadmap

### Phase 1: MVP (Weeks 1-4) ✓ In Progress
- [x] Project setup
- [ ] Win32 window
- [ ] DirectX 12 initialization
- [ ] Basic glyph atlas
- [ ] Simple text rendering
- [ ] ConPTY integration
- [ ] Basic VT parser
- [ ] Screen buffer

### Phase 2: Full VT Support (Weeks 5-6)
- [ ] Complete VT state machine
- [ ] Color support
- [ ] Text attributes
- [ ] Scrollback

### Phase 3: Advanced Rendering (Week 7)
- [ ] HarfBuzz integration
- [ ] Ligatures
- [ ] Performance optimization

### Phase 4: UI Layer (Weeks 8-9)
- [ ] Tabs
- [ ] Splits
- [ ] Mouse input

### Phase 5: Configuration (Week 10)
- [ ] JSON config
- [ ] Profiles
- [ ] Keybindings

### Phase 6: Custom Shaders (Week 11)
- [ ] Post-processing pipeline
- [ ] Shader loading
- [ ] Hot reload

### Phase 7: Polish (Week 12)
- [ ] Performance profiling
- [ ] Bug fixes
- [ ] Documentation

## Resources

- [Windows Terminal](https://github.com/microsoft/terminal) - Reference implementation
- [DirectX 12 Programming Guide](https://www.3dgep.com/learning-directx-12-1/)
- [VT100 Parser Spec](https://vt100.net/emu/dec_ansi_parser)
- [ConPTY Documentation](https://learn.microsoft.com/en-us/windows/console/creating-a-pseudoconsole-session)

## License

MIT License (to be added)

## Contributing

This is a learning project. Contributions and suggestions are welcome!
