#include "terminal/ScreenBuffer.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace TerminalDX12::Terminal {

ScreenBuffer::ScreenBuffer(int cols, int rows, int scrollbackLines)
    : m_cols(cols)
    , m_rows(rows)
    , m_scrollbackLines(scrollbackLines)
    , m_scrollbackUsed(0)
    , m_scrollOffset(0)
    , m_scrollTop(0)
    , m_scrollBottom(-1)
    , m_useAltBuffer(false)
    , m_cursorX(0)
    , m_cursorY(0)
    , m_cursorVisible(true)
    , m_dirty(true)
{
    m_buffer.resize(m_cols * m_rows);
    m_scrollback.reserve(m_scrollbackLines);

    spdlog::info("ScreenBuffer created: {}x{} with {} scrollback lines",
                 m_cols, m_rows, m_scrollbackLines);
}

ScreenBuffer::~ScreenBuffer() {
}

void ScreenBuffer::Resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) {
        return;
    }

    spdlog::info("Resizing screen buffer from {}x{} to {}x{}",
                 m_cols, m_rows, cols, rows);

    // Create new buffer
    std::vector<Cell> newBuffer(cols * rows);

    // Copy existing content (line by line)
    int copyRows = std::min(m_rows, rows);
    int copyCols = std::min(m_cols, cols);

    for (int y = 0; y < copyRows; ++y) {
        for (int x = 0; x < copyCols; ++x) {
            int oldIdx = y * m_cols + x;
            int newIdx = y * cols + x;
            newBuffer[newIdx] = m_buffer[oldIdx];
        }
    }

    m_buffer = std::move(newBuffer);
    m_cols = cols;
    m_rows = rows;

    // Clamp cursor to new size
    ClampCursor();

    // Resize alternative buffer too
    if (!m_altBuffer.empty()) {
        m_altBuffer.resize(cols * rows);
    }

    m_dirty = true;
}

void ScreenBuffer::SetCursorPos(int x, int y) {
    m_cursorX = std::clamp(x, 0, m_cols - 1);
    m_cursorY = std::clamp(y, 0, m_rows - 1);
    m_dirty = true;
}

void ScreenBuffer::ClampCursor() {
    m_cursorX = std::clamp(m_cursorX, 0, m_cols - 1);
    m_cursorY = std::clamp(m_cursorY, 0, m_rows - 1);
}

void ScreenBuffer::WriteChar(char32_t ch) {
    WriteChar(ch, m_currentAttr);
}

void ScreenBuffer::WriteChar(char32_t ch, const CellAttributes& attr) {
    // Handle control characters
    switch (ch) {
        case U'\n':
            NewLine();
            return;
        case U'\r':
            CarriageReturn();
            return;
        case U'\t':
            Tab();
            return;
        case U'\b':
            Backspace();
            return;
    }

    // Write character at cursor position
    if (m_cursorY >= 0 && m_cursorY < m_rows &&
        m_cursorX >= 0 && m_cursorX < m_cols) {
        int idx = CellIndex(m_cursorX, m_cursorY);
        m_buffer[idx].ch = ch;
        m_buffer[idx].attr = attr;
        m_dirty = true;
    }

    // Advance cursor
    m_cursorX++;

    // Wrap to next line if needed
    if (m_cursorX >= m_cols) {
        m_cursorX = 0;
        m_cursorY++;

        // Scroll if at bottom
        if (m_cursorY >= m_rows) {
            ScrollUp(1);
            m_cursorY = m_rows - 1;
        }
    }
}

void ScreenBuffer::WriteString(const std::u32string& str) {
    for (char32_t ch : str) {
        WriteChar(ch);
    }
}

