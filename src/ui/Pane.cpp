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
    if (IsLeaf()) return;

    constexpr int dividerSize = 4;
    bool horizontal = (m_splitDirection == SplitDirection::Horizontal);
    int totalSize = horizontal ? availableSpace.width : availableSpace.height;
    int firstSize = static_cast<int>((totalSize - dividerSize) * m_splitRatio);
    int secondSize = totalSize - firstSize - dividerSize;

    PaneRect firstRect = availableSpace, secondRect = availableSpace;
    if (horizontal) {
        firstRect.width = firstSize;
        secondRect.x += firstSize + dividerSize;
        secondRect.width = secondSize;
    } else {
        firstRect.height = firstSize;
        secondRect.y += firstSize + dividerSize;
        secondRect.height = secondSize;
    }

    if (m_firstChild) m_firstChild->CalculateLayout(firstRect);
    if (m_secondChild) m_secondChild->CalculateLayout(secondRect);
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

PaneRect Pane::GetDividerRect() const {
    if (IsLeaf()) {
        return {0, 0, 0, 0};
    }

    constexpr int dividerSize = 4;
    bool horizontal = (m_splitDirection == SplitDirection::Horizontal);
    int totalSize = horizontal ? m_bounds.width : m_bounds.height;
    int firstSize = static_cast<int>((totalSize - dividerSize) * m_splitRatio);

    PaneRect divider;
    if (horizontal) {
        divider.x = m_bounds.x + firstSize;
        divider.y = m_bounds.y;
        divider.width = dividerSize;
        divider.height = m_bounds.height;
    } else {
        divider.x = m_bounds.x;
        divider.y = m_bounds.y + firstSize;
        divider.width = m_bounds.width;
        divider.height = dividerSize;
    }
    return divider;
}

Pane* Pane::FindDividerAt(int x, int y, SplitDirection& outDirection) {
    // Check if point is within this pane's bounds
    if (x < m_bounds.x || x >= m_bounds.x + m_bounds.width ||
        y < m_bounds.y || y >= m_bounds.y + m_bounds.height) {
        return nullptr;
    }

    if (IsLeaf()) {
        return nullptr;
    }

    // Check if point is on this pane's divider (with hit margin)
    constexpr int hitMargin = 4;
    PaneRect divider = GetDividerRect();

    bool onDivider = false;
    if (m_splitDirection == SplitDirection::Horizontal) {
        onDivider = x >= divider.x - hitMargin && x < divider.x + divider.width + hitMargin &&
                    y >= divider.y && y < divider.y + divider.height;
    } else {
        onDivider = y >= divider.y - hitMargin && y < divider.y + divider.height + hitMargin &&
                    x >= divider.x && x < divider.x + divider.width;
    }

    if (onDivider) {
        outDirection = m_splitDirection;
        return this;
    }

    // Check children's dividers
    if (m_firstChild) {
        Pane* found = m_firstChild->FindDividerAt(x, y, outDirection);
        if (found) return found;
    }
    if (m_secondChild) {
        Pane* found = m_secondChild->FindDividerAt(x, y, outDirection);
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
    // Note: Current implementation uses simple circular navigation. Could be enhanced with spatial awareness.
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
        m_tab = paneToKeep->m_tab;
        m_splitDirection = SplitDirection::None;
        m_firstChild.reset();
        m_secondChild.reset();
    } else {
        // Adopt the kept pane's children
        m_splitDirection = paneToKeep->m_splitDirection;
        m_splitRatio = paneToKeep->m_splitRatio;
        m_firstChild = std::move(paneToKeep->m_firstChild);
        m_secondChild = std::move(paneToKeep->m_secondChild);
        if (m_firstChild) m_firstChild->SetParent(this);
        if (m_secondChild) m_secondChild->SetParent(this);
    }

    return true;
}

} // namespace TerminalDX12::UI
