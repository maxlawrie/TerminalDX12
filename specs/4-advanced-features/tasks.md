# Tasks: Advanced Features (Remaining)

**Input**: Design documents from `/specs/4-advanced-features/`
**Prerequisites**: plan.md, spec.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1=Tabs, US2=Search, US3=URL)

---

## Phase 1: User Story 1 - Tab Persistence & Reordering (Priority: P3)

**Goal**: Save/restore tabs across sessions, enable drag-to-reorder

**Independent Test**: Close terminal with 3 tabs, reopen, verify all 3 tabs restored

### Tests for User Story 1

- [ ] T001 [P] [US1] Unit test for TabManager::SaveState() in tests/unit/test_tab_manager.cpp
- [ ] T002 [P] [US1] Unit test for TabManager::RestoreState() in tests/unit/test_tab_manager.cpp
- [ ] T003 [P] [US1] Unit test for TabManager::MoveTab() in tests/unit/test_tab_manager.cpp

### Implementation for User Story 1

- [ ] T004 [US1] Add SaveState()/RestoreState() declarations to include/ui/TabManager.h
- [ ] T005 [US1] Implement TabManager::SaveState() - serialize to %APPDATA%\TerminalDX12\tabs.json
- [ ] T006 [US1] Implement TabManager::RestoreState() - deserialize and create tabs
- [ ] T007 [US1] Call SaveState() from Application::Shutdown()
- [ ] T008 [US1] Call RestoreState() from Application::Initialize() after TabManager creation
- [ ] T009 [US1] Add MoveTab(fromIndex, toIndex) declaration to include/ui/TabManager.h
- [ ] T010 [US1] Implement TabManager::MoveTab() - reorder tabs in vector
- [ ] T011 [US1] Add drag state tracking to Application (m_draggingTab, m_dragStartX)
- [ ] T012 [US1] Handle WM_LBUTTONDOWN on tab bar - start drag if on tab
- [ ] T013 [US1] Handle WM_MOUSEMOVE during drag - update visual feedback
- [ ] T014 [US1] Handle WM_LBUTTONUP - complete drag, call MoveTab()
- [ ] T015 [US1] Render drag indicator (highlight drop position)

**Checkpoint**: Tabs persist across restart, can be reordered by drag

---

## Phase 2: User Story 2 - Enhanced Search (Priority: P2)

**Goal**: Add regex support and search through scrollback buffer

**Independent Test**: Type regex pattern like `error\d+`, verify matches highlighted

### Tests for User Story 2

- [ ] T016 [P] [US2] Unit test for regex search in tests/unit/test_search.cpp
- [ ] T017 [P] [US2] Unit test for scrollback search in tests/unit/test_search.cpp
- [ ] T018 [P] [US2] Unit test for search edge cases (empty pattern, invalid regex)

### Implementation for User Story 2

- [ ] T019 [US2] Add m_searchRegexEnabled flag to Application class
- [ ] T020 [US2] Add regex toggle UI element to search bar
- [ ] T021 [US2] Modify UpdateSearchResults() to use std::regex when enabled
- [ ] T022 [US2] Add try/catch for regex compilation errors, show error in search bar
- [ ] T023 [US2] Extend UpdateSearchResults() to search scrollback via GetCellWithScrollback()
- [ ] T024 [US2] Update search match positions to include scrollback offset
- [ ] T025 [US2] Modify SearchNext()/SearchPrevious() to scroll to scrollback matches
- [ ] T026 [US2] Add match count display showing "X of Y matches"
- [ ] T027 [P] [US2] Add keyboard shortcut for regex toggle (Alt+R or Ctrl+Alt+R)

**Checkpoint**: Can search with regex, finds matches in scrollback history

---

## Phase 3: User Story 2b - Search All Tabs (Priority: P3)

**Goal**: Search across all open tabs, not just active tab

**Independent Test**: Open 3 tabs with different content, search finds matches in all

### Implementation for User Story 2b

- [ ] T028 [US2] Add m_searchAllTabs flag to Application class
- [ ] T029 [US2] Add "All Tabs" toggle button to search UI
- [ ] T030 [US2] Modify UpdateSearchResults() to iterate all tabs when enabled
- [ ] T031 [US2] Store tab index with each search match result
- [ ] T032 [US2] Modify SearchNext() to switch tabs when navigating to match in different tab
- [ ] T033 [US2] Update match count to show "X of Y (across N tabs)"

**Checkpoint**: Search finds and navigates to matches across all tabs

---

## Phase 4: User Story 3 - URL Context Menu (Priority: P3)

**Goal**: Right-click on URL shows "Copy URL" option

**Independent Test**: Right-click on https://example.com, select Copy URL, paste shows URL

### Implementation for User Story 3

- [ ] T034 [US3] Modify ShowContextMenu() to detect if click is on URL
- [ ] T035 [US3] Add "Copy URL" menu item when URL detected at click position
- [ ] T036 [US3] Implement CopyUrlToClipboard() helper function
- [ ] T037 [US3] Handle "Copy URL" menu selection - call CopyUrlToClipboard()
- [ ] T038 [P] [US3] Add "Open URL" as secondary option in URL context menu

**Checkpoint**: Right-click on URL provides Copy URL option

---

## Phase 5: Polish & Documentation

- [ ] T039 [P] Update SPECIFICATION.md with new feature statuses
- [ ] T040 [P] Update CHANGELOG.md with Phase 4 completion
- [ ] T041 [P] Add user documentation for new search features
- [ ] T042 Run full test suite, fix any regressions
- [ ] T043 Update spec.md status to Complete

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Tab Persistence)**: Can start immediately
- **Phase 2 (Enhanced Search)**: Can start immediately, parallel with Phase 1
- **Phase 3 (Search All Tabs)**: Depends on Phase 2 completion
- **Phase 4 (URL Context Menu)**: Can start immediately, parallel with others
- **Phase 5 (Polish)**: After all features complete

### Recommended Execution Order

1. Start T001-T003 (tests) and T016-T018 (search tests) in parallel
2. Implement T004-T008 (tab save/restore) - quick win
3. Implement T019-T027 (enhanced search) - highest value
4. Implement T009-T015 (tab drag) - UX polish
5. Implement T028-T033 (search all tabs) - nice to have
6. Implement T034-T038 (URL context menu) - quick addition
7. Complete T039-T043 (polish)

### Parallel Opportunities

```text
# Can run simultaneously:
Phase 1 tests (T001-T003)
Phase 2 tests (T016-T018)
Phase 4 implementation (T034-T038)

# After tests pass:
Tab persistence (T004-T008) || Enhanced search (T019-T027)
```

---

## Task Count Summary

| Phase | Tasks | Priority |
|-------|-------|----------|
| US1: Tab Persistence | 15 | P3 |
| US2: Enhanced Search | 12 | P2 |
| US2b: Search All Tabs | 6 | P3 |
| US3: URL Context Menu | 5 | P3 |
| Polish | 5 | - |
| **Total** | **43** | |
