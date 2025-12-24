#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::Terminal {

VTStateMachine::VTStateMachine(ScreenBuffer* screenBuffer)
    : m_screenBuffer(screenBuffer)
    , m_state(State::Ground)
    , m_intermediateChar(0)
    , m_finalChar(0)
{
}

VTStateMachine::~VTStateMachine() {
}

void VTStateMachine::ProcessInput(const char* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        uint8_t byte = static_cast<uint8_t>(data[i]);

        // If we're in an escape sequence, process bytes directly
        if (m_state != State::Ground) {
            ProcessCharacter(data[i]);
            continue;
        }

        // UTF-8 decoding for Ground state
        if (m_utf8BytesNeeded > 0) {
            // Expecting continuation byte (10xxxxxx)
            if ((byte & 0xC0) == 0x80) {
                m_utf8Buffer[m_utf8BytesReceived++] = byte;
                if (m_utf8BytesReceived == m_utf8BytesNeeded) {
                    // Decode complete UTF-8 sequence
                    char32_t codepoint = 0;
                    if (m_utf8BytesNeeded == 2) {
                        codepoint = ((m_utf8Buffer[0] & 0x1F) << 6) |
                                    (m_utf8Buffer[1] & 0x3F);
                    } else if (m_utf8BytesNeeded == 3) {
                        codepoint = ((m_utf8Buffer[0] & 0x0F) << 12) |
                                    ((m_utf8Buffer[1] & 0x3F) << 6) |
                                    (m_utf8Buffer[2] & 0x3F);
                    } else if (m_utf8BytesNeeded == 4) {
                        codepoint = ((m_utf8Buffer[0] & 0x07) << 18) |
                                    ((m_utf8Buffer[1] & 0x3F) << 12) |
                                    ((m_utf8Buffer[2] & 0x3F) << 6) |
                                    (m_utf8Buffer[3] & 0x3F);
                    }
                    m_utf8BytesNeeded = 0;
                    m_utf8BytesReceived = 0;
                    // Write the decoded codepoint
                    m_screenBuffer->WriteChar(codepoint);
                }
            } else {
                // Invalid continuation byte - reset and process as new byte
                m_utf8BytesNeeded = 0;
                m_utf8BytesReceived = 0;
                // Fall through to process this byte
            }
        }

        if (m_utf8BytesNeeded == 0) {
            // Check for UTF-8 lead byte
            if ((byte & 0x80) == 0) {
                // ASCII (0xxxxxxx) - process directly
                ProcessCharacter(data[i]);
            } else if ((byte & 0xE0) == 0xC0) {
                // 2-byte sequence (110xxxxx)
                m_utf8Buffer[0] = byte;
                m_utf8BytesNeeded = 2;
                m_utf8BytesReceived = 1;
            } else if ((byte & 0xF0) == 0xE0) {
                // 3-byte sequence (1110xxxx)
                m_utf8Buffer[0] = byte;
                m_utf8BytesNeeded = 3;
                m_utf8BytesReceived = 1;
            } else if ((byte & 0xF8) == 0xF0) {
                // 4-byte sequence (11110xxx)
                m_utf8Buffer[0] = byte;
                m_utf8BytesNeeded = 4;
                m_utf8BytesReceived = 1;
            } else {
                // Invalid UTF-8 lead byte - skip
            }
        }
    }
}

