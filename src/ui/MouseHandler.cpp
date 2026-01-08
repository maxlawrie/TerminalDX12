#include "ui/MouseHandler.h"
#include "ui/SearchManager.h"  // For CellPos
#include "ui/SelectionManager.h"
#include "ui/PaneManager.h"
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include "ui/Pane.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

MouseHandler::MouseHandler(MouseHandlerCallbacks callbacks,
                           SelectionManager& selectionManager,
                           PaneManager& paneManager,
                           TabManager* tabManager)
    : m_callbacks(std::move(callbacks))
    , m_selectionManager(selectionManager)
    , m_paneManager(paneManager)
    , m_tabManager(tabManager)
{
}

void MouseHandler::SetScreenToCellConverter(std::function<CellPos(int, int)> converter) {
    m_screenToCell = std::move(converter);
}

void MouseHandler::SetUrlDetector(std::function<std::string(int, int)> detector) {
    m_detectUrl = std::move(detector);
}

void MouseHandler::OnMouseButton(int x, int y, int button, bool down) {
    // Handle divider resize
    if (button == 0) {
        if (down) {
            // Check if clicking on a divider
            SplitDirection direction;
            Pane* dividerPane = m_paneManager.FindDividerAt(x, y, direction);
            if (dividerPane) {
                int startPos = (direction == SplitDirection::Horizontal) ? x : y;
                m_paneManager.StartDividerResize(dividerPane, startPos);
                return;
            }
        } else {
            // Release - end any active resize
            if (m_paneManager.IsResizingDivider()) {
                m_paneManager.EndDividerResize();
                if (m_callbacks.onDividerResized) {
                    m_callbacks.onDividerResized();
                }
                // Reset cursor to arrow
                if (m_callbacks.setCursor) {
                    m_callbacks.setCursor(LoadCursor(nullptr, IDC_ARROW));
                }
                return;
            }
        }
    }

    // Handle tab bar clicks
    if (HandleTabBarClick(x, y, button, down)) {
        return;
    }

    // Handle pane focus clicks
    if (HandlePaneFocusClick(x, y, button, down)) {
        // Continue processing - don't return, we want selection to work too
    }

    // Convert screen coordinates to cell position
    CellPos cellPos = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};

    // Handle Ctrl+Click for URL opening
    if (button == 0 && down && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
        if (m_detectUrl) {
            std::string url = m_detectUrl(cellPos.x, cellPos.y);
            if (!url.empty() && m_callbacks.onOpenUrl) {
                m_callbacks.onOpenUrl(url);
                return;
            }
        }
    }

    // Right-click context menu
    if (button == 1 && down) {
        if (m_callbacks.onShowContextMenu) {
            m_callbacks.onShowContextMenu(x, y);
        }
        return;
    }

    // Middle-click paste (X11 style)
    if (button == 2 && down) {
        if (m_callbacks.onPaste) {
            m_callbacks.onPaste();
        }
        return;
    }

    // Report mouse to application if mouse mode is enabled
    if (ShouldReportMouse()) {
        SendMouseEvent(cellPos.x, cellPos.y, button, down, false);
        m_mouseButtonDown = down;
        m_lastMouseButton = button;
        return;
    }

    // Only handle left mouse button for selection
    if (button != 0) {
        return;
    }

    if (down) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();

        // Check for double/triple click (within 500ms and same position)
        if (elapsed < 500 && cellPos.x == m_lastClickPos.x && cellPos.y == m_lastClickPos.y) {
            m_clickCount++;
            if (m_clickCount > 3) m_clickCount = 1;
        } else {
            m_clickCount = 1;
        }

        m_lastClickTime = now;
        m_lastClickPos = cellPos;

        // Get screen buffer for selection operations
        Terminal::ScreenBuffer* screenBuffer = m_callbacks.getActiveScreenBuffer ?
            m_callbacks.getActiveScreenBuffer() : nullptr;

        // Find which pane was clicked for selection tracking
        Pane* clickedPane = m_paneManager.FindPaneAt(x, y);

        // Handle click type
        if (m_clickCount == 3) {
            // Triple-click: select entire line
            m_selectionManager.SelectLine(screenBuffer, cellPos.y);
        } else if (m_clickCount == 2) {
            // Double-click: select word
            m_selectionManager.SelectWord(screenBuffer, cellPos.x, cellPos.y);
        } else {
            // Single-click: start drag selection
            // Check for Shift+Click to extend selection
            if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && m_selectionManager.HasSelection()) {
                // Extend selection to this position
                SelectionPos pos{cellPos.x, cellPos.y};
                m_selectionManager.ExtendSelection(pos);
            } else {
                // Start new selection
                // Alt+drag enables rectangle selection mode
                bool rectMode = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
                SelectionPos pos{cellPos.x, cellPos.y};
                m_selectionManager.StartSelection(pos, rectMode, clickedPane);
            }
        }
        m_mouseButtonDown = true;
    } else {
        // Button release
        m_selectionManager.EndSelection();
        m_mouseButtonDown = false;
    }
}

