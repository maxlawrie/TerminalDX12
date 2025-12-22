#include "core/Application.h"
#include "core/Config.h"
#include "core/Window.h"
#include "rendering/DX12Renderer.h"
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include "ui/SettingsDialog.h"
#include "pty/ConPtySession.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <string>
#include <locale>
#include <codecvt>
#include <shellapi.h>

namespace TerminalDX12 {
namespace Core {

Application* Application::s_instance = nullptr;

Application::Application()
    : m_running(false)
    , m_minimized(false)
    , m_selectionStart{0, 0}
    , m_selectionEnd{0, 0}
    , m_selecting(false)
    , m_hasSelection(false)
    , m_lastClickTime{}
    , m_lastClickPos{0, 0}
    , m_clickCount(0)
    , m_lastMouseButton(-1)
    , m_mouseButtonDown(false)
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
    windowDesc.width = 1920;
    windowDesc.height = 1080;
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
        OnWindowClose();
    };

    // Keyboard input handlers
    m_window->OnCharEvent = [this](wchar_t ch) {
        OnChar(ch);
    };

    m_window->OnKeyEvent = [this](UINT key, bool isDown) {
        OnKey(key, isDown);
    };

    // Mouse handlers
    m_window->OnMouseWheel = [this](int delta) {
        OnMouseWheel(delta);
    };

    m_window->OnMouseButton = [this](int x, int y, int button, bool down) {
        OnMouseButton(x, y, button, down);
    };

    m_window->OnMouseMove = [this](int x, int y) {
        OnMouseMove(x, y);
    };

    // Create DirectX 12 renderer
    m_renderer = std::make_unique<Rendering::DX12Renderer>();
    if (!m_renderer->Initialize(m_window.get())) {
        return false;
    }

    // Create tab manager
    m_tabManager = std::make_unique<UI::TabManager>();

    // Calculate terminal size (80x24 columns/rows for now - will be dynamic later)
    int termCols = 80;
    int termRows = 24;
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;

    // Create initial tab
    UI::Tab* initialTab = m_tabManager->CreateTab(m_shellCommand, termCols, termRows, scrollbackLines);
    if (!initialTab) {
        spdlog::warn("Failed to start initial terminal session - continuing without ConPTY");
    } else {
        spdlog::info("Terminal session started successfully");
    }

    // Apply window transparency from config
    float opacity = m_config->GetTerminal().opacity;
    if (opacity < 1.0f) {
        m_window->SetOpacity(opacity);
        spdlog::info("Window opacity set to {}%", static_cast<int>(opacity * 100));
    }

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
    if (m_tabManager) {
        UI::Tab* tab = m_tabManager->GetActiveTab();
        if (tab) return tab->GetScreenBuffer();
    }
    return nullptr;
}

Terminal::VTStateMachine* Application::GetActiveVTParser() {
    if (m_tabManager) {
        UI::Tab* tab = m_tabManager->GetActiveTab();
        if (tab) return tab->GetVTParser();
    }
    return nullptr;
}

Pty::ConPtySession* Application::GetActiveTerminal() {
    if (m_tabManager) {
        UI::Tab* tab = m_tabManager->GetActiveTab();
        if (tab) return tab->GetTerminal();
    }
    return nullptr;
}

