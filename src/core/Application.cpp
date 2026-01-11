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

    // Setup window callbacks
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

    // Set process exit callback
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

    // Calculate terminal size and create initial tab
    auto [termCols, termRows] = CalculateTerminalSize(windowDesc.width, windowDesc.height);
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
            UpdatePaneLayout();
            ResizeAllPaneBuffers();
        }
    };
    inputCallbacks.onCloseTab = [this]() {
        if (m_tabManager && m_tabManager->GetTabCount() > 1) {
            m_tabManager->CloseActiveTab();
            UpdatePaneLayout();
            ResizeAllPaneBuffers();
        }
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
        UpdatePaneLayout();
        ResizeAllPaneBuffers();
    };
    mouseCallbacks.getPaneManager = [this]() { return GetActivePaneManager(); };

    m_mouseHandler = std::make_unique<UI::MouseHandler>(
        std::move(mouseCallbacks), m_selectionManager, m_tabManager.get());

    m_mouseHandler->SetScreenToCellConverter([this](int x, int y) -> UI::CellPos {
        CellPos pos = ScreenToCell(x, y);
        return {pos.x, pos.y};
    });

    m_mouseHandler->SetUrlDetector([this](int cellX, int cellY) {
        return UI::UrlHelper::DetectUrlAt(GetActiveScreenBuffer(), cellX, cellY);
    });

    m_mouseHandler->SetCharWidth(m_charWidth);

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

void Application::Render() {
    if (!m_renderer || !m_renderingPipeline) {
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

    if (m_resizeStabilizeFrames > 0) {
        m_resizeStabilizeFrames--;
        return;
    }

    // Build render context
    Rendering::RenderContext ctx;
    ctx.renderer = m_renderer.get();
    ctx.tabManager = m_tabManager.get();
    ctx.searchManager = &m_searchManager;
    ctx.selectionManager = &m_selectionManager;
    ctx.config = m_config.get();
    ctx.charWidth = static_cast<float>(m_charWidth);
    ctx.lineHeight = static_cast<float>(m_lineHeight);
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
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1 || y >= kTabBarHeight) {
        if (m_hoveredTabId != -1) {
            m_window->HideTooltip();
            m_hoveredTabId = -1;
        }
        return;
    }

    float tabX = 5.0f;
    int foundTabId = -1;
    std::wstring tooltipText;

    for (const auto& tab : m_tabManager->GetTabs()) {
        std::string sessionNames;
        const auto& sessions = tab->GetSessions();
        for (size_t j = 0; j < sessions.size(); ++j) {
            if (j > 0) sessionNames += "|";
            sessionNames += GetInitials(sessions[j]->GetTitle());
        }
        if (sessionNames.empty()) sessionNames = "T";

        int sessionsLen = static_cast<int>(sessionNames.length() * m_charWidth) + 30;
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

        m_pendingResize = true;
        m_pendingWidth = width;
        m_pendingHeight = height;

        if (anyAltBuffer) return;

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
        ReloadFontSettings();
    }
}

void Application::UpdateFontMetrics() {
    if (m_renderer) {
        m_charWidth = m_renderer->GetCharWidth();
        m_lineHeight = m_renderer->GetLineHeight();
        spdlog::info("Font metrics updated: charWidth={}, lineHeight={}", m_charWidth, m_lineHeight);
        if (m_mouseHandler) m_mouseHandler->SetCharWidth(m_charWidth);
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

} // namespace Core
} // namespace TerminalDX12
