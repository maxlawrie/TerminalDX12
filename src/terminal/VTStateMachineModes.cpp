// VTStateMachineModes.cpp - Mode handling and terminal queries
// Part of VTStateMachine implementation

#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>

namespace TerminalDX12::Terminal {

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
                        int scrollTop = m_screenBuffer->GetScrollRegionTop();
                        int scrollBottom = m_screenBuffer->GetScrollRegionBottom();
                        spdlog::info("DECOM: Origin mode ENABLED, scrollRegion=[{},{}], cursor -> (0,{})",
                                     scrollTop, scrollBottom, scrollTop);
                        m_screenBuffer->SetCursorPos(0, scrollTop);
                    } else {
                        spdlog::info("DECOM: Origin mode DISABLED, cursor -> (0,0)");
                        m_screenBuffer->SetCursorPos(0, 0);
                    }
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
                    m_screenBuffer->UseAlternativeBuffer(set);
                    spdlog::debug("Alt screen buffer (mode {}) {}", mode, set ? "enabled" : "disabled");
                    break;

                case 1049:  // Alternate screen buffer (save cursor, switch, clear)
                    if (set) {
                        // Save cursor state to dedicated 1049 storage (NOT m_savedCursor!)
                        // This prevents interference with DECSC/DECRC (ESC 7/8)
                        m_savedCursor1049.x = m_screenBuffer->GetCursorX();
                        m_savedCursor1049.y = m_screenBuffer->GetCursorY();
                        m_savedCursor1049.attr = m_screenBuffer->GetCurrentAttributes();
                        m_savedCursor1049.originMode = m_originMode;
                        m_savedCursor1049.autoWrap = m_autoWrap;
                        m_savedCursor1049.valid = true;
                        // Switch to alt buffer
                        m_screenBuffer->UseAlternativeBuffer(true);
                        // Clear the alt buffer and reset cursor
                        m_screenBuffer->Clear();
                        m_screenBuffer->SetCursorPos(0, 0);
                        // Reset scroll region and origin mode for alt buffer
                        m_screenBuffer->ResetScrollRegion();
                        m_originMode = false;
                        spdlog::debug("Mode 1049: saved cursor ({},{}), switched to alt buffer, cleared, origin OFF",
                                      m_savedCursor1049.x, m_savedCursor1049.y);
                    } else {
                        // Switch back to main buffer
                        m_screenBuffer->UseAlternativeBuffer(false);
                        // Restore cursor state from dedicated 1049 storage
                        if (m_savedCursor1049.valid) {
                            m_screenBuffer->SetCursorPos(m_savedCursor1049.x, m_savedCursor1049.y);
                            m_screenBuffer->SetCurrentAttributes(m_savedCursor1049.attr);
                            m_originMode = m_savedCursor1049.originMode;
                            m_autoWrap = m_savedCursor1049.autoWrap;
                            spdlog::debug("Mode 1049: switched to main buffer, restored cursor ({},{})",
                                          m_savedCursor1049.x, m_savedCursor1049.y);
                        } else {
                            spdlog::debug("Mode 1049: switched to main buffer, no saved cursor");
                        }
                    }
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

                case 1004:  // Focus reporting
                    m_focusReporting = set;
                    spdlog::debug("Focus reporting {}", set ? "enabled" : "disabled");
                    break;

                case 1005:  // UTF-8 Mouse Mode (legacy, rarely used)
                    // Acknowledge but don't change behavior - SGR (1006) is preferred
                    spdlog::debug("UTF-8 mouse mode {} (ignored, use SGR)", set ? "enabled" : "disabled");
                    break;

                case 1015:  // URXVT Mouse Mode (legacy)
                    // Acknowledge but don't change behavior - SGR (1006) is preferred
                    spdlog::debug("URXVT mouse mode {} (ignored, use SGR)", set ? "enabled" : "disabled");
                    break;

                case 2026:  // Synchronized output (DEC)
                    // This mode buffers output until disabled to prevent flicker
                    // Our renderer is already frame-based, so just acknowledge
                    m_synchronizedOutput = set;
                    spdlog::debug("Synchronized output {}", set ? "enabled" : "disabled");
                    break;

                case 2027:  // Grapheme cluster mode
                    // Affects how combining characters are counted - acknowledge
                    spdlog::debug("Grapheme cluster mode {}", set ? "enabled" : "disabled");
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

void VTStateMachine::HandleCursorStyle() {
    // CSI n SP q - Set cursor style (DECSCUSR): 0/1=blink block, 2=steady block,
    // 3=blink underline, 4=steady underline, 5=blink bar, 6=steady bar
    static constexpr struct { CursorStyle style; bool blink; } kStyles[] = {
        {CursorStyle::BlinkingBlock, true}, {CursorStyle::BlinkingBlock, true},
        {CursorStyle::SteadyBlock, false}, {CursorStyle::BlinkingUnderline, true},
        {CursorStyle::SteadyUnderline, false}, {CursorStyle::BlinkingBar, true},
        {CursorStyle::SteadyBar, false}
    };
    int style = GetParam(0, 0);
    if (style >= 0 && style <= 6) {
        m_cursorStyle = kStyles[style].style;
        m_cursorBlink = kStyles[style].blink;
    } else {
        m_cursorStyle = CursorStyle::BlinkingBlock;
        m_cursorBlink = true;
    }
    spdlog::debug("DECSCUSR: Cursor style set to {} (blink={})", style, m_cursorBlink);
}

