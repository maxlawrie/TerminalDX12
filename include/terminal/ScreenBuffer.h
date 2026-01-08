#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>

namespace TerminalDX12::Terminal {

// OSC 133 shell integration semantic zones
enum class SemanticZone {
    None,       // Regular output
    Prompt,     // OSC 133;A - Prompt region
    Input,      // OSC 133;B - User input region
    Output      // OSC 133;C/D - Command output region
};

// Prompt marker for navigation
struct PromptMarker {
    int absoluteLine;    // Absolute line number (including scrollback)
    int exitCode = -1;   // Exit code from OSC 133;D;N (-1 if not set)
};

// Hyperlink data (OSC 8)
struct Hyperlink {
    std::string url;     // The URL/URI
    std::string id;      // Optional ID for link grouping
};

// Color palette entry (OSC 4)
struct PaletteColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    bool modified = false;  // True if this entry was set via OSC 4
};

// Cell attributes (SGR - Select Graphic Rendition)
struct CellAttributes {
    uint8_t foreground;      // Foreground color (0-255 palette index)
    uint8_t background;      // Background color (0-255 palette index)
    uint8_t flags;           // Bold, underline, etc.
    uint8_t flags2;          // Additional flags (blink, hidden)

    // True color RGB (used when FLAG_TRUECOLOR_FG/BG is set)
    uint8_t fgR, fgG, fgB;   // Foreground RGB
    uint8_t bgR, bgG, bgB;   // Background RGB

    // Flags (flags byte)
    static constexpr uint8_t FLAG_BOLD          = 0x01;
    static constexpr uint8_t FLAG_ITALIC        = 0x02;
    static constexpr uint8_t FLAG_UNDERLINE     = 0x04;
    static constexpr uint8_t FLAG_INVERSE       = 0x08;
    static constexpr uint8_t FLAG_DIM           = 0x10;  // SGR 2 - Dim/Faint
    static constexpr uint8_t FLAG_STRIKETHROUGH = 0x20;  // SGR 9 - Strikethrough
    static constexpr uint8_t FLAG_TRUECOLOR_FG  = 0x40;  // Use fgR/fgG/fgB instead of palette
    static constexpr uint8_t FLAG_TRUECOLOR_BG  = 0x80;  // Use bgR/bgG/bgB instead of palette

    // Flags2 (flags2 byte)
    static constexpr uint8_t FLAG2_BLINK        = 0x01;  // SGR 5/6 - Blink
    static constexpr uint8_t FLAG2_HIDDEN       = 0x02;  // SGR 8 - Hidden/Invisible
    static constexpr uint8_t FLAG2_HYPERLINK    = 0x04;  // OSC 8 - Cell has hyperlink

    // Underline styles (SGR 4, 21, 4:0-4:5)
    enum class UnderlineStyle : uint8_t {
        None = 0,
        Single = 1,
        Double = 2,
        Curly = 3,
        Dotted = 4,
        Dashed = 5
    };
    UnderlineStyle underlineStyle = UnderlineStyle::None;

    // Hyperlink ID (index into ScreenBuffer's hyperlink table, 0 = no link)
    uint16_t hyperlinkId = 0;

    CellAttributes()
        : foreground(7)      // White
        , background(0)      // Black
        , flags(0)
        , flags2(0)
        , fgR(0), fgG(0), fgB(0)
        , bgR(0), bgG(0), bgB(0)
    {}

    // Attribute queries
    bool IsBold() const { return (flags & FLAG_BOLD) != 0; }
    bool IsItalic() const { return (flags & FLAG_ITALIC) != 0; }
    bool IsUnderline() const { return underlineStyle != UnderlineStyle::None || (flags & FLAG_UNDERLINE) != 0; }
    bool IsDoubleUnderline() const { return underlineStyle == UnderlineStyle::Double; }
    UnderlineStyle GetUnderlineStyle() const { return underlineStyle; }
    bool IsInverse() const { return (flags & FLAG_INVERSE) != 0; }
    bool IsDim() const { return (flags & FLAG_DIM) != 0; }
    bool IsStrikethrough() const { return (flags & FLAG_STRIKETHROUGH) != 0; }
    bool UsesTrueColorFg() const { return (flags & FLAG_TRUECOLOR_FG) != 0; }
    bool UsesTrueColorBg() const { return (flags & FLAG_TRUECOLOR_BG) != 0; }
    bool IsBlink() const { return (flags2 & FLAG2_BLINK) != 0; }
    bool IsHidden() const { return (flags2 & FLAG2_HIDDEN) != 0; }
    bool HasHyperlink() const { return (flags2 & FLAG2_HYPERLINK) != 0; }

    // Set foreground to true RGB color
    void SetForegroundRGB(uint8_t r, uint8_t g, uint8_t b) {
        fgR = r; fgG = g; fgB = b;
        flags |= FLAG_TRUECOLOR_FG;
    }

    // Set background to true RGB color
    void SetBackgroundRGB(uint8_t r, uint8_t g, uint8_t b) {
        bgR = r; bgG = g; bgB = b;
        flags |= FLAG_TRUECOLOR_BG;
    }

