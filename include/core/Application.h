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
}

namespace Core {

class Application {
public:
    Application();
    ~Application();

    bool Initialize(const std::wstring& shell = L"powershell.exe");
    int Run();
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

    // Selection state
    CellPos m_selectionStart;
    CellPos m_selectionEnd;
    bool m_selecting;      // Currently dragging to select
    bool m_hasSelection;   // Valid selection exists

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
