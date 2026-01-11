#include "core/Application.h"
#include "core/Config.h"
#include "core/Window.h"
#include "rendering/DX12Renderer.h"
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include "ui/TerminalSession.h"
#include "ui/Pane.h"
#include "ui/PaneManager.h"
#include "ui/SettingsDialog.h"
#include "ui/UrlHelper.h"
#include "pty/ConPtySession.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <algorithm>
#include <chrono>
#include <spdlog/spdlog.h>
#include <string>
#include <shellapi.h>

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

namespace TerminalDX12 {
namespace Core {

Application* Application::s_instance = nullptr;

Application::Application()
    : m_running(false)
    , m_minimized(false)
{
    s_instance = this;
}

Application::~Application() {
    Shutdown();
    s_instance = nullptr;
}

bool Application::Initialize(const std::wstring& shell) {
    // Load configuration
    m_config = std::make_unique<Config>();
    m_config->LoadDefault();
    
    // Create default config file if it doesn't exist
    m_config->CreateDefaultIfMissing();
    
    // Log any config warnings
    for (const auto& warning : m_config->GetWarnings()) {
        spdlog::warn("Config: {}", warning);
    }
    
    // Use shell from command line if provided, otherwise from config
    m_shellCommand = shell.empty() ? m_config->GetTerminal().shell : shell;

    // Create window
    m_window = std::make_unique<Window>();

    WindowDesc windowDesc;
    // Default size matching Windows Terminal (120 cols x 30 rows)
    // With charWidth=10, lineHeight=25, padding=20, tabBar=35
    windowDesc.width = 1220;   // 120*10 + 20 padding
    windowDesc.height = 795;   // 30*25 + 35 tabBar + 10 padding
    windowDesc.title = L"TerminalDX12 - GPU-Accelerated Terminal Emulator";
    windowDesc.resizable = true;

    if (!m_window->Create(windowDesc)) {
        return false;
    }

    // Setup window callbacks
    m_window->OnResize = [this](int width, int height) {
        OnWindowResize(width, height);
    };

    m_window->OnClose = [this]() {
        m_running = false;
    };

    // Keyboard input handlers
    m_window->OnCharEvent = [this](wchar_t ch) {
        if (m_inputHandler) m_inputHandler->OnChar(ch);
    };

    m_window->OnKeyEvent = [this](UINT key, bool isDown) {
        if (m_inputHandler) m_inputHandler->OnKey(key, isDown);
    };

    // Mouse handlers
    m_window->OnMouseWheel = [this](int x, int y, int delta, bool horizontal) {
        // Try MouseHandler first (for apps with mouse mode enabled)
        // If it returns false, fall back to local scrollback
        if (m_mouseHandler && m_mouseHandler->OnMouseWheel(x, y, delta, horizontal)) {
            return;
        }
        // Local scrollback (only for vertical scroll)
        if (!horizontal && m_inputHandler) {
            m_inputHandler->OnMouseWheel(delta);
        }
    };

    m_window->OnMouseButton = [this](int x, int y, int button, bool down) {
        if (m_mouseHandler) m_mouseHandler->OnMouseButton(x, y, button, down);
    };

    m_window->OnMouseMove = [this](int x, int y) {
        if (m_mouseHandler) m_mouseHandler->OnMouseMove(x, y);
    };

    // Paint handler for live resize
    m_window->OnPaint = [this]() {
        Render();
    };

    // Create DirectX 12 renderer
    m_renderer = std::make_unique<Rendering::DX12Renderer>();
    if (!m_renderer->Initialize(m_window.get())) {
        return false;
    }

    // Load font settings from config (renderer starts with defaults)
    const auto& fontConfig = m_config->GetFont();
    if (!fontConfig.resolvedPath.empty()) {
        m_renderer->ReloadFont(fontConfig.resolvedPath, fontConfig.size);
    }
    UpdateFontMetrics();

    // Create tab manager
    m_tabManager = std::make_unique<UI::TabManager>();

    // Set session created callback for OSC 52 clipboard access
    m_tabManager->SetSessionCreatedCallback([this](UI::TerminalSession* session) {
        if (session) {
            session->SetClipboardReadCallback([this]() {
                HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
                return m_selectionManager.GetClipboardText(hwnd);
            });
            session->SetClipboardWriteCallback([this](const std::string& text) {
                HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
                m_selectionManager.SetClipboardText(text, hwnd);
            });
        }
    });

    // Set process exit callback - close window when all processes have exited
    m_tabManager->SetProcessExitCallback([this](int tabId, int sessionId, int exitCode) {
        spdlog::info("Process in tab {} session {} exited with code {}", tabId, sessionId, exitCode);
        const auto& tabs = m_tabManager->GetTabs();
        bool anyRunning = std::any_of(tabs.begin(), tabs.end(),
            [](const auto& tab) { return tab && tab->HasRunningSessions(); });
        if (!anyRunning) {
            spdlog::info("All shell processes have exited, closing window");
            m_running = false;
        }
    });

    // Calculate terminal size based on window dimensions
    auto [termCols, termRows] = CalculateTerminalSize(windowDesc.width, windowDesc.height);
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;

    // Create initial tab
    UI::Tab* initialTab = m_tabManager->CreateTab(m_shellCommand, termCols, termRows, scrollbackLines);
    if (!initialTab) {
        spdlog::warn("Failed to start initial terminal session - continuing without ConPTY");
    } else {
        spdlog::info("Terminal session started successfully");
        UpdatePaneLayout();
    }

    // Apply window transparency from config
    float opacity = m_config->GetTerminal().opacity;
    if (opacity < 1.0f) {
        m_window->SetOpacity(opacity);
        spdlog::info("Window opacity set to {}%", static_cast<int>(opacity * 100));
    }

    // Create input handler with callbacks
    UI::InputHandlerCallbacks inputCallbacks;
    inputCallbacks.onShowSettings = [this]() { ShowSettings(); };
    inputCallbacks.onNewWindow = [this]() {
        wchar_t exePath[MAX_PATH];
        if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
            STARTUPINFOW si = {};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = {};
            std::wstring cmdLine = exePath;
            if (!m_shellCommand.empty() && m_shellCommand != L"powershell.exe") {
                cmdLine += L" \"" + m_shellCommand + L"\"";
            }
            if (CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr,
                              FALSE, 0, nullptr, nullptr, &si, &pi)) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                spdlog::info("Launched new window");
            } else {
                spdlog::error("Failed to launch new window: {}", GetLastError());
            }
        }
    };
    inputCallbacks.onQuit = [this]() { m_running = false; };
    inputCallbacks.onNewTab = [this]() {
        if (m_tabManager) {
            int cols = 80, rows = 24;
            if (auto* tab = m_tabManager->GetActiveTab()) {
                if (auto* session = tab->GetFocusedSession(); session && session->GetScreenBuffer()) {
                    cols = session->GetScreenBuffer()->GetCols();
                    rows = session->GetScreenBuffer()->GetRows();
                }
            }
            m_tabManager->CreateTab(m_shellCommand, cols, rows, m_config->GetTerminal().scrollbackLines);
        }
    };
    inputCallbacks.onCloseTab = [this]() {
        if (m_tabManager && m_tabManager->GetTabCount() > 1) m_tabManager->CloseActiveTab();
    };
    inputCallbacks.onNextTab = [this]() {
        if (m_tabManager) m_tabManager->NextTab();
    };
    inputCallbacks.onPreviousTab = [this]() {
        if (m_tabManager) m_tabManager->PreviousTab();
    };
    inputCallbacks.onSwitchToTab = [this](int tabIndex) {
        if (m_tabManager && tabIndex < static_cast<int>(m_tabManager->GetTabs().size())) {
            m_tabManager->SwitchToTab(m_tabManager->GetTabs()[tabIndex]->GetId());
        }
    };
    inputCallbacks.onSplitPane = [this](UI::SplitDirection dir) { SplitPane(dir); };
    inputCallbacks.onClosePane = [this]() { ClosePane(); };
    inputCallbacks.onToggleZoom = [this]() { if (auto* pm = GetActivePaneManager()) { pm->ToggleZoom(); UpdatePaneLayout(); } };
    inputCallbacks.onFocusNextPane = [this]() { if (auto* pm = GetActivePaneManager()) pm->FocusNextPane(); };
    inputCallbacks.onFocusPreviousPane = [this]() { if (auto* pm = GetActivePaneManager()) pm->FocusPreviousPane(); };
    inputCallbacks.onOpenSearch = [this]() { m_searchManager.Open(); };
    inputCallbacks.onCloseSearch = [this]() { m_searchManager.Close(); };
    inputCallbacks.onSearchNext = [this]() { m_searchManager.NextMatch(); };
    inputCallbacks.onSearchPrevious = [this]() { m_searchManager.PreviousMatch(); };
    inputCallbacks.onUpdateSearchResults = [this]() { m_searchManager.UpdateResults(GetActiveScreenBuffer()); };
    inputCallbacks.onCopySelection = [this]() { CopySelectionToClipboard(); };
    inputCallbacks.onPaste = [this]() { PasteFromClipboard(); };
    inputCallbacks.onUpdatePaneLayout = [this]() { UpdatePaneLayout(); };
    inputCallbacks.getActiveTerminal = [this]() { return GetActiveTerminal(); };
    inputCallbacks.getActiveScreenBuffer = [this]() { return GetActiveScreenBuffer(); };
    inputCallbacks.getTabCount = [this]() { return m_tabManager ? m_tabManager->GetTabCount() : 0; };
    inputCallbacks.hasMultiplePanes = [this]() { auto* pm = GetActivePaneManager(); return pm && pm->HasMultiplePanes(); };
    inputCallbacks.getShellCommand = [this]() -> const std::wstring& { return m_shellCommand; };
    inputCallbacks.getVTParser = [this]() { return GetActiveVTParser(); };

    m_inputHandler = std::make_unique<UI::InputHandler>(
        std::move(inputCallbacks), m_searchManager, m_selectionManager);

    // Set up context menu callbacks
    m_selectionManager.SetSettingsCallback([this]() { ShowSettings(); });
    m_selectionManager.SetSplitHorizontalCallback([this]() { SplitPane(UI::SplitDirection::Horizontal); });
    m_selectionManager.SetSplitVerticalCallback([this]() { SplitPane(UI::SplitDirection::Vertical); });
    m_selectionManager.SetClosePaneCallback([this]() { ClosePane(); });
    m_selectionManager.SetNewTabCallback([this]() {
        if (m_tabManager) {
            int cols = 80, rows = 24;
            if (auto* tab = m_tabManager->GetActiveTab()) {
                if (auto* session = tab->GetFocusedSession(); session && session->GetScreenBuffer()) {
                    cols = session->GetScreenBuffer()->GetCols();
                    rows = session->GetScreenBuffer()->GetRows();
                }
            }
            m_tabManager->CreateTab(m_shellCommand, cols, rows, m_config->GetTerminal().scrollbackLines);
        }
    });
    m_selectionManager.SetHasMultiplePanes([this]() { auto* pm = GetActivePaneManager(); return pm && pm->HasMultiplePanes(); });

    // Create mouse handler with callbacks
    UI::MouseHandlerCallbacks mouseCallbacks;
    mouseCallbacks.onPaste = [this]() { PasteFromClipboard(); };
    mouseCallbacks.onOpenUrl = [](const std::string& url) { UI::UrlHelper::OpenUrl(url); };
    mouseCallbacks.onShowContextMenu = [this](int x, int y) {
        HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
        m_selectionManager.ShowContextMenu(x, y, hwnd, GetActiveScreenBuffer(), GetActiveTerminal());
    };
    mouseCallbacks.onSwitchToTab = [this](int tabId) {
        if (m_tabManager) m_tabManager->SwitchToTab(tabId);
    };
    mouseCallbacks.getActiveTerminal = [this]() { return GetActiveTerminal(); };
    mouseCallbacks.getActiveScreenBuffer = [this]() { return GetActiveScreenBuffer(); };
    mouseCallbacks.getActiveVTParser = [this]() { return GetActiveVTParser(); };
    mouseCallbacks.getWindowHandle = [this]() { return m_window ? m_window->GetHandle() : nullptr; };
    mouseCallbacks.setCursor = [this](HCURSOR cursor) {
        if (m_window) m_window->SetCursor(cursor);
    };
    mouseCallbacks.onDividerResized = [this]() {
        // Update layout and resize all pane buffers after divider resize
        UpdatePaneLayout();
        ResizeAllPaneBuffers();
    };
    mouseCallbacks.getPaneManager = [this]() { return GetActivePaneManager(); };

    m_mouseHandler = std::make_unique<UI::MouseHandler>(
        std::move(mouseCallbacks), m_selectionManager, m_tabManager.get());

    // Set up screen-to-cell converter
    m_mouseHandler->SetScreenToCellConverter([this](int x, int y) -> UI::CellPos {
        CellPos pos = ScreenToCell(x, y);
        return {pos.x, pos.y};
    });

    // Set up URL detector
    m_mouseHandler->SetUrlDetector([this](int cellX, int cellY) {
        return UI::UrlHelper::DetectUrlAt(GetActiveScreenBuffer(), cellX, cellY);
    });

    m_window->Show();
    m_running = true;

    return true;
}

