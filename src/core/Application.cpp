#include "core/Application.h"
#include "core/Config.h"
#include "core/Window.h"
#include "rendering/DX12Renderer.h"
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include "ui/Pane.h"
#include "ui/SettingsDialog.h"
#include "ui/UrlHelper.h"
#include "pty/ConPtySession.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <string>
#include <shellapi.h>

namespace {
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
    m_window->OnMouseWheel = [this](int delta) {
        if (m_inputHandler) m_inputHandler->OnMouseWheel(delta);
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

    // Create tab manager
    m_tabManager = std::make_unique<UI::TabManager>();

    // Set tab created callback for OSC 52 clipboard access
    m_tabManager->SetTabCreatedCallback([this](UI::Tab* tab) {
        if (tab) {
            tab->SetClipboardReadCallback([this]() {
                HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
                return m_selectionManager.GetClipboardText(hwnd);
            });
            tab->SetClipboardWriteCallback([this](const std::string& text) {
                HWND hwnd = m_window ? m_window->GetHandle() : nullptr;
                m_selectionManager.SetClipboardText(text, hwnd);
            });
        }
    });

    // Set process exit callback - close window when all processes have exited
    m_tabManager->SetProcessExitCallback([this](int tabId, int exitCode) {
        spdlog::info("Process in tab {} exited with code {}", tabId, exitCode);

        // Check if any tabs still have running processes
        bool anyRunning = false;
        for (const auto& tab : m_tabManager->GetTabs()) {
            if (tab && tab->IsRunning()) {
                anyRunning = true;
                break;
            }
        }

        // If no tabs have running processes, close the window
        if (!anyRunning) {
            spdlog::info("All shell processes have exited, closing window");
            m_running = false;
        }
    });

    // Calculate terminal size based on window dimensions
    // For initial tab, there's no tab bar visible yet, so startY should be kPadding
    const int startY = kPadding;  // Single tab = no tab bar = minimal padding

    int availableWidth = windowDesc.width - kStartX - kPadding;
    int availableHeight = windowDesc.height - startY - kPadding;
    int termCols = std::max(20, availableWidth / kCharWidth);
    int termRows = std::max(5, availableHeight / kLineHeight);
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;


    // Create initial tab
    UI::Tab* initialTab = m_tabManager->CreateTab(m_shellCommand, termCols, termRows, scrollbackLines);
    if (!initialTab) {
        spdlog::warn("Failed to start initial terminal session - continuing without ConPTY");
    } else {
        spdlog::info("Terminal session started successfully");

        // Initialize pane manager with the initial tab
        m_paneManager.Initialize(initialTab);
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
            UI::Tab* activeTab = m_tabManager->GetActiveTab();
            if (activeTab && activeTab->GetScreenBuffer()) {
                cols = activeTab->GetScreenBuffer()->GetCols();
                rows = activeTab->GetScreenBuffer()->GetRows();
            }
            int scrollbackLines = m_config->GetTerminal().scrollbackLines;
            m_tabManager->CreateTab(m_shellCommand, cols, rows, scrollbackLines);
            spdlog::info("Created new tab ({}x{})", cols, rows);
        }
    };
    inputCallbacks.onCloseTab = [this]() {
        if (m_tabManager && m_tabManager->GetTabCount() > 1) {
            m_tabManager->CloseActiveTab();
            spdlog::info("Closed active tab");
        }
    };
    inputCallbacks.onNextTab = [this]() {
        if (m_tabManager) m_tabManager->NextTab();
    };
    inputCallbacks.onPreviousTab = [this]() {
        if (m_tabManager) m_tabManager->PreviousTab();
    };
    inputCallbacks.onSwitchToTab = [this](int tabIndex) {
        if (m_tabManager) {
            const auto& tabs = m_tabManager->GetTabs();
            if (tabIndex < static_cast<int>(tabs.size())) {
                m_tabManager->SwitchToTab(tabs[tabIndex]->GetId());
                spdlog::debug("Switched to tab {} via Ctrl+{}", tabIndex + 1, tabIndex + 1);
            }
        }
    };
    inputCallbacks.onSplitPane = [this](UI::SplitDirection dir) { SplitPane(dir); };
    inputCallbacks.onClosePane = [this]() { ClosePane(); };
    inputCallbacks.onToggleZoom = [this]() {
        m_paneManager.ToggleZoom();
        UpdatePaneLayout();
        spdlog::info("Pane zoom {}", m_paneManager.IsZoomed() ? "enabled" : "disabled");
    };
    inputCallbacks.onFocusNextPane = [this]() { m_paneManager.FocusNextPane(); };
    inputCallbacks.onFocusPreviousPane = [this]() { m_paneManager.FocusPreviousPane(); };
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
    inputCallbacks.hasMultiplePanes = [this]() { return m_paneManager.HasMultiplePanes(); };
    inputCallbacks.getShellCommand = [this]() -> const std::wstring& { return m_shellCommand; };

