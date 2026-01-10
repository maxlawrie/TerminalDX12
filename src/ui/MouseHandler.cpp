#include "ui/MouseHandler.h"
#include "ui/SearchManager.h"
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

MouseHandler::MouseHandler(MouseHandlerCallbacks callbacks, SelectionManager& selectionManager, TabManager* tabManager)
    : m_callbacks(std::move(callbacks)), m_selectionManager(selectionManager), m_tabManager(tabManager) {}

void MouseHandler::SetScreenToCellConverter(std::function<CellPos(int, int)> converter) { m_screenToCell = std::move(converter); }
void MouseHandler::SetUrlDetector(std::function<std::string(int, int)> detector) { m_detectUrl = std::move(detector); }

void MouseHandler::OnMouseButton(int x, int y, int button, bool down) {
    PaneManager* pm = m_callbacks.getPaneManager ? m_callbacks.getPaneManager() : nullptr;
    if (button == 0 && pm) {
        if (down) {
            SplitDirection dir; Pane* divPane = pm->FindDividerAt(x, y, dir);
            if (divPane) { pm->StartDividerResize(divPane, dir == SplitDirection::Horizontal ? x : y); return; }
        } else if (pm->IsResizingDivider()) {
            pm->EndDividerResize();
            if (m_callbacks.onDividerResized) m_callbacks.onDividerResized();
            if (m_callbacks.setCursor) m_callbacks.setCursor(LoadCursor(nullptr, IDC_ARROW));
            return;
        }
    }
    if (HandleTabBarClick(x, y, button, down)) return;
    HandlePaneFocusClick(x, y, button, down);
    CellPos cell = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};
    if (button == 0 && down && (GetAsyncKeyState(VK_CONTROL) & 0x8000) && m_detectUrl) {
        std::string url = m_detectUrl(cell.x, cell.y);
        if (!url.empty() && m_callbacks.onOpenUrl) { m_callbacks.onOpenUrl(url); return; }
    }
    if (button == 1 && down) { if (m_callbacks.onShowContextMenu) m_callbacks.onShowContextMenu(x, y); return; }
    if (button == 2 && down) { if (m_callbacks.onPaste) m_callbacks.onPaste(); return; }
    if (ShouldReportMouse()) { SendMouseEvent(cell.x, cell.y, button, down, false); m_mouseButtonDown = down; m_lastMouseButton = button; return; }
    if (button != 0) return;
    if (down) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();
        m_clickCount = (elapsed < 500 && cell.x == m_lastClickPos.x && cell.y == m_lastClickPos.y) ? std::min(m_clickCount + 1, 3) : 1;
        m_lastClickTime = now; m_lastClickPos = cell;
        Terminal::ScreenBuffer* sb = m_callbacks.getActiveScreenBuffer ? m_callbacks.getActiveScreenBuffer() : nullptr;
        Pane* clickedPane = pm ? pm->FindPaneAt(x, y) : nullptr;
        if (m_clickCount == 3) m_selectionManager.SelectLine(sb, cell.y);
        else if (m_clickCount == 2) m_selectionManager.SelectWord(sb, cell.x, cell.y);
        else if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && m_selectionManager.HasSelection()) m_selectionManager.ExtendSelection({cell.x, cell.y});
        else m_selectionManager.StartSelection({cell.x, cell.y}, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0, clickedPane);
        m_mouseButtonDown = true;
    } else { m_selectionManager.EndSelection(); m_mouseButtonDown = false; }
}