void Application::Shutdown() {
    // Tab manager will stop all terminal sessions
    m_tabManager.reset();

    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_window.reset();
}

UI::PaneManager* Application::GetActivePaneManager() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    return tab ? &tab->GetPaneManager() : nullptr;
}

// Helper accessors - use focused pane when available, fallback to active tab
Terminal::ScreenBuffer* Application::GetActiveScreenBuffer() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    UI::TerminalSession* session = tab ? tab->GetFocusedSession() : nullptr;
    return session ? session->GetScreenBuffer() : nullptr;
}

Terminal::VTStateMachine* Application::GetActiveVTParser() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    UI::TerminalSession* session = tab ? tab->GetFocusedSession() : nullptr;
    return session ? session->GetVTParser() : nullptr;
}

Pty::ConPtySession* Application::GetActiveTerminal() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    UI::TerminalSession* session = tab ? tab->GetFocusedSession() : nullptr;
    return session ? session->GetTerminal() : nullptr;
}

int Application::GetTerminalStartY() const {
    int tabCount = m_tabManager ? m_tabManager->GetTabCount() : 0;
    return (tabCount > 1) ? kTabBarHeight + 5 : kPadding;
}

std::pair<int, int> Application::CalculateTerminalSize(int width, int height) const {
    int startY = GetTerminalStartY();
    int availableWidth = width - kStartX - kPadding;
    int availableHeight = height - startY - kPadding;
    int cols = std::max(20, availableWidth / m_charWidth);
    int rows = std::max(5, availableHeight / m_lineHeight);
    return {cols, rows};
}

