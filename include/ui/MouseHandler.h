#pragma once

#include <Windows.h>
#include <functional>
#include <string>
#include <chrono>
#include "ui/SearchManager.h"

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

struct MouseHandlerCallbacks {
    std::function<void()> onPaste;
    std::function<void(const std::string&)> onOpenUrl;
    std::function<void(int, int)> onShowContextMenu;
    std::function<void(int)> onSwitchToTab;
    std::function<Pty::ConPtySession*()> getActiveTerminal;
    std::function<Terminal::ScreenBuffer*()> getActiveScreenBuffer;
    std::function<Terminal::VTStateMachine*()> getActiveVTParser;
    std::function<HWND()> getWindowHandle;
    std::function<void(HCURSOR)> setCursor;
    std::function<void()> onDividerResized;
    std::function<PaneManager*()> getPaneManager;  // New: get active tab's PaneManager
};

class MouseHandler {
public:
    MouseHandler(MouseHandlerCallbacks callbacks,
                 SelectionManager& selectionManager,
                 TabManager* tabManager);

    void OnMouseButton(int x, int y, int button, bool down);
    void OnMouseMove(int x, int y);
    bool OnMouseWheel(int x, int y, int delta, bool horizontal = false);
    void SetScreenToCellConverter(std::function<CellPos(int, int)> converter);
    void SetUrlDetector(std::function<std::string(int, int)> detector);
    void SetTabBarHeight(int height) { m_tabBarHeight = height; }
    void SetCharWidth(int width) { m_charWidth = width; }
    bool IsMouseButtonDown() const { return m_mouseButtonDown; }
    int GetLastMouseButton() const { return m_lastMouseButton; }

private:
    bool ShouldReportMouse() const;
    void SendMouseEvent(int cellX, int cellY, int button, bool pressed, bool motion);
    bool HandleTabBarClick(int x, int y, int button, bool down);
    bool HandlePaneFocusClick(int x, int y, int button, bool down);

    MouseHandlerCallbacks m_callbacks;
    SelectionManager& m_selectionManager;
    TabManager* m_tabManager;

    std::function<CellPos(int, int)> m_screenToCell;
    std::function<std::string(int, int)> m_detectUrl;

    int m_tabBarHeight = 30;
    int m_charWidth = 10;
    bool m_mouseButtonDown = false;
    int m_lastMouseButton = -1;

    std::chrono::steady_clock::time_point m_lastClickTime;
    CellPos m_lastClickPos{0, 0};
    int m_clickCount = 0;
};

} // namespace UI
} // namespace TerminalDX12
