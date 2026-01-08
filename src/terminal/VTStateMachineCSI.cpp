// VTStateMachineCSI.cpp - CSI cursor and editing command handlers
// Part of VTStateMachine implementation

#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::Terminal {

void VTStateMachine::HandleCursorPosition() {
    int row = GetParam(0, 1) - 1;  // VT100 is 1-indexed
    int col = GetParam(1, 1) - 1;
    int origRow = row;
    int origCol = col;

    // In origin mode, row is relative to scroll region and constrained to it
    if (m_originMode) {
        int scrollTop = m_screenBuffer->GetScrollRegionTop();
        int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
        row = std::clamp(row + scrollTop, scrollTop, scrollBottom);
        spdlog::info("CUP: ({},{}) origin=ON region=[{},{}] -> row {} (requested {} outside region? {})",
                     origRow + 1, origCol + 1, scrollTop, scrollBottom, row,
                     origRow + 1, (origRow + scrollTop < scrollTop || origRow + scrollTop > scrollBottom) ? "YES" : "no");
    } else {
        spdlog::info("CUP: ({},{}) origin=OFF -> row {}", origRow + 1, origCol + 1, row);
    }

    m_screenBuffer->SetCursorPos(col, row);
}

void VTStateMachine::HandleCursorUp() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    // In origin mode, stop at scroll region top
    int minY = m_originMode ? m_screenBuffer->GetScrollRegionTop() : 0;
    m_screenBuffer->SetCursorPos(x, std::max(minY, y - count));
}

void VTStateMachine::HandleCursorDown() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    // In origin mode, stop at scroll region bottom
    int maxY = m_originMode ? m_screenBuffer->GetScrollRegionBottom() : (m_screenBuffer->GetRows() - 1);
    m_screenBuffer->SetCursorPos(x, std::min(maxY, y + count));
}

void VTStateMachine::HandleCursorForward() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(std::min(m_screenBuffer->GetCols() - 1, x + count), y);
}

void VTStateMachine::HandleCursorBack() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(std::max(0, x - count), y);
}

void VTStateMachine::HandleCursorNextLine() {
    // ESC[nE - Move cursor to beginning of line n rows down
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    int maxY = m_originMode ? m_screenBuffer->GetScrollRegionBottom() : (m_screenBuffer->GetRows() - 1);
    m_screenBuffer->SetCursorPos(0, std::min(maxY, y + count));
}

void VTStateMachine::HandleCursorPreviousLine() {
    // ESC[nF - Move cursor to beginning of line n rows up
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    int minY = m_originMode ? m_screenBuffer->GetScrollRegionTop() : 0;
    m_screenBuffer->SetCursorPos(0, std::max(minY, y - count));
}

void VTStateMachine::HandleCursorHorizontalAbsolute() {
    // ESC[nG - Move cursor to column n (1-indexed)
    int col = GetParam(0, 1) - 1;
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(col, y);
}

void VTStateMachine::HandleCursorVerticalAbsolute() {
    // ESC[nd - Move cursor to row n (1-indexed)
    int row = GetParam(0, 1) - 1;
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    // In origin mode, row is relative to scroll region
    if (m_originMode) {
        int scrollTop = m_screenBuffer->GetScrollRegionTop();
        int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
        row = std::clamp(row + scrollTop, scrollTop, scrollBottom);
    }

    m_screenBuffer->SetCursorPos(x, row);
}

void VTStateMachine::HandleEraseCharacter() {
    // ESC[nX - Erase n characters at cursor (replace with spaces)
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    for (int i = 0; i < count && (x + i) < m_screenBuffer->GetCols(); ++i) {
        m_screenBuffer->GetCellMutable(x + i, y).ch = U' ';
    }
}

void VTStateMachine::HandleDeleteCharacter() {
    // ESC[nP - Delete n characters at cursor (shift remaining left)
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    int cols = m_screenBuffer->GetCols();

    // Shift characters left
    for (int i = x; i < cols - count; ++i) {
        m_screenBuffer->GetCellMutable(i, y) = m_screenBuffer->GetCell(i + count, y);
    }

    // Clear the rightmost characters
    for (int i = cols - count; i < cols; ++i) {
        m_screenBuffer->GetCellMutable(i, y).ch = U' ';
    }
}

