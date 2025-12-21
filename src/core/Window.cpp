#include "core/Window.h"
#include <windowsx.h>

namespace TerminalDX12 {
namespace Core {

Window::Window()
    : m_hwnd(nullptr)
    , m_width(0)
    , m_height(0)
    , m_dpiScale(1.0f)
    , m_isResizing(false)
{
}

Window::~Window() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

bool Window::Create(const WindowDesc& desc) {
    // Enable per-monitor DPI awareness V2
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProcStatic;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    // Calculate window size with DPI
    UpdateDPI();

    int scaledWidth = static_cast<int>(desc.width * m_dpiScale);
    int scaledHeight = static_cast<int>(desc.height * m_dpiScale);

    // Window style
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!desc.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    // Adjust window rect to account for borders
    RECT rect = {0, 0, scaledWidth, scaledHeight};
    AdjustWindowRect(&rect, style, FALSE);

    // Create window
    m_hwnd = CreateWindowExW(
        0,
        WINDOW_CLASS_NAME,
        desc.title.c_str(),
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left,
        rect.bottom - rect.top,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        this // Pass this pointer for WM_CREATE
    );

    if (!m_hwnd) {
        return false;
    }

    m_width = desc.width;
    m_height = desc.height;

    return true;
}

void Window::Show() {
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
}

void Window::Hide() {
    ShowWindow(m_hwnd, SW_HIDE);
}

void Window::Close() {
    PostMessage(m_hwnd, WM_CLOSE, 0, 0);
}

void Window::SetTitle(const std::wstring& title) {
    SetWindowTextW(m_hwnd, title.c_str());
}

void Window::SetOpacity(float opacity) {
    // Clamp to valid range
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;

    // Add WS_EX_LAYERED style if not already set
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (!(exStyle & WS_EX_LAYERED)) {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
    }

    // Set the window opacity
    BYTE alpha = static_cast<BYTE>(opacity * 255.0f);
    SetLayeredWindowAttributes(m_hwnd, 0, alpha, LWA_ALPHA);
}

void Window::UpdateDPI() {
    HDC hdc = GetDC(nullptr);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    m_dpiScale = dpi / 96.0f;
}

void Window::HandleResize(int width, int height) {
    if (width == m_width && height == m_height) {
        return;
    }

    m_width = width;
    m_height = height;

    if (OnResize) {
        OnResize(width, height);
    }
}

LRESULT CALLBACK Window::WindowProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Window* window = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<Window*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->WindowProc(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Window::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            if (OnClose) {
                OnClose();
            }
            // Don't call DestroyWindow here - let Application::Shutdown() handle
            // cleanup in the correct order (DX12 resources before window)
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);

                if (wParam == SIZE_MINIMIZED) {
                    // Don't resize when minimized
                } else {
                    HandleResize(width, height);
                }
            }
            return 0;

        case WM_ENTERSIZEMOVE:
            m_isResizing = true;
            return 0;

        case WM_EXITSIZEMOVE:
            m_isResizing = false;
            // Trigger final resize
            RECT rect;
            if (GetClientRect(hwnd, &rect)) {
                HandleResize(rect.right - rect.left, rect.bottom - rect.top);
            }
            return 0;

        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);

                if (OnPaint) {
                    OnPaint();
                }

                EndPaint(hwnd, &ps);
            }
            return 0;

        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (OnKeyEvent) {
                OnKeyEvent(static_cast<UINT>(wParam), true);
            }
            return 0;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (OnKeyEvent) {
                OnKeyEvent(static_cast<UINT>(wParam), false);
            }
            return 0;

        case WM_CHAR:
            if (OnCharEvent) {
                OnCharEvent(static_cast<wchar_t>(wParam));
            }
            return 0;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (OnMouseButton) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int button = (msg == WM_LBUTTONDOWN) ? 0 : (msg == WM_RBUTTONDOWN) ? 1 : 2;
                OnMouseButton(x, y, button, true);
            }
            return 0;

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            if (OnMouseButton) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                int button = (msg == WM_LBUTTONUP) ? 0 : (msg == WM_RBUTTONUP) ? 1 : 2;
                OnMouseButton(x, y, button, false);
            }
            return 0;

        case WM_MOUSEMOVE:
            if (OnMouseMove) {
                int x = GET_X_LPARAM(lParam);
                int y = GET_Y_LPARAM(lParam);
                OnMouseMove(x, y);
            }
            return 0;

        case WM_MOUSEWHEEL:
            if (OnMouseWheel) {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                OnMouseWheel(delta);
            }
            return 0;

        case WM_DPICHANGED:
            {
                UpdateDPI();

                // Windows suggests a new size for the window
                RECT* rect = reinterpret_cast<RECT*>(lParam);
                SetWindowPos(hwnd, nullptr,
                    rect->left, rect->top,
                    rect->right - rect->left,
                    rect->bottom - rect->top,
                    SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace Core
} // namespace TerminalDX12
