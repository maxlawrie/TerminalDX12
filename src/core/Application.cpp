#include "core/Application.h"
#include "core/Config.h"
#include "core/Window.h"
#include "rendering/DX12Renderer.h"
#include "rendering/RenderingPipeline.h"
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

// =============================================================================
// Callback setup helpers
// =============================================================================

void Application::SetupWindowCallbacks() {
    m_window->OnResize = [this](int width, int height) {
        OnWindowResize(width, height);
    };

    m_window->OnClose = [this]() {
        m_running = false;
    };

    m_window->OnCharEvent = [this](wchar_t ch) {
        if (m_inputHandler) m_inputHandler->OnChar(ch);
    };

    m_window->OnKeyEvent = [this](UINT key, bool isDown) {
        if (m_inputHandler) m_inputHandler->OnKey(key, isDown);
    };

    m_window->OnMouseWheel = [this](int x, int y, int delta, bool horizontal) {
        if (m_mouseHandler && m_mouseHandler->OnMouseWheel(x, y, delta, horizontal)) {
            return;
        }
        if (!horizontal && m_inputHandler) {
            m_inputHandler->OnMouseWheel(delta);
        }
    };

    m_window->OnMouseButton = [this](int x, int y, int button, bool down) {
        if (m_mouseHandler) m_mouseHandler->OnMouseButton(x, y, button, down);
    };

    m_window->OnMouseMove = [this](int x, int y) {
        if (m_mouseHandler) m_mouseHandler->OnMouseMove(x, y);
        UpdateTabTooltip(x, y);
    };

    m_window->OnPaint = [this]() {
        Render();
    };
}

void Application::SetupTabManagerCallbacks() {
    // OSC 52 clipboard access
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

    // Process exit callback
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
}

