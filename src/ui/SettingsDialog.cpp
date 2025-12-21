#include "ui/SettingsDialog.h"
#include "core/Config.h"
#include <spdlog/spdlog.h>
#include <CommCtrl.h>
#include <windowsx.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

namespace TerminalDX12::UI {

// Control IDs
enum {
    IDC_TAB_CONTROL = 1001,
    // Font tab
    IDC_FONT_NAME = 1010,
    IDC_FONT_SIZE = 1011,
    IDC_FONT_BOLD = 1012,
    // Colors tab
    IDC_COLOR_FOREGROUND = 1020,
    IDC_COLOR_BACKGROUND = 1021,
    IDC_COLOR_CURSOR = 1022,
    // Terminal tab
    IDC_SCROLLBACK_LINES = 1030,
    IDC_SHELL_PATH = 1031,
    IDC_CURSOR_BLINK = 1032,
    IDC_OPACITY = 1033,
    // Buttons
    IDC_APPLY = 1100,
    IDC_CANCEL = 1101,
    IDC_OK = 1102
};

// Dialog dimensions
constexpr int DIALOG_WIDTH = 450;
constexpr int DIALOG_HEIGHT = 350;

SettingsDialog::SettingsDialog(HWND parentWindow, Core::Config* config)
    : m_parentWindow(parentWindow)
    , m_config(config)
    , m_originalConfig(nullptr)
{
}

SettingsDialog::~SettingsDialog() {
    delete m_originalConfig;
}

INT_PTR CALLBACK SettingsDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* dialog = nullptr;

    if (msg == WM_INITDIALOG) {
        dialog = reinterpret_cast<SettingsDialog*>(lParam);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
    } else {
        dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (dialog) {
        return dialog->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return FALSE;
}

bool SettingsDialog::Show() {
    // Backup original settings for cancel
    m_originalConfig = new Core::Config();
    *m_originalConfig = *m_config;
    m_settingsChanged = false;

    // Create a simple dialog template in memory
    // This is a simplified approach - for production, use a .rc resource file

    // For now, create a simpler modeless settings window
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        SettingsDialog* dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                dialog = reinterpret_cast<SettingsDialog*>(cs->lpCreateParams);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
                dialog->InitializeControls(hwnd);
                dialog->LoadCurrentSettings(hwnd);
                return 0;
            }
            case WM_COMMAND:
                if (dialog) {
                    int id = LOWORD(wParam);
                    int notification = HIWORD(wParam);

                    switch (id) {
                        case IDC_OK:
                            dialog->ApplySettings(hwnd);
                            dialog->m_settingsChanged = true;
                            DestroyWindow(hwnd);
                            return 0;
                        case IDC_APPLY:
                            dialog->ApplySettings(hwnd);
                            dialog->m_settingsChanged = true;
                            if (dialog->m_previewCallback) {
                                dialog->m_previewCallback();
                            }
                            return 0;
                        case IDC_CANCEL:
                            dialog->RestoreSettings();
                            DestroyWindow(hwnd);
                            return 0;
                    }

                    // Live preview on text changes
                    if (notification == EN_CHANGE) {
                        dialog->PreviewSettings(hwnd);
                    }
                }
                break;
            case WM_CLOSE:
                if (dialog) {
                    dialog->RestoreSettings();
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    };
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"TerminalDX12Settings";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassExW(&wc);

    // Center dialog on parent
    RECT parentRect;
    GetWindowRect(m_parentWindow, &parentRect);
    int x = parentRect.left + (parentRect.right - parentRect.left - DIALOG_WIDTH) / 2;
    int y = parentRect.top + (parentRect.bottom - parentRect.top - DIALOG_HEIGHT) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"TerminalDX12Settings",
        L"Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
        m_parentWindow,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!hwnd) {
        spdlog::error("Failed to create settings window");
        return false;
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    // Modal message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(hwnd)) {
            break;
        }
    }

    return m_settingsChanged;
}

void SettingsDialog::InitializeControls(HWND hwnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    int y = 10;
    int labelWidth = 120;
    int controlWidth = 200;
    int controlHeight = 22;
    int spacing = 30;

    // Font section
    CreateWindowW(L"STATIC", L"Font Settings", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  10, y, 200, 20, hwnd, nullptr, hInstance, nullptr);
    y += 25;

    CreateWindowW(L"STATIC", L"Font Name:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  labelWidth + 30, y, controlWidth, controlHeight, hwnd,
                  (HMENU)IDC_FONT_NAME, hInstance, nullptr);
    y += spacing;

    CreateWindowW(L"STATIC", L"Font Size:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  labelWidth + 30, y, 60, controlHeight, hwnd,
                  (HMENU)IDC_FONT_SIZE, hInstance, nullptr);
    y += spacing;

    // Terminal section
    y += 10;
    CreateWindowW(L"STATIC", L"Terminal Settings", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  10, y, 200, 20, hwnd, nullptr, hInstance, nullptr);
    y += 25;

    CreateWindowW(L"STATIC", L"Shell Path:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  labelWidth + 30, y, controlWidth, controlHeight, hwnd,
                  (HMENU)IDC_SHELL_PATH, hInstance, nullptr);
    y += spacing;

    CreateWindowW(L"STATIC", L"Scrollback Lines:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  labelWidth + 30, y, 80, controlHeight, hwnd,
                  (HMENU)IDC_SCROLLBACK_LINES, hInstance, nullptr);
    y += spacing;

    CreateWindowW(L"BUTTON", L"Cursor Blink", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  20, y, 150, controlHeight, hwnd,
                  (HMENU)IDC_CURSOR_BLINK, hInstance, nullptr);
    y += spacing;

    CreateWindowW(L"STATIC", L"Opacity (0-100):", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  20, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  labelWidth + 30, y, 60, controlHeight, hwnd,
                  (HMENU)IDC_OPACITY, hInstance, nullptr);
    y += spacing;

    // Buttons at bottom
    int buttonWidth = 80;
    int buttonY = DIALOG_HEIGHT - 70;
    int buttonX = DIALOG_WIDTH - buttonWidth * 3 - 50;

    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  buttonX, buttonY, buttonWidth, 28, hwnd,
                  (HMENU)IDC_OK, hInstance, nullptr);
    buttonX += buttonWidth + 10;

    CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  buttonX, buttonY, buttonWidth, 28, hwnd,
                  (HMENU)IDC_APPLY, hInstance, nullptr);
    buttonX += buttonWidth + 10;

    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  buttonX, buttonY, buttonWidth, 28, hwnd,
                  (HMENU)IDC_CANCEL, hInstance, nullptr);
}

