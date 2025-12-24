#pragma once

#include <memory>
#include <functional>

namespace TerminalDX12::UI {

class Tab;

// Split orientation
enum class SplitDirection {
    None,       // Leaf pane (contains a tab)
    Horizontal, // Left/Right split
    Vertical    // Top/Bottom split
};

// Rectangle for pane bounds
struct PaneRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

// Pane tree node - either a leaf with a Tab or a split with two children
class Pane {
public:
    // Create a leaf pane with a tab
    explicit Pane(Tab* tab);

    // Create a split pane with two children
    Pane(SplitDirection direction, std::unique_ptr<Pane> first, std::unique_ptr<Pane> second);

    ~Pane();

    // Check if this is a leaf pane (has a tab)
    bool IsLeaf() const { return m_tab != nullptr; }

    // Get the tab (only valid for leaf panes)
    Tab* GetTab() { return m_tab; }
    const Tab* GetTab() const { return m_tab; }

    // Get split direction (None for leaf panes)
    SplitDirection GetSplitDirection() const { return m_splitDirection; }

    // Get children (only valid for split panes)
    Pane* GetFirstChild() { return m_firstChild.get(); }
    Pane* GetSecondChild() { return m_secondChild.get(); }
    const Pane* GetFirstChild() const { return m_firstChild.get(); }
    const Pane* GetSecondChild() const { return m_secondChild.get(); }

    // Split ratio (0.0 to 1.0, position of divider)
    float GetSplitRatio() const { return m_splitRatio; }
    void SetSplitRatio(float ratio);

    // Pane bounds (set during layout calculation)
    const PaneRect& GetBounds() const { return m_bounds; }
    void SetBounds(const PaneRect& bounds) { m_bounds = bounds; }

    // Calculate layout for this pane and all children
    void CalculateLayout(const PaneRect& availableSpace);

    // Find the leaf pane containing a specific tab
    Pane* FindPaneWithTab(Tab* tab);

    // Find leaf pane at screen coordinates
    Pane* FindPaneAt(int x, int y);

    // Get all leaf panes
    void GetAllLeafPanes(std::vector<Pane*>& leaves);

    // Navigation - find adjacent pane in a direction
    Pane* FindAdjacentPane(Pane* from, SplitDirection direction, bool forward);

    // Split this leaf pane (must be a leaf)
    // Returns the new pane created, or nullptr if this isn't a leaf
    Pane* Split(SplitDirection direction, Tab* newTab);

    // Close a child pane and promote the other
    // Returns true if successful
    bool CloseChild(Pane* childToClose);

    // Get parent pane (set during tree operations)
    Pane* GetParent() { return m_parent; }
    void SetParent(Pane* parent) { m_parent = parent; }

private:
    // For leaf panes
    Tab* m_tab = nullptr;

    // For split panes
    SplitDirection m_splitDirection = SplitDirection::None;
    std::unique_ptr<Pane> m_firstChild;
    std::unique_ptr<Pane> m_secondChild;
    float m_splitRatio = 0.5f;

    // Layout
    PaneRect m_bounds;

    // Parent pointer (not owned)
    Pane* m_parent = nullptr;
};

} // namespace TerminalDX12::UI
