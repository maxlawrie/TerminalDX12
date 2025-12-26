# Implementation Plan: Advanced Features (Remaining)

**Branch**: `4-advanced-features` | **Date**: 2025-12-26 | **Spec**: [spec.md](spec.md)

## Summary

Complete the remaining advanced features for TerminalDX12. Most of Phase 4 is already implemented (tabs, basic search, URL detection, split panes). This plan covers the remaining polish items:
- Tab persistence (save/restore on restart)
- Tab reordering via drag-and-drop
- Enhanced search (regex, scrollback, all-tabs)
- URL context menu (right-click copy)

## Already Implemented

| Feature | Status | Implementation |
|---------|--------|----------------|
| Multiple tabs (Ctrl+T, Ctrl+W) | Complete | `src/ui/TabManager.cpp` |
| Tab switching (Ctrl+Tab, Ctrl+1-9) | Complete | `src/core/Application.cpp` |
| Basic search (Ctrl+Shift+F) | Complete | `src/core/Application.cpp` |
| Find next/previous (F3/Shift+F3) | Complete | `src/core/Application.cpp` |
| URL detection (http/https/file) | Complete | `src/core/Application.cpp` |
| Ctrl+Click to open URL | Complete | `src/core/Application.cpp` |
| OSC 8 hyperlinks | Complete | `src/terminal/VTStateMachine.cpp` |
| Split panes (vertical/horizontal) | Complete | `src/ui/Pane.cpp` |
| Pane navigation (Alt+Arrow) | Complete | `src/core/Application.cpp` |

## Remaining Work

| Feature | Priority | Complexity | User Story |
|---------|----------|------------|------------|
| Tab reordering (drag) | P3 | Medium | US1 |
| Save tabs on exit | P3 | Low | US1 |
| Restore tabs on startup | P3 | Low | US1 |
| Regex search | P2 | Medium | US2 |
| Search in scrollback | P2 | Medium | US2 |
| Search all tabs | P3 | Medium | US2 |
| Right-click "Copy URL" | P3 | Low | US3 |

## Technical Context

**Language/Version**: C++20 (Visual Studio 2022/2026)
**Primary Dependencies**: FreeType, spdlog, DirectX 12, nlohmann/json
**Storage**: JSON config file for tab persistence
**Testing**: Google Test (gtest), Python integration tests
**Target Platform**: Windows 10 1809+ (ConPTY requirement)
**Performance Goals**: 60 FPS rendering, sub-ms input latency
**Constraints**: <100MB memory for 10K line scrollback

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| GPU-First Rendering | PASS | No changes to rendering pipeline |
| VT/ANSI Compliance | N/A | No VT sequence changes |
| Test-First Development | REQUIRED | Unit tests for search, tab persistence |
| Windows Native | PASS | Win32 drag-drop, file I/O |
| Unicode Support | PASS | Regex must handle UTF-8 |

## Project Structure

### Documentation (this feature)

```text
specs/4-advanced-features/
├── spec.md              # Feature specification
├── plan.md              # This file
└── tasks.md             # Task breakdown
```

### Source Code (affected files)

```text
include/ui/
├── TabManager.h         # Add tab persistence, reorder methods

src/ui/
├── TabManager.cpp       # Implement save/restore, drag-drop

src/core/
├── Application.cpp      # Enhanced search, URL context menu
├── Config.cpp           # Tab state serialization (optional)

tests/unit/
├── test_tab_manager.cpp # New: tab persistence tests
├── test_search.cpp      # New: regex and scrollback search tests
```

## Implementation Phases

### Phase A: Tab Persistence (US1)

1. **Save tabs on exit**:
   - Add `TabManager::SaveState()` method
   - Serialize: tab count, shell command per tab, working directory
   - Save to `%APPDATA%\TerminalDX12\tabs.json`
   - Call from `Application::Shutdown()`

2. **Restore tabs on startup**:
   - Add `TabManager::RestoreState()` method
   - Load `tabs.json` if exists
   - Create tabs with saved shell/directory
   - Fall back to single default tab if file missing/invalid

3. **Tab reordering**:
   - Add `TabManager::MoveTab(fromIndex, toIndex)`
   - Implement WM_LBUTTONDOWN/WM_MOUSEMOVE drag detection on tab bar
   - Visual feedback: ghost tab during drag
   - Update tab order in manager on drop

### Phase B: Enhanced Search (US2)

1. **Search in scrollback**:
   - Extend `UpdateSearchResults()` to search full scrollback buffer
   - Use `ScreenBuffer::GetCellWithScrollback()` for access
   - Scroll to match when navigating results

2. **Regex search**:
   - Add regex toggle button/shortcut to search UI
   - Use `std::regex` for pattern matching
   - Handle regex errors gracefully (show error, keep last valid pattern)
   - Performance: limit regex search to visible + scrollback window

3. **Search all tabs** (optional, lower priority):
   - Add "Search All Tabs" toggle
   - Aggregate results from all tab buffers
   - Show tab indicator in match list

### Phase C: URL Context Menu (US3)

1. **Right-click on URL**:
   - Detect if click is on a URL (reuse `DetectUrlAt()`)
   - If URL detected, show context menu with "Copy URL" option
   - Implement `CopyUrlToClipboard()` helper

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Regex performance on large buffers | Medium | Medium | Limit search scope, use iterators |
| Tab state corruption | Low | Medium | Validate JSON, graceful fallback |
| Drag-drop UX complexity | Medium | Low | Start with simple index swap |

## Dependencies

- `std::regex` (C++ standard library) for regex search
- `nlohmann/json` (already available) for tab state persistence
- Existing Win32 context menu infrastructure for URL copy