int Application::Run() {
    while (m_running) {
        if (!ProcessMessages()) break;
        if (!m_minimized) Render();
    }
    return 0;
}

bool Application::ProcessMessages() {
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

void Application::BuildColorPalette(float palette[256][3], Terminal::ScreenBuffer* screenBuffer) {
    const auto& colorConfig = m_config->GetColors();
    auto setColor = [&](int i, uint8_t r, uint8_t g, uint8_t b) {
        palette[i][0] = r / 255.0f; palette[i][1] = g / 255.0f; palette[i][2] = b / 255.0f;
    };

    for (int i = 0; i < 256; ++i) {
        if (screenBuffer && screenBuffer->IsPaletteColorModified(i)) {
            const auto& c = screenBuffer->GetPaletteColor(i);
            setColor(i, c.r, c.g, c.b);
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

void Application::RenderTabBar(int charWidth) {
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1) return;

    // Render tab bar background
    m_renderer->RenderRect(0.0f, 0.0f, 2000.0f, static_cast<float>(kTabBarHeight),
                           0.15f, 0.15f, 0.15f, 1.0f);

    // Render each tab
    float tabX = 5.0f;
    int activeIndex = m_tabManager->GetActiveTabIndex();
    const auto& tabs = m_tabManager->GetTabs();

    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& tab = tabs[i];
        bool active = (static_cast<int>(i) == activeIndex);
        bool hasActivity = !active && tab->HasActivity();

        // Build session names string
        std::string sessionNames;
        const auto& sessions = tab->GetSessions();
        for (size_t j = 0; j < sessions.size(); ++j) {
            if (j > 0) sessionNames += ", ";
            sessionNames += WStringToAscii(sessions[j]->GetTitle(), 12);
        }
        if (sessionNames.empty()) sessionNames = "Terminal";

        // Calculate tab width based on session names
        int sessionsLen = static_cast<int>(sessionNames.length() * charWidth) + 30;
        float tabWidth = static_cast<float>(std::max(100, sessionsLen));

        // Tab background
        float bg = active ? 0.3f : 0.2f;
        m_renderer->RenderRect(tabX, 3.0f, tabWidth, static_cast<float>(kTabBarHeight - 6),
                               bg, bg, active ? 0.35f : 0.2f, 1.0f);

        // Activity indicator and session names (blue)
        float textX = tabX + 8.0f;
        if (hasActivity) {
            m_renderer->RenderText("â", textX, 8.0f, 1.0f, 0.6f, 0.0f, 1.0f);
            textX += 12.0f;
        }

        // Session names in blue
        m_renderer->RenderText(sessionNames.c_str(), textX, 8.0f,
                               active ? 0.4f : 0.3f, active ? 0.7f : 0.5f, active ? 1.0f : 0.8f, 1.0f);

        // Tab separator
        m_renderer->RenderRect(tabX + tabWidth - 1.0f, 5.0f, 1.0f, static_cast<float>(kTabBarHeight - 10), 0.4f, 0.4f, 0.4f, 1.0f);
        tabX += tabWidth + 5.0f;
    }
}

void Application::RenderCursor(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                               int charWidth, int lineHeight, float cursorR, float cursorG, float cursorB) {
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
        m_renderer->RenderText("_", static_cast<float>(startX + cursorX * charWidth),
                               static_cast<float>(startY + cursorY * lineHeight), cursorR, cursorG, cursorB, 1.0f);
    }
}

void Application::RenderSearchBar(int startX, int charWidth, int lineHeight) {
    if (!m_searchManager.IsActive()) return;

    constexpr int searchBarHeight = 30;
    int windowHeight = m_window ? m_window->GetHeight() : 720;
    float searchBarY = static_cast<float>(windowHeight - searchBarHeight);

    // Search bar background
    m_renderer->RenderRect(0.0f, searchBarY, 2000.0f, static_cast<float>(searchBarHeight),
                           0.2f, 0.2f, 0.25f, 1.0f);

    // Search label and query
    m_renderer->RenderText("Search:", 10.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);
    m_renderer->RenderText(WStringToAscii(m_searchManager.GetQuery()).c_str(),
                           80.0f, searchBarY + 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);

    // Search results count
    char countBuf[64];
    if (m_searchManager.GetMatches().empty()) {
        snprintf(countBuf, sizeof(countBuf), "No matches");
    } else {
        snprintf(countBuf, sizeof(countBuf), "%d of %d",
                 m_searchManager.GetCurrentMatchIndex() + 1, static_cast<int>(m_searchManager.GetMatches().size()));
    }
    m_renderer->RenderText(countBuf, 400.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);

    // Instructions
    m_renderer->RenderText("F3/Enter: Next  Shift+F3: Prev  Esc: Close",
                           550.0f, searchBarY + 5.0f, 0.5f, 0.5f, 0.5f, 1.0f);

    // Highlight search matches
    const auto& matches = m_searchManager.GetMatches();
    if (!matches.empty()) {
        int termStartY = GetTerminalStartY();
        int queryLen = static_cast<int>(m_searchManager.GetQuery().length());
        int currentIdx = m_searchManager.GetCurrentMatchIndex();
        for (size_t i = 0; i < matches.size(); ++i) {
            float intensity = (static_cast<int>(i) == currentIdx) ? 0.8f : 0.5f;
            float alpha = (static_cast<int>(i) == currentIdx) ? 0.6f : 0.4f;
            for (int j = 0; j < queryLen; ++j) {
                m_renderer->RenderRect(static_cast<float>(startX + (matches[i].x + j) * charWidth),
                                       static_cast<float>(termStartY + matches[i].y * lineHeight),
                                       static_cast<float>(charWidth), static_cast<float>(lineHeight),
                                       intensity, intensity * 0.75f, 0.0f, alpha);
            }
        }
    }
}

void Application::RenderTerminalContent(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                                        int charWidth, int lineHeight, const float palette[256][3],
                                        void* pane) {
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
    bool hasSel = m_selectionManager.HasSelection() && m_selectionManager.GetSelectionPane() == pane;
    bool rectSel = m_selectionManager.IsRectangleSelection();
    int selStartY = 0, selEndY = -1, selStartX = 0, selEndX = 0;
    if (hasSel) {
        const auto& s = m_selectionManager.GetSelectionStart(), e = m_selectionManager.GetSelectionEnd();
        selStartY = std::min(s.y, e.y); selEndY = std::max(s.y, e.y);
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
                m_renderer->RenderText("\xE2\x96\x88", static_cast<float>(startX + x * charWidth), posY, 0.2f, 0.4f, 0.8f, 0.5f);
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
                m_renderer->RenderText("\xE2\x96\x88", posX, posY, bg.r, bg.g, bg.b, 1.0f);
            }

            // Foreground text
            if (cell.ch == U' ' || cell.ch == U'\0' || !IsValidCodepoint(cell.ch)) continue;
            auto c = getColor(cell.attr, !inv);
            float r = c.r, g = c.g, b = c.b;
            if (cell.attr.IsBold() && !cell.attr.UsesTrueColorFg()) { r = std::min(1.0f, r + 0.2f); g = std::min(1.0f, g + 0.2f); b = std::min(1.0f, b + 0.2f); }
            if (cell.attr.IsDim()) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }

            m_renderer->RenderChar(Utf32ToUtf8(cell.ch), posX, posY, r, g, b, 1.0f);
            if (cell.attr.IsUnderline())
                m_renderer->RenderRect(posX, posY + lineHeight - 1, static_cast<float>(charWidth), 3.0f, r, g, b, 1.0f);
        }
    }
}

