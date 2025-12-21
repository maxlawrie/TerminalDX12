# Data Model: Core VT Compatibility

**Feature**: Core VT Compatibility  
**Date**: 2024-12-21

## Overview

This document defines the data structure changes required for Phase 1 VT compatibility.

## Entity: CellAttributes (Extended)

**Purpose**: Store text rendering attributes for a single terminal cell.

**Current Structure**:
```cpp
struct CellAttributes {
    uint8_t foreground;  // 0-255 palette index
    uint8_t background;  // 0-255 palette index
    uint8_t flags;       // Bold, italic, underline, inverse
};
// Size: 3 bytes
```

**Extended Structure**:
```cpp
struct CellAttributes {
    // Palette colors (legacy support)
    uint8_t foreground;      // 0-255 palette index
    uint8_t background;      // 0-255 palette index
    
    // Attribute flags (extended)
    uint8_t flags;
    
    // True color RGB (when FLAG_TRUECOLOR_* set)
    uint8_t fgR, fgG, fgB;
    uint8_t bgR, bgG, bgB;
    
    // Flag definitions
    static constexpr uint8_t FLAG_BOLD          = 0x01;
    static constexpr uint8_t FLAG_ITALIC        = 0x02;
    static constexpr uint8_t FLAG_UNDERLINE     = 0x04;
    static constexpr uint8_t FLAG_INVERSE       = 0x08;
    static constexpr uint8_t FLAG_DIM           = 0x10;  // NEW
    static constexpr uint8_t FLAG_STRIKETHROUGH = 0x20;  // NEW
    static constexpr uint8_t FLAG_TRUECOLOR_FG  = 0x40;  // NEW
    static constexpr uint8_t FLAG_TRUECOLOR_BG  = 0x80;  // NEW
    
    // Helper methods
    bool IsDim() const { return (flags & FLAG_DIM) != 0; }
    bool IsStrikethrough() const { return (flags & FLAG_STRIKETHROUGH) != 0; }
    bool UsesTrueColorFg() const { return (flags & FLAG_TRUECOLOR_FG) != 0; }
    bool UsesTrueColorBg() const { return (flags & FLAG_TRUECOLOR_BG) != 0; }
    
    // Get effective colors
    void GetForegroundRGB(uint8_t& r, uint8_t& g, uint8_t& b) const;
    void GetBackgroundRGB(uint8_t& r, uint8_t& g, uint8_t& b) const;
    
    // Set colors
    void SetForegroundRGB(uint8_t r, uint8_t g, uint8_t b);
    void SetBackgroundRGB(uint8_t r, uint8_t g, uint8_t b);
    void SetForegroundPalette(uint8_t index);
    void SetBackgroundPalette(uint8_t index);
};
// Size: 9 bytes (was 3)
```

**Validation Rules**:
- Palette indices 0-15 map to standard VGA colors
- Palette indices 16-255 map to extended 256-color palette
- RGB values 0-255 for each channel
- When FLAG_TRUECOLOR_* is set, use RGB fields; otherwise use palette

---

## Entity: SavedCursorState

**Purpose**: Store cursor state for DECSC/DECRC save/restore.

**Structure**:
```cpp
struct SavedCursorState {
    int x;                    // Cursor column (0-indexed)
    int y;                    // Cursor row (0-indexed)
    CellAttributes attr;      // Text attributes at save time
    bool originMode;          // DECOM state
    bool autoWrap;            // DECAWM state
    bool valid;               // Has state been saved?
    
    SavedCursorState() 
        : x(0), y(0), originMode(false), autoWrap(true), valid(false) {}
};
```

**State Transitions**:
- ESC 7 (DECSC): Save all fields, set valid=true
- ESC 8 (DECRC): Restore all fields if valid=true
- CSI s (SCP): Save x, y only (separate from DECSC)
- CSI u (RCP): Restore x, y only

---

## Entity: ScrollRegion

**Purpose**: Define scrolling boundaries within the screen buffer.

