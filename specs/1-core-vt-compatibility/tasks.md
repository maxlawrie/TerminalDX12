# Tasks: Core VT Compatibility

**Input**: Design documents from `/specs/1-core-vt-compatibility/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md

**Tests**: Unit tests are REQUIRED per the TerminalDX12 Constitution (Test-First Development).

**Organization**: Tasks grouped by user story for independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: User story label (US1, US2, US3, US4)

---

## Phase 1: Setup

**Purpose**: Prepare data structures for VT compatibility features

- [ ] T001 [P] Add FLAG_DIM constant to CellAttributes in include/terminal/ScreenBuffer.h
- [ ] T002 [P] Add FLAG_STRIKETHROUGH constant to CellAttributes in include/terminal/ScreenBuffer.h
- [ ] T003 [P] Add FLAG_TRUECOLOR_FG and FLAG_TRUECOLOR_BG constants in include/terminal/ScreenBuffer.h
- [ ] T004 [P] Add RGB fields (fgR, fgG, fgB, bgR, bgG, bgB) to CellAttributes in include/terminal/ScreenBuffer.h
- [ ] T005 Add helper methods (IsDim, IsStrikethrough, UsesTrueColorFg/Bg) in include/terminal/ScreenBuffer.h
- [ ] T006 Add SetForegroundRGB and SetBackgroundRGB methods in include/terminal/ScreenBuffer.h
- [ ] T007 Verify build succeeds with extended CellAttributes structure

**Checkpoint**: CellAttributes extended with new flags and RGB support

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure - MUST complete before user stories

- [ ] T008 [P] Add SavedCursorState struct to include/terminal/VTStateMachine.h
- [ ] T009 [P] Add scroll region members (m_scrollTop, m_scrollBottom) to include/terminal/ScreenBuffer.h
- [ ] T010 [P] Add mode state booleans (m_applicationCursorKeys, m_autoWrap, m_bracketedPaste) to include/terminal/VTStateMachine.h
- [ ] T011 [P] Add response callback (m_responseCallback, SetResponseCallback) to include/terminal/VTStateMachine.h
- [ ] T012 Add SetScrollRegion and ScrollRegionUp/Down method declarations in include/terminal/ScreenBuffer.h
- [ ] T013 Implement scroll region methods in src/terminal/ScreenBuffer.cpp
- [ ] T014 Add unit tests for scroll region in tests/unit/ScreenBuffer_test.cpp
- [ ] T015 Wire up response callback in src/core/Application.cpp to write to ConPTY

**Checkpoint**: Foundation ready - scroll regions, cursor state, mode tracking, response callback

---

## Phase 3: User Story 1 - Run Claude Code (Priority: P1) MVP

**Goal**: Claude Code runs with proper true colors and dim text rendering

**Independent Test**: Launch Claude Code, verify colors accurate (not mapped to 16), dim text at 50% intensity

### Tests for User Story 1

- [ ] T016 [P] [US1] Add unit test for SGR 2 (dim) parsing in tests/unit/VTStateMachine_test.cpp
- [ ] T017 [P] [US1] Add unit test for SGR 38;2;R;G;B storing RGB in tests/unit/VTStateMachine_test.cpp
- [ ] T018 [P] [US1] Add unit test for SGR 48;2;R;G;B storing RGB in tests/unit/VTStateMachine_test.cpp

### Implementation for User Story 1

- [ ] T019 [US1] Implement SGR 2 (dim) handling in HandleSGR() in src/terminal/VTStateMachine.cpp
- [ ] T020 [US1] Implement SGR 22 to clear both bold and dim in HandleSGR() in src/terminal/VTStateMachine.cpp
- [ ] T021 [US1] Modify SGR 38;2;R;G;B to store true RGB using SetForegroundRGB in src/terminal/VTStateMachine.cpp
- [ ] T022 [US1] Modify SGR 48;2;R;G;B to store true RGB using SetBackgroundRGB in src/terminal/VTStateMachine.cpp
- [ ] T023 [US1] Update color resolution to use true RGB when FLAG_TRUECOLOR set in src/core/Application.cpp
- [ ] T024 [US1] Apply 0.5 intensity multiplier when FLAG_DIM set in rendering in src/core/Application.cpp
- [ ] T025 [US1] Run all unit tests and verify passing

**Checkpoint**: True color and dim text work - Claude Code colors display accurately

---

## Phase 4: User Story 2 - Run vim (Priority: P1)

**Goal**: vim opens, allows editing, exits cleanly preserving shell state

**Independent Test**: Open vim, edit file, use cursor keys, exit :q, verify shell history preserved

### Tests for User Story 2

- [ ] T026 [P] [US2] Add unit test for ESC 7 (DECSC) cursor save in tests/unit/VTStateMachine_test.cpp
- [ ] T027 [P] [US2] Add unit test for ESC 8 (DECRC) cursor restore in tests/unit/VTStateMachine_test.cpp
- [ ] T028 [P] [US2] Add unit test for CSI ?7h/l (DECAWM) in tests/unit/VTStateMachine_test.cpp
- [ ] T029 [P] [US2] Add unit test for CSI ?1h/l (DECCKM) in tests/unit/VTStateMachine_test.cpp

### Implementation for User Story 2

- [ ] T030 [US2] Implement DECSC (ESC 7) handler in HandleEscapeSequence() in src/terminal/VTStateMachine.cpp
- [ ] T031 [US2] Implement DECRC (ESC 8) handler in HandleEscapeSequence() in src/terminal/VTStateMachine.cpp
- [ ] T032 [US2] Implement DECAWM mode 7 in HandleMode() in src/terminal/VTStateMachine.cpp
- [ ] T033 [US2] Implement DECCKM mode 1 in HandleMode() in src/terminal/VTStateMachine.cpp
- [ ] T034 [US2] Add IsApplicationCursorKeysEnabled() accessor in include/terminal/VTStateMachine.h
- [ ] T035 [US2] Update keyboard handler to send ESC O x when DECCKM enabled in src/core/Application.cpp
- [ ] T036 [US2] Run all unit tests and verify passing

**Checkpoint**: vim cursor keys work, alternate screen preserves shell state

---

## Phase 5: User Story 3 - Cursor Behavior (Priority: P2)

**Goal**: Cursor show/hide, save/restore position, cursor position reports

**Independent Test**: Run app that hides cursor, verify hidden then restored; request CPR, verify response

### Tests for User Story 3

- [ ] T037 [P] [US3] Add unit test for CSI ?25h/l (DECTCEM) in tests/unit/VTStateMachine_test.cpp
- [ ] T038 [P] [US3] Add unit test for CSI s (SCP) in tests/unit/VTStateMachine_test.cpp
- [ ] T039 [P] [US3] Add unit test for CSI u (RCP) in tests/unit/VTStateMachine_test.cpp
- [ ] T040 [P] [US3] Add unit test for CSI 6n (CPR) response in tests/unit/VTStateMachine_test.cpp

### Implementation for User Story 3

- [ ] T041 [US3] Implement DECTCEM mode 25 in HandleMode() in src/terminal/VTStateMachine.cpp
- [ ] T042 [US3] Add CSI s handler for cursor position save in HandleCSI() in src/terminal/VTStateMachine.cpp
- [ ] T043 [US3] Add CSI u handler for cursor position restore in HandleCSI() in src/terminal/VTStateMachine.cpp
- [ ] T044 [US3] Implement HandleDeviceStatusReport() for CSI 6n in src/terminal/VTStateMachine.cpp
- [ ] T045 [US3] Implement HandleDeviceAttributes() for CSI c in src/terminal/VTStateMachine.cpp
- [ ] T046 [US3] Run all unit tests and verify passing

**Checkpoint**: Cursor visibility toggles, position save/restore works, device responses sent

---

## Phase 6: User Story 4 - Scroll Regions (Priority: P2)

**Goal**: Scroll regions with fixed headers/footers for htop-like apps

**Independent Test**: Run htop, verify header stays fixed while process list scrolls

### Tests for User Story 4

- [ ] T047 [P] [US4] Add unit test for DECSTBM (CSI t;b r) in tests/unit/VTStateMachine_test.cpp
- [ ] T048 [P] [US4] Add unit test for scroll region bounds in tests/unit/ScreenBuffer_test.cpp
- [ ] T049 [P] [US4] Add unit test for CSI n S (scroll up) in tests/unit/VTStateMachine_test.cpp
- [ ] T050 [P] [US4] Add unit test for CSI n T (scroll down) in tests/unit/VTStateMachine_test.cpp

### Implementation for User Story 4

- [ ] T051 [US4] Implement HandleSetScrollingRegion() for DECSTBM in src/terminal/VTStateMachine.cpp
- [ ] T052 [US4] Add HandleScrollUp() for CSI n S in src/terminal/VTStateMachine.cpp
- [ ] T053 [US4] Add HandleScrollDown() for CSI n T in src/terminal/VTStateMachine.cpp
- [ ] T054 [US4] Modify NewLine in ScreenBuffer to respect scroll region in src/terminal/ScreenBuffer.cpp
- [ ] T055 [US4] Run all unit tests and verify passing

**Checkpoint**: Scroll regions work - htop header stays fixed

---

## Phase 7: Additional VT Sequences

**Goal**: Complete remaining sequences for full compatibility

### Tests

- [ ] T056 [P] Add unit test for SGR 9 (strikethrough) in tests/unit/VTStateMachine_test.cpp
- [ ] T057 [P] Add unit test for CSI ?2004h/l (bracketed paste) in tests/unit/VTStateMachine_test.cpp

### Implementation

- [ ] T058 Implement SGR 9 (strikethrough) in HandleSGR() in src/terminal/VTStateMachine.cpp
- [ ] T059 Implement SGR 29 (not strikethrough) in HandleSGR() in src/terminal/VTStateMachine.cpp
- [ ] T060 Render strikethrough line through text center in src/core/Application.cpp
- [ ] T061 Implement bracketed paste mode 2004 in HandleMode() in src/terminal/VTStateMachine.cpp
- [ ] T062 Wrap pasted text in bracket sequences when mode enabled in src/core/Application.cpp

**Checkpoint**: All Phase 1 VT sequences implemented

---

## Phase 8: Polish and Integration Testing

**Purpose**: Validation across all user stories

- [ ] T063 Run complete unit test suite (run_tests.bat)
- [ ] T064 Manual test: Launch vim, edit file, exit, verify shell state preserved
- [ ] T065 Manual test: Launch htop, verify header fixed, process list scrolls
- [ ] T066 Manual test: Run Claude Code for 10 minutes, verify no rendering artifacts
- [ ] T067 Update SPECIFICATION.md status for all completed sequences
- [ ] T068 Commit all changes with descriptive message

---

## Dependencies and Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies - start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 - BLOCKS all user stories
- **Phases 3-6 (User Stories)**: All depend on Phase 2 completion, can run in parallel
- **Phase 7 (Additional)**: Depends on Phase 2, can run parallel with user stories
- **Phase 8 (Polish)**: Depends on all previous phases

### User Story Independence

All four user stories can be implemented in parallel after Phase 2:
- **US1 (Claude Code)**: True color, dim - no dependencies on other stories
- **US2 (vim)**: Cursor save/restore, modes - no dependencies on other stories
- **US3 (Cursor)**: Visibility, CPR - no dependencies on other stories
- **US4 (Scroll Regions)**: DECSTBM - no dependencies on other stories

### Parallel Opportunities

```
Phase 1: T001, T002, T003, T004 can run in parallel
Phase 2: T008, T009, T010, T011 can run in parallel
US1 Tests: T016, T017, T018 can run in parallel
US2 Tests: T026, T027, T028, T029 can run in parallel
US3 Tests: T037, T038, T039, T040 can run in parallel
US4 Tests: T047, T048, T049, T050 can run in parallel
```

---

## Implementation Strategy

### MVP First (Claude Code Compatibility)

1. Complete Phase 1: Setup (T001-T007) - 7 tasks
2. Complete Phase 2: Foundational (T008-T015) - 8 tasks
3. Complete Phase 3: User Story 1 (T016-T025) - 10 tasks
4. **VALIDATE**: Test Claude Code - colors and dim text should work
5. Continue to remaining user stories

### Incremental Delivery

1. Setup + Foundational = Infrastructure ready
2. Add US1 = Claude Code partial compatibility
3. Add US2 = Editor (vim) support
4. Add US3 = Full cursor/device query support
5. Add US4 = TUI app (htop) support

---

## Summary

| Phase | Tasks | Parallel | Description |
|-------|-------|----------|-------------|
| Setup | 7 | 4 | Extend CellAttributes |
| Foundational | 8 | 4 | Scroll regions, state tracking |
| US1 (Claude Code) | 10 | 3 | True color, dim |
| US2 (vim) | 11 | 4 | Cursor save/restore, modes |
| US3 (Cursor) | 10 | 4 | Visibility, CPR, device attrs |
| US4 (Scroll Regions) | 9 | 4 | DECSTBM, scroll commands |
| Additional | 7 | 2 | Strikethrough, bracketed paste |
| Polish | 6 | 0 | Integration testing |
| **Total** | **68** | **25** | |

**MVP Scope**: Phases 1-3 (25 tasks) delivers Claude Code compatibility
**Full Scope**: All phases (68 tasks) delivers complete Phase 1 VT compatibility
