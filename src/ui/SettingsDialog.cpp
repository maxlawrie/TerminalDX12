#include "ui/SettingsDialog.h"
#include "ui/FontEnumerator.h"
#include "core/Config.h"
#include <spdlog/spdlog.h>
#include <CommCtrl.h>
#include <Commdlg.h>
#include <windowsx.h>
#include <sstream>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace TerminalDX12::UI {

// Control IDs
enum {
    IDC_TAB_CONTROL = 1001,
    // Font tab
    IDC_FONT_COMBO = 1010,
    IDC_FONT_SIZE = 1011,
    IDC_FONT_BOLD = 1012,
    // Colors tab
    IDC_COLOR_FOREGROUND = 1020,
    IDC_COLOR_BACKGROUND = 1021,
    IDC_COLOR_CURSOR = 1022,
    IDC_COLOR_SELECTION = 1023,
    // Terminal tab
    IDC_SCROLLBACK_LINES = 1030,
    IDC_SHELL_PATH = 1031,
    IDC_CURSOR_BLINK = 1032,
    IDC_OPACITY = 1033,
    IDC_CURSOR_BLOCK = 1034,
    IDC_CURSOR_UNDERLINE = 1035,
    IDC_CURSOR_BAR = 1036,
    IDC_WORKING_DIR = 1037,
    // Buttons
    IDC_APPLY = 1100,
    IDC_CANCEL = 1101,
    IDC_OK = 1102,
    IDC_RESET = 1103
};

// Dialog dimensions
constexpr int DIALOG_WIDTH = 480;
constexpr int DIALOG_HEIGHT = 420;

// Custom colors for ChooseColor dialog
static COLORREF s_customColors[16] = {};

SettingsDialog::SettingsDialog(HWND parentWindow, Core::Config* config)
    : m_parentWindow(parentWindow)
    , m_config(config)
{
}

