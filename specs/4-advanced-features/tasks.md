# Tasks: Advanced Features (Remaining)

**Input**: Design documents from `/specs/4-advanced-features/`
**Prerequisites**: plan.md, spec.md
**Task ID Scope**: `4-Txxx` (IDs are scoped to this phase; use prefix when cross-referencing)

## Format: `[ID] [P?] [Story] Description @agent`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1=Tabs, US2=Search, US3=URL)
- **@agent**: Recommended agent for this task

---

## Phase 1: User Story 1 - Tab Persistence & Reordering (Priority: P3)

**Goal**: Save/restore tabs across sessions, enable drag-to-reorder

**Independent Test**: Close terminal with 3 tabs, reopen, verify all 3 tabs restored

### Tests for User Story 1

- [ ] T001 [P] [US1] Unit test for TabManager::SaveState() in tests/unit/test_tab_manager.cpp @test-automator
- [ ] T002 [P] [US1] Unit test for TabManager::RestoreState() in tests/unit/test_tab_manager.cpp @test-automator
- [ ] T003 [P] [US1] Unit test for TabManager::MoveTab() in tests/unit/test_tab_manager.cpp @test-automator

### Implementation for User Story 1

- [ ] T004 [US1] Add SaveState()/RestoreState() declarations to include/ui/TabManager.h @cpp-pro
- [ ] T005 [US1] Implement TabManager::SaveState() - serialize to %APPDATA%\TerminalDX12\tabs.json @cpp-pro
- [ ] T006 [US1] Implement TabManager::RestoreState() - deserialize and create tabs @cpp-pro
- [ ] T007 [US1] Call SaveState() from Application::Shutdown() @cpp-pro
- [ ] T008 [US1] Call RestoreState() from Application::Initialize() after TabManager creation @cpp-pro
- [ ] T009 [US1] Add MoveTab(fromIndex, toIndex) declaration to include/ui/TabManager.h @cpp-pro
- [ ] T010 [US1] Implement TabManager::MoveTab() - reorder tabs in vector @cpp-pro
- [ ] T011 [US1] Add drag state tracking to Application (m_draggingTab, m_dragStartX) @cpp-pro
- [ ] T012 [US1] Handle WM_LBUTTONDOWN on tab bar - start drag if on tab @cpp-pro
- [ ] T013 [US1] Handle WM_MOUSEMOVE during drag - update visual feedback @cpp-pro
- [ ] T014 [US1] Handle WM_LBUTTONUP - complete drag, call MoveTab() @cpp-pro
- [ ] T015 [US1] Render drag indicator (highlight drop position) @cpp-pro

**Checkpoint**: Tabs persist across restart, can be reordered by drag

---

## Phase 2: User Story 2 - Enhanced Search (Priority: P2)

**Goal**: Add regex support and search through scrollback buffer

**Independent Test**: Type regex pattern like `error\d+`, verify matches highlighted

### Tests for User Story 2

- [ ] T016 [P] [US2] Unit test for regex search in tests/unit/test_search.cpp @test-automator
- [ ] T017 [P] [US2] Unit test for scrollback search in tests/unit/test_search.cpp @test-automator
- [ ] T018 [P] [US2] Unit test for search edge cases (empty pattern, invalid regex) @test-automator

### Implementation for User Story 2

- [ ] T019 [US2] Add m_searchRegexEnabled flag to Application class @cpp-pro
- [ ] T020 [US2] Add regex toggle UI element to search bar @cpp-pro
- [ ] T021 [US2] Modify UpdateSearchResults() to use std::regex when enabled @cpp-pro
- [ ] T022 [US2] Add try/catch for regex compilation errors, show error in search bar @cpp-pro
- [ ] T023 [US2] Extend UpdateSearchResults() to search scrollback via GetCellWithScrollback() @cpp-pro
- [ ] T024 [US2] Update search match positions to include scrollback offset @cpp-pro
- [ ] T025 [US2] Modify SearchNext()/SearchPrevious() to scroll to scrollback matches @cpp-pro
- [ ] T026 [US2] Add match count display showing "X of Y matches" @cpp-pro
- [ ] T027 [P] [US2] Add keyboard shortcut for regex toggle (Alt+R or Ctrl+Alt+R) @cpp-pro

