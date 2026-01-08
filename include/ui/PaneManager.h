#pragma once

#include <memory>
#include <vector>
#include "ui/Pane.h"

namespace TerminalDX12::Core {
    class Window;
    class Config;
}

namespace TerminalDX12::Rendering {
    class DX12Renderer;
}

namespace TerminalDX12::UI {

class TabManager;
class Tab;

/// @brief Manages terminal panes (splits) within the application
class PaneManager {
public:
    PaneManager() = default;

    /// @brief Initialize with a root pane from an initial tab
    /// @param initialTab The tab for the root pane
    void Initialize(Tab* initialTab);

    /// @brief Get the root pane
    Pane* GetRootPane() const { return m_rootPane.get(); }

    /// @brief Get the currently focused pane
    Pane* GetFocusedPane() const { return m_focusedPane; }

    /// @brief Set the focused pane
    void SetFocusedPane(Pane* pane) { m_focusedPane = pane; }

    /// @brief Check if pane zoom is enabled
    bool IsZoomed() const { return m_paneZoomed; }

    /// @brief Toggle zoom on the focused pane
    void ToggleZoom();

    /// @brief Split the focused pane in the given direction
    /// @param direction The split direction
    /// @param newTab The tab for the new pane
    /// @return The newly created pane, or nullptr on failure
    Pane* SplitFocusedPane(SplitDirection direction, Tab* newTab);

    /// @brief Close the focused pane
    /// @param tabManager TabManager to close the associated tab
    /// @return The tab that was closed (to allow TabManager to clean it up), or nullptr
    Tab* CloseFocusedPane();

    /// @brief Focus the next pane (circular)
    void FocusNextPane();

    /// @brief Focus the previous pane (circular)
    void FocusPreviousPane();

    /// @brief Focus a pane in the given direction
    void FocusPaneInDirection(SplitDirection direction);

    /// @brief Find which pane is at the given screen coordinates
    /// @return The pane at the position, or nullptr
    Pane* FindPaneAt(int x, int y);

    /// @brief Update pane layout based on available space
    /// @param width Window width
    /// @param height Window height
    /// @param tabBarHeight Height of tab bar (0 if not visible)
    void UpdateLayout(int width, int height, int tabBarHeight);

    /// @brief Get all leaf panes
    void GetAllLeafPanes(std::vector<Pane*>& leaves) const;

    /// @brief Check if there are multiple panes
    bool HasMultiplePanes() const;

    /// @brief Find divider at screen coordinates
    /// @return The pane containing the divider, or nullptr
    Pane* FindDividerAt(int x, int y, SplitDirection& outDirection);

    /// @brief Start resizing a divider
    /// @param pane The split pane to resize
    /// @param startPos The starting mouse position
    void StartDividerResize(Pane* pane, int startPos);

    /// @brief Update divider position during resize
    /// @param currentPos Current mouse position
    void UpdateDividerResize(int currentPos);

    /// @brief End divider resize
    void EndDividerResize();

    /// @brief Check if currently resizing a divider
    bool IsResizingDivider() const { return m_resizingPane != nullptr; }

    /// @brief Get the direction of the divider being resized
    SplitDirection GetResizeDirection() const;

private:
    std::unique_ptr<Pane> m_rootPane;
    Pane* m_focusedPane = nullptr;
    bool m_paneZoomed = false;

    // Divider resize state
    Pane* m_resizingPane = nullptr;
    int m_resizeStartPos = 0;
    float m_resizeStartRatio = 0.5f;
};

} // namespace TerminalDX12::UI