UI::InputHandlerCallbacks Application::BuildInputCallbacks() {
    UI::InputHandlerCallbacks cb;

    cb.onShowSettings = [this]() { ShowSettings(); };

    cb.onNewWindow = [this]() {
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

    cb.onQuit = [this]() { m_running = false; };

    cb.onNewTab = [this]() { CreateNewTab(); };

    cb.onCloseTab = [this]() {
        if (m_tabManager && m_tabManager->GetTabCount() > 1) {
            m_tabManager->CloseActiveTab();
            UpdatePaneLayout();
            ResizeAllPaneBuffers();
        }
    };

    cb.onNextTab = [this]() { if (m_tabManager) m_tabManager->NextTab(); };
    cb.onPreviousTab = [this]() { if (m_tabManager) m_tabManager->PreviousTab(); };

    cb.onSwitchToTab = [this](int tabIndex) {
        if (m_tabManager && tabIndex < static_cast<int>(m_tabManager->GetTabs().size())) {
            m_tabManager->SwitchToTab(m_tabManager->GetTabs()[tabIndex]->GetId());
        }
    };

    cb.onSplitPane = [this](UI::SplitDirection dir) { SplitPane(dir); };
    cb.onClosePane = [this]() { ClosePane(); };
    cb.onToggleZoom = [this]() { if (auto* pm = GetActivePaneManager()) { pm->ToggleZoom(); UpdatePaneLayout(); } };
    cb.onFocusNextPane = [this]() { if (auto* pm = GetActivePaneManager()) pm->FocusNextPane(); };
    cb.onFocusPreviousPane = [this]() { if (auto* pm = GetActivePaneManager()) pm->FocusPreviousPane(); };

    cb.onOpenSearch = [this]() { m_searchManager.Open(); };
    cb.onCloseSearch = [this]() { m_searchManager.Close(); };
    cb.onSearchNext = [this]() { m_searchManager.NextMatch(); };
    cb.onSearchPrevious = [this]() { m_searchManager.PreviousMatch(); };
    cb.onUpdateSearchResults = [this]() { m_searchManager.UpdateResults(GetActiveScreenBuffer()); };

    cb.onCopySelection = [this]() { CopySelectionToClipboard(); };
    cb.onPaste = [this]() { PasteFromClipboard(); };
    cb.onUpdatePaneLayout = [this]() { UpdatePaneLayout(); };

    cb.getActiveTerminal = [this]() { return GetActiveTerminal(); };
    cb.getActiveScreenBuffer = [this]() { return GetActiveScreenBuffer(); };
    cb.getTabCount = [this]() { return m_tabManager ? m_tabManager->GetTabCount() : 0; };
    cb.hasMultiplePanes = [this]() { auto* pm = GetActivePaneManager(); return pm && pm->HasMultiplePanes(); };
    cb.getShellCommand = [this]() -> const std::wstring& { return m_shellCommand; };
    cb.getVTParser = [this]() { return GetActiveVTParser(); };

    return cb;
}

void Application::SetupSelectionCallbacks() {
    m_selectionManager.SetSettingsCallback([this]() { ShowSettings(); });
    m_selectionManager.SetSplitHorizontalCallback([this]() { SplitPane(UI::SplitDirection::Horizontal); });
    m_selectionManager.SetSplitVerticalCallback([this]() { SplitPane(UI::SplitDirection::Vertical); });
    m_selectionManager.SetClosePaneCallback([this]() { ClosePane(); });
    m_selectionManager.SetNewTabCallback([this]() { CreateNewTab(); });
    m_selectionManager.SetHasMultiplePanes([this]() { auto* pm = GetActivePaneManager(); return pm && pm->HasMultiplePanes(); });
}

UI::MouseHandlerCallbacks Application::BuildMouseCallbacks() {
    UI::MouseHandlerCallbacks cb;

    cb.onPaste = [this]() { PasteFromClipboard(); };
    cb.onOpenUrl = [](const std::string& url) { UI::UrlHelper::OpenUrl(url); };

    cb.onShowContextMenu = [this](int x, int y) {
        HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
        m_selectionManager.ShowContextMenu(x, y, hwnd, GetActiveScreenBuffer(), GetActiveTerminal());
    };

    cb.onSwitchToTab = [this](int tabId) {
        if (m_tabManager) m_tabManager->SwitchToTab(tabId);
    };

    cb.getActiveTerminal = [this]() { return GetActiveTerminal(); };
    cb.getActiveScreenBuffer = [this]() { return GetActiveScreenBuffer(); };
    cb.getActiveVTParser = [this]() { return GetActiveVTParser(); };
    cb.getWindowHandle = [this]() { return m_window ? m_window->GetHandle() : nullptr; };

    cb.setCursor = [this](HCURSOR cursor) {
        if (m_window) m_window->SetCursor(cursor);
    };

    cb.onDividerResized = [this]() {
        UpdatePaneLayout();
        ResizeAllPaneBuffers();
    };

    cb.getPaneManager = [this]() { return GetActivePaneManager(); };

    return cb;
}

// =============================================================================
// Initialize
// =============================================================================

bool Application::Initialize(const std::wstring& shell) {
    // Load configuration
    m_config = std::make_unique<Config>();
    m_config->LoadDefault();
    m_config->CreateDefaultIfMissing();

    for (const auto& warning : m_config->GetWarnings()) {
        spdlog::warn("Config: {}", warning);
    }

    m_shellCommand = shell.empty() ? m_config->GetTerminal().shell : shell;

    // Create window
    m_window = std::make_unique<Window>();

    WindowDesc windowDesc;
    windowDesc.width = 1220;
    windowDesc.height = 795;
    windowDesc.title = L"TerminalDX12 - GPU-Accelerated Terminal Emulator";
    windowDesc.resizable = true;

    if (!m_window->Create(windowDesc)) {
        return false;
    }

    SetupWindowCallbacks();

    // Create DirectX 12 renderer
    m_renderer = std::make_unique<Rendering::DX12Renderer>();
    if (!m_renderer->Initialize(m_window.get())) {
        return false;
    }

    // Create rendering pipeline
    m_renderingPipeline = std::make_unique<Rendering::RenderingPipeline>();

    // Load font settings from config
    const auto& fontConfig = m_config->GetFont();
    if (!fontConfig.resolvedPath.empty()) {
        m_renderer->ReloadFont(fontConfig.resolvedPath, fontConfig.size);
    }
    UpdateFontMetrics();

    // Create tab manager and setup callbacks
    m_tabManager = std::make_unique<UI::TabManager>();
    SetupTabManagerCallbacks();

    // Calculate terminal size and create initial tab
    int tabCount = m_tabManager ? m_tabManager->GetTabCount() : 0;
    auto [termCols, termRows] = m_layoutCalc.CalculateTerminalSize(windowDesc.width, windowDesc.height, tabCount);
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;

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

    // Create input handler
    m_inputHandler = std::make_unique<UI::InputHandler>(
        BuildInputCallbacks(), m_searchManager, m_selectionManager);

    // Setup context menu callbacks
    SetupSelectionCallbacks();

    // Create mouse handler
    m_mouseHandler = std::make_unique<UI::MouseHandler>(
        BuildMouseCallbacks(), m_selectionManager, m_tabManager.get());

    m_mouseHandler->SetScreenToCellConverter([this](int x, int y) -> UI::CellPos {
        int tabCount = m_tabManager ? m_tabManager->GetTabCount() : 0;
        auto pos = m_layoutCalc.ScreenToCell(x, y, GetActivePaneManager(), GetActiveScreenBuffer(), tabCount);
        return {pos.x, pos.y};
    });

    m_mouseHandler->SetUrlDetector([this](int cellX, int cellY) {
        return UI::UrlHelper::DetectUrlAt(GetActiveScreenBuffer(), cellX, cellY);
    });

    m_mouseHandler->SetCharWidth(m_layoutCalc.GetCharWidth());

    m_window->Show();
    m_running = true;

    return true;
}

void Application::Shutdown() {
    m_tabManager.reset();

    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_renderingPipeline.reset();
    m_window.reset();
}

UI::PaneManager* Application::GetActivePaneManager() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    return tab ? &tab->GetPaneManager() : nullptr;
}

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