void VTStateMachine::HandleKittyKeyboard() {
    // Kitty keyboard protocol (CSI > flags u, CSI < u, CSI ? u)
    // We don't fully implement this, but we need to acknowledge it
    // to prevent apps from getting confused

    if (m_intermediateChar == '>') {
        // CSI > flags u - Push keyboard mode with flags
        int flags = GetParam(0, 0);
        spdlog::debug("Kitty keyboard: push mode flags={}", flags);
        // We don't actually change behavior, just acknowledge
    } else if (m_intermediateChar == '<') {
        // CSI < u - Pop keyboard mode
        spdlog::debug("Kitty keyboard: pop mode");
    } else if (m_intermediateChar == '?') {
        // CSI ? u - Query keyboard mode
        // Response: CSI ? flags u
        // Report 0 (no enhanced mode)
        SendResponse("[?0u");
        spdlog::debug("Kitty keyboard: query mode, reporting 0");
    }
}

void VTStateMachine::HandleXtversion() {
    // CSI > q - Request terminal version (XTVERSION)
    // Response: DCS > | text ST
    std::string response = "P>|TerminalDX12 1.0\\";
    SendResponse(response);
    spdlog::debug("XTVERSION: Reported TerminalDX12 1.0");
}

void VTStateMachine::HandleDECRQM() {
    // CSI ? Ps $ p - Request DEC private mode (DECRQM)
    // Response: CSI ? Ps ; Pm $ y
    // Pm: 0=not recognized, 1=set, 2=reset
    int mode = GetParam(0, 0);
    int status = 0;  // Default: not recognized

    // For private modes (CSI ? Ps $ p), we need a way to detect the ?
    // Since this is called after parsing, check common private modes
    {
        switch (mode) {
            case 1:    status = m_applicationCursorKeys ? 1 : 2; break;
            case 6:    status = m_originMode ? 1 : 2; break;
            case 7:    status = m_autoWrap ? 1 : 2; break;
            case 12:   status = m_cursorBlink ? 1 : 2; break;
            case 25:   status = m_screenBuffer->IsCursorVisible() ? 1 : 2; break;
            case 47:
            case 1047:
            case 1049: status = m_screenBuffer->IsUsingAlternativeBuffer() ? 1 : 2; break;
            case 1000: status = (m_mouseMode == MouseMode::X10) ? 1 : 2; break;
            case 1002: status = (m_mouseMode == MouseMode::Normal) ? 1 : 2; break;
            case 1003: status = (m_mouseMode == MouseMode::All) ? 1 : 2; break;
            case 1004: status = m_focusReporting ? 1 : 2; break;
            case 1006: status = m_sgrMouseMode ? 1 : 2; break;
            case 2004: status = m_bracketedPaste ? 1 : 2; break;
            case 2026: status = m_synchronizedOutput ? 1 : 2; break;
        }
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "[?%d;%d$y", mode, status);
    SendResponse(buf);
    spdlog::debug("DECRQM: Mode {} status {}", mode, status);
}

void VTStateMachine::HandleWindowManipulation() {
    // CSI Ps ; Ps ; Ps t - Window manipulation (XTWINOPS/DECSLPP)
    // We only handle queries, not actual window manipulation
    int op = GetParam(0, 0);

    switch (op) {
        case 14:  // Report text area size in pixels
            // CSI 14 t -> CSI 4 ; height ; width t
            // Report based on cell size * dimensions (estimated)
            {
                int rows = m_screenBuffer->GetRows();
                int cols = m_screenBuffer->GetCols();
                // Estimate: 8 pixels per char width, 16 per height
                char buf[64];
                snprintf(buf, sizeof(buf), "[4;%d;%dt", rows * 16, cols * 8);
                SendResponse(buf);
                spdlog::debug("XTWINOPS 14: Report pixel size -> {}x{}", cols * 8, rows * 16);
            }
            break;

        case 18:  // Report text area size in characters
            // CSI 18 t -> CSI 8 ; rows ; cols t
            {
                int rows = m_screenBuffer->GetRows();
                int cols = m_screenBuffer->GetCols();
                char buf[64];
                snprintf(buf, sizeof(buf), "[8;%d;%dt", rows, cols);
                SendResponse(buf);
                spdlog::debug("XTWINOPS 18: Report char size -> {}x{}", cols, rows);
            }
            break;

        case 19:  // Report screen size in characters
            // CSI 19 t -> CSI 9 ; rows ; cols t
            // Same as 18 for terminal emulators
            {
                int rows = m_screenBuffer->GetRows();
                int cols = m_screenBuffer->GetCols();
                char buf[64];
                snprintf(buf, sizeof(buf), "[9;%d;%dt", rows, cols);
                SendResponse(buf);
                spdlog::debug("XTWINOPS 19: Report screen size -> {}x{}", cols, rows);
            }
            break;

        case 22:  // Push title/icon (save)
            // CSI 22 ; 0 t or CSI 22 ; 2 t - Save window title
            spdlog::debug("XTWINOPS 22: Push title (ignored)");
            break;

        case 23:  // Pop title/icon (restore)
            // CSI 23 ; 0 t or CSI 23 ; 2 t - Restore window title
            spdlog::debug("XTWINOPS 23: Pop title (ignored)");
            break;

        default:
            // Many XTWINOPS are for actual window manipulation (resize, move, etc.)
            // which we don't support
            spdlog::debug("XTWINOPS {}: Unhandled operation", op);
            break;
    }
}

} // namespace TerminalDX12::Terminal
