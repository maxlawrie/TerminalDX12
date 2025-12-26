/**
 * @file Application.h
 * @brief Main application class for TerminalDX12
 *
 * This file contains the Application class which coordinates all terminal
 * components including windowing, rendering, terminal emulation, and input handling.
 */

#pragma once

#include <memory>
#include <string>
#include <chrono>
#include <vector>
#include <Windows.h>

namespace TerminalDX12 {

// Forward declarations
namespace Core {
    class Window;
    class Config;
}
namespace Rendering { class DX12Renderer; }
namespace Pty { class ConPtySession; }
namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}
namespace UI {
    class TabManager;
    class Tab;
    class Pane;
    enum class SplitDirection;
}

namespace Core {

/**
 * @class Application
 * @brief Main application class managing the terminal emulator lifecycle
 *
 * Application is the central coordinator for TerminalDX12. It manages:
 * - Window creation and event handling
 * - DirectX 12 rendering pipeline
 * - Terminal emulation via ConPTY
 * - Text selection and clipboard operations
 * - Mouse and keyboard input processing
 * - Tab management for multiple terminal sessions
 *
 * @note This class follows a singleton pattern accessible via Get()
 *
 * @example
 * @code
 * Core::Application app;
 * if (!app.Initialize(L"powershell.exe")) {
 *     return EXIT_FAILURE;
 * }
 * return app.Run();
 * @endcode
 */
class Application {
public:
    Application();
    ~Application();

    /**
     * @brief Initialize the terminal application
     * @param shell Path to the shell executable (default: powershell.exe)
     * @return true if initialization succeeded, false on error
     */
    [[nodiscard]] bool Initialize(const std::wstring& shell = L"powershell.exe");

    /**
     * @brief Run the main application loop
     * @return Exit code (0 for success)
     */
    int Run();

    /**
     * @brief Clean up resources and prepare for exit
     */
    void Shutdown();

    static Application& Get() { return *s_instance; }
    Window* GetWindow() { return m_window.get(); }

private:
    void OnWindowResize(int width, int height);
    void OnWindowClose();
    void OnTerminalOutput(const char* data, size_t size);
    void OnChar(wchar_t ch);
    void OnKey(UINT key, bool isDown);
    void SendWin32InputKey(UINT vk, wchar_t unicodeChar, bool keyDown, DWORD controlState);
    void OnMouseWheel(int delta);
    void OnMouseButton(int x, int y, int button, bool down);
    void OnMouseMove(int x, int y);
    bool ProcessMessages();
    void Update(float deltaTime);
    void Render();

    // Selection helpers
    struct CellPos { int x, y; };
    CellPos ScreenToCell(int pixelX, int pixelY) const;
    std::string GetSelectedText() const;
    void CopySelectionToClipboard();
    void PasteFromClipboard();
    std::string GetClipboardText();          // For OSC 52 read
    void SetClipboardText(const std::string& text);  // For OSC 52 write
    void ClearSelection();

    // Mouse reporting helpers
    void SendMouseEvent(int x, int y, int button, bool pressed, bool motion);
    bool ShouldReportMouse() const;

    // Word/line selection helpers
    void SelectWord(int cellX, int cellY);
    void SelectLine(int cellY);
    bool IsWordChar(char32_t ch) const;

    // Context menu
    void ShowContextMenu(int x, int y);

    // Search functionality
    void OpenSearch();
    void CloseSearch();
    void SearchNext();
    void SearchPrevious();
    void UpdateSearchResults();

    // URL detection
    std::string DetectUrlAt(int cellX, int cellY) const;
    void OpenUrl(const std::string& url);

    // Terminal layout - consistent startY calculation
    int GetTerminalStartY() const;

    // Settings dialog
    void ShowSettings();

    // Pane management
    void SplitPane(UI::SplitDirection direction);
    void ClosePane();
    void FocusNextPane();
    void FocusPreviousPane();
    void FocusPaneInDirection(UI::SplitDirection direction);
    void RenderPane(UI::Pane* pane, int windowWidth, int windowHeight);
    void UpdatePaneLayout();

    std::unique_ptr<Config> m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Rendering::DX12Renderer> m_renderer;
    std::unique_ptr<UI::TabManager> m_tabManager;

    // Pane tree (root pane contains all panes for current tab)
    std::unique_ptr<UI::Pane> m_rootPane;
    UI::Pane* m_focusedPane = nullptr;
    bool m_paneZoomed = false;  // Is focused pane maximized

    // Helper accessors for active tab's components
    Terminal::ScreenBuffer* GetActiveScreenBuffer();
    Terminal::VTStateMachine* GetActiveVTParser();
    Pty::ConPtySession* GetActiveTerminal();

    bool m_running;
    bool m_minimized;
    std::wstring m_shellCommand;

    // Pending resize - defer to start of next frame
    bool m_pendingResize = false;
    bool m_pendingConPTYResize = false;  // Resize ConPTY after DX12 resize completes
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;

    // Selection state
    CellPos m_selectionStart;
    CellPos m_selectionEnd;
    bool m_selecting;         // Currently dragging to select
    bool m_hasSelection;      // Valid selection exists
    bool m_rectangleSelection = false;  // Alt+drag rectangle selection mode

    // Mouse click tracking for double/triple click
    std::chrono::steady_clock::time_point m_lastClickTime;
    CellPos m_lastClickPos;
    int m_clickCount;              // 1=single, 2=double, 3=triple
    int m_lastMouseButton;         // Last button pressed (for mouse reporting)
    bool m_mouseButtonDown;        // Is a button currently pressed

    // Search state
    bool m_searchMode = false;         // Search bar is visible
    std::wstring m_searchQuery;        // Current search text
    std::vector<CellPos> m_searchMatches;  // Found match positions
    int m_currentMatchIndex = -1;      // Currently highlighted match (-1 = none)

    static Application* s_instance;
};

} // namespace Core
} // namespace TerminalDX12