void Application::Render() {
    if (!m_renderer || !m_renderingPipeline) {
        return;
    }

    // Get leaf panes for resize processing
    std::vector<UI::Pane*> leafPanes;
    if (auto* pm = GetActivePaneManager()) {
        pm->GetAllLeafPanes(leafPanes);
    }

    // Process any pending resize using ResizeManager
    bool skipFrame = m_resizeManager.ProcessFrame(
        m_renderer.get(),
        leafPanes,
        m_layoutCalc,
        [](UI::Pane* pane, int cols, int rows) {
            if (pane && pane->GetSession()) {
                pane->GetSession()->Resize(cols, rows);
            }
        },
        [this]() { UpdatePaneLayout(); }
    );

    if (skipFrame) {
        return;
    }

    // Build render context
    Rendering::RenderContext ctx;
    ctx.renderer = m_renderer.get();
    ctx.tabManager = m_tabManager.get();
    ctx.searchManager = &m_searchManager;
    ctx.selectionManager = &m_selectionManager;
    ctx.config = m_config.get();
    ctx.charWidth = static_cast<float>(m_layoutCalc.GetCharWidth());
    ctx.lineHeight = static_cast<float>(m_layoutCalc.GetLineHeight());
    ctx.windowWidth = m_window ? m_window->GetWidth() : 800;
    ctx.windowHeight = m_window ? m_window->GetHeight() : 600;

    // Cursor color from config
    const auto& colorConfig = m_config->GetColors();
    ctx.cursorR = colorConfig.cursor.r / 255.0f;
    ctx.cursorG = colorConfig.cursor.g / 255.0f;
    ctx.cursorB = colorConfig.cursor.b / 255.0f;

    // Delegate to RenderingPipeline
    m_renderingPipeline->RenderFrame(ctx);
}

void Application::UpdateTabTooltip(int x, int y) {
    int tabBarHeight = LayoutCalculator::kTabBarHeight;
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1 || y >= tabBarHeight) {
        if (m_hoveredTabId != -1) {
            m_window->HideTooltip();
            m_hoveredTabId = -1;
        }
        return;
    }

    float tabX = 5.0f;
    int foundTabId = -1;
    std::wstring tooltipText;
    int charWidth = m_layoutCalc.GetCharWidth();

    for (const auto& tab : m_tabManager->GetTabs()) {
        std::string sessionNames;
        const auto& sessions = tab->GetSessions();
        for (size_t j = 0; j < sessions.size(); ++j) {
            if (j > 0) sessionNames += "|";
            sessionNames += GetInitials(sessions[j]->GetTitle());
        }
        if (sessionNames.empty()) sessionNames = "T";

        int sessionsLen = static_cast<int>(sessionNames.length() * charWidth) + 30;
        float tabWidth = static_cast<float>(std::max(100, sessionsLen));

        if (x >= tabX && x < tabX + tabWidth) {
            foundTabId = tab->GetId();
            for (size_t j = 0; j < sessions.size(); ++j) {
                if (j > 0) tooltipText += L", ";
                tooltipText += sessions[j]->GetTitle();
            }
            if (tooltipText.empty()) tooltipText = L"Terminal";
            break;
        }
        tabX += tabWidth + 5.0f;
    }

    if (foundTabId != m_hoveredTabId) {
        m_hoveredTabId = foundTabId;
        if (foundTabId != -1) {
            m_window->ShowTooltip(x, y, tooltipText);
        } else {
            m_window->HideTooltip();
        }
    }
}

