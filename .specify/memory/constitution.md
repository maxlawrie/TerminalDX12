# TerminalDX12 Constitution

## Core Principles

### I. GPU-First Rendering
All text rendering must leverage DirectX 12's instanced drawing capabilities. Glyphs are rasterized into a texture atlas via FreeType, and rendered in a single instanced draw call. CPU-side rendering or GDI fallbacks are not permitted. Performance-critical paths must minimize GPU state changes and maximize batch efficiency.

### II. VT/ANSI Compliance
The terminal must accurately implement VT100/ANSI escape sequences as specified in SPECIFICATION.md. Each sequence implementation must match xterm behavior for compatibility. New escape sequences require documentation of expected behavior before implementation. The VT state machine in `src/terminal/VTStateMachine.cpp` is the authoritative parser.

### III. Test-First Development
All new features require tests before implementation:
- **Unit tests** (GTest) for VT parser, screen buffer, and Unicode handling
- **Integration tests** (Python) for visual validation and user interaction
- Tests must pass before merging; the `run_tests.bat` script is the validation gate
- Target: maintain 100% coverage of implemented VT sequences

### IV. Windows Native Architecture
This is a Windows-only terminal leveraging native APIs:
- **ConPTY** for pseudo-console functionality (Windows 10 1809+)
- **Win32 Input Mode** for proper keyboard handling
- **DirectX 12** for GPU rendering (no cross-platform abstractions)
- No Unix/POSIX compatibility layers; embrace Windows conventions

### V. Unicode & Internationalization
Full Unicode support is non-negotiable:
- UTF-8 input decoding with proper multi-byte sequence handling
- UTF-32 internal representation for accurate glyph indexing
- FreeType glyph rasterization for any Unicode codepoint
- Wide character (CJK) handling with proper cell width calculation

## Architecture Standards

### Rendering Pipeline
1. Glyph atlas: 2048x2048 RGBA texture, dynamically populated
2. Instance buffer: per-glyph position, UV coordinates, foreground/background colors
3. Single draw call per frame for all visible text
4. Selection overlay rendered as separate pass

### Terminal Emulation
1. VTStateMachine: state-based parser for escape sequences
2. ScreenBuffer: 80x24 default grid with 10,000-line scrollback
3. Cell structure: Unicode codepoint + text attributes (color, bold, underline, etc.)
4. ConPTY bridge: bidirectional pipe for shell I/O

### Module Boundaries
- `core/` - Application lifecycle, window management
- `pty/` - ConPTY process spawning and I/O
- `terminal/` - VT parsing, screen buffer, scrollback
- `rendering/` - DirectX 12 renderer, glyph atlas, shaders
- `ui/` - Input handling, selection, clipboard

## Quality Gates

### Before Merge
- All existing tests pass (`run_tests.bat` exits 0)
- New code has corresponding test coverage
- No compiler warnings at `/W4` level
- SPECIFICATION.md updated if adding new VT sequences

### Bug Fixes
- Unit tests must pass before manual testing
- **Manual testing is required** before closing any bug issue
- Reproduce the original bug scenario to confirm the fix
- Document the root cause and fix in the commit message

### Performance Requirements
- 60 FPS rendering for standard terminal output
- Sub-millisecond input latency
- Memory-efficient scrollback (< 100MB for 10K lines)

## Governance

This constitution defines the architectural boundaries and development practices for TerminalDX12. Amendments require:
1. Documentation of the proposed change
2. Impact analysis on existing architecture
3. Update to this constitution before implementation

**Version**: 1.1.0 | **Ratified**: 2025-12-21 | **Last Amended**: 2025-12-28