int Application::Run() {
    using Clock = std::chrono::high_resolution_clock;
    auto lastTime = Clock::now();

    spdlog::info("Application::Run() - entering main loop");

    while (m_running) {
        // Process Windows messages
        if (!ProcessMessages()) {
            spdlog::info("ProcessMessages returned false, exiting loop");
            break;
        }

        // Calculate delta time
        auto currentTime = Clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastTime).count();
        lastTime = currentTime;

        // Update
        Update(deltaTime);

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

void Application::Update(float deltaTime) {
    // TODO: Update terminal state, process input, etc.
    (void)deltaTime; // Suppress unused parameter warning
}

void Application::Render() {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!m_renderer || !screenBuffer) {
        return;
    }

    static int frameCount = 0;
    if (frameCount < 5) {
        spdlog::info("Render() called - frame {}", frameCount);
    }
    frameCount++;

    m_renderer->BeginFrame();
    m_renderer->ClearText();

    // Get color palette from config
    const auto& colorConfig = m_config->GetColors();
    float colorPalette[16][3];
    for (int i = 0; i < 16; ++i) {
        colorPalette[i][0] = colorConfig.palette[i].r / 255.0f;
        colorPalette[i][1] = colorConfig.palette[i].g / 255.0f;
        colorPalette[i][2] = colorConfig.palette[i].b / 255.0f;
    }
    
    // Cursor color from config
    float cursorR = colorConfig.cursor.r / 255.0f;
    float cursorG = colorConfig.cursor.g / 255.0f;
    float cursorB = colorConfig.cursor.b / 255.0f;

    // Render screen buffer contents
    const int fontSize = 16;  // Should match the font size used by renderer
    const int lineHeight = 25;  // Should match the line height from GlyphAtlas
    const int charWidth = 10;   // Approximate width for monospace font
    const int startX = 10;
    const int tabBarHeight = 30;  // Height reserved for tab bar

    // Render tab bar if multiple tabs exist
    int startY = 10;
    if (m_tabManager && m_tabManager->GetTabCount() > 1) {
        startY = tabBarHeight + 5;  // Offset terminal below tab bar

        // Render tab bar background (dark gray)
        m_renderer->RenderRect(0.0f, 0.0f, 2000.0f, static_cast<float>(tabBarHeight),
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
            m_renderer->RenderRect(tabX, 3.0f, tabWidth, static_cast<float>(tabBarHeight - 6),
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
            m_renderer->RenderRect(tabX + tabWidth - 1.0f, 5.0f, 1.0f, static_cast<float>(tabBarHeight - 10),
                                   0.4f, 0.4f, 0.4f, 1.0f);

            tabX += tabWidth + 5.0f;
        }
    }

    int rows = screenBuffer->GetRows();
    int cols = screenBuffer->GetCols();

    // Get cursor position for rendering
    int cursorX, cursorY;
    screenBuffer->GetCursorPos(cursorX, cursorY);
    bool cursorVisible = screenBuffer->IsCursorVisible();

    // Calculate normalized selection bounds for rendering
    int selStartY = 0, selEndY = -1, selStartX = 0, selEndX = 0;
    if (m_hasSelection) {
        selStartY = std::min(m_selectionStart.y, m_selectionEnd.y);
        selEndY = std::max(m_selectionStart.y, m_selectionEnd.y);
        if (m_selectionStart.y < m_selectionEnd.y ||
            (m_selectionStart.y == m_selectionEnd.y && m_selectionStart.x <= m_selectionEnd.x)) {
            selStartX = m_selectionStart.x;
            selEndX = m_selectionEnd.x;
        } else {
            selStartX = m_selectionEnd.x;
            selEndX = m_selectionStart.x;
        }
    }

    for (int y = 0; y < rows; ++y) {
        // Selection highlight pass (render blue highlight for selected text)
        if (m_hasSelection && y >= selStartY && y <= selEndY) {
            int rowSelStart = (y == selStartY) ? selStartX : 0;
            int rowSelEnd = (y == selEndY) ? selEndX : cols - 1;

            for (int x = rowSelStart; x <= rowSelEnd; ++x) {
                float posX = static_cast<float>(startX + x * charWidth);
                float posY = static_cast<float>(startY + y * lineHeight);
                // Render blue selection highlight using block character
                m_renderer->RenderText("\xE2\x96\x88", posX, posY, 0.2f, 0.4f, 0.8f, 0.5f);
            }
        }

        // First pass: Render backgrounds for non-default colors
        for (int x = 0; x < cols; ) {
            const auto& cell = screenBuffer->GetCellWithScrollback(x, y);

            // Determine if this cell has a non-default background
            bool hasTrueColorBg = cell.attr.UsesTrueColorBg();
            bool hasTrueColorFg = cell.attr.UsesTrueColorFg();
            uint8_t bgIndex = cell.attr.IsInverse() ? cell.attr.foreground % 16 : cell.attr.background % 16;

            // For inverse, we use the foreground color as background
            bool hasNonDefaultBg = false;
            float bgR = 0.0f, bgG = 0.0f, bgB = 0.0f;

            if (cell.attr.IsInverse()) {
                if (hasTrueColorFg) {
                    bgR = cell.attr.fgR / 255.0f;
                    bgG = cell.attr.fgG / 255.0f;
                    bgB = cell.attr.fgB / 255.0f;
                    hasNonDefaultBg = true;
                } else if (bgIndex != 0) {
                    const float* color = colorPalette[bgIndex];
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
                    const float* color = colorPalette[bgIndex];
                    bgR = color[0];
                    bgG = color[1];
                    bgB = color[2];
                    hasNonDefaultBg = true;
                }
            }

            // Skip default background (black)
            if (!hasNonDefaultBg) {
                x++;
                continue;
            }

            // Render single background cell
            float posX = static_cast<float>(startX + x * charWidth);
            float posY = static_cast<float>(startY + y * lineHeight);
            m_renderer->RenderText("\xE2\x96\x88", posX, posY, bgR, bgG, bgB, 1.0f);

            x++;
        }

        // Second pass: Render foreground text character by character for precise grid alignment
        for (int x = 0; x < cols; ++x) {
            const auto& cell = screenBuffer->GetCellWithScrollback(x, y);

            // Skip spaces
            if (cell.ch == U' ' || cell.ch == U'\0') {
                continue;
            }

            // Get foreground color - check for true color first
            float fgR, fgG, fgB;
            if (cell.attr.UsesTrueColorFg()) {
                // Use 24-bit true color
                fgR = cell.attr.fgR / 255.0f;
                fgG = cell.attr.fgG / 255.0f;
                fgB = cell.attr.fgB / 255.0f;
            } else {
                // Use palette color
                uint8_t fgIndex = cell.attr.foreground % 16;
                const float* fgColor = colorPalette[fgIndex];
                fgR = fgColor[0];
                fgG = fgColor[1];
                fgB = fgColor[2];
            }

            // Get background color - check for true color first
            float bgR, bgG, bgB;
            if (cell.attr.UsesTrueColorBg()) {
                // Use 24-bit true color
                bgR = cell.attr.bgR / 255.0f;
                bgG = cell.attr.bgG / 255.0f;
                bgB = cell.attr.bgB / 255.0f;
            } else {
                // Use palette color
                uint8_t bgIndex = cell.attr.background % 16;
                const float* bgColor = colorPalette[bgIndex];
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

            // Calculate exact grid position for this character
            float posX = static_cast<float>(startX + x * charWidth);
            float posY = static_cast<float>(startY + y * lineHeight);

            // Convert single character to UTF-8
            std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
            std::string utf8Char = converter.to_bytes(std::u32string(1, cell.ch));

            // Render single character at exact grid position
            m_renderer->RenderChar(utf8Char, posX, posY, r, g, b, 1.0f);

            // Render underline if the cell has the underline attribute
            if (cell.attr.IsUnderline()) {
                // Render underline extending slightly below the cell for visibility
                // This ensures underlined text is visually distinct (2-3 pixels taller)
                float underlineY = posY + lineHeight - 1;  // Start near bottom of cell
                float underlineHeight = 3.0f;  // 3 pixels thick, extends below cell
                // Use same color as the text
                m_renderer->RenderRect(posX, underlineY, static_cast<float>(charWidth),
                                       underlineHeight, r, g, b, 1.0f);
            }
        }
    }

    // Render blinking cursor (only when not scrolled back)
    int scrollOffset = screenBuffer->GetScrollOffset();
    if (cursorVisible && scrollOffset == 0 && cursorY >= 0 && cursorY < rows && cursorX >= 0 && cursorX < cols) {
        // Calculate blink state (blink every 500ms)
        static auto startTime = std::chrono::steady_clock::now();
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
        bool showCursor = (elapsed / 500) % 2 == 0;

        if (showCursor) {
            // Render cursor as a block character
            float cursorPosX = static_cast<float>(startX + cursorX * charWidth);
            float cursorPosY = static_cast<float>(startY + cursorY * lineHeight);

            // Render underscore or block cursor
            m_renderer->RenderText("_", cursorPosX, cursorPosY, cursorR, cursorG, cursorB, 1.0f);
        }
    }

    // Render search bar if in search mode
    if (m_searchMode) {
        const int searchBarHeight = 30;
        int windowHeight = 720;  // TODO: Get actual window height
        float searchBarY = static_cast<float>(windowHeight - searchBarHeight);

        // Search bar background
        m_renderer->RenderRect(0.0f, searchBarY, 2000.0f, static_cast<float>(searchBarHeight),
                               0.2f, 0.2f, 0.25f, 1.0f);

        // Search label
        m_renderer->RenderText("Search:", 10.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);

        // Search query (convert wstring to UTF-8)
        std::string queryUtf8;
        for (wchar_t wc : m_searchQuery) {
            if (wc < 128) queryUtf8 += static_cast<char>(wc);
            else queryUtf8 += '?';
        }
        m_renderer->RenderText(queryUtf8.c_str(), 80.0f, searchBarY + 5.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        // Search results count
        char countBuf[64];
        if (m_searchMatches.empty()) {
            snprintf(countBuf, sizeof(countBuf), "No matches");
        } else {
            snprintf(countBuf, sizeof(countBuf), "%d of %d",
                     m_currentMatchIndex + 1, static_cast<int>(m_searchMatches.size()));
        }
        m_renderer->RenderText(countBuf, 400.0f, searchBarY + 5.0f, 0.7f, 0.7f, 0.7f, 1.0f);

        // Instructions
        m_renderer->RenderText("F3/Enter: Next  Shift+F3: Prev  Esc: Close",
                               550.0f, searchBarY + 5.0f, 0.5f, 0.5f, 0.5f, 1.0f);
    }

    // Highlight search matches
    if (m_searchMode && !m_searchMatches.empty()) {
        int queryLen = static_cast<int>(m_searchQuery.length());
        for (int i = 0; i < static_cast<int>(m_searchMatches.size()); ++i) {
            const auto& match = m_searchMatches[i];
            bool isCurrent = (i == m_currentMatchIndex);

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

    m_renderer->EndFrame();
    m_renderer->Present();
}

void Application::OnWindowResize(int width, int height) {
    m_minimized = (width == 0 || height == 0);

    if (!m_minimized && m_renderer) {
        m_renderer->Resize(width, height);
    }
}

void Application::OnWindowClose() {
    m_running = false;
}

void Application::OnTerminalOutput(const char* data, size_t size) {
    // This is now handled internally by each Tab
    // Kept for potential external callbacks
    (void)data;
    (void)size;
}

void Application::OnChar(wchar_t ch) {
    // Handle search mode text input
    if (m_searchMode) {
        // Only accept printable characters
        if (ch >= 32) {
            m_searchQuery += ch;
            UpdateSearchResults();
        }
        return;
    }

    Pty::ConPtySession* terminal = GetActiveTerminal();
    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    // Skip control characters that are handled by OnKey via WM_KEYDOWN
    // These would otherwise be sent twice (once by WM_KEYDOWN, once by WM_CHAR)
    if (ch == L'\b' ||   // Backspace - handled by VK_BACK
        ch == L'	' ||   // Tab - handled by VK_TAB
        ch == L'\r' ||   // Enter - handled by VK_RETURN
        ch == L'') { // Escape - handled by VK_ESCAPE
        return;
    }

    // Convert wchar_t to UTF-8 for ConPTY
    char utf8[4] = {0};
    int len = WideCharToMultiByte(CP_UTF8, 0, &ch, 1, utf8, sizeof(utf8), nullptr, nullptr);

    if (len > 0) {
        terminal->WriteInput(utf8, len);
    }
}

// Helper to get scan code for a virtual key
static WORD GetScanCodeForVK(UINT vk) {
    return static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
}

// Send key in Win32 input mode format: ESC [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
void Application::SendWin32InputKey(UINT vk, wchar_t unicodeChar, bool keyDown, DWORD controlState) {
    Pty::ConPtySession* terminal = GetActiveTerminal();
    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    WORD scanCode = GetScanCodeForVK(vk);

    // Format: ESC [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "[%u;%u;%u;%u;%u;1_",
                       vk,
                       scanCode,
                       static_cast<unsigned>(unicodeChar),
                       keyDown ? 1 : 0,
                       controlState);

    if (len > 0 && len < (int)sizeof(buf)) {
        terminal->WriteInput(buf, len);
    }
}

void Application::OnKey(UINT key, bool isDown) {
    if (!isDown) {
        return;
    }

    // Handle search mode input
    if (m_searchMode) {
        if (key == VK_ESCAPE) {
            CloseSearch();
            return;
        }
        if (key == VK_RETURN) {
            // Enter: Next match (Shift+Enter: Previous)
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                SearchPrevious();
            } else {
                SearchNext();
            }
            return;
        }
        if (key == VK_F3) {
            // F3: Next match (Shift+F3: Previous)
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                SearchPrevious();
            } else {
                SearchNext();
            }
            return;
        }
        if (key == VK_BACK) {
            // Backspace: Remove last character
            if (!m_searchQuery.empty()) {
                m_searchQuery.pop_back();
                UpdateSearchResults();
            }
            return;
        }
        // Other keys handled by OnChar for text input
        return;
    }

    Pty::ConPtySession* terminal = GetActiveTerminal();
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();

    // Handle Ctrl+key shortcuts
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        // Ctrl+Comma: Open settings
        if (key == VK_OEM_COMMA) {
            ShowSettings();
            return;
        }
        // Ctrl+Shift+N: New window
        if (key == 'N' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            // Launch a new instance of the application
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(nullptr, exePath, MAX_PATH)) {
                STARTUPINFOW si = {};
                si.cb = sizeof(si);
                PROCESS_INFORMATION pi = {};

                // Pass the same shell command if specified
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
            return;
        }
        // Ctrl+Shift+F: Open search
        if (key == 'F' && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
            OpenSearch();
            return;
        }
        // Tab management shortcuts
        if (key == 'T') {
            // Ctrl+T: New tab
            if (m_tabManager) {
                int scrollbackLines = m_config->GetTerminal().scrollbackLines;
                m_tabManager->CreateTab(m_shellCommand, 80, 24, scrollbackLines);
                spdlog::info("Created new tab");
            }
            return;
        }
        if (key == 'W') {
            // Ctrl+W: Close active tab
            if (m_tabManager && m_tabManager->GetTabCount() > 1) {
                m_tabManager->CloseActiveTab();
                spdlog::info("Closed active tab");
            } else if (m_tabManager && m_tabManager->GetTabCount() == 1) {
                // Last tab - close window
                m_running = false;
            }
            return;
        }
        if (key == VK_TAB) {
            // Ctrl+Tab / Ctrl+Shift+Tab: Switch tabs
            if (m_tabManager) {
                if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
                    m_tabManager->PreviousTab();
                } else {
                    m_tabManager->NextTab();
                }
            }
            return;
        }

        // Ctrl+Up/Down: Navigate between shell prompts (OSC 133)
        if (key == VK_UP && screenBuffer) {
            // Calculate current absolute line position
            int currentLine = screenBuffer->GetCursorY() +
                             (screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                             screenBuffer->GetRows() + screenBuffer->GetScrollOffset();
            int prevPrompt = screenBuffer->GetPreviousPromptLine(currentLine);
            if (prevPrompt >= 0) {
                // Scroll to show the previous prompt
                int scrollbackTotal = static_cast<int>(screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                                     screenBuffer->GetRows();
                int targetOffset = std::max(0, scrollbackTotal - prevPrompt + screenBuffer->GetRows() / 2);
                screenBuffer->SetScrollOffset(targetOffset);
                spdlog::debug("Jumped to previous prompt at line {}", prevPrompt);
            }
            return;
        }
        if (key == VK_DOWN && screenBuffer) {
            int currentLine = screenBuffer->GetCursorY() +
                             (screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                             screenBuffer->GetRows() + screenBuffer->GetScrollOffset();
            int nextPrompt = screenBuffer->GetNextPromptLine(currentLine);
            if (nextPrompt >= 0) {
                int scrollbackTotal = static_cast<int>(screenBuffer->GetBuffer().size() / screenBuffer->GetCols()) -
                                     screenBuffer->GetRows();
                int targetOffset = std::max(0, scrollbackTotal - nextPrompt + screenBuffer->GetRows() / 2);
                screenBuffer->SetScrollOffset(targetOffset);
                spdlog::debug("Jumped to next prompt at line {}", nextPrompt);
            } else {
                // No next prompt, scroll to bottom
                screenBuffer->ScrollToBottom();
            }
            return;
        }

        // Copy/Paste
        if (key == 'C') {
            if (m_hasSelection) {
                CopySelectionToClipboard();
            } else if (terminal && terminal->IsRunning()) {
                // Send SIGINT (Ctrl+C) to terminal
                terminal->WriteInput("", 1);
            }
            return;
        }
        if (key == 'V') {
            PasteFromClipboard();
            return;
        }
    }

    if (!terminal || !terminal->IsRunning()) {
        return;
    }

    // Handle scrollback navigation
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        // Shift is pressed - handle scrollback
        if (key == VK_PRIOR && screenBuffer) {  // Shift+Page Up
            int currentOffset = screenBuffer->GetScrollOffset();
            screenBuffer->SetScrollOffset(currentOffset + screenBuffer->GetRows());
            return;
        } else if (key == VK_NEXT && screenBuffer) {  // Shift+Page Down
            int currentOffset = screenBuffer->GetScrollOffset();
            screenBuffer->SetScrollOffset(std::max(0, currentOffset - screenBuffer->GetRows()));
            return;
        }
    }

    // Get control key state
    DWORD controlState = 0;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) controlState |= SHIFT_PRESSED;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) controlState |= LEFT_CTRL_PRESSED;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) controlState |= LEFT_ALT_PRESSED;

    // For special keys, use Win32 input mode format
    // This ensures ConPTY correctly interprets the key
    switch (key) {
        case VK_BACK:
            SendWin32InputKey(VK_BACK, L'', true, controlState);
            return;
        case VK_RETURN:
            SendWin32InputKey(VK_RETURN, L'', true, controlState);
            return;
        case VK_TAB:
            SendWin32InputKey(VK_TAB, L'	', true, controlState);
            return;
        case VK_ESCAPE:
            SendWin32InputKey(VK_ESCAPE, L'', true, controlState);
            return;
        case VK_UP:
            SendWin32InputKey(VK_UP, 0, true, controlState);
            return;
        case VK_DOWN:
            SendWin32InputKey(VK_DOWN, 0, true, controlState);
            return;
        case VK_RIGHT:
            SendWin32InputKey(VK_RIGHT, 0, true, controlState);
            return;
        case VK_LEFT:
            SendWin32InputKey(VK_LEFT, 0, true, controlState);
            return;
        case VK_HOME:
            SendWin32InputKey(VK_HOME, 0, true, controlState);
            return;
        case VK_END:
            SendWin32InputKey(VK_END, 0, true, controlState);
            return;
        case VK_PRIOR:  // Page Up
            SendWin32InputKey(VK_PRIOR, 0, true, controlState);
            return;
        case VK_NEXT:   // Page Down
            SendWin32InputKey(VK_NEXT, 0, true, controlState);
            return;
        case VK_INSERT:
            SendWin32InputKey(VK_INSERT, 0, true, controlState);
            return;
        case VK_DELETE:
            SendWin32InputKey(VK_DELETE, 0, true, controlState);
            return;
    }
}