void VTStateMachine::HandleInsertCharacter() {
    // ESC[n@ - Insert n blank characters at cursor (shift remaining right)
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    int cols = m_screenBuffer->GetCols();

    // Shift characters right
    for (int i = cols - 1; i >= x + count; --i) {
        m_screenBuffer->GetCellMutable(i, y) = m_screenBuffer->GetCell(i - count, y);
    }

    // Clear the inserted positions
    for (int i = x; i < x + count && i < cols; ++i) {
        m_screenBuffer->GetCellMutable(i, y).ch = U' ';
    }
}

void VTStateMachine::HandleInsertLine() {
    // ESC[nL - Insert n blank lines at cursor position
    // Lines from cursor to bottom of scroll region are shifted down
    // New blank lines are inserted at cursor position
    int count = GetParam(0, 1);
    int cursorY = m_screenBuffer->GetCursorY();
    int scrollBottom = m_screenBuffer->GetScrollRegionBottom();

    // Clamp count to available lines
    count = std::min(count, scrollBottom - cursorY + 1);

    // Temporarily adjust scroll region to start at cursor line
    int savedTop = m_screenBuffer->GetScrollRegionTop();
    m_screenBuffer->SetScrollRegion(cursorY, scrollBottom);

    // Scroll content down within the region
    m_screenBuffer->ScrollRegionDown(count);

    // Restore original scroll region
    m_screenBuffer->SetScrollRegion(savedTop, scrollBottom);
}

void VTStateMachine::HandleDeleteLine() {
    // ESC[nM - Delete n lines at cursor position
    // Lines from cursor to bottom of scroll region are shifted up
    // Blank lines are added at bottom of scroll region
    int count = GetParam(0, 1);
    int cursorY = m_screenBuffer->GetCursorY();
    int scrollBottom = m_screenBuffer->GetScrollRegionBottom();

    // Clamp count to available lines
    count = std::min(count, scrollBottom - cursorY + 1);

    // Temporarily adjust scroll region to start at cursor line
    int savedTop = m_screenBuffer->GetScrollRegionTop();
    m_screenBuffer->SetScrollRegion(cursorY, scrollBottom);

    // Scroll content up within the region
    m_screenBuffer->ScrollRegionUp(count);

    // Restore original scroll region
    m_screenBuffer->SetScrollRegion(savedTop, scrollBottom);
}

void VTStateMachine::HandleSetScrollingRegion() {
    // ESC[t;br - Set scrolling region from line t to line b
    int top = GetParam(0, 1) - 1;     // Convert to 0-indexed
    int bottom = GetParam(1, 0);
    int rows = m_screenBuffer->GetRows();

    if (bottom == 0) {
        bottom = rows - 1;
    } else {
        bottom = bottom - 1;  // Convert to 0-indexed
    }

    spdlog::info("DECSTBM: Setting scroll region [{},{}] (screen has {} rows)", top, bottom, rows);
    m_screenBuffer->SetScrollRegion(top, bottom);

    // DECSTBM homes cursor - position depends on origin mode
    if (m_originMode) {
        // In origin mode: home to top of scroll region
        spdlog::info("DECSTBM: origin_mode=ON -> cursor to (0,{})", top);
        m_screenBuffer->SetCursorPos(0, top);
    } else {
        // Normal mode: home to absolute top-left
        spdlog::info("DECSTBM: origin_mode=OFF -> cursor to (0,0)");
        m_screenBuffer->SetCursorPos(0, 0);
    }
}

