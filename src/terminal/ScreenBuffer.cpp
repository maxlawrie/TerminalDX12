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
    m_lineWrapped.resize(m_rows, false);
    m_scrollback.reserve(m_scrollbackLines);

    // Initialize default tab stops (every 8 columns)
    ResetTabStops();

    spdlog::info("ScreenBuffer created: {}x{} with {} scrollback lines",
                 m_cols, m_rows, m_scrollbackLines);
}

ScreenBuffer::~ScreenBuffer() {
}

void ScreenBuffer::Resize(int cols, int rows) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (cols == m_cols && rows == m_rows) {
        return;
    }

    spdlog::info("Resizing screen buffer from {}x{} to {}x{}, altBuffer={}",
                 m_cols, m_rows, cols, rows, m_useAltBuffer ? "yes" : "no");

    int oldCols = m_cols;
    int oldRows = m_rows;

    // For alternative buffer, just create a fresh empty buffer
    // TUI apps will fully redraw after receiving SIGWINCH, so we don't need to preserve content
    // This avoids potential race conditions with buffer access during resize
    if (m_useAltBuffer) {
        spdlog::info("Alt buffer resize: creating fresh {}x{} buffer", cols, rows);

        // Create fresh buffer - TUI app will redraw everything
        std::vector<Cell> newBuffer(cols * rows);  // All cells initialized to spaces

        // CRITICAL: Swap buffer BEFORE updating dimensions!
        // If we update dimensions first, bounds checks will pass but buffer is still old size
        // causing buffer overrun crashes
        m_buffer = std::move(newBuffer);
        m_cols = cols;
        m_rows = rows;
        m_lineWrapped.assign(rows, false);

        // Clamp cursor to new bounds
        ClampCursor();

        // Reset scroll region to full screen
        m_scrollTop = 0;
        m_scrollBottom = -1;

        m_dirty = true;
        spdlog::info("Alt buffer resize: done, cursor at ({}, {})", m_cursorX, m_cursorY);
        return;
    }

    // Text reflow for main buffer
    // Step 1: Extract all text as logical lines (combining soft-wrapped lines)
    // Use m_lineWrapped flags to determine soft wraps - m_lineWrapped[y] = true means
    // row y is a continuation of row y-1 (i.e., row y-1 soft-wrapped to row y)
    std::vector<std::vector<Cell>> logicalLines;
    std::vector<Cell> currentLine;

    // Process visible buffer
    for (int y = 0; y < oldRows; ++y) {
        // Add cells from this row
        for (int x = 0; x < oldCols; ++x) {
            currentLine.push_back(m_buffer[y * oldCols + x]);
        }

        // Check if next row is a continuation of this row (soft wrap)
        // m_lineWrapped[y+1] = true means row y soft-wrapped to row y+1
        bool nextRowIsContinuation = false;
        if (y + 1 < oldRows && y + 1 < static_cast<int>(m_lineWrapped.size())) {
            nextRowIsContinuation = m_lineWrapped[y + 1];
        }

        // End the logical line if next row is NOT a continuation, or if this is the last line
        if (!nextRowIsContinuation || y == oldRows - 1) {
            // Trim trailing spaces from logical line
            while (!currentLine.empty() && currentLine.back().ch == U' ') {
                currentLine.pop_back();
            }
            logicalLines.push_back(std::move(currentLine));
            currentLine.clear();
        }
    }
    
    // Step 2: Re-wrap logical lines to new column width
    std::vector<Cell> newBuffer(cols * rows);
    std::vector<bool> newWrapped(rows, false);
    int newRow = 0;
    int newCol = 0;
    
    for (const auto& logicalLine : logicalLines) {
        if (newRow >= rows) break;
        
        for (const Cell& cell : logicalLine) {
            if (newRow >= rows) break;
            
            newBuffer[newRow * cols + newCol] = cell;
            newCol++;
            
            // Wrap to next line if we've hit the column limit
            if (newCol >= cols) {
                newCol = 0;
                newRow++;
                if (newRow < rows) {
                    newWrapped[newRow] = true;  // This is a soft wrap
                }
            }
        }
        
        // Move to next line after logical line
        // Always advance to next row, even for empty lines
        newRow++;
        newCol = 0;
    }
    
    m_buffer = std::move(newBuffer);
    m_lineWrapped = std::move(newWrapped);
    m_cols = cols;
    m_rows = rows;
    
    // Resize the inactive alternative buffer (no reflow for alt buffer)
    if (!m_altBuffer.empty()) {
        std::vector<Cell> newAltBuffer(cols * rows);
        int copyRows = std::min(oldRows, rows);
        int copyCols = std::min(oldCols, cols);
        for (int y = 0; y < copyRows; ++y) {
            for (int x = 0; x < copyCols; ++x) {
                newAltBuffer[y * cols + x] = m_altBuffer[y * oldCols + x];
            }
        }
        m_altBuffer = std::move(newAltBuffer);
    }

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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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

        // Mark the new line as soft-wrapped (continuation of previous line)
        if (m_cursorY < m_rows && m_cursorY < static_cast<int>(m_lineWrapped.size())) {
            m_lineWrapped[m_cursorY] = true;
        }

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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (lines <= 0) return;

    lines = std::min(lines, m_rows);

    // Log scroll events to track resize issues
    spdlog::info("ScrollUp({}) called, altBuf={}, scrollTop={}, scrollBottom={}",
                 lines, m_useAltBuffer ? "yes" : "no", m_scrollTop, GetScrollRegionBottom());

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

    // Shift wrap flags up
    if (!m_lineWrapped.empty()) {
        std::move(m_lineWrapped.begin() + lines,
                 m_lineWrapped.end(),
                 m_lineWrapped.begin());
        std::fill(m_lineWrapped.end() - lines,
                 m_lineWrapped.end(),
                 false);
    }

    m_dirty = true;
}