SettingsDialog::~SettingsDialog() = default;

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
    m_originalConfig = std::make_unique<Core::Config>();
    *m_originalConfig = *m_config;
    m_settingsChanged = false;

    // Store colors for color picker preview
    const auto& colors = m_config->GetColors();
    m_foregroundColor = RGB(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    m_backgroundColor = RGB(colors.background.r, colors.background.g, colors.background.b);
    m_cursorColor = RGB(colors.cursor.r, colors.cursor.g, colors.cursor.b);
    m_selectionColor = RGB(colors.selection.r, colors.selection.g, colors.selection.b);

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
                        case IDC_RESET:
                            dialog->ResetToDefaults(hwnd);
                            return 0;
                        case IDC_COLOR_FOREGROUND:
                            dialog->ShowColorPicker(hwnd, dialog->m_foregroundColor, IDC_COLOR_FOREGROUND);
                            return 0;
                        case IDC_COLOR_BACKGROUND:
                            dialog->ShowColorPicker(hwnd, dialog->m_backgroundColor, IDC_COLOR_BACKGROUND);
                            return 0;
                        case IDC_COLOR_CURSOR:
                            dialog->ShowColorPicker(hwnd, dialog->m_cursorColor, IDC_COLOR_CURSOR);
                            return 0;
                        case IDC_COLOR_SELECTION:
                            dialog->ShowColorPicker(hwnd, dialog->m_selectionColor, IDC_COLOR_SELECTION);
                            return 0;
                    }

                    // Live preview on control changes
                    if (notification == EN_CHANGE || notification == CBN_SELCHANGE) {
                        dialog->PreviewSettings(hwnd);
                    }
                }
                break;
            case WM_DRAWITEM: {
                // Custom draw for color swatch buttons
                if (dialog) {
                    DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                    if (dis->CtlType == ODT_BUTTON) {
                        COLORREF color = 0;
                        switch (dis->CtlID) {
                            case IDC_COLOR_FOREGROUND: color = dialog->m_foregroundColor; break;
                            case IDC_COLOR_BACKGROUND: color = dialog->m_backgroundColor; break;
                            case IDC_COLOR_CURSOR: color = dialog->m_cursorColor; break;
                            case IDC_COLOR_SELECTION: color = dialog->m_selectionColor; break;
                        }
                        HBRUSH brush = CreateSolidBrush(color);
                        FillRect(dis->hDC, &dis->rcItem, brush);
                        DeleteObject(brush);
                        FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                        return TRUE;
                    }
                }
                break;
            }
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
    int labelWidth = 130;
    int controlWidth = 200;
    int controlHeight = 22;
    int spacing = 28;
    int leftMargin = 20;
    int controlX = leftMargin + labelWidth + 10;

    // ========== APPEARANCE SECTION ==========
    CreateWindowW(L"STATIC", L"Appearance", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  10, y, 200, 20, hwnd, nullptr, hInstance, nullptr);
    y += 22;

    // Font dropdown (ComboBox)
    CreateWindowW(L"STATIC", L"Font Family:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    HWND fontCombo = CreateWindowW(L"COMBOBOX", L"",
                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
                  controlX, y, controlWidth, 200, hwnd,
                  (HMENU)IDC_FONT_COMBO, hInstance, nullptr);

    // Populate font dropdown with monospace fonts
    FontEnumerator fontEnum;
    auto fonts = fontEnum.EnumerateMonospaceFonts();
    for (const auto& font : fonts) {
        SendMessageW(fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font.familyName.c_str()));
    }
    y += spacing;

    // Font size
    CreateWindowW(L"STATIC", L"Font Size:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  controlX, y, 60, controlHeight, hwnd,
                  (HMENU)IDC_FONT_SIZE, hInstance, nullptr);
    CreateWindowW(L"STATIC", L"(6-72)", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  controlX + 70, y + 3, 50, 20, hwnd, nullptr, hInstance, nullptr);
    y += spacing;

    // Color swatches
    int swatchSize = 28;
    CreateWindowW(L"STATIC", L"Foreground:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  controlX, y, swatchSize, swatchSize, hwnd,
                  (HMENU)IDC_COLOR_FOREGROUND, hInstance, nullptr);
    y += spacing + 2;

    CreateWindowW(L"STATIC", L"Background:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  controlX, y, swatchSize, swatchSize, hwnd,
                  (HMENU)IDC_COLOR_BACKGROUND, hInstance, nullptr);
    y += spacing + 2;

    CreateWindowW(L"STATIC", L"Cursor:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  controlX, y, swatchSize, swatchSize, hwnd,
                  (HMENU)IDC_COLOR_CURSOR, hInstance, nullptr);
    y += spacing + 2;

    CreateWindowW(L"STATIC", L"Selection:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  controlX, y, swatchSize, swatchSize, hwnd,
                  (HMENU)IDC_COLOR_SELECTION, hInstance, nullptr);
    y += spacing + 8;

    // ========== TERMINAL SECTION ==========
    CreateWindowW(L"STATIC", L"Terminal", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  10, y, 200, 20, hwnd, nullptr, hInstance, nullptr);
    y += 22;

    // Shell path
    CreateWindowW(L"STATIC", L"Shell Path:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  controlX, y, controlWidth, controlHeight, hwnd,
                  (HMENU)IDC_SHELL_PATH, hInstance, nullptr);
    y += spacing;

    // Working directory
    CreateWindowW(L"STATIC", L"Working Directory:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                  controlX, y, controlWidth, controlHeight, hwnd,
                  (HMENU)IDC_WORKING_DIR, hInstance, nullptr);
    y += spacing;

    // Scrollback lines
    CreateWindowW(L"STATIC", L"Scrollback Lines:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  controlX, y, 80, controlHeight, hwnd,
                  (HMENU)IDC_SCROLLBACK_LINES, hInstance, nullptr);
    CreateWindowW(L"STATIC", L"(100-100,000)", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  controlX + 90, y + 3, 90, 20, hwnd, nullptr, hInstance, nullptr);
    y += spacing;

    // Cursor style radio buttons
    CreateWindowW(L"STATIC", L"Cursor Style:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Block", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                  controlX, y, 60, controlHeight, hwnd,
                  (HMENU)IDC_CURSOR_BLOCK, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Underline", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                  controlX + 70, y, 80, controlHeight, hwnd,
                  (HMENU)IDC_CURSOR_UNDERLINE, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Bar", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                  controlX + 160, y, 50, controlHeight, hwnd,
                  (HMENU)IDC_CURSOR_BAR, hInstance, nullptr);
    y += spacing;

    // Cursor blink
    CreateWindowW(L"BUTTON", L"Cursor Blink", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  leftMargin, y, 150, controlHeight, hwnd,
                  (HMENU)IDC_CURSOR_BLINK, hInstance, nullptr);
    y += spacing;

    // Opacity
    CreateWindowW(L"STATIC", L"Opacity:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  leftMargin, y + 3, labelWidth, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                  controlX, y, 60, controlHeight, hwnd,
                  (HMENU)IDC_OPACITY, hInstance, nullptr);
    CreateWindowW(L"STATIC", L"% (10-100)", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  controlX + 70, y + 3, 80, 20, hwnd, nullptr, hInstance, nullptr);
    y += spacing;

    // ========== BUTTONS ==========
    int buttonWidth = 80;
    int buttonHeight = 28;
    int buttonY = DIALOG_HEIGHT - 70;
    int buttonX = 10;

    // Reset to Defaults on the left
    CreateWindowW(L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  buttonX, buttonY, buttonWidth, buttonHeight, hwnd,
                  (HMENU)IDC_RESET, hInstance, nullptr);

    // OK, Apply, Cancel on the right
    buttonX = DIALOG_WIDTH - buttonWidth * 3 - 50;
    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  buttonX, buttonY, buttonWidth, buttonHeight, hwnd,
                  (HMENU)IDC_OK, hInstance, nullptr);
    buttonX += buttonWidth + 10;

    CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  buttonX, buttonY, buttonWidth, buttonHeight, hwnd,
                  (HMENU)IDC_APPLY, hInstance, nullptr);
    buttonX += buttonWidth + 10;

    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  buttonX, buttonY, buttonWidth, buttonHeight, hwnd,
                  (HMENU)IDC_CANCEL, hInstance, nullptr);
}