void SettingsDialog::LoadCurrentSettings(HWND hwnd) {
    if (!m_config) return;

    const auto& font = m_config->GetFont();
    const auto& terminal = m_config->GetTerminal();

    // Font settings - convert family to wide string
    std::wstring fontFamily(font.family.begin(), font.family.end());
    SetDlgItemTextW(hwnd, IDC_FONT_NAME, fontFamily.c_str());
    SetDlgItemInt(hwnd, IDC_FONT_SIZE, font.size, FALSE);

    // Terminal settings
    SetDlgItemTextW(hwnd, IDC_SHELL_PATH, terminal.shell.c_str());
    SetDlgItemInt(hwnd, IDC_SCROLLBACK_LINES, terminal.scrollbackLines, FALSE);
    CheckDlgButton(hwnd, IDC_CURSOR_BLINK, terminal.cursorBlink ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_OPACITY, static_cast<int>(terminal.opacity * 100), FALSE);
}

void SettingsDialog::ApplySettings(HWND hwnd) {
    if (!m_config) return;

    // Read font settings
    wchar_t buffer[256];
    GetDlgItemTextW(hwnd, IDC_FONT_NAME, buffer, 256);
    std::wstring fontNameW = buffer;
    std::string fontName(fontNameW.begin(), fontNameW.end());

    int fontSize = GetDlgItemInt(hwnd, IDC_FONT_SIZE, nullptr, FALSE);
    if (fontSize < 6) fontSize = 6;
    if (fontSize > 72) fontSize = 72;

    // Read terminal settings
    GetDlgItemTextW(hwnd, IDC_SHELL_PATH, buffer, 256);
    std::wstring shellPath = buffer;

    int scrollbackLines = GetDlgItemInt(hwnd, IDC_SCROLLBACK_LINES, nullptr, FALSE);
    if (scrollbackLines < 100) scrollbackLines = 100;
    if (scrollbackLines > 1000000) scrollbackLines = 1000000;

    bool cursorBlink = IsDlgButtonChecked(hwnd, IDC_CURSOR_BLINK) == BST_CHECKED;

    int opacityPercent = GetDlgItemInt(hwnd, IDC_OPACITY, nullptr, FALSE);
    if (opacityPercent < 0) opacityPercent = 0;
    if (opacityPercent > 100) opacityPercent = 100;
    float opacity = opacityPercent / 100.0f;

    // Update config using mutable accessors
    Core::FontConfig& font = m_config->GetFontMut();
    font.family = fontName;
    font.size = fontSize;

    Core::TerminalConfig& terminal = m_config->GetTerminalMut();
    terminal.shell = shellPath;
    terminal.scrollbackLines = scrollbackLines;
    terminal.cursorBlink = cursorBlink;
    terminal.opacity = opacity;

    spdlog::info("Settings applied - Font: {} {}pt, Shell: {}, Scrollback: {}, Blink: {}, Opacity: {}%",
                 fontName, fontSize,
                 std::string(shellPath.begin(), shellPath.end()),
                 scrollbackLines, cursorBlink, opacityPercent);

    // Save to config file
    m_config->Save(Core::Config::GetDefaultConfigPath());
}

void SettingsDialog::PreviewSettings(HWND hwnd) {
    // Called on any control change for live preview
    if (m_previewCallback) {
        m_previewCallback();
    }
}

void SettingsDialog::RestoreSettings() {
    if (m_originalConfig && m_config) {
        *m_config = *m_originalConfig;
        spdlog::info("Settings restored");
    }
}

std::wstring SettingsDialog::GetEditText(HWND hwnd, int controlId) {
    wchar_t buffer[256];
    GetDlgItemTextW(hwnd, controlId, buffer, 256);
    return buffer;
}

int SettingsDialog::GetEditInt(HWND hwnd, int controlId, int defaultValue) {
    BOOL success = FALSE;
    int value = GetDlgItemInt(hwnd, controlId, &success, FALSE);
    return success ? value : defaultValue;
}

bool SettingsDialog::GetCheckState(HWND hwnd, int controlId) {
    return IsDlgButtonChecked(hwnd, controlId) == BST_CHECKED;
}

INT_PTR SettingsDialog::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Not used currently - using window proc directly
    return FALSE;
}

void SettingsDialog::InitFontTab(HWND hwnd) {
    // Reserved for tabbed interface
}

void SettingsDialog::InitColorsTab(HWND hwnd) {
    // Reserved for tabbed interface
}

void SettingsDialog::InitTerminalTab(HWND hwnd) {
    // Reserved for tabbed interface
}

} // namespace TerminalDX12::UI
