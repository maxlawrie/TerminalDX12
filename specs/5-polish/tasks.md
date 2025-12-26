# Tasks: Polish & Refinement

**Input**: Design documents from `/specs/5-polish/`
**Prerequisites**: plan.md, spec.md

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1=Settings, US2=Ligatures, US4=Multi-window, US5=Transparency)

---

## Phase 1: User Story 5 - Window Transparency (Priority: P3) 🎯 Quick Win

**Goal**: Enable semi-transparent terminal window background

**Independent Test**: Set opacity to 0.8, verify desktop visible through terminal

### Implementation for User Story 5

- [ ] T001 [US5] Research DWM transparency APIs (DwmExtendFrameIntoClientArea vs SetLayeredWindowAttributes)
- [ ] T002 [US5] Modify Window::Create() to enable WS_EX_LAYERED style
- [ ] T003 [US5] Add Window::SetOpacity(float) method using SetLayeredWindowAttributes
- [ ] T004 [US5] Call SetOpacity() from Application::Initialize() using config value
- [ ] T005 [US5] Modify swap chain to use DXGI_FORMAT with alpha if needed
- [ ] T006 [US5] Update background clear color to use alpha from config
- [ ] T007 [US5] Test with various opacity values (0.5, 0.8, 1.0)
- [ ] T008 [P] [US5] Add opacity slider to future settings dialog (placeholder)

**Checkpoint**: Terminal window is semi-transparent based on config

---

## Phase 2: User Story 1 - Settings Dialog (Priority: P1)

**Goal**: GUI for editing terminal settings without manual JSON editing

**Independent Test**: Press Ctrl+Comma, change font size, click Apply, verify change takes effect

### Tests for User Story 1

- [ ] T009 [P] [US1] Unit test for settings round-trip (load → modify → save → reload)
- [ ] T010 [P] [US1] Test that invalid settings show validation error

### Implementation for User Story 1

#### Dialog Infrastructure
- [ ] T011 [US1] Create include/ui/SettingsDialog.h with class declaration
- [ ] T012 [US1] Create src/ui/SettingsDialog.cpp with stub implementation
- [ ] T013 [US1] Add SettingsDialog to CMakeLists.txt
- [ ] T014 [US1] Add Ctrl+Comma handler in Application to open dialog

#### General Tab
- [ ] T015 [US1] Create dialog window with tab control (Win32 CreateDialogParam)
- [ ] T016 [US1] Add "General" tab with shell path edit box
- [ ] T017 [US1] Add working directory edit box with Browse button
- [ ] T018 [US1] Add scrollback lines numeric input

#### Appearance Tab
- [ ] T019 [US1] Add "Appearance" tab
- [ ] T020 [US1] Add font family dropdown (enumerate system fonts)
- [ ] T021 [US1] Add font size numeric input
- [ ] T022 [US1] Add foreground color button with color picker dialog
- [ ] T023 [US1] Add background color button with color picker dialog
- [ ] T024 [US1] Implement live preview of font/color changes

#### Terminal Tab
- [ ] T025 [US1] Add "Terminal" tab
- [ ] T026 [US1] Add cursor style radio buttons (Block/Underline/Bar)
- [ ] T027 [US1] Add cursor blink checkbox
- [ ] T028 [US1] Add opacity slider (0-100%)

#### Dialog Actions
- [ ] T029 [US1] Implement Apply button - save config, apply changes live
- [ ] T030 [US1] Implement Cancel button - discard changes, close dialog
- [ ] T031 [US1] Implement Reset to Defaults button
- [ ] T032 [US1] Add Config::ApplyLive() method for runtime config changes
- [ ] T033 [US1] Handle dialog close (X button) same as Cancel

**Checkpoint**: Settings dialog opens, changes persist after Apply

---

## Phase 3: User Story 2 - Font Ligatures (Priority: P2)

**Goal**: Render programming ligatures (=>, ->, !=) using HarfBuzz

**Independent Test**: Type "=>" in terminal with Fira Code font, verify arrow ligature renders

### Setup

- [ ] T034 [US2] Add "harfbuzz" to vcpkg.json dependencies
- [ ] T035 [US2] Run vcpkg install to fetch HarfBuzz
- [ ] T036 [US2] Update CMakeLists.txt to find and link HarfBuzz

### HarfBuzz Wrapper

- [ ] T037 [US2] Create include/rendering/HarfBuzzShaper.h
- [ ] T038 [US2] Implement HarfBuzzShaper constructor (load font with hb_font_create)
- [ ] T039 [US2] Implement Shape(text) method returning glyph IDs and positions
- [ ] T040 [US2] Enable ligature features (calt, liga, clig) via hb_feature_t
- [ ] T041 [P] [US2] Unit test for HarfBuzz shaping basic cases