void Application::OnMouseWheel(int delta) {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!screenBuffer) {
        return;
    }

    // Mouse wheel delta is in units of WHEEL_DELTA (120)
    // Positive delta = scroll up (view older content)
    // Negative delta = scroll down (view newer content)

    // Scroll by 3 lines per wheel notch
    const int linesPerNotch = 3;
    int scrollLines = (delta / WHEEL_DELTA) * linesPerNotch;

    int currentOffset = screenBuffer->GetScrollOffset();
    int newOffset = currentOffset + scrollLines;

    // Clamp to valid range
    newOffset = std::max(0, newOffset);

    screenBuffer->SetScrollOffset(newOffset);
}

Application::CellPos Application::ScreenToCell(int pixelX, int pixelY) const {
    // Terminal layout constants (same as in Render())
    const int startX = 10;
    const int startY = 10;
    const int charWidth = 10;
    const int lineHeight = 25;

    CellPos pos;
    pos.x = (pixelX - startX) / charWidth;
    pos.y = (pixelY - startY) / lineHeight;

    // Clamp to valid range
    Terminal::ScreenBuffer* screenBuffer = const_cast<Application*>(this)->GetActiveScreenBuffer();
    if (screenBuffer) {
        pos.x = std::max(0, std::min(pos.x, screenBuffer->GetCols() - 1));
        pos.y = std::max(0, std::min(pos.y, screenBuffer->GetRows() - 1));
    }

    return pos;
}

