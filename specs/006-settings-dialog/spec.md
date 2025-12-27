# Feature Specification: Settings Dialog

**Feature Branch**: `006-settings-dialog`
**Created**: 2025-12-28
**Status**: Draft
**Input**: User description: "Settings Dialog - GUI configuration for terminal settings. Ctrl+Comma opens dialog with tabs for General (shell path, working dir, scrollback), Appearance (font family/size, colors, live preview), and Terminal (cursor style, blink, opacity). Apply/Cancel/Reset buttons. Changes persist after restart. Based on GitHub issue #3."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Open Settings Dialog (Priority: P1)

User wants to access terminal settings through a keyboard shortcut rather than manually editing JSON configuration files.

**Why this priority**: This is the foundational interaction that enables all other settings functionality. Without the ability to open the dialog, no other features work.

**Independent Test**: Can be fully tested by pressing Ctrl+Comma and verifying a modal dialog appears, delivering immediate access to settings.

**Acceptance Scenarios**:

1. **Given** the terminal is running and focused, **When** user presses Ctrl+Comma, **Then** a modal settings dialog opens centered on the terminal window
2. **Given** the settings dialog is open, **When** user presses Escape or clicks Cancel, **Then** the dialog closes without saving changes
3. **Given** the settings dialog is open, **When** user clicks outside the dialog, **Then** the dialog remains open (modal behavior)

---

### User Story 2 - Change Font Settings (Priority: P1)

User wants to change the terminal font family and size to improve readability or match their preferences.

**Why this priority**: Font settings are the most commonly adjusted terminal setting and directly impact usability.

**Independent Test**: Can be tested by opening settings, selecting a different font family from dropdown, adjusting font size, clicking Apply, and verifying the terminal text updates immediately.

**Acceptance Scenarios**:

1. **Given** the settings dialog is open on the Appearance tab, **When** user selects a different font from the dropdown, **Then** the dropdown shows only monospace fonts available on the system
2. **Given** user has selected a new font, **When** user clicks Apply, **Then** the terminal text immediately renders with the new font
3. **Given** user has changed font size via numeric input, **When** user clicks Apply, **Then** the terminal text renders at the new size and the terminal grid recalculates rows/columns

---

### User Story 3 - Change Color Scheme (Priority: P2)

User wants to customize foreground/background colors to reduce eye strain or match their aesthetic preferences.

**Why this priority**: Color customization is a common preference but less critical than font settings for basic usability.

**Independent Test**: Can be tested by opening settings, clicking color picker for background, selecting a new color, clicking Apply, and verifying background changes.

**Acceptance Scenarios**:

1. **Given** the Appearance tab is active, **When** user clicks the foreground color swatch, **Then** a color picker dialog opens
2. **Given** user selects a new background color, **When** user clicks Apply, **Then** the terminal background immediately updates to the new color
3. **Given** user has made color changes, **When** user clicks Cancel, **Then** the terminal reverts to the original colors

---

### User Story 4 - Configure Shell and Working Directory (Priority: P2)

User wants to change the default shell (e.g., from PowerShell to cmd.exe or WSL) and set a default working directory for new tabs.

**Why this priority**: Important for power users but most users are satisfied with the default PowerShell shell.

**Independent Test**: Can be tested by setting shell path to cmd.exe, clicking Apply, opening a new tab, and verifying cmd.exe launches.

**Acceptance Scenarios**:

1. **Given** the General tab is active, **When** user edits the shell path field, **Then** user can type a path or executable name
2. **Given** user clicks Browse next to working directory, **When** folder picker opens, **Then** user can navigate and select a folder
3. **Given** user has set a new shell path, **When** user opens a new tab after Apply, **Then** the new shell is launched instead of the default

---

### User Story 5 - Adjust Cursor Appearance (Priority: P3)

User wants to change cursor style (block, underline, bar) and toggle cursor blinking.

**Why this priority**: Cursor preferences are personal but have minimal impact on core functionality.

**Independent Test**: Can be tested by selecting Bar cursor style, unchecking blink, clicking Apply, and verifying cursor appearance changes.

**Acceptance Scenarios**:

