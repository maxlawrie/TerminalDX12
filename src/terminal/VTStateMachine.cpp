// VTStateMachine.cpp - Core VT100/ANSI escape sequence state machine
// Handler implementations are split across:
//   - VTStateMachineCSI.cpp  (cursor, editing, scroll, erase)
//   - VTStateMachineSGR.cpp  (colors and text attributes)
//   - VTStateMachineOSC.cpp  (operating system commands)
//   - VTStateMachineModes.cpp (terminal modes and queries)

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
            if (y == scrollTop) {
                // Exactly at top of scroll region - scroll content down
                m_screenBuffer->ScrollRegionDown(1);
            } else {
                // Not at top - move cursor up (clamp handles y < 0)
                m_screenBuffer->SetCursorPos(m_screenBuffer->GetCursorX(), y - 1);
            }
            m_state = State::Ground;
            break;
        }

        case 'D': {  // Index (IND) - move down, scroll if at bottom
            int y = m_screenBuffer->GetCursorY();
            int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
            if (y == scrollBottom) {
                // Exactly at bottom of scroll region - scroll content up
                m_screenBuffer->ScrollRegionUp(1);
            } else {
                // Not at bottom - move cursor down (clamp handles overflow)
                m_screenBuffer->SetCursorPos(m_screenBuffer->GetCursorX(), y + 1);
            }
            m_state = State::Ground;
            break;
        }

        case 'E':  // Next Line (NEL)
            m_screenBuffer->WriteChar(U'\n');
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
    // Debug: Log all CSI sequences
    std::string paramStr;
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (i > 0) paramStr += ";";
        paramStr += std::to_string(m_params[i]);
    }
    if (m_intermediateChar) {
        spdlog::debug("CSI: {}{}{}  (intermediate='{}')", paramStr,
                      m_intermediateChar ? std::string(1, m_intermediateChar) : "",
                      m_finalChar, m_intermediateChar);
    } else {
        spdlog::debug("CSI: {}{}", paramStr, m_finalChar);
    }

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
        case 'u':
            if (m_intermediateChar == '>' || m_intermediateChar == '<' || m_intermediateChar == '?') {
                HandleKittyKeyboard();                      // Kitty keyboard protocol
            } else {
                HandleCursorRestore();                      // RCP
            }
            break;
        case 'S': HandleScrollUp(); break;                  // SU
        case 'T': HandleScrollDown(); break;                // SD
        case 't': HandleWindowManipulation(); break;        // XTWINOPS
        case 'g': HandleTabClear(); break;                  // TBC
        case 'q':
            if (m_intermediateChar == ' ') {
                HandleCursorStyle();                        // DECSCUSR
            } else if (m_intermediateChar == '>') {
                HandleXtversion();                          // XTVERSION
            }
            break;
        case 'p':
            if (m_intermediateChar == '$') {
                HandleDECRQM();                             // DECRQM
            }
            break;
        default:
            // Unhandled CSI sequence - log it
            {
                std::string paramStr;
                for (size_t i = 0; i < m_params.size(); ++i) {
                    if (i > 0) paramStr += ";";
                    paramStr += std::to_string(m_params[i]);
                }
                spdlog::warn("UNHANDLED CSI: {} {} {} (intermediate='{}')",
                             paramStr, m_intermediateChar ? std::string(1, m_intermediateChar) : "",
                             m_finalChar, m_intermediateChar);
            }
            break;
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

void VTStateMachine::SendResponse(const std::string& response) {
    if (m_responseCallback && !response.empty()) {
        // Add ESC prefix for CSI ([), OSC (]), and DCS (P) sequences
        char first = response[0];
        if (first == '[' || first == ']' || first == 'P') {
            std::string fullResponse = "\x1b" + response;
            m_responseCallback(fullResponse);
        } else {
            m_responseCallback(response);
        }
    }
}

} // namespace TerminalDX12::Terminal
