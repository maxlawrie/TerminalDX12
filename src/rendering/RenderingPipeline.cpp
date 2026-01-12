#include "rendering/RenderingPipeline.h"
#include "rendering/IRenderer.h"
#include "core/Config.h"
#include "terminal/ScreenBuffer.h"
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include "ui/Pane.h"
#include "ui/PaneManager.h"
#include "ui/TerminalSession.h"
#include "ui/SearchManager.h"
#include "ui/SelectionManager.h"
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>

namespace {

// Convert wstring to ASCII (non-ASCII chars become '?')
std::string WStringToAscii(const std::wstring& ws, size_t maxLen = 0) {
    std::string result;
    for (wchar_t wc : ws) {
        result += (wc < 128) ? static_cast<char>(wc) : '?';
    }
    if (maxLen > 0 && result.length() > maxLen) {
        result = result.substr(0, maxLen - 3) + "...";
    }
    return result;
}

// Extract initials from a session title (e.g., "PowerShell" -> "PS", "cmd" -> "C")
std::string GetInitials(const std::wstring& title) {
    std::string result;
    bool nextIsInitial = true;
    for (wchar_t wc : title) {
        if (wc == L' ' || wc == L'-' || wc == L'_') {
            nextIsInitial = true;
        } else if (nextIsInitial && wc < 128 && std::isalpha(static_cast<char>(wc))) {
            result += static_cast<char>(std::toupper(static_cast<char>(wc)));
            nextIsInitial = false;
        }
    }
    if (result.empty() && !title.empty()) {
        for (size_t i = 0; i < std::min((size_t)2, title.length()); ++i) {
            if (title[i] < 128) result += static_cast<char>(std::toupper(static_cast<char>(title[i])));
        }
    }
    return result;
}

// Check if codepoint is valid for rendering
bool IsValidCodepoint(char32_t ch) {
    return ch <= 0x10FFFF && (ch >= 0x20 || ch == U'\t') &&
           (ch < 0xD800 || ch > 0xDFFF) && ch < 0xF0000 &&
           (ch & 0xFFFF) != 0xFFFE && (ch & 0xFFFF) != 0xFFFF;
}

// Convert a single UTF-32 codepoint to UTF-8 string
std::string Utf32ToUtf8(char32_t codepoint) {
    std::string result;
    if (codepoint < 0x80) {
        result += static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        result += static_cast<char>(0xC0 | (codepoint >> 6));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        result += static_cast<char>(0xE0 | (codepoint >> 12));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x110000) {
        result += static_cast<char>(0xF0 | (codepoint >> 18));
        result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        result += static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    return result;
}

} // anonymous namespace

namespace TerminalDX12::Rendering {

void RenderingPipeline::BuildColorPalette(float palette[256][3],
                                          Terminal::ScreenBuffer* screenBuffer,
                                          const Core::Config* config) {
    BuildColorPalette(palette, screenBuffer, config->GetColors());
}

void RenderingPipeline::BuildColorPalette(float palette[256][3],
                                          Terminal::ScreenBuffer* screenBuffer,
                                          const Core::ColorConfig& colorConfig) {
    auto setColor = [&](int i, uint8_t r, uint8_t g, uint8_t b) {
        palette[i][0] = r / 255.0f;
        palette[i][1] = g / 255.0f;
        palette[i][2] = b / 255.0f;
    };

    for (int i = 0; i < 256; ++i) {
        if (screenBuffer && screenBuffer->IsPaletteColorModified(i)) {
            const auto& c = screenBuffer->GetPaletteColor(i);
            setColor(i, c.r, c.g, c.b);
        } else if (i == 0) {
            // Palette 0 = default background color
            setColor(i, colorConfig.background.r, colorConfig.background.g, colorConfig.background.b);
        } else if (i == 7) {
            // Palette 7 = default foreground color
            setColor(i, colorConfig.foreground.r, colorConfig.foreground.g, colorConfig.foreground.b);
        } else if (i < 16) {
            const auto& c = colorConfig.palette[i];
            setColor(i, c.r, c.g, c.b);
        } else if (i < 232) {
            // 6x6x6 color cube
            int idx = i - 16, r = idx / 36, g = (idx / 6) % 6, b = idx % 6;
            setColor(i, r ? r * 40 + 55 : 0, g ? g * 40 + 55 : 0, b ? b * 40 + 55 : 0);
        } else {
            // Grayscale
            uint8_t gray = (i - 232) * 10 + 8;
            setColor(i, gray, gray, gray);
        }
    }
}

void RenderingPipeline::RenderTabBar(const RenderContext& ctx) {
    if (!ctx.tabManager || ctx.tabManager->GetTabCount() <= 1) return;

    auto* renderer = ctx.renderer;
    int charWidth = static_cast<int>(ctx.charWidth);

    // Render tab bar background
    renderer->RenderRect(0.0f, 0.0f, 2000.0f, static_cast<float>(kTabBarHeight),
                         0.15f, 0.15f, 0.15f, 1.0f);

    // Render each tab
    float tabX = 5.0f;
    int activeIndex = ctx.tabManager->GetActiveTabIndex();
    const auto& tabs = ctx.tabManager->GetTabs();

    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& tab = tabs[i];
        bool active = (static_cast<int>(i) == activeIndex);
        bool hasActivity = !active && tab->HasActivity();

        // Build session initials string
        std::string sessionNames;
        const auto& sessions = tab->GetSessions();
        for (size_t j = 0; j < sessions.size(); ++j) {
            if (j > 0) sessionNames += "|";
            sessionNames += GetInitials(sessions[j]->GetTitle());
        }
        if (sessionNames.empty()) sessionNames = "T";

        // Calculate tab width based on session names
        int sessionsLen = static_cast<int>(sessionNames.length() * charWidth) + 30;
        float tabWidth = static_cast<float>(std::max(100, sessionsLen));

        // Tab background
        float bg = active ? 0.3f : 0.2f;
        renderer->RenderRect(tabX, 3.0f, tabWidth, static_cast<float>(kTabBarHeight - 6),
                             bg, bg, active ? 0.35f : 0.2f, 1.0f);

        // Activity indicator and session names (blue)
        float textX = tabX + 8.0f;
        if (hasActivity) {
            renderer->RenderText("\xE2\x97\x8F", textX, 8.0f, 1.0f, 0.6f, 0.0f, 1.0f);
            textX += 12.0f;
        }

        // Session names in blue
        renderer->RenderText(sessionNames.c_str(), textX, 8.0f,
                             active ? 0.4f : 0.3f, active ? 0.7f : 0.5f, active ? 1.0f : 0.8f, 1.0f);

        // Tab separator
        renderer->RenderRect(tabX + tabWidth - 1.0f, 5.0f, 1.0f,
                             static_cast<float>(kTabBarHeight - 10), 0.4f, 0.4f, 0.4f, 1.0f);
        tabX += tabWidth + 5.0f;
    }
}

void RenderingPipeline::RenderCursor(const RenderContext& ctx,
                                     Terminal::ScreenBuffer* screenBuffer,
                                     int startX, int startY,
                                     int fontSize, int charWidth, int lineHeight) {
    int cursorX, cursorY, rows, cols;
    screenBuffer->GetCursorPos(cursorX, cursorY);
    screenBuffer->GetDimensions(cols, rows);

    bool visible = screenBuffer->IsCursorVisible() && screenBuffer->GetScrollOffset() == 0 &&
                   cursorX >= 0 && cursorX < cols && cursorY >= 0 && cursorY < rows;
    if (!visible) return;

    // Blink every 500ms
    static auto startTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime).count();
    if ((elapsed / 500) % 2 == 0) {
        ctx.renderer->RenderText("_", static_cast<float>(startX + cursorX * charWidth),
                                 static_cast<float>(startY + cursorY * lineHeight),
                                 ctx.cursorR, ctx.cursorG, ctx.cursorB, 1.0f, fontSize);
    }
}

