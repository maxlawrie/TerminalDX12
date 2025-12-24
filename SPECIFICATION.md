# TerminalDX12 Feature Specification

This document specifies all features for TerminalDX12, including current implementation status.

**Status Legend:**
- **COMPLETE** - Fully implemented and tested
- **PARTIAL** - Some functionality implemented
- **STUB** - Code structure exists but not functional
- **NOT STARTED** - No implementation exists

---

## Table of Contents

1. [VT Escape Sequences](#1-vt-escape-sequences)
2. [Text Attributes (SGR)](#2-text-attributes-sgr)
3. [Color Support](#3-color-support)
4. [Screen Buffer](#4-screen-buffer)
5. [Cursor Management](#5-cursor-management)
6. [Scrolling](#6-scrolling)
7. [Mouse Support](#7-mouse-support)
8. [Clipboard](#8-clipboard)
9. [Keyboard Input](#9-keyboard-input)
10. [Shell Integration](#10-shell-integration)
11. [Tabs](#11-tabs)
12. [Split Panes](#12-split-panes)
13. [Search](#13-search)
14. [URL Detection](#14-url-detection)
15. [Font Rendering](#15-font-rendering)
16. [Configuration](#16-configuration)
17. [Window Management](#17-window-management)
18. [Rendering](#18-rendering)
19. [Claude Code Compatibility](#claude-code-compatibility)

---

## 1. VT Escape Sequences

### 1.1 Simple Escape Sequences (ESC + char)

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `ESC c` | RIS | Full terminal reset | **COMPLETE** |
| `ESC D` | IND | Index - move cursor down, scroll if at bottom | **COMPLETE** |
| `ESC E` | NEL | Next Line - move to column 1 of next line | **COMPLETE** |
| `ESC H` | HTS | Horizontal Tab Set | **COMPLETE** |
| `ESC M` | RI | Reverse Index - move cursor up, scroll if at top | **COMPLETE** |
| `ESC 7` | DECSC | Save Cursor Position | **COMPLETE** |
| `ESC 8` | DECRC | Restore Cursor Position | **COMPLETE** |
| `ESC =` | DECKPAM | Keypad Application Mode | **COMPLETE** |
| `ESC >` | DECKPNM | Keypad Numeric Mode | **COMPLETE** |

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 115-130

### 1.2 CSI Sequences (ESC [ ... final)

#### Cursor Movement

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI n A` | CUU | Cursor Up n rows | **COMPLETE** |
| `CSI n B` | CUD | Cursor Down n rows | **COMPLETE** |
| `CSI n C` | CUF | Cursor Forward n columns | **COMPLETE** |
| `CSI n D` | CUB | Cursor Back n columns | **COMPLETE** |
| `CSI n E` | CNL | Cursor Next Line n times | **COMPLETE** |
| `CSI n F` | CPL | Cursor Previous Line n times | **COMPLETE** |
| `CSI n G` | CHA | Cursor Horizontal Absolute to column n | **COMPLETE** |
| `CSI n ; m H` | CUP | Cursor Position to row n, column m | **COMPLETE** |
| `CSI n ; m f` | HVP | Horizontal Vertical Position (same as CUP) | **COMPLETE** |
| `CSI n d` | VPA | Vertical Position Absolute to row n | **COMPLETE** |
| `CSI s` | SCP | Save Cursor Position | **COMPLETE** |
| `CSI u` | RCP | Restore Cursor Position | **COMPLETE** |

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 175-220

#### Erase Operations

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI 0 J` | ED | Erase from cursor to end of screen | **COMPLETE** |
| `CSI 1 J` | ED | Erase from start of screen to cursor | **COMPLETE** |
| `CSI 2 J` | ED | Erase entire screen | **COMPLETE** |
| `CSI 3 J` | ED | Erase scrollback buffer | **COMPLETE** |
| `CSI 0 K` | EL | Erase from cursor to end of line | **COMPLETE** |
| `CSI 1 K` | EL | Erase from start of line to cursor | **COMPLETE** |
| `CSI 2 K` | EL | Erase entire line | **COMPLETE** |
| `CSI n X` | ECH | Erase n characters at cursor | **COMPLETE** |

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 220-250

#### Insert/Delete Operations

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI n @` | ICH | Insert n blank characters at cursor | **COMPLETE** |
| `CSI n P` | DCH | Delete n characters at cursor | **COMPLETE** |
| `CSI n L` | IL | Insert n blank lines at cursor | **COMPLETE** |
| `CSI n M` | DL | Delete n lines at cursor | **COMPLETE** |

**Specification:**
- ICH: Insert n blank characters at cursor position, shifting existing characters right
- DCH: Delete n characters at cursor, shifting remaining characters left
- IL: Insert n blank lines at cursor row, scrolling down within scroll region
- DL: Delete n lines at cursor row, scrolling up within scroll region

#### Scrolling

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI n S` | SU | Scroll Up n lines | **COMPLETE** |
| `CSI n T` | SD | Scroll Down n lines | **COMPLETE** |
| `CSI t ; b r` | DECSTBM | Set Top and Bottom Margins (scroll region) | **COMPLETE** |

**Specification:**
- SU: Scroll content up n lines, new blank lines appear at bottom
- SD: Scroll content down n lines, new blank lines appear at top
- DECSTBM: Define scrolling region from row t to row b (1-indexed)

#### Mode Setting

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI ? n h` | DECSET | Enable private mode n | **COMPLETE** |
| `CSI ? n l` | DECRST | Disable private mode n | **COMPLETE** |
| `CSI n h` | SM | Set ANSI mode n | NOT STARTED |
| `CSI n l` | RM | Reset ANSI mode n | NOT STARTED |

**Private Modes (DECSET/DECRST):**

| Mode | Name | Description | Status |
|------|------|-------------|--------|
| 1 | DECCKM | Application Cursor Keys | **COMPLETE** |
| 7 | DECAWM | Auto-Wrap Mode | **COMPLETE** |
| 12 | - | Cursor Blink | **COMPLETE** |
| 25 | DECTCEM | Cursor Visible | **COMPLETE** |
| 47 | - | Alternate Screen Buffer | **COMPLETE** |
| 1000 | - | Mouse Click Tracking | **COMPLETE** |
| 1002 | - | Mouse Cell Motion Tracking | **COMPLETE** |
| 1003 | - | Mouse All Motion Tracking | **COMPLETE** |
| 1006 | - | SGR Mouse Mode | **COMPLETE** |
| 1049 | - | Alternate Screen + Save Cursor | **COMPLETE** |
| 2004 | - | Bracketed Paste Mode | **COMPLETE** |

**Specification:**
- DECCKM: When enabled, cursor keys send application sequences (ESC O A) instead of normal (ESC [ A)
- DECAWM: When enabled, cursor wraps to next line at right margin; when disabled, cursor stays at margin
- DECTCEM: When enabled, cursor is visible; when disabled, cursor is hidden
- Mouse modes: Enable mouse event reporting to the application

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 442-445 (stub only)

#### Device Status

| Sequence | Name | Description | Status |
|----------|------|-------------|--------|
| `CSI c` | DA | Primary Device Attributes | **COMPLETE** |
| `CSI > c` | DA2 | Secondary Device Attributes | **COMPLETE** |
| `CSI 5 n` | DSR | Device Status Report | **COMPLETE** |
| `CSI 6 n` | CPR | Cursor Position Report | **COMPLETE** |

**Specification:**
- DA: Terminal responds with `CSI ? 1 ; 2 c` (VT100 with AVO)
- DSR: Terminal responds with `CSI 0 n` (ready, no malfunction)
- CPR: Terminal responds with `CSI row ; column R`

**Implementation:** `src/terminal/VTStateMachine.cpp` HandleDeviceAttributes(), HandleDeviceStatusReport()

### 1.3 OSC Sequences (ESC ] ... ST/BEL)

| Sequence | Description | Status |
|----------|-------------|--------|
| `OSC 0 ; text ST` | Set Window Title and Icon Name | PARTIAL |
| `OSC 1 ; text ST` | Set Icon Name | PARTIAL |
| `OSC 2 ; text ST` | Set Window Title | PARTIAL |
| `OSC 4 ; index ; color ST` | Set Color Palette Entry | NOT STARTED |
| `OSC 8 ; params ; uri ST` | Hyperlink | NOT STARTED |
| `OSC 10 ; color ST` | Set Foreground Color | NOT STARTED |
| `OSC 11 ; color ST` | Set Background Color | NOT STARTED |
| `OSC 52 ; base64 ST` | Clipboard Access | NOT STARTED |
| `OSC 133 ; A ST` | Shell Integration - Prompt Start | **COMPLETE** |
| `OSC 133 ; B ST` | Shell Integration - Command Start | **COMPLETE** |
| `OSC 133 ; C ST` | Shell Integration - Command End | **COMPLETE** |
| `OSC 133 ; D ; exitcode ST` | Shell Integration - Command Finished | **COMPLETE** |

**Specification:**
- OSC 0/1/2: Set window title displayed in title bar
- OSC 4: Customize the 256-color palette at runtime
- OSC 8: Create clickable hyperlinks in terminal output
- OSC 52: Allow applications to read/write system clipboard
- OSC 133: Shell integration for command tracking, marks, and navigation

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 135-170 (parsing only, no action)

### 1.4 DCS Sequences (ESC P ... ST)

| Sequence | Description | Status |
|----------|-------------|--------|
| `DCS $ q " p ST` | Request DECSCL (compatibility level) | NOT STARTED |
| `DCS + q ... ST` | Request terminfo capability | NOT STARTED |

**Status:** NOT STARTED - No DCS parsing implemented

---

## 2. Text Attributes (SGR)

SGR (Select Graphic Rendition) sequences: `CSI n m`

### 2.1 Basic Attributes

| Code | Attribute | Status |
|------|-----------|--------|
| 0 | Reset all attributes | **COMPLETE** |
| 1 | Bold | **COMPLETE** |
| 2 | Dim/Faint | **COMPLETE** |
| 3 | Italic | **COMPLETE** |
| 4 | Underline | **COMPLETE** |
| 5 | Slow Blink | **COMPLETE** |
| 6 | Rapid Blink | **COMPLETE** |
| 7 | Inverse/Reverse | **COMPLETE** |
| 8 | Hidden/Invisible | **COMPLETE** |
| 9 | Strikethrough | **COMPLETE** |
| 21 | Double Underline | NOT STARTED |
| 22 | Normal Intensity (not bold/dim) | **COMPLETE** |
| 23 | Not Italic | **COMPLETE** |
| 24 | Not Underlined | **COMPLETE** |
| 25 | Not Blinking | **COMPLETE** |
| 27 | Not Inverse | **COMPLETE** |
| 28 | Not Hidden | **COMPLETE** |
| 29 | Not Strikethrough | **COMPLETE** |

**Implementation:** `src/terminal/VTStateMachine.cpp` lines 280-310
**Rendering:** `src/core/Application.cpp` lines 345-354 (underline rendering)

### 2.2 Underline Styles (SGR 4:n)

| Code | Style | Status |
|------|-------|--------|
| 4:0 | No underline | NOT STARTED |
| 4:1 | Single underline | NOT STARTED |
| 4:2 | Double underline | NOT STARTED |
| 4:3 | Curly underline | NOT STARTED |
| 4:4 | Dotted underline | NOT STARTED |
| 4:5 | Dashed underline | NOT STARTED |

**Specification:** Extended underline styles use colon-separated parameters. Currently only basic `CSI 4 m` is supported.

---

## 3. Color Support

### 3.1 Standard Colors (16-color)

| Codes | Description | Status |
|-------|-------------|--------|
| 30-37 | Foreground (Black, Red, Green, Yellow, Blue, Magenta, Cyan, White) | **COMPLETE** |
| 40-47 | Background (same colors) | **COMPLETE** |
| 39 | Default Foreground | **COMPLETE** |
| 49 | Default Background | **COMPLETE** |
| 90-97 | Bright Foreground | **COMPLETE** |
| 100-107 | Bright Background | **COMPLETE** |

**Color Palette (VGA):**
| Index | Color | RGB Value |
|-------|-------|-----------|
| 0 | Black | (0, 0, 0) |
| 1 | Red | (205, 49, 49) |
| 2 | Green | (0, 205, 0) |
| 3 | Yellow | (205, 205, 0) |
| 4 | Blue | (0, 0, 238) |
| 5 | Magenta | (205, 0, 205) |
| 6 | Cyan | (0, 205, 205) |
| 7 | White | (229, 229, 229) |
| 8-15 | Bright variants | (brighter values) |

**Implementation:** `src/core/Application.cpp` lines 300-340 (hardcoded palette)

### 3.2 256-Color Mode

| Sequence | Description | Status |
|----------|-------------|--------|
| `CSI 38 ; 5 ; n m` | Set foreground to color n (0-255) | **COMPLETE** |
| `CSI 48 ; 5 ; n m` | Set background to color n (0-255) | **COMPLETE** |

**Specification:**
- Colors 0-15: Standard 16 colors
- Colors 16-231: 6x6x6 color cube (R,G,B each 0-5)
- Colors 232-255: Grayscale (24 shades)

**Implementation:** Full 256-color support with proper palette indexing

### 3.3 Truecolor (24-bit RGB)

| Sequence | Description | Status |
|----------|-------------|--------|
| `CSI 38 ; 2 ; r ; g ; b m` | Set foreground to RGB | **COMPLETE** |
| `CSI 48 ; 2 ; r ; g ; b m` | Set background to RGB | **COMPLETE** |

**Specification:** Full 24-bit color (16.7 million colors)

**Implementation:** Stores full RGB values in CellAttributes (fgR, fgG, fgB, bgR, bgG, bgB) with FLAG_TRUECOLOR_FG/BG flags

**Implementation:** `src/terminal/VTStateMachine.cpp` HandleSGR()

---

## 4. Screen Buffer

### 4.1 Primary Buffer

| Feature | Description | Status |
|---------|-------------|--------|
| Cell Storage | 2D grid of cells (char + attributes) | **COMPLETE** |
| Dimensions | Configurable columns x rows | **COMPLETE** |
| Resize | Dynamic resize with content preservation | **COMPLETE** |
| Clear | Clear entire buffer or regions | **COMPLETE** |
| Line Operations | Clear line, clear to end/start | **COMPLETE** |

**Implementation:** `src/terminal/ScreenBuffer.cpp` (340 lines)

### 4.2 Alternate Buffer

| Feature | Description | Status |
|---------|-------------|--------|
| Switch to Alt | Save primary, switch to blank alternate | **COMPLETE** |
| Switch to Primary | Restore primary buffer | **COMPLETE** |
| No Scrollback | Alternate buffer has no scrollback | **COMPLETE** |
| Isolation | Changes don't affect primary | **COMPLETE** |

**Specification:** Used by full-screen apps (vim, less, htop). Primary buffer content preserved.

**Implementation:** `src/terminal/ScreenBuffer.cpp` lines 280-310

### 4.3 Cell Structure

```cpp
struct CellAttributes {
    uint8_t foreground;      // Color index (0-255)
    uint8_t background;      // Color index (0-255)
    uint8_t flags;           // Attribute flags

    static constexpr uint8_t FLAG_BOLD      = 0x01;
    static constexpr uint8_t FLAG_ITALIC    = 0x02;
    static constexpr uint8_t FLAG_UNDERLINE = 0x04;
    static constexpr uint8_t FLAG_INVERSE   = 0x08;
};

struct Cell {
    char32_t ch;             // Unicode codepoint
    CellAttributes attr;     // Text attributes
};
```

**Status:** **COMPLETE**

**Implementation:** `include/terminal/ScreenBuffer.h` lines 10-35

---

## 5. Cursor Management

### 5.1 Cursor Position

| Feature | Description | Status |
|---------|-------------|--------|
| Get Position | Query current cursor row/column | **COMPLETE** |
| Set Position | Move cursor to specific row/column | **COMPLETE** |
| Bounds Clamping | Clamp to valid buffer range | **COMPLETE** |
| Report Position | Respond to CPR request | **COMPLETE** |

**Implementation:** `src/terminal/ScreenBuffer.cpp` lines 50-80

### 5.2 Cursor Visibility

| Feature | Description | Status |
|---------|-------------|--------|
| Show/Hide | DECTCEM mode | **COMPLETE** |
| Blink | Toggle visibility on timer | **COMPLETE** |
| Blink Rate | Configurable blink interval | PARTIAL |

**Current Implementation:** 500ms blink rate, hardcoded

**Implementation:** `src/core/Application.cpp` lines 360-380

### 5.3 Cursor Style

| Style | Description | Status |
|-------|-------------|--------|
| Block | Solid block cursor | **COMPLETE** |
| Underline | Thin line under character | **COMPLETE** |
| Bar | Thin vertical line | **COMPLETE** |
| Blinking variants | Blinking versions of each | **COMPLETE** |

**Specification:** Supports `CSI n SP q` (DECSCUSR) to set cursor style (n=0-6)

### 5.4 Cursor Save/Restore

| Feature | Description | Status |
|---------|-------------|--------|
| DECSC (ESC 7) | Save cursor position + attributes | **COMPLETE** |
| DECRC (ESC 8) | Restore cursor position + attributes | **COMPLETE** |
| SCP (CSI s) | Save cursor position only | **COMPLETE** |
| RCP (CSI u) | Restore cursor position only | **COMPLETE** |

**Specification:** DECSC/DECRC save: position, attributes, character set, origin mode, autowrap
**Implementation:** `src/terminal/VTStateMachine.cpp` SaveCursorState(), RestoreCursorState()

---

## 6. Scrolling

### 6.1 Scrollback Buffer

| Feature | Description | Status |
|---------|-------------|--------|
| Line Storage | Circular buffer for history | **COMPLETE** |
| Capacity | Configurable max lines | **COMPLETE** |
| Navigation | Scroll up/down through history | **COMPLETE** |
| Mouse Wheel | Scroll with mouse wheel | **COMPLETE** |
| Keyboard | Shift+PageUp/Down | **COMPLETE** |

**Current Implementation:** 10,000 lines, 3 lines per mouse wheel notch

**Implementation:** `src/terminal/ScreenBuffer.cpp` lines 140-180

### 6.2 Scroll Region

| Feature | Description | Status |
|---------|-------------|--------|
| DECSTBM | Set top/bottom margins | **COMPLETE** |
| Region Scroll | Scroll only within region | **COMPLETE** |
| Origin Mode | Cursor relative to region | NOT STARTED |

**Specification:**
- DECSTBM `CSI t ; b r` sets scroll region from row t to row b
- Content outside region is unaffected by scroll operations
- DECOM enables origin mode where cursor positions are relative to scroll region

**Implementation:** `src/terminal/ScreenBuffer.cpp` SetScrollRegion(), ScrollRegionUp(), ScrollRegionDown()

### 6.3 Scroll Operations

| Feature | Description | Status |
|---------|-------------|--------|
| ScrollUp | Move content up, add blank at bottom | **COMPLETE** |
| ScrollDown | Move content down, add blank at top | **COMPLETE** |
| SU (CSI n S) | Scroll up n lines | **COMPLETE** |
| SD (CSI n T) | Scroll down n lines | **COMPLETE** |

**Implementation:** `src/terminal/ScreenBuffer.cpp` ScrollUp(), ScrollDown(), ScrollRegionUp(), ScrollRegionDown()

---

## 7. Mouse Support

### 7.1 Selection

| Feature | Description | Status |
|---------|-------------|--------|
| Click to Start | Left click begins selection | **COMPLETE** |
| Drag to Select | Mouse move extends selection | **COMPLETE** |
| Release to End | Mouse up finalizes selection | **COMPLETE** |
| Visual Highlight | Blue highlight on selected text | **COMPLETE** |
| Multi-line | Selection spans multiple lines | **COMPLETE** |
| Double-click | Select word | NOT STARTED |
| Triple-click | Select line | NOT STARTED |
| Shift+Click | Extend selection | NOT STARTED |
| Block/Rectangle | Alt+drag for block selection | NOT STARTED |

**Implementation:** `src/core/Application.cpp` lines 509-590

### 7.2 Mouse Buttons

| Feature | Description | Status |
|---------|-------------|--------|
| Left Button | Selection start/end | **COMPLETE** |
| Right Button | Context menu | NOT STARTED |
| Middle Button | Paste (X11 style) | NOT STARTED |
| Scroll Wheel | Scrollback navigation | **COMPLETE** |

### 7.3 Mouse Reporting

| Mode | Description | Status |
|------|-------------|--------|
| 1000 | Click tracking (X10) | **COMPLETE** |
| 1002 | Cell motion tracking | **COMPLETE** |
| 1003 | All motion tracking | **COMPLETE** |
| 1006 | SGR extended mode | **COMPLETE** |
| 1015 | URXVT mode | NOT STARTED |

**Specification:**
- Mode 1000: Report button press/release with coordinates
- Mode 1002: Also report motion while button pressed
- Mode 1003: Report all motion, even without button
- Mode 1006: Extended format `CSI < button ; x ; y M/m`

**Implementation:** `src/terminal/VTStateMachine.cpp` HandleMode() for mode tracking, `src/core/Application.cpp` for mouse event encoding

---

## 8. Clipboard

### 8.1 Copy

| Feature | Description | Status |
|---------|-------------|--------|
| Ctrl+C | Copy selection to clipboard | **COMPLETE** |
| Right-click Copy | Context menu copy | NOT STARTED |
| Copy as HTML | Preserve formatting | NOT STARTED |
| Copy as Plain | Strip formatting | **COMPLETE** |
| Trim Trailing | Remove trailing spaces | **COMPLETE** |

**Implementation:** `src/core/Application.cpp` lines 620-680

### 8.2 Paste

| Feature | Description | Status |
|---------|-------------|--------|
| Ctrl+V | Paste from clipboard | **COMPLETE** |
| Right-click Paste | Context menu paste | NOT STARTED |
| Ctrl+Shift+V | Paste without formatting | NOT STARTED |
| Bracketed Paste | Wrap in ESC sequences | **COMPLETE** |
| Middle Button | X11-style paste | NOT STARTED |

**Specification:**
- Bracketed paste mode (DECSET 2004): Wrap pasted text in `ESC [ 200 ~` and `ESC [ 201 ~`
- Allows applications to distinguish typed text from pasted text

**Implementation:** `src/core/Application.cpp` checks IsBracketedPasteEnabled() and wraps paste content

**Implementation:** `src/core/Application.cpp` lines 680-730

### 8.3 OSC 52 Clipboard

| Feature | Description | Status |
|---------|-------------|--------|
| Read | Application reads clipboard | NOT STARTED |
| Write | Application writes clipboard | NOT STARTED |

**Specification:** `OSC 52 ; c ; base64-data ST` for clipboard access from applications

---

## 9. Keyboard Input

### 9.1 Regular Input

| Feature | Description | Status |
|---------|-------------|--------|
| ASCII Characters | a-z, 0-9, symbols | **COMPLETE** |
| Unicode Input | Full UTF-8 support | **COMPLETE** |
| Enter | Carriage return (0x0D) | **COMPLETE** |
| Backspace | Delete character (0x7F or 0x08) | **COMPLETE** |
| Tab | Horizontal tab (0x09) | **COMPLETE** |

**Implementation:** `src/core/Application.cpp` WndProc WM_CHAR handling

### 9.2 Special Keys

| Key | Normal | Application Mode | Status |
|-----|--------|------------------|--------|
| Up | `ESC [ A` | `ESC O A` | **COMPLETE** |
| Down | `ESC [ B` | `ESC O B` | **COMPLETE** |
| Left | `ESC [ C` | `ESC O C` | **COMPLETE** |
| Right | `ESC [ D` | `ESC O D` | **COMPLETE** |
| Home | `ESC [ H` | `ESC O H` | PARTIAL |
| End | `ESC [ F` | `ESC O F` | PARTIAL |
| Insert | `ESC [ 2 ~` | - | NOT STARTED |
| Delete | `ESC [ 3 ~` | - | NOT STARTED |
| PageUp | `ESC [ 5 ~` | - | **COMPLETE** |
| PageDown | `ESC [ 6 ~` | - | **COMPLETE** |
| F1-F4 | `ESC O P/Q/R/S` | - | NOT STARTED |
| F5-F12 | `ESC [ 15/17/18/19/20/21/23/24 ~` | - | NOT STARTED |

**Implementation:** `src/core/Application.cpp` WndProc WM_KEYDOWN handling

### 9.3 Modifier Combinations

| Modifier | Format | Status |
|----------|--------|--------|
| Shift | `CSI 1 ; 2 X` | NOT STARTED |
| Alt | `CSI 1 ; 3 X` | NOT STARTED |
| Ctrl | `CSI 1 ; 5 X` | PARTIAL |
| Ctrl+Shift | `CSI 1 ; 6 X` | NOT STARTED |
| Alt+Shift | `CSI 1 ; 4 X` | NOT STARTED |

**Specification:** Modified keys use `CSI 1 ; modifier final` format

---

## 10. Shell Integration

### 10.1 ConPTY Session

| Feature | Description | Status |
|---------|-------------|--------|
| Create PTY | Create Windows pseudo-console | **COMPLETE** |
| Spawn Shell | Start PowerShell process | **COMPLETE** |
| Resize | Propagate size changes | **COMPLETE** |
| Read Output | Async read from PTY | **COMPLETE** |
| Write Input | Send keystrokes to PTY | **COMPLETE** |
| Close | Clean shutdown | **COMPLETE** |

**Implementation:** `src/pty/ConPtySession.cpp`

### 10.2 Shell Configuration

| Feature | Description | Status |
|---------|-------------|--------|
| Shell Selection | Choose shell (cmd, PowerShell, WSL) | **COMPLETE** |
| Working Directory | Set initial directory | **COMPLETE** |
| Environment | Pass environment variables | NOT STARTED |
| Arguments | Shell command-line arguments | NOT STARTED |

**Current Implementation:** Configurable via config.json or command-line argument

### 10.3 OSC 133 Shell Integration

| Marker | Description | Status |
|--------|-------------|--------|
| FinalTerm A | Prompt started | **COMPLETE** |
| FinalTerm B | Command started | **COMPLETE** |
| FinalTerm C | Command executed | **COMPLETE** |
| FinalTerm D | Command finished with exit code | **COMPLETE** |

**Specification:** Used by VS Code, iTerm2, Windows Terminal for:
- Command detection and navigation
- Command duration tracking
- Exit code display
- Prompt markers

---

## 11. Tabs

### 11.1 Tab Management

| Feature | Description | Status |
|---------|-------------|--------|
| Create Tab | New terminal session in tab | **COMPLETE** |
| Close Tab | Close tab and session | **COMPLETE** |
| Switch Tab | Click or keyboard shortcut | **COMPLETE** |
| Reorder Tabs | Drag to reorder | NOT STARTED |
| Tab Titles | Display tab title | **COMPLETE** |
| Tab Icons | Display shell icon | NOT STARTED |

**Current Implementation:** `src/ui/TabManager.cpp` (165 lines), `src/ui/Tab.cpp` (fully implemented)

### 11.2 Tab Keyboard Shortcuts

| Shortcut | Action | Status |
|----------|--------|--------|
| Ctrl+T | New tab | NOT STARTED |
| Ctrl+W | Close tab | NOT STARTED |
| Ctrl+Tab | Next tab | NOT STARTED |
| Ctrl+Shift+Tab | Previous tab | NOT STARTED |
| Ctrl+1-9 | Switch to tab N | NOT STARTED |

### 11.3 Tab Persistence

| Feature | Description | Status |
|---------|-------------|--------|
| Save Tabs | Remember open tabs | NOT STARTED |
| Restore Tabs | Reopen tabs on startup | NOT STARTED |
| Tab State | Save working directory | NOT STARTED |

---

## 12. Split Panes

### 12.1 Pane Management

| Feature | Description | Status |
|---------|-------------|--------|
| Vertical Split | Split pane left/right | NOT STARTED |
| Horizontal Split | Split pane top/bottom | NOT STARTED |
| Close Pane | Close pane, keep others | NOT STARTED |
| Resize Pane | Drag divider | NOT STARTED |
| Focus Pane | Click to focus | NOT STARTED |
| Zoom Pane | Maximize single pane | NOT STARTED |

### 12.2 Pane Navigation

| Shortcut | Action | Status |
|----------|--------|--------|
| Alt+Arrow | Move focus between panes | NOT STARTED |
| Alt+Shift+Arrow | Resize pane | NOT STARTED |
| Ctrl+Shift+W | Close focused pane | NOT STARTED |

---

## 13. Search

### 13.1 Search UI

| Feature | Description | Status |
|---------|-------------|--------|
| Open Search | Ctrl+Shift+F | NOT STARTED |
| Search Box | Text input field | NOT STARTED |
| Close Search | Escape | NOT STARTED |
| Match Count | Display N of M | NOT STARTED |

### 13.2 Search Features

| Feature | Description | Status |
|---------|-------------|--------|
| Find Next | F3 or Enter | NOT STARTED |
| Find Previous | Shift+F3 | NOT STARTED |
| Case Sensitive | Toggle option | NOT STARTED |
| Whole Word | Toggle option | NOT STARTED |
| Regex | Regular expression search | NOT STARTED |
| Highlight All | Highlight all matches | NOT STARTED |

### 13.3 Search Scope

| Feature | Description | Status |
|---------|-------------|--------|
| Visible Area | Search current view | NOT STARTED |
| Scrollback | Search history | NOT STARTED |
| All Tabs | Search all tabs | NOT STARTED |

---

## 14. URL Detection

### 14.1 URL Recognition

| Feature | Description | Status |
|---------|-------------|--------|
| HTTP/HTTPS | Detect web URLs | NOT STARTED |
| File Paths | Detect file:// URLs | NOT STARTED |
| Email | Detect mailto: links | NOT STARTED |
| Custom Schemes | Configurable patterns | NOT STARTED |

### 14.2 URL Interaction

| Feature | Description | Status |
|---------|-------------|--------|
| Underline | Show underline on hover | NOT STARTED |
| Ctrl+Click | Open in browser | NOT STARTED |
| Right-Click | Copy URL option | NOT STARTED |
| OSC 8 | Explicit hyperlinks | NOT STARTED |

**Specification:**
- Detect URLs using regex patterns
- Visual feedback on hover (underline, color change)
- Ctrl+Click opens URL in default browser
- OSC 8 hyperlinks: `OSC 8 ; params ; uri ST text OSC 8 ; ; ST`

---

## 15. Font Rendering

### 15.1 Basic Rendering

| Feature | Description | Status |
|---------|-------------|--------|
| FreeType | Font loading and glyph rendering | **COMPLETE** |
| Glyph Cache | 2048x2048 texture atlas | **COMPLETE** |
| Monospace | Fixed-width character rendering | **COMPLETE** |
| Unicode | UTF-32 codepoint support | **COMPLETE** |

**Implementation:** `src/rendering/GlyphAtlas.cpp`

### 15.2 Font Variants

| Feature | Description | Status |
|---------|-------------|--------|
| Regular | Normal weight | **COMPLETE** |
| Bold | Bold weight via FreeType | PARTIAL |
| Italic | Italic style via FreeType | PARTIAL |
| Bold Italic | Combined | PARTIAL |

**Current Implementation:** Uses FreeType synthetic bold/italic. Should load separate font files.

### 15.3 Advanced Font Features

| Feature | Description | Status |
|---------|-------------|--------|
| Ligatures | Programming ligatures (fi, ->, ==) | NOT STARTED |
| Font Fallback | Chain of fallback fonts | PARTIAL |
| Emoji | Color emoji rendering | NOT STARTED |
| CJK | Chinese/Japanese/Korean | PARTIAL |
| RTL | Right-to-left text | NOT STARTED |
| Box Drawing | Optimized line characters | **COMPLETE** |
| Powerline | Nerd Font symbols | NOT STARTED |

**Specification:**
- Ligatures require HarfBuzz shaping engine integration
- Emoji require color font support (COLR/CPAL or CBDT/CBLC)
- Font fallback chain for missing glyphs

### 15.4 Font Configuration

| Feature | Description | Status |
|---------|-------------|--------|
| Font Family | Select font by name | **COMPLETE** |
| Font Size | Configurable size | **COMPLETE** |
| Line Height | Configurable line spacing | NOT STARTED |
| Letter Spacing | Configurable character spacing | NOT STARTED |
| DPI Scaling | High-DPI support | **COMPLETE** |

**Current Implementation:** Configurable via config.json (family, size, weight)

---

## 16. Configuration

### 16.1 Configuration File

| Feature | Description | Status |
|---------|-------------|--------|
| File Format | JSON config | **COMPLETE** |
| Auto-reload | Watch for changes | NOT STARTED |
| Defaults | Sensible default values | **COMPLETE** |
| Validation | Validate config values | **COMPLETE** |

**Proposed Format (JSON):**
```json
{
  "font": {
    "family": "Consolas",
    "size": 14,
    "ligatures": true
  },
  "colors": {
    "foreground": "#CCCCCC",
    "background": "#1E1E1E",
    "palette": ["#000000", "#CD3131", ...]
  },
  "terminal": {
    "scrollback": 10000,
    "cursorStyle": "block",
    "cursorBlink": true
  },
  "keybindings": {
    "copy": "ctrl+c",
    "paste": "ctrl+v"
  }
}
```

### 16.2 Configurable Settings

| Setting | Description | Status |
|---------|-------------|--------|
| Font Family | Font name | **COMPLETE** |
| Font Size | Size in points | **COMPLETE** |
| Color Scheme | Named color schemes | **COMPLETE** |
| Cursor Style | Block/underline/bar | **COMPLETE** |
| Cursor Blink | Enable/disable | **COMPLETE** |
| Scrollback Lines | History size | **COMPLETE** |
| Bell | Audible/visual/none | NOT STARTED |
| Window Size | Initial dimensions | NOT STARTED |
| Key Bindings | Custom shortcuts | **COMPLETE** |
| Shell | Default shell path | **COMPLETE** |
| Working Directory | Initial directory | **COMPLETE** |

### 16.3 Settings UI

| Feature | Description | Status |
|---------|-------------|--------|
| Settings Dialog | GUI for settings | NOT STARTED |
| Live Preview | See changes immediately | NOT STARTED |
| Profiles | Multiple configurations | NOT STARTED |

---

## 17. Window Management

### 17.1 Window Features

| Feature | Description | Status |
|---------|-------------|--------|
| Title Bar | Show window title | **COMPLETE** |
| Resize | Resize window | **COMPLETE** |
| Minimize | Minimize to taskbar | **COMPLETE** |
| Maximize | Maximize window | **COMPLETE** |
| Close | Close window | **COMPLETE** |
| DPI Awareness | Per-monitor DPI | **COMPLETE** |

**Implementation:** `src/core/Window.cpp`

### 17.2 Advanced Window

| Feature | Description | Status |
|---------|-------------|--------|
| Transparency | Window opacity | NOT STARTED |
| Always on Top | Stay above other windows | NOT STARTED |
| Borderless | No window chrome | NOT STARTED |
| Full Screen | Full screen mode | NOT STARTED |
| Remember Position | Save window position | NOT STARTED |
| Multiple Windows | Multiple terminal windows | NOT STARTED |

---

## 18. Rendering

### 18.1 DirectX 12 Pipeline

| Feature | Description | Status |
|---------|-------------|--------|
| Device Creation | DX12 device and queues | **COMPLETE** |
| Swap Chain | Frame presentation | **COMPLETE** |
| Render Target | Render to backbuffer | **COMPLETE** |
| Command Lists | Record and execute | **COMPLETE** |
| Synchronization | Fence-based sync | **COMPLETE** |

**Implementation:** `src/rendering/DX12Renderer.cpp`

### 18.2 Text Rendering

| Feature | Description | Status |
|---------|-------------|--------|
| Glyph Instances | Instanced glyph rendering | **COMPLETE** |
| Texture Atlas | Cached glyph textures | **COMPLETE** |
| Vertex/Pixel Shaders | HLSL shaders | **COMPLETE** |
| Color Blending | Text color application | **COMPLETE** |

**Implementation:**
- `src/rendering/TextRenderer.cpp`
- `shaders/GlyphVertex.hlsl`
- `shaders/GlyphPixel.hlsl`

### 18.3 Visual Effects

| Feature | Description | Status |
|---------|-------------|--------|
| Background Color | Solid background | **COMPLETE** |
| Selection Highlight | Blue selection overlay | **COMPLETE** |
| Underline | Text underline rendering | **COMPLETE** |
| Cursor | Block cursor rendering | **COMPLETE** |
| Inverse Video | Swap fg/bg colors | **COMPLETE** |
| Bold | Brighter colors | **COMPLETE** |

### 18.4 Performance

| Feature | Description | Status |
|---------|-------------|--------|
| Dirty Tracking | Only re-render changes | **COMPLETE** |
| Batch Rendering | Instance batching | **COMPLETE** |
| Frame Rate | Uncapped (vsync optional) | **COMPLETE** |
| GPU Acceleration | Hardware rendering | **COMPLETE** |

---

## Claude Code Compatibility

This section tracks features required for running [Claude Code](https://claude.com/claude-code), Anthropic's CLI coding agent.

### Required Features

| Feature | Description | Section | Current Status |
|---------|-------------|---------|----------------|
| **True Color (24-bit)** | Full RGB color for syntax highlighting | [3.3](#33-truecolor-24-bit-rgb) | **COMPLETE** |
| **Dim/Faint (SGR 2)** | De-emphasized text styling | [2.1](#21-basic-attributes) | **COMPLETE** |
| **Strikethrough (SGR 9)** | Show removed/deleted code | [2.1](#21-basic-attributes) | **COMPLETE** |
| **Cursor Hide/Show** | DECTCEM (mode 25) | [5.2](#52-cursor-visibility) | **COMPLETE** |
| **Cursor Save/Restore** | CSI s / CSI u sequences | [5.4](#54-cursor-saverestore) | **COMPLETE** |
| **Bracketed Paste** | Mode 2004 for paste detection | [8.2](#82-paste) | **COMPLETE** |
| **Alt Screen Buffer** | Mode 1049 for TUI apps | [4.2](#42-alternate-buffer) | **COMPLETE** |
| **Cursor Horizontal Absolute** | CSI n G (CHA) | [1.2](#cursor-movement) | **COMPLETE** |
| **Erase Characters** | CSI n X (ECH) | [1.2](#erase-operations) | **COMPLETE** |
| **Cursor Position Report** | CSI 6 n / response | [1.2](#device-status) | **COMPLETE** |
| **Application Cursor Keys** | DECCKM (mode 1) | [1.2](#mode-setting) | **COMPLETE** |
| **Auto-Wrap Mode** | DECAWM (mode 7) | [1.2](#mode-setting) | **COMPLETE** |

### Implementation Checklist

**High Priority (Core functionality):**
- [X] True color support (store full RGB, not map to 16)
- [X] Dim/Faint text attribute (SGR 2)
- [X] Cursor visibility control (DECTCEM mode 25)
- [X] Cursor save/restore (CSI s/u)
- [X] Cursor Horizontal Absolute (CSI G)
- [X] Device Status Report / Cursor Position Report (CSI 6 n -> CSI row;col R)

**Medium Priority (Enhanced experience):**
- [X] Strikethrough attribute (SGR 9)
- [X] Bracketed paste mode (mode 2004)
- [X] Erase Characters (CSI X)
- [X] Application cursor keys (DECCKM mode 1)
- [X] Auto-wrap mode (DECAWM mode 7)

**Low Priority (Nice to have):**
- [ ] OSC 8 hyperlinks (clickable URLs in output)
- [X] OSC 133 shell integration (command tracking)

### Notes

- Claude Code uses ANSI escape sequences extensively for its TUI interface
- The status line and syntax highlighting require proper color support
- Cursor manipulation is used for dynamic UI updates
- See [Claude Code docs](https://docs.anthropic.com/en/docs/claude-code) for more information

**Status: All Claude Code compatibility features are now COMPLETE as of Phase 1 VT Compatibility implementation.**

---

## Implementation Priority

### Phase 1: Core VT Compatibility (Critical) - **COMPLETE**
1. ~~Private mode sequences (DECTCEM, DECAWM, DECCKM)~~ ✓
2. ~~Insert/Delete character and line operations~~ ✓
3. ~~Cursor save/restore (DECSC/DECRC)~~ ✓
4. ~~Device Attributes response~~ ✓
5. ~~Scroll regions (DECSTBM)~~ ✓
6. ~~True color (24-bit RGB)~~ ✓
7. ~~Dim/Strikethrough text attributes~~ ✓
8. ~~Mouse mode reporting~~ ✓
9. ~~Bracketed paste mode~~ ✓
10. ~~OSC 133 shell integration~~ ✓

### Phase 2: Configuration - **COMPLETE**
1. ~~JSON configuration file~~ ✓
2. ~~Font selection~~ ✓
3. ~~Color scheme support~~ ✓
4. ~~Keybinding customization~~ ✓

### Phase 3: Mouse Enhancement - PARTIAL
1. ~~Mouse reporting to applications~~ ✓
2. Double/triple-click selection
3. Right-click context menu

### Phase 4: Advanced Features - PARTIAL
1. ~~Tab support~~ ✓
2. Search functionality
3. URL detection
4. Split panes

### Phase 5: Polish
1. Settings UI
2. Ligatures (HarfBuzz)
3. ~~Shell integration (OSC 133)~~ ✓
4. Multiple windows
