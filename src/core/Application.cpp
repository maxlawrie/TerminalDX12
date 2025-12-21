#include "core/Application.h"
#include "core/Config.h"
#include "core/Window.h"
#include "rendering/DX12Renderer.h"
#include "pty/ConPtySession.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <chrono>
#include <spdlog/spdlog.h>
#include <string>
#include <locale>
#include <codecvt>

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
    windowDesc.width = 1280;
    windowDesc.height = 720;
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

    // Create terminal session
    m_terminal = std::make_unique<Pty::ConPtySession>();

    // Set output callback
    m_terminal->SetOutputCallback([this](const char* data, size_t size) {
        OnTerminalOutput(data, size);
    });

    // Calculate terminal size (80x24 columns/rows for now - will be dynamic later)
    int termCols = 80;
    int termRows = 24;

    // Create screen buffer with configured scrollback
    int scrollbackLines = m_config->GetTerminal().scrollbackLines;
    m_screenBuffer = std::make_unique<Terminal::ScreenBuffer>(termCols, termRows, scrollbackLines);

    // Create VT parser
    m_vtParser = std::make_unique<Terminal::VTStateMachine>(m_screenBuffer.get());

    // Wire up response callback for device queries (CPR, DA, etc.)
    m_vtParser->SetResponseCallback([this](const std::string& response) {
        if (m_terminal && m_terminal->IsRunning()) {
            m_terminal->WriteInput(response.c_str(), response.size());
        }
    });

    // Start PowerShell (make this optional - continue even if it fails)
    if (!m_terminal->Start(m_shellCommand, termCols, termRows)) {
        spdlog::warn("Failed to start terminal session - continuing without ConPTY");
        m_terminal.reset();  // Clear the terminal if it failed to start
    } else {
        spdlog::info("Terminal session started successfully");
    }

    m_window->Show();
    m_running = true;

    return true;
}