void MouseHandler::OnMouseMove(int x, int y) {
    PaneManager* pm = m_callbacks.getPaneManager ? m_callbacks.getPaneManager() : nullptr;
    if (pm && pm->IsResizingDivider()) { pm->UpdateDividerResize(pm->GetResizeDirection() == SplitDirection::Horizontal ? x : y); return; }
    if (pm) {
        SplitDirection hd; Pane* dp = pm->FindDividerAt(x, y, hd);
        if (dp && m_callbacks.setCursor) m_callbacks.setCursor(LoadCursor(nullptr, hd == SplitDirection::Horizontal ? IDC_SIZEWE : IDC_SIZENS));
        else if (!m_mouseButtonDown && m_callbacks.setCursor) m_callbacks.setCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    CellPos cell = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};
    Terminal::VTStateMachine* vt = m_callbacks.getActiveVTParser ? m_callbacks.getActiveVTParser() : nullptr;
    if (ShouldReportMouse() && vt) {
        auto mm = vt->GetMouseMode();
        if (mm == Terminal::VTStateMachine::MouseMode::All || (mm == Terminal::VTStateMachine::MouseMode::Normal && m_mouseButtonDown))
            SendMouseEvent(cell.x, cell.y, m_lastMouseButton, true, true);
        return;
    }
    if (m_selectionManager.IsSelecting()) m_selectionManager.ExtendSelection({cell.x, cell.y});
}

bool MouseHandler::OnMouseWheel(int x, int y, int delta, bool horizontal) {
    if (!ShouldReportMouse()) return false;
    CellPos cell = m_screenToCell ? m_screenToCell(x, y) : CellPos{0, 0};
    int btn = horizontal ? (delta > 0 ? 66 : 67) : (delta > 0 ? 64 : 65);
    for (int i = 0, n = std::max(1, std::abs(delta) / WHEEL_DELTA); i < n; ++i) SendMouseEvent(cell.x, cell.y, btn, true, false);
    return true;
}

bool MouseHandler::ShouldReportMouse() const {
    Terminal::VTStateMachine* vt = m_callbacks.getActiveVTParser ? m_callbacks.getActiveVTParser() : nullptr;
    return vt && vt->IsMouseReportingEnabled();
}

void MouseHandler::SendMouseEvent(int cellX, int cellY, int button, bool pressed, bool motion) {
    Pty::ConPtySession* t = m_callbacks.getActiveTerminal ? m_callbacks.getActiveTerminal() : nullptr;
    Terminal::VTStateMachine* vt = m_callbacks.getActiveVTParser ? m_callbacks.getActiveVTParser() : nullptr;
    if (!t || !t->IsRunning() || !vt) return;
    int col = cellX + 1, row = cellY + 1; char buf[64]; int len = 0;
    if (vt->IsSGRMouseModeEnabled()) { int cb = button + (motion ? 32 : 0); len = snprintf(buf, sizeof(buf), "\x1b[<%d;%d;%d%c", cb, col, row, pressed ? 'M' : 'm'); }
    else { if (!pressed && vt->GetMouseMode() == Terminal::VTStateMachine::MouseMode::X10) return;
        int cb = button + (motion ? 32 : 0) + (pressed ? 0 : 3); len = snprintf(buf, sizeof(buf), "\x1b[M%c%c%c", cb + 32, std::min(col + 32, 255), std::min(row + 32, 255)); }
    if (len > 0 && len < (int)sizeof(buf)) t->WriteInput(buf, len);
}

bool MouseHandler::HandleTabBarClick(int x, int y, int button, bool down) {
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1 || y >= m_tabBarHeight || button != 0 || !down) return false;
    float tabX = 5.0f;
    for (const auto& tab : m_tabManager->GetTabs()) {
        float tw = (float)std::max(80, std::min((int)tab->GetTitle().length(), 15) * 10 + 20);
        if (x >= tabX && x < tabX + tw) { m_tabManager->SwitchToTab(tab->GetId()); return true; }
        tabX += tw + 5.0f;
    }
    return true;
}

bool MouseHandler::HandlePaneFocusClick(int x, int y, int button, bool down) {
    if (button != 0 || !down) return false;
    PaneManager* pm = m_callbacks.getPaneManager ? m_callbacks.getPaneManager() : nullptr;
    if (!pm) return false;
    Pane* root = pm->GetRootPane();
    if (!root || root->IsLeaf()) return false;
    Pane* clicked = root->FindPaneAt(x, y);
    if (clicked && clicked != pm->GetFocusedPane() && clicked->IsLeaf()) { pm->SetFocusedPane(clicked); return true; }
    return false;
}

} // namespace TerminalDX12::UI
