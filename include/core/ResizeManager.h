#pragma once

#include <functional>
#include <vector>

namespace TerminalDX12 {

namespace Rendering { class IRenderer; }
namespace UI {
    class PaneManager;
    class Pane;
}

namespace Core {

class LayoutCalculator;

/**
 * @brief Manages window resize state machine
 *
 * Handles the complex resize sequencing:
 * 1. Queue resize request
 * 2. Apply renderer resize on next frame
 * 3. Resize ConPTY buffers one frame later
 * 4. Wait for stabilization frames
 */
class ResizeManager {
public:
    using ResizeBufferCallback = std::function<void(UI::Pane*, int cols, int rows)>;
    using UpdateLayoutCallback = std::function<void()>;

    ResizeManager() = default;
    ~ResizeManager() = default;

    /**
     * @brief Queue a resize request for processing
     * @param width New window width
     * @param height New window height
     */
    void QueueResize(int width, int height);

    /**
     * @brief Check if there's a pending resize
     */
    bool HasPendingResize() const { return m_pendingResize; }

    /**
     * @brief Check if we're in stabilization period
     */
    bool IsStabilizing() const { return m_stabilizeFrames > 0; }

    /**
     * @brief Process pending resize - call at start of each frame
     * @param renderer The renderer to resize
     * @param panes Leaf panes to resize buffers for
     * @param layoutCalc Layout calculator for size calculations
     * @param resizeBuffer Callback to resize a pane's buffer
     * @param updateLayout Callback to update pane layout
     * @return true if frame should be skipped (resize in progress)
     */
    bool ProcessFrame(Rendering::IRenderer* renderer,
                      const std::vector<UI::Pane*>& panes,
                      const LayoutCalculator& layoutCalc,
                      ResizeBufferCallback resizeBuffer,
                      UpdateLayoutCallback updateLayout);

    /**
     * @brief Get pending dimensions
     */
    int GetPendingWidth() const { return m_pendingWidth; }
    int GetPendingHeight() const { return m_pendingHeight; }

private:
    bool m_pendingResize = false;
    bool m_pendingConPTYResize = false;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;
    int m_stabilizeFrames = 0;
};

} // namespace Core
} // namespace TerminalDX12