void Application::Render() {
    if (!m_renderer) {
        return;
    }

    // Apply any pending resize BEFORE starting the frame
    if (m_pendingResize) {
        m_pendingResize = false;
        m_renderer->Resize(m_pendingWidth, m_pendingHeight);
        m_pendingConPTYResize = true;
        return;
    }

    // Resize ConPTY after DX12 has stabilized (one frame later)
    if (m_pendingConPTYResize) {
        m_pendingConPTYResize = false;
        ResizeAllPaneBuffers();
        m_resizeStabilizeFrames = 2;
        return;
    }

    if (m_resizeStabilizeFrames > 0) { m_resizeStabilizeFrames--; return; }

    m_renderer->BeginFrame();
    m_renderer->ClearText();

    // Render tab bar if multiple tabs exist
    RenderTabBar(m_charWidth);

    // Get all leaf panes to render
    std::vector<UI::Pane*> leafPanes;
    UI::PaneManager* pm = GetActivePaneManager();
    if (pm) pm->GetAllLeafPanes(leafPanes);

    // Cursor color from config
    const auto& colorConfig = m_config->GetColors();
    float cursorR = colorConfig.cursor.r / 255.0f;
    float cursorG = colorConfig.cursor.g / 255.0f;
    float cursorB = colorConfig.cursor.b / 255.0f;

    UI::Pane* focusedPane = pm ? pm->GetFocusedPane() : nullptr;

    // Render each pane's terminal content
    for (UI::Pane* pane : leafPanes) {
        if (!pane || !pane->GetSession()) continue;
        Terminal::ScreenBuffer* buffer = pane->GetSession()->GetScreenBuffer();
        if (!buffer) continue;

        const UI::PaneRect& bounds = pane->GetBounds();

        // Build color palette for this pane's buffer
        float colorPalette[256][3];
        BuildColorPalette(colorPalette, buffer);

        // Render terminal content at pane position
        RenderTerminalContent(buffer, bounds.x, bounds.y, m_charWidth, m_lineHeight, colorPalette, pane);

        // Render cursor only for focused pane
        if (pane == focusedPane) {
            RenderCursor(buffer, bounds.x, bounds.y, m_charWidth, m_lineHeight, cursorR, cursorG, cursorB);
        }

        // Draw focused pane border if multiple panes
        if (leafPanes.size() > 1 && pane == focusedPane) {
            float bx = static_cast<float>(bounds.x);
            float by = static_cast<float>(bounds.y);
            float bw = static_cast<float>(bounds.width);
            float bh = static_cast<float>(bounds.height);
            // Blue accent border
            m_renderer->RenderRect(bx, by, bw, 2.0f, 0.3f, 0.5f, 0.9f, 1.0f);           // Top
            m_renderer->RenderRect(bx, by + bh - 2.0f, bw, 2.0f, 0.3f, 0.5f, 0.9f, 1.0f); // Bottom
            m_renderer->RenderRect(bx, by, 2.0f, bh, 0.3f, 0.5f, 0.9f, 1.0f);           // Left
            m_renderer->RenderRect(bx + bw - 2.0f, by, 2.0f, bh, 0.3f, 0.5f, 0.9f, 1.0f); // Right
        }
    }

    // Render pane dividers
    if (pm) RenderPaneDividers(pm->GetRootPane());

    // Render search bar and highlights
    RenderSearchBar(kStartX, m_charWidth, m_lineHeight);

    m_renderer->EndFrame();
    m_renderer->Present();
}

