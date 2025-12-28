# Manual Test Checklist for TerminalDX12

Use this checklist to manually verify terminal functionality.

## Pre-Test Setup
- [ ] Terminal executable is built and available at `C:\Temp\TerminalDX12Test\TerminalDX12.exe`
- [ ] All required DLLs are in the same directory
- [ ] Shader files are present in `shaders/` subdirectory

---

## Test 1: Basic Startup
**Steps:**
1. Launch `TerminalDX12.exe`
2. Wait for window to appear

**Expected Results:**
- [ ] Window opens successfully
- [ ] Window title shows "TerminalDX12"
- [ ] PowerShell prompt is visible (e.g., `PS C:\Users\username>`)
- [ ] Green blinking cursor is visible

---

## Test 2: Keyboard Input
**Steps:**
1. Type: `echo Hello World`
2. Press Enter

**Expected Results:**
- [ ] Text appears as you type
- [ ] "Hello World" appears after pressing Enter
- [ ] Cursor moves to new line with prompt

---

## Test 3: Foreground Colors
**Steps:**

**Option 1 - PowerShell (recommended):**
```powershell
Write-Host "Red" -ForegroundColor Red; Write-Host "Green" -ForegroundColor Green; Write-Host "Blue" -ForegroundColor Blue
```

**Option 2 - Using escape sequences in PowerShell:**
```powershell
$ESC = [char]27
Write-Host "${ESC}[31mRed${ESC}[0m ${ESC}[32mGreen${ESC}[0m ${ESC}[34mBlue${ESC}[0m ${ESC}[33mYellow${ESC}[0m"
```

**Expected Results:**
- [ ] "Red" appears in red color
- [ ] "Green" appears in green color
- [ ] "Blue" appears in blue color
- [ ] "Yellow" appears in yellow color

---

## Test 4: Background Colors
**Steps:**
1. Type: `powershell -Command "Write-Host 'Test' -BackgroundColor Red"`
2. Press Enter

**Expected Results:**
- [ ] Text appears with red background
- [ ] Background color fills the character cells

---

## Test 5: Bold Text
**Steps:**
1. Type some text with bold ANSI sequence
2. Observe rendering

**Expected Results:**
- [ ] Bold text appears brighter than normal text

---

## Test 6: Underline Text
**Steps:**
1. Type text with underline ANSI sequence
2. Observe rendering

**Expected Results:**
- [ ] Underlined text shows underline beneath characters

---

## Test 7: Directory Listing
**Steps:**
1. Type: `dir`
2. Press Enter

**Expected Results:**
- [ ] Directory contents appear
- [ ] Folders may appear in different color than files
- [ ] Text is readable and properly aligned

---

## Test 8: Clear Screen
**Steps:**
1. Type: `cls`
2. Press Enter

**Expected Results:**
- [ ] Screen clears completely
- [ ] Only prompt and cursor remain
- [ ] Cursor returns to top-left area

---

## Test 9: Long Output (Scrolling)
**Steps:**
1. Type: `dir /s C:\Windows\System32`
2. Press Enter
3. Wait for output to scroll

**Expected Results:**
- [ ] Text scrolls smoothly as output appears
- [ ] Older text scrolls off the top
- [ ] Terminal remains responsive

---

## Test 10: Scrollback with Keyboard
**Steps:**
1. After Test 9, press `Shift+Page Up`
2. Press it multiple times
3. Press `Shift+Page Down` to scroll back

**Expected Results:**
- [ ] Pressing Shift+Page Up scrolls back through history
- [ ] Can view previously scrolled-off content
- [ ] Cursor disappears while viewing scrollback
- [ ] Shift+Page Down returns to current view
- [ ] Cursor reappears when back to current view

---

## Test 11: Scrollback with Mouse Wheel
**Steps:**
1. After long output, scroll mouse wheel up
2. Scroll mouse wheel down

**Expected Results:**
- [ ] Mouse wheel up scrolls back through history
- [ ] Mouse wheel down scrolls forward
- [ ] Scrolling is smooth (3 lines per notch)
- [ ] Cursor hidden while scrolled back

---

## Test 12: Special Keys
**Steps:**
Test each key and observe behavior:
1. Press `Up Arrow` - should recall previous command
2. Press `Down Arrow` - should navigate command history
3. Press `Left/Right Arrow` - should move cursor in command line
4. Press `Home` - should move to start of line
5. Press `End` - should move to end of line
6. Press `Backspace` - should delete character
7. Press `Delete` - should delete character ahead
8. Press `Tab` - should auto-complete (if supported by cmd.exe)

