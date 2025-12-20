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
        ProcessCharacter(data[i]);
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
        case State::CSI_Param:
            if (ch >= '0' && ch <= '9') {
                m_paramBuffer += ch;
                m_state = State::CSI_Param;
            } else if (ch == ';') {
                // Parse current parameter
                if (!m_paramBuffer.empty()) {
                    m_params.push_back(std::stoi(m_paramBuffer));
                    m_paramBuffer.clear();
                } else {
                    m_params.push_back(0);
                }
            } else if (ch >= 0x20 && ch <= 0x2F) {  // Intermediate byte
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
            }
            break;

        case State::CSI_Intermediate:
            if (ch >= 0x40 && ch <= 0x7E) {
                m_finalChar = ch;
                HandleCSI();
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

        case 'M':  // Reverse Index (move up, scroll if needed)
            // TODO: Implement RI
            m_state = State::Ground;
            break;

        case 'D':  // Index (move down, scroll if needed)
            // TODO: Implement IND
            m_state = State::Ground;
            break;

        case 'E':  // Next Line
            m_screenBuffer->WriteChar('\n');
            m_state = State::Ground;
            break;

        case 'c':  // Reset
            m_screenBuffer->Clear();
            m_screenBuffer->SetCursorPos(0, 0);
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
        case 'H': HandleCursorPosition(); break;
        case 'f': HandleCursorPosition(); break;  // HVP (same as CUP)
        case 'J': HandleEraseInDisplay(); break;
        case 'K': HandleEraseInLine(); break;
        case 'm': HandleSGR(); break;
        case 'c': HandleDeviceAttributes(); break;
        case 'h': HandleMode(); break;
        case 'l': HandleMode(); break;
        default:
            spdlog::debug("Unhandled CSI sequence: ESC[{}{}",
                         m_intermediateChar ? std::string(1, m_intermediateChar) : "",
                         m_finalChar);
            break;
    }
}

void VTStateMachine::HandleCursorPosition() {
    int row = GetParam(0, 1) - 1;  // VT100 is 1-indexed
    int col = GetParam(1, 1) - 1;

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
                break;

            case 1:  // Bold
                attr.flags |= CellAttributes::FLAG_BOLD;
                break;

            case 3:  // Italic
                attr.flags |= CellAttributes::FLAG_ITALIC;
                break;

            case 4:  // Underline
                attr.flags |= CellAttributes::FLAG_UNDERLINE;
                break;

            case 7:  // Inverse
                attr.flags |= CellAttributes::FLAG_INVERSE;
                break;

            case 22:  // Not bold
                attr.flags &= ~CellAttributes::FLAG_BOLD;
                break;

            case 23:  // Not italic
                attr.flags &= ~CellAttributes::FLAG_ITALIC;
                break;

            case 24:  // Not underline
                attr.flags &= ~CellAttributes::FLAG_UNDERLINE;
                break;

            case 27:  // Not inverse
                attr.flags &= ~CellAttributes::FLAG_INVERSE;
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
                        // True color mode: 38;2;R;G;B - map to closest 16-color
                        int r = m_params[i + 2];
                        int g = m_params[i + 3];
                        int b = m_params[i + 4];
                        // Map to standard colors based on channel values
                        bool bright = (r > 127 || g > 127 || b > 127);
                        if (r > g && r > b) attr.foreground = bright ? 9 : 1;       // Red
                        else if (g > r && g > b) attr.foreground = bright ? 10 : 2; // Green
                        else if (b > r && b > g) attr.foreground = bright ? 12 : 4; // Blue
                        else if (r > 200 && g > 200 && b < 100) attr.foreground = bright ? 11 : 3; // Yellow
                        else if (r > 200 && b > 200 && g < 100) attr.foreground = bright ? 13 : 5; // Magenta
                        else if (g > 200 && b > 200 && r < 100) attr.foreground = bright ? 14 : 6; // Cyan
                        else if (r + g + b < 128) attr.foreground = 0;              // Black
                        else attr.foreground = bright ? 15 : 7;                      // White/Gray
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
                        int r = m_params[i + 2];
                        int g = m_params[i + 3];
                        int b = m_params[i + 4];
                        bool bright = (r > 127 || g > 127 || b > 127);
                        if (r > g && r > b) attr.background = bright ? 9 : 1;
                        else if (g > r && g > b) attr.background = bright ? 10 : 2;
                        else if (b > r && b > g) attr.background = bright ? 12 : 4;
                        else if (r > 200 && g > 200 && b < 100) attr.background = bright ? 11 : 3;
                        else if (r > 200 && b > 200 && g < 100) attr.background = bright ? 13 : 5;
                        else if (g > 200 && b > 200 && r < 100) attr.background = bright ? 14 : 6;
                        else if (r + g + b < 128) attr.background = 0;
                        else attr.background = bright ? 15 : 7;
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
    // Terminal identification - we could send a response here
    // For now, just ignore
}

void VTStateMachine::HandleMode() {
    // Set/Reset mode sequences
    // For now, just ignore
}

void VTStateMachine::HandleOSC() {
    // OSC sequences: ESC ] Ps ; Pt BEL
    // Ps = 0: Change icon name and window title
    // Ps = 1: Change icon name
    // Ps = 2: Change window title
    // For now, we just silently consume these

    if (m_oscBuffer.empty()) {
        return;
    }

    // Parse the OSC type (number before first semicolon)
    size_t semicolon = m_oscBuffer.find(';');
    if (semicolon != std::string::npos) {
        std::string type = m_oscBuffer.substr(0, semicolon);
        std::string value = m_oscBuffer.substr(semicolon + 1);

        spdlog::debug("OSC sequence: type={}, value=\"{}\"", type, value);

        // OSC 0, 1, 2 are window title - we could handle these if desired
        // For now, just log them
    }

    m_oscBuffer.clear();
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

} // namespace TerminalDX12::Terminal
