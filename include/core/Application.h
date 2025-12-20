#pragma once

#include <memory>
#include <Windows.h>

namespace TerminalDX12 {

// Forward declarations
namespace Core { class Window; }
namespace Rendering { class DX12Renderer; }
namespace Pty { class ConPtySession; }
namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}

namespace Core {

class Application {
public:
    Application();
    ~Application();

    bool Initialize();
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
    void OnMouseWheel(int delta);
    bool ProcessMessages();
    void Update(float deltaTime);
    void Render();

    std::unique_ptr<Window> m_window;
    std::unique_ptr<Rendering::DX12Renderer> m_renderer;
    std::unique_ptr<Pty::ConPtySession> m_terminal;
    std::unique_ptr<Terminal::ScreenBuffer> m_screenBuffer;
    std::unique_ptr<Terminal::VTStateMachine> m_vtParser;

    bool m_running;
    bool m_minimized;

    static Application* s_instance;
};

} // namespace Core
} // namespace TerminalDX12