**Structure** (embedded in ScreenBuffer):
```cpp
class ScreenBuffer {
    // Scroll region bounds (1-indexed to match VT convention, convert internally)
    int m_scrollTop;      // First row of region (0-indexed internally)
    int m_scrollBottom;   // Last row of region (0-indexed internally, -1 = last row)
    
    // Methods
    void SetScrollRegion(int top, int bottom);  // 1-indexed input
    void ResetScrollRegion();                   // Reset to full screen
    int GetScrollTop() const;
    int GetScrollBottom() const;
    bool IsInScrollRegion(int row) const;
};
```

**Validation Rules**:
- top must be >= 1 and <= rows
- bottom must be >= top and <= rows
- If invalid parameters, reset to full screen
- Scroll operations only affect rows within region

---

## Entity: TerminalModes

**Purpose**: Track VT private and ANSI mode states.

**Structure** (embedded in VTStateMachine):
```cpp
class VTStateMachine {
    // Private modes (DECSET/DECRST)
    bool m_applicationCursorKeys;  // Mode 1 (DECCKM)
    bool m_autoWrap;               // Mode 7 (DECAWM)
    bool m_cursorVisible;          // Mode 25 (DECTCEM)
    bool m_bracketedPaste;         // Mode 2004
    bool m_altScreenActive;        // Mode 1049 (already partial)
    
    // Saved cursor states
    SavedCursorState m_savedCursor;       // For DECSC/DECRC
    SavedCursorState m_savedCursorCSI;    // For CSI s/u (separate)
    
    // Mode accessors
    bool IsApplicationCursorKeysEnabled() const;
    bool IsAutoWrapEnabled() const;
    bool IsCursorVisible() const;
    bool IsBracketedPasteEnabled() const;
    
    // Mode setters (called by HandleMode)
    void SetPrivateMode(int mode, bool enable);
};
```

**Mode Mapping**:
| Mode | Name | Default | Description |
|------|------|---------|-------------|
| 1 | DECCKM | OFF | Application cursor key mode |
| 7 | DECAWM | ON | Auto-wrap at right margin |
| 25 | DECTCEM | ON | Cursor visible |
| 2004 | Bracketed Paste | OFF | Wrap paste in ESC sequences |

---

## Entity: DeviceResponse

**Purpose**: Define response sequences for device queries.

**Responses**:
```cpp
// Primary Device Attributes (CSI c)
// Response: CSI ? 1 ; 2 c (VT100 with Advanced Video Option)
constexpr const char* DA_RESPONSE = "\x1b[?1;2c";

// Device Status Report (CSI 5 n)
// Response: CSI 0 n (Ready, no malfunction)
constexpr const char* DSR_OK_RESPONSE = "\x1b[0n";

// Cursor Position Report (CSI 6 n)
// Response: CSI row ; col R (1-indexed)
// Generated dynamically based on cursor position
std::string GetCPR(int row, int col) {
    return "\x1b[" + std::to_string(row + 1) + ";" + std::to_string(col + 1) + "R";
}
```

---

## Memory Impact

**Current Cell size**: `char32_t (4) + CellAttributes (3) = 7 bytes`  
**New Cell size**: `char32_t (4) + CellAttributes (9) = 13 bytes`

**Buffer Impact** (80x24 + 10000 scrollback):
- Current: `(80 * 24 + 80 * 10000) * 7 = ~5.6 MB`
- New: `(80 * 24 + 80 * 10000) * 13 = ~10.4 MB`

**Conclusion**: Still well under the 100MB constraint from constitution.

---

## Relationships

```
ScreenBuffer 1──* Cell
Cell 1──1 CellAttributes

VTStateMachine 1──1 ScreenBuffer
VTStateMachine 1──1 SavedCursorState (DECSC)
VTStateMachine 1──1 SavedCursorState (CSI s)

VTStateMachine --> Application (response callback)
Application --> ConPtySession (write response)
```