void SettingsDialog::LoadCurrentSettings(HWND hwnd) {
    if (!m_config) return;

    const auto& font = m_config->GetFont();
    const auto& terminal = m_config->GetTerminal();
    const auto& colors = m_config->GetColors();

    // Font settings
    std::wstring fontFamily(font.family.begin(), font.family.end());
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    int index = static_cast<int>(SendMessageW(fontCombo, CB_FINDSTRINGEXACT, -1,
                                               reinterpret_cast<LPARAM>(fontFamily.c_str())));
    if (index != CB_ERR) {
        SendMessageW(fontCombo, CB_SETCURSEL, index, 0);
    } else {
        // Font not found - add it and select (fallback will happen on next load)
        SendMessageW(fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(fontFamily.c_str()));
        SendMessageW(fontCombo, CB_SETCURSEL, SendMessageW(fontCombo, CB_GETCOUNT, 0, 0) - 1, 0);
    }
    SetDlgItemInt(hwnd, IDC_FONT_SIZE, font.size, FALSE);

    // Terminal settings
    SetDlgItemTextW(hwnd, IDC_SHELL_PATH, terminal.shell.c_str());
    SetDlgItemTextW(hwnd, IDC_WORKING_DIR, terminal.workingDirectory.c_str());
    SetDlgItemInt(hwnd, IDC_SCROLLBACK_LINES, terminal.scrollbackLines, FALSE);
    CheckDlgButton(hwnd, IDC_CURSOR_BLINK, terminal.cursorBlink ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_OPACITY, static_cast<int>(terminal.opacity * 100), FALSE);

    // Cursor style
    switch (terminal.cursorStyle) {
        case Core::CursorStyle::Block:
            CheckRadioButton(hwnd, IDC_CURSOR_BLOCK, IDC_CURSOR_BAR, IDC_CURSOR_BLOCK);
            break;
        case Core::CursorStyle::Underline:
            CheckRadioButton(hwnd, IDC_CURSOR_BLOCK, IDC_CURSOR_BAR, IDC_CURSOR_UNDERLINE);
            break;
        case Core::CursorStyle::Bar:
            CheckRadioButton(hwnd, IDC_CURSOR_BLOCK, IDC_CURSOR_BAR, IDC_CURSOR_BAR);
            break;
    }

    // Colors
    m_foregroundColor = RGB(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    m_backgroundColor = RGB(colors.background.r, colors.background.g, colors.background.b);
    m_cursorColor = RGB(colors.cursor.r, colors.cursor.g, colors.cursor.b);
    m_selectionColor = RGB(colors.selection.r, colors.selection.g, colors.selection.b);

    // Force redraw of color swatches
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_FOREGROUND), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_BACKGROUND), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_CURSOR), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SELECTION), nullptr, TRUE);
}