void ScreenBuffer::ScrollUp(int lines) {
    if (lines <= 0) return;

    lines = std::min(lines, m_rows);

    // Don't add to scrollback if using alt buffer
    if (!m_useAltBuffer) {
        // Move top lines to scrollback
        for (int i = 0; i < lines; ++i) {
            if (m_scrollbackUsed < m_scrollbackLines) {
                // Add to end of scrollback
                for (int x = 0; x < m_cols; ++x) {
                    m_scrollback.push_back(m_buffer[i * m_cols + x]);
                }
                m_scrollbackUsed++;
            } else {
                // Scrollback full, shift and replace last line
                std::move(m_scrollback.begin() + m_cols,
                         m_scrollback.end(),
                         m_scrollback.begin());

                // Replace last line
                for (int x = 0; x < m_cols; ++x) {
                    m_scrollback[(m_scrollbackUsed - 1) * m_cols + x] =
                        m_buffer[i * m_cols + x];
                }
            }
        }
    }

    // Shift buffer up
    std::move(m_buffer.begin() + lines * m_cols,
             m_buffer.end(),
             m_buffer.begin());

    // Clear bottom lines
    std::fill(m_buffer.end() - lines * m_cols,
             m_buffer.end(),
             Cell());

    m_dirty = true;
}

void ScreenBuffer::ScrollDown(int lines) {
    if (lines <= 0) return;

    lines = std::min(lines, m_rows);

    // Shift buffer down
    std::move_backward(m_buffer.begin(),
                      m_buffer.end() - lines * m_cols,
                      m_buffer.end());

    // Clear top lines
    std::fill(m_buffer.begin(),
             m_buffer.begin() + lines * m_cols,
             Cell());

    m_dirty = true;
}

void ScreenBuffer::ScrollToBottom() {
    m_scrollOffset = 0;
    m_dirty = true;
}

void ScreenBuffer::SetScrollOffset(int offset) {
    m_scrollOffset = std::clamp(offset, 0, m_scrollbackUsed);
    m_dirty = true;
}

void ScreenBuffer::Clear() {
    std::fill(m_buffer.begin(), m_buffer.end(), Cell());
    m_cursorX = 0;
    m_cursorY = 0;
    m_dirty = true;
}

void ScreenBuffer::ClearLine(int y) {
    if (y < 0 || y >= m_rows) return;

    ClearLine(y, 0, m_cols - 1);
}

void ScreenBuffer::ClearLine(int y, int startX, int endX) {
    if (y < 0 || y >= m_rows) return;

    startX = std::clamp(startX, 0, m_cols - 1);
    endX = std::clamp(endX, 0, m_cols - 1);

    for (int x = startX; x <= endX; ++x) {
        int idx = CellIndex(x, y);
        m_buffer[idx] = Cell();
    }

    m_dirty = true;
}

void ScreenBuffer::ClearRect(int x, int y, int width, int height) {
    for (int dy = 0; dy < height; ++dy) {
        ClearLine(y + dy, x, x + width - 1);
    }
}

const Cell& ScreenBuffer::GetCell(int x, int y) const {
    static Cell emptyCell;

    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return emptyCell;
    }

    return m_buffer[CellIndex(x, y)];
}

Cell& ScreenBuffer::GetCellMutable(int x, int y) {
    static Cell emptyCell;

    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return emptyCell;
    }

    m_dirty = true;
    return m_buffer[CellIndex(x, y)];
}

const Cell& ScreenBuffer::GetCellWithScrollback(int x, int y) const {
    static Cell emptyCell;

    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return emptyCell;
    }

    // If we're scrolled back, get from scrollback buffer
    if (m_scrollOffset > 0) {
        // Calculate which line in scrollback this corresponds to
        // scrollOffset = 0 means viewing current buffer
        // scrollOffset = 1 means viewing 1 line back, etc.

        int scrollbackLineIndex = m_scrollbackUsed - m_scrollOffset + y;

        if (scrollbackLineIndex >= 0 && scrollbackLineIndex < m_scrollbackUsed) {
            // Get from scrollback
            int scrollbackIdx = scrollbackLineIndex * m_cols + x;
            if (scrollbackIdx >= 0 && scrollbackIdx < static_cast<int>(m_scrollback.size())) {
                return m_scrollback[scrollbackIdx];
            }
        }
    }

    // Get from current buffer
    return m_buffer[CellIndex(x, y)];
}

