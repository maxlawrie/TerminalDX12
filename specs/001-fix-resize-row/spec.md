# Feature Specification: Fix First Row Missing After Terminal Resize

**Feature Branch**: `001-fix-resize-row`
**Created**: 2025-12-22
**Status**: Draft
**Input**: User description: "Fix first row missing after terminal resize - When resizing the terminal window while a TUI application is running, the first row of content disappears. Need to investigate how resize notifications are handled and ensure the app properly redraws all content including row 0 after resize completes."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Resize Terminal with TUI Application Running (Priority: P1)

As a user running a TUI application (like Claude Code) in the terminal, when I resize the terminal window by dragging the edges, I expect all content including the header/first row to remain visible after the resize completes.

**Why this priority**: This is the core bug being fixed. Without this, TUI applications lose their header content after resize, making the display incomplete and confusing.

**Independent Test**: Can be fully tested by running a TUI application, resizing the window, and verifying the first row content is preserved.

**Acceptance Scenarios**:

1. **Given** Claude Code is running in the terminal displaying its welcome screen with "Claude Code v2.0.75" header, **When** I resize the window by dragging the edge smaller then larger, **Then** the "Claude Code v2.0.75" header line remains visible after resize completes
2. **Given** any TUI application is running with content on row 0, **When** I resize the terminal window in any direction, **Then** all rows including row 0 display correctly after resize
3. **Given** the terminal is at default size with a TUI application running, **When** I rapidly resize the window multiple times, **Then** the first row content is preserved after all resize operations complete

---

### User Story 2 - Live Resize Visual Feedback (Priority: P2)

As a user resizing the terminal window, I expect the content to redraw smoothly during the drag operation without stretching or corruption.

**Why this priority**: Live resize is working but must continue to work after the bug fix. This ensures the fix doesn't regress existing functionality.

**Independent Test**: Can be tested by dragging the window edge and observing content redraws in real-time.

**Acceptance Scenarios**:

1. **Given** the terminal is displaying any content, **When** I drag the window edge to resize, **Then** the content redraws at the new size without stretching or visual artifacts
2. **Given** a TUI application is running, **When** I resize the window while the application is actively updating, **Then** the resize completes without corruption

---

### Edge Cases

- What happens when resizing makes the terminal too small to display the TUI application's minimum layout?
- How does the system handle resize during active output streaming from the application?
- What happens when the user resizes very rapidly (many resize events per second)?
- How does the system handle resize when using the alternate screen buffer vs primary buffer?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Terminal MUST preserve content on row 0 after any resize operation completes
- **FR-002**: Terminal MUST properly notify the PTY/application of size changes so it can redraw
- **FR-003**: Terminal MUST correctly handle the application's redraw response after resize notification
- **FR-004**: Terminal MUST maintain synchronization between buffer dimensions and PTY-reported dimensions
- **FR-005**: Terminal MUST continue to provide smooth live resize feedback during drag operations
- **FR-006**: Terminal MUST handle both alternate screen buffer and primary buffer resize correctly
- **FR-007**: Terminal MUST properly reset scroll regions after resize to prevent content clipping

### Key Entities

- **Screen Buffer**: The character grid storing terminal content, must resize without losing row 0
- **PTY (Pseudo Terminal)**: The interface to the shell/application, must be notified of correct dimensions
- **Resize Event**: Window resize notifications, must be processed in correct order and timing
- **Alternate Buffer**: Separate buffer used by TUI applications, requires special resize handling

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After any resize operation, 100% of visible rows including row 0 display correctly
- **SC-002**: TUI applications (Claude Code, vim, htop) show complete UI including headers after resize
- **SC-003**: Live resize continues to redraw content during drag without stretching
- **SC-004**: No visual artifacts or corruption appear during or after resize operations
- **SC-005**: Resize behavior matches Windows Terminal's handling of the same TUI applications

## Assumptions

- The TUI application (Claude Code) correctly sends redraw commands when it receives resize notifications
- The issue is within the terminal emulator's resize handling, not in the application itself
- Windows Terminal's behavior is the reference standard for correct resize handling
- The ConPTY interface correctly passes resize notifications to applications