    // Set foreground to palette color (clears true color flag)
    void SetForegroundPalette(uint8_t index) {
        foreground = index;
        flags &= ~FLAG_TRUECOLOR_FG;
    }

    // Set background to palette color (clears true color flag)
    void SetBackgroundPalette(uint8_t index) {
        background = index;
        flags &= ~FLAG_TRUECOLOR_BG;
    }
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

    // Thread-safe dimension access - use this when iterating
    void GetDimensions(int& cols, int& rows) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        cols = m_cols;
        rows = m_rows;
    }

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

    // Scroll region support (DECSTBM)
    void SetScrollRegion(int top, int bottom);
    void ResetScrollRegion();
    int GetScrollRegionTop() const { return m_scrollTop; }
    int GetScrollRegionBottom() const;
    void ScrollRegionUp(int lines = 1);
    void ScrollRegionDown(int lines = 1);

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
    // Returns by value to prevent dangling references during resize
    Cell GetCellWithScrollback(int x, int y) const;

    // Get visible buffer (accounts for scrollback)
    const std::vector<Cell>& GetBuffer() const { return m_buffer; }

    // Dirty tracking
    bool IsDirty() const { return m_dirty; }
    void ClearDirty() { m_dirty = false; }
    void MarkDirty() { m_dirty = true; }

    // Alternative screen buffer (for apps like vim)
    void UseAlternativeBuffer(bool use);
    bool IsUsingAlternativeBuffer() const { return m_useAltBuffer; }

    // OSC 133 shell integration
    void MarkPromptStart();
    void MarkInputStart();
    void MarkCommandStart();
    void MarkCommandEnd(int exitCode = -1);
    SemanticZone GetCurrentZone() const { return m_currentZone; }

    // Prompt navigation
    int GetPreviousPromptLine(int fromLine) const;
    int GetNextPromptLine(int fromLine) const;
    const std::vector<PromptMarker>& GetPromptMarkers() const { return m_promptMarkers; }

    // Tab stop management
    void SetTabStop(int column);           // HTS - Set tab stop at column
    void ClearTabStop(int column);         // TBC 0 - Clear tab stop at column
    void ClearAllTabStops();               // TBC 3 - Clear all tab stops
    void ResetTabStops();                  // Reset to default (every 8 columns)

    // Hyperlink management (OSC 8)
    uint16_t AddHyperlink(const std::string& url, const std::string& id = "");
    void ClearCurrentHyperlink();
    const Hyperlink* GetHyperlink(uint16_t id) const;
    uint16_t GetCurrentHyperlinkId() const { return m_currentHyperlinkId; }

    // Palette management (OSC 4)
    void SetPaletteColor(int index, uint8_t r, uint8_t g, uint8_t b);
    const PaletteColor& GetPaletteColor(int index) const;
    bool IsPaletteColorModified(int index) const;
    void ResetPaletteColor(int index);
    void ResetAllPaletteColors();

private:
    void NewLine();
    void CarriageReturn();
    void Tab();
    void Backspace();
    void ClearLineInternal(int y, int startX, int endX);  // Internal - caller must hold mutex

    int CellIndex(int x, int y) const { return y * m_cols + x; }
    int ValidCellIndex(int x, int y) const {
        if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) return -1;
        int idx = CellIndex(x, y);
        return (idx >= 0 && idx < static_cast<int>(m_buffer.size())) ? idx : -1;
    }
    void ClampCursor();

    // Primary buffer
    int m_cols;
    int m_rows;
    std::vector<Cell> m_buffer;           // Current visible buffer
    std::vector<bool> m_lineWrapped;      // True if line is soft-wrapped (continuation of previous)
    std::vector<Cell> m_scrollback;       // Lines scrolled off top
    std::vector<bool> m_scrollbackWrapped; // Wrap flags for scrollback lines
    int m_scrollbackLines;                // Max scrollback lines
    int m_scrollbackUsed;                 // Current scrollback usage
    int m_scrollOffset;                   // Scrollback view offset

    // Scroll region (for DECSTBM)
    int m_scrollTop;                      // Top of scroll region (0-indexed)
    int m_scrollBottom;                   // Bottom of scroll region (-1 = use m_rows - 1)

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

    // OSC 133 shell integration
    SemanticZone m_currentZone = SemanticZone::None;
    std::vector<PromptMarker> m_promptMarkers;

    // Tab stops (columns where tab stops are set)
    std::vector<bool> m_tabStops;

    // Hyperlinks (OSC 8)
    std::unordered_map<uint16_t, Hyperlink> m_hyperlinks;
    uint16_t m_nextHyperlinkId = 1;       // Next ID to assign (0 = no link)
    uint16_t m_currentHyperlinkId = 0;    // Currently active hyperlink (for new cells)

    // Color palette (OSC 4) - 256 colors
    PaletteColor m_palette[256];

    // Thread safety for buffer access
    mutable std::recursive_mutex m_mutex;
};

} // namespace TerminalDX12::Terminal