void Application::OnWindowResize(int width, int height) {
    m_minimized = (width == 0 || height == 0);

    if (!m_minimized && m_renderer) {
        UI::PaneManager* pm = GetActivePaneManager();
        // Check if any pane's session is using alt buffer (TUI app running)
        std::vector<UI::Pane*> leaves;
        if (pm) pm->GetAllLeafPanes(leaves);
        bool anyAltBuffer = std::any_of(leaves.begin(), leaves.end(),
            [](UI::Pane* pane) {
                auto* buf = pane && pane->GetSession() ? pane->GetSession()->GetScreenBuffer() : nullptr;
                return buf && buf->IsUsingAlternativeBuffer();
            });

        // Defer DX12 renderer resize to next frame start
        m_pendingResize = true;
        m_pendingWidth = width;
        m_pendingHeight = height;

        // If TUI is running, skip buffer resize (will be handled in Render)
        if (anyAltBuffer) return;

        // Update pane layout and resize buffers immediately
        UpdatePaneLayout();
        for (UI::Pane* pane : leaves) {
            if (!pane || !pane->GetSession()) continue;
            const UI::PaneRect& bounds = pane->GetBounds();
            int cols = std::max(20, bounds.width / m_charWidth);
            int rows = std::max(5, bounds.height / m_lineHeight);
            pane->GetSession()->Resize(cols, rows);
        }
    }
}

