#include "ui/InputHandler.h"
#include "ui/SearchManager.h"
#include "ui/SelectionManager.h"
#include "ui/Pane.h"
#include "terminal/ScreenBuffer.h"
#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>

namespace TerminalDX12::UI {

InputHandler::InputHandler(InputHandlerCallbacks callbacks,
                           SearchManager& searchManager,
                           SelectionManager& selectionManager)
    : m_callbacks(std::move(callbacks))
    , m_searchManager(searchManager)
    , m_selectionManager(selectionManager)
{
}

void InputHandler::OnChar(wchar_t ch) {
    // Handle search mode text input
    if (m_searchManager.IsActive()) {
        if (ch >= 32) {
            m_searchManager.AppendChar(ch);
            if (m_callbacks.onUpdateSearchResults) {
                m_callbacks.onUpdateSearchResults();
            }
        }
        return;
    }

    Pty::ConPtySession* terminal = m_callbacks.getActiveTerminal ? m_callbacks.getActiveTerminal() : nullptr;
    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    // Skip control characters that are handled by OnKey via WM_KEYDOWN
    // These would otherwise be sent twice (once by WM_KEYDOWN, once by WM_CHAR)
    if (ch == L'\b' ||   // Backspace - handled by VK_BACK
        ch == L'\t' ||   // Tab - handled by VK_TAB
        ch == L'\r' ||   // Enter - handled by VK_RETURN
        ch == L'\x1b') { // Escape - handled by VK_ESCAPE
        return;
    }

    // Convert wchar_t to UTF-8
    char utf8[4];
    int len = 0;

    if (ch < 0x80) {
        utf8[0] = static_cast<char>(ch);
        len = 1;
    } else if (ch < 0x800) {
        utf8[0] = static_cast<char>(0xC0 | (ch >> 6));
        utf8[1] = static_cast<char>(0x80 | (ch & 0x3F));
        len = 2;
    } else {
        utf8[0] = static_cast<char>(0xE0 | (ch >> 12));
        utf8[1] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
        utf8[2] = static_cast<char>(0x80 | (ch & 0x3F));
        len = 3;
    }

    terminal->WriteInput(utf8, len);
}

void InputHandler::OnKey(UINT key, bool isDown) {
    if (!isDown) {
        return;
    }

    // Handle search mode input
    if (m_searchManager.IsActive()) {
        if (HandleSearchModeKey(key)) {
            return;
        }
        // Other keys handled by OnChar for text input
        return;
    }

    // Handle Ctrl+key shortcuts
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        if (HandleCtrlShortcuts(key)) {
            return;
        }
    }

    // Alt+Arrow: Navigate between panes
    if (GetAsyncKeyState(VK_MENU) & 0x8000) {
        if (HandleAltArrowNavigation(key)) {
            return;
        }
    }

    // Ctrl modifier handling for prompt navigation and copy/paste
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        Terminal::ScreenBuffer* screenBuffer = m_callbacks.getActiveScreenBuffer ? m_callbacks.getActiveScreenBuffer() : nullptr;
        Pty::ConPtySession* terminal = m_callbacks.getActiveTerminal ? m_callbacks.getActiveTerminal() : nullptr;