void VTStateMachine::ProcessCharacter(char ch) {
    switch (m_state) {
        case State::Ground:
            if (ch == '\x1b') {  // ESC
                m_state = State::Escape;
                ResetState();
            } else {
                ExecuteCharacter(ch);
            }
            break;

        case State::Escape:
            HandleEscapeSequence(ch);
            break;

        case State::CSI_Entry:
            // Just entered CSI - private mode indicators are only valid here
            if (ch == '\x1b') {
                // ESC during CSI - abort and start new escape sequence
                m_state = State::Escape;
                ResetState();
            } else if (ch < 0x20 && ch != '\x1b') {
                // C0 control characters - execute them (CR, LF, etc.)
                ExecuteCharacter(ch);
            } else if (ch >= '0' && ch <= '9') {
                m_paramBuffer += ch;
                m_state = State::CSI_Param;
            } else if (ch == ';') {
                // Empty first parameter
                m_params.push_back(0);
                m_state = State::CSI_Param;
            } else if (ch == '?' || ch == '>' || ch == '!') {
                // Private mode indicator - ONLY valid at CSI_Entry (immediately after ESC[)
                m_intermediateChar = ch;
                m_state = State::CSI_Param;
            } else if (ch >= 0x20 && ch <= 0x2F) {  // Intermediate byte
                m_intermediateChar = ch;
                m_state = State::CSI_Intermediate;
            } else if (ch >= 0x40 && ch <= 0x7E) {  // Final byte
                m_finalChar = ch;
                HandleCSI();
                m_state = State::Ground;
            } else {
                // Invalid character - abort sequence and return to ground
                m_state = State::Ground;
            }
            break;

        case State::CSI_Param:
            // Parsing CSI parameters - private mode indicators are NOT valid here
            if (ch == '\x1b') {
                // ESC during CSI - abort and start new escape sequence
                m_state = State::Escape;
                ResetState();
            } else if (ch < 0x20 && ch != '\x1b') {
                // C0 control characters - execute them (CR, LF, etc.)
                ExecuteCharacter(ch);
            } else if (ch >= '0' && ch <= '9') {
                m_paramBuffer += ch;
            } else if (ch == ';') {
                // Parse current parameter
                if (!m_paramBuffer.empty()) {
                    m_params.push_back(std::stoi(m_paramBuffer));
                    m_paramBuffer.clear();
                } else {
                    m_params.push_back(0);
                }
            } else if (ch >= 0x20 && ch <= 0x2F) {  // Intermediate byte
                // Parse pending parameter first
                if (!m_paramBuffer.empty()) {
                    m_params.push_back(std::stoi(m_paramBuffer));
                    m_paramBuffer.clear();
                }
                m_intermediateChar = ch;
                m_state = State::CSI_Intermediate;
            } else if (ch >= 0x40 && ch <= 0x7E) {  // Final byte
                // Parse last parameter if any
                if (!m_paramBuffer.empty()) {
                    m_params.push_back(std::stoi(m_paramBuffer));
                }
                m_finalChar = ch;
                HandleCSI();
                m_state = State::Ground;
            } else {
                // Invalid character (including ?, >, ! after parameters) - abort
                m_state = State::Ground;
            }
            break;

        case State::CSI_Intermediate:
            // Handle control characters during CSI intermediate
            if (ch == '\x1b') {
                // ESC - abort and start new escape sequence
                m_state = State::Escape;
                ResetState();
            } else if (ch < 0x20) {
                // C0 control characters - execute them
                ExecuteCharacter(ch);
            } else if (ch >= 0x40 && ch <= 0x7E) {
                m_finalChar = ch;
                HandleCSI();
                m_state = State::Ground;
            } else if (ch >= 0x80) {
                // Invalid - abort sequence
                m_state = State::Ground;
            }
            break;

        case State::OSC_String:
            // OSC sequences end with BEL (0x07) or ST (ESC \)
            if (ch == '\x07') {  // BEL - end of OSC
                HandleOSC();
                m_state = State::Ground;
            } else if (ch == '\x1b') {
                // Might be ST (String Terminator) - ESC \
                // For simplicity, just end the OSC on any ESC
                HandleOSC();
                m_state = State::Escape;
                ResetState();
            } else if (ch == '\x9c') {
                // C1 ST (String Terminator)
                HandleOSC();
                m_state = State::Ground;
            } else {
                // Accumulate OSC content
                m_oscBuffer += ch;
                // Sanity limit to prevent unbounded memory growth
                if (m_oscBuffer.size() > 4096) {
                    m_oscBuffer.clear();
                    m_state = State::Ground;
                }
            }
            break;

        default:
            m_state = State::Ground;
            break;
    }
}

void VTStateMachine::HandleEscapeSequence(char ch) {
    switch (ch) {
        case '[':  // CSI
            m_state = State::CSI_Entry;
            break;

        case ']':  // OSC (Operating System Command)
            m_state = State::OSC_String;
            m_oscBuffer.clear();
            break;

        case 'M': {  // Reverse Index (RI) - move up, scroll if at top
            int y = m_screenBuffer->GetCursorY();
            int scrollTop = m_screenBuffer->GetScrollRegionTop();
            if (y <= scrollTop) {
                // At top of scroll region - scroll content down
                m_screenBuffer->ScrollRegionDown(1);
            } else {
                // Not at top - just move cursor up
                m_screenBuffer->SetCursorPos(m_screenBuffer->GetCursorX(), y - 1);
            }
            m_state = State::Ground;
            break;
        }

        case 'D': {  // Index (IND) - move down, scroll if at bottom
            int y = m_screenBuffer->GetCursorY();
            int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
            if (y >= scrollBottom) {
                // At bottom of scroll region - scroll content up
                m_screenBuffer->ScrollRegionUp(1);
            } else {
                // Not at bottom - just move cursor down
                m_screenBuffer->SetCursorPos(m_screenBuffer->GetCursorX(), y + 1);
            }
            m_state = State::Ground;
            break;
        }

        case 'E':  // Next Line
            m_screenBuffer->WriteChar('\n');
            m_state = State::Ground;
            break;

        case 'c':  // Reset
            m_screenBuffer->Clear();
            m_screenBuffer->SetCursorPos(0, 0);
            m_state = State::Ground;
            break;

        case '7':  // DECSC - Save cursor
            m_savedCursor.x = m_screenBuffer->GetCursorX();
            m_savedCursor.y = m_screenBuffer->GetCursorY();
            m_savedCursor.attr = m_screenBuffer->GetCurrentAttributes();
            m_savedCursor.originMode = m_originMode;
            m_savedCursor.autoWrap = m_autoWrap;
            m_savedCursor.valid = true;
            m_state = State::Ground;
            break;

        case '8':  // DECRC - Restore cursor
            if (m_savedCursor.valid) {
                m_screenBuffer->SetCursorPos(m_savedCursor.x, m_savedCursor.y);
                m_screenBuffer->SetCurrentAttributes(m_savedCursor.attr);
                m_originMode = m_savedCursor.originMode;
                m_autoWrap = m_savedCursor.autoWrap;
            }
            m_state = State::Ground;
            break;

        case 'H':  // HTS - Horizontal Tab Set
            m_screenBuffer->SetTabStop(m_screenBuffer->GetCursorX());
            m_state = State::Ground;
            break;

        case '=':  // DECKPAM - Keypad Application Mode
            m_keypadApplicationMode = true;
            spdlog::debug("DECKPAM: Keypad application mode enabled");
            m_state = State::Ground;
            break;

        case '>':  // DECKPNM - Keypad Numeric Mode
            m_keypadApplicationMode = false;
            spdlog::debug("DECKPNM: Keypad numeric mode enabled");
            m_state = State::Ground;
            break;

        default:
            // Unknown escape sequence, ignore
            m_state = State::Ground;
            break;
    }
}