Application::CellPos Application::ScreenToCell(int pixelX, int pixelY) const {
    // Find which pane the mouse is in
    UI::PaneManager* pm = const_cast<Application*>(this)->GetActivePaneManager();
    UI::Pane* pane = pm ? pm->FindPaneAt(pixelX, pixelY) : nullptr;
    if (pane && pane->IsLeaf()) {
        const UI::PaneRect& bounds = pane->GetBounds();
        CellPos pos{(pixelX - bounds.x) / m_charWidth, (pixelY - bounds.y) / m_lineHeight};
        if (pane->GetSession() && pane->GetSession()->GetScreenBuffer()) {
            auto* buf = pane->GetSession()->GetScreenBuffer();
            pos.x = std::clamp(pos.x, 0, buf->GetCols() - 1);
            pos.y = std::clamp(pos.y, 0, buf->GetRows() - 1);
        }
        return pos;
    }

    // Fallback to focused pane calculation
    CellPos pos{(pixelX - kStartX) / m_charWidth, (pixelY - GetTerminalStartY()) / m_lineHeight};
    if (auto* buf = const_cast<Application*>(this)->GetActiveScreenBuffer()) {
        pos.x = std::clamp(pos.x, 0, buf->GetCols() - 1);
        pos.y = std::clamp(pos.y, 0, buf->GetRows() - 1);
    }
    return pos;
}

