# Implementation Plan: Polish & Refinement

**Branch**: `5-polish` | **Date**: 2025-12-26 | **Spec**: [spec.md](spec.md)

## Summary

Add polish features to TerminalDX12: a graphical settings dialog, font ligature support via HarfBuzz, multi-window capability, and window transparency effects. These features enhance the user experience without changing core terminal functionality.

## Already Implemented

| Feature | Status | Implementation |
|---------|--------|----------------|
| OSC 133 Shell Integration | Complete | `src/terminal/VTStateMachine.cpp` |
| Configuration system (JSON) | Complete | `src/core/Config.cpp` |
| Opacity config field | Complete | `include/core/Config.h` (TerminalConfig::opacity) |
| Cursor style/blink config | Complete | `include/core/Config.h` |

## Remaining Work

| Feature | Priority | Complexity | User Story |
|---------|----------|------------|------------|
| Settings Dialog (GUI) | P1 | High | US1 |
| Font Ligatures (HarfBuzz) | P2 | High | US2 |
| Multi-window support | P3 | Medium | US4 |
| Window transparency | P3 | Low | US5 |

## Technical Context

**Language/Version**: C++20 (Visual Studio 2026)
**Primary Dependencies**: FreeType, spdlog, DirectX 12, nlohmann/json
**New Dependencies**:
- HarfBuzz (for ligatures) - available via vcpkg
- Win32 Dialog APIs (for settings UI)
**Testing**: Google Test (gtest), manual UI testing
**Target Platform**: Windows 10 1809+ (ConPTY requirement)
**Performance Goals**: 60 FPS rendering, ligatures must not degrade performance
**Constraints**: Settings changes should apply without restart where possible

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| GPU-First Rendering | REQUIRES REVIEW | HarfBuzz integration must maintain GPU-based glyph rendering |
| VT/ANSI Compliance | N/A | No VT sequence changes |
| Test-First Development | PARTIAL | UI testing is manual; unit tests for config changes |
| Windows Native | PASS | Win32 dialogs, DWM transparency |
| Unicode Support | PASS | HarfBuzz improves Unicode text shaping |

## Project Structure

### Documentation (this feature)

```text
specs/5-polish/
├── spec.md              # Feature specification
├── plan.md              # This file
└── tasks.md             # Task breakdown
```

### Source Code (new files)

```text
include/ui/
├── SettingsDialog.h     # NEW: Settings dialog class

src/ui/
├── SettingsDialog.cpp   # NEW: Win32 dialog implementation

include/rendering/
├── HarfBuzzShaper.h     # NEW: Text shaping wrapper (optional)

src/rendering/
├── HarfBuzzShaper.cpp   # NEW: HarfBuzz integration (optional)
├── GlyphAtlas.cpp       # MODIFY: Integrate shaped text
├── TextRenderer.cpp     # MODIFY: Use shaped glyph runs

src/core/
├── Application.cpp      # MODIFY: Multi-window, transparency
├── Window.cpp           # MODIFY: Transparency support

resources/
├── settings.rc          # NEW: Dialog resource file (optional)
```

### vcpkg.json Updates

```json
{
  "dependencies": [
    "harfbuzz"  // NEW: for ligature support
  ]
}
```

## Implementation Phases

### Phase A: Settings Dialog (US1) - Priority P1

**Goal**: GUI for editing terminal settings without manual JSON editing

1. **Design settings dialog layout**:
   - Tab-based UI: General, Appearance, Terminal, Keybindings
   - General: Shell path, working directory, scrollback lines
   - Appearance: Font family, size, colors (with color picker)
   - Terminal: Cursor style, blink, opacity
   - Keybindings: Table of action → key mappings

2. **Implementation approach options**:
   - **Option A**: Win32 Dialog (native, no dependencies)
   - **Option B**: ImGui overlay (modern, requires integration)
   - **Recommended**: Win32 Dialog for consistency with Windows UI

3. **Dialog features**:
   - Load current settings on open
   - Live preview for font/color changes
   - Apply button saves to config.json
   - Cancel discards changes
   - Reset to Defaults button

4. **Integration**:
   - Ctrl+Comma opens settings dialog
   - Modal dialog blocks terminal input while open
   - Apply triggers config reload without restart

### Phase B: Font Ligatures (US2) - Priority P2

**Goal**: Render programming ligatures (=>, ->, !=, etc.) using HarfBuzz

1. **Add HarfBuzz dependency**:
   - Add to vcpkg.json
   - Update CMakeLists.txt to link harfbuzz

2. **Create HarfBuzzShaper class**:
   - Initialize with font file path
   - `Shape(const std::string& text)` returns glyph IDs + positions
   - Handle font features (calt, liga, clig for ligatures)

3. **Integrate with rendering pipeline**:
   - Before rendering line, pass through HarfBuzz shaper
   - Convert shaped glyphs to atlas coordinates
   - Render shaped glyph run instead of individual characters

4. **Configuration**:
   - `font.ligatures: true/false` toggle in config
   - When disabled, bypass HarfBuzz (direct FreeType rendering)

5. **Performance considerations**:
   - Cache shaped runs for static content
   - Only reshape on content change
   - Batch shaping for visible lines

### Phase C: Window Transparency (US5) - Priority P3

**Goal**: Semi-transparent terminal window background

1. **Enable DWM composition**:
   - Use `DwmExtendFrameIntoClientArea` for glass effect
   - Or use `SetLayeredWindowAttributes` for simple opacity

2. **Modify swap chain**:
   - Enable alpha channel in swap chain format
   - Clear with transparent background color

3. **Shader updates**:
   - Background color alpha from config
   - Text remains fully opaque for readability

4. **Configuration**:
   - `terminal.opacity: 0.0-1.0` already in config
   - Apply on startup and settings change

### Phase D: Multi-Window Support (US4) - Priority P3

**Goal**: Open multiple independent terminal windows

1. **Refactor Application class**:
   - Extract window-specific state to `TerminalWindow` class
   - `Application` becomes window manager
   - Each window has own renderer, tabs, shell sessions

2. **Window management**:
   - Ctrl+Shift+N opens new window
   - Windows share config but not session state
   - Track open windows in Application

3. **Lifecycle**:
   - Application exits when last window closes
   - Each window can be closed independently

4. **Considerations**:
   - Shared glyph atlas across windows (performance)
   - Independent ConPTY sessions per window
   - Config changes apply to all windows

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| HarfBuzz performance overhead | Medium | Medium | Cache shaped runs, profile before/after |
| Win32 dialog complexity | Low | Low | Start with minimal viable dialog |
| Multi-window DX12 sharing | Medium | High | Research shared device patterns |
| Transparency artifacts | Low | Low | Test with various GPU vendors |

## Dependencies

| Dependency | Version | Purpose | vcpkg |
|------------|---------|---------|-------|
| HarfBuzz | 8.0+ | Text shaping/ligatures | harfbuzz |
| FreeType | existing | Font rasterization | freetype |
| DWM APIs | Windows | Transparency effects | (system) |

## Complexity Justification

| Feature | Complexity | Justification |
|---------|------------|---------------|
| HarfBuzz integration | High | Significant rendering pipeline changes, but provides standard solution for ligatures |
| Multi-window | Medium | Requires architecture refactor but follows established patterns |
| Settings dialog | Medium | Standard Win32 development, no architectural changes |
| Transparency | Low | Simple API calls, minimal code changes |

## Recommended Implementation Order

1. **Window Transparency** (quick win, low risk)
2. **Settings Dialog** (high value for users)
3. **Font Ligatures** (complex, can be deferred)
4. **Multi-Window** (architecture change, lowest priority)

