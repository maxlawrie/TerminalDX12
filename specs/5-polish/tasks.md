# Tasks: Polish & Refinement

**Input**: Design documents from `/specs/5-polish/`
**Prerequisites**: plan.md, spec.md
**Task ID Scope**: `5-Txxx` (IDs are scoped to this phase; use prefix when cross-referencing)

## Format: `[ID] [P?] [Story] Description @agent`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (US1=Settings, US2=Ligatures, US4=Multi-window, US5=Transparency)
- **@agent**: Recommended agent for this task

---

## Phase 1: User Story 5 - Window Transparency (Priority: P3) 🎯 Quick Win

**Goal**: Enable semi-transparent terminal window background

**Independent Test**: Set opacity to 0.8, verify desktop visible through terminal

### Implementation for User Story 5

- [ ] T001 [US5] Research DWM transparency APIs (DwmExtendFrameIntoClientArea vs SetLayeredWindowAttributes) @cpp-pro
- [ ] T002 [US5] Modify Window::Create() to enable WS_EX_LAYERED style @cpp-pro
- [ ] T003 [US5] Add Window::SetOpacity(float) method using SetLayeredWindowAttributes @cpp-pro
- [ ] T004 [US5] Call SetOpacity() from Application::Initialize() using config value @cpp-pro
- [ ] T005 [US5] Modify swap chain to use DXGI_FORMAT with alpha if needed @cpp-pro
- [ ] T006 [US5] Update background clear color to use alpha from config @cpp-pro
- [ ] T007 [US5] Test with various opacity values (0.5, 0.8, 1.0) @test-automator
- [ ] T008 [P] [US5] Add opacity slider to future settings dialog (placeholder) @cpp-pro

**Checkpoint**: Terminal window is semi-transparent based on config

---

## Phase 1b: Window Modes (Priority: P3)

**Goal**: Support always-on-top and borderless/fullscreen modes

**Independent Test**: Toggle always-on-top, verify window stays above others

### Implementation for Window Modes

- [ ] T071 [P] [US5] Add Window::SetAlwaysOnTop(bool) using SetWindowPos with HWND_TOPMOST @cpp-pro
- [ ] T072 [P] [US5] Add Window::SetBorderless(bool) toggling WS_OVERLAPPEDWINDOW style @cpp-pro
- [ ] T073 [US5] Add Window::ToggleFullscreen() using monitor dimensions @cpp-pro
- [ ] T074 [US5] Add config fields: window.alwaysOnTop, window.borderless @cpp-pro
- [ ] T075 [US5] Add window mode options to Terminal tab in settings dialog (after T028) @cpp-pro

**Checkpoint**: Window modes configurable and functional

---

## Phase 2: User Story 1 - Settings Dialog (Priority: P1)

**Goal**: GUI for editing terminal settings without manual JSON editing

**Independent Test**: Press Ctrl+Comma, change font size, click Apply, verify change takes effect

### Tests for User Story 1

- [ ] T009 [P] [US1] Unit test for settings round-trip (load → modify → save → reload) @test-automator
- [ ] T010 [P] [US1] Test that invalid settings show validation error @test-automator

### Implementation for User Story 1

#### Dialog Infrastructure
- [ ] T011 [US1] Create include/ui/SettingsDialog.h with class declaration @cpp-pro
- [ ] T012 [US1] Create src/ui/SettingsDialog.cpp with stub implementation @cpp-pro
- [ ] T013 [US1] Add SettingsDialog to CMakeLists.txt @build-engineer
- [ ] T014 [US1] Add Ctrl+Comma handler in Application to open dialog @cpp-pro

#### General Tab
- [ ] T015 [US1] Create dialog window with tab control (Win32 CreateDialogParam) @cpp-pro
- [ ] T016 [US1] Add "General" tab with shell path edit box @cpp-pro
- [ ] T017 [US1] Add working directory edit box with Browse button @cpp-pro
- [ ] T018 [US1] Add scrollback lines numeric input @cpp-pro

#### Appearance Tab
- [ ] T019 [US1] Add "Appearance" tab @cpp-pro
- [ ] T020 [US1] Add font family dropdown (enumerate system fonts) @cpp-pro
- [ ] T021 [US1] Add font size numeric input @cpp-pro
- [ ] T022 [US1] Add foreground color button with color picker dialog @cpp-pro
- [ ] T023 [US1] Add background color button with color picker dialog @cpp-pro
- [ ] T024 [US1] Implement live preview of font/color changes @cpp-pro

#### Terminal Tab
- [ ] T025 [US1] Add "Terminal" tab @cpp-pro
- [ ] T026 [US1] Add cursor style radio buttons (Block/Underline/Bar) @cpp-pro
- [ ] T027 [US1] Add cursor blink checkbox @cpp-pro
- [ ] T028 [US1] Add opacity slider (0-100%) @cpp-pro

#### Dialog Actions
- [ ] T029 [US1] Implement Apply button - save config, apply changes live @cpp-pro
- [ ] T030 [US1] Implement Cancel button - discard changes, close dialog @cpp-pro
- [ ] T031 [US1] Implement Reset to Defaults button @cpp-pro
- [ ] T032 [US1] Add Config::ApplyLive() method for runtime config changes @cpp-pro
- [ ] T033 [US1] Handle dialog close (X button) same as Cancel @cpp-pro

**Checkpoint**: Settings dialog opens, changes persist after Apply

---

## Phase 3: User Story 2 - Font Ligatures (Priority: P2)

**Goal**: Render programming ligatures (=>, ->, !=) using HarfBuzz

**Independent Test**: Type "=>" in terminal with Fira Code font, verify arrow ligature renders

### Setup