void ScreenBuffer::ScrollDown(int lines) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    spdlog::info("ScreenBuffer::Clear() called, altBuffer={}, rows={}", m_useAltBuffer ? "yes" : "no", m_rows);
    std::fill(m_buffer.begin(), m_buffer.end(), Cell());
    std::fill(m_lineWrapped.begin(), m_lineWrapped.end(), false);
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
    // Thread-local to avoid data races when multiple threads access out-of-bounds
    thread_local Cell emptyCell;

    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return emptyCell;
    }

    // Extra safety: verify index is within buffer bounds
    int idx = CellIndex(x, y);
    if (idx < 0 || idx >= static_cast<int>(m_buffer.size())) {
        return emptyCell;
    }

    return m_buffer[idx];
}

Cell& ScreenBuffer::GetCellMutable(int x, int y) {
    // Thread-local to avoid data races when multiple threads access out-of-bounds
    thread_local Cell emptyCell;

    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return emptyCell;
    }

    // Extra safety: verify index is within buffer bounds
    int idx = CellIndex(x, y);
    if (idx < 0 || idx >= static_cast<int>(m_buffer.size())) {
        return emptyCell;
    }

    m_dirty = true;
    return m_buffer[idx];
}

Cell ScreenBuffer::GetCellWithScrollback(int x, int y) const {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

    // Return empty cell for out-of-bounds access
    if (x < 0 || x >= m_cols || y < 0 || y >= m_rows) {
        return Cell();  // Returns by value - safe copy
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
                return m_scrollback[scrollbackIdx];  // Returns by value - safe copy
            }
        }
    }

    // Extra safety: verify index is within buffer bounds
    // This catches any mismatch between m_cols/m_rows and actual buffer size
    int idx = CellIndex(x, y);
    if (idx < 0 || idx >= static_cast<int>(m_buffer.size())) {
        return Cell();  // Return empty cell for out-of-bounds
    }

    // Get from current buffer - returns by value to prevent dangling references
    // This is critical: if caller holds reference while resize happens,
    // the old buffer is freed and reference becomes invalid (use-after-free)
    return m_buffer[idx];
}


void ScreenBuffer::UseAlternativeBuffer(bool use) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);

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

    } else {
        // Switch back to primary buffer
        std::swap(m_buffer, m_altBuffer);
        m_useAltBuffer = false;

    }

    m_dirty = true;
}

void ScreenBuffer::NewLine() {
    m_cursorX = 0;
    m_cursorY++;

    // Mark the new line as NOT soft-wrapped (explicit newline)
    if (m_cursorY < m_rows && m_cursorY < static_cast<int>(m_lineWrapped.size())) {
        m_lineWrapped[m_cursorY] = false;
    }

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
    // Move to next tab stop
    for (int col = m_cursorX + 1; col < m_cols; ++col) {
        if (col < static_cast<int>(m_tabStops.size()) && m_tabStops[col]) {
            m_cursorX = col;
            m_dirty = true;
            return;
        }
    }
    // No tab stop found, move to end of line
    m_cursorX = m_cols - 1;
    m_dirty = true;
}

