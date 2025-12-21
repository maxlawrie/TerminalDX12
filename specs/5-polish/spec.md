# Feature Specification: Polish & Refinement

**Feature Branch**: `5-polish`  
**Created**: 2024-12-21  
**Status**: Not Started  
**Priority**: Low  

## Overview

Polish and enhance the terminal with settings UI, font ligatures, shell integration markers, and multi-window support.

## User Scenarios & Testing

### User Story 1 - Settings UI (Priority: P1)

A user opens a settings dialog to customize the terminal visually instead of editing JSON.

**Why this priority**: GUI settings are more accessible for casual users.

**Independent Test**: Open settings, change font size, click Apply, verify change takes effect.

**Acceptance Scenarios**:

1. **Given** terminal running, **When** user presses Ctrl+Comma, **Then** settings dialog opens
2. **Given** settings dialog open, **When** user changes font size, **Then** preview updates live
3. **Given** changes made, **When** user clicks Apply, **Then** changes saved to config.json
4. **Given** settings dialog open, **When** user clicks Cancel, **Then** no changes saved

---

### User Story 2 - Programming ligatures (Priority: P2)

A developer uses a font with ligatures (Fira Code, JetBrains Mono) and sees combined glyphs for common sequences.

**Why this priority**: Ligatures improve code readability for many developers.

**Independent Test**: Type "=>" in editor, verify arrow ligature renders.

**Acceptance Scenarios**:

1. **Given** ligatures enabled and Fira Code font, **When** output contains "=>", **Then** arrow ligature renders
2. **Given** ligatures enabled, **When** output contains "!=", **Then** not-equal ligature renders
3. **Given** ligatures disabled in config, **When** output contains "=>", **Then** separate characters render
4. **Given** font without ligatures, **When** ligatures enabled, **Then** gracefully fall back to separate chars

---

### User Story 3 - Shell integration (Priority: P2)

The terminal tracks command execution using OSC 133 markers, enabling features like command duration and click-to-rerun.

**Why this priority**: Modern shell integration improves workflow efficiency.

**Independent Test**: Run a command, verify prompt is marked, command duration shown.

**Acceptance Scenarios**:

1. **Given** shell sends OSC 133 A (prompt start), **When** rendering, **Then** prompt line is marked
2. **Given** command executes, **When** OSC 133 D received with exit code, **Then** exit code displayed
3. **Given** command completed, **When** user clicks on command, **Then** command copied to current prompt
4. **Given** failed command (non-zero exit), **When** rendering, **Then** error indicator shown

---

### User Story 4 - Multiple windows (Priority: P3)

A user opens multiple terminal windows, each with independent sessions and configurations.

**Why this priority**: Multi-monitor workflows benefit from multiple windows.

**Independent Test**: Open new window, verify independent shell, close window.

**Acceptance Scenarios**:

1. **Given** terminal running, **When** user presses Ctrl+Shift+N, **Then** new window opens
2. **Given** multiple windows, **When** one window closed, **Then** others remain open
3. **Given** all windows closed, **When** last window closes, **Then** application exits
4. **Given** new window opened, **When** checking sessions, **Then** new window has independent shell

---

### User Story 5 - Window transparency (Priority: P3)

A user enables window transparency to see content behind the terminal.

**Why this priority**: Aesthetic preference for some users.

**Independent Test**: Set opacity to 80%, verify window is semi-transparent.

**Acceptance Scenarios**:

1. **Given** config sets "opacity": 0.8, **When** terminal renders, **Then** window is 80% opaque
2. **Given** transparency enabled, **When** typing, **Then** text remains crisp and readable
3. **Given** transparency enabled, **When** scrolling, **Then** no visual artifacts

---

### Edge Cases

- What happens when HarfBuzz is unavailable for ligatures?
- How does shell integration work with non-standard shells?
- What happens when opening window on monitor with different DPI?

## Requirements

### Functional Requirements

#### Settings UI
- **FR-001**: Terminal MUST provide GUI settings dialog
- **FR-002**: Settings dialog MUST show live preview of changes
- **FR-003**: Settings MUST be saved to config.json on Apply
- **FR-004**: Settings UI MUST cover font, colors, keybindings, and terminal options

#### Ligatures
- **FR-005**: Terminal MUST integrate HarfBuzz for text shaping
- **FR-006**: Terminal MUST support enabling/disabling ligatures in config
- **FR-007**: Terminal MUST gracefully handle fonts without ligature support

#### Shell Integration
- **FR-008**: Terminal MUST parse OSC 133 shell integration markers
- **FR-009**: Terminal MUST track command boundaries (start, end, exit code)
- **FR-010**: Terminal MUST display command duration when available
- **FR-011**: Terminal MUST provide visual markers for prompts and command output

#### Multiple Windows
- **FR-012**: Terminal MUST support opening new windows (Ctrl+Shift+N)
- **FR-013**: Each window MUST have independent session state
- **FR-014**: Windows MUST share configuration but not session data
- **FR-015**: Application MUST exit only when all windows closed

#### Window Effects
- **FR-016**: Terminal MUST support configurable window opacity
- **FR-017**: Terminal MUST support always-on-top mode
- **FR-018**: Terminal MUST support borderless/fullscreen modes

### Key Entities

- **SettingsDialog**: Win32 dialog with tabbed interface for settings categories
- **HarfBuzzShaper**: Text shaping engine for ligature support
- **ShellIntegration**: OSC 133 parser, command tracking, prompt detection
- **WindowManager**: Multiple window coordination, shared config

## Success Criteria

### Measurable Outcomes

- **SC-001**: Settings changes apply without terminal restart
- **SC-002**: Ligatures render correctly for top 20 programming ligature sequences
- **SC-003**: Command duration accuracy within 10ms
- **SC-004**: Window transparency has no measurable performance impact
- **SC-005**: Multi-window operation stable for 8-hour sessions

## Assumptions

- HarfBuzz can be added as vcpkg dependency
- OSC 133 format follows VS Code/iTerm2 conventions
- Win32 layered windows provide adequate transparency

## Dependencies

- HarfBuzz (new vcpkg dependency for ligatures)
- Win32 dialog/window APIs
- Existing configuration system from Phase 2
