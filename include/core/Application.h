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
#include <utility>
#include <Windows.h>
#include "ui/SearchManager.h"
#include "ui/SelectionManager.h"
#include "ui/InputHandler.h"
#include "ui/MouseHandler.h"

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
    class TerminalSession;
    class SearchManager;
    class SelectionManager;
    class PaneManager;
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
    // Layout constants
    static constexpr int kCharWidth = 10;
    static constexpr int kLineHeight = 25;
    static constexpr int kStartX = 10;
    static constexpr int kPadding = 10;
    static constexpr int kTabBarHeight = 30;

    void OnWindowResize(int width, int height);
    bool ProcessMessages();
    void Render();

    // Render helpers
    void BuildColorPalette(float palette[256][3], Terminal::ScreenBuffer* screenBuffer);
    void RenderTabBar(int charWidth);
    void RenderTerminalContent(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                               int charWidth, int lineHeight, const float palette[256][3],
                               void* pane = nullptr);
    void RenderCursor(Terminal::ScreenBuffer* screenBuffer, int startX, int startY,
                      int charWidth, int lineHeight, float cursorR, float cursorG, float cursorB);
    void RenderSearchBar(int startX, int charWidth, int lineHeight);

    // Selection helpers
    struct CellPos { int x, y; };
    CellPos ScreenToCell(int pixelX, int pixelY) const;
    void CopySelectionToClipboard();
    void PasteFromClipboard();

    // Terminal layout helpers
    int GetTerminalStartY() const;
    std::pair<int, int> CalculateTerminalSize(int width, int height) const;

    // Settings dialog
    void ShowSettings();

    // Pane management (delegates to active tab's PaneManager)
    UI::PaneManager* GetActivePaneManager();
    void SplitPane(UI::SplitDirection direction);
    void ClosePane();
    void UpdatePaneLayout();
    void ResizeAllPaneBuffers();
    void RenderPaneDividers(UI::Pane* pane);

    std::unique_ptr<Config> m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Rendering::DX12Renderer> m_renderer;
    std::unique_ptr<UI::TabManager> m_tabManager;

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
    int m_resizeStabilizeFrames = 0;  // Skip frames after resize to let TUI stabilize

    // Selection manager
    UI::SelectionManager m_selectionManager;

    // Search manager
    UI::SearchManager m_searchManager;

    // Input handler
    std::unique_ptr<UI::InputHandler> m_inputHandler;

    // Mouse handler
    std::unique_ptr<UI::MouseHandler> m_mouseHandler;

    static Application* s_instance;
};

} // namespace Core
} // namespace TerminalDX12
