# Implementation Plan: Settings Dialog

**Branch**: `006-settings-dialog` | **Date**: 2025-12-28 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/006-settings-dialog/spec.md`

## Summary

Implement a GUI settings dialog (Ctrl+Comma) with tabbed interface for configuring terminal settings. The dialog uses Win32 controls for General (shell, working directory, scrollback), Appearance (font, colors), and Terminal (cursor, opacity) settings. Changes are applied immediately and persisted to config.json.

## Technical Context

**Language/Version**: C++20 (MSVC)
**Primary Dependencies**: Win32 API (dialog, common controls), DirectWrite (font enumeration), existing Config class
**Storage**: config.json (existing JSON format via nlohmann/json)
**Testing**: GoogleTest (unit tests), Python (integration tests)
**Target Platform**: Windows 10 1809+ (64-bit)
**Project Type**: Single desktop application
**Performance Goals**: Dialog opens in <1 second, settings apply in <500ms
**Constraints**: Must integrate with existing DX12 render loop without blocking
**Scale/Scope**: Single modal dialog, ~17 configurable settings

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. GPU-First Rendering | N/A | Dialog uses Win32 controls, not DX12 rendering |
| II. VT/ANSI Compliance | N/A | No VT sequences involved |
| III. Test-First Development | MUST COMPLY | Unit tests for settings round-trip, integration tests for dialog behavior |
| IV. Windows Native Architecture | COMPLIANT | Uses Win32 dialogs, DirectWrite, Windows common controls |
| V. Unicode & Internationalization | COMPLIANT | Font enumeration supports Unicode font names |
| Quality Gates | MUST COMPLY | Tests must pass, no warnings at /W4 |
| Performance Requirements | COMPLIANT | Dialog is modal, no impact on 60 FPS rendering |

**Gate Status**: PASS - All applicable principles satisfied

## Project Structure

### Documentation (this feature)

```text
specs/006-settings-dialog/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
src/
├── core/
│   ├── Config.h/.cpp        # Existing - add ApplySettings() method
│   └── Application.cpp      # Add Ctrl+Comma handler, ShowSettingsDialog()
├── ui/
│   ├── SettingsDialog.h     # NEW - Dialog class declaration
│   ├── SettingsDialog.cpp   # NEW - Dialog implementation
│   └── FontEnumerator.h/.cpp # NEW - Monospace font enumeration
└── rendering/
    └── TextRenderer.cpp     # Existing - add ReloadFont() method

tests/
├── unit/
│   ├── test_settings_dialog.cpp  # NEW - Settings state tests
│   └── test_font_enumerator.cpp  # NEW - Font enumeration tests
└── integration/
    └── test_settings_visual.py   # NEW - Dialog visual tests
```

**Structure Decision**: Follows existing module boundaries. SettingsDialog goes in `ui/` per constitution. FontEnumerator is a new utility class for DirectWrite font enumeration.

## Complexity Tracking

No constitution violations requiring justification. The feature follows existing patterns and module boundaries.