void Application::OnMouseButton(int x, int y, int button, bool down) {
    // Handle tab bar clicks (only when multiple tabs exist)
    const int tabBarHeight = 30;
    if (m_tabManager && m_tabManager->GetTabCount() > 1 && y < tabBarHeight && button == 0 && down) {
        // Find which tab was clicked
        float tabX = 5.0f;
        const int charWidth = 10;
        const auto& tabs = m_tabManager->GetTabs();

        for (size_t i = 0; i < tabs.size(); ++i) {
            const auto& tab = tabs[i];
            std::wstring wTitle = tab->GetTitle();
            int titleLen = static_cast<int>(wTitle.length());
            if (titleLen > 15) titleLen = 15;

            float tabWidth = static_cast<float>(std::max(80, titleLen * charWidth + 20));

            if (x >= tabX && x < tabX + tabWidth) {
                // Switch to this tab
                m_tabManager->SwitchToTab(tab->GetId());
                return;
            }
            tabX += tabWidth + 5.0f;
        }
        return;  // Click was in tab bar but not on a tab
    }

    CellPos cellPos = ScreenToCell(x, y);

    // Handle Ctrl+Click for URL opening
    if (button == 0 && down && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
        std::string url = DetectUrlAt(cellPos.x, cellPos.y);
        if (!url.empty()) {
            OpenUrl(url);
            return;
        }
    }

    // Right-click context menu
    if (button == 1 && down) {
        ShowContextMenu(x, y);
        return;
    }

    // Report mouse to application if mouse mode is enabled
    if (ShouldReportMouse()) {
        SendMouseEvent(cellPos.x, cellPos.y, button, down, false);
        m_mouseButtonDown = down;
        m_lastMouseButton = button;
        return;
    }

    // Only handle left mouse button for selection
    if (button != 0) {
        return;
    }

    if (down) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClickTime).count();

        // Check for double/triple click (within 500ms and same position)
        if (elapsed < 500 && cellPos.x == m_lastClickPos.x && cellPos.y == m_lastClickPos.y) {
            m_clickCount++;
            if (m_clickCount > 3) m_clickCount = 1;
        } else {
            m_clickCount = 1;
        }

        m_lastClickTime = now;
        m_lastClickPos = cellPos;

        // Handle click type
        if (m_clickCount == 3) {
            // Triple-click: select entire line
            SelectLine(cellPos.y);
            m_selecting = false;
        } else if (m_clickCount == 2) {
            // Double-click: select word
            SelectWord(cellPos.x, cellPos.y);
            m_selecting = false;
        } else {
            // Single-click: start drag selection
            // Check for Shift+Click to extend selection
            if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && m_hasSelection) {
                // Extend selection to this position
                m_selectionEnd = cellPos;
                m_hasSelection = true;
            } else {
                // Start new selection
                m_selectionStart = cellPos;
                m_selectionEnd = cellPos;
                m_hasSelection = false;
            }
            m_selecting = true;
        }
        m_mouseButtonDown = true;
    } else {
        // Button release
        m_selecting = false;
        m_mouseButtonDown = false;

        // Check if we actually selected something (not just a click)
        if (m_selectionStart.x != m_selectionEnd.x || m_selectionStart.y != m_selectionEnd.y) {
            m_hasSelection = true;
        }
    }
}