void VTStateMachine::HandleCSI() {
    switch (m_finalChar) {
        case 'A': HandleCursorUp(); break;
        case 'B': HandleCursorDown(); break;
        case 'C': HandleCursorForward(); break;
        case 'D': HandleCursorBack(); break;
        case 'E': HandleCursorNextLine(); break;            // CNL
        case 'F': HandleCursorPreviousLine(); break;        // CPL
        case 'G': HandleCursorHorizontalAbsolute(); break;  // CHA
        case 'd': HandleCursorVerticalAbsolute(); break;    // VPA
        case 'H': HandleCursorPosition(); break;
        case 'f': HandleCursorPosition(); break;  // HVP (same as CUP)
        case 'J': HandleEraseInDisplay(); break;
        case 'K': HandleEraseInLine(); break;
        case 'X': HandleEraseCharacter(); break;            // ECH
        case 'P': HandleDeleteCharacter(); break;           // DCH
        case '@': HandleInsertCharacter(); break;           // ICH
        case 'L': HandleInsertLine(); break;                // IL
        case 'M': HandleDeleteLine(); break;                // DL
        case 'm': HandleSGR(); break;
        case 'c': HandleDeviceAttributes(); break;
        case 'h': HandleMode(); break;
        case 'l': HandleMode(); break;
        case 'r': HandleSetScrollingRegion(); break;        // DECSTBM
        case 'n': HandleDeviceStatusReport(); break;        // DSR
        case 's': HandleCursorSave(); break;                // SCP
        case 'u': HandleCursorRestore(); break;             // RCP
        case 'S': HandleScrollUp(); break;                  // SU
        case 'T': HandleScrollDown(); break;                // SD
        case 'g': HandleTabClear(); break;                  // TBC
        case 'q':
            if (m_intermediateChar == ' ') {
                HandleCursorStyle();                        // DECSCUSR
            }
            break;
        default:
            // Unhandled CSI sequence - silently ignore
            break;
    }
}

void VTStateMachine::HandleCursorPosition() {
    int row = GetParam(0, 1) - 1;  // VT100 is 1-indexed
    int col = GetParam(1, 1) - 1;

    // In origin mode, row is relative to scroll region and constrained to it
    if (m_originMode) {
        int scrollTop = m_screenBuffer->GetScrollRegionTop();
        int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
        row = std::clamp(row + scrollTop, scrollTop, scrollBottom);
    }

    m_screenBuffer->SetCursorPos(col, row);
}

void VTStateMachine::HandleCursorUp() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(x, std::max(0, y - count));
}

void VTStateMachine::HandleCursorDown() {
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(x, std::min(m_screenBuffer->GetRows() - 1, y + count));
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
    m_screenBuffer->SetCursorPos(0, std::min(m_screenBuffer->GetRows() - 1, y + count));
}

void VTStateMachine::HandleCursorPreviousLine() {
    // ESC[nF - Move cursor to beginning of line n rows up
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);
    m_screenBuffer->SetCursorPos(0, std::max(0, y - count));
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
    // ESC[nL - Insert n blank lines at cursor (scroll down)
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    // For now, just clear lines - proper implementation would scroll region
    for (int i = 0; i < count; ++i) {
        m_screenBuffer->ClearLine(y + i);
    }
}

void VTStateMachine::HandleDeleteLine() {
    // ESC[nM - Delete n lines at cursor (scroll up)
    int count = GetParam(0, 1);
    int x, y;
    m_screenBuffer->GetCursorPos(x, y);

    // For now, just clear lines - proper implementation would scroll region
    for (int i = 0; i < count; ++i) {
        m_screenBuffer->ClearLine(y + i);
    }
}

void VTStateMachine::HandleSetScrollingRegion() {
    // ESC[t;br - Set scrolling region from line t to line b
    int top = GetParam(0, 1) - 1;     // Convert to 0-indexed
    int bottom = GetParam(1, 0);
    
    if (bottom == 0) {
        bottom = m_screenBuffer->GetRows() - 1;
    } else {
        bottom = bottom - 1;  // Convert to 0-indexed
    }
    
    m_screenBuffer->SetScrollRegion(top, bottom);
    m_screenBuffer->SetCursorPos(0, 0);  // DECSTBM homes cursor
}