1. **Given** the Terminal tab is active, **When** user selects a cursor style radio button, **Then** the selection is visually indicated
2. **Given** user has selected Underline cursor and disabled blink, **When** user clicks Apply, **Then** the terminal shows a static underline cursor
3. **Given** cursor blink is enabled, **When** user clicks Apply, **Then** the cursor blinks at the configured rate

---

### User Story 6 - Reset to Defaults (Priority: P3)

User wants to restore all settings to their original default values after making unwanted changes.

**Why this priority**: Safety net feature that is rarely used but important when needed.

**Independent Test**: Can be tested by changing several settings, clicking Reset to Defaults, and verifying all settings return to original values.

**Acceptance Scenarios**:

1. **Given** user has modified multiple settings, **When** user clicks Reset to Defaults, **Then** a confirmation prompt appears
2. **Given** user confirms reset, **When** the reset completes, **Then** all settings in the dialog show default values
3. **Given** user clicks Reset to Defaults then Cancel, **When** dialog closes, **Then** the terminal retains its previous (pre-dialog) settings

---

### Edge Cases

- What happens when user enters an invalid shell path? Dialog shows validation warning, Apply proceeds but warns user
- What happens when selected font is uninstalled after config save? Terminal falls back to Consolas with a warning message
- How does system handle very large scrollback values (e.g., 1,000,000 lines)? Cap at reasonable maximum (100,000) with validation message
- What happens if config.json is read-only or inaccessible? Show error message, allow viewing settings but disable Apply
- What happens when user sets opacity to 0%? Enforce minimum opacity of 10% to prevent invisible window

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST open a modal settings dialog when user presses Ctrl+Comma
- **FR-002**: System MUST organize settings into three tabs: General, Appearance, Terminal
- **FR-003**: System MUST enumerate and display only monospace fonts available on the system
- **FR-004**: System MUST provide color picker dialogs for foreground, background, cursor, and selection colors
- **FR-005**: System MUST validate shell path exists before allowing Apply (warning only, not blocking)
- **FR-006**: System MUST provide a Browse button for selecting working directory via folder picker
- **FR-007**: System MUST provide radio buttons for cursor style selection (Block, Underline, Bar)
- **FR-008**: System MUST provide a checkbox for cursor blink toggle
- **FR-009**: System MUST provide a slider for window opacity (10%-100%)
- **FR-010**: System MUST provide numeric input for scrollback lines (100-100,000)
- **FR-011**: System MUST apply changes immediately when Apply button is clicked
- **FR-012**: System MUST discard all changes when Cancel button is clicked
- **FR-013**: System MUST show confirmation before Reset to Defaults
- **FR-014**: System MUST persist settings to config.json after Apply
- **FR-015**: System MUST load and display current settings when dialog opens
- **FR-016**: System MUST provide numeric input for font size (6-72 points)
- **FR-017**: System MUST recalculate terminal grid dimensions when font settings change

### Key Entities

- **Settings State**: Temporary copy of all configuration values for editing, discarded on Cancel
- **Dialog Tabs**: Logical groupings of related settings (General, Appearance, Terminal)
- **Font Enumerator**: System font query filtered to monospace families
- **Color Picker**: Standard Windows color selection dialog
- **Folder Picker**: Standard Windows folder browser dialog

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Users can open settings dialog within 1 second of pressing Ctrl+Comma
- **SC-002**: Font changes apply and render correctly within 500ms of clicking Apply
- **SC-003**: Color changes are visible immediately (within one frame) after Apply
- **SC-004**: Settings persist correctly across terminal restarts 100% of the time
- **SC-005**: All settings can be modified using only keyboard navigation (accessibility)
- **SC-006**: Dialog correctly displays current settings when opened, matching config.json values
- **SC-007**: Reset to Defaults restores all settings to documented default values

## Assumptions

- The existing Config class and config.json format will be used for persistence
- Windows common dialogs (ChooseColor, SHBrowseForFolder) are acceptable for pickers
- Font enumeration will use DirectWrite or GDI EnumFontFamilies
- The dialog will be implemented as a Win32 dialog resource or custom painted window
- Live preview is not required for the MVP; changes apply on clicking Apply