void Application::OnMouseMove(int x, int y) {
    CellPos cellPos = ScreenToCell(x, y);

    // Report mouse motion if application wants it
    Terminal::VTStateMachine* vtParser = GetActiveVTParser();
    if (ShouldReportMouse() && vtParser) {
        auto mouseMode = vtParser->GetMouseMode();
        // Mode 1003 (All) reports all motion
        // Mode 1002 (Normal) reports motion only while button pressed
        if (mouseMode == Terminal::VTStateMachine::MouseMode::All ||
            (mouseMode == Terminal::VTStateMachine::MouseMode::Normal && m_mouseButtonDown)) {
            SendMouseEvent(cellPos.x, cellPos.y, m_lastMouseButton, true, true);
        }
        return;
    }

    if (!m_selecting) {
        return;
    }

    m_selectionEnd = cellPos;

    // Mark as having selection if we've moved from start
    if (m_selectionStart.x != m_selectionEnd.x || m_selectionStart.y != m_selectionEnd.y) {
        m_hasSelection = true;
    }
}

void Application::ClearSelection() {
    m_hasSelection = false;
    m_selecting = false;
    m_selectionStart = {0, 0};
    m_selectionEnd = {0, 0};
}

std::string Application::GetSelectedText() const {
    Terminal::ScreenBuffer* screenBuffer = const_cast<Application*>(this)->GetActiveScreenBuffer();
    if (!m_hasSelection || !screenBuffer) {
        return "";
    }

    // Normalize selection (ensure start <= end)
    int startY = std::min(m_selectionStart.y, m_selectionEnd.y);
    int endY = std::max(m_selectionStart.y, m_selectionEnd.y);
    int startX, endX;

    if (m_selectionStart.y < m_selectionEnd.y ||
        (m_selectionStart.y == m_selectionEnd.y && m_selectionStart.x <= m_selectionEnd.x)) {
        startX = m_selectionStart.x;
        endX = m_selectionEnd.x;
    } else {
        startX = m_selectionEnd.x;
        endX = m_selectionStart.x;
    }

    std::u32string text;
    int cols = screenBuffer->GetCols();

    for (int y = startY; y <= endY; ++y) {
        int rowStartX = (y == startY) ? startX : 0;
        int rowEndX = (y == endY) ? endX : cols - 1;

        for (int x = rowStartX; x <= rowEndX; ++x) {
            const auto& cell = screenBuffer->GetCellWithScrollback(x, y);
            text += cell.ch;
        }

        // Add newline between rows (but not after last row)
        if (y < endY) {
            // Trim trailing spaces from line before adding newline
            while (!text.empty() && text.back() == U' ') {
                text.pop_back();
            }
            text += U'\n';
        }
    }

    // Trim trailing spaces from last line
    while (!text.empty() && text.back() == U' ') {
        text.pop_back();
    }

    // Convert to UTF-8
    std::string utf8;
    for (char32_t ch : text) {
        if (ch < 0x80) {
            utf8 += static_cast<char>(ch);
        } else if (ch < 0x800) {
            utf8 += static_cast<char>(0xC0 | (ch >> 6));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            utf8 += static_cast<char>(0xE0 | (ch >> 12));
            utf8 += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            utf8 += static_cast<char>(0xF0 | (ch >> 18));
            utf8 += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
            utf8 += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }

    return utf8;
}

void Application::CopySelectionToClipboard() {
    if (!m_hasSelection || !m_window) {
        return;
    }

    std::string text = GetSelectedText();
    if (text.empty()) {
        return;
    }

    // Convert UTF-8 to wide string for clipboard
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) {
        return;
    }

    HWND hwnd = m_window->GetHandle();

    if (!OpenClipboard(hwnd)) {
        spdlog::error("Failed to open clipboard");
        return;
    }

    EmptyClipboard();

    // Allocate global memory for clipboard
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, wideLen * sizeof(wchar_t));
    if (!hGlobal) {
        CloseClipboard();
        spdlog::error("Failed to allocate clipboard memory");
        return;
    }

    wchar_t* pGlobal = static_cast<wchar_t*>(GlobalLock(hGlobal));
    if (pGlobal) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pGlobal, wideLen);
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_UNICODETEXT, hGlobal);
        spdlog::info("Copied {} characters to clipboard", text.length());
    }

    CloseClipboard();

    // Clear selection after copy
    ClearSelection();
}

