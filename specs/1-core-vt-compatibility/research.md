# Technical Research: Core VT Compatibility

**Feature**: Core VT Compatibility  
**Date**: 2025-12-21

## Decision 1: True Color Storage

**Question**: How should we store 24-bit RGB colors alongside the existing 16-color palette?

**Decision**: Extend CellAttributes with optional RGB fields and a flag.

**Rationale**:
- Backwards compatible - existing 16-color code continues to work
- Memory efficient - RGB only allocated when used
- Simple shader update - check flag, use RGB or palette lookup

**Alternatives Considered**:
1. **Replace palette with RGB everywhere**: Rejected - breaks existing rendering, increases memory for simple colors
2. **Use union for color data**: Rejected - complicates code, C++ union semantics with non-trivial types
3. **Separate color structure**: Rejected - adds indirection, cache unfriendly

**Implementation**:
```cpp
struct CellAttributes {
    // Existing fields...
    uint8_t foreground;  // 0-255 palette index (kept for compatibility)
    uint8_t background;
    uint8_t flags;
    
    // New fields for true color
    uint8_t fgR, fgG, fgB;
    uint8_t bgR, bgG, bgB;
    
    // New flags
    static constexpr uint8_t FLAG_DIM           = 0x10;
    static constexpr uint8_t FLAG_STRIKETHROUGH = 0x20;
    static constexpr uint8_t FLAG_TRUECOLOR_FG  = 0x40;
    static constexpr uint8_t FLAG_TRUECOLOR_BG  = 0x80;
};
```

---

## Decision 2: Scroll Region Implementation

**Question**: How should scroll regions interact with the existing ScrollUp/ScrollDown methods?

**Decision**: Add region-aware scroll methods and store region bounds in ScreenBuffer.

**Rationale**:
- Clear separation between global scroll (scrollback) and region scroll (display only)
- Region scrolling never affects scrollback buffer
- Cursor movements respect region boundaries when origin mode enabled

**Alternatives Considered**:
1. **Modify existing ScrollUp/Down**: Rejected - breaks scrollback behavior
2. **Create RegionBuffer subclass**: Rejected - over-engineering, tight coupling issues
3. **Store region in VTStateMachine**: Rejected - scroll region is screen buffer state

**Implementation**:
```cpp
class ScreenBuffer {
    int m_scrollTop = 0;
    int m_scrollBottom = -1;  // -1 means m_rows - 1
    
    void SetScrollRegion(int top, int bottom);
    void ScrollRegionUp(int lines = 1);   // Scroll within region only
    void ScrollRegionDown(int lines = 1);
    bool IsInScrollRegion(int row) const;
};
```

---

## Decision 3: Cursor Save/Restore State

**Question**: What state should DECSC/DECRC save and restore?

**Decision**: Save position, attributes, and mode flags per xterm specification.

**Rationale**:
- xterm is the de facto standard for VT compatibility
- Applications like vim depend on exact state restoration
- CSI s/u only save position; DECSC/DECRC save full state

**Saved State (DECSC/DECRC)**:
- Cursor position (row, col)
- Text attributes (foreground, background, flags)
- Origin mode (DECOM)
- Auto-wrap mode (DECAWM)
- Character set (G0/G1) - future enhancement

**Saved State (CSI s/u)**:
- Cursor position only

**Implementation**:
```cpp
struct SavedCursorState {
    int x, y;
    CellAttributes attr;
    bool originMode;
    bool autoWrap;
};
```

---

## Decision 4: Device Status Response Mechanism

**Question**: How should the terminal send response sequences back to the application?

**Decision**: Add response callback to VTStateMachine that writes to ConPTY input.

**Rationale**:
- Responses go through same pipe as user input
- ConPTY handles proper sequencing
- Clean separation of concerns

**Alternatives Considered**:
1. **Direct write to PTY in HandleXxx**: Rejected - VTStateMachine shouldn't know about PTY
2. **Queue responses for batch send**: Rejected - responses need immediate delivery
3. **Return value from Process**: Rejected - complex API, doesn't fit event model

**Implementation**:
```cpp
class VTStateMachine {
    std::function<void(const char*, size_t)> m_responseCallback;
    
    void SetResponseCallback(std::function<void(const char*, size_t)> cb);
    void SendResponse(const std::string& response);
};

// In Application.cpp
vtMachine.SetResponseCallback([&pty](const char* data, size_t len) {
    pty.Write(data, len);
});
```

---

## Decision 5: Dim Text Rendering

**Question**: How should dim text (SGR 2) be rendered?

**Decision**: Apply 50% intensity multiplier in the pixel shader.

**Rationale**:
- GPU-efficient - no CPU color calculation
- Consistent with existing rendering pipeline
- Works with both palette and true colors

**Alternatives Considered**:
1. **Pre-calculate dim colors in CPU**: Rejected - doubles color storage complexity
2. **Use alpha blending**: Rejected - dim isn't transparency, affects perception differently
3. **Separate dim palette**: Rejected - doesn't work with true color

**Implementation**:
```hlsl
// GlyphPixel.hlsl
float dimFactor = (flags & FLAG_DIM) ? 0.5 : 1.0;
output.color = textColor * dimFactor;
```

---

## Decision 6: Application Cursor Keys Mode

**Question**: How should DECCKM mode affect key handling?

**Decision**: Store mode state in VTStateMachine, Application queries mode when sending key sequences.

**Rationale**:
- Mode state belongs with VT parser
- Key handling in Application needs read access
- Clean query interface

**Implementation**:
```cpp
class VTStateMachine {
    bool m_applicationCursorKeys = false;  // DECCKM
    
    bool IsApplicationCursorKeysEnabled() const;
};

// In Application key handler:
if (vtMachine.IsApplicationCursorKeysEnabled()) {
    // Send ESC O A instead of ESC [ A
}
```

---

## Reference: xterm Control Sequences

Primary reference: https://invisible-island.net/xterm/ctlseqs/ctlseqs.html

Key sequences for this implementation:
- CSI Ps n: DSR (Device Status Report)
- CSI ? Ps h/l: DECSET/DECRST (Private Mode Set/Reset)
- CSI Ps ; Ps r: DECSTBM (Set Scrolling Region)
- ESC 7 / ESC 8: DECSC/DECRC (Save/Restore Cursor)
- CSI s / CSI u: SCP/RCP (Save/Restore Cursor Position)
