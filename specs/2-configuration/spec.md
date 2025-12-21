# Feature Specification: Configuration System

**Feature Branch**: `2-configuration`  
**Created**: 2024-12-21  
**Status**: Not Started  
**Priority**: High  

## Overview

Implement a JSON-based configuration system allowing users to customize fonts, colors, keybindings, and terminal behavior without recompiling.

## User Scenarios & Testing

### User Story 1 - Custom font and size (Priority: P1)

A user wants to use their preferred programming font (e.g., JetBrains Mono, Fira Code) at a comfortable size.

**Why this priority**: Font choice is the most common customization request.

**Independent Test**: Create config with custom font, restart terminal, verify font renders correctly.

**Acceptance Scenarios**:

1. **Given** config specifies "font.family": "JetBrains Mono", **When** terminal starts, **Then** JetBrains Mono is used
2. **Given** config specifies "font.size": 18, **When** terminal starts, **Then** glyphs render at 18pt size
3. **Given** font file doesn't exist, **When** terminal starts, **Then** fallback to default font with warning

---

### User Story 2 - Custom color scheme (Priority: P1)

A user wants to use a dark theme like Dracula, Solarized, or Nord for better visibility.

**Why this priority**: Color schemes affect usability and eye strain.

**Independent Test**: Apply a color scheme, verify all 16 base colors match scheme definition.

**Acceptance Scenarios**:

1. **Given** config includes custom color palette, **When** terminal renders colored text, **Then** colors match config values
2. **Given** config sets "background": "#282A36", **When** terminal renders, **Then** background is Dracula purple
3. **Given** invalid color value in config, **When** parsing, **Then** use default color and log warning

---

### User Story 3 - Custom keybindings (Priority: P2)

A user wants to remap copy/paste to match their muscle memory from other terminals.

**Why this priority**: Keybindings improve productivity for power users.

**Independent Test**: Remap copy to Ctrl+Shift+C, verify it works, verify Ctrl+C sends SIGINT.

**Acceptance Scenarios**:

1. **Given** config maps "copy": "ctrl+shift+c", **When** user presses Ctrl+Shift+C with selection, **Then** text is copied
2. **Given** config maps custom shortcut, **When** shortcut conflicts with application key, **Then** config takes precedence
3. **Given** invalid shortcut syntax in config, **When** parsing, **Then** skip binding and log warning

---

### User Story 4 - Default shell selection (Priority: P2)

A user prefers cmd.exe or WSL over PowerShell and wants it as the default.

**Why this priority**: Shell preference is fundamental to workflow.

**Independent Test**: Set default shell to cmd.exe, restart terminal, verify cmd.exe starts.

**Acceptance Scenarios**:

1. **Given** config sets "shell": "cmd.exe", **When** terminal starts, **Then** cmd.exe is spawned
2. **Given** config sets "shell": "wsl.exe", **When** terminal starts, **Then** WSL shell is spawned
3. **Given** specified shell doesn't exist, **When** terminal starts, **Then** fall back to PowerShell with error message

---

### Edge Cases

- What happens when config file is malformed JSON?
- How does system handle config file permissions errors?
- What happens when config specifies unavailable font?

## Requirements

### Functional Requirements

- **FR-001**: Terminal MUST load config from %APPDATA%\TerminalDX12\config.json
- **FR-002**: Terminal MUST use sensible defaults when config is missing
- **FR-003**: Terminal MUST validate config values and log warnings for invalid entries
- **FR-004**: Terminal MUST support font family, size, and weight configuration
- **FR-005**: Terminal MUST support foreground, background, and 16-color palette configuration
- **FR-006**: Terminal MUST support keybinding customization for copy, paste, scroll operations
- **FR-007**: Terminal MUST support shell path and initial directory configuration
- **FR-008**: Terminal MUST support cursor style (block/underline/bar) and blink configuration
- **FR-009**: Terminal MUST support scrollback line count configuration

### Key Entities

- **Config**: Root configuration object loaded from JSON
- **FontConfig**: Font family, size, weight, ligatures toggle
- **ColorConfig**: Foreground, background, cursor, selection, 16-color palette
- **KeybindingConfig**: Map of action names to key combinations
- **TerminalConfig**: Shell path, working directory, scrollback size, cursor settings

## Success Criteria

### Measurable Outcomes

- **SC-001**: Users can change fonts without recompiling
- **SC-002**: Color scheme changes apply on next terminal launch
- **SC-003**: Invalid config entries produce clear log warnings with line numbers
- **SC-004**: Config parsing adds less than 50ms to startup time
- **SC-005**: Default config file is auto-generated if missing

## Assumptions

- JSON is an acceptable config format for users
- %APPDATA% is a standard location for Windows application config
- Font files are installed system-wide or in standard user font directories

## Dependencies

- nlohmann/json (already in vcpkg.json)
- FreeType (for font loading validation)
- spdlog (for warning/error logging)