void ScreenBuffer::UseAlternativeBuffer(bool use) {
    if (use == m_useAltBuffer) {
        return;
    }

    if (use) {
        // Switch to alternative buffer
        if (m_altBuffer.empty()) {
            m_altBuffer.resize(m_cols * m_rows);
        }

        std::swap(m_buffer, m_altBuffer);
        m_useAltBuffer = true;

        spdlog::debug("Switched to alternative buffer");
    } else {
        // Switch back to primary buffer
        std::swap(m_buffer, m_altBuffer);
        m_useAltBuffer = false;

        spdlog::debug("Switched to primary buffer");
    }

    m_dirty = true;
}

void ScreenBuffer::NewLine() {
    m_cursorX = 0;
    m_cursorY++;

    if (m_cursorY >= m_rows) {
        ScrollUp(1);
        m_cursorY = m_rows - 1;
    }

    m_dirty = true;
}

void ScreenBuffer::CarriageReturn() {
    m_cursorX = 0;
    m_dirty = true;
}

void ScreenBuffer::Tab() {
    // Move to next tab stop (every 8 columns)
    int nextTab = ((m_cursorX / 8) + 1) * 8;
    m_cursorX = std::min(nextTab, m_cols - 1);
    m_dirty = true;
}

void ScreenBuffer::Backspace() {
    if (m_cursorX > 0) {
        m_cursorX--;
        m_dirty = true;
    }
}
// Scroll region methods
void ScreenBuffer::SetScrollRegion(int top, int bottom) {
    // Validate bounds
    if (top < 0 || top >= m_rows || bottom < top || bottom >= m_rows) {
        ResetScrollRegion();
        return;
    }
    m_scrollTop = top;
    m_scrollBottom = bottom;
    spdlog::debug("Set scroll region: {} to {}", top, bottom);
}

void ScreenBuffer::ResetScrollRegion() {
    m_scrollTop = 0;
    m_scrollBottom = -1;  // -1 means use m_rows - 1
}

int ScreenBuffer::GetScrollRegionBottom() const {
    return (m_scrollBottom < 0) ? m_rows - 1 : m_scrollBottom;
}

void ScreenBuffer::ScrollRegionUp(int lines) {
    if (lines <= 0) return;

    int bottom = GetScrollRegionBottom();
    int regionHeight = bottom - m_scrollTop + 1;
    lines = std::min(lines, regionHeight);

    // Move lines up within the region
    for (int y = m_scrollTop; y <= bottom - lines; ++y) {
        for (int x = 0; x < m_cols; ++x) {
            m_buffer[CellIndex(x, y)] = m_buffer[CellIndex(x, y + lines)];
        }
    }

    // Clear the bottom lines of the region
    for (int y = bottom - lines + 1; y <= bottom; ++y) {
        for (int x = 0; x < m_cols; ++x) {
            m_buffer[CellIndex(x, y)] = Cell();
        }
    }

    m_dirty = true;
}

void ScreenBuffer::ScrollRegionDown(int lines) {
    if (lines <= 0) return;

    int bottom = GetScrollRegionBottom();
    int regionHeight = bottom - m_scrollTop + 1;
    lines = std::min(lines, regionHeight);

    // Move lines down within the region
    for (int y = bottom; y >= m_scrollTop + lines; --y) {
        for (int x = 0; x < m_cols; ++x) {
            m_buffer[CellIndex(x, y)] = m_buffer[CellIndex(x, y - lines)];
        }
    }

    // Clear the top lines of the region
    for (int y = m_scrollTop; y < m_scrollTop + lines; ++y) {
        for (int x = 0; x < m_cols; ++x) {
            m_buffer[CellIndex(x, y)] = Cell();
        }
    }

    m_dirty = true;
}


} // namespace TerminalDX12::Terminal
