# Feature Specification: Mouse Enhancement

**Feature Branch**: `3-mouse-enhancement`  
**Created**: 2024-12-21  
**Status**: Complete  
**Priority**: Medium  

## Overview

Enhance mouse functionality with application mouse reporting, smart selection (double/triple-click), and context menus.

## User Scenarios & Testing

### User Story 1 - Mouse reporting for TUI apps (Priority: P1)

A user runs a mouse-aware TUI application (like htop, mc, or tmux) and can interact with it using the mouse.

**Why this priority**: Many modern TUI apps rely on mouse input.

**Independent Test**: Run htop, click on a process, verify it selects that process.

**Acceptance Scenarios**:

1. **Given** app enables mouse mode 1000, **When** user clicks, **Then** terminal sends click coordinates to app
2. **Given** app enables mouse mode 1002, **When** user drags, **Then** terminal sends motion events
3. **Given** app enables SGR mode 1006, **When** user clicks at column 150, **Then** coordinates sent correctly (not truncated)

---

### User Story 2 - Double-click word selection (Priority: P1)

A user double-clicks on a word to select it, then copies with Ctrl+C.

**Why this priority**: Standard text editing convention expected by all users.

**Independent Test**: Double-click on a word in output, verify entire word selected.

**Acceptance Scenarios**:

1. **Given** terminal shows "hello world", **When** user double-clicks "world", **Then** "world" is selected
2. **Given** terminal shows "file.txt", **When** user double-clicks "file", **Then** "file" is selected (stops at dot)
3. **Given** terminal shows URL "https://example.com", **When** user double-clicks, **Then** entire URL is selected

---

### User Story 3 - Triple-click line selection (Priority: P2)

A user triple-clicks to select an entire line.

**Why this priority**: Standard text editing convention.

**Independent Test**: Triple-click on a line, verify entire line selected including leading/trailing spaces.

**Acceptance Scenarios**:

1. **Given** terminal has text line, **When** user triple-clicks anywhere on line, **Then** entire line is selected
2. **Given** line has leading whitespace, **When** triple-click, **Then** selection includes leading whitespace
3. **Given** line wraps visually, **When** triple-click, **Then** only logical line selected (not wrapped portion)

---

### User Story 4 - Right-click context menu (Priority: P3)

A user right-clicks to access copy, paste, and select all options.

**Why this priority**: Convenience feature for mouse-oriented users.

**Independent Test**: Right-click, verify context menu appears with Copy/Paste options.

**Acceptance Scenarios**:

1. **Given** text is selected, **When** right-click, **Then** context menu shows "Copy" option enabled
2. **Given** clipboard has text, **When** right-click, **Then** context menu shows "Paste" option enabled
3. **Given** user clicks "Select All", **When** menu action triggered, **Then** all buffer content selected

---

### Edge Cases

- What happens when double-clicking on whitespace?
- How does mouse reporting interact with local selection?
- What happens when clicking outside visible buffer area?

## Requirements

### Functional Requirements

#### Mouse Reporting
- **FR-001**: Terminal MUST support X10 mouse mode (mode 1000)
- **FR-002**: Terminal MUST support cell motion tracking (mode 1002)
- **FR-003**: Terminal MUST support all motion tracking (mode 1003)
- **FR-004**: Terminal MUST support SGR extended mouse format (mode 1006)
- **FR-005**: Mouse reporting MUST disable local selection while active

#### Selection Enhancement
- **FR-006**: Terminal MUST support double-click word selection
- **FR-007**: Terminal MUST support triple-click line selection
- **FR-008**: Terminal MUST support Shift+Click to extend selection
- **FR-009**: Word boundaries MUST respect common programming delimiters

#### Context Menu
- **FR-010**: Terminal MUST show context menu on right-click
- **FR-011**: Context menu MUST include Copy, Paste, Select All options
- **FR-012**: Context menu MUST grey out unavailable options

### Key Entities

- **MouseState**: Current mouse button state, position, reporting mode
- **SelectionManager**: Selection start/end, word/line detection logic
- **ContextMenu**: Win32 popup menu with action handlers

## Success Criteria

### Measurable Outcomes

- **SC-001**: htop process selection works via mouse click
- **SC-002**: mc file manager navigation works via mouse
- **SC-003**: Double-click selects words in 95% of common cases
- **SC-004**: Triple-click selects entire logical line
- **SC-005**: Context menu appears within 50ms of right-click

## Assumptions

- Mouse reporting modes are mutually exclusive (latest mode wins)
- Word selection uses programmer-friendly delimiters (space, punctuation, brackets)
- Context menu uses native Win32 popup menu for platform consistency

## Dependencies

- ConPTY supports mouse input forwarding
- Win32 TrackPopupMenu for context menu
- Existing selection highlighting infrastructure
