# Quickstart: Settings Dialog

**Feature**: 006-settings-dialog
**Date**: 2025-12-28

## Prerequisites

- Windows 10 1809+ (64-bit)
- Visual Studio 2022 with C++ Desktop workload
- Existing TerminalDX12 build environment

## Quick Test Scenarios

### Scenario 1: Open Settings Dialog

```
1. Launch TerminalDX12.exe
2. Press Ctrl+Comma
3. Verify: Modal dialog appears with 3 tabs (General, Appearance, Terminal)
4. Press Escape
5. Verify: Dialog closes, no changes applied
```

### Scenario 2: Change Font

```
1. Launch TerminalDX12.exe
2. Press Ctrl+Comma
3. Click "Appearance" tab
4. Select "Cascadia Mono" from font dropdown
5. Change font size to 14
6. Click Apply
7. Verify: Terminal text immediately changes to Cascadia Mono at 14pt
8. Close and reopen terminal
9. Verify: Font setting persists
```

### Scenario 3: Change Colors

```
1. Open Settings (Ctrl+Comma)
2. Go to Appearance tab
3. Click the "Background" color swatch
4. Select a dark blue color (#1a1a2e)
5. Click OK in color picker
6. Click Apply
7. Verify: Terminal background changes to dark blue
```

### Scenario 4: Configure Shell

```
1. Open Settings (Ctrl+Comma)
2. On General tab, change Shell to "cmd.exe"
3. Click Apply
4. Open a new tab (Ctrl+T)
5. Verify: New tab runs cmd.exe instead of PowerShell
```

### Scenario 5: Reset to Defaults

```
1. Modify several settings (font, colors, shell)
2. Click "Reset to Defaults"
3. Confirm the reset prompt
4. Verify: All settings show default values
5. Click Cancel
6. Verify: Terminal still has previous (modified) settings
7. Open Settings again, Reset to Defaults, click Apply
8. Verify: Terminal now shows default settings
```

## Key Files

| File | Purpose |
|------|---------|
| `src/ui/SettingsDialog.h` | Dialog class declaration |
| `src/ui/SettingsDialog.cpp` | Dialog implementation |
| `src/ui/FontEnumerator.h` | Monospace font enumeration |
| `src/core/Config.h` | Settings data structures (existing) |
| `%APPDATA%\TerminalDX12\config.json` | Persisted settings |

## Build & Test

```powershell
# Build
cmake --build build --config Release

# Run unit tests
.\build\tests\Release\TerminalDX12Tests.exe --gtest_filter="SettingsDialog*"

# Run integration tests
python -m pytest tests/integration/test_settings_visual.py -v

# Manual test
.\build\bin\Release\TerminalDX12.exe
# Press Ctrl+Comma to open settings
```

## Common Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Dialog doesn't open | Ctrl+Comma not handled | Check WM_KEYDOWN handler in Application.cpp |
| Fonts not listed | DirectWrite init failed | Ensure dwrite.lib is linked |
| Colors not saving | JSON write error | Check %APPDATA%\TerminalDX12 permissions |
| Preview not updating | Timer not firing | Check WM_TIMER handling |
