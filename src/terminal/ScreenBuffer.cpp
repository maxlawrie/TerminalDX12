#include "terminal/ScreenBuffer.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <fstream>
#include <chrono>
#include <ctime>

namespace TerminalDX12::Terminal {

// Debug helper to log row 0 changes - write to file for GUI app
static void LogRow0Change(const std::string& operation, const std::vector<Cell>& buffer, int cols) {
    static std::ofstream scrollLog("C:\Users\maxla\TerminalDX12\scroll_debug.log", std::ios::app);
    std::string row0;
    for (int x = 0; x < std::min(40, cols); ++x) {
        char32_t ch = buffer[x].ch;
        if (ch >= 32 && ch < 127) row0 += static_cast<char>(ch);
        else if (ch == 0) row0 += '.';
        else row0 += '?';
    }
    if (scrollLog.is_open()) {
        scrollLog << operation << ": [" << row0 << "]" << std::endl;
        scrollLog.flush();
    }
}

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

    spdlog::info("Resizing screen buffer from {}x{} to {}x{}, altBuffer={}",
                 m_cols, m_rows, cols, rows, m_useAltBuffer ? "yes" : "no");

    // Debug: log row 0 content before resize
    std::string row0Before;
    for (int x = 0; x < std::min(40, m_cols); ++x) {
        char32_t ch = m_buffer[x].ch;
        if (ch >= 32 && ch < 127) row0Before += static_cast<char>(ch);
        else if (ch == 0) row0Before += '.';
        else row0Before += '?';
    }
    spdlog::info("Row 0 BEFORE resize: [{}]", row0Before);

    int oldCols = m_cols;
    int oldRows = m_rows;

    // Helper lambda to resize a buffer with content preservation
    auto resizeBuffer = [&](std::vector<Cell>& buffer) {
        std::vector<Cell> newBuffer(cols * rows);

        int copyRows = std::min(oldRows, rows);
        int copyCols = std::min(oldCols, cols);

        for (int y = 0; y < copyRows; ++y) {
            for (int x = 0; x < copyCols; ++x) {
                int oldIdx = y * oldCols + x;
                int newIdx = y * cols + x;
                if (oldIdx < static_cast<int>(buffer.size())) {
                    newBuffer[newIdx] = buffer[oldIdx];
                }
            }
        }

        buffer = std::move(newBuffer);
    };

    // Resize the active buffer
    resizeBuffer(m_buffer);

    // Resize the inactive alternative buffer too (with content preservation)
    if (!m_altBuffer.empty()) {
        resizeBuffer(m_altBuffer);
    }

    m_cols = cols;
    m_rows = rows;

    // Debug: log row 0 content after resize
    std::string row0After;
    for (int x = 0; x < std::min(40, cols); ++x) {
        char32_t ch = m_buffer[x].ch;
        if (ch >= 32 && ch < 127) row0After += static_cast<char>(ch);
        else if (ch == 0) row0After += '.';
        else row0After += '?';
    }
    spdlog::info("Row 0 AFTER resize: [{}]", row0After);

    // Clamp cursor to new size
    ClampCursor();