void VTStateMachine::HandleDeviceStatusReport() {
    // ESC[n - Various device status queries
    int param = GetParam(0, 0);

    if (param == 5) {
        // Device status - report OK
        SendResponse("[0n");
    } else if (param == 6) {
        // Cursor position report (CPR)
        int x = m_screenBuffer->GetCursorX();
        int y = m_screenBuffer->GetCursorY();
        int absY = y;

        // In origin mode, report position relative to scroll region
        if (m_originMode) {
            y = y - m_screenBuffer->GetScrollRegionTop();
            spdlog::debug("CPR: absolute=({},{}) origin_mode=ON scrollTop={} -> report ({},{})",
                          absY, x, m_screenBuffer->GetScrollRegionTop(), y + 1, x + 1);
        } else {
            spdlog::debug("CPR: absolute=({},{}) origin_mode=OFF -> report ({},{})",
                          absY, x, y + 1, x + 1);
        }

        std::string response = "[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "R";
        SendResponse(response);
    }
}

void VTStateMachine::HandleEraseInDisplay() {
    int mode = GetParam(0, 0);

    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    switch (mode) {
        case 0:  // Erase from cursor to end of display
            m_screenBuffer->ClearLine(y, x, m_screenBuffer->GetCols() - 1);
            for (int row = y + 1; row < m_screenBuffer->GetRows(); ++row) {
                m_screenBuffer->ClearLine(row);
            }
            break;

        case 1:  // Erase from start to cursor
            for (int row = 0; row < y; ++row) {
                m_screenBuffer->ClearLine(row);
            }
            m_screenBuffer->ClearLine(y, 0, x);
            break;

        case 2:  // Erase entire display
        case 3:  // Erase entire display + scrollback
            m_screenBuffer->Clear();
            m_screenBuffer->SetCursorPos(0, 0);
            break;
    }
}

void VTStateMachine::HandleEraseInLine() {
    int mode = GetParam(0, 0);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    switch (mode) {
        case 0:  // Erase from cursor to end of line
            m_screenBuffer->ClearLine(y, x, m_screenBuffer->GetCols() - 1);
            break;

        case 1:  // Erase from start of line to cursor
            m_screenBuffer->ClearLine(y, 0, x);
            break;

        case 2:  // Erase entire line
            m_screenBuffer->ClearLine(y);
            break;
    }
}

void VTStateMachine::HandleDeviceAttributes() {
    // CSI c or CSI 0 c - Primary Device Attributes
    // Respond as VT100 with Advanced Video Option
    if (m_intermediateChar == '>') {
        // CSI > c - Secondary DA - report as xterm v380
        SendResponse("[>41;380;0c");
    } else {
        // CSI c - Primary DA - VT220 with features
        SendResponse("[?62;1;2;4;6;9;15;18;21;22c");
    }
}

void VTStateMachine::HandleCursorSave() {
    // CSI s - Save cursor position (SCP)
    m_savedCursorCSI.x = m_screenBuffer->GetCursorX();
    m_savedCursorCSI.y = m_screenBuffer->GetCursorY();
    m_savedCursorCSI.valid = true;
}

void VTStateMachine::HandleCursorRestore() {
    // CSI u - Restore cursor position (RCP)
    if (m_savedCursorCSI.valid) {
        m_screenBuffer->SetCursorPos(m_savedCursorCSI.x, m_savedCursorCSI.y);
    }
}

void VTStateMachine::HandleScrollUp() {
    // CSI n S - Scroll up n lines within scroll region
    int count = GetParam(0, 1);
    m_screenBuffer->ScrollRegionUp(count);
}

void VTStateMachine::HandleScrollDown() {
    // CSI n T - Scroll down n lines within scroll region
    int count = GetParam(0, 1);
    m_screenBuffer->ScrollRegionDown(count);
}

void VTStateMachine::HandleTabClear() {
    // CSI n g - Tab clear (TBC)
    int mode = GetParam(0, 0);
    switch (mode) {
        case 0:  // Clear tab stop at current position
            m_screenBuffer->ClearTabStop(m_screenBuffer->GetCursorX());
            break;
        case 3:  // Clear all tab stops
            m_screenBuffer->ClearAllTabStops();
            break;
        default:
            // Other modes not commonly used, ignore
            break;
    }
}

} // namespace TerminalDX12::Terminal