    m_inputHandler = std::make_unique<UI::InputHandler>(
        std::move(inputCallbacks), m_searchManager, m_selectionManager);

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

    m_mouseHandler = std::make_unique<UI::MouseHandler>(
        std::move(mouseCallbacks), m_selectionManager, m_paneManager, m_tabManager.get());

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

// Helper accessors for active tab's components
Terminal::ScreenBuffer* Application::GetActiveScreenBuffer() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    return tab ? tab->GetScreenBuffer() : nullptr;
}

Terminal::VTStateMachine* Application::GetActiveVTParser() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    return tab ? tab->GetVTParser() : nullptr;
}

Pty::ConPtySession* Application::GetActiveTerminal() {
    UI::Tab* tab = m_tabManager ? m_tabManager->GetActiveTab() : nullptr;
    return tab ? tab->GetTerminal() : nullptr;
}

int Application::GetTerminalStartY() const {
    int tabCount = m_tabManager ? m_tabManager->GetTabCount() : 0;
    return (tabCount > 1) ? kTabBarHeight + 5 : kPadding;
}

int Application::Run() {
    spdlog::info("Application::Run() - entering main loop");

    while (m_running) {
        // Process Windows messages
        if (!ProcessMessages()) {
            spdlog::info("ProcessMessages returned false, exiting loop");
            break;
        }

        // Render
        if (!m_minimized) {
            Render();
        }
    }

    return 0;
}

bool Application::ProcessMessages() {
    MSG msg = {};

    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_running = false;
            return false;
        }

        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return true;
}

void Application::BuildColorPalette(float palette[256][3], Terminal::ScreenBuffer* screenBuffer) {
    const auto& colorConfig = m_config->GetColors();
    for (int i = 0; i < 256; ++i) {
        // Check if this color was modified via OSC 4
        if (screenBuffer && screenBuffer->IsPaletteColorModified(i)) {
            const auto& paletteColor = screenBuffer->GetPaletteColor(i);
            palette[i][0] = paletteColor.r / 255.0f;
            palette[i][1] = paletteColor.g / 255.0f;
            palette[i][2] = paletteColor.b / 255.0f;
        } else if (i < 16) {
            // Use config palette for first 16 colors
            palette[i][0] = colorConfig.palette[i].r / 255.0f;
            palette[i][1] = colorConfig.palette[i].g / 255.0f;
            palette[i][2] = colorConfig.palette[i].b / 255.0f;
        } else {
            // Generate 256-color palette (colors 16-255)
            if (i < 232) {
                // 6x6x6 color cube (indices 16-231)
                int idx = i - 16;
                int r = idx / 36;
                int g = (idx / 6) % 6;
                int b = idx % 6;
                palette[i][0] = r ? (r * 40 + 55) / 255.0f : 0.0f;
                palette[i][1] = g ? (g * 40 + 55) / 255.0f : 0.0f;
                palette[i][2] = b ? (b * 40 + 55) / 255.0f : 0.0f;
            } else {
                // Grayscale (indices 232-255)
                int gray = (i - 232) * 10 + 8;
                palette[i][0] = gray / 255.0f;
                palette[i][1] = gray / 255.0f;
                palette[i][2] = gray / 255.0f;
            }
        }
    }
}