void VTStateMachine::HandleDeviceStatusReport() {
    // ESC[n - Various device status queries
    int param = GetParam(0, 0);
    
    if (param == 5) {
        // Device status - report OK
        SendResponse("[0n");
    } else if (param == 6) {
        // Cursor position report (CPR)
        int x = m_screenBuffer->GetCursorX();
        int y = m_screenBuffer->GetCursorY();
        std::string response = "[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "R";
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

void VTStateMachine::HandleSGR() {
    // If no parameters, default to 0 (reset)
    if (m_params.empty()) {
        m_params.push_back(0);
    }

    // Debug log SGR parameters
    std::string paramStr;
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (i > 0) paramStr += ";";
        paramStr += std::to_string(m_params[i]);
    }
    spdlog::debug("SGR sequence: ESC[{}m", paramStr);

    CellAttributes attr = m_screenBuffer->GetCurrentAttributes();

    for (size_t i = 0; i < m_params.size(); ++i) {
        int param = m_params[i];
        switch (param) {
            case 0:  // Reset
                attr.foreground = 7;   // White
                attr.background = 0;   // Black
                attr.flags = 0;
                attr.flags2 = 0;
                attr.underlineStyle = CellAttributes::UnderlineStyle::None;
                break;

            case 1:  // Bold
                attr.flags |= CellAttributes::FLAG_BOLD;
                break;

            case 2:  // Dim/Faint
                attr.flags |= CellAttributes::FLAG_DIM;
                break;

            case 3:  // Italic
                attr.flags |= CellAttributes::FLAG_ITALIC;
                break;

            case 4:  // Underline (single)
                attr.underlineStyle = CellAttributes::UnderlineStyle::Single;
                attr.flags |= CellAttributes::FLAG_UNDERLINE;
                break;

            case 5:  // Slow Blink
            case 6:  // Rapid Blink (treat same as slow blink)
                attr.flags2 |= CellAttributes::FLAG2_BLINK;
                break;

            case 7:  // Inverse
                attr.flags |= CellAttributes::FLAG_INVERSE;
                break;

            case 8:  // Hidden/Invisible
                attr.flags2 |= CellAttributes::FLAG2_HIDDEN;
                break;

            case 9:  // Strikethrough
                attr.flags |= CellAttributes::FLAG_STRIKETHROUGH;
                break;

            case 21:  // Double underline
                attr.underlineStyle = CellAttributes::UnderlineStyle::Double;
                attr.flags |= CellAttributes::FLAG_UNDERLINE;
                break;

            case 22:  // Not bold/dim - clears both
                attr.flags &= ~(CellAttributes::FLAG_BOLD | CellAttributes::FLAG_DIM);
                break;

            case 23:  // Not italic
                attr.flags &= ~CellAttributes::FLAG_ITALIC;
                break;

            case 24:  // Not underline
                attr.underlineStyle = CellAttributes::UnderlineStyle::None;
                attr.flags &= ~CellAttributes::FLAG_UNDERLINE;
                break;

            case 25:  // Not blinking
                attr.flags2 &= ~CellAttributes::FLAG2_BLINK;
                break;

            case 27:  // Not inverse
                attr.flags &= ~CellAttributes::FLAG_INVERSE;
                break;

            case 28:  // Not hidden
                attr.flags2 &= ~CellAttributes::FLAG2_HIDDEN;
                break;

            case 29:  // Not strikethrough
                attr.flags &= ~CellAttributes::FLAG_STRIKETHROUGH;
                break;

            // Foreground colors (30-37 for standard, 90-97 for bright)
            case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
                attr.foreground = param - 30;
                break;

            case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
                attr.foreground = param - 90 + 8;  // Bright colors are 8-15
                break;

            // Extended foreground color: 38;5;N (256-color) or 38;2;R;G;B (true color)
            case 38:
                if (i + 1 < m_params.size()) {
                    if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                        // 256-color mode: 38;5;N
                        int colorIndex = m_params[i + 2];
                        // Map 256-color to 16-color for now (first 16 are the same)
                        if (colorIndex < 16) {
                            attr.foreground = colorIndex;
                        } else if (colorIndex >= 232) {
                            // Grayscale ramp (232-255) -> map to closest standard color
                            int gray = (colorIndex - 232) * 255 / 23;
                            if (gray < 64) attr.foreground = 0;       // Black
                            else if (gray < 192) attr.foreground = 8; // Gray
                            else attr.foreground = 7;                 // White
                        } else {
                            // Color cube (16-231) -> map to closest 16-color
                            // Color cube is 6x6x6: index = 16 + 36*r + 6*g + b where r,g,b in [0,5]
                            int idx = colorIndex - 16;
                            int r = idx / 36;
                            int g = (idx % 36) / 6;
                            int b = idx % 6;
                            // Map to standard colors based on which channel is highest
                            if (r > g && r > b) attr.foreground = (r > 3) ? 9 : 1;      // Red
                            else if (g > r && g > b) attr.foreground = (g > 3) ? 10 : 2; // Green
                            else if (b > r && b > g) attr.foreground = (b > 3) ? 12 : 4; // Blue
                            else if (r == g && r > b) attr.foreground = (r > 3) ? 11 : 3; // Yellow
                            else if (r == b && r > g) attr.foreground = (r > 3) ? 13 : 5; // Magenta
                            else if (g == b && g > r) attr.foreground = (g > 3) ? 14 : 6; // Cyan
                            else attr.foreground = (r > 2) ? 15 : 7;                      // White/Gray
                        }
                        i += 2;  // Skip the 5 and N parameters
                    } else if (m_params[i + 1] == 2 && i + 4 < m_params.size()) {
                        // True color mode: 38;2;R;G;B - store actual RGB
                        attr.SetForegroundRGB(
                            static_cast<uint8_t>(m_params[i + 2]),
                            static_cast<uint8_t>(m_params[i + 3]),
                            static_cast<uint8_t>(m_params[i + 4]));
                        i += 4;  // Skip the 2, R, G, B parameters
                    }
                }
                break;

            // Background colors (40-47 for standard, 100-107 for bright)
            case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
                attr.background = param - 40;
                break;

            case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
                attr.background = param - 100 + 8;
                break;

            // Extended background color: 48;5;N (256-color) or 48;2;R;G;B (true color)
            case 48:
                if (i + 1 < m_params.size()) {
                    if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                        // 256-color mode: 48;5;N
                        int colorIndex = m_params[i + 2];
                        if (colorIndex < 16) {
                            attr.background = colorIndex;
                        } else if (colorIndex >= 232) {
                            int gray = (colorIndex - 232) * 255 / 23;
                            if (gray < 64) attr.background = 0;
                            else if (gray < 192) attr.background = 8;
                            else attr.background = 7;
                        } else {
                            int idx = colorIndex - 16;
                            int r = idx / 36;
                            int g = (idx % 36) / 6;
                            int b = idx % 6;
                            if (r > g && r > b) attr.background = (r > 3) ? 9 : 1;
                            else if (g > r && g > b) attr.background = (g > 3) ? 10 : 2;
                            else if (b > r && b > g) attr.background = (b > 3) ? 12 : 4;
                            else if (r == g && r > b) attr.background = (r > 3) ? 11 : 3;
                            else if (r == b && r > g) attr.background = (r > 3) ? 13 : 5;
                            else if (g == b && g > r) attr.background = (g > 3) ? 14 : 6;
                            else attr.background = (r > 2) ? 15 : 7;
                        }
                        i += 2;
                    } else if (m_params[i + 1] == 2 && i + 4 < m_params.size()) {
                        // True color mode: 48;2;R;G;B - store actual RGB
                        attr.SetBackgroundRGB(
                            static_cast<uint8_t>(m_params[i + 2]),
                            static_cast<uint8_t>(m_params[i + 3]),
                            static_cast<uint8_t>(m_params[i + 4]));
                        i += 4;
                    }
                }
                break;

            case 39:  // Default foreground
                attr.foreground = 7;
                break;

            case 49:  // Default background
                attr.background = 0;
                break;
        }
    }

    m_screenBuffer->SetCurrentAttributes(attr);
}

