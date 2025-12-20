#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace TerminalDX12::Terminal {

// Cell attributes (SGR - Select Graphic Rendition)
struct CellAttributes {
    uint8_t foreground;      // Foreground color (0-255)
    uint8_t background;      // Background color (0-255)
    uint8_t flags;           // Bold, underline, etc.

    // Flags
    static constexpr uint8_t FLAG_BOLD      = 0x01;
    static constexpr uint8_t FLAG_ITALIC    = 0x02;
    static constexpr uint8_t FLAG_UNDERLINE = 0x04;
    static constexpr uint8_t FLAG_INVERSE   = 0x08;

    CellAttributes()
        : foreground(7)      // White
        , background(0)      // Black
        , flags(0)
    {}

    bool IsBold() const { return (flags & FLAG_BOLD) != 0; }
    bool IsItalic() const { return (flags & FLAG_ITALIC) != 0; }
    bool IsUnderline() const { return (flags & FLAG_UNDERLINE) != 0; }
    bool IsInverse() const { return (flags & FLAG_INVERSE) != 0; }
};

// Single cell in the terminal grid
struct Cell {
    char32_t ch;             // Unicode codepoint
    CellAttributes attr;      // Text attributes

    Cell() : ch(U' ') {}
    Cell(char32_t c, const CellAttributes& a) : ch(c), attr(a) {}
};

// Terminal screen buffer with scrollback
class ScreenBuffer {
public:
    ScreenBuffer(int cols = 80, int rows = 24, int scrollbackLines = 10000);
    ~ScreenBuffer();

    // Size management
    void Resize(int cols, int rows);
    int GetCols() const { return m_cols; }
    int GetRows() const { return m_rows; }

    // Cursor operations
    void SetCursorPos(int x, int y);
    void GetCursorPos(int& x, int& y) const { x = m_cursorX; y = m_cursorY; }
    int GetCursorX() const { return m_cursorX; }
    int GetCursorY() const { return m_cursorY; }
    void SetCursorVisible(bool visible) { m_cursorVisible = visible; }
    bool IsCursorVisible() const { return m_cursorVisible; }

    // Write operations
    void WriteChar(char32_t ch);
    void WriteChar(char32_t ch, const CellAttributes& attr);
    void WriteString(const std::u32string& str);

    // Scrolling
    void ScrollUp(int lines = 1);
    void ScrollDown(int lines = 1);
    void ScrollToBottom();
    int GetScrollOffset() const { return m_scrollOffset; }
    void SetScrollOffset(int offset);

    // Clearing
    void Clear();
    void ClearLine(int y);
    void ClearLine(int y, int startX, int endX);
    void ClearRect(int x, int y, int width, int height);

    // Attributes
    void SetCurrentAttributes(const CellAttributes& attr) { m_currentAttr = attr; }
    const CellAttributes& GetCurrentAttributes() const { return m_currentAttr; }

    // Cell access
    const Cell& GetCell(int x, int y) const;
    Cell& GetCellMutable(int x, int y);

    // Get cell accounting for scrollback offset
    const Cell& GetCellWithScrollback(int x, int y) const;

    // Get visible buffer (accounts for scrollback)
    const std::vector<Cell>& GetBuffer() const { return m_buffer; }

    // Dirty tracking
    bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }
    void MarkDirty() { m_dirty = true; }

    // Alternative screen buffer (for apps like vim)
    void UseAlternativeBuffer(bool use);
    bool IsUsingAlternativeBuffer() const { return m_useAltBuffer; }

private:
    void NewLine();
    void CarriageReturn();
    void Tab();
    void Backspace();

    int CellIndex(int x, int y) const { return y * m_cols + x; }
    void ClampCursor();

    // Primary buffer
    int m_cols;
    int m_rows;
    std::vector<Cell> m_buffer;           // Current visible buffer
    std::vector<Cell> m_scrollback;       // Lines scrolled off top
    int m_scrollbackLines;                // Max scrollback lines
    int m_scrollbackUsed;                 // Current scrollback usage
    int m_scrollOffset;                   // Scrollback view offset

    // Alternative buffer (no scrollback)
    std::vector<Cell> m_altBuffer;
    bool m_useAltBuffer;

    // Cursor state
    int m_cursorX;
    int m_cursorY;
    bool m_cursorVisible;

    // Current text attributes
    CellAttributes m_currentAttr;

    // Dirty flag for rendering optimization
    bool m_dirty;
};

} // namespace TerminalDX12::Terminal