void Application::PasteFromClipboard() {
    Pty::ConPtySession* terminal = GetActiveTerminal();
    if (!terminal || !terminal->IsRunning() || !m_window) {
        return;
    }

    HWND hwnd = m_window->GetHandle();

    if (!OpenClipboard(hwnd)) {
        spdlog::error("Failed to open clipboard");
        return;
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return;
    }

    const wchar_t* pData = static_cast<const wchar_t*>(GlobalLock(hData));
    if (pData) {
        // Convert wide string to UTF-8
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string utf8(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pData, -1, &utf8[0], utf8Len, nullptr, nullptr);

            // Remove null terminator
            if (!utf8.empty() && utf8.back() == '\0') {
                utf8.pop_back();
            }

            // Send to terminal
            terminal->WriteInput(utf8.c_str(), utf8.length());
            spdlog::info("Pasted {} characters from clipboard", utf8.length());
        }
        GlobalUnlock(hData);
    }

    CloseClipboard();
}

bool Application::ShouldReportMouse() const {
    Terminal::VTStateMachine* vtParser = const_cast<Application*>(this)->GetActiveVTParser();
    return vtParser && vtParser->IsMouseReportingEnabled();
}

void Application::SendMouseEvent(int x, int y, int button, bool pressed, bool motion) {
    Pty::ConPtySession* terminal = GetActiveTerminal();
    Terminal::VTStateMachine* vtParser = GetActiveVTParser();
    if (!terminal || !terminal->IsRunning() || !vtParser) {
        return;
    }

    // Convert to 1-based coordinates for terminal
    int col = x + 1;
    int row = y + 1;

    // Build mouse event sequence
    char buf[64];
    int len = 0;

    if (vtParser->IsSGRMouseModeEnabled()) {
        // SGR Extended Mouse Mode (1006): CSI < Cb ; Cx ; Cy M/m
        // Button encoding: 0=left, 1=middle, 2=right, 32+button for motion
        int cb = button;
        if (motion) cb += 32;

        char terminator = pressed ? 'M' : 'm';
        len = snprintf(buf, sizeof(buf), "[<%d;%d;%d%c", cb, col, row, terminator);
    } else {
        // X10/Normal Mouse Mode: CSI M Cb Cx Cy (encoded as Cb+32, Cx+32, Cy+32)
        // Only for press events in X10 mode
        if (!pressed && vtParser->GetMouseMode() == Terminal::VTStateMachine::MouseMode::X10) {
            return;
        }

        int cb = button;
        if (motion) cb += 32;
        if (!pressed) cb += 3;  // Release is encoded as button 3

        // Coordinates are encoded with +32 offset, clamped to 223 (255-32)
        int cx = std::min(col + 32, 255);
        int cy = std::min(row + 32, 255);
        cb += 32;

        len = snprintf(buf, sizeof(buf), "[M%c%c%c", cb, cx, cy);
    }

    if (len > 0 && len < (int)sizeof(buf)) {
        terminal->WriteInput(buf, len);
    }
}