void Application::CopySelectionToClipboard() {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
    m_selectionManager.CopySelectionToClipboard(screenBuffer, hwnd);
}

void Application::PasteFromClipboard() {
    Pty::ConPtySession* terminal = GetActiveTerminal();
    HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
    m_selectionManager.PasteFromClipboard(terminal, hwnd);
}

void Application::ShowSettings() {
    if (m_window && m_config) {
        UI::SettingsDialog(m_window->GetHandle(), m_config.get()).Show();
        // Reload font settings after dialog closes
        ReloadFontSettings();
    }
}

void Application::UpdateFontMetrics() {
    if (m_renderer) {
        m_charWidth = m_renderer->GetCharWidth();
        m_lineHeight = m_renderer->GetLineHeight();
        spdlog::info("Font metrics updated: charWidth={}, lineHeight={}", m_charWidth, m_lineHeight);
    }
}

void Application::ReloadFontSettings() {
    if (!m_config || !m_renderer) return;

    const auto& font = m_config->GetFont();
    std::string fontPath = font.resolvedPath;

    // If no resolved path, try to resolve it now
    if (fontPath.empty()) {
        fontPath = "C:/Windows/Fonts/consola.ttf";
    }

    spdlog::info("Reloading font: {} size {}", fontPath, font.size);

    if (m_renderer->ReloadFont(fontPath, font.size)) {
        UpdateFontMetrics();
        // Trigger resize to recalculate grid
        if (m_window) {
            RECT rect;
            GetClientRect(m_window->GetHandle(), &rect);
            OnWindowResize(rect.right - rect.left, rect.bottom - rect.top);
        }
    }
}