void ScreenBuffer::Backspace() {
    if (m_cursorX > 0) {
        m_cursorX--;
        m_dirty = true;
    }
}

void ScreenBuffer::SetTabStop(int column) {
    if (column >= 0 && column < m_cols) {
        if (column >= static_cast<int>(m_tabStops.size())) {
            m_tabStops.resize(m_cols, false);
        }
        m_tabStops[column] = true;
    }
}

void ScreenBuffer::ClearTabStop(int column) {
    if (column >= 0 && column < static_cast<int>(m_tabStops.size())) {
        m_tabStops[column] = false;
    }
}

void ScreenBuffer::ClearAllTabStops() {
    std::fill(m_tabStops.begin(), m_tabStops.end(), false);
}

void ScreenBuffer::ResetTabStops() {
    // Default tab stops every 8 columns
    m_tabStops.resize(m_cols, false);
    for (int col = 8; col < m_cols; col += 8) {
        m_tabStops[col] = true;
    }
}

// Scroll region methods
void ScreenBuffer::SetScrollRegion(int top, int bottom) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    // Validate bounds
    if (top < 0 || top >= m_rows || bottom < top || bottom >= m_rows) {
        spdlog::info("SetScrollRegion({}, {}) - invalid, resetting to full screen", top, bottom);
        // Don't call ResetScrollRegion to avoid recursive lock attempt
        m_scrollTop = 0;
        m_scrollBottom = -1;
        return;
    }
    m_scrollTop = top;
    m_scrollBottom = bottom;
    spdlog::info("SetScrollRegion: {} to {} (explicit region set)", top, bottom);
}

void ScreenBuffer::ResetScrollRegion() {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    m_scrollTop = 0;
    m_scrollBottom = -1;  // -1 means use m_rows - 1
    spdlog::info("ResetScrollRegion: now full screen (no explicit region)");
}

int ScreenBuffer::GetScrollRegionBottom() const {
    return (m_scrollBottom < 0) ? m_rows - 1 : m_scrollBottom;
}

void ScreenBuffer::ScrollRegionUp(int lines) {
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
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

// OSC 8 Hyperlink support
uint16_t ScreenBuffer::AddHyperlink(const std::string& url, const std::string& id) {
    if (url.empty()) {
        // Empty URL means end the current hyperlink
        m_currentHyperlinkId = 0;
        return 0;
    }

    // Check if we already have a hyperlink with the same URL and ID
    for (const auto& [existingId, link] : m_hyperlinks) {
        if (link.url == url && link.id == id) {
            m_currentHyperlinkId = existingId;
            return existingId;
        }
    }

    // Create a new hyperlink entry
    uint16_t newId = m_nextHyperlinkId++;
    m_hyperlinks[newId] = Hyperlink{url, id};
    m_currentHyperlinkId = newId;

    // Update current attributes to include hyperlink flag
    m_currentAttr.flags2 |= CellAttributes::FLAG2_HYPERLINK;
    m_currentAttr.hyperlinkId = newId;

    return newId;
}

void ScreenBuffer::ClearCurrentHyperlink() {
    m_currentHyperlinkId = 0;
    m_currentAttr.flags2 &= ~CellAttributes::FLAG2_HYPERLINK;
    m_currentAttr.hyperlinkId = 0;
}

const Hyperlink* ScreenBuffer::GetHyperlink(uint16_t id) const {
    auto it = m_hyperlinks.find(id);
    if (it != m_hyperlinks.end()) {
        return &it->second;
    }
    return nullptr;
}

// OSC 4 Palette management
void ScreenBuffer::SetPaletteColor(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= 256) return;
    m_palette[index].r = r;
    m_palette[index].g = g;
    m_palette[index].b = b;
    m_palette[index].modified = true;
    m_dirty = true;
}

const PaletteColor& ScreenBuffer::GetPaletteColor(int index) const {
    static PaletteColor defaultColor;
    if (index < 0 || index >= 256) return defaultColor;
    return m_palette[index];
}

bool ScreenBuffer::IsPaletteColorModified(int index) const {
    if (index < 0 || index >= 256) return false;
    return m_palette[index].modified;
}

void ScreenBuffer::ResetPaletteColor(int index) {
    if (index < 0 || index >= 256) return;
    m_palette[index].modified = false;
    m_dirty = true;
}

void ScreenBuffer::ResetAllPaletteColors() {
    for (int i = 0; i < 256; ++i) {
        m_palette[i].modified = false;
    }
    m_dirty = true;
}

} // namespace TerminalDX12::Terminal