void Application::Shutdown() {
    if (m_terminal) {
        m_terminal->Stop();
        m_terminal.reset();
    }

    if (m_renderer) {
        m_renderer->Shutdown();
        m_renderer.reset();
    }

    m_window.reset();
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
    if (!m_renderer || !m_screenBuffer) {
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
    const int startY = 10;

    int rows = m_screenBuffer->GetRows();
    int cols = m_screenBuffer->GetCols();

    // Get cursor position for rendering
    int cursorX, cursorY;
    m_screenBuffer->GetCursorPos(cursorX, cursorY);
    bool cursorVisible = m_screenBuffer->IsCursorVisible();

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
            const auto& cell = m_screenBuffer->GetCellWithScrollback(x, y);
            uint8_t bgIndex = cell.attr.IsInverse() ? cell.attr.foreground % 16 : cell.attr.background % 16;

            // Skip default background (black) unless inverse
            if (bgIndex == 0 && !cell.attr.IsInverse()) {
                x++;
                continue;
            }

            // Find run of same background color
            int runStart = x;
            int runLength = 1;

            while (x + runLength < cols) {
                const auto& nextCell = m_screenBuffer->GetCellWithScrollback(x + runLength, y);
                uint8_t nextBg = nextCell.attr.IsInverse() ? nextCell.attr.foreground % 16 : nextCell.attr.background % 16;

                if (nextBg != bgIndex) {
                    break;
                }
                runLength++;
            }

            // Render background using space characters (simpler than block chars)
            std::string spaces(runLength, ' ');
            const float* bgColor = colorPalette[bgIndex];
            float posX = static_cast<float>(startX + runStart * charWidth);
            float posY = static_cast<float>(startY + y * lineHeight);

            // Render with background color - we'll use inverse rendering
            // Render colored blocks behind text
            for (int i = 0; i < runLength; i++) {
                m_renderer->RenderText("\xE2\x96\x88", posX + i * charWidth, posY, bgColor[0], bgColor[1], bgColor[2], 1.0f);
            }

            x += runLength;
        }

        // Second pass: Render foreground text character by character for precise grid alignment
        for (int x = 0; x < cols; ++x) {
            const auto& cell = m_screenBuffer->GetCellWithScrollback(x, y);

            // Skip spaces
            if (cell.ch == U' ' || cell.ch == U'\0') {
                continue;
            }

            // Get color from palette
            uint8_t fgIndex = cell.attr.foreground % 16;
            const float* fgColor = colorPalette[fgIndex];

            // Handle inverse attribute
            float r, g, b;
            if (cell.attr.IsInverse()) {
                uint8_t bgIndex = cell.attr.background % 16;
                const float* bgColor = colorPalette[bgIndex];
                r = bgColor[0];
                g = bgColor[1];
                b = bgColor[2];
            } else {
                r = fgColor[0];
                g = fgColor[1];
                b = fgColor[2];
            }

            // Apply bold by brightening colors
            if (cell.attr.IsBold() && fgIndex < 8) {
                r = std::min(1.0f, r + 0.2f);
                g = std::min(1.0f, g + 0.2f);
                b = std::min(1.0f, b + 0.2f);
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
    int scrollOffset = m_screenBuffer->GetScrollOffset();
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
    if (!m_vtParser) {
        return;
    }

    // Parse VT100/ANSI sequences and update screen buffer
    m_vtParser->ProcessInput(data, size);
}

void Application::OnChar(wchar_t ch) {
    if (!m_terminal || !m_terminal->IsRunning()) {
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
        m_terminal->WriteInput(utf8, len);
    }
}

// Helper to get scan code for a virtual key
static WORD GetScanCodeForVK(UINT vk) {
    return static_cast<WORD>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
}

// Send key in Win32 input mode format: ESC [ Vk ; Sc ; Uc ; Kd ; Cs ; Rc _
void Application::SendWin32InputKey(UINT vk, wchar_t unicodeChar, bool keyDown, DWORD controlState) {
    if (!m_terminal || !m_terminal->IsRunning()) {
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
        m_terminal->WriteInput(buf, len);
    }
}

void Application::OnKey(UINT key, bool isDown) {
    if (!isDown) {
        return;
    }

    // Handle Ctrl+C/V for copy/paste
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
        if (key == 'C') {
            if (m_hasSelection) {
                CopySelectionToClipboard();
            } else if (m_terminal && m_terminal->IsRunning()) {
                // Send SIGINT (Ctrl+C) to terminal
                m_terminal->WriteInput("\x03", 1);
            }
            return;
        }
        if (key == 'V') {
            PasteFromClipboard();
            return;
        }
    }

    if (!m_terminal || !m_terminal->IsRunning()) {
        return;
    }

    // Handle scrollback navigation
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
        // Shift is pressed - handle scrollback
        if (key == VK_PRIOR) {  // Shift+Page Up
            int currentOffset = m_screenBuffer->GetScrollOffset();
            m_screenBuffer->SetScrollOffset(currentOffset + m_screenBuffer->GetRows());
            return;
        } else if (key == VK_NEXT) {  // Shift+Page Down
            int currentOffset = m_screenBuffer->GetScrollOffset();
            m_screenBuffer->SetScrollOffset(std::max(0, currentOffset - m_screenBuffer->GetRows()));
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
            SendWin32InputKey(VK_BACK, L'\b', true, controlState);
            return;
        case VK_RETURN:
            SendWin32InputKey(VK_RETURN, L'\r', true, controlState);
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
    if (!m_screenBuffer) {
        return;
    }

    // Mouse wheel delta is in units of WHEEL_DELTA (120)
    // Positive delta = scroll up (view older content)
    // Negative delta = scroll down (view newer content)

    // Scroll by 3 lines per wheel notch
    const int linesPerNotch = 3;
    int scrollLines = (delta / WHEEL_DELTA) * linesPerNotch;

    int currentOffset = m_screenBuffer->GetScrollOffset();
    int newOffset = currentOffset + scrollLines;

    // Clamp to valid range
    newOffset = std::max(0, newOffset);

    m_screenBuffer->SetScrollOffset(newOffset);
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
    if (m_screenBuffer) {
        pos.x = std::max(0, std::min(pos.x, m_screenBuffer->GetCols() - 1));
        pos.y = std::max(0, std::min(pos.y, m_screenBuffer->GetRows() - 1));
    }

    return pos;
}

void Application::OnMouseButton(int x, int y, int button, bool down) {
    CellPos cellPos = ScreenToCell(x, y);

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
    if (ShouldReportMouse() && m_vtParser) {
        auto mouseMode = m_vtParser->GetMouseMode();
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
    if (!m_hasSelection || !m_screenBuffer) {
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
    int cols = m_screenBuffer->GetCols();

    for (int y = startY; y <= endY; ++y) {
        int rowStartX = (y == startY) ? startX : 0;
        int rowEndX = (y == endY) ? endX : cols - 1;

        for (int x = rowStartX; x <= rowEndX; ++x) {
            const auto& cell = m_screenBuffer->GetCellWithScrollback(x, y);
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
    if (!m_terminal || !m_terminal->IsRunning() || !m_window) {
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
            m_terminal->WriteInput(utf8.c_str(), utf8.length());
            spdlog::info("Pasted {} characters from clipboard", utf8.length());
        }
        GlobalUnlock(hData);
    }

    CloseClipboard();
}

bool Application::ShouldReportMouse() const {
    return m_vtParser && m_vtParser->IsMouseReportingEnabled();
}

void Application::SendMouseEvent(int x, int y, int button, bool pressed, bool motion) {
    if (!m_terminal || !m_terminal->IsRunning() || !m_vtParser) {
        return;
    }

    // Convert to 1-based coordinates for terminal
    int col = x + 1;
    int row = y + 1;

    // Build mouse event sequence
    char buf[64];
    int len = 0;

    if (m_vtParser->IsSGRMouseModeEnabled()) {
        // SGR Extended Mouse Mode (1006): CSI < Cb ; Cx ; Cy M/m
        // Button encoding: 0=left, 1=middle, 2=right, 32+button for motion
        int cb = button;
        if (motion) cb += 32;

        char terminator = pressed ? 'M' : 'm';
        len = snprintf(buf, sizeof(buf), "[<%d;%d;%d%c", cb, col, row, terminator);
    } else {
        // X10/Normal Mouse Mode: CSI M Cb Cx Cy (encoded as Cb+32, Cx+32, Cy+32)
        // Only for press events in X10 mode
        if (!pressed && m_vtParser->GetMouseMode() == Terminal::VTStateMachine::MouseMode::X10) {
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
        m_terminal->WriteInput(buf, len);
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
    if (!m_screenBuffer) return;

    int cols = m_screenBuffer->GetCols();

    // Get character at click position
    const auto& cell = m_screenBuffer->GetCellWithScrollback(cellX, cellY);
    
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
        const auto& prevCell = m_screenBuffer->GetCellWithScrollback(startX - 1, cellY);
        if (!IsWordChar(prevCell.ch)) break;
        startX--;
    }

    // Scan right
    while (endX < cols - 1) {
        const auto& nextCell = m_screenBuffer->GetCellWithScrollback(endX + 1, cellY);
        if (!IsWordChar(nextCell.ch)) break;
        endX++;
    }

    // Set selection
    m_selectionStart = {startX, cellY};
    m_selectionEnd = {endX, cellY};
    m_hasSelection = (startX != endX);
}

void Application::SelectLine(int cellY) {
    if (!m_screenBuffer) return;

    int cols = m_screenBuffer->GetCols();

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
            if (m_screenBuffer) {
                m_selectionStart = {0, 0};
                m_selectionEnd = {m_screenBuffer->GetCols() - 1, m_screenBuffer->GetRows() - 1};
                m_hasSelection = true;
            }
            break;
    }
}

} // namespace Core
} // namespace TerminalDX12
