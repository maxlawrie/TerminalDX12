/**
 * @file Window.h
 * @brief Win32 window management for TerminalDX12
 *
 * Provides a modern C++ wrapper around Win32 window creation and event handling
 * with support for DPI awareness and resize tracking.
 */

#pragma once

#include <Windows.h>
#include <functional>
#include <string>

namespace TerminalDX12 {
namespace Core {

/**
 * @struct WindowDesc
 * @brief Configuration parameters for window creation
 */
struct WindowDesc {
    int width = 1280;           ///< Initial window width in pixels
    int height = 720;           ///< Initial window height in pixels
    std::wstring title = L"TerminalDX12";  ///< Window title
    bool resizable = true;      ///< Whether the window can be resized
};

/**
 * @class Window
 * @brief Win32 window wrapper with DPI awareness and event callbacks
 *
 * Window provides a clean C++ interface for Win32 window management including:
 * - Per-monitor DPI awareness V2
 * - Event callbacks for keyboard, mouse, and window events
 * - Resize state tracking for smooth terminal resize handling
 * - Window opacity control for transparency effects
 *
 * @example
 * @code
 * WindowDesc desc;
 * desc.width = 1920;
 * desc.height = 1080;
 * desc.title = L"My Terminal";
 *
 * Window window;
 * if (window.Create(desc)) {
 *     window.OnResize = [](int w, int h) { HandleResize(w, h); };
 *     window.Show();
 * }
 * @endcode
 */
class Window {
public:
    Window();
    ~Window();

    /**
     * @brief Create the window with specified parameters
     * @param desc Window configuration
     * @return true if window was created successfully
     */
    bool Create(const WindowDesc& desc);

    /** @brief Show the window */
    void Show();

    /** @brief Hide the window */
    void Hide();

    /** @brief Close and destroy the window */
    void Close();

    // Getters
    HWND GetHandle() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    float GetDPIScale() const { return m_dpiScale; }
    bool IsResizing() const { return m_isResizing; }

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
