# Data Model: Settings Dialog

**Feature**: 006-settings-dialog
**Date**: 2025-12-28

## Entities

### SettingsDialogState

Temporary state held while the dialog is open. Discarded on Cancel, persisted on Apply.

| Field | Type | Validation | Source |
|-------|------|------------|--------|
| shellPath | wstring | File exists (warning only) | TerminalConfig.shell |
| workingDirectory | wstring | Directory exists (warning only) | TerminalConfig.workingDirectory |
| scrollbackLines | int | 100-100,000 | TerminalConfig.scrollbackLines |
| fontFamily | wstring | Must be in enumerated list | FontConfig.family |
| fontSize | int | 6-72 | FontConfig.size |
| foregroundColor | Color | Valid RGB | ColorConfig.foreground |
| backgroundColor | Color | Valid RGB | ColorConfig.background |
| cursorColor | Color | Valid RGB | ColorConfig.cursor |
| selectionColor | Color | Valid RGBA | ColorConfig.selection |
| cursorStyle | CursorStyle | Block/Underline/Bar | TerminalConfig.cursorStyle |
| cursorBlink | bool | - | TerminalConfig.cursorBlink |
| opacity | float | 0.1-1.0 | TerminalConfig.opacity |

### FontInfo

Represents an enumerated system font.

| Field | Type | Description |
|-------|------|-------------|
| familyName | wstring | Font family name (e.g., "Consolas") |
| isMonospace | bool | True if glyph widths are uniform |

### DialogPage

Logical tab pages in the settings dialog.

| Value | Description | Controls |
|-------|-------------|----------|
| General | Shell and terminal behavior | Shell path, working dir, scrollback |
| Appearance | Visual settings | Font dropdown, font size, colors |
| Terminal | Cursor and window | Cursor style, blink, opacity |

## State Transitions

```
┌─────────────────────────────────────────────────────────────────────┐
│                                                                     │
│  [Dialog Closed]                                                    │
│       │                                                             │
│       │ Ctrl+Comma                                                  │
│       ▼                                                             │
│  [Loading] ──────────────────────────────────────────────────────┐  │
│       │                                                          │  │
│       │ Load current config, enumerate fonts                     │  │
│       ▼                                                          │  │
│  [Editing] ◄─────────────────────────────────────────────────┐   │  │
│       │                                                      │   │  │
│       ├── User changes setting ─► [Preview Pending]          │   │  │
│       │                               │                      │   │  │
│       │                               │ 150ms timer          │   │  │
│       │                               ▼                      │   │  │
│       │                          [Previewing] ───────────────┘   │  │
│       │                                                          │  │
│       ├── Apply ─────────────────────────────────────────────────┤  │
│       │       │                                                  │  │
│       │       │ Save to config.json                              │  │
│       │       ▼                                                  │  │
│       │  [Dialog Closed]                                         │  │
│       │                                                          │  │
│       ├── Cancel / Escape ───────────────────────────────────────┤  │
│       │       │                                                  │  │
│       │       │ Restore original config                          │  │
│       │       ▼                                                  │  │
│       │  [Dialog Closed]                                         │  │
│       │                                                          │  │
│       └── Reset to Defaults ─────────────────────────────────────┘  │
│               │                                                     │
│               │ Confirm? → Load defaults → Back to [Editing]        │
│               │                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## Relationships

```
Config (existing)
├── FontConfig
│   ├── family ←──────── SettingsDialogState.fontFamily
│   └── size ←────────── SettingsDialogState.fontSize
├── ColorConfig
│   ├── foreground ←──── SettingsDialogState.foregroundColor
│   ├── background ←──── SettingsDialogState.backgroundColor
│   ├── cursor ←──────── SettingsDialogState.cursorColor
│   └── selection ←───── SettingsDialogState.selectionColor
└── TerminalConfig
    ├── shell ←───────── SettingsDialogState.shellPath
    ├── workingDirectory ← SettingsDialogState.workingDirectory
    ├── scrollbackLines ← SettingsDialogState.scrollbackLines
    ├── cursorStyle ←──── SettingsDialogState.cursorStyle
    ├── cursorBlink ←──── SettingsDialogState.cursorBlink
    └── opacity ←──────── SettingsDialogState.opacity

FontEnumerator
└── EnumerateMonospaceFonts() → vector<FontInfo>
```

## Validation Rules

| Field | Rule | Error Handling |
|-------|------|----------------|
| shellPath | Check file exists | Warning icon, proceed anyway |
| workingDirectory | Check directory exists | Warning icon, proceed anyway |
| scrollbackLines | Clamp to 100-100,000 | Auto-clamp with tooltip |
| fontSize | Clamp to 6-72 | Auto-clamp with tooltip |
| opacity | Clamp to 10%-100% | Slider enforces range |
| fontFamily | Must match enumerated font | Fallback to Consolas |