void SettingsDialog::ApplySettings(HWND hwnd) {
    if (!m_config) return;

    // Read font settings from ComboBox
    wchar_t buffer[256];
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    int selIndex = static_cast<int>(SendMessageW(fontCombo, CB_GETCURSEL, 0, 0));
    if (selIndex != CB_ERR) {
        SendMessageW(fontCombo, CB_GETLBTEXT, selIndex, reinterpret_cast<LPARAM>(buffer));
    } else {
        wcscpy_s(buffer, L"Consolas");
    }
    std::wstring fontNameW = buffer;
    std::string fontName;
    fontName.reserve(fontNameW.size());
    for (wchar_t wc : fontNameW) {
        fontName.push_back(static_cast<char>(wc & 0xFF));
    }

    int fontSize = GetDlgItemInt(hwnd, IDC_FONT_SIZE, nullptr, FALSE);
    if (fontSize < 6) fontSize = 6;
    if (fontSize > 72) fontSize = 72;

    // Read terminal settings
    GetDlgItemTextW(hwnd, IDC_SHELL_PATH, buffer, 256);
    std::wstring shellPath = buffer;

    GetDlgItemTextW(hwnd, IDC_WORKING_DIR, buffer, 256);
    std::wstring workingDir = buffer;

    int scrollbackLines = GetDlgItemInt(hwnd, IDC_SCROLLBACK_LINES, nullptr, FALSE);
    if (scrollbackLines < 100) scrollbackLines = 100;
    if (scrollbackLines > 100000) scrollbackLines = 100000;

    bool cursorBlink = IsDlgButtonChecked(hwnd, IDC_CURSOR_BLINK) == BST_CHECKED;

    int opacityPercent = GetDlgItemInt(hwnd, IDC_OPACITY, nullptr, FALSE);
    if (opacityPercent < 10) opacityPercent = 10;
    if (opacityPercent > 100) opacityPercent = 100;
    float opacity = opacityPercent / 100.0f;

    // Cursor style
    Core::CursorStyle cursorStyle = Core::CursorStyle::Block;
    if (IsDlgButtonChecked(hwnd, IDC_CURSOR_UNDERLINE) == BST_CHECKED) {
        cursorStyle = Core::CursorStyle::Underline;
    } else if (IsDlgButtonChecked(hwnd, IDC_CURSOR_BAR) == BST_CHECKED) {
        cursorStyle = Core::CursorStyle::Bar;
    }

    // Update config
    Core::FontConfig& font = m_config->GetFontMut();
    font.family = fontName;
    font.size = fontSize;

    Core::TerminalConfig& terminal = m_config->GetTerminalMut();
    terminal.shell = shellPath;
    terminal.workingDirectory = workingDir;
    terminal.scrollbackLines = scrollbackLines;
    terminal.cursorBlink = cursorBlink;
    terminal.opacity = opacity;
    terminal.cursorStyle = cursorStyle;

    // Update colors
    Core::ColorConfig& colors = m_config->GetColorsMut();
    colors.foreground = Core::Color(GetRValue(m_foregroundColor), GetGValue(m_foregroundColor), GetBValue(m_foregroundColor));
    colors.background = Core::Color(GetRValue(m_backgroundColor), GetGValue(m_backgroundColor), GetBValue(m_backgroundColor));
    colors.cursor = Core::Color(GetRValue(m_cursorColor), GetGValue(m_cursorColor), GetBValue(m_cursorColor));
    colors.selection = Core::Color(GetRValue(m_selectionColor), GetGValue(m_selectionColor), GetBValue(m_selectionColor), 128);

    // Convert shell path to narrow string for logging
    std::string shellPathNarrow;
    shellPathNarrow.reserve(shellPath.size());
    for (wchar_t wc : shellPath) {
        shellPathNarrow.push_back(static_cast<char>(wc & 0xFF));
    }
    spdlog::info("Settings applied - Font: {} {}pt, Shell: {}, Scrollback: {}, Cursor: {}, Opacity: {}%",
                 fontName, fontSize,
                 shellPathNarrow,
                 scrollbackLines,
                 cursorStyle == Core::CursorStyle::Block ? "Block" :
                    (cursorStyle == Core::CursorStyle::Underline ? "Underline" : "Bar"),
                 opacityPercent);

    // Save to config file
    m_config->Save(Core::Config::GetDefaultConfigPath());
}