**Checkpoint**: Can search with regex, finds matches in scrollback history

---

## Phase 3: User Story 2b - Search All Tabs (Priority: P3)

**Goal**: Search across all open tabs, not just active tab

**Independent Test**: Open 3 tabs with different content, search finds matches in all

### Implementation for User Story 2b

- [ ] T028 [US2] Add m_searchAllTabs flag to Application class @cpp-pro
- [ ] T029 [US2] Add "All Tabs" toggle button to search UI @cpp-pro
- [ ] T030 [US2] Modify UpdateSearchResults() to iterate all tabs when enabled @cpp-pro
- [ ] T031 [US2] Store tab index with each search match result @cpp-pro
- [ ] T032 [US2] Modify SearchNext() to switch tabs when navigating to match in different tab @cpp-pro
- [ ] T033 [US2] Update match count to show "X of Y (across N tabs)" @cpp-pro

**Checkpoint**: Search finds and navigates to matches across all tabs

---

## Phase 4: User Story 3 - URL Context Menu (Priority: P3)

**Goal**: Right-click on URL shows "Copy URL" option

**Independent Test**: Right-click on https://example.com, select Copy URL, paste shows URL

### Implementation for User Story 3

- [ ] T034 [US3] Modify ShowContextMenu() to detect if click is on URL @cpp-pro
- [ ] T035 [US3] Add "Copy URL" menu item when URL detected at click position @cpp-pro
- [ ] T036 [US3] Implement CopyUrlToClipboard() helper function @cpp-pro
- [ ] T037 [US3] Handle "Copy URL" menu selection - call CopyUrlToClipboard() @cpp-pro
- [ ] T038 [P] [US3] Add "Open URL" as secondary option in URL context menu @cpp-pro

**Checkpoint**: Right-click on URL provides Copy URL option

---

## Phase 5: Polish & Documentation

- [ ] T039 [P] Update SPECIFICATION.md with new feature statuses @documentation-engineer
- [ ] T040 [P] Update CHANGELOG.md with Phase 4 completion @documentation-engineer
- [ ] T041 [P] Add user documentation for new search features @documentation-engineer
- [ ] T042 Run full test suite, fix any regressions @test-automator
- [ ] T043 Update spec.md status to Complete @documentation-engineer

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Tab Persistence)**: Can start immediately
- **Phase 2 (Enhanced Search)**: Can start immediately, parallel with Phase 1
- **Phase 3 (Search All Tabs)**: Depends on Phase 2 completion
- **Phase 4 (URL Context Menu)**: Can start immediately, parallel with others
- **Phase 5 (Polish)**: After all features complete

### Recommended Execution Order

1. Start T001-T003 (tests) and T016-T018 (search tests) in parallel @test-automator @test-automator
2. Implement T004-T008 (tab save/restore) - quick win @cpp-pro
3. Implement T019-T027 (enhanced search) - highest value @cpp-pro
4. Implement T009-T015 (tab drag) - UX polish @cpp-pro
5. Implement T028-T033 (search all tabs) - nice to have @cpp-pro
6. Implement T034-T038 (URL context menu) - quick addition @cpp-pro
7. Complete T039-T043 (polish) @documentation-engineer

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

## Agent Assignments Summary

| Agent | Task Count | Responsibility |
|-------|------------|----------------|
| @test-automator | 7 | Unit tests for TabManager, Search, regression testing |
| @cpp-pro | 32 | All C++ implementation tasks |
| @documentation-engineer | 4 | SPECIFICATION.md, CHANGELOG.md, user docs |

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