void Application::OnWindowResize(int width, int height) {
    m_minimized = (width == 0 || height == 0);

    if (!m_minimized && m_renderer) {
        UI::PaneManager* pm = GetActivePaneManager();
        std::vector<UI::Pane*> leaves;
        if (pm) pm->GetAllLeafPanes(leaves);
        bool anyAltBuffer = std::any_of(leaves.begin(), leaves.end(),
            [](UI::Pane* pane) {
                auto* buf = pane && pane->GetSession() ? pane->GetSession()->GetScreenBuffer() : nullptr;
                return buf && buf->IsUsingAlternativeBuffer();
            });

        m_resizeManager.QueueResize(width, height);

        if (anyAltBuffer) return;

        UpdatePaneLayout();
        for (UI::Pane* pane : leaves) {
            if (!pane || !pane->GetSession()) continue;
            const UI::PaneRect& bounds = pane->GetBounds();
            int cols = std::max(20, bounds.width / m_layoutCalc.GetCharWidth());
            int rows = std::max(5, bounds.height / m_layoutCalc.GetLineHeight());
            pane->GetSession()->Resize(cols, rows);
        }
    }
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
        ReloadFontSettings();
    }
}

void Application::UpdateFontMetrics() {
    if (m_renderer) {
        int charWidth = m_renderer->GetCharWidth();
        int lineHeight = m_renderer->GetLineHeight();
        m_layoutCalc.SetFontMetrics(charWidth, lineHeight);
        spdlog::info("Font metrics updated: charWidth={}, lineHeight={}", charWidth, lineHeight);
        if (m_mouseHandler) m_mouseHandler->SetCharWidth(charWidth);
    }
}

void Application::ReloadFontSettings() {
    if (!m_config || !m_renderer) return;

    const auto& font = m_config->GetFont();
    std::string fontPath = font.resolvedPath;

    if (fontPath.empty()) {
        fontPath = "C:/Windows/Fonts/consola.ttf";
    }

    spdlog::info("Reloading font: {} size {}", fontPath, font.size);

    if (m_renderer->ReloadFont(fontPath, font.size)) {
        UpdateFontMetrics();
        if (m_window) {
            RECT rect;
            GetClientRect(m_window->GetHandle(), &rect);
            OnWindowResize(rect.right - rect.left, rect.bottom - rect.top);
        }
    }
}

void Application::CreateNewTab() {
    if (!m_tabManager) return;

    int cols = 80, rows = 24;
    if (auto* tab = m_tabManager->GetActiveTab()) {
        if (auto* session = tab->GetFocusedSession(); session && session->GetScreenBuffer()) {
            cols = session->GetScreenBuffer()->GetCols();
            rows = session->GetScreenBuffer()->GetRows();
        }
    }
    m_tabManager->CreateTab(m_shellCommand, cols, rows, m_config->GetTerminal().scrollbackLines);
    UpdatePaneLayout();
    ResizeAllPaneBuffers();
}

void Application::SplitPane(UI::SplitDirection direction) {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    if (!tab) return;

    UI::TerminalSession* focusedSession = tab->GetFocusedSession();
    Terminal::ScreenBuffer* buf = focusedSession ? focusedSession->GetScreenBuffer() : nullptr;
    if (!buf) return;

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
    if (!m_window || !m_tabManager) return;
    int tabCount = m_tabManager->GetTabCount();
    int tabBar = (tabCount > 1) ? LayoutCalculator::kTabBarHeight : 0;
    int width = m_window->GetWidth();
    int height = m_window->GetHeight();

    // Update ALL tabs' pane layouts - important when tab bar visibility changes
    for (const auto& tab : m_tabManager->GetTabs()) {
        tab->GetPaneManager().UpdateLayout(width, height, tabBar);
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
        int cols = std::max(20, bounds.width / m_layoutCalc.GetCharWidth());
        int rows = std::max(5, bounds.height / m_layoutCalc.GetLineHeight());
        pane->GetSession()->Resize(cols, rows);
    }
}

} // namespace Core
} // namespace TerminalDX12