        // Ctrl+Up/Down: Navigate between shell prompts (OSC 133)
        if (key == VK_UP && screenBuffer) {
            int currentLine = screenBuffer->GetCursorY() +
                             (screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                             screenBuffer->GetRows() + screenBuffer->GetScrollOffset();
            int prevPrompt = screenBuffer->GetPreviousPromptLine(currentLine);
            if (prevPrompt >= 0) {
                int scrollbackTotal = static_cast<int>(screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                                     screenBuffer->GetRows();
                int targetOffset = std::max(0, scrollbackTotal - prevPrompt + screenBuffer->GetRows() / 2);
                screenBuffer->SetScrollOffset(targetOffset);
                spdlog::debug("Jumped to previous prompt at line {}", prevPrompt);
            }
            return;
        }
        if (key == VK_DOWN && screenBuffer) {
            int currentLine = screenBuffer->GetCursorY() +
                             (screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                             screenBuffer->GetRows() + screenBuffer->GetScrollOffset();
            int nextPrompt = screenBuffer->GetNextPromptLine(currentLine);
            if (nextPrompt >= 0) {
                int scrollbackTotal = static_cast<int>(screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                                     screenBuffer->GetRows();
                int targetOffset = std::max(0, scrollbackTotal - nextPrompt + screenBuffer->GetRows() / 2);
                screenBuffer->SetScrollOffset(targetOffset);
                spdlog::debug("Jumped to next prompt at line {}", nextPrompt);
            } else {
                screenBuffer->ScrollToBottom();
            }
            return;
        }

        // Copy/Paste
        if (key == 'C') {
            if (m_selectionManager.HasSelection()) {
                if (m_callbacks.onCopySelection) {
                    m_callbacks.onCopySelection();
                }
            } else if (terminal && terminal->IsRunning()) {
                // Send SIGINT (Ctrl+C) to terminal
                terminal->WriteInput("\x03", 1);
            }
            return;
        }
        if (key == 'V') {
            if (m_callbacks.onPaste) {
                m_callbacks.onPaste();
            }
            return;
        }
    }

    Pty::ConPtySession* terminal = m_callbacks.getActiveTerminal ? m_callbacks.getActiveTerminal() : nullptr;

    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    // Handle scrollback navigation
    if (HandleScrollbackNavigation(key)) {
        return;
    }

    // Get control key state
    DWORD controlState = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) controlState |= SHIFT_PRESSED;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) controlState |= LEFT_CTRL_PRESSED;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) controlState |= LEFT_ALT_PRESSED;

    // Handle special keys
    HandleSpecialKeys(key, controlState);
}

void InputHandler::OnMouseWheel(int delta) {
    Terminal::ScreenBuffer* screenBuffer = m_callbacks.getActiveScreenBuffer ? m_callbacks.getActiveScreenBuffer() : nullptr;
    if (!screenBuffer) {
        return;
    }

    // Mouse wheel delta is in units of WHEEL_DELTA (120)
    // Scroll 3 lines per notch
    int lines = (delta / WHEEL_DELTA) * 3;
    int currentOffset = screenBuffer->GetScrollOffset();
    screenBuffer->SetScrollOffset(currentOffset + lines);
}

void InputHandler::SendWin32InputKey(UINT vk, wchar_t unicodeChar, bool keyDown, DWORD controlState) {
    Pty::ConPtySession* terminal = m_callbacks.getActiveTerminal ? m_callbacks.getActiveTerminal() : nullptr;
    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    // Map virtual key to scan code
    UINT scanCode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);

    // Win32 input mode format: ESC [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "\x1b[%u;%u;%u;%u;%u;1_",
                       vk, scanCode, static_cast<unsigned>(unicodeChar),
                       keyDown ? 1 : 0, controlState);
    terminal->WriteInput(buf, len);
}

bool InputHandler::HandleCtrlShortcuts(UINT key) {
    // Ctrl+Comma: Open settings
    if (key == VK_OEM_COMMA) {
        if (m_callbacks.onShowSettings) {
            m_callbacks.onShowSettings();
        }
        return true;
    }

    // Ctrl+Shift+N: New window
    if (key == 'N' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        if (m_callbacks.onNewWindow) {
            m_callbacks.onNewWindow();
        }
        return true;
    }

    // Ctrl+Shift+F: Open search
    if (key == 'F' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        if (m_callbacks.onOpenSearch) {
            m_callbacks.onOpenSearch();
        }
        return true;
    }

    // Tab management shortcuts
    if (key == 'T') {
        // Ctrl+T: New tab
        if (m_callbacks.onNewTab) {
            m_callbacks.onNewTab();
        }
        return true;
    }

    if (key == 'W') {
        // Ctrl+Shift+W: Close focused pane (if more than one pane)
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            if (m_callbacks.onClosePane) {
                m_callbacks.onClosePane();
            }
            return true;
        }
        // Ctrl+W: Close active tab
        int tabCount = m_callbacks.getTabCount ? m_callbacks.getTabCount() : 0;
        if (tabCount > 1) {
            if (m_callbacks.onCloseTab) {
                m_callbacks.onCloseTab();
            }
        } else if (tabCount == 1) {
            // Last tab - close window
            if (m_callbacks.onQuit) {
                m_callbacks.onQuit();
            }
        }
        return true;
    }

    if (key == VK_TAB) {
        // Ctrl+Tab / Ctrl+Shift+Tab: Switch tabs
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
            if (m_callbacks.onPreviousTab) {
                m_callbacks.onPreviousTab();
            }
        } else {
            if (m_callbacks.onNextTab) {
                m_callbacks.onNextTab();
            }
        }
        return true;
    }

    // Ctrl+1-9: Switch to tab by number
    if (key >= '1' && key <= '9') {
        int tabIndex = key - '1';  // '1' -> 0, '2' -> 1, etc.
        if (m_callbacks.onSwitchToTab) {
            m_callbacks.onSwitchToTab(tabIndex);
        }
        return true;
    }

    // Ctrl+Shift+D: Split pane vertically (left/right)
    if (key == 'D' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        if (m_callbacks.onSplitPane) {
            m_callbacks.onSplitPane(SplitDirection::Horizontal);
        }
        return true;
    }

    // Ctrl+Shift+E: Split pane horizontally (top/bottom)
    if (key == 'E' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        if (m_callbacks.onSplitPane) {
            m_callbacks.onSplitPane(SplitDirection::Vertical);
        }
        return true;
    }

    // Ctrl+Shift+Z: Toggle zoom on focused pane
    if (key == 'Z' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        bool hasMultiple = m_callbacks.hasMultiplePanes ? m_callbacks.hasMultiplePanes() : false;
        if (hasMultiple) {
            if (m_callbacks.onToggleZoom) {
                m_callbacks.onToggleZoom();
            }
        }
        return true;
    }

    return false;
}

