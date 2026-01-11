#include "core/ResizeManager.h"
#include "core/LayoutCalculator.h"
#include "rendering/IRenderer.h"
#include "ui/Pane.h"
#include "ui/TerminalSession.h"
#include "terminal/ScreenBuffer.h"
#include <algorithm>

namespace TerminalDX12::Core {

void ResizeManager::QueueResize(int width, int height) {
    m_pendingResize = true;
    m_pendingWidth = width;
    m_pendingHeight = height;
}

bool ResizeManager::ProcessFrame(Rendering::IRenderer* renderer,
                                  const std::vector<UI::Pane*>& panes,
                                  const LayoutCalculator& layoutCalc,
                                  ResizeBufferCallback resizeBuffer,
                                  UpdateLayoutCallback updateLayout) {
    // Apply renderer resize
    if (m_pendingResize) {
        m_pendingResize = false;
        if (renderer) {
            renderer->Resize(m_pendingWidth, m_pendingHeight);
        }
        m_pendingConPTYResize = true;
        return true; // Skip this frame
    }

    // Resize ConPTY buffers one frame later
    if (m_pendingConPTYResize) {
        m_pendingConPTYResize = false;
        if (updateLayout) {
            updateLayout();
        }
        for (UI::Pane* pane : panes) {
            if (!pane || !pane->GetSession()) continue;
            const UI::PaneRect& bounds = pane->GetBounds();
            int cols = std::max(20, bounds.width / layoutCalc.GetCharWidth());
            int rows = std::max(5, bounds.height / layoutCalc.GetLineHeight());
            if (resizeBuffer) {
                resizeBuffer(pane, cols, rows);
            }
        }
        m_stabilizeFrames = 2;
        return true; // Skip this frame
    }

    // Wait for stabilization
    if (m_stabilizeFrames > 0) {
        m_stabilizeFrames--;
        return true; // Skip this frame
    }

    return false; // OK to render
}

} // namespace TerminalDX12::Core