void Application::RenderTabBar(int charWidth) {
    if (!m_tabManager || m_tabManager->GetTabCount() <= 1) {
        return;
    }

    // Render tab bar background (dark gray)
    m_renderer->RenderRect(0.0f, 0.0f, 2000.0f, static_cast<float>(kTabBarHeight),
                           0.15f, 0.15f, 0.15f, 1.0f);

    // Render each tab
    float tabX = 5.0f;
    int activeIndex = m_tabManager->GetActiveTabIndex();
    const auto& tabs = m_tabManager->GetTabs();

    for (size_t i = 0; i < tabs.size(); ++i) {
        const auto& tab = tabs[i];
        bool isActive = (static_cast<int>(i) == activeIndex);

        // Tab background
        float bgR = isActive ? 0.3f : 0.2f;
        float bgG = isActive ? 0.3f : 0.2f;
        float bgB = isActive ? 0.35f : 0.2f;

        // Get tab title (convert wstring to UTF-8)
        std::wstring wTitle = tab->GetTitle();
        std::string title;
        for (wchar_t wc : wTitle) {
            if (wc < 128) title += static_cast<char>(wc);
            else title += '?';
        }
        if (title.length() > 15) {
            title = title.substr(0, 12) + "...";
        }

        float tabWidth = static_cast<float>(std::max(80, static_cast<int>(title.length() * charWidth) + 20));

        // Render tab background
        m_renderer->RenderRect(tabX, 3.0f, tabWidth, static_cast<float>(kTabBarHeight - 6),
                               bgR, bgG, bgB, 1.0f);

        // Activity indicator (orange dot if tab has activity and isn't active)
        if (!isActive && tab->HasActivity()) {
            m_renderer->RenderText("\xE2\x97\x8F", tabX + 5.0f, 8.0f, 1.0f, 0.6f, 0.0f, 1.0f);
        }

        // Render tab title
        float textX = tabX + 15.0f + (!isActive && tab->HasActivity() ? 10.0f : 0.0f);
        float textR = isActive ? 1.0f : 0.7f;
        float textG = isActive ? 1.0f : 0.7f;
        float textB = isActive ? 1.0f : 0.7f;
        m_renderer->RenderText(title.c_str(), textX, 8.0f, textR, textG, textB, 1.0f);

        // Tab separator
        m_renderer->RenderRect(tabX + tabWidth - 1.0f, 5.0f, 1.0f, static_cast<float>(kTabBarHeight - 10),
                               0.4f, 0.4f, 0.4f, 1.0f);

        tabX += tabWidth + 5.0f;
    }
}

void Application::RenderCursor(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                               int charWidth, int lineHeight, float cursorR, float cursorG, float cursorB) {
    int cursorX, cursorY;
    screenBuffer->GetCursorPos(cursorX, cursorY);
    bool cursorVisible = screenBuffer->IsCursorVisible();
    int scrollOffset = screenBuffer->GetScrollOffset();

    int rows, cols;
    screenBuffer->GetDimensions(cols, rows);

    if (!cursorVisible || scrollOffset != 0 || cursorY < 0 || cursorY >= rows || cursorX < 0 || cursorX >= cols) {
        return;
    }

    // Calculate blink state (blink every 500ms)
    static auto startTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
    bool showCursor = (elapsed / 500) % 2 == 0;

    if (showCursor) {
        float cursorPosX = static_cast<float>(startX + cursorX * charWidth);
        float cursorPosY = static_cast<float>(startY + cursorY * lineHeight);
        m_renderer->RenderText("_", cursorPosX, cursorPosY, cursorR, cursorG, cursorB, 1.0f);
    }
}

