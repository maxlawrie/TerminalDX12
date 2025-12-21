# Feature Specification: Core VT Compatibility

**Feature Branch**: `1-core-vt-compatibility`  
**Created**: 2024-12-21  
**Status**: In Progress  
**Priority**: Critical  

## Overview

Implement complete VT100/ANSI escape sequence support required for modern terminal applications including Claude Code, vim, htop, and other TUI programs.

## User Scenarios & Testing

### User Story 1 - Run Claude Code (Priority: P1)

A developer opens TerminalDX12 and runs Claude Code. The interface displays correctly with proper colors, cursor positioning, and text attributes. The status line updates dynamically and syntax highlighting works.

**Why this priority**: Claude Code compatibility is the primary use case driving this terminal's development.

**Independent Test**: Launch Claude Code, verify status line renders, type a prompt, and confirm response displays with proper formatting.

**Acceptance Scenarios**:

1. **Given** Claude Code is running, **When** it sends 24-bit color sequences, **Then** colors display accurately (not mapped to 16-color)
2. **Given** Claude Code updates the status line, **When** it sends cursor positioning sequences, **Then** cursor moves to correct position
3. **Given** Claude Code sends dim text (SGR 2), **When** rendering, **Then** text appears at reduced intensity

---

### User Story 2 - Run vim with full feature support (Priority: P1)

A developer opens vim in TerminalDX12. The editor displays correctly, cursor movements work, and the alternate screen buffer preserves shell history.

**Why this priority**: vim is a fundamental test of VT compatibility.

**Independent Test**: Open vim, edit a file, use cursor keys, exit vim, verify shell history intact.

**Acceptance Scenarios**:

1. **Given** vim is running, **When** pressing arrow keys, **Then** cursor moves correctly in normal mode
2. **Given** vim is open, **When** user exits with :q, **Then** shell prompt returns with history preserved
3. **Given** vim is in insert mode, **When** auto-wrap is enabled, **Then** text wraps at screen edge

---

### User Story 3 - Proper cursor behavior (Priority: P2)

Applications can show/hide the cursor, save/restore cursor position, and receive cursor position reports.

**Why this priority**: Many TUI apps depend on cursor manipulation for proper rendering.

**Independent Test**: Run an app that hides cursor during updates, verify cursor hidden then restored.

**Acceptance Scenarios**:

1. **Given** app sends DECTCEM disable (CSI ?25l), **When** rendering, **Then** cursor is not visible
2. **Given** app saves cursor (CSI s), moves cursor, then restores (CSI u), **Then** cursor returns to saved position
3. **Given** app requests cursor position (CSI 6n), **When** terminal responds, **Then** response contains current row;col

---

### User Story 4 - Scroll region support (Priority: P2)

Applications can define scroll regions to scroll content within a portion of the screen while keeping headers/footers fixed.

**Why this priority**: Required for apps with fixed UI elements (status bars, menus).

**Independent Test**: Run less or htop, verify header stays fixed while content scrolls.

**Acceptance Scenarios**:

1. **Given** DECSTBM sets scroll region rows 2-23, **When** content scrolls, **Then** rows 1 and 24 remain unchanged
2. **Given** scroll region is set, **When** cursor at bottom of region and newline received, **Then** region scrolls up

---

### Edge Cases

- What happens when scroll region boundaries are invalid (top > bottom)?
- How does system handle cursor position requests during rapid output?
- What happens when saving cursor twice without restore?

## Requirements

### Functional Requirements

#### Cursor Control
- **FR-001**: Terminal MUST support cursor hide/show (DECTCEM mode 25)
- **FR-002**: Terminal MUST support cursor save/restore (CSI s / CSI u)
- **FR-003**: Terminal MUST support DECSC/DECRC (ESC 7 / ESC 8) with full state
- **FR-004**: Terminal MUST support Cursor Horizontal Absolute (CSI n G)
- **FR-005**: Terminal MUST respond to Cursor Position Report requests (CSI 6n)

#### Mode Settings
- **FR-006**: Terminal MUST support Application Cursor Keys mode (DECCKM mode 1)
- **FR-007**: Terminal MUST support Auto-Wrap mode (DECAWM mode 7)
- **FR-008**: Terminal MUST support Bracketed Paste mode (mode 2004)

#### Text Attributes
- **FR-009**: Terminal MUST support Dim/Faint text (SGR 2)
- **FR-010**: Terminal MUST support Strikethrough text (SGR 9)
- **FR-011**: Terminal MUST store and render full 24-bit RGB colors

#### Character/Line Operations
- **FR-012**: Terminal MUST support Insert Characters (CSI n @)
- **FR-013**: Terminal MUST support Delete Characters (CSI n P)
- **FR-014**: Terminal MUST support Insert Lines (CSI n L)
- **FR-015**: Terminal MUST support Delete Lines (CSI n M)
- **FR-016**: Terminal MUST support Erase Characters (CSI n X)

#### Scrolling
- **FR-017**: Terminal MUST support scroll regions (DECSTBM CSI t;b r)
- **FR-018**: Terminal MUST support Scroll Up (CSI n S) within scroll region
- **FR-019**: Terminal MUST support Scroll Down (CSI n T) within scroll region

#### Device Responses
- **FR-020**: Terminal MUST respond to Primary Device Attributes (CSI c)
- **FR-021**: Terminal MUST respond to Device Status Report (CSI 5n)

### Key Entities

- **ScreenBuffer**: Manages cell grid, cursor state, scroll region boundaries
- **VTStateMachine**: Parses escape sequences, dispatches to appropriate handlers
- **CellAttributes**: Extended to include dim flag, strikethrough flag, full RGB storage

## Success Criteria

### Measurable Outcomes

- **SC-001**: Claude Code runs without rendering artifacts for 30-minute sessions
- **SC-002**: vim opens, allows editing, and exits cleanly preserving shell state
- **SC-003**: htop displays correctly with fixed header and scrolling process list
- **SC-004**: All 176 existing unit tests continue to pass
- **SC-005**: New unit tests achieve 100% coverage of implemented VT sequences

## Assumptions

- VT100/xterm behavior is the reference standard for escape sequence implementation
- Applications expect response sequences for device queries within 100ms
- Scroll region operations apply only to the active screen buffer

## Dependencies

- Existing VTStateMachine parser infrastructure
- Screen buffer cell storage
- DirectX 12 renderer (for new text attributes like dim, strikethrough)
