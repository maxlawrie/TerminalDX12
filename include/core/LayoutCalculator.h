#pragma once

#include <utility>

namespace TerminalDX12 {

namespace UI {
    class PaneManager;
    class Pane;
    struct PaneRect;
}
namespace Terminal { class ScreenBuffer; }

namespace Core {

/**
 * @brief Holds font metrics and performs layout calculations
 *
 * Centralizes all layout-related calculations including:
 * - Terminal grid size calculations
 * - Pixel-to-cell coordinate conversion
 * - Tab bar height calculations
 */
class LayoutCalculator {
public:
    // Layout constants
    static constexpr int kStartX = 10;
    static constexpr int kPadding = 10;
    static constexpr int kTabBarHeight = 30;

    LayoutCalculator() = default;
    ~LayoutCalculator() = default;

    /**
     * @brief Set font metrics from renderer
     */
    void SetFontMetrics(int charWidth, int lineHeight);

    /**
     * @brief Get current character width
     */
    int GetCharWidth() const { return m_charWidth; }

    /**
     * @brief Get current line height
     */
    int GetLineHeight() const { return m_lineHeight; }

    /**
     * @brief Calculate terminal start Y position
     * @param tabCount Number of tabs (tab bar shown if > 1)
     */
    int GetTerminalStartY(int tabCount) const;

    /**
     * @brief Calculate terminal grid size in columns and rows
     * @param windowWidth Window width in pixels
     * @param windowHeight Window height in pixels
     * @param tabCount Number of tabs
     * @return Pair of (columns, rows)
     */
    std::pair<int, int> CalculateTerminalSize(int windowWidth, int windowHeight, int tabCount) const;

    /**
     * @brief Convert screen pixel coordinates to cell position
     * @param pixelX X coordinate in pixels
     * @param pixelY Y coordinate in pixels
     * @param paneManager PaneManager to find pane at coordinates (can be null)
     * @param fallbackBuffer ScreenBuffer for clamping (can be null)
     * @param tabCount Number of tabs for fallback calculation
     * @return Cell position (x, y)
     */
    struct CellPos { int x, y; };
    CellPos ScreenToCell(int pixelX, int pixelY, UI::PaneManager* paneManager,
                         Terminal::ScreenBuffer* fallbackBuffer, int tabCount) const;

    /**
     * @brief Calculate cell position within a pane's bounds
     */
    CellPos ScreenToCellInPane(int pixelX, int pixelY, const UI::PaneRect& bounds,
                               Terminal::ScreenBuffer* buffer) const;

private:
    int m_charWidth = 10;
    int m_lineHeight = 25;
};

} // namespace Core
} // namespace TerminalDX12