void Application::RenderSearchBar(int startX, int charWidth, int lineHeight) {
    if (!m_searchManager.IsActive()) {
        return;
    }

    const int searchBarHeight = 30;
    int windowHeight = m_window ? m_window->GetHeight() : 720;
    float searchBarY = static_cast<float>(windowHeight - searchBarHeight);

    // Search bar background
    m_renderer->RenderRect(0.0f, searchBarY, 2000.0f, static_cast<float>(searchBarHeight),
                           0.2f, 0.2f, 0.25f, 1.0f);

    // Search label
    m_renderer->RenderText("Search:", 10.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);

    // Search query (convert wstring to UTF-8)
    std::string queryUtf8;
    for (wchar_t wc : m_searchManager.GetQuery()) {
        if (wc < 128) queryUtf8 += static_cast<char>(wc);
        else queryUtf8 += '?';
    }
    m_renderer->RenderText(queryUtf8.c_str(), 80.0f, searchBarY + 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);

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
    if (!m_searchManager.GetMatches().empty()) {
        int startY = GetTerminalStartY();
        int queryLen = static_cast<int>(m_searchManager.GetQuery().length());
        for (int i = 0; i < static_cast<int>(m_searchManager.GetMatches().size()); ++i) {
            const auto& match = m_searchManager.GetMatches()[i];
            bool isCurrent = (i == m_searchManager.GetCurrentMatchIndex());

            for (int j = 0; j < queryLen; ++j) {
                float posX = static_cast<float>(startX + (match.x + j) * charWidth);
                float posY = static_cast<float>(startY + match.y * lineHeight);

                // Current match: bright yellow, other matches: dim yellow
                if (isCurrent) {
                    m_renderer->RenderRect(posX, posY, static_cast<float>(charWidth),
                                           static_cast<float>(lineHeight), 0.8f, 0.6f, 0.0f, 0.6f);
                } else {
                    m_renderer->RenderRect(posX, posY, static_cast<float>(charWidth),
                                           static_cast<float>(lineHeight), 0.5f, 0.4f, 0.0f, 0.4f);
                }
            }
        }
    }
}

