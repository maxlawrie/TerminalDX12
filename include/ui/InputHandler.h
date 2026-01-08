#pragma once

#include <Windows.h>
#include <string>
#include <functional>

namespace TerminalDX12 {

namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}

namespace Pty {
    class ConPtySession;
}

namespace UI {

class SearchManager;
class SelectionManager;
class TabManager;
class PaneManager;
enum class SplitDirection;

/**
 * @brief Callback interface for InputHandler to notify Application of actions.
 *
 * These callbacks allow InputHandler to trigger Application-level actions
 * without depending on Application directly.
 */
struct InputHandlerCallbacks {
    std::function<void()> onShowSettings;
    std::function<void()> onNewWindow;
    std::function<void()> onQuit;
    std::function<void()> onNewTab;
    std::function<void()> onCloseTab;
    std::function<void()> onNextTab;
    std::function<void()> onPreviousTab;
    std::function<void(int)> onSwitchToTab;  // Tab index (0-based)
    std::function<void(SplitDirection)> onSplitPane;
    std::function<void()> onClosePane;
    std::function<void()> onToggleZoom;
    std::function<void()> onFocusNextPane;
    std::function<void()> onFocusPreviousPane;
    std::function<void()> onOpenSearch;
    std::function<void()> onCloseSearch;
    std::function<void()> onSearchNext;
    std::function<void()> onSearchPrevious;
    std::function<void()> onUpdateSearchResults;
    std::function<void()> onCopySelection;
    std::function<void()> onPaste;
    std::function<void()> onUpdatePaneLayout;
    std::function<Pty::ConPtySession*()> getActiveTerminal;
    std::function<Terminal::ScreenBuffer*()> getActiveScreenBuffer;
    std::function<int()> getTabCount;
    std::function<bool()> hasMultiplePanes;
    std::function<const std::wstring&()> getShellCommand;
    std::function<Terminal::VTStateMachine*()> getVTParser;
};

/**
 * @brief Handles keyboard and mouse input for the terminal application.
 *
 * InputHandler processes raw input events and translates them into either:
 * - Direct terminal input (sent via Win32 input mode to ConPTY)
 * - Application-level actions (triggered via callbacks)
 *
 * This class manages the complexity of:
 * - Keyboard shortcuts (Ctrl+T, Ctrl+W, etc.)
 * - Search mode text input
 * - Win32 input mode encoding for special keys
 * - Scroll navigation
 * - Pane focus navigation
 */
class InputHandler {
public:
    /**
     * @brief Construct InputHandler with required dependencies.
     * @param callbacks Callbacks for triggering Application actions.
     * @param searchManager Reference to SearchManager for search state.
     * @param selectionManager Reference to SelectionManager for selection state.
     */
    InputHandler(InputHandlerCallbacks callbacks,
                 SearchManager& searchManager,
                 SelectionManager& selectionManager);

    /**
     * @brief Handle character input (from WM_CHAR).
     * @param ch Unicode character received.
     */
    void OnChar(wchar_t ch);

    /**
     * @brief Handle key input (from WM_KEYDOWN/WM_KEYUP).
     * @param key Virtual key code.
     * @param isDown True if key down, false if key up.
     */
    void OnKey(UINT key, bool isDown);

    /**
     * @brief Handle mouse wheel scroll.
     * @param delta Scroll delta (positive = up, negative = down).
     */
    void OnMouseWheel(int delta);

private:
    /**
     * @brief Send a key using Win32 input mode format.
     * @param vk Virtual key code.
     * @param unicodeChar Unicode character (0 for non-printable).
     * @param keyDown True for key down event.
     * @param controlState Modifier key state flags.
     */
    void SendWin32InputKey(UINT vk, wchar_t unicodeChar, bool keyDown, DWORD controlState);

    /**
     * @brief Handle keyboard shortcuts (Ctrl+key combinations).
     * @param key Virtual key code.
     * @return True if shortcut was handled.
     */
    bool HandleCtrlShortcuts(UINT key);

    /**
     * @brief Handle Alt+Arrow pane navigation.
     * @param key Virtual key code.
     * @return True if navigation was handled.
     */
    bool HandleAltArrowNavigation(UINT key);

    /**
     * @brief Handle special keys (arrows, function keys, etc.).
     * @param key Virtual key code.
     * @param controlState Modifier key state.
     * @return True if key was handled.
     */
    bool HandleSpecialKeys(UINT key, DWORD controlState);

    /**
     * @brief Handle scrollback navigation (Shift+PageUp/Down).
     * @param key Virtual key code.
     * @return True if navigation was handled.
     */
    bool HandleScrollbackNavigation(UINT key);

    /**
     * @brief Handle search mode input.
     * @param key Virtual key code.
     * @return True if key was consumed by search mode.
     */
    bool HandleSearchModeKey(UINT key);

    InputHandlerCallbacks m_callbacks;
    SearchManager& m_searchManager;
    SelectionManager& m_selectionManager;
};

} // namespace UI
} // namespace TerminalDX12