void VTStateMachine::HandleDeviceAttributes() {
    // CSI c or CSI 0 c - Primary Device Attributes
    // Respond as VT100 with Advanced Video Option
    if (m_intermediateChar == '>') {
        // CSI > c - Secondary DA - report as xterm v380
        SendResponse("[>41;380;0c");
    } else {
        // CSI c - Primary DA - VT220 with features  
        SendResponse("[?62;1;2;4;6;9;15;18;21;22c");
    }
}

void VTStateMachine::HandleMode() {
    // CSI ? Ps h (set) or CSI ? Ps l (reset) - Private modes
    bool set = (m_finalChar == 'h');

    for (int mode : m_params) {
        if (m_intermediateChar == '?') {
            // Private modes (DEC)
            switch (mode) {
                case 1:  // DECCKM - Application cursor keys
                    m_applicationCursorKeys = set;
                    spdlog::debug("DECCKM: Application cursor keys {}", set ? "enabled" : "disabled");
                    break;

                case 6:  // DECOM - Origin Mode
                    m_originMode = set;
                    // When origin mode is set/reset, cursor moves to home position
                    if (set) {
                        m_screenBuffer->SetCursorPos(0, m_screenBuffer->GetScrollRegionTop());
                    } else {
                        m_screenBuffer->SetCursorPos(0, 0);
                    }
                    spdlog::debug("DECOM: Origin mode {}", set ? "enabled" : "disabled");
                    break;

                case 7:  // DECAWM - Auto-wrap mode
                    m_autoWrap = set;
                    spdlog::debug("DECAWM: Auto-wrap {}", set ? "enabled" : "disabled");
                    break;
                    
                case 12:  // Cursor blink
                    m_cursorBlink = set;
                    spdlog::debug("Cursor blink {}", set ? "enabled" : "disabled");
                    break;

                case 25:  // DECTCEM - Cursor visible
                    m_screenBuffer->SetCursorVisible(set);
                    spdlog::debug("DECTCEM: Cursor {}", set ? "visible" : "hidden");
                    break;

                case 47:    // Alternate screen buffer (simple)
                case 1047:  // Alternate screen buffer (like 47)
                case 1049:  // Alternate screen buffer (save cursor, switch, clear)
                    m_screenBuffer->UseAlternativeBuffer(set);
                    spdlog::debug("Alt screen buffer (mode {}) {}", mode, set ? "enabled" : "disabled");
                    break;
                    
                case 2004:  // Bracketed paste mode
                    m_bracketedPaste = set;
                    spdlog::debug("Bracketed paste mode {}", set ? "enabled" : "disabled");
                    break;

                // Mouse tracking modes
                case 1000:  // X10 Mouse - Button press only
                    m_mouseMode = set ? MouseMode::X10 : MouseMode::None;
                    spdlog::debug("Mouse X10 mode {}", set ? "enabled" : "disabled");
                    break;

                case 1002:  // Normal Mouse Tracking - Button + motion while pressed
                    m_mouseMode = set ? MouseMode::Normal : MouseMode::None;
                    spdlog::debug("Mouse normal tracking {}", set ? "enabled" : "disabled");
                    break;

                case 1003:  // All Mouse Tracking - All motion
                    m_mouseMode = set ? MouseMode::All : MouseMode::None;
                    spdlog::debug("Mouse all motion tracking {}", set ? "enabled" : "disabled");
                    break;

                case 1006:  // SGR Extended Mouse Mode
                    m_sgrMouseMode = set;
                    spdlog::debug("SGR mouse mode {}", set ? "enabled" : "disabled");
                    break;

                default:
                    spdlog::debug("Unknown private mode: {}", mode);
                    break;
            }
        } else {
            // ANSI (non-private) modes
            switch (mode) {
                case 4:  // IRM - Insert/Replace Mode
                    m_insertMode = set;
                    spdlog::debug("IRM: Insert mode {}", set ? "enabled" : "disabled");
                    break;

                case 20:  // LNM - Line Feed/New Line Mode
                    m_lineFeedNewLineMode = set;
                    spdlog::debug("LNM: Line feed/new line mode {}", set ? "enabled" : "disabled");
                    break;

                default:
                    spdlog::debug("Unknown ANSI mode: {}", mode);
                    break;
            }
        }
    }
}

