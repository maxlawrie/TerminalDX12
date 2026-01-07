#include "ui/PaneManager.h"
#include "ui/Tab.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

void PaneManager::Initialize(Tab* initialTab) {
    if (initialTab) {
        m_rootPane = std::make_unique<Pane>(initialTab);
        m_focusedPane = m_rootPane.get();
    }
}

void PaneManager::ToggleZoom() {
    if (m_rootPane && !m_rootPane->IsLeaf()) {
        m_paneZoomed = !m_paneZoomed;
        spdlog::info("Pane zoom {}", m_paneZoomed ? "enabled" : "disabled");
    }
}

Pane* PaneManager::SplitFocusedPane(SplitDirection direction, Tab* newTab) {
    if (!m_focusedPane || !m_focusedPane->IsLeaf()) {
        spdlog::warn("Cannot split: no focused leaf pane");
        return nullptr;
    }

    if (!newTab) {
        spdlog::error("Cannot split: no tab provided");
        return nullptr;
    }

    // Split the focused pane
    Pane* newPane = m_focusedPane->Split(direction, newTab);
    if (newPane) {
        // Focus the new pane
        m_focusedPane = newPane;
        spdlog::info("Split pane {}", direction == SplitDirection::Horizontal ? "horizontally" : "vertically");
    }

    return newPane;
}

Tab* PaneManager::CloseFocusedPane() {
    if (!m_focusedPane || !m_rootPane) {
        return nullptr;
    }

    // If there's only one pane, don't close it
    std::vector<Pane*> leaves;
    m_rootPane->GetAllLeafPanes(leaves);
    if (leaves.size() <= 1) {
        spdlog::debug("Cannot close: only one pane remaining");
        return nullptr;
    }

    Pane* parent = m_focusedPane->GetParent();
    if (!parent) {
        // Focused pane is the root - shouldn't happen if leaves > 1
        return nullptr;
    }

    // Get the tab from the pane we're closing
    Tab* tabToClose = m_focusedPane->GetTab();

    // Find another pane to focus
    Pane* newFocus = m_rootPane->FindAdjacentPane(m_focusedPane, SplitDirection::Horizontal, true);

    // Close the child in the parent
    if (parent->CloseChild(m_focusedPane)) {
        // Update focus
        if (newFocus && newFocus != m_focusedPane) {
            m_focusedPane = newFocus;
        } else {
            // Find any leaf pane
            leaves.clear();
            m_rootPane->GetAllLeafPanes(leaves);
            m_focusedPane = leaves.empty() ? nullptr : leaves[0];
        }

        spdlog::info("Closed pane");
        return tabToClose;
    }

    return nullptr;
}

void PaneManager::FocusNextPane() {
    if (!m_rootPane || !m_focusedPane) {
        return;
    }

    Pane* next = m_rootPane->FindAdjacentPane(m_focusedPane, SplitDirection::Horizontal, true);
    if (next) {
        m_focusedPane = next;
        spdlog::debug("Focused next pane");
    }
}

void PaneManager::FocusPreviousPane() {
    if (!m_rootPane || !m_focusedPane) {
        return;
    }

    Pane* prev = m_rootPane->FindAdjacentPane(m_focusedPane, SplitDirection::Horizontal, false);
    if (prev) {
        m_focusedPane = prev;
        spdlog::debug("Focused previous pane");
    }
}

void PaneManager::FocusPaneInDirection(SplitDirection direction) {
    if (!m_rootPane || !m_focusedPane) {
        return;
    }

    // For now, use simple next/prev navigation
    Pane* target = m_rootPane->FindAdjacentPane(m_focusedPane, direction, true);
    if (target) {
        m_focusedPane = target;
    }
}

Pane* PaneManager::FindPaneAt(int x, int y) {
    if (!m_rootPane) {
        return nullptr;
    }
    return m_rootPane->FindPaneAt(x, y);
}

void PaneManager::UpdateLayout(int width, int height, int tabBarHeight) {
    if (!m_rootPane) {
        return;
    }

    PaneRect availableSpace = {
        0,
        tabBarHeight,
        width,
        height - tabBarHeight
    };

    const int charWidth = 10;
    const int lineHeight = 25;

    // If zoomed, only the focused pane gets the full space
    if (m_paneZoomed && m_focusedPane && m_focusedPane->IsLeaf()) {
        m_focusedPane->SetBounds(availableSpace);
        if (m_focusedPane->GetTab() && m_focusedPane->GetTab()->GetScreenBuffer()) {
            int cols = std::max(10, availableSpace.width / charWidth);
            int rows = std::max(5, availableSpace.height / lineHeight);
            m_focusedPane->GetTab()->Resize(cols, rows);
        }
        return;
    }

    m_rootPane->CalculateLayout(availableSpace);

    // Resize all panes' tabs to fit their new bounds
    std::vector<Pane*> leaves;
    m_rootPane->GetAllLeafPanes(leaves);

    for (Pane* pane : leaves) {
        if (pane->GetTab() && pane->GetTab()->GetScreenBuffer()) {
            const PaneRect& bounds = pane->GetBounds();
            int cols = std::max(10, bounds.width / charWidth);
            int rows = std::max(5, bounds.height / lineHeight);
            pane->GetTab()->Resize(cols, rows);
        }
    }
}

void PaneManager::GetAllLeafPanes(std::vector<Pane*>& leaves) const {
    if (m_rootPane) {
        m_rootPane->GetAllLeafPanes(leaves);
    }
}

bool PaneManager::HasMultiplePanes() const {
    if (!m_rootPane) {
        return false;
    }
    return !m_rootPane->IsLeaf();
}

} // namespace TerminalDX12::UI