**Expected Results:**
- [ ] All arrow keys work correctly
- [ ] Home/End keys work
- [ ] Backspace deletes character
- [ ] Delete key works
- [ ] Tab attempts completion

---

## Test 13: Text Wrapping
**Steps:**
1. Type a very long line (more than 80 characters)
2. Press Enter

**Expected Results:**
- [ ] Long lines wrap to next line
- [ ] Text remains readable
- [ ] Cursor positioning is correct

---

## Test 14: Multiple Lines of Output
**Steps:**
1. Type: `echo Line1 & echo Line2 & echo Line3`
2. Press Enter

**Expected Results:**
- [ ] Each line appears on separate row
- [ ] Lines are properly spaced
- [ ] All text is visible

---

## Test 15: Window Resize
**Steps:**
1. Drag window edge to resize window
2. Make it smaller, then larger

**Expected Results:**
- [ ] Window resizes smoothly
- [ ] Text remains visible during resize
- [ ] No crashes or visual glitches

---

## Test 16: Copy/Paste (if implemented)
**Steps:**
1. Try to select text with mouse
2. Try to copy/paste

**Expected Results:**
- [ ] Can select text (if implemented)
- [ ] Can copy selected text (if implemented)
- [ ] Can paste from clipboard (if implemented)

---

## Test 17: Extended Session
**Steps:**
1. Use the terminal for 5-10 minutes
2. Run various commands
3. Generate lots of output

**Expected Results:**
- [ ] Terminal remains stable
- [ ] No memory leaks evident
- [ ] Performance stays consistent
- [ ] Scrollback continues to work

---

## Test 18: Exit and Restart
**Steps:**
1. Type: `exit`
2. Press Enter
3. Or close window with X button

**Expected Results:**
- [ ] Terminal closes cleanly
- [ ] No crash dialogs
- [ ] Process terminates fully

---

## Test 19: Tab Management
**Steps:**
1. Press `Ctrl+T` to create a new tab
2. Press `Ctrl+T` again to create another tab
3. Click on different tabs to switch between them
4. Press `Ctrl+1`, `Ctrl+2`, `Ctrl+3` to switch tabs by number
5. Press `Ctrl+W` to close the current tab

**Expected Results:**
- [ ] New tabs open with fresh PowerShell sessions
- [ ] Tab bar shows all open tabs
- [ ] Clicking tabs switches between sessions
- [ ] Ctrl+number shortcuts switch tabs correctly
- [ ] Closing last tab closes the window

---

## Test 20: Split Panes
**Steps:**
1. Press `Ctrl+Shift+D` for vertical split (side-by-side)
2. Press `Ctrl+Shift+E` for horizontal split (top/bottom)
3. Press `Alt+Arrow` keys to navigate between panes
4. Click on a pane to focus it
5. Press `Ctrl+Shift+W` to close current pane

**Expected Results:**
- [ ] Vertical split creates side-by-side panes
- [ ] Horizontal split creates top/bottom panes
- [ ] Alt+Arrow navigates between panes
- [ ] Clicking focuses the clicked pane
- [ ] Closing pane redistributes space

---

## Test 21: In-Terminal Search
**Steps:**
1. Generate some output (e.g., `Get-ChildItem -Recurse`)
2. Press `Ctrl+Shift+F` to open search
3. Type a search term
4. Press `Enter` or `F3` to find next match
5. Press `Shift+F3` to find previous match
6. Press `Escape` to close search

**Expected Results:**
- [ ] Search bar appears at top or bottom
- [ ] Matches are highlighted in the terminal
- [ ] Next/previous navigation works
- [ ] Search closes with Escape

---

## Test 22: Pane Zoom
**Steps:**
1. Create a split pane (Ctrl+Shift+D or Ctrl+Shift+E)
2. Press `Ctrl+Shift+Z` to zoom current pane
3. Press `Ctrl+Shift+Z` again to unzoom

**Expected Results:**
- [ ] Zoomed pane fills entire terminal area
- [ ] Other panes are hidden while zoomed
- [ ] Unzoom restores original pane layout
- [ ] Focus remains on the same pane

---

## Test 23: Settings Dialog - Open/Close
**Steps:**
1. Press `Ctrl+,` (Ctrl+Comma) to open settings
2. Verify the dialog appears with three tabs: General, Appearance, Terminal
3. Press `Escape` or click Cancel

