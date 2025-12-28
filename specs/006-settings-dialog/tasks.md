# Tasks: Settings Dialog

**Input**: Design documents from `/specs/006-settings-dialog/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: REQUIRED per constitution Principle III (Test-First Development)

**Organization**: Tasks grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and resource files

**Note**: Using programmatic Win32 dialog creation instead of .rc resource files for better maintainability.

- [X] T001 Create Win32 dialog controls programmatically in `src/ui/SettingsDialog.cpp` (instead of .rc file)
- [X] T002 [P] Create General tab controls programmatically in `src/ui/SettingsDialog.cpp`
- [X] T003 [P] Create Appearance tab controls programmatically in `src/ui/SettingsDialog.cpp`
- [X] T004 [P] Create Terminal tab controls programmatically in `src/ui/SettingsDialog.cpp`
- [X] T005 Define control IDs as enum in `src/ui/SettingsDialog.cpp`
- [X] T006 CMakeLists.txt already includes SettingsDialog.cpp and links necessary libraries

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T007 Create `include/ui/FontEnumerator.h` with FontInfo struct and EnumerateMonospaceFonts() declaration
- [X] T008 Implement DirectWrite font enumeration in `src/ui/FontEnumerator.cpp` using glyph width comparison
- [X] T009 [P] Create `include/ui/SettingsDialog.h` with SettingsDialogState struct and SettingsDialog class
- [X] T010 [P] Add Osc52Policy enum to `include/core/Config.h` if not present (for security configuration)
- [X] T011 Create `src/ui/SettingsDialog.cpp` with basic dialog infrastructure (PropertySheet setup)
- [X] T012 Implement LoadCurrentSettings() to copy Config values to SettingsDialogState in `src/ui/SettingsDialog.cpp`
- [X] T013 Implement SaveSettings() to write SettingsDialogState back to Config and persist to JSON in `src/ui/SettingsDialog.cpp`
- [X] T014 [P] Add ApplySettings() method to `src/core/Config.cpp` for hot-reloading config values
- [X] T015 [P] Add unit test file `tests/unit/test_font_enumerator.cpp` with GTest setup
- [X] T015b [P] Implement font fallback logic in `src/ui/FontEnumerator.cpp`: if configured font not found, return Consolas with spdlog::warn

**Checkpoint**: Foundation ready - user story implementation can now begin

---

## Phase 3: User Story 1 - Open Settings Dialog (Priority: P1) 🎯 MVP

**Goal**: User can open settings dialog with Ctrl+Comma, see three tabs, and close with Cancel/Escape

**Independent Test**: Press Ctrl+Comma, verify modal dialog appears with 3 tabs, press Escape to close

### Tests for User Story 1

- [ ] T016 [P] [US1] Unit test: Dialog opens and shows three tabs in `tests/unit/test_settings_dialog.cpp`
- [ ] T017 [P] [US1] Unit test: Dialog closes on Cancel without saving in `tests/unit/test_settings_dialog.cpp`
- [ ] T018 [P] [US1] Unit test: Dialog is modal (blocks parent input) in `tests/unit/test_settings_dialog.cpp`

### Implementation for User Story 1

- [X] T019 [US1] Add WM_KEYDOWN handler for Ctrl+Comma in `src/core/Application.cpp`
- [X] T020 [US1] Implement ShowSettingsDialog() method in `src/core/Application.cpp`
- [X] T021 [US1] Implement General page dialog procedure in `src/ui/SettingsDialog.cpp`
- [X] T022 [P] [US1] Implement Appearance page dialog procedure in `src/ui/SettingsDialog.cpp`
- [X] T023 [P] [US1] Implement Terminal page dialog procedure in `src/ui/SettingsDialog.cpp`
- [X] T024 [US1] Add keyboard navigation (Tab, arrow keys) for accessibility in all page procedures
- [X] T025 [US1] Implement OnCancel() handler to close dialog without changes in `src/ui/SettingsDialog.cpp`

**Checkpoint**: User Story 1 complete - dialog opens, shows tabs, closes correctly

---

## Phase 4: User Story 2 - Change Font Settings (Priority: P1)

**Goal**: User can select monospace font from dropdown and change font size, changes apply immediately

**Independent Test**: Open settings, select different font, change size to 14, click Apply, verify terminal text updates

### Tests for User Story 2

- [ ] T026 [P] [US2] Unit test: Font dropdown populated with only monospace fonts in `tests/unit/test_settings_dialog.cpp`
- [ ] T027 [P] [US2] Unit test: Font size validation (6-72 range) in `tests/unit/test_settings_dialog.cpp`
- [ ] T028 [P] [US2] Unit test: Apply button triggers font reload in `tests/unit/test_settings_dialog.cpp`
- [X] T028b [P] [US2] Unit test: Missing font triggers fallback to Consolas with warning in `tests/unit/test_font_enumerator.cpp`

### Implementation for User Story 2

- [X] T029 [US2] Populate font dropdown with EnumerateMonospaceFonts() on dialog init in `src/ui/SettingsDialog.cpp`
- [X] T030 [US2] Add font size numeric input with spinner control (6-72 range) in Appearance page
- [X] T031 [US2] Implement OnApply() handler to read font settings and call ApplySettings() in `src/ui/SettingsDialog.cpp`
- [ ] T032 [US2] Add ReloadFont() method to `src/rendering/TextRenderer.cpp` for hot font swapping
- [ ] T033 [US2] Implement grid recalculation in `src/core/Application.cpp` after font change (resize terminal grid)

**Checkpoint**: User Story 2 complete - font changes apply immediately

---

## Phase 5: User Story 3 - Change Color Scheme (Priority: P2)

**Goal**: User can change foreground, background, cursor, and selection colors via color picker

**Independent Test**: Open settings, click background color swatch, select dark blue, click Apply, verify background changes

### Tests for User Story 3

- [ ] T034 [P] [US3] Unit test: Color swatch buttons open ChooseColorW dialog in `tests/unit/test_settings_dialog.cpp`
- [ ] T035 [P] [US3] Unit test: Color values persist correctly (RGB round-trip) in `tests/unit/test_settings_dialog.cpp`
- [ ] T036 [P] [US3] Unit test: Cancel reverts to original colors in `tests/unit/test_settings_dialog.cpp`

### Implementation for User Story 3

- [X] T037 [US3] Add color swatch buttons for foreground, background, cursor, selection in Appearance page resource
- [X] T038 [US3] Implement ShowColorPicker() helper using ChooseColorW in `src/ui/SettingsDialog.cpp`
- [X] T039 [US3] Handle BN_CLICKED for each color button to open picker and update swatch preview
- [X] T040 [US3] Extend OnApply() to apply color changes via ApplySettings() in `src/ui/SettingsDialog.cpp`
- [X] T041 [US3] Store original settings for Cancel rollback in SettingsDialogState

**Checkpoint**: User Story 3 complete - color changes work

---

## Phase 6: User Story 4 - Configure Shell and Working Directory (Priority: P2)

**Goal**: User can set shell executable path and working directory, new tabs use these settings

**Independent Test**: Set shell to cmd.exe, click Apply, open new tab, verify cmd.exe launches

### Tests for User Story 4

- [ ] T042 [P] [US4] Unit test: Shell path validation shows warning for non-existent file in `tests/unit/test_settings_dialog.cpp`
- [ ] T043 [P] [US4] Unit test: Browse button opens IFileDialog with FOS_PICKFOLDERS in `tests/unit/test_settings_dialog.cpp`
- [ ] T044 [P] [US4] Unit test: Working directory persists and loads correctly in `tests/unit/test_settings_dialog.cpp`
- [ ] T044b [P] [US4] Unit test: Scrollback lines validation clamps to 100-100,000 range in `tests/unit/test_settings_dialog.cpp`

### Implementation for User Story 4

- [X] T045 [US4] Add shell path edit control and Browse button in General page resource
- [X] T046 [US4] Add working directory edit control and Browse button in General page resource
- [ ] T047 [US4] Implement ShowFolderPicker() helper using IFileDialog with FOS_PICKFOLDERS in `src/ui/SettingsDialog.cpp`
- [ ] T048 [US4] Implement ValidateShellPath() with warning icon for non-existent paths in `src/ui/SettingsDialog.cpp`
- [X] T049 [US4] Extend OnApply() to save shell and working directory settings
- [X] T049b [US4] Add scrollback lines numeric input (100-100,000) in General page resource

**Checkpoint**: User Story 4 complete - shell and General tab configuration works

---

## Phase 7: User Story 5 - Adjust Cursor Appearance (Priority: P3)

**Goal**: User can change cursor style (Block/Underline/Bar) and toggle blink

**Independent Test**: Select Bar cursor, uncheck blink, click Apply, verify cursor appearance changes

### Tests for User Story 5

- [ ] T050 [P] [US5] Unit test: Cursor style radio buttons update correctly in `tests/unit/test_settings_dialog.cpp`
- [ ] T051 [P] [US5] Unit test: Cursor blink checkbox persists in `tests/unit/test_settings_dialog.cpp`
- [ ] T052 [P] [US5] Unit test: Opacity slider clamps to 10-100% range in `tests/unit/test_settings_dialog.cpp`

### Implementation for User Story 5

- [X] T053 [US5] Add cursor style radio buttons (Block, Underline, Bar) in Terminal page resource
- [X] T054 [US5] Add cursor blink checkbox in Terminal page resource
- [X] T055 [US5] Add opacity slider (10%-100%) with value label in Terminal page resource
- [X] T057 [US5] Extend OnApply() to save cursor and opacity settings

**Checkpoint**: User Story 5 complete - cursor and terminal settings work

---

## Phase 8: User Story 6 - Reset to Defaults (Priority: P3)

**Goal**: User can reset all settings to defaults with confirmation prompt

**Independent Test**: Modify settings, click Reset to Defaults, confirm, verify all fields show defaults

### Tests for User Story 6

- [ ] T058 [P] [US6] Unit test: Reset to Defaults shows confirmation dialog in `tests/unit/test_settings_dialog.cpp`
- [ ] T059 [P] [US6] Unit test: Confirming reset loads all default values in `tests/unit/test_settings_dialog.cpp`
- [ ] T060 [P] [US6] Unit test: Reset then Cancel does not change persisted settings in `tests/unit/test_settings_dialog.cpp`

### Implementation for User Story 6

- [X] T061 [US6] Add Reset to Defaults button in dialog footer (PSH_WIZARD flag area or custom button)
- [X] T062 [US6] Implement ResetToDefaults() to load Config::GetDefaults() values in `src/ui/SettingsDialog.cpp`
- [X] T063 [US6] Show MessageBox confirmation before reset in `src/ui/SettingsDialog.cpp`
- [X] T064 [US6] Update all UI controls with default values after reset confirmation

**Checkpoint**: User Story 6 complete - reset to defaults works

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories

- [ ] T065 [P] Add debounced preview timer (150ms) for live settings preview in `src/ui/SettingsDialog.cpp`
- [ ] T066 [P] Add SetPreviewConfig() method to Application for temporary config display
- [ ] T067 Implement config.json read-only detection and show error in `src/ui/SettingsDialog.cpp`
- [ ] T067b [P] Unit test: Read-only config.json disables Apply button and shows error in `tests/unit/test_settings_dialog.cpp`
- [ ] T068 [P] Integration test: Dialog visual validation in `tests/integration/test_settings_visual.py`
- [X] T069 [P] Update `tests/manual_tests.md` with Settings Dialog test cases
- [ ] T070 [P] Update `docs/CONFIGURATION.md` with Settings Dialog documentation
- [X] T071 Update `CHANGELOG.md` with Settings Dialog entry
- [ ] T072 Run quickstart.md validation scenarios

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies - can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion - BLOCKS all user stories
- **User Stories (Phase 3-8)**: All depend on Foundational phase completion
  - US1 (P1) and US2 (P1) are both MVP priority
  - US3-US6 can proceed after US1 is complete
- **Polish (Phase 9)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational - No dependencies on other stories
- **User Story 2 (P1)**: Can start after Foundational - Uses FontEnumerator from Foundational
- **User Story 3 (P2)**: Can start after Foundational - Independent of other stories
- **User Story 4 (P2)**: Can start after Foundational - Independent of other stories
- **User Story 5 (P3)**: Can start after Foundational - Independent of other stories
- **User Story 6 (P3)**: Should be after US1-5 to test reset of all settings types

### Within Each User Story

- Tests MUST be written and FAIL before implementation
- UI resources before logic
- Dialog procedures before Apply handler
- Core implementation before integration

### Parallel Opportunities

- T002, T003, T004 (dialog resources) can run in parallel
- T007, T009, T010, T014, T015 (foundational files) can run in parallel
- T016, T017, T018 (US1 tests) can run in parallel
- All test tasks marked [P] within a story can run in parallel
- Once Foundational completes, US1-US5 can theoretically run in parallel

---

## Parallel Example: User Story 2

```bash
# Launch all tests for User Story 2 together:
Task: T026 "Unit test: Font dropdown populated with only monospace fonts"
Task: T027 "Unit test: Font size validation (6-72 range)"
Task: T028 "Unit test: Apply button triggers font reload"