    // Adjust scroll region to new size (don't reset - preserve TUI app's region)
    // Only clamp the bottom if it exceeds new rows
    if (m_scrollBottom >= rows) {
        m_scrollBottom = rows - 1;
    }
    // Clamp top if needed
    if (m_scrollTop >= rows) {
        m_scrollTop = 0;
        m_scrollBottom = -1;  // Full reset if top was invalid
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

        // Scroll if at bottom of scroll region
        int bottom = GetScrollRegionBottom();
        if (m_cursorY > bottom) {
            // Check if an explicit scroll region is set (m_scrollBottom >= 0)
            bool hasExplicitScrollRegion = (m_scrollBottom >= 0);
            
            if (hasExplicitScrollRegion) {
                // Explicit scroll region - scroll within region, discard top
                ScrollRegionUp(1);
            } else {
                // No explicit scroll region - scroll whole screen, add to scrollback
                ScrollUp(1);
            }
            m_cursorY = bottom;
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

    // Log scroll events to track resize issues
    spdlog::info("ScrollUp({}) called, altBuf={}, scrollTop={}, scrollBottom={}",
                 lines, m_useAltBuffer ? "yes" : "no", m_scrollTop, GetScrollRegionBottom());
    
    // Log row 0 before scroll
    LogRow0Change("ScrollUp-BEFORE", m_buffer, m_cols);

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

    // Log row 0 after scroll
    LogRow0Change("ScrollUp-AFTER", m_buffer, m_cols);

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
    spdlog::info("ScreenBuffer::Clear() called, altBuffer={}, rows={}", m_useAltBuffer ? "yes" : "no", m_rows);
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

// File logger for alt buffer tracking
static void AltBufLog(const std::string& msg) {
    static std::ofstream debugFile("C:\\Users\\maxla\\TerminalDX12\\resize_debug.log", std::ios::app);
    if (debugFile.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&time));
        debugFile << "[" << buf << "] ALTBUF: " << msg << std::endl;
        debugFile.flush();
    }
}

void ScreenBuffer::UseAlternativeBuffer(bool use) {
    AltBufLog("UseAlternativeBuffer(" + std::to_string(use) + ") called, current=" + std::to_string(m_useAltBuffer));

    if (use == m_useAltBuffer) {
        AltBufLog("No change needed, returning");
        return;
    }

    if (use) {
        // Switch to alternative buffer
        if (m_altBuffer.empty()) {
            m_altBuffer.resize(m_cols * m_rows);
        }

        std::swap(m_buffer, m_altBuffer);
        m_useAltBuffer = true;

        AltBufLog("Switched TO alternative buffer");
    } else {
        // Switch back to primary buffer
        std::swap(m_buffer, m_altBuffer);
        m_useAltBuffer = false;

        AltBufLog("Switched BACK to primary buffer");
    }

    m_dirty = true;
}

void ScreenBuffer::NewLine() {
    m_cursorX = 0;
    m_cursorY++;

    // Scroll if at bottom of scroll region
    int bottom = GetScrollRegionBottom();
    if (m_cursorY > bottom) {
        // Check if an explicit scroll region is set (m_scrollBottom >= 0)
        // If no explicit region, use ScrollUp to add to scrollback
        // If explicit region, use ScrollRegionUp which discards top of region
        bool hasExplicitScrollRegion = (m_scrollBottom >= 0);
        
        if (hasExplicitScrollRegion) {
            // Explicit scroll region - scroll within region, discard top
            ScrollRegionUp(1);
        } else {
            // No explicit scroll region - scroll whole screen, add to scrollback
            ScrollUp(1);
        }
        m_cursorY = bottom;
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
        spdlog::info("SetScrollRegion({}, {}) - invalid, resetting to full screen", top, bottom);
        ResetScrollRegion();
        return;
    }
    m_scrollTop = top;
    m_scrollBottom = bottom;
    spdlog::info("SetScrollRegion: {} to {} (explicit region set)", top, bottom);
}

void ScreenBuffer::ResetScrollRegion() {
    m_scrollTop = 0;
    m_scrollBottom = -1;  // -1 means use m_rows - 1
    spdlog::info("ResetScrollRegion: now full screen (no explicit region)");
}

int ScreenBuffer::GetScrollRegionBottom() const {
    return (m_scrollBottom < 0) ? m_rows - 1 : m_scrollBottom;
}

void ScreenBuffer::ScrollRegionUp(int lines) {
    if (lines <= 0) return;

    int bottom = GetScrollRegionBottom();
    
    // Log scroll region events to track resize issues
    spdlog::info("ScrollRegionUp({}) called, scrollTop={}, scrollBottom={}",
                 lines, m_scrollTop, bottom);
    
    // Log row 0 before if scroll region starts at 0
    if (m_scrollTop == 0) {
        LogRow0Change("ScrollRegionUp-BEFORE", m_buffer, m_cols);
    }
    
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

    // Log row 0 after if scroll region starts at 0
    if (m_scrollTop == 0) {
        LogRow0Change("ScrollRegionUp-AFTER", m_buffer, m_cols);
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

// OSC 133 shell integration methods
void ScreenBuffer::MarkPromptStart() {
    m_currentZone = SemanticZone::Prompt;

    // Calculate absolute line number (scrollback + current line)
    int absoluteLine = m_scrollbackUsed + m_cursorY;

    // Add prompt marker
    PromptMarker marker;
    marker.absoluteLine = absoluteLine;
    marker.exitCode = -1;
    m_promptMarkers.push_back(marker);

    spdlog::debug("OSC 133;A - Prompt start at line {}", absoluteLine);
}

void ScreenBuffer::MarkInputStart() {
    m_currentZone = SemanticZone::Input;
    spdlog::debug("OSC 133;B - Input start");
}

void ScreenBuffer::MarkCommandStart() {
    m_currentZone = SemanticZone::Output;
    spdlog::debug("OSC 133;C - Command execution start");
}

void ScreenBuffer::MarkCommandEnd(int exitCode) {
    m_currentZone = SemanticZone::None;

    // Update the last prompt marker with exit code if we have one
    if (!m_promptMarkers.empty()) {
        m_promptMarkers.back().exitCode = exitCode;
    }

    spdlog::debug("OSC 133;D - Command end, exit code: {}", exitCode);
}

int ScreenBuffer::GetPreviousPromptLine(int fromLine) const {
    // Find the prompt before fromLine
    int foundLine = -1;
    for (const auto& marker : m_promptMarkers) {
        if (marker.absoluteLine < fromLine) {
            foundLine = marker.absoluteLine;
        } else {
            break;
        }
    }
    return foundLine;
}

int ScreenBuffer::GetNextPromptLine(int fromLine) const {
    // Find the prompt after fromLine
    for (const auto& marker : m_promptMarkers) {
        if (marker.absoluteLine > fromLine) {
            return marker.absoluteLine;
        }
    }
    return -1;
}

} // namespace TerminalDX12::Terminal
