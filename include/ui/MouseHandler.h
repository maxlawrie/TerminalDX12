#pragma once

#include <Windows.h>
#include <functional>
#include <string>
#include <chrono>
#include "ui/SearchManager.h"  // For CellPos

namespace TerminalDX12 {

namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}

namespace Pty {
    class ConPtySession;
}

namespace UI {

class SelectionManager;
class PaneManager;
class TabManager;
struct SelectionPos;

/**
 * @brief Callback interface for MouseHandler to notify Application of actions.
 */
struct MouseHandlerCallbacks {
    std::function<void()> onPaste;
    std::function<void(const std::string&)> onOpenUrl;
    std::function<void(int, int)> onShowContextMenu;
    std::function<void(int)> onSwitchToTab;  // Tab ID
    std::function<Pty::ConPtySession*()> getActiveTerminal;
    std::function<Terminal::ScreenBuffer*()> getActiveScreenBuffer;
    std::function<Terminal::VTStateMachine*()> getActiveVTParser;
    std::function<HWND()> getWindowHandle;
    std::function<void(HCURSOR)> setCursor;
    std::function<void()> onDividerResized;  // Called when divider resize ends
};

/**
 * @brief Handles mouse input for the terminal application.
 *
 * MouseHandler processes mouse events and translates them into:
 * - Text selection (click, drag, double-click, triple-click)
 * - Mouse reporting to terminal applications (X10, Normal, SGR modes)
 * - Tab switching (clicks on tab bar)
 * - Pane focusing (clicks in panes)
 * - URL opening (Ctrl+click)
 * - Context menu (right-click)
 * - Paste (middle-click)
 */
class MouseHandler {
public:
    /**
     * @brief Construct MouseHandler with required dependencies.
     * @param callbacks Callbacks for triggering Application actions.
     * @param selectionManager Reference to SelectionManager for selection state.
     * @param paneManager Reference to PaneManager for pane focus.
     * @param tabManager Pointer to TabManager for tab operations.
     */
    MouseHandler(MouseHandlerCallbacks callbacks,
                 SelectionManager& selectionManager,
                 PaneManager& paneManager,
                 TabManager* tabManager);

    /**
     * @brief Handle mouse button press/release.
     * @param x Screen X coordinate.
     * @param y Screen Y coordinate.
     * @param button Button index (0=left, 1=right, 2=middle).
     * @param down True if pressed, false if released.
     */
    void OnMouseButton(int x, int y, int button, bool down);

    /**
     * @brief Handle mouse movement.
     * @param x Screen X coordinate.
     * @param y Screen Y coordinate.
     */
    void OnMouseMove(int x, int y);

    /**
     * @brief Handle mouse wheel scroll.
     * @param x Screen X coordinate.
     * @param y Screen Y coordinate.
     * @param delta Scroll delta (positive = up, negative = down).
     * @param horizontal True for horizontal scroll (WM_MOUSEHWHEEL).
     * @return True if event was sent to application, false if local scroll should occur.
     */
    bool OnMouseWheel(int x, int y, int delta, bool horizontal = false);

    /**
     * @brief Set the screen-to-cell conversion function.
     * @param converter Function that converts screen coordinates to cell position.
     */
    void SetScreenToCellConverter(std::function<CellPos(int, int)> converter);

    /**
     * @brief Set the URL detection function.
     * @param detector Function that detects URL at cell position.
     */
    void SetUrlDetector(std::function<std::string(int, int)> detector);

    /**
     * @brief Set tab bar height for click detection.
     * @param height Height in pixels.
     */
    void SetTabBarHeight(int height) { m_tabBarHeight = height; }

    /**
     * @brief Check if mouse button is currently pressed.
     */
    bool IsMouseButtonDown() const { return m_mouseButtonDown; }

    /**
     * @brief Get the last mouse button pressed.
     */
    int GetLastMouseButton() const { return m_lastMouseButton; }

private:
    /**
     * @brief Check if terminal wants mouse events reported.
     */
    bool ShouldReportMouse() const;

    /**
     * @brief Send mouse event to terminal in appropriate format.
     */
    void SendMouseEvent(int cellX, int cellY, int button, bool pressed, bool motion);

    /**
     * @brief Handle tab bar click.
     * @return True if click was in tab bar.
     */
    bool HandleTabBarClick(int x, int y, int button, bool down);

    /**
     * @brief Handle pane focus click.
     * @return True if pane focus changed.
     */
    bool HandlePaneFocusClick(int x, int y, int button, bool down);

    MouseHandlerCallbacks m_callbacks;
    SelectionManager& m_selectionManager;
    PaneManager& m_paneManager;
    TabManager* m_tabManager;

    std::function<CellPos(int, int)> m_screenToCell;
    std::function<std::string(int, int)> m_detectUrl;

    int m_tabBarHeight = 30;
    bool m_mouseButtonDown = false;
    int m_lastMouseButton = -1;

    // Click tracking for double/triple click
    std::chrono::steady_clock::time_point m_lastClickTime;
    CellPos m_lastClickPos{0, 0};
    int m_clickCount = 0;
};

} // namespace UI
} // namespace TerminalDX12
