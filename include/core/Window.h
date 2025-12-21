#pragma once

#include <Windows.h>
#include <functional>
#include <string>

namespace TerminalDX12 {
namespace Core {

struct WindowDesc {
    int width = 1280;
    int height = 720;
    std::wstring title = L"TerminalDX12";
    bool resizable = true;
};

class Window {
public:
    Window();
    ~Window();

    bool Create(const WindowDesc& desc);
    void Show();
    void Hide();
    void Close();

    // Getters
    HWND GetHandle() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    float GetDPIScale() const { return m_dpiScale; }

    // Setters
    void SetTitle(const std::wstring& title);
    void SetOpacity(float opacity);  // 0.0 = transparent, 1.0 = opaque

    // Event callbacks
    std::function<void(UINT key, bool isDown)> OnKeyEvent;
    std::function<void(wchar_t ch)> OnCharEvent;
    std::function<void(int width, int height)> OnResize;
    std::function<void()> OnPaint;
    std::function<void()> OnClose;
    std::function<void(int x, int y, int button, bool down)> OnMouseButton;
    std::function<void(int x, int y)> OnMouseMove;
    std::function<void(int delta)> OnMouseWheel;

private:
    static LRESULT CALLBACK WindowProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void UpdateDPI();
    void HandleResize(int width, int height);

    HWND m_hwnd;
    int m_width;
    int m_height;
    float m_dpiScale;
    bool m_isResizing;

    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"TerminalDX12WindowClass";
};

} // namespace Core
} // namespace TerminalDX12
