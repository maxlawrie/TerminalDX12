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

### VI. Security
Security-sensitive operations require explicit safeguards:
- **OSC 52 clipboard access** controlled by policy (default: WriteOnly to prevent exfiltration)
- **Shell execution** limited to user-configured shells only
- **File paths** validated before use; no arbitrary file access from escape sequences
- **Input sanitization** for all external data (clipboard paste, drag-drop, config files)
- Security issues MUST be reported via SECURITY.md process

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

### Thread Safety
The terminal operates with multiple threads (main/UI, render, VT parser). Thread safety rules:
- **Buffer access**: Always acquire mutex before reading/writing ScreenBuffer cells
- **Dimension changes**: Swap buffers BEFORE updating dimensions to prevent race conditions
- **Cell access**: Return cells by value (not reference) to prevent dangling references
- **Resize operations**: Use `std::lock_guard<std::recursive_mutex>` for all resize paths
- Document thread ownership in comments for shared resources

### Error Handling
Consistent error handling across the codebase:
- **Logging**: Use spdlog for all error reporting (never silent failures)
- **Return values**: Prefer `std::optional` or `bool` returns over exceptions for expected failures
- **Exceptions**: Reserve for truly exceptional conditions (out of memory, system failures)
- **User feedback**: Display user-friendly messages for recoverable errors
- **Crash recovery**: Save terminal state before known risky operations

### Logging Standards
Use spdlog with consistent log levels:
- **error**: Failures that affect functionality (file not found, API failures)
- **warn**: Recoverable issues or deprecated usage (fallback font, unknown escape sequence)
- **info**: Significant state changes (config loaded, tab created, resize)
- **debug**: Detailed flow for troubleshooting (escape sequence parsed, glyph cached)
- **trace**: High-volume data (every character processed, every frame rendered)
- Default level: `info` for Release, `debug` for Debug builds

### Configuration Compatibility
Protect users from breaking config changes:
- **Backward compatible**: New config versions MUST load old config files
- **Unknown fields**: Ignore unknown JSON fields (don't fail on future additions)
- **Missing fields**: Use sensible defaults for missing fields
- **Migration**: If format changes, auto-migrate on first load and backup original
- **Validation**: Warn on invalid values, use defaults rather than failing

## Quality Gates

### Before Merge
- All existing tests pass (`run_tests.bat` exits 0)
- New code has corresponding test coverage
- No compiler warnings at `/W4` level
- SPECIFICATION.md updated if adding new VT sequences

### Bug Fixes
- **Create a GitHub issue** for every new bug discovered before starting work
- Before investigating, check if a similar bug or issue was already identified (even if closed)
- Unit tests must pass before manual testing
- **Manual testing is required** before closing any bug issue
- Reproduce the original bug scenario to confirm the fix
- Document the root cause and fix in the commit message

### Issue Completion
Before closing any issue (feature or bug):
- **Automated tests** MUST cover all acceptance scenarios from the spec
- **Manual test suite** (`tests/manual_tests.md`) MUST be updated with new test cases
- All spec requirements MUST have corresponding test coverage
- Run both automated (`run_tests.bat`) and manual test suites to verify
- Update `tests/README.md` with new test counts if tests were added

### Documentation Updates
Keep documentation in sync with code changes:
- **README.md**: Update for new features, changed requirements, or new dependencies
- **CHANGELOG.md**: Add entry for every user-visible change
- **SPECIFICATION.md**: Update status when implementing or modifying VT sequences
- **ARCHITECTURE.md**: Update for structural changes or new components
- **tests/README.md**: Update test counts when adding tests
- **docs/CONFIGURATION.md**: Update when adding new config options
- **Doxygen API docs**: Regenerate when public APIs change (`doxygen Doxyfile`)

### Code Documentation
Public APIs MUST have Doxygen comments:
- **Classes**: `@brief` description, `@note` for usage guidance
- **Public methods**: `@brief`, `@param`, `@return`, `@throws` (if applicable)
- **Complex algorithms**: `@details` explaining the approach
- Header files in `include/` are the primary documentation surface

### Commit Message Format
Use consistent commit message format:
```
<type>: <short description>

<body - explain what and why>

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude Opus 4.5 <noreply@anthropic.com>
```
Types: `feat`, `fix`, `docs`, `refactor`, `test`, `perf`, `chore`

### Branch & Issue Linking
- **Feature branches**: Use speckit numbering format (e.g., `006-settings-dialog`)
- **Commits**: Reference GitHub issues in commit messages (e.g., `fixes #12`, `closes #3`)
- **PRs**: Link to related issues in PR description
- **Issue references**: Use `#N` syntax for automatic GitHub linking

### Speckit Workflow
For new features, follow the speckit workflow:
1. `/speckit.specify` - Create feature specification
2. `/speckit.plan` - Generate implementation plan and research
3. `/speckit.tasks` - Generate task breakdown
4. `/speckit.implement` - Execute implementation
5. `/speckit.analyze` - Validate consistency (optional)

Artifacts are stored in `specs/<number>-<feature-name>/`.

### Performance Requirements
- 60 FPS rendering for standard terminal output
- Sub-millisecond input latency
- Memory-efficient scrollback (< 100MB for 10K lines)

## Governance

This constitution defines the architectural boundaries and development practices for TerminalDX12. Amendments require:
1. Documentation of the proposed change
2. Impact analysis on existing architecture
3. Update to this constitution before implementation

**Version**: 1.3.1 | **Ratified**: 2025-12-21 | **Last Amended**: 2025-12-28
