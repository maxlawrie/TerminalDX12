# Configuration Guide

TerminalDX12 uses a JSON configuration file for customization. This guide documents all available options.

## Configuration File Location

The configuration file is located at:

```
%APPDATA%\TerminalDX12\config.json
```

If the file doesn't exist, TerminalDX12 creates a default configuration on first launch.

## Example Configuration

```json
{
  "font": {
    "family": "Cascadia Code",
    "size": 14,
    "weight": 400,
    "ligatures": true
  },
  "colors": {
    "foreground": "#cccccc",
    "background": "#0c0c0c",
    "cursor": "#00ff00",
    "selection": "#3399ff80",
    "palette": [
      "#0c0c0c", "#c50f1f", "#13a10e", "#c19c00",
      "#0037da", "#881798", "#3a96dd", "#cccccc",
      "#767676", "#e74856", "#16c60c", "#f9f1a5",
      "#3b78ff", "#b4009e", "#61d6d6", "#f2f2f2"
    ]
  },
  "terminal": {
    "shell": "powershell.exe",
    "workingDirectory": "",
    "scrollbackLines": 10000,
    "cursorStyle": "block",
    "cursorBlink": true,
    "cursorBlinkMs": 530,
    "opacity": 1.0,
    "osc52Policy": "writeOnly"
  },
  "keybindings": {
    "copy": "ctrl+c",
    "paste": "ctrl+v",
    "scrollUp": "shift+pageup",
    "scrollDown": "shift+pagedown",
    "newTab": "ctrl+t",
    "closeTab": "ctrl+w",
    "search": "ctrl+f"
  }
}
```

## Configuration Sections

### Font

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `family` | string | `"Consolas"` | Font family name. Monospace fonts recommended. |
| `size` | int | `16` | Font size in pixels. |
| `weight` | int | `400` | Font weight (400 = normal, 700 = bold). |
| `ligatures` | bool | `false` | Enable programming ligatures (if font supports). |

**Recommended fonts:**
- Cascadia Code (with ligatures)
- JetBrains Mono
- Fira Code
- Consolas (Windows default)

### Colors

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `foreground` | hex | `#cccccc` | Default text color. |
| `background` | hex | `#0c0c0c` | Terminal background color. |
| `cursor` | hex | `#00ff00` | Cursor color. |
| `selection` | hex | `#3399ff80` | Selection highlight (supports alpha). |
| `palette` | array[16] | (see below) | 16-color ANSI palette. |

**Color format:** `#RRGGBB` or `#RRGGBBAA` (with alpha)

**Default palette (Windows Terminal colors):**

| Index | Color | Hex |
|-------|-------|-----|
| 0 | Black | `#0c0c0c` |
| 1 | Red | `#c50f1f` |
| 2 | Green | `#13a10e` |
| 3 | Yellow | `#c19c00` |
| 4 | Blue | `#0037da` |
| 5 | Magenta | `#881798` |
| 6 | Cyan | `#3a96dd` |
| 7 | White | `#cccccc` |
| 8 | Bright Black | `#767676` |
| 9 | Bright Red | `#e74856` |
| 10 | Bright Green | `#16c60c` |
| 11 | Bright Yellow | `#f9f1a5` |
| 12 | Bright Blue | `#3b78ff` |
| 13 | Bright Magenta | `#b4009e` |
| 14 | Bright Cyan | `#61d6d6` |
| 15 | Bright White | `#f2f2f2` |

### Terminal

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `shell` | string | `"powershell.exe"` | Shell executable to launch. |
| `workingDirectory` | string | `""` | Starting directory (empty = current). |
| `scrollbackLines` | int | `10000` | Number of lines to keep in history. |
| `cursorStyle` | string | `"block"` | Cursor shape: `block`, `underline`, or `bar`. |
| `cursorBlink` | bool | `true` | Enable cursor blinking. |
| `cursorBlinkMs` | int | `530` | Blink interval in milliseconds. |
| `opacity` | float | `1.0` | Window transparency (0.0-1.0). |
| `osc52Policy` | string | `"writeOnly"` | Clipboard access policy (see below). |