void RenderingPipeline::RenderSearchBar(const RenderContext& ctx) {
    if (!ctx.searchManager || !ctx.searchManager->IsActive()) return;

    auto* renderer = ctx.renderer;
    int charWidth = static_cast<int>(ctx.charWidth);
    int lineHeight = static_cast<int>(ctx.lineHeight);

    constexpr int searchBarHeight = 30;
    float searchBarY = static_cast<float>(ctx.windowHeight - searchBarHeight);

    // Search bar background
    renderer->RenderRect(0.0f, searchBarY, 2000.0f, static_cast<float>(searchBarHeight),
                         0.2f, 0.2f, 0.25f, 1.0f);

    // Search label and query
    renderer->RenderText("Search:", 10.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);
    renderer->RenderText(WStringToAscii(ctx.searchManager->GetQuery()).c_str(),
                         80.0f, searchBarY + 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Search results count
    char countBuf[64];
    if (ctx.searchManager->GetMatches().empty()) {
        snprintf(countBuf, sizeof(countBuf), "No matches");
    } else {
        snprintf(countBuf, sizeof(countBuf), "%d of %d",
                 ctx.searchManager->GetCurrentMatchIndex() + 1,
                 static_cast<int>(ctx.searchManager->GetMatches().size()));
    }
    renderer->RenderText(countBuf, 400.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);

    // Instructions
    renderer->RenderText("F3/Enter: Next  Shift+F3: Prev  Esc: Close",
                         550.0f, searchBarY + 5.0f, 0.5f, 0.5f, 0.5f, 1.0f);

    // Highlight search matches
    const auto& matches = ctx.searchManager->GetMatches();
    if (!matches.empty()) {
        int tabCount = ctx.tabManager ? ctx.tabManager->GetTabCount() : 0;
        int termStartY = (tabCount > 1) ? kTabBarHeight + 5 : kPadding;
        int queryLen = static_cast<int>(ctx.searchManager->GetQuery().length());
        int currentIdx = ctx.searchManager->GetCurrentMatchIndex();
        for (size_t i = 0; i < matches.size(); ++i) {
            float intensity = (static_cast<int>(i) == currentIdx) ? 0.8f : 0.5f;
            float alpha = (static_cast<int>(i) == currentIdx) ? 0.6f : 0.4f;
            for (int j = 0; j < queryLen; ++j) {
                renderer->RenderRect(static_cast<float>(kStartX + (matches[i].x + j) * charWidth),
                                     static_cast<float>(termStartY + matches[i].y * lineHeight),
                                     static_cast<float>(charWidth), static_cast<float>(lineHeight),
                                     intensity, intensity * 0.75f, 0.0f, alpha);
            }
        }
    }
}

void RenderingPipeline::RenderTerminalContent(const RenderContext& ctx,
                                               Terminal::ScreenBuffer* screenBuffer,
                                               int startX, int startY,
                                               const float palette[256][3], void* pane,
                                               int fontSize, int charWidth, int lineHeight) {
    auto* renderer = ctx.renderer;

    int rows, cols;
    screenBuffer->GetDimensions(cols, rows);

    struct RGB { float r, g, b; };
    auto getColor = [&palette](const Terminal::CellAttributes& attr, bool fg) -> RGB {
        if (fg ? attr.UsesTrueColorFg() : attr.UsesTrueColorBg())
            return fg ? RGB{attr.fgR/255.0f, attr.fgG/255.0f, attr.fgB/255.0f}
                      : RGB{attr.bgR/255.0f, attr.bgG/255.0f, attr.bgB/255.0f};
        uint8_t idx = (fg ? attr.foreground : attr.background) % 16;
        return {palette[idx][0], palette[idx][1], palette[idx][2]};
    };

    // Calculate selection bounds (only if selection is in this pane)
    bool hasSel = ctx.selectionManager && ctx.selectionManager->HasSelection() &&
                  ctx.selectionManager->GetSelectionPane() == pane;
    bool rectSel = ctx.selectionManager && ctx.selectionManager->IsRectangleSelection();
    int selStartY = 0, selEndY = -1, selStartX = 0, selEndX = 0;
    if (hasSel) {
        const auto& s = ctx.selectionManager->GetSelectionStart();
        const auto& e = ctx.selectionManager->GetSelectionEnd();
        selStartY = std::min(s.y, e.y);
        selEndY = std::max(s.y, e.y);
        bool sFirst = s.y < e.y || (s.y == e.y && s.x <= e.x);
        selStartX = rectSel ? std::min(s.x, e.x) : (sFirst ? s.x : e.x);
        selEndX = rectSel ? std::max(s.x, e.x) : (sFirst ? e.x : s.x);
    }

    for (int y = 0; y < rows; ++y) {
        float posY = static_cast<float>(startY + y * lineHeight);

        // Selection highlight
        if (hasSel && y >= selStartY && y <= selEndY) {
            int xs = rectSel ? selStartX : (y == selStartY ? selStartX : 0);
            int xe = rectSel ? selEndX : (y == selEndY ? selEndX : cols - 1);
            for (int x = xs; x <= xe; ++x)
                renderer->RenderText("\xE2\x96\x88", static_cast<float>(startX + x * charWidth),
                                     posY, 0.2f, 0.4f, 0.8f, 0.5f, fontSize);
        }

        // Background and foreground in single pass
        for (int x = 0; x < cols; ++x) {
            auto cell = screenBuffer->GetCellWithScrollback(x, y);
            float posX = static_cast<float>(startX + x * charWidth);
            bool inv = cell.attr.IsInverse();

            // Background (inverse: use fg as bg)
            bool hasBg = inv ? (cell.attr.UsesTrueColorFg() || cell.attr.foreground % 16 != 0)
                             : (cell.attr.UsesTrueColorBg() || cell.attr.background % 16 != 0);
            if (hasBg) {
                auto bg = getColor(cell.attr, inv);
                renderer->RenderText("\xE2\x96\x88", posX, posY, bg.r, bg.g, bg.b, 1.0f, fontSize);
            }

            // Foreground text
            if (cell.ch == U' ' || cell.ch == U'\0' || !IsValidCodepoint(cell.ch)) continue;
            auto c = getColor(cell.attr, !inv);
            float r = c.r, g = c.g, b = c.b;
            if (cell.attr.IsBold() && !cell.attr.UsesTrueColorFg()) {
                r = std::min(1.0f, r + 0.2f);
                g = std::min(1.0f, g + 0.2f);
                b = std::min(1.0f, b + 0.2f);
            }
            if (cell.attr.IsDim()) {
                r *= 0.5f;
                g *= 0.5f;
                b *= 0.5f;
            }

            renderer->RenderChar(Utf32ToUtf8(cell.ch), posX, posY, r, g, b, 1.0f, fontSize);
            if (cell.attr.IsUnderline())
                renderer->RenderRect(posX, posY + lineHeight - 1, static_cast<float>(charWidth),
                                     3.0f, r, g, b, 1.0f);
        }
    }
}

void RenderingPipeline::RenderPaneDividers(const RenderContext& ctx, UI::Pane* pane) {
    if (!pane || pane->IsLeaf()) return;

    auto* renderer = ctx.renderer;
    const UI::PaneRect& bounds = pane->GetBounds();
    float splitRatio = pane->GetSplitRatio();

    if (pane->GetSplitDirection() == UI::SplitDirection::Horizontal) {
        // Vertical divider for left/right split
        int dividerX = bounds.x + static_cast<int>(bounds.width * splitRatio);
        renderer->RenderRect(static_cast<float>(dividerX - 1), static_cast<float>(bounds.y),
                             2.0f, static_cast<float>(bounds.height), 0.3f, 0.3f, 0.3f, 1.0f);
    } else if (pane->GetSplitDirection() == UI::SplitDirection::Vertical) {
        // Horizontal divider for top/bottom split
        int dividerY = bounds.y + static_cast<int>(bounds.height * splitRatio);
        renderer->RenderRect(static_cast<float>(bounds.x), static_cast<float>(dividerY - 1),
                             static_cast<float>(bounds.width), 2.0f, 0.3f, 0.3f, 0.3f, 1.0f);
    }

    // Recurse into children
    RenderPaneDividers(ctx, pane->GetFirstChild());
    RenderPaneDividers(ctx, pane->GetSecondChild());
}

void RenderingPipeline::RenderPaneFocusBorder(const RenderContext& ctx,
                                               const UI::PaneRect& bounds) {
    auto* renderer = ctx.renderer;
    float bx = static_cast<float>(bounds.x);
    float by = static_cast<float>(bounds.y);
    float bw = static_cast<float>(bounds.width);
    float bh = static_cast<float>(bounds.height);

    // Blue accent border
    renderer->RenderRect(bx, by, bw, 2.0f, 0.3f, 0.5f, 0.9f, 1.0f);           // Top
    renderer->RenderRect(bx, by + bh - 2.0f, bw, 2.0f, 0.3f, 0.5f, 0.9f, 1.0f); // Bottom
    renderer->RenderRect(bx, by, 2.0f, bh, 0.3f, 0.5f, 0.9f, 1.0f);           // Left
    renderer->RenderRect(bx + bw - 2.0f, by, 2.0f, bh, 0.3f, 0.5f, 0.9f, 1.0f); // Right
}

void RenderingPipeline::RenderFrame(const RenderContext& ctx) {
    if (!ctx.renderer) return;

    ctx.renderer->BeginFrame();
    ctx.renderer->ClearText();

    // Render tab bar if multiple tabs exist
    RenderTabBar(ctx);

    // Get all leaf panes to render
    std::vector<UI::Pane*> leafPanes;
    UI::Tab* activeTab = ctx.tabManager ? ctx.tabManager->GetActiveTab() : nullptr;
    UI::PaneManager* pm = activeTab ? &activeTab->GetPaneManager() : nullptr;
    if (pm) pm->GetAllLeafPanes(leafPanes);

    UI::Pane* focusedPane = pm ? pm->GetFocusedPane() : nullptr;

    // Render each pane's terminal content
    for (UI::Pane* pane : leafPanes) {
        if (!pane || !pane->GetSession()) continue;
        Terminal::ScreenBuffer* buffer = pane->GetSession()->GetScreenBuffer();
        if (!buffer) continue;

        const UI::PaneRect& bounds = pane->GetBounds();

        // Build color palette for this pane's buffer using profile colors
        float colorPalette[256][3];
        const Core::Profile* profile = pane->GetSession()->GetEffectiveProfile();

        // Get font size and metrics for this pane
        int fontSize = profile ? profile->font.size : ctx.config->GetFont().size;
        int charWidth = ctx.renderer->GetGlyphWidth(fontSize);
        int lineHeight = ctx.renderer->GetGlyphHeight(fontSize);

        if (profile) {
            BuildColorPalette(colorPalette, buffer, profile->colors);

            // Render pane background with profile's background color
            float bgR = profile->colors.background.r / 255.0f;
            float bgG = profile->colors.background.g / 255.0f;
            float bgB = profile->colors.background.b / 255.0f;
            ctx.renderer->RenderRect(static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                                     static_cast<float>(bounds.width), static_cast<float>(bounds.height),
                                     bgR, bgG, bgB, 1.0f);
        } else {
            BuildColorPalette(colorPalette, buffer, ctx.config);

            // Use global background color
            const auto& bg = ctx.config->GetColors().background;
            float bgR = bg.r / 255.0f;
            float bgG = bg.g / 255.0f;
            float bgB = bg.b / 255.0f;
            ctx.renderer->RenderRect(static_cast<float>(bounds.x), static_cast<float>(bounds.y),
                                     static_cast<float>(bounds.width), static_cast<float>(bounds.height),
                                     bgR, bgG, bgB, 1.0f);
        }

        // Render terminal content at pane position
        RenderTerminalContent(ctx, buffer, bounds.x, bounds.y, colorPalette, pane, fontSize, charWidth, lineHeight);

        // Render cursor only for focused pane
        if (pane == focusedPane) {
            RenderCursor(ctx, buffer, bounds.x, bounds.y, fontSize, charWidth, lineHeight);
        }

        // Draw focused pane border if multiple panes
        if (leafPanes.size() > 1 && pane == focusedPane) {
            RenderPaneFocusBorder(ctx, bounds);
        }
    }

    // Render pane dividers
    if (pm) RenderPaneDividers(ctx, pm->GetRootPane());

    // Render search bar and highlights
    RenderSearchBar(ctx);

    ctx.renderer->EndFrame();
    ctx.renderer->Present();
}

} // namespace TerminalDX12::Rendering