**Expected Results:**
- [ ] Settings dialog opens as modal (blocks main window)
- [ ] Three tabs are visible and clickable
- [ ] Dialog closes on Escape without saving
- [ ] Main window becomes responsive again after close

---

## Test 24: Settings Dialog - Font Settings
**Steps:**
1. Open settings with `Ctrl+,`
2. Click the "Appearance" tab
3. Select a different font from the dropdown (e.g., "Courier New")
4. Change font size to 14
5. Click Apply

**Expected Results:**
- [ ] Font dropdown shows only monospace fonts
- [ ] Font dropdown defaults to current font (Consolas)
- [ ] Font size input validates range 6-72
- [ ] After Apply, terminal text updates to new font
- [ ] Font change persists after closing dialog

---

## Test 25: Settings Dialog - Color Settings
**Steps:**
1. Open settings with `Ctrl+,`
2. Click the "Appearance" tab
3. Click the "Background" color swatch button
4. Select a dark blue color in the color picker
5. Click OK on color picker
6. Click Apply

**Expected Results:**
- [ ] Color swatch buttons show current colors
- [ ] Clicking swatch opens Windows color picker (ChooseColorW)
- [ ] Color picker shows custom color palette
- [ ] After Apply, terminal background changes to selected color
- [ ] Color swatches update to show new selection

---

## Test 26: Settings Dialog - Terminal Settings
**Steps:**
1. Open settings with `Ctrl+,`
2. Click the "Terminal" tab
3. Change cursor style to "Bar" using radio buttons
4. Uncheck "Cursor Blink" checkbox
5. Adjust opacity slider to 80%
6. Click Apply

**Expected Results:**
- [ ] Cursor style radio buttons work (Block, Underline, Bar)
- [ ] Cursor blink checkbox toggles cursor blinking
- [ ] Opacity slider changes terminal transparency (if supported)
- [ ] Changes apply immediately after clicking Apply

---

## Test 27: Settings Dialog - General Settings
**Steps:**
1. Open settings with `Ctrl+,`
2. On the "General" tab, modify:
   - Shell path (e.g., change to cmd.exe)
   - Working directory
   - Scrollback lines (e.g., 20000)
3. Click Apply
4. Open a new tab (Ctrl+T)

**Expected Results:**
- [ ] Shell path edit box shows current shell
- [ ] Working directory can be set
- [ ] Scrollback lines validates range 100-100,000
- [ ] New tabs use the updated shell path
- [ ] Settings persist after restart

---

## Test 28: Settings Dialog - Reset to Defaults
**Steps:**
1. Open settings with `Ctrl+,`
2. Change several settings
3. Click "Reset to Defaults" button
4. Confirm the reset when prompted

**Expected Results:**
- [ ] Confirmation dialog appears before reset
- [ ] All fields revert to default values
- [ ] Font resets to Consolas 16pt
- [ ] Colors reset to default scheme
- [ ] Cursor resets to Block with blink enabled

---

## Summary Checklist

**Core Functionality:**
- [ ] Terminal starts successfully
- [ ] Keyboard input works
- [ ] Text renders correctly
- [ ] Cursor is visible and blinks

**VT100/ANSI Support:**
- [ ] Foreground colors (16 colors)
- [ ] Background colors (16 colors)
- [ ] Bold attribute
- [ ] Underline attribute
- [ ] Clear screen

**Advanced Features:**
- [ ] Scrollback with Shift+Page Up/Down
- [ ] Scrollback with mouse wheel
- [ ] Window resize
- [ ] Long sessions are stable
- [ ] Tab management (Ctrl+T, Ctrl+W)
- [ ] Split panes (Ctrl+Shift+D/E)
- [ ] In-terminal search (Ctrl+Shift+F)
- [ ] Pane zoom (Ctrl+Shift+Z)
- [ ] Settings dialog (Ctrl+,)
- [ ] Font selection and size
- [ ] Color customization
- [ ] Cursor style settings
- [ ] Reset to defaults

**Performance:**
- [ ] Renders quickly
- [ ] No lag when typing
- [ ] Smooth scrolling
- [ ] No memory leaks

---

## Notes Section

Record any issues or observations:

```
Date: ___________
Tester: ___________

Issues Found:
-
-
-

Observations:
-
-
-
```

---

## Test Environment

- OS: Windows _______
- Display Resolution: _______
- DPI Scaling: _______
- GPU: _______
- Build Date: _______
