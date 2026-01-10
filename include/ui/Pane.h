#pragma once

#include <memory>
#include <functional>

namespace TerminalDX12::UI {

class TerminalSession;

// Split orientation
enum class SplitDirection {
    None,       // Leaf pane (contains a session)
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

// Pane tree node - either a leaf with a TerminalSession or a split with two children
class Pane {
public:
    // Create a leaf pane with a terminal session
    explicit Pane(TerminalSession* session);

    // Create a split pane with two children
    Pane(SplitDirection direction, std::unique_ptr<Pane> first, std::unique_ptr<Pane> second);

    ~Pane();

    // Check if this is a leaf pane (has a session)
    bool IsLeaf() const { return m_session != nullptr; }

    // Get the terminal session (only valid for leaf panes)
    TerminalSession* GetSession() { return m_session; }
    const TerminalSession* GetSession() const { return m_session; }

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

    // Find the leaf pane containing a specific session
    Pane* FindPaneWithSession(TerminalSession* session);

    // Find leaf pane at screen coordinates
    Pane* FindPaneAt(int x, int y);

    // Find if point is on a divider, returns the split pane containing the divider
    // Also sets outDirection to indicate horizontal or vertical divider
    Pane* FindDividerAt(int x, int y, SplitDirection& outDirection);

    // Get the divider rectangle for this split pane
    PaneRect GetDividerRect() const;

    // Get all leaf panes
    void GetAllLeafPanes(std::vector<Pane*>& leaves);

    // Navigation - find adjacent pane in a direction
    Pane* FindAdjacentPane(Pane* from, SplitDirection direction, bool forward);

    // Split this leaf pane (must be a leaf)
    // Returns the new pane created, or nullptr if this isn't a leaf
    Pane* Split(SplitDirection direction, TerminalSession* newSession);

    // Close a child pane and promote the other
    // Returns true if successful
    bool CloseChild(Pane* childToClose);

    // Get parent pane (set during tree operations)
    Pane* GetParent() { return m_parent; }
    void SetParent(Pane* parent) { m_parent = parent; }

private:
    // For leaf panes
    TerminalSession* m_session = nullptr;

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
