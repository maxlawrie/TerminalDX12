# How to Add a VT Escape Sequence

This guide walks through adding support for a new VT/ANSI escape sequence to TerminalDX12.

## Overview

VT escape sequences are parsed by `VTStateMachine` and affect the `ScreenBuffer`. The process involves:

1. Understanding the sequence format
2. Adding the parser case
3. Implementing the action in ScreenBuffer
4. Testing the implementation

## Escape Sequence Basics

### Sequence Types

| Type | Format | Example | Description |
|------|--------|---------|-------------|
| CSI | `ESC [ ... <final>` | `ESC[31m` | Control Sequence Introducer |
| OSC | `ESC ] ... ST` | `ESC]0;title^G` | Operating System Command |
| DCS | `ESC P ... ST` | | Device Control String |
| Simple | `ESC <char>` | `ESC M` | Single-character sequences |

### Common Parameters

- `ESC` = `\x1b` or `\033`
- `ST` (String Terminator) = `ESC \` or `\x9c`
- `BEL` = `\x07`

## Example: Adding CSI Sequence

Let's add support for `CSI ? 1049 h` (switch to alternate screen buffer).

### Step 1: Find the Sequence Specification

Consult resources:
- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [ECMA-48 Standard](https://www.ecma-international.org/publications-and-standards/standards/ecma-48/)
- `SPECIFICATION.md` in this project

For our example:
- Sequence: `CSI ? 1049 h`
- Meaning: Save cursor, switch to alternate screen, clear alternate screen
- Reverse: `CSI ? 1049 l` - Switch back to main screen, restore cursor

### Step 2: Add Parser Case

Edit `src/terminal/VTStateMachine.cpp`:

```cpp
void VTStateMachine::HandleCSI(const std::vector<int>& params, char finalByte, bool isPrivate) {
    if (isPrivate) {
        // Private mode sequences (CSI ? ...)
        switch (finalByte) {
            case 'h':  // Set mode
                for (int param : params) {
                    switch (param) {
                        case 1049:
                            // NEW: Switch to alternate screen buffer
                            m_screenBuffer->SwitchToAlternateBuffer();
                            break;
                        // ... other cases
                    }
                }
                break;

            case 'l':  // Reset mode
                for (int param : params) {
                    switch (param) {
                        case 1049:
                            // NEW: Switch back to main buffer
                            m_screenBuffer->SwitchToMainBuffer();
                            break;
                        // ... other cases
                    }
                }
                break;
        }
    }
}
```

### Step 3: Implement ScreenBuffer Method

Add declaration to `include/terminal/ScreenBuffer.h`:

```cpp
class ScreenBuffer {
public:
    // ... existing methods ...

    /// Switch to alternate screen buffer (saves main buffer state)
    void SwitchToAlternateBuffer();

    /// Switch back to main screen buffer (restores state)
    void SwitchToMainBuffer();

private:
    // Storage for alternate buffer support
    std::vector<Cell> m_alternateBuffer;
    CursorState m_savedCursorMain;
    bool m_usingAlternateBuffer = false;
};
```

Add implementation to `src/terminal/ScreenBuffer.cpp`:

```cpp
void ScreenBuffer::SwitchToAlternateBuffer() {
    if (m_usingAlternateBuffer) return;  // Already in alternate

    // Save main buffer cursor
    m_savedCursorMain = m_cursor;

    // Save main buffer content and clear for alternate
    m_alternateBuffer = std::move(m_cells);
    m_cells.resize(m_width * m_height);
    std::fill(m_cells.begin(), m_cells.end(), Cell{});

    // Reset cursor for alternate buffer
    m_cursor = CursorState{};

    m_usingAlternateBuffer = true;
    MarkAllDirty();
}

void ScreenBuffer::SwitchToMainBuffer() {
    if (!m_usingAlternateBuffer) return;  // Already in main

    // Restore main buffer
    m_cells = std::move(m_alternateBuffer);

    // Restore saved cursor
    m_cursor = m_savedCursorMain;

    m_usingAlternateBuffer = false;
    MarkAllDirty();
}
```

### Step 4: Add Tests

Create or update test file `tests/test_vt_sequences.cpp`:

```cpp
TEST_CASE("Alternate screen buffer", "[vt][screen]") {
    ScreenBuffer buffer(80, 24);
    VTStateMachine vt(&buffer);

    // Write some content to main buffer
    vt.ProcessString("Hello Main\r\n");

    // Switch to alternate buffer
    vt.ProcessString("\x1b[?1049h");
    REQUIRE(buffer.IsAlternateBuffer() == true);

    // Write to alternate
    vt.ProcessString("Hello Alternate\r\n");

    // Switch back to main
    vt.ProcessString("\x1b[?1049l");
    REQUIRE(buffer.IsAlternateBuffer() == false);

    // Verify main content is preserved
    std::string content = buffer.GetLine(0);
    REQUIRE(content.find("Hello Main") != std::string::npos);
}
```

### Step 5: Update Documentation

Add to `SPECIFICATION.md`:

```markdown
### Alternate Screen Buffer

| Sequence | Action | Status |
|----------|--------|--------|
| CSI ? 1049 h | Switch to alternate screen buffer | COMPLETE |
| CSI ? 1049 l | Switch back to main screen buffer | COMPLETE |
```

## Testing Your Implementation

### Manual Testing

```powershell
# Test with vim (uses alternate buffer)
vim test.txt

# Test with less (uses alternate buffer)
Get-Content largefile.txt | less

# Direct escape sequence test
Write-Host "`e[?1049h" -NoNewline  # Switch to alt
Write-Host "Alternate buffer"
Start-Sleep 2
Write-Host "`e[?1049l" -NoNewline  # Switch back
```

### Automated Testing

```bash
# Run C++ tests
./build/tests/Release/TerminalDX12Tests.exe --reporter compact "[vt]"

# Run Python tests
python -m pytest tests/ -k "vt" -v
```

## Common Patterns

### SGR (Select Graphic Rendition) Attributes

Located in `HandleSGR()`:

```cpp
case 1: m_currentAttr.bold = true; break;
case 4: m_currentAttr.underline = true; break;
case 38: /* Foreground color - parse subparams */ break;
```

### Cursor Movement

Located in `HandleCSI()` with final byte mapping:

```cpp
case 'A': MoveCursorUp(param); break;
case 'B': MoveCursorDown(param); break;
case 'C': MoveCursorRight(param); break;
case 'D': MoveCursorLeft(param); break;
case 'H': SetCursorPosition(row, col); break;
```

### Mode Flags

Private modes (`CSI ? n h/l`) often toggle boolean flags:

```cpp
case 25: m_cursor.visible = (finalByte == 'h'); break;  // Cursor visibility
case 1000: m_mouseMode = (finalByte == 'h') ? MouseMode::Normal : MouseMode::None; break;
```

## Debugging Tips

1. **Enable debug logging**: Set `SPDLOG_LEVEL=debug`
2. **Log raw sequences**: Add logging in `VTStateMachine::ProcessByte()`
3. **Compare with xterm**: Test same sequence in xterm for reference
4. **Use vttest**: Run `vttest` utility to verify compliance

## Resources

- [XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html)
- [ECMA-48 Standard](https://www.ecma-international.org/publications-and-standards/standards/ecma-48/)
- [VT100 User Guide](https://vt100.net/docs/vt100-ug/)
- [Microsoft Console Virtual Terminal Sequences](https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences)