void Application::RenderTerminalContent(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                                        int charWidth, int lineHeight, const float palette[256][3]) {
    int rows, cols;
    screenBuffer->GetDimensions(cols, rows);

    // Calculate normalized selection bounds for rendering
    int selStartY = 0, selEndY = -1, selStartX = 0, selEndX = 0;
    if (m_selectionManager.HasSelection()) {
        const auto& selStart = m_selectionManager.GetSelectionStart();
        const auto& selEnd = m_selectionManager.GetSelectionEnd();
        selStartY = std::min(selStart.y, selEnd.y);
        selEndY = std::max(selStart.y, selEnd.y);
        // For rectangle selection, use min/max X; for normal selection, use order-based X
        if (m_selectionManager.IsRectangleSelection()) {
            selStartX = std::min(selStart.x, selEnd.x);
            selEndX = std::max(selStart.x, selEnd.x);
        } else if (selStart.y < selEnd.y ||
            (selStart.y == selEnd.y && selStart.x <= selEnd.x)) {
            selStartX = selStart.x;
            selEndX = selEnd.x;
        } else {
            selStartX = selEnd.x;
            selEndX = selStart.x;
        }
    }

    for (int y = 0; y < rows; ++y) {
        // Selection highlight pass (render blue highlight for selected text)
        if (m_selectionManager.HasSelection() && y >= selStartY && y <= selEndY) {
            int rowSelStart, rowSelEnd;
            if (m_selectionManager.IsRectangleSelection()) {
                rowSelStart = selStartX;
                rowSelEnd = selEndX;
            } else {
                rowSelStart = (y == selStartY) ? selStartX : 0;
                rowSelEnd = (y == selEndY) ? selEndX : cols - 1;
            }

            for (int x = rowSelStart; x <= rowSelEnd; ++x) {
                float posX = static_cast<float>(startX + x * charWidth);
                float posY = static_cast<float>(startY + y * lineHeight);
                m_renderer->RenderText("\xE2\x96\x88", posX, posY, 0.2f, 0.4f, 0.8f, 0.5f);
            }
        }

        // First pass: Render backgrounds for non-default colors
        for (int x = 0; x < cols; ) {
            auto cell = screenBuffer->GetCellWithScrollback(x, y);

            bool hasTrueColorBg = cell.attr.UsesTrueColorBg();
            bool hasTrueColorFg = cell.attr.UsesTrueColorFg();
            uint8_t bgIndex = cell.attr.IsInverse() ? cell.attr.foreground % 16 : cell.attr.background % 16;

            bool hasNonDefaultBg = false;
            float bgR = 0.0f, bgG = 0.0f, bgB = 0.0f;

            if (cell.attr.IsInverse()) {
                if (hasTrueColorFg) {
                    bgR = cell.attr.fgR / 255.0f;
                    bgG = cell.attr.fgG / 255.0f;
                    bgB = cell.attr.fgB / 255.0f;
                    hasNonDefaultBg = true;
                } else if (bgIndex != 0) {
                    const float* color = palette[bgIndex];
                    bgR = color[0];
                    bgG = color[1];
                    bgB = color[2];
                    hasNonDefaultBg = true;
                }
            } else {
                if (hasTrueColorBg) {
                    bgR = cell.attr.bgR / 255.0f;
                    bgG = cell.attr.bgG / 255.0f;
                    bgB = cell.attr.bgB / 255.0f;
                    hasNonDefaultBg = true;
                } else if (bgIndex != 0) {
                    const float* color = palette[bgIndex];
                    bgR = color[0];
                    bgG = color[1];
                    bgB = color[2];
                    hasNonDefaultBg = true;
                }
            }

            if (!hasNonDefaultBg) {
                x++;
                continue;
            }

            float posX = static_cast<float>(startX + x * charWidth);
            float posY = static_cast<float>(startY + y * lineHeight);
            m_renderer->RenderText("\xE2\x96\x88", posX, posY, bgR, bgG, bgB, 1.0f);

            x++;
        }

        // Second pass: Render foreground text character by character
        for (int x = 0; x < cols; ++x) {
            auto cell = screenBuffer->GetCellWithScrollback(x, y);

            if (cell.ch == U' ' || cell.ch == U'\0') {
                continue;
            }

            // Skip invalid/garbage codepoints
            if (cell.ch > 0x10FFFF ||
                (cell.ch < 0x20 && cell.ch != U'\t') ||
                (cell.ch >= 0xD800 && cell.ch <= 0xDFFF) ||
                (cell.ch >= 0xF0000) ||
                ((cell.ch & 0xFFFF) == 0xFFFE) ||
                ((cell.ch & 0xFFFF) == 0xFFFF)) {
                continue;
            }

            // Get foreground color
            float fgR, fgG, fgB;
            if (cell.attr.UsesTrueColorFg()) {
                fgR = cell.attr.fgR / 255.0f;
                fgG = cell.attr.fgG / 255.0f;
                fgB = cell.attr.fgB / 255.0f;
            } else {
                uint8_t fgIndex = cell.attr.foreground % 16;
                const float* fgColor = palette[fgIndex];
                fgR = fgColor[0];
                fgG = fgColor[1];
                fgB = fgColor[2];
            }

            // Get background color
            float bgR, bgG, bgB;
            if (cell.attr.UsesTrueColorBg()) {
                bgR = cell.attr.bgR / 255.0f;
                bgG = cell.attr.bgG / 255.0f;
                bgB = cell.attr.bgB / 255.0f;
            } else {
                uint8_t bgIndex = cell.attr.background % 16;
                const float* bgColor = palette[bgIndex];
                bgR = bgColor[0];
                bgG = bgColor[1];
                bgB = bgColor[2];
            }

            // Handle inverse attribute
            float r, g, b;
            if (cell.attr.IsInverse()) {
                r = bgR;
                g = bgG;
                b = bgB;
            } else {
                r = fgR;
                g = fgG;
                b = fgB;
            }

            // Apply bold by brightening colors (only for non-true-color)
            if (cell.attr.IsBold() && !cell.attr.UsesTrueColorFg()) {
                r = std::min(1.0f, r + 0.2f);
                g = std::min(1.0f, g + 0.2f);
                b = std::min(1.0f, b + 0.2f);
            }

            // Apply dim by reducing intensity
            if (cell.attr.IsDim()) {
                r *= 0.5f;
                g *= 0.5f;
                b *= 0.5f;
            }

            float posX = static_cast<float>(startX + x * charWidth);
            float posY = static_cast<float>(startY + y * lineHeight);

            std::string utf8Char = Utf32ToUtf8(cell.ch);
            m_renderer->RenderChar(utf8Char, posX, posY, r, g, b, 1.0f);

            // Render underline if present
            if (cell.attr.IsUnderline()) {
                float underlineY = posY + lineHeight - 1;
                float underlineHeight = 3.0f;
                m_renderer->RenderRect(posX, underlineY, static_cast<float>(charWidth),
                                       underlineHeight, r, g, b, 1.0f);
            }
        }
    }
}