void MouseHandler::OnMouseMove(int x, int y) {
    // Handle active divider resize
    if (m_paneManager.IsResizingDivider()) {
        SplitDirection direction = m_paneManager.GetResizeDirection();
        int pos = (direction == SplitDirection::Horizontal) ? x : y;
        m_paneManager.UpdateDividerResize(pos);
        return;
    }

    // Update cursor for divider hover
    SplitDirection hoverDirection;
    Pane* dividerPane = m_paneManager.FindDividerAt(x, y, hoverDirection);
    if (dividerPane) {
        if (m_callbacks.setCursor) {
            HCURSOR cursor = (hoverDirection == SplitDirection::Horizontal) ?
                LoadCursor(nullptr, IDC_SIZEWE) : LoadCursor(nullptr, IDC_SIZENS);
            m_callbacks.setCursor(cursor);
        }
    } else if (!m_mouseButtonDown) {
        // Reset cursor when not over divider (and not selecting)
        if (m_callbacks.setCursor) {
            m_callbacks.setCursor(LoadCursor(nullptr, IDC_ARROW));
        }
    }

    CellPos cellPos = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};

    // Report mouse motion if application wants it
    Terminal::VTStateMachine* vtParser = m_callbacks.getActiveVTParser ?
        m_callbacks.getActiveVTParser() : nullptr;

    if (ShouldReportMouse() && vtParser) {
        auto mouseMode = vtParser->GetMouseMode();
        // Mode 1003 (All) reports all motion
        // Mode 1002 (Normal) reports motion only while button pressed
        if (mouseMode == Terminal::VTStateMachine::MouseMode::All ||
            (mouseMode == Terminal::VTStateMachine::MouseMode::Normal && m_mouseButtonDown)) {
            SendMouseEvent(cellPos.x, cellPos.y, m_lastMouseButton, true, true);
        }
        return;
    }

    if (!m_selectionManager.IsSelecting()) {
        return;
    }

    SelectionPos pos{cellPos.x, cellPos.y};
    m_selectionManager.ExtendSelection(pos);
}

bool MouseHandler::OnMouseWheel(int x, int y, int delta, bool horizontal) {
    // If mouse mode is not enabled, return false to let caller handle scrollback
    if (!ShouldReportMouse()) {
        return false;
    }

    // Convert screen coordinates to cell position
    CellPos cellPos = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};

    // Mouse wheel button encoding (xterm):
    // Button 64 = wheel up (button 4)
    // Button 65 = wheel down (button 5)
    // Button 66 = wheel left (button 6) - horizontal
    // Button 67 = wheel right (button 7) - horizontal
    int button;
    if (horizontal) {
        button = (delta > 0) ? 66 : 67;  // Left or right
    } else {
        button = (delta > 0) ? 64 : 65;  // Up or down
    }

    // Send one event per WHEEL_DELTA (120 units = 1 notch)
    // Most apps expect one event per scroll notch
    int notches = std::abs(delta) / WHEEL_DELTA;
    if (notches == 0) notches = 1;  // Always send at least one event

    for (int i = 0; i < notches; ++i) {
        SendMouseEvent(cellPos.x, cellPos.y, button, true, false);
    }

    return true;
}

