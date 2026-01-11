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
#include "core/LayoutCalculator.h"
#include "core/ResizeManager.h"
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
namespace Rendering {
    class DX12Renderer;
    class RenderingPipeline;
}
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
 */
class Application {
public:
    Application();
    ~Application();

    [[nodiscard]] bool Initialize(const std::wstring& shell = L"powershell.exe");
    int Run();
    void Shutdown();

    static Application& Get() { return *s_instance; }
    Window* GetWindow() { return m_window.get(); }

private:
    // Font/settings reload
    void ReloadFontSettings();
    void UpdateFontMetrics();

    void OnWindowResize(int width, int height);
    bool ProcessMessages();
    void Render();
    void UpdateTabTooltip(int x, int y);

    // Selection helpers
    void CopySelectionToClipboard();
    void PasteFromClipboard();

    // Settings dialog
    void ShowSettings();

    // Tab and pane management
    void CreateNewTab();
    UI::PaneManager* GetActivePaneManager();
    void SplitPane(UI::SplitDirection direction);
    void ClosePane();
    void UpdatePaneLayout();
    void ResizeAllPaneBuffers();

    // Callback setup helpers (simplifies Initialize)
    void SetupWindowCallbacks();
    void SetupTabManagerCallbacks();
    UI::InputHandlerCallbacks BuildInputCallbacks();
    void SetupSelectionCallbacks();
    UI::MouseHandlerCallbacks BuildMouseCallbacks();

    std::unique_ptr<Config> m_config;
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Rendering::DX12Renderer> m_renderer;
    std::unique_ptr<Rendering::RenderingPipeline> m_renderingPipeline;
    std::unique_ptr<UI::TabManager> m_tabManager;

    // Layout and resize management
    LayoutCalculator m_layoutCalc;
    ResizeManager m_resizeManager;

    // Helper accessors for active tab's components
    Terminal::ScreenBuffer* GetActiveScreenBuffer();
    Terminal::VTStateMachine* GetActiveVTParser();
    Pty::ConPtySession* GetActiveTerminal();

    bool m_running;
    bool m_minimized;
    std::wstring m_shellCommand;

    // Tab tooltip tracking
    int m_hoveredTabId = -1;

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