void Application::Render() {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!m_renderer || !screenBuffer) {
        return;
    }

    // Apply any pending resize BEFORE starting the frame
    if (m_pendingResize) {
        m_pendingResize = false;

        spdlog::info("Applying deferred DX12 resize: {}x{}", m_pendingWidth, m_pendingHeight);

        // Resize renderer only, skip buffer resize for this frame
        m_renderer->Resize(m_pendingWidth, m_pendingHeight);

        // Queue ConPTY resize for next frame
        m_pendingConPTYResize = true;

        // Skip rest of frame to let DX12 stabilize
        return;
    }

    // Resize ConPTY after DX12 has stabilized (one frame later)
    if (m_pendingConPTYResize) {
        m_pendingConPTYResize = false;

        int startY = GetTerminalStartY();
        int availableWidth = m_pendingWidth - kStartX - kPadding;
        int availableHeight = m_pendingHeight - startY - kPadding;
        int newCols = std::max(20, availableWidth / kCharWidth);
        int newRows = std::max(5, availableHeight / kLineHeight);

        spdlog::info("Applying deferred ConPTY resize: {}x{}", newCols, newRows);

        if (m_tabManager) {
            for (const auto& tab : m_tabManager->GetTabs()) {
                if (tab) {
                    tab->ResizeScreenBuffer(newCols, newRows);
                    tab->ResizeConPTY(newCols, newRows);
                }
            }
        }

        // Skip this frame to let TUI app process resize
        m_resizeStabilizeFrames = 2;
        return;
    }

    // Skip frames while resize is stabilizing
    if (m_resizeStabilizeFrames > 0) {
        m_resizeStabilizeFrames--;
        return;
    }

    m_renderer->BeginFrame();
    m_renderer->ClearText();

    int startY = GetTerminalStartY();

    // Build color palette
    float colorPalette[256][3];
    BuildColorPalette(colorPalette, screenBuffer);

    // Cursor color from config
    const auto& colorConfig = m_config->GetColors();
    float cursorR = colorConfig.cursor.r / 255.0f;
    float cursorG = colorConfig.cursor.g / 255.0f;
    float cursorB = colorConfig.cursor.b / 255.0f;

    // Render tab bar if multiple tabs exist
    RenderTabBar(kCharWidth);

    // Render terminal content (selection, backgrounds, text)
    RenderTerminalContent(screenBuffer, kStartX, startY, kCharWidth, kLineHeight, colorPalette);

    // Render cursor
    RenderCursor(screenBuffer, kStartX, startY, kCharWidth, kLineHeight, cursorR, cursorG, cursorB);

    // Render search bar and highlights
    RenderSearchBar(kStartX, kCharWidth, kLineHeight);

    m_renderer->EndFrame();
    m_renderer->Present();
}