void VTStateMachine::HandleOSC() {
    // OSC sequences: ESC ] Ps ; Pt BEL
    // Ps = 0: Change icon name and window title
    // Ps = 1: Change icon name
    // Ps = 2: Change window title
    // Ps = 133: Shell integration (FinalTerm/iTerm2)

    if (m_oscBuffer.empty()) {
        return;
    }

    // Parse the OSC type (number before first semicolon)
    size_t semicolon = m_oscBuffer.find(';');
    if (semicolon != std::string::npos) {
        std::string type = m_oscBuffer.substr(0, semicolon);
        std::string value = m_oscBuffer.substr(semicolon + 1);

        spdlog::debug("OSC sequence: type={}, value=\"{}\"", type, value);

        // OSC 133 - Shell integration
        if (type == "133") {
            HandleOSC133(value);
        }
        // OSC 10 - Set/Query foreground color
        else if (type == "10") {
            HandleOSC10(value);
        }
        // OSC 11 - Set/Query background color
        else if (type == "11") {
            HandleOSC11(value);
        }
        // OSC 8 - Hyperlink
        else if (type == "8") {
            HandleOSC8(value);
        }
        // OSC 52 - Clipboard access
        else if (type == "52") {
            HandleOSC52(value);
        }
        // OSC 4 - Set/Query palette color
        else if (type == "4") {
            HandleOSC4(value);
        }
        // OSC 0, 1, 2 are window title - we could handle these if desired
    }

    m_oscBuffer.clear();
}

void VTStateMachine::HandleOSC133(const std::string& param) {
    // OSC 133 shell integration:
    // A = Prompt start (before prompt is displayed)
    // B = Prompt end / Input start (after prompt, before user types)
    // C = Command start (user pressed Enter)
    // D = Command finished (optionally with exit code: D;N)

    if (param.empty()) return;

    char marker = param[0];
    switch (marker) {
        case 'A':
            m_screenBuffer->MarkPromptStart();
            break;
        case 'B':
            m_screenBuffer->MarkInputStart();
            break;
        case 'C':
            m_screenBuffer->MarkCommandStart();
            break;
        case 'D': {
            int exitCode = -1;
            // Check for exit code: D;N
            if (param.length() > 2 && param[1] == ';') {
                try {
                    exitCode = std::stoi(param.substr(2));
                } catch (...) {
                    exitCode = -1;
                }
            }
            m_screenBuffer->MarkCommandEnd(exitCode);
            break;
        }
        default:
            spdlog::debug("Unknown OSC 133 marker: {}", marker);
            break;
    }
}

void VTStateMachine::HandleOSC8(const std::string& param) {
    // OSC 8 - Hyperlink
    // Format: OSC 8 ; params ; uri ST
    // params can include id=xxx for link grouping
    // Empty uri ends the hyperlink

    // param format: "params;uri" where params can be empty
    size_t semicolon = param.find(';');
    if (semicolon == std::string::npos) {
        // Invalid format - just ignore
        return;
    }

    std::string params = param.substr(0, semicolon);
    std::string uri = param.substr(semicolon + 1);

    // Parse optional ID from params
    std::string linkId;
    if (!params.empty()) {
        // params are key=value pairs separated by ':'
        size_t pos = 0;
        while (pos < params.length()) {
            size_t colonPos = params.find(':', pos);
            std::string kvPair = (colonPos == std::string::npos)
                ? params.substr(pos)
                : params.substr(pos, colonPos - pos);

            size_t eqPos = kvPair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = kvPair.substr(0, eqPos);
                std::string value = kvPair.substr(eqPos + 1);
                if (key == "id") {
                    linkId = value;
                }
            }

            if (colonPos == std::string::npos) break;
            pos = colonPos + 1;
        }
    }

    if (uri.empty()) {
        // End hyperlink
        m_screenBuffer->ClearCurrentHyperlink();
        spdlog::debug("OSC 8: End hyperlink");
    } else {
        // Start hyperlink
        m_screenBuffer->AddHyperlink(uri, linkId);
        spdlog::debug("OSC 8: Start hyperlink uri=\"{}\" id=\"{}\"", uri, linkId);
    }
}