bool MouseHandler::ShouldReportMouse() const {
    Terminal::VTStateMachine* vtParser = m_callbacks.getActiveVTParser ?
        m_callbacks.getActiveVTParser() : nullptr;
    return vtParser && vtParser->IsMouseReportingEnabled();
}

void MouseHandler::SendMouseEvent(int cellX, int cellY, int button, bool pressed, bool motion) {
    Pty::ConPtySession* terminal = m_callbacks.getActiveTerminal ?
        m_callbacks.getActiveTerminal() : nullptr;
    Terminal::VTStateMachine* vtParser = m_callbacks.getActiveVTParser ?
        m_callbacks.getActiveVTParser() : nullptr;

    if (!terminal || !terminal->IsRunning() || !vtParser) {
        return;
    }

    // Convert to 1-based coordinates for terminal
    int col = cellX + 1;
    int row = cellY + 1;

    // Build mouse event sequence
    char buf[64];
    int len = 0;

    if (vtParser->IsSGRMouseModeEnabled()) {
        // SGR Extended Mouse Mode (1006): CSI < Cb ; Cx ; Cy M/m
        // Button encoding: 0=left, 1=middle, 2=right, 32+button for motion
        int cb = button;
        if (motion) cb += 32;

        char terminator = pressed ? 'M' : 'm';
        len = snprintf(buf, sizeof(buf), "\x1b[<%d;%d;%d%c", cb, col, row, terminator);
    } else {
        // X10/Normal Mouse Mode: CSI M Cb Cx Cy (encoded as Cb+32, Cx+32, Cy+32)
        // Only for press events in X10 mode
        if (!pressed && vtParser->GetMouseMode() == Terminal::VTStateMachine::MouseMode::X10) {
            return;
        }

        int cb = button;
        if (motion) cb += 32;
        if (!pressed) cb += 3;  // Release is encoded as button 3

        // Coordinates are encoded with +32 offset, clamped to 223 (255-32)
        int cx = std::min(col + 32, 255);
        int cy = std::min(row + 32, 255);
        cb += 32;

        len = snprintf(buf, sizeof(buf), "\x1b[M%c%c%c", cb, cx, cy);
    }

    if (len > 0 && len < static_cast<int>(sizeof(buf))) {
        terminal->WriteInput(buf, len);
    }
}

bool MouseHandler::HandleTabBarClick(int x, int y, int button, bool down) {
    // Only handle left click in tab bar when multiple tabs exist
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1) {
        return false;
    }

    if (y >= m_tabBarHeight || button != 0 || !down) {
        return false;
    }

    // Find which tab was clicked
    float tabX = 5.0f;
    const int charWidth = 10;
    const auto& tabs = m_tabManager->GetTabs();

    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& tab = tabs[i];
        std::wstring wTitle = tab->GetTitle();
        int titleLen = static_cast<int>(wTitle.length());
        if (titleLen > 15) titleLen = 15;

        float tabWidth = static_cast<float>(std::max(80, titleLen * charWidth + 20));

        if (x >= tabX && x < tabX + tabWidth) {
            // Switch to this tab
            m_tabManager->SwitchToTab(tab->GetId());
            return true;
        }
        tabX += tabWidth + 5.0f;
    }

    return true;  // Click was in tab bar but not on a tab
}

bool MouseHandler::HandlePaneFocusClick(int x, int y, int button, bool down) {
    if (button != 0 || !down) {
        return false;
    }

    Pane* rootPane = m_paneManager.GetRootPane();
    if (!rootPane || rootPane->IsLeaf()) {
        return false;
    }

    Pane* clickedPane = rootPane->FindPaneAt(x, y);
    if (clickedPane && clickedPane != m_paneManager.GetFocusedPane() && clickedPane->IsLeaf()) {
        m_paneManager.SetFocusedPane(clickedPane);
        spdlog::info("Focused pane at ({}, {})", x, y);
        return true;
    }

    return false;
}

} // namespace TerminalDX12::UI
