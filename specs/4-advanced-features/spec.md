# Feature Specification: Advanced Features

**Feature Branch**: `4-advanced-features`  
**Created**: 2024-12-21  
**Status**: Partial  
**Priority**: Medium  

## Overview

Add advanced terminal features: tabs for multiple sessions, search functionality, URL detection, and split panes.

## User Scenarios & Testing

### User Story 1 - Multiple terminal tabs (Priority: P1)

A user opens multiple terminal sessions in tabs, switches between them, and closes individual tabs.

**Why this priority**: Tabs are essential for multi-tasking without multiple windows.

**Independent Test**: Open new tab with Ctrl+T, switch with Ctrl+Tab, close with Ctrl+W.

**Acceptance Scenarios**:

1. **Given** terminal is open, **When** user presses Ctrl+T, **Then** new tab opens with fresh shell session
2. **Given** multiple tabs open, **When** user presses Ctrl+Tab, **Then** focus moves to next tab
3. **Given** tab is focused, **When** user presses Ctrl+W, **Then** tab closes and shell terminates
4. **Given** only one tab remains, **When** user closes it, **Then** terminal window closes

---

### User Story 2 - Search in scrollback (Priority: P1)

A user searches for text in terminal output to find a previous command or error message.

**Why this priority**: Finding information in long output is a common need.

**Independent Test**: Press Ctrl+Shift+F, type search term, navigate results with F3.

**Acceptance Scenarios**:

1. **Given** search box open, **When** user types "error", **Then** first match is highlighted and scrolled into view
2. **Given** match is highlighted, **When** user presses F3, **Then** next match is highlighted
3. **Given** at last match, **When** user presses F3, **Then** search wraps to first match
4. **Given** no matches found, **When** search completes, **Then** "0 matches" displayed

---

### User Story 3 - Clickable URLs (Priority: P2)

A user Ctrl+clicks a URL in terminal output to open it in their default browser.

**Why this priority**: URLs in output (error messages, logs) are common.

**Independent Test**: Hover over URL, see underline, Ctrl+click opens browser.

**Acceptance Scenarios**:

1. **Given** output contains "https://example.com", **When** mouse hovers, **Then** URL is underlined
2. **Given** URL is underlined, **When** user Ctrl+clicks, **Then** URL opens in default browser
3. **Given** output contains "file://C:/path/file.txt", **When** Ctrl+click, **Then** file opens in default app
4. **Given** OSC 8 hyperlink in output, **When** Ctrl+click, **Then** linked URL opens

---

### User Story 4 - Split panes (Priority: P3)

A user splits the terminal to view multiple sessions side-by-side.

**Why this priority**: Power user feature for complex workflows.

**Independent Test**: Press Ctrl+Shift+D to split, verify two panes with independent shells.

**Acceptance Scenarios**:

1. **Given** single pane, **When** user presses Ctrl+Shift+D, **Then** pane splits vertically with new shell
2. **Given** split panes, **When** user presses Alt+Arrow, **Then** focus moves between panes
3. **Given** focused pane, **When** user presses Ctrl+Shift+W, **Then** pane closes, others expand
4. **Given** only one pane remains, **When** user closes it, **Then** tab closes

---

### Edge Cases

- What happens when searching while output is streaming?
- How does URL detection handle wrapped lines?
- What happens when splitting with insufficient screen space?

## Requirements

### Functional Requirements

#### Tabs
- **FR-001**: Terminal MUST support creating new tabs with Ctrl+T
- **FR-002**: Terminal MUST support closing tabs with Ctrl+W
- **FR-003**: Terminal MUST support tab switching with Ctrl+Tab and Ctrl+1-9
- **FR-004**: Tabs MUST display shell name or custom title
- **FR-005**: Tabs MUST show activity indicator for background updates

#### Search
- **FR-006**: Terminal MUST open search bar with Ctrl+Shift+F
- **FR-007**: Terminal MUST highlight all matches in visible area
- **FR-008**: Terminal MUST support case-sensitive toggle
- **FR-009**: Terminal MUST support regex search toggle
- **FR-010**: Terminal MUST navigate matches with F3/Shift+F3

#### URL Detection
- **FR-011**: Terminal MUST detect http:// and https:// URLs
- **FR-012**: Terminal MUST detect file:// paths
- **FR-013**: Terminal MUST underline URLs on hover
- **FR-014**: Terminal MUST open URLs with Ctrl+Click
- **FR-015**: Terminal MUST support OSC 8 hyperlinks

#### Split Panes
- **FR-016**: Terminal MUST support vertical split (Ctrl+Shift+D)
- **FR-017**: Terminal MUST support horizontal split (Ctrl+Shift+E)
- **FR-018**: Terminal MUST support pane navigation (Alt+Arrow)
- **FR-019**: Terminal MUST support pane resizing (Alt+Shift+Arrow)

### Key Entities

- **TabManager**: Collection of tabs, active tab, tab creation/destruction
- **Tab**: Contains one or more panes, title, activity state
- **Pane**: Single terminal session with buffer and shell
- **SearchEngine**: Pattern matching, result iteration, highlight coordination
- **URLDetector**: Regex-based URL recognition, OSC 8 link parsing

## Success Criteria

### Measurable Outcomes

- **SC-001**: Can open 10+ tabs without performance degradation
- **SC-002**: Search finds matches in 10,000-line scrollback within 500ms
- **SC-003**: 95% of standard URLs are correctly detected
- **SC-004**: Pane operations complete within 100ms
- **SC-005**: Tab switching is visually instant (<16ms)

## Assumptions

- One shell process per pane (no multiplexing)
- Search highlights persist until dismissed or text changes
- URL detection uses standard regex patterns, not ML-based

## Dependencies

- ConPTY for multiple shell instances
- Win32 ShellExecute for opening URLs
- Existing glyph atlas supports underline styling