**Shell examples:**
- `"powershell.exe"` - Windows PowerShell
- `"pwsh.exe"` - PowerShell Core
- `"cmd.exe"` - Command Prompt
- `"wsl.exe"` - Windows Subsystem for Linux
- `"wsl.exe -d Ubuntu"` - Specific WSL distro

**OSC 52 clipboard policies:**

| Policy | Read | Write | Use Case |
|--------|------|-------|----------|
| `disabled` | No | No | Maximum security, no clipboard access. |
| `readOnly` | Yes | No | Allow programs to read clipboard (security risk). |
| `writeOnly` | No | Yes | Allow programs to set clipboard (default, recommended). |
| `readWrite` | Yes | Yes | Full access (use with caution). |

### Keybindings

Define keyboard shortcuts using the format: `[modifiers+]key`

**Modifiers:** `ctrl`, `shift`, `alt`

**Available actions:**

| Action | Default | Description |
|--------|---------|-------------|
| `copy` | `ctrl+c` | Copy selection (or send SIGINT if nothing selected). |
| `paste` | `ctrl+v` | Paste from clipboard. |
| `scrollUp` | `shift+pageup` | Scroll up one page. |
| `scrollDown` | `shift+pagedown` | Scroll down one page. |
| `newTab` | `ctrl+t` | Open a new tab. |
| `closeTab` | `ctrl+w` | Close current tab. |
| `nextTab` | `ctrl+tab` | Switch to next tab. |
| `prevTab` | `ctrl+shift+tab` | Switch to previous tab. |
| `search` | `ctrl+f` | Open in-terminal search. |
| `zoomIn` | `ctrl+plus` | Increase font size. |
| `zoomOut` | `ctrl+minus` | Decrease font size. |
| `resetZoom` | `ctrl+0` | Reset to default font size. |

## Theme Presets

### Solarized Dark

```json
{
  "colors": {
    "foreground": "#839496",
    "background": "#002b36",
    "cursor": "#93a1a1",
    "palette": [
      "#073642", "#dc322f", "#859900", "#b58900",
      "#268bd2", "#d33682", "#2aa198", "#eee8d5",
      "#002b36", "#cb4b16", "#586e75", "#657b83",
      "#839496", "#6c71c4", "#93a1a1", "#fdf6e3"
    ]
  }
}
```

### Monokai

```json
{
  "colors": {
    "foreground": "#f8f8f2",
    "background": "#272822",
    "cursor": "#f8f8f0",
    "palette": [
      "#272822", "#f92672", "#a6e22e", "#f4bf75",
      "#66d9ef", "#ae81ff", "#a1efe4", "#f8f8f2",
      "#75715e", "#f92672", "#a6e22e", "#f4bf75",
      "#66d9ef", "#ae81ff", "#a1efe4", "#f9f8f5"
    ]
  }
}
```

### One Dark

```json
{
  "colors": {
    "foreground": "#abb2bf",
    "background": "#282c34",
    "cursor": "#528bff",
    "palette": [
      "#282c34", "#e06c75", "#98c379", "#e5c07b",
      "#61afef", "#c678dd", "#56b6c2", "#abb2bf",
      "#545862", "#e06c75", "#98c379", "#e5c07b",
      "#61afef", "#c678dd", "#56b6c2", "#c8ccd4"
    ]
  }
}
```

## Troubleshooting

### Configuration Not Loading

1. Check file exists at `%APPDATA%\TerminalDX12\config.json`
2. Validate JSON syntax (use a JSON validator)
3. Check for warnings in terminal output on startup

### Font Not Found

- Ensure font is installed system-wide (not user-only)
- Use exact font family name from Font settings
- Fallback: Consolas is always available on Windows

### Transparency Not Working

- Ensure GPU supports transparency compositing
- Check Windows composition is enabled
- Try `opacity` values between 0.8-1.0

## Programmatic Access

Configuration can be accessed in code via `Core::Config`:

```cpp
#include "core/Config.h"

Core::Config config;
config.LoadDefault();

// Read values
const auto& font = config.GetFont();
int fontSize = font.size;

// Modify values (for testing)
config.GetFontMut().size = 14;
```

See `include/core/Config.h` for the complete API.