bool Application::IsWordChar(char32_t ch) const {
    // Word characters: alphanumeric, underscore, and some extended chars
    if (ch >= U'a' && ch <= U'z') return true;
    if (ch >= U'A' && ch <= U'Z') return true;
    if (ch >= U'0' && ch <= U'9') return true;
    if (ch == U'_' || ch == U'-') return true;
    // Allow URL-like characters for URL selection
    if (ch == U'/' || ch == U':' || ch == U'.' || ch == U'@' ||
        ch == U'?' || ch == U'=' || ch == U'&' || ch == U'%' ||
        ch == U'+' || ch == U'#' || ch == U'~') return true;
    return false;
}

void Application::SelectWord(int cellX, int cellY) {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!screenBuffer) return;

    int cols = screenBuffer->GetCols();

    // Get character at click position
    const auto& cell = screenBuffer->GetCellWithScrollback(cellX, cellY);
    
    // If clicking on whitespace, don't select anything
    if (cell.ch == U' ' || cell.ch == 0) {
        ClearSelection();
        return;
    }

    // Find word boundaries
    int startX = cellX;
    int endX = cellX;

    // Scan left
    while (startX > 0) {
        const auto& prevCell = screenBuffer->GetCellWithScrollback(startX - 1, cellY);
        if (!IsWordChar(prevCell.ch)) break;
        startX--;
    }

    // Scan right
    while (endX < cols - 1) {
        const auto& nextCell = screenBuffer->GetCellWithScrollback(endX + 1, cellY);
        if (!IsWordChar(nextCell.ch)) break;
        endX++;
    }

    // Set selection
    m_selectionStart = {startX, cellY};
    m_selectionEnd = {endX, cellY};
    m_hasSelection = (startX != endX);
}

void Application::SelectLine(int cellY) {
    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!screenBuffer) return;

    int cols = screenBuffer->GetCols();

    // Select entire line
    m_selectionStart = {0, cellY};
    m_selectionEnd = {cols - 1, cellY};
    m_hasSelection = true;
}

void Application::ShowContextMenu(int x, int y) {
    if (!m_window) return;

    HWND hwnd = m_window->GetHandle();
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Menu item IDs
    constexpr UINT ID_COPY = 1;
    constexpr UINT ID_PASTE = 2;
    constexpr UINT ID_SELECT_ALL = 3;

    // Add menu items
    UINT copyFlags = MF_STRING | (m_hasSelection ? 0 : MF_GRAYED);
    AppendMenuW(hMenu, copyFlags, ID_COPY, L"Copy	Ctrl+C");

    // Check if clipboard has text
    bool hasClipboardText = false;
    if (OpenClipboard(hwnd)) {
        hasClipboardText = (GetClipboardData(CF_UNICODETEXT) != nullptr);
        CloseClipboard();
    }
    UINT pasteFlags = MF_STRING | (hasClipboardText ? 0 : MF_GRAYED);
    AppendMenuW(hMenu, pasteFlags, ID_PASTE, L"Paste	Ctrl+V");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_SELECT_ALL, L"Select All");

    // Get screen coordinates
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);

    // Show menu and get selection
    UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(hMenu);

    // Handle selection
    switch (cmd) {
        case ID_COPY:
            CopySelectionToClipboard();
            break;
        case ID_PASTE:
            PasteFromClipboard();
            break;
        case ID_SELECT_ALL:
            {
                Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
                if (screenBuffer) {
                    m_selectionStart = {0, 0};
                    m_selectionEnd = {screenBuffer->GetCols() - 1, screenBuffer->GetRows() - 1};
                    m_hasSelection = true;
                }
            }
            break;
    }
}