void Application::OnWindowResize(int width, int height) {
    m_minimized = (width == 0 || height == 0);

    if (!m_minimized && m_renderer) {
        // Check if any tab is using alt buffer (TUI app running)
        bool anyAltBuffer = false;
        if (m_tabManager) {
            for (const auto& tab : m_tabManager->GetTabs()) {
                if (tab) {
                    auto* screenBuffer = tab->GetScreenBuffer();
                    if (screenBuffer && screenBuffer->IsUsingAlternativeBuffer()) {
                        anyAltBuffer = true;
                        break;
                    }
                }
            }
        }

        // If TUI is running, only resize DX12 renderer (skip buffer resize)
        if (anyAltBuffer) {
            spdlog::info("OnWindowResize: alt buffer active, only resizing DX12 renderer {}x{}", width, height);
            m_pendingResize = true;
            m_pendingWidth = width;
            m_pendingHeight = height;
            // Skip ScreenBuffer and ConPTY resize
            return;
        }

        // Defer DX12 renderer resize to next frame start
        m_pendingResize = true;
        m_pendingWidth = width;
        m_pendingHeight = height;

        spdlog::info("OnWindowResize: queued deferred resize {}x{}", width, height);

        // Resize terminal buffers immediately (protected by mutex)
        int startY = GetTerminalStartY();
        int availableWidth = width - kStartX - kPadding;
        int availableHeight = height - startY - kPadding;
        int newCols = std::max(20, availableWidth / kCharWidth);
        int newRows = std::max(5, availableHeight / kLineHeight);

        spdlog::info("OnWindowResize: resizing buffers to {}x{}", newCols, newRows);

        if (m_tabManager) {
            for (const auto& tab : m_tabManager->GetTabs()) {
                if (tab) {
                    tab->ResizeScreenBuffer(newCols, newRows);
                }
            }
        }

        spdlog::info("OnWindowResize: buffer resize complete");
    }
}

Application::CellPos Application::ScreenToCell(int pixelX, int pixelY) const {
    int startY = GetTerminalStartY();

    CellPos pos;
    pos.x = (pixelX - kStartX) / kCharWidth;
    pos.y = (pixelY - startY) / kLineHeight;

    // Clamp to valid range
    Terminal::ScreenBuffer* screenBuffer = const_cast<Application*>(this)->GetActiveScreenBuffer();
    if (screenBuffer) {
        pos.x = std::max(0, std::min(pos.x, screenBuffer->GetCols() - 1));
        pos.y = std::max(0, std::min(pos.y, screenBuffer->GetRows() - 1));
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
    if (!m_window || !m_config) {
        return;
    }

    UI::SettingsDialog dialog(m_window->GetHandle(), m_config.get());
    if (dialog.Show()) {
        spdlog::info("Settings were changed and saved");
    }
}

// Pane management
void Application::SplitPane(UI::SplitDirection direction) {
    UI::Pane* focusedPane = m_paneManager.GetFocusedPane();
    if (!focusedPane || !focusedPane->IsLeaf()) {
        spdlog::warn("Cannot split: no focused leaf pane");
        return;
    }

    UI::Tab* currentTab = focusedPane->GetTab();
    if (!currentTab) {
        spdlog::warn("Cannot split: focused pane has no tab");
        return;
    }

    // Get dimensions from current tab
    Terminal::ScreenBuffer* screenBuffer = currentTab->GetScreenBuffer();
    if (!screenBuffer) {
        return;
    }

    int cols = screenBuffer->GetCols();
    int rows = screenBuffer->GetRows();
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;

    // Create a new tab for the new pane
    UI::Tab* newTab = m_tabManager->CreateTab(m_shellCommand, cols, rows, scrollbackLines);
    if (!newTab) {
        spdlog::error("Failed to create new tab for split pane");
        return;
    }

    // Split the focused pane
    UI::Pane* newPane = m_paneManager.SplitFocusedPane(direction, newTab);
    if (newPane) {
        UpdatePaneLayout();
    }
}

void Application::ClosePane() {
    UI::Tab* tabToClose = m_paneManager.CloseFocusedPane();
    if (tabToClose) {
        m_tabManager->CloseTab(tabToClose->GetId());
        UpdatePaneLayout();
    }
}

void Application::UpdatePaneLayout() {
    if (!m_window) {
        return;
    }

    int width = m_window->GetWidth();
    int height = m_window->GetHeight();
    int tabBar = (m_tabManager && m_tabManager->GetTabCount() > 1) ? kTabBarHeight : 0;

    m_paneManager.UpdateLayout(width, height, tabBar);
}

} // namespace Core
} // namespace TerminalDX12
