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

namespace {
constexpr int kColorSwatchIds[] = {1020, 1021, 1022, 1023}; // FG, BG, Cursor, Selection

void InvalidateColorSwatches(HWND hwnd) {
    for (int id : kColorSwatchIds)
        InvalidateRect(GetDlgItem(hwnd, id), nullptr, TRUE);
}

// Helper struct for control layout
struct LayoutParams {
    HWND hwnd;
    HINSTANCE hInst;
    int leftMargin = 20, labelWidth = 130, controlWidth = 200, controlHeight = 22;
    int spacing = 28, controlX = 160;
    int y = 10;

    void Label(const wchar_t* text, int width = 200) {
        CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                      leftMargin, y + 3, width, 20, hwnd, nullptr, hInst, nullptr);
    }

    void Section(const wchar_t* text) {
        CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                      10, y, 200, 20, hwnd, nullptr, hInst, nullptr);
        y += 22;
    }

    HWND Edit(int id, int width = 200, DWORD extraStyle = 0) {
        HWND ctrl = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | extraStyle,
                      controlX, y, width, controlHeight, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        return ctrl;
    }

    void EditWithLabel(const wchar_t* label, int id, int width = 200, DWORD extraStyle = 0, const wchar_t* hint = nullptr) {
        Label(label);
        Edit(id, width, extraStyle);
        if (hint) {
            CreateWindowW(L"STATIC", hint, WS_CHILD | WS_VISIBLE | SS_LEFT,
                          controlX + width + 10, y + 3, 90, 20, hwnd, nullptr, hInst, nullptr);
        }
        y += spacing;
    }

    void ColorSwatch(const wchar_t* label, int id) {
        Label(label);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                      controlX, y, 28, 28, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        y += spacing + 2;
    }

    void NextRow() { y += spacing; }
};
} // anonymous namespace

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

INT_PTR CALLBACK SettingsDialog::DialogProc(HWND, UINT, WPARAM, LPARAM) {
    // Not used - using window proc directly in Show()
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
                        case IDC_COLOR_BACKGROUND:
                        case IDC_COLOR_CURSOR:
                        case IDC_COLOR_SELECTION: {
                            COLORREF* colorPtrs[] = {&dialog->m_foregroundColor, &dialog->m_backgroundColor,
                                                     &dialog->m_cursorColor, &dialog->m_selectionColor};
                            for (int i = 0; i < 4; ++i) {
                                if (id == kColorSwatchIds[i]) {
                                    dialog->ShowColorPicker(hwnd, *colorPtrs[i], id);
                                    break;
                                }
                            }
                            return 0;
                        }
                    }

                    // Live preview on control changes
                    if (notification == EN_CHANGE || notification == CBN_SELCHANGE) {
                        dialog->PreviewSettings(hwnd);
                    }
                }
                break;
            case WM_DRAWITEM:
                if (dialog) {
                    DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                    if (dis->CtlType == ODT_BUTTON) {
                        COLORREF colors[] = {dialog->m_foregroundColor, dialog->m_backgroundColor,
                                             dialog->m_cursorColor, dialog->m_selectionColor};
                        for (int i = 0; i < 4; ++i) {
                            if (dis->CtlID == kColorSwatchIds[i]) {
                                HBRUSH brush = CreateSolidBrush(colors[i]);
                                FillRect(dis->hDC, &dis->rcItem, brush);
                                DeleteObject(brush);
                                FrameRect(dis->hDC, &dis->rcItem, (HBRUSH)GetStockObject(BLACK_BRUSH));
                                return TRUE;
                            }
                        }
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
    LayoutParams L{hwnd, hInstance};

    // Appearance section
    L.Section(L"Appearance");

    // Font dropdown
    L.Label(L"Font Family:");
    HWND fontCombo = CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        L.controlX, L.y, L.controlWidth, 200, hwnd, (HMENU)IDC_FONT_COMBO, hInstance, nullptr);
    FontEnumerator fontEnum;
    for (const auto& font : fontEnum.EnumerateMonospaceFonts())
        SendMessageW(fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font.familyName.c_str()));
    L.NextRow();

    L.EditWithLabel(L"Font Size:", IDC_FONT_SIZE, 60, ES_NUMBER, L"(6-72)");

    // Color swatches
    L.ColorSwatch(L"Foreground:", IDC_COLOR_FOREGROUND);
    L.ColorSwatch(L"Background:", IDC_COLOR_BACKGROUND);
    L.ColorSwatch(L"Cursor:", IDC_COLOR_CURSOR);
    L.ColorSwatch(L"Selection:", IDC_COLOR_SELECTION);
    L.y += 6;

    // Terminal section
    L.Section(L"Terminal");
    L.EditWithLabel(L"Shell Path:", IDC_SHELL_PATH, 200, ES_AUTOHSCROLL);
    L.EditWithLabel(L"Working Directory:", IDC_WORKING_DIR, 200, ES_AUTOHSCROLL);
    L.EditWithLabel(L"Scrollback Lines:", IDC_SCROLLBACK_LINES, 80, ES_NUMBER, L"(100-100,000)");

    // Cursor style radio buttons
    L.Label(L"Cursor Style:");
    CreateWindowW(L"BUTTON", L"Block", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        L.controlX, L.y, 60, L.controlHeight, hwnd, (HMENU)IDC_CURSOR_BLOCK, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Underline", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        L.controlX + 70, L.y, 80, L.controlHeight, hwnd, (HMENU)IDC_CURSOR_UNDERLINE, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Bar", WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        L.controlX + 160, L.y, 50, L.controlHeight, hwnd, (HMENU)IDC_CURSOR_BAR, hInstance, nullptr);
    L.NextRow();

    CreateWindowW(L"BUTTON", L"Cursor Blink", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        L.leftMargin, L.y, 150, L.controlHeight, hwnd, (HMENU)IDC_CURSOR_BLINK, hInstance, nullptr);
    L.NextRow();

    L.EditWithLabel(L"Opacity:", IDC_OPACITY, 60, ES_NUMBER, L"% (10-100)");

    // Buttons at bottom
    constexpr int btnW = 80, btnH = 28, btnY = DIALOG_HEIGHT - 70;
    CreateWindowW(L"BUTTON", L"Reset", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, btnY, btnW, btnH, hwnd, (HMENU)IDC_RESET, hInstance, nullptr);
    int x = DIALOG_WIDTH - btnW * 3 - 50;
    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        x, btnY, btnW, btnH, hwnd, (HMENU)IDC_OK, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnW + 10, btnY, btnW, btnH, hwnd, (HMENU)IDC_APPLY, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnW * 2 + 20, btnY, btnW, btnH, hwnd, (HMENU)IDC_CANCEL, hInstance, nullptr);
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

    InvalidateColorSwatches(hwnd);
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
    InvalidateColorSwatches(hwnd);

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

} // namespace TerminalDX12::UI