### Rendering Integration

- [ ] T042 [US2] Modify GlyphAtlas to accept HarfBuzz glyph IDs
- [ ] T043 [US2] Modify TextRenderer to use shaped glyph runs
- [ ] T044 [US2] Add shaping cache for visible lines (avoid re-shaping static content)
- [ ] T045 [US2] Implement bypass when config.font.ligatures is false
- [ ] T046 [US2] Profile rendering performance before/after HarfBuzz

### Testing

- [ ] T047 [P] [US2] Test common ligatures: => -> != == === !== <= >=
- [ ] T048 [P] [US2] Test with fonts: Fira Code, JetBrains Mono, Cascadia Code
- [ ] T049 [US2] Test fallback when font has no ligatures

**Checkpoint**: Ligatures render correctly with compatible fonts

---

## Phase 4: User Story 4 - Multi-Window Support (Priority: P3)

**Goal**: Open multiple independent terminal windows

**Independent Test**: Press Ctrl+Shift+N, verify new window opens with independent shell

### Architecture Refactor

- [ ] T050 [US4] Create include/core/TerminalWindow.h - extract per-window state from Application
- [ ] T051 [US4] Move m_window, m_renderer, m_tabManager to TerminalWindow class
- [ ] T052 [US4] Create src/core/TerminalWindow.cpp with window lifecycle
- [ ] T053 [US4] Refactor Application to manage vector of TerminalWindow instances
- [ ] T054 [US4] Move message loop to Application, dispatch to correct window

### Window Management

- [ ] T055 [US4] Add Ctrl+Shift+N handler to create new TerminalWindow
- [ ] T056 [US4] Implement TerminalWindow::Close() - cleanup resources
- [ ] T057 [US4] Track window count, exit Application when last window closes
- [ ] T058 [US4] Each window gets independent TabManager and shell sessions

### Resource Sharing

- [ ] T059 [US4] Share GlyphAtlas across windows (singleton or shared_ptr)
- [ ] T060 [US4] Share Config across windows (read-only after startup)
- [ ] T061 [US4] Ensure DX12 device is shared, swap chains are per-window

### Testing

- [ ] T062 [P] [US4] Test opening 3 windows, each with different shell
- [ ] T063 [P] [US4] Test closing windows in various orders
- [ ] T064 [US4] Test config changes apply to all windows

**Checkpoint**: Multiple independent terminal windows work correctly

---

## Phase 5: Polish & Documentation

- [ ] T065 [P] Update SPECIFICATION.md with Phase 5 feature statuses
- [ ] T066 [P] Update CHANGELOG.md with Phase 5 features
- [ ] T067 [P] Document settings dialog usage in README or docs/
- [ ] T068 [P] Document ligature font requirements
- [ ] T069 Run full test suite, fix any regressions
- [ ] T070 Update spec.md status to Complete

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Transparency)**: No dependencies - start here (quick win)
- **Phase 2 (Settings)**: No dependencies - can parallel with Phase 1
- **Phase 3 (Ligatures)**: Requires HarfBuzz setup (T034-T036) first
- **Phase 4 (Multi-window)**: Largest refactor, do last
- **Phase 5 (Polish)**: After all features complete

### Recommended Execution Order

```text
Week 1: Transparency (T001-T008) - Quick win, builds confidence
Week 2-3: Settings Dialog (T009-T033) - High user value
Week 4-5: Ligatures (T034-T049) - Complex but contained
Week 6+: Multi-window (T050-T064) - Only if needed
```

### Parallel Opportunities

```text
# Can run simultaneously:
T001-T008 (Transparency) || T009-T033 (Settings Dialog)

# Within Settings Dialog:
T015-T018 (General tab) || T019-T024 (Appearance tab) || T025-T028 (Terminal tab)

# Within Ligatures:
T037-T041 (HarfBuzz wrapper) then T042-T046 (Integration)
```

---

## Task Count Summary

| Phase | Tasks | Priority | Complexity |
|-------|-------|----------|------------|
| US5: Transparency | 8 | P3 | Low |
| US1: Settings Dialog | 25 | P1 | Medium |
| US2: Ligatures | 16 | P2 | High |
| US4: Multi-window | 15 | P3 | High |
| Polish | 6 | - | Low |
| **Total** | **70** | | |

---

## Notes

- OSC 133 Shell Integration is already complete (implemented in Phase 1)
- Settings Dialog is the highest-value feature for end users
- Ligatures require HarfBuzz which adds ~2MB to binary size
- Multi-window is optional - many users are satisfied with tabs
- Transparency may have GPU compatibility issues on older hardware
