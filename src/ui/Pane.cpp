#include "ui/Pane.h"
#include "ui/Tab.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

Pane::Pane(Tab* tab)
    : m_tab(tab)
    , m_splitDirection(SplitDirection::None)
{
}

Pane::Pane(SplitDirection direction, std::unique_ptr<Pane> first, std::unique_ptr<Pane> second)
    : m_splitDirection(direction)
    , m_firstChild(std::move(first))
    , m_secondChild(std::move(second))
{
    if (m_firstChild) {
        m_firstChild->SetParent(this);
    }
    if (m_secondChild) {
        m_secondChild->SetParent(this);
    }
}

Pane::~Pane() = default;

void Pane::SetSplitRatio(float ratio) {
    m_splitRatio = std::clamp(ratio, 0.1f, 0.9f);
}

void Pane::CalculateLayout(const PaneRect& availableSpace) {
    m_bounds = availableSpace;

    if (IsLeaf()) {
        // Leaf pane - nothing more to calculate
        return;
    }

    // Split pane - divide space between children
    const int dividerSize = 4; // Pixels for the divider

    if (m_splitDirection == SplitDirection::Horizontal) {
        // Left/Right split
        int leftWidth = static_cast<int>((availableSpace.width - dividerSize) * m_splitRatio);
        int rightWidth = availableSpace.width - leftWidth - dividerSize;

        PaneRect leftRect = {
            availableSpace.x,
            availableSpace.y,
            leftWidth,
            availableSpace.height
        };

        PaneRect rightRect = {
            availableSpace.x + leftWidth + dividerSize,
            availableSpace.y,
            rightWidth,
            availableSpace.height
        };

        if (m_firstChild) {
            m_firstChild->CalculateLayout(leftRect);
        }
        if (m_secondChild) {
            m_secondChild->CalculateLayout(rightRect);
        }
    } else if (m_splitDirection == SplitDirection::Vertical) {
        // Top/Bottom split
        int topHeight = static_cast<int>((availableSpace.height - dividerSize) * m_splitRatio);
        int bottomHeight = availableSpace.height - topHeight - dividerSize;

        PaneRect topRect = {
            availableSpace.x,
            availableSpace.y,
            availableSpace.width,
            topHeight
        };

        PaneRect bottomRect = {
            availableSpace.x,
            availableSpace.y + topHeight + dividerSize,
            availableSpace.width,
            bottomHeight
        };

        if (m_firstChild) {
            m_firstChild->CalculateLayout(topRect);
        }
        if (m_secondChild) {
            m_secondChild->CalculateLayout(bottomRect);
        }
    }
}

Pane* Pane::FindPaneWithTab(Tab* tab) {
    if (IsLeaf()) {
        return (m_tab == tab) ? this : nullptr;
    }

    if (m_firstChild) {
        Pane* found = m_firstChild->FindPaneWithTab(tab);
        if (found) return found;
    }
    if (m_secondChild) {
        Pane* found = m_secondChild->FindPaneWithTab(tab);
        if (found) return found;
    }

    return nullptr;
}

Pane* Pane::FindPaneAt(int x, int y) {
    // Check if point is within this pane's bounds
    if (x < m_bounds.x || x >= m_bounds.x + m_bounds.width ||
        y < m_bounds.y || y >= m_bounds.y + m_bounds.height) {
        return nullptr;
    }

    if (IsLeaf()) {
        return this;
    }

    // Check children
    if (m_firstChild) {
        Pane* found = m_firstChild->FindPaneAt(x, y);
        if (found) return found;
    }
    if (m_secondChild) {
        Pane* found = m_secondChild->FindPaneAt(x, y);
        if (found) return found;
    }

    return nullptr;
}

void Pane::GetAllLeafPanes(std::vector<Pane*>& leaves) {
    if (IsLeaf()) {
        leaves.push_back(this);
        return;
    }

    if (m_firstChild) {
        m_firstChild->GetAllLeafPanes(leaves);
    }
    if (m_secondChild) {
        m_secondChild->GetAllLeafPanes(leaves);
    }
}

Pane* Pane::FindAdjacentPane(Pane* from, SplitDirection direction, bool forward) {
    // Find all leaf panes
    std::vector<Pane*> leaves;
    GetAllLeafPanes(leaves);

    if (leaves.size() <= 1) {
        return nullptr;
    }

    // Find current pane index
    auto it = std::find(leaves.begin(), leaves.end(), from);
    if (it == leaves.end()) {
        return nullptr;
    }

    size_t currentIndex = std::distance(leaves.begin(), it);

    // For now, simple circular navigation
    // TODO: Implement proper directional navigation based on layout
    if (forward) {
        return leaves[(currentIndex + 1) % leaves.size()];
    } else {
        return leaves[(currentIndex + leaves.size() - 1) % leaves.size()];
    }
}

Pane* Pane::Split(SplitDirection direction, Tab* newTab) {
    if (!IsLeaf()) {
        spdlog::warn("Cannot split a non-leaf pane");
        return nullptr;
    }

    // Create a new leaf pane for the new tab
    auto newPane = std::make_unique<Pane>(newTab);
    Pane* newPanePtr = newPane.get();

    // Create a new leaf pane for the existing tab
    auto existingPane = std::make_unique<Pane>(m_tab);

    // Convert this pane to a split pane
    m_tab = nullptr;
    m_splitDirection = direction;
    m_firstChild = std::move(existingPane);
    m_secondChild = std::move(newPane);
    m_splitRatio = 0.5f;

    // Update parent pointers
    m_firstChild->SetParent(this);
    m_secondChild->SetParent(this);

    spdlog::info("Split pane {} ({})",
                 direction == SplitDirection::Horizontal ? "horizontally" : "vertically",
                 m_bounds.width > 0 ? "has bounds" : "no bounds yet");

    return newPanePtr;
}

bool Pane::CloseChild(Pane* childToClose) {
    if (IsLeaf()) {
        return false;
    }

    Pane* paneToKeep = nullptr;

    if (m_firstChild.get() == childToClose) {
        paneToKeep = m_secondChild.get();
    } else if (m_secondChild.get() == childToClose) {
        paneToKeep = m_firstChild.get();
    } else {
        return false;
    }

    if (!paneToKeep) {
        return false;
    }

    // Promote the remaining child
    if (paneToKeep->IsLeaf()) {
        // Simple case: remaining child is a leaf
        m_tab = paneToKeep->m_tab;
        m_splitDirection = SplitDirection::None;
        m_firstChild.reset();
        m_secondChild.reset();
    } else {
        // Remaining child is a split - adopt its children
        m_splitDirection = paneToKeep->m_splitDirection;
        m_splitRatio = paneToKeep->m_splitRatio;

        // Move children from paneToKeep
        if (paneToKeep == m_firstChild.get()) {
            auto first = std::move(paneToKeep->m_firstChild);
            auto second = std::move(paneToKeep->m_secondChild);
            m_firstChild = std::move(first);
            m_secondChild = std::move(second);
        } else {
            auto first = std::move(paneToKeep->m_firstChild);
            auto second = std::move(paneToKeep->m_secondChild);
            m_firstChild = std::move(first);
            m_secondChild = std::move(second);
        }

        // Update parent pointers
        if (m_firstChild) m_firstChild->SetParent(this);
        if (m_secondChild) m_secondChild->SetParent(this);
    }

    return true;
}

} // namespace TerminalDX12::UI