bool InputHandler::HandleAltArrowNavigation(UINT key) {
    if (key == VK_LEFT || key == VK_UP) {
        if (m_callbacks.onFocusPreviousPane) m_callbacks.onFocusPreviousPane();
        return true;
    }
    if (key == VK_RIGHT || key == VK_DOWN) {
        if (m_callbacks.onFocusNextPane) m_callbacks.onFocusNextPane();
        return true;
    }
    return false;
}

bool InputHandler::HandleSpecialKeys(UINT key, DWORD controlState) {
    static const std::pair<UINT, wchar_t> charKeys[] = {
        {VK_BACK, L'\b'}, {VK_RETURN, L'\r'}, {VK_TAB, L'\t'}, {VK_ESCAPE, L'\x1b'}
    };
    for (auto [vk, ch] : charKeys) {
        if (key == vk) { SendWin32InputKey(vk, ch, true, controlState); return true; }
    }

    static const UINT navKeys[] = {
        VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_HOME, VK_END,
        VK_PRIOR, VK_NEXT, VK_INSERT, VK_DELETE
    };
    for (UINT vk : navKeys) {
        if (key == vk) { SendWin32InputKey(vk, 0, true, controlState); return true; }
    }

    if (key >= VK_F1 && key <= VK_F12) {
        SendWin32InputKey(key, 0, true, controlState);
        return true;
    }
    return false;
}

bool InputHandler::HandleScrollbackNavigation(UINT key) {
    if (!(GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
        return false;
    }

    Terminal::ScreenBuffer* screenBuffer = m_callbacks.getActiveScreenBuffer ? m_callbacks.getActiveScreenBuffer() : nullptr;
    if (!screenBuffer) {
        return false;
    }

    if (key == VK_PRIOR) {  // Shift+Page Up
        int currentOffset = screenBuffer->GetScrollOffset();
        screenBuffer->SetScrollOffset(currentOffset + screenBuffer->GetRows());
        return true;
    } else if (key == VK_NEXT) {  // Shift+Page Down
        int currentOffset = screenBuffer->GetScrollOffset();
        screenBuffer->SetScrollOffset(std::max(0, currentOffset - screenBuffer->GetRows()));
        return true;
    }
    return false;
}

bool InputHandler::HandleSearchModeKey(UINT key) {
    if (key == VK_ESCAPE) {
        if (m_callbacks.onCloseSearch) m_callbacks.onCloseSearch();
        return true;
    }
    if (key == VK_RETURN || key == VK_F3) {
        bool prev = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        if (prev && m_callbacks.onSearchPrevious) m_callbacks.onSearchPrevious();
        else if (!prev && m_callbacks.onSearchNext) m_callbacks.onSearchNext();
        return true;
    }
    if (key == VK_BACK && !m_searchManager.GetQuery().empty()) {
        m_searchManager.Backspace();
        if (m_callbacks.onUpdateSearchResults) m_callbacks.onUpdateSearchResults();
        return true;
    }
    return false;
}

} // namespace TerminalDX12::UI