bool VTStateMachine::ParseOSCColor(const std::string& colorStr, uint8_t& r, uint8_t& g, uint8_t& b) {
    // Supported formats:
    // rgb:RR/GG/BB - X11 format with 2 hex digits per component
    // rgb:RRRR/GGGG/BBBB - X11 format with 4 hex digits per component
    // #RRGGBB - CSS-style hex format

    if (colorStr.empty()) return false;

    if (colorStr[0] == '#' && colorStr.length() == 7) {
        // #RRGGBB format
        try {
            r = static_cast<uint8_t>(std::stoi(colorStr.substr(1, 2), nullptr, 16));
            g = static_cast<uint8_t>(std::stoi(colorStr.substr(3, 2), nullptr, 16));
            b = static_cast<uint8_t>(std::stoi(colorStr.substr(5, 2), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }

    if (colorStr.substr(0, 4) == "rgb:") {
        std::string rgb = colorStr.substr(4);
        size_t slash1 = rgb.find('/');
        size_t slash2 = rgb.rfind('/');
        if (slash1 != std::string::npos && slash2 != std::string::npos && slash1 != slash2) {
            try {
                std::string rStr = rgb.substr(0, slash1);
                std::string gStr = rgb.substr(slash1 + 1, slash2 - slash1 - 1);
                std::string bStr = rgb.substr(slash2 + 1);

                // Handle both 2-digit and 4-digit formats
                int rVal = std::stoi(rStr, nullptr, 16);
                int gVal = std::stoi(gStr, nullptr, 16);
                int bVal = std::stoi(bStr, nullptr, 16);

                // If 4-digit format, scale down to 8-bit
                if (rStr.length() == 4) rVal = rVal >> 8;
                if (gStr.length() == 4) gVal = gVal >> 8;
                if (bStr.length() == 4) bVal = bVal >> 8;

                r = static_cast<uint8_t>(rVal);
                g = static_cast<uint8_t>(gVal);
                b = static_cast<uint8_t>(bVal);
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    return false;
}

void VTStateMachine::HandleOSC10(const std::string& param) {
    // OSC 10 - Set/Query foreground color
    // Query: OSC 10 ; ? ST -> respond with current color
    // Set: OSC 10 ; color ST -> set foreground color

    if (param == "?") {
        // Query - respond with current foreground color
        char response[32];
        snprintf(response, sizeof(response), "]10;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                 m_themeFgR, m_themeFgR, m_themeFgG, m_themeFgG, m_themeFgB, m_themeFgB);
        SendResponse(response);
    } else {
        // Set foreground color
        uint8_t r, g, b;
        if (ParseOSCColor(param, r, g, b)) {
            m_themeFgR = r;
            m_themeFgG = g;
            m_themeFgB = b;
            m_hasThemeFg = true;
            spdlog::debug("OSC 10: Set foreground color to #{:02x}{:02x}{:02x}", r, g, b);
        }
    }
}

void VTStateMachine::HandleOSC11(const std::string& param) {
    // OSC 11 - Set/Query background color
    // Query: OSC 11 ; ? ST -> respond with current color
    // Set: OSC 11 ; color ST -> set background color

    if (param == "?") {
        // Query - respond with current background color
        char response[32];
        snprintf(response, sizeof(response), "]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                 m_themeBgR, m_themeBgR, m_themeBgG, m_themeBgG, m_themeBgB, m_themeBgB);
        SendResponse(response);
    } else {
        // Set background color
        uint8_t r, g, b;
        if (ParseOSCColor(param, r, g, b)) {
            m_themeBgR = r;
            m_themeBgG = g;
            m_themeBgB = b;
            m_hasThemeBg = true;
            spdlog::debug("OSC 11: Set background color to #{:02x}{:02x}{:02x}", r, g, b);
        }
    }
}

// Base64 encoding table
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size()) n |= static_cast<uint8_t>(input[i + 2]);

        output.push_back(base64_chars[(n >> 18) & 0x3F]);
        output.push_back(base64_chars[(n >> 12) & 0x3F]);
        output.push_back((i + 1 < input.size()) ? base64_chars[(n >> 6) & 0x3F] : '=');
        output.push_back((i + 2 < input.size()) ? base64_chars[n & 0x3F] : '=');
    }
    return output;
}

static std::string Base64Decode(const std::string& input) {
    std::string output;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(base64_chars[i])] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

void VTStateMachine::HandleOSC52(const std::string& param) {
    // OSC 52 - Clipboard access
    // Format: OSC 52 ; Pc ; Pd ST
    // Pc = clipboard selection ('c' for clipboard, 'p' for primary, 's' for select)
    // Pd = base64 encoded data, or '?' to query

    size_t semicolon = param.find(';');
    if (semicolon == std::string::npos) {
        spdlog::debug("OSC 52: Invalid format (no semicolon)");
        return;
    }

    std::string selection = param.substr(0, semicolon);
    std::string data = param.substr(semicolon + 1);

    // We only support 'c' (system clipboard) on Windows
    // Ignore selection parameter and always use system clipboard

    if (data == "?") {
        // Query clipboard
        if (m_clipboardReadCallback) {
            std::string clipboardContent = m_clipboardReadCallback();
            std::string encoded = Base64Encode(clipboardContent);
            std::string response = "]52;" + selection + ";" + encoded + "\x07";
            SendResponse(response);
            spdlog::debug("OSC 52: Query clipboard, {} bytes", clipboardContent.size());
        }
    } else if (!data.empty()) {
        // Set clipboard
        std::string decoded = Base64Decode(data);
        if (m_clipboardWriteCallback && !decoded.empty()) {
            m_clipboardWriteCallback(decoded);
            spdlog::debug("OSC 52: Set clipboard, {} bytes", decoded.size());
        }
    }
}

void VTStateMachine::HandleOSC4(const std::string& param) {
    // OSC 4 - Set/Query palette color
    // Format: OSC 4 ; index ; color ST
    // Query: OSC 4 ; index ; ? ST -> respond with rgb:RRRR/GGGG/BBBB
    // Multiple colors: OSC 4 ; index1 ; color1 ; index2 ; color2 ; ... ST

    // Parse pairs of index;color
    size_t pos = 0;
    while (pos < param.length()) {
        // Find the index
        size_t semicolon1 = param.find(';', pos);
        if (semicolon1 == std::string::npos) break;

        std::string indexStr = param.substr(pos, semicolon1 - pos);
        int index = 0;
        try {
            index = std::stoi(indexStr);
        } catch (...) {
            spdlog::debug("OSC 4: Invalid index: {}", indexStr);
            break;
        }

        if (index < 0 || index >= 256) {
            spdlog::debug("OSC 4: Index out of range: {}", index);
            break;
        }

        // Find the color (or next semicolon for another pair)
        size_t semicolon2 = param.find(';', semicolon1 + 1);
        std::string colorStr;
        if (semicolon2 == std::string::npos) {
            colorStr = param.substr(semicolon1 + 1);
            pos = param.length();
        } else {
            colorStr = param.substr(semicolon1 + 1, semicolon2 - semicolon1 - 1);
            pos = semicolon2 + 1;
        }

        if (colorStr == "?") {
            // Query color - respond with current color
            const auto& paletteColor = m_screenBuffer->GetPaletteColor(index);
            // Format response as rgb:RRRR/GGGG/BBBB (16-bit components)
            char response[64];
            snprintf(response, sizeof(response), "]4;%d;rgb:%04x/%04x/%04x\x07",
                     index,
                     paletteColor.r * 257,  // Scale 8-bit to 16-bit
                     paletteColor.g * 257,
                     paletteColor.b * 257);
            SendResponse(response);
            spdlog::debug("OSC 4: Query color {}", index);
        } else {
            // Set color
            uint8_t r, g, b;
            if (ParseOSCColor(colorStr, r, g, b)) {
                m_screenBuffer->SetPaletteColor(index, r, g, b);
                spdlog::debug("OSC 4: Set color {} to #{:02x}{:02x}{:02x}", index, r, g, b);
            }
        }
    }
}

void VTStateMachine::ExecuteCharacter(char ch) {
    if (ch >= 32 || ch == '\n' || ch == '\r' || ch == '\t' || ch == '\b') {
        m_screenBuffer->WriteChar(static_cast<char32_t>(ch));
    }
}

int VTStateMachine::GetParam(size_t index, int defaultValue) const {
    if (index < m_params.size()) {
        return m_params[index];
    }
    return defaultValue;
}

void VTStateMachine::ResetState() {
    m_params.clear();
    m_paramBuffer.clear();
    m_intermediateChar = 0;
    m_finalChar = 0;
}

void VTStateMachine::Clear() {
    m_state = State::Ground;
    ResetState();
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

void VTStateMachine::HandleCursorStyle() {
    // CSI n SP q - Set cursor style (DECSCUSR)
    // 0 or default: Blinking block (default)
    // 1: Blinking block
    // 2: Steady block
    // 3: Blinking underline
    // 4: Steady underline
    // 5: Blinking bar
    // 6: Steady bar
    int style = GetParam(0, 0);

    switch (style) {
        case 0:
        case 1:
            m_cursorStyle = CursorStyle::BlinkingBlock;
            m_cursorBlink = true;
            break;
        case 2:
            m_cursorStyle = CursorStyle::SteadyBlock;
            m_cursorBlink = false;
            break;
        case 3:
            m_cursorStyle = CursorStyle::BlinkingUnderline;
            m_cursorBlink = true;
            break;
        case 4:
            m_cursorStyle = CursorStyle::SteadyUnderline;
            m_cursorBlink = false;
            break;
        case 5:
            m_cursorStyle = CursorStyle::BlinkingBar;
            m_cursorBlink = true;
            break;
        case 6:
            m_cursorStyle = CursorStyle::SteadyBar;
            m_cursorBlink = false;
            break;
        default:
            // Unknown style, use default
            m_cursorStyle = CursorStyle::BlinkingBlock;
            m_cursorBlink = true;
            break;
    }

    spdlog::debug("DECSCUSR: Cursor style set to {} (blink={})", style, m_cursorBlink);
}

void VTStateMachine::SendResponse(const std::string& response) {
    if (m_responseCallback) {
        m_responseCallback(response);
    }
}

} // namespace TerminalDX12::Terminal
