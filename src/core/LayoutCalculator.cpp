#include "core/LayoutCalculator.h"
#include "ui/PaneManager.h"
#include "ui/Pane.h"
#include "ui/TerminalSession.h"
#include "terminal/ScreenBuffer.h"
#include <algorithm>

namespace TerminalDX12::Core {

void LayoutCalculator::SetFontMetrics(int charWidth, int lineHeight) {
    m_charWidth = charWidth > 0 ? charWidth : 10;
    m_lineHeight = lineHeight > 0 ? lineHeight : 25;
}

int LayoutCalculator::GetTerminalStartY(int tabCount) const {
    return (tabCount > 1) ? kTabBarHeight + 5 : kPadding;
}

std::pair<int, int> LayoutCalculator::CalculateTerminalSize(int windowWidth, int windowHeight, int tabCount) const {
    int startY = GetTerminalStartY(tabCount);
    int availableWidth = windowWidth - kStartX - kPadding;
    int availableHeight = windowHeight - startY - kPadding;
    int cols = std::max(20, availableWidth / m_charWidth);
    int rows = std::max(5, availableHeight / m_lineHeight);
    return {cols, rows};
}

LayoutCalculator::CellPos LayoutCalculator::ScreenToCellInPane(int pixelX, int pixelY,
                                                                const UI::PaneRect& bounds,
                                                                Terminal::ScreenBuffer* buffer) const {
    CellPos pos{(pixelX - bounds.x) / m_charWidth, (pixelY - bounds.y) / m_lineHeight};
    if (buffer) {
        pos.x = std::clamp(pos.x, 0, buffer->GetCols() - 1);
        pos.y = std::clamp(pos.y, 0, buffer->GetRows() - 1);
    }
    return pos;
}

LayoutCalculator::CellPos LayoutCalculator::ScreenToCell(int pixelX, int pixelY,
                                                          UI::PaneManager* paneManager,
                                                          Terminal::ScreenBuffer* fallbackBuffer,
                                                          int tabCount) const {
    // Try to find the pane at these coordinates
    if (paneManager) {
        UI::Pane* pane = paneManager->FindPaneAt(pixelX, pixelY);
        if (pane && pane->IsLeaf()) {
            Terminal::ScreenBuffer* buf = nullptr;
            if (pane->GetSession()) {
                buf = pane->GetSession()->GetScreenBuffer();
            }
            return ScreenToCellInPane(pixelX, pixelY, pane->GetBounds(), buf);
        }
    }

    // Fallback to simple calculation
    int startY = GetTerminalStartY(tabCount);
    CellPos pos{(pixelX - kStartX) / m_charWidth, (pixelY - startY) / m_lineHeight};
    if (fallbackBuffer) {
        pos.x = std::clamp(pos.x, 0, fallbackBuffer->GetCols() - 1);
        pos.y = std::clamp(pos.y, 0, fallbackBuffer->GetRows() - 1);
    }
    return pos;
}

} // namespace TerminalDX12::Core