void Application::SplitPane(UI::SplitDirection direction) {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    if (!tab) return;

    UI::TerminalSession* focusedSession = tab->GetFocusedSession();
    Terminal::ScreenBuffer* buf = focusedSession ? focusedSession->GetScreenBuffer() : nullptr;
    if (!buf) return;

    // Use Tab's SplitPane which creates the session and splits the pane
    if (tab->SplitPane(direction, m_shellCommand, buf->GetCols(), buf->GetRows(),
                       m_config->GetTerminal().scrollbackLines)) {
        ResizeAllPaneBuffers();
    }
}

void Application::ClosePane() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    if (tab) {
        tab->ClosePane();
        ResizeAllPaneBuffers();
    }
}

void Application::UpdatePaneLayout() {
    if (!m_window) return;
    int tabBar = (m_tabManager && m_tabManager->GetTabCount() > 1) ? kTabBarHeight : 0;
    if (auto* pm = GetActivePaneManager()) {
        pm->UpdateLayout(m_window->GetWidth(), m_window->GetHeight(), tabBar);
    }
}

void Application::ResizeAllPaneBuffers() {
    UpdatePaneLayout();
    std::vector<UI::Pane*> leaves;
    if (auto* pm = GetActivePaneManager()) {
        pm->GetAllLeafPanes(leaves);
    }
    for (UI::Pane* pane : leaves) {
        if (!pane || !pane->GetSession()) continue;
        const UI::PaneRect& bounds = pane->GetBounds();
        int cols = std::max(20, bounds.width / m_charWidth);
        int rows = std::max(5, bounds.height / m_lineHeight);
        pane->GetSession()->Resize(cols, rows);
    }
}

void Application::RenderPaneDividers(UI::Pane* pane) {
    if (!pane || pane->IsLeaf()) return;

    const UI::PaneRect& bounds = pane->GetBounds();
    float splitRatio = pane->GetSplitRatio();

    if (pane->GetSplitDirection() == UI::SplitDirection::Horizontal) {
        // Vertical divider for left/right split
        int dividerX = bounds.x + static_cast<int>(bounds.width * splitRatio);
        m_renderer->RenderRect(static_cast<float>(dividerX - 1), static_cast<float>(bounds.y),
                               2.0f, static_cast<float>(bounds.height), 0.3f, 0.3f, 0.3f, 1.0f);
    } else if (pane->GetSplitDirection() == UI::SplitDirection::Vertical) {
        // Horizontal divider for top/bottom split
        int dividerY = bounds.y + static_cast<int>(bounds.height * splitRatio);
        m_renderer->RenderRect(static_cast<float>(bounds.x), static_cast<float>(dividerY - 1),
                               static_cast<float>(bounds.width), 2.0f, 0.3f, 0.3f, 0.3f, 1.0f);
    }

    // Recurse into children
    RenderPaneDividers(pane->GetFirstChild());
    RenderPaneDividers(pane->GetSecondChild());
}

} // namespace Core
} // namespace TerminalDX12