// Search functionality
void Application::OpenSearch() {
    m_searchMode = true;
    m_searchQuery.clear();
    m_searchMatches.clear();
    m_currentMatchIndex = -1;
    spdlog::info("Search mode opened");
}

void Application::CloseSearch() {
    m_searchMode = false;
    m_searchQuery.clear();
    m_searchMatches.clear();
    m_currentMatchIndex = -1;
    spdlog::info("Search mode closed");
}

void Application::UpdateSearchResults() {
    m_searchMatches.clear();
    m_currentMatchIndex = -1;

    Terminal::ScreenBuffer* screenBuffer = GetActiveScreenBuffer();
    if (!screenBuffer || m_searchQuery.empty()) {
        return;
    }

    // Convert search query to lowercase for case-insensitive search
    std::wstring queryLower = m_searchQuery;
    for (auto& ch : queryLower) {
        ch = towlower(ch);
    }

    int rows = screenBuffer->GetRows();
    int cols = screenBuffer->GetCols();
    int queryLen = static_cast<int>(queryLower.length());

    // Search through visible area and scrollback
    // For now, just search visible area
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x <= cols - queryLen; ++x) {
            bool match = true;
            for (int i = 0; i < queryLen && match; ++i) {
                const auto& cell = screenBuffer->GetCellWithScrollback(x + i, y);
                wchar_t cellChar = static_cast<wchar_t>(cell.ch);
                if (towlower(cellChar) != queryLower[i]) {
                    match = false;
                }
            }
            if (match) {
                m_searchMatches.push_back({x, y});
            }
        }
    }

    // If we found matches, select the first one
    if (!m_searchMatches.empty()) {
        m_currentMatchIndex = 0;
    }

    spdlog::info("Search found {} matches", m_searchMatches.size());
}

void Application::SearchNext() {
    if (m_searchMatches.empty()) {
        return;
    }

    m_currentMatchIndex = (m_currentMatchIndex + 1) % static_cast<int>(m_searchMatches.size());
    spdlog::debug("Search: next match {} of {}", m_currentMatchIndex + 1, m_searchMatches.size());
}

void Application::SearchPrevious() {
    if (m_searchMatches.empty()) {
        return;
    }

    m_currentMatchIndex--;
    if (m_currentMatchIndex < 0) {
        m_currentMatchIndex = static_cast<int>(m_searchMatches.size()) - 1;
    }
    spdlog::debug("Search: previous match {} of {}", m_currentMatchIndex + 1, m_searchMatches.size());
}

// URL detection
std::string Application::DetectUrlAt(int cellX, int cellY) const {
    Terminal::ScreenBuffer* screenBuffer = const_cast<Application*>(this)->GetActiveScreenBuffer();
    if (!screenBuffer) return "";

    int cols = screenBuffer->GetCols();

    // Build the text around the clicked position
    std::string lineText;
    for (int x = 0; x < cols; ++x) {
        const auto& cell = screenBuffer->GetCellWithScrollback(x, cellY);
        if (cell.ch < 128 && cell.ch > 0) {
            lineText += static_cast<char>(cell.ch);
        } else if (cell.ch >= 128) {
            lineText += ' ';  // Replace non-ASCII with space
        } else {
            lineText += ' ';
        }
    }

    // URL prefixes to detect
    const char* prefixes[] = {"https://", "http://", "file://", "ftp://"};

    // Find all URLs in the line and check if cellX is within any of them
    for (const char* prefix : prefixes) {
        size_t pos = 0;
        while ((pos = lineText.find(prefix, pos)) != std::string::npos) {
            // Find the end of the URL
            size_t urlEnd = pos;
            while (urlEnd < lineText.length()) {
                char ch = lineText[urlEnd];
                // URL valid characters
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                    (ch >= '0' && ch <= '9') ||
                    ch == '-' || ch == '.' || ch == '_' || ch == '~' ||
                    ch == ':' || ch == '/' || ch == '?' || ch == '#' ||
                    ch == '[' || ch == ']' || ch == '@' || ch == '!' ||
                    ch == '$' || ch == '&' || ch == '\'' || ch == '(' ||
                    ch == ')' || ch == '*' || ch == '+' || ch == ',' ||
                    ch == ';' || ch == '=' || ch == '%') {
                    urlEnd++;
                } else {
                    break;
                }
            }

            // Check if the clicked position is within this URL
            if (cellX >= static_cast<int>(pos) && cellX < static_cast<int>(urlEnd)) {
                return lineText.substr(pos, urlEnd - pos);
            }

            pos = urlEnd;
        }
    }

    return "";
}

void Application::OpenUrl(const std::string& url) {
    if (url.empty()) return;

    // Convert to wide string for ShellExecute
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return;

    std::wstring wideUrl(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wideUrl[0], wideLen);

    // Open URL in default browser
    HINSTANCE result = ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) > 32) {
        spdlog::info("Opened URL: {}", url);
    } else {
        spdlog::error("Failed to open URL: {}", url);
    }
}

void Application::ShowSettings() {
    if (!m_window || !m_config) {
        return;
    }

    UI::SettingsDialog dialog(m_window->GetHandle(), m_config.get());

    // Set preview callback to trigger re-render
    dialog.SetPreviewCallback([this]() {
        // Re-render with new settings
    });

    bool changed = dialog.Show();
    if (changed) {
        spdlog::info("Settings were changed and saved");
    }
}

} // namespace Core
} // namespace TerminalDX12