# Then implement sequentially:
Task: T029 "Populate font dropdown" (depends on FontEnumerator)
Task: T030 "Add font size numeric input"
Task: T031 "Implement OnApply() handler"
Task: T032 "Add ReloadFont() method"
Task: T033 "Implement grid recalculation"
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2)

1. Complete Phase 1: Setup (resources)
2. Complete Phase 2: Foundational (FontEnumerator, dialog infrastructure)
3. Complete Phase 3: User Story 1 (open/close dialog)
4. Complete Phase 4: User Story 2 (font settings)
5. **STOP and VALIDATE**: Test dialog opens, font changes work
6. Deploy/demo if ready - this is functional MVP

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. Add US1 → Dialog opens/closes → Minimal viable UI
3. Add US2 → Font settings work → **MVP Complete!**
4. Add US3 → Color settings work → Visual customization
5. Add US4 → Shell config works → Power user features
6. Add US5 → Cursor/opacity work → Full appearance control
7. Add US6 → Reset works → Safety net feature
8. Each story adds value without breaking previous stories

---

## Summary

| Phase | Story | Tasks | Description |
|-------|-------|-------|-------------|
| 1 | - | 6 | Setup (resources) |
| 2 | - | 10 | Foundational (infrastructure) |
| 3 | US1 | 10 | Open/Close Dialog |
| 4 | US2 | 9 | Font Settings |
| 5 | US3 | 8 | Color Scheme |
| 6 | US4 | 10 | Shell/Working Dir/Scrollback |
| 7 | US5 | 7 | Cursor/Opacity |
| 8 | US6 | 7 | Reset to Defaults |
| 9 | - | 9 | Polish |
| **Total** | | **76** | |

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- Verify tests fail before implementing
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