- [ ] T034 [US2] Add "harfbuzz" to vcpkg.json dependencies @build-engineer
- [ ] T035 [US2] Run vcpkg install to fetch HarfBuzz @build-engineer
- [ ] T036 [US2] Update CMakeLists.txt to find and link HarfBuzz @build-engineer

### HarfBuzz Wrapper

- [ ] T037 [US2] Create include/rendering/HarfBuzzShaper.h @cpp-pro
- [ ] T038 [US2] Implement HarfBuzzShaper constructor (load font with hb_font_create) @cpp-pro
- [ ] T039 [US2] Implement Shape(text) method returning glyph IDs and positions @cpp-pro
- [ ] T040 [US2] Enable ligature features (calt, liga, clig) via hb_feature_t @cpp-pro
- [ ] T041 [P] [US2] Unit test for HarfBuzz shaping basic cases @test-automator

### Rendering Integration

- [ ] T042 [US2] Modify GlyphAtlas to accept HarfBuzz glyph IDs @cpp-pro
- [ ] T043 [US2] Modify TextRenderer to use shaped glyph runs @cpp-pro
- [ ] T044 [US2] Add shaping cache for visible lines (avoid re-shaping static content) @cpp-pro
- [ ] T045 [US2] Implement bypass when config.font.ligatures is false @cpp-pro
- [ ] T046 [US2] Profile rendering performance before/after HarfBuzz @performance-engineer

### Testing

- [ ] T047 [P] [US2] Test common ligatures: => -> != == === !== <= >= @test-automator
- [ ] T048 [P] [US2] Test with fonts: Fira Code, JetBrains Mono, Cascadia Code @test-automator
- [ ] T049 [US2] Test fallback when font has no ligatures @test-automator

**Checkpoint**: Ligatures render correctly with compatible fonts

---

## Phase 4: User Story 4 - Multi-Window Support (Priority: P3)

**Goal**: Open multiple independent terminal windows

**Independent Test**: Press Ctrl+Shift+N, verify new window opens with independent shell

### Architecture Refactor

- [ ] T050 [US4] Create include/core/TerminalWindow.h - extract per-window state from Application @refactoring-specialist
- [ ] T051 [US4] Move m_window, m_renderer, m_tabManager to TerminalWindow class @refactoring-specialist
- [ ] T052 [US4] Create src/core/TerminalWindow.cpp with window lifecycle @cpp-pro
- [ ] T053 [US4] Refactor Application to manage vector of TerminalWindow instances @refactoring-specialist
- [ ] T054 [US4] Move message loop to Application, dispatch to correct window @cpp-pro

### Window Management

- [ ] T055 [US4] Add Ctrl+Shift+N handler to create new TerminalWindow @cpp-pro
- [ ] T056 [US4] Implement TerminalWindow::Close() - cleanup resources @cpp-pro
- [ ] T057 [US4] Track window count, exit Application when last window closes @cpp-pro
- [ ] T058 [US4] Each window gets independent TabManager and shell sessions @cpp-pro

### Resource Sharing

- [ ] T059 [US4] Share GlyphAtlas across windows (singleton or shared_ptr) @cpp-pro
- [ ] T060 [US4] Share Config across windows (read-only after startup) @cpp-pro
- [ ] T061 [US4] Ensure DX12 device is shared, swap chains are per-window @cpp-pro

### Testing

- [ ] T062 [P] [US4] Test opening 3 windows, each with different shell @test-automator
- [ ] T063 [P] [US4] Test closing windows in various orders @test-automator
- [ ] T064 [US4] Test config changes apply to all windows @test-automator

**Checkpoint**: Multiple independent terminal windows work correctly

---

## Phase 5: Polish & Documentation

- [ ] T065 [P] Update SPECIFICATION.md with Phase 5 feature statuses @documentation-engineer
- [ ] T066 [P] Update CHANGELOG.md with Phase 5 features @documentation-engineer
- [ ] T067 [P] Document settings dialog usage in README or docs/ @documentation-engineer
- [ ] T068 [P] Document ligature font requirements @documentation-engineer
- [ ] T069 Run full test suite, fix any regressions @test-automator
- [ ] T070 Update spec.md status to Complete @documentation-engineer

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
T037-T041 (HarfBuzz wrapper) then T042-T046 (Integration) @test-automator @performance-engineer
```

---

## Agent Assignments Summary

| Agent | Task Count | Responsibility |
|-------|------------|----------------|
| @cpp-pro | 51 | Core C++ implementation, Win32 APIs, DirectX 12, HarfBuzz |
| @test-automator | 11 | Unit tests, integration tests, regression testing |
| @build-engineer | 4 | CMakeLists.txt, vcpkg dependencies, build configuration |
| @refactoring-specialist | 3 | Architecture refactor for multi-window support |
| @performance-engineer | 1 | HarfBuzz performance profiling |
| @documentation-engineer | 5 | SPECIFICATION.md, CHANGELOG.md, user docs |

---

## Task Count Summary

| Phase | Tasks | Priority | Complexity |
|-------|-------|----------|------------|
| US5: Transparency | 8 | P3 | Low |
| US5: Window Modes | 5 | P3 | Low |
| US1: Settings Dialog | 25 | P1 | Medium |
| US2: Ligatures | 16 | P2 | High |
| US4: Multi-window | 15 | P3 | High |
| Polish | 6 | - | Low |
| **Total** | **75** | | |

---

## Notes

- OSC 133 Shell Integration is already complete (implemented in Phase 1)
- Settings Dialog is the highest-value feature for end users
- Ligatures require HarfBuzz which adds ~2MB to binary size
- Multi-window is optional - many users are satisfied with tabs
- Transparency may have GPU compatibility issues on older hardware