void SettingsDialog::ShowColorPicker(HWND hwnd, COLORREF& color, int controlId) {
    CHOOSECOLORW cc = { sizeof(cc) };
    cc.hwndOwner = hwnd;
    cc.lpCustColors = s_customColors;
    cc.rgbResult = color;
    cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        color = cc.rgbResult;
        // Redraw the color swatch button
        InvalidateRect(GetDlgItem(hwnd, controlId), nullptr, TRUE);
    }
}

void SettingsDialog::ResetToDefaults(HWND hwnd) {
    // Confirm reset
    int result = MessageBoxW(hwnd,
        L"Are you sure you want to reset all settings to their default values?",
        L"Reset to Defaults",
        MB_YESNO | MB_ICONQUESTION);

    if (result != IDYES) {
        return;
    }

    // Reset to default values
    Core::Config defaults;

    // Apply defaults to dialog controls
    const auto& font = defaults.GetFont();
    const auto& terminal = defaults.GetTerminal();
    const auto& colors = defaults.GetColors();

    // Font
    std::wstring fontFamily(font.family.begin(), font.family.end());
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    int index = static_cast<int>(SendMessageW(fontCombo, CB_FINDSTRINGEXACT, -1,
                                               reinterpret_cast<LPARAM>(fontFamily.c_str())));
    if (index != CB_ERR) {
        SendMessageW(fontCombo, CB_SETCURSEL, index, 0);
    }
    SetDlgItemInt(hwnd, IDC_FONT_SIZE, font.size, FALSE);

    // Terminal
    SetDlgItemTextW(hwnd, IDC_SHELL_PATH, terminal.shell.c_str());
    SetDlgItemTextW(hwnd, IDC_WORKING_DIR, terminal.workingDirectory.c_str());
    SetDlgItemInt(hwnd, IDC_SCROLLBACK_LINES, terminal.scrollbackLines, FALSE);
    CheckDlgButton(hwnd, IDC_CURSOR_BLINK, terminal.cursorBlink ? BST_CHECKED : BST_UNCHECKED);
    SetDlgItemInt(hwnd, IDC_OPACITY, static_cast<int>(terminal.opacity * 100), FALSE);
    CheckRadioButton(hwnd, IDC_CURSOR_BLOCK, IDC_CURSOR_BAR, IDC_CURSOR_BLOCK);

    // Colors
    m_foregroundColor = RGB(colors.foreground.r, colors.foreground.g, colors.foreground.b);
    m_backgroundColor = RGB(colors.background.r, colors.background.g, colors.background.b);
    m_cursorColor = RGB(colors.cursor.r, colors.cursor.g, colors.cursor.b);
    m_selectionColor = RGB(colors.selection.r, colors.selection.g, colors.selection.b);

    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_FOREGROUND), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_BACKGROUND), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_CURSOR), nullptr, TRUE);
    InvalidateRect(GetDlgItem(hwnd, IDC_COLOR_SELECTION), nullptr, TRUE);

    spdlog::info("Settings reset to defaults");
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
