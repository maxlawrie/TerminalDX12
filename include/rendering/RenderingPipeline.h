#pragma once

#include <array>
#include <string>
#include <vector>

namespace TerminalDX12 {

// Forward declarations
namespace Core {
    class Config;
    struct ColorConfig;
}
namespace Terminal { class ScreenBuffer; }
namespace Rendering { class IRenderer; }
namespace UI {
    class TabManager;
    class Tab;
    class Pane;
    class PaneManager;
    class SearchManager;
    class SelectionManager;
    struct PaneRect;
}

namespace Rendering {

/**
 * @brief Context passed to rendering methods containing all required state
 */
struct RenderContext {
    IRenderer* renderer = nullptr;
    UI::TabManager* tabManager = nullptr;
    UI::SearchManager* searchManager = nullptr;
    UI::SelectionManager* selectionManager = nullptr;
    const Core::Config* config = nullptr;

    float charWidth = 10.0f;
    float lineHeight = 25.0f;
    int windowWidth = 800;
    int windowHeight = 600;

    // Cursor color from config
    float cursorR = 1.0f;
    float cursorG = 1.0f;
    float cursorB = 1.0f;
};

/**
 * @brief Handles all terminal rendering operations
 *
 * Extracted from Application to separate rendering concerns from application logic.
 * Takes a RenderContext containing all required state and orchestrates rendering
 * of the terminal UI including tab bar, terminal content, cursor, search bar,
 * and pane dividers.
 */
class RenderingPipeline {
public:
    // Layout constants
    static constexpr int kStartX = 10;
    static constexpr int kPadding = 10;
    static constexpr int kTabBarHeight = 30;

    RenderingPipeline() = default;
    ~RenderingPipeline() = default;

    /**
     * @brief Render a complete frame
     * @param ctx Render context with all required state
     */
    void RenderFrame(const RenderContext& ctx);

private:
    /**
     * @brief Build the 256-color palette from config and OSC 4 modifications
     */
    void BuildColorPalette(float palette[256][3], Terminal::ScreenBuffer* screenBuffer,
                          const Core::Config* config);

    /**
     * @brief Build the 256-color palette from ColorConfig directly (for per-pane colors)
     */
    void BuildColorPalette(float palette[256][3], Terminal::ScreenBuffer* screenBuffer,
                          const Core::ColorConfig& colorConfig);

    /**
     * @brief Render the tab bar at the top of the window
     */
    void RenderTabBar(const RenderContext& ctx);

    /**
     * @brief Render terminal content for a single pane
     * @param fontSize Font size for this pane (for per-pane font support)
     * @param charWidth Character width for this pane's font size
     * @param lineHeight Line height for this pane's font size
     */
    void RenderTerminalContent(const RenderContext& ctx, Terminal::ScreenBuffer* screenBuffer,
                               int startX, int startY, const float palette[256][3], void* pane,
                               int fontSize, int charWidth, int lineHeight);

    /**
     * @brief Render the blinking cursor
     * @param fontSize Font size for this pane
     * @param charWidth Character width for this pane's font size
     * @param lineHeight Line height for this pane's font size
     */
    void RenderCursor(const RenderContext& ctx, Terminal::ScreenBuffer* screenBuffer,
                      int startX, int startY, int fontSize, int charWidth, int lineHeight);

    /**
     * @brief Render the search bar and search match highlights
     */
    void RenderSearchBar(const RenderContext& ctx);

    /**
     * @brief Recursively render pane dividers
     */
    void RenderPaneDividers(const RenderContext& ctx, UI::Pane* pane);

    /**
     * @brief Render a focus border around a pane
     */
    void RenderPaneFocusBorder(const RenderContext& ctx, const UI::PaneRect& bounds);
};

} // namespace Rendering
} // namespace TerminalDX12
