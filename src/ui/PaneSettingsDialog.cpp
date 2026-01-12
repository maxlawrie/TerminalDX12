#include "ui/PaneSettingsDialog.h"
#include "ui/TerminalSession.h"
#include "core/Config.h"
#include <spdlog/spdlog.h>
#include <CommCtrl.h>
#include <Commdlg.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace TerminalDX12::UI {

namespace {
constexpr int kColorSwatchIds[] = {2020, 2021, 2022}; // FG, BG, Cursor

void InvalidateColorSwatches(HWND hwnd) {
    for (int id : kColorSwatchIds)
        InvalidateRect(GetDlgItem(hwnd, id), nullptr, TRUE);
}

// Helper struct for control layout
struct LayoutParams {
    HWND hwnd;
    HINSTANCE hInst;
    int leftMargin = 20, labelWidth = 130, controlWidth = 200, controlHeight = 22;
    int spacing = 28, controlX = 140;
    int y = 15;

    void Label(const wchar_t* text, int width = 200) {
        CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                      leftMargin, y + 3, width, 20, hwnd, nullptr, hInst, nullptr);
    }

    void Section(const wchar_t* text) {
        CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                      10, y, 300, 20, hwnd, nullptr, hInst, nullptr);
        y += 24;
    }

    HWND Combo(int id, int width = 200) {
        HWND ctrl = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
            controlX, y, width, 200, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        return ctrl;
    }

    HWND Check(int id, const wchar_t* text) {
        HWND ctrl = CreateWindowW(L"BUTTON", text,
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            leftMargin, y, 300, controlHeight, hwnd, (HMENU)(INT_PTR)id, hInst, nullptr);
        return ctrl;
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
    IDC_PROFILE_COMBO = 2010,
    IDC_USE_CUSTOM = 2011,
    IDC_COLOR_FOREGROUND = 2020,
    IDC_COLOR_BACKGROUND = 2021,
    IDC_COLOR_CURSOR = 2022,
    IDC_APPLY = 2100,
    IDC_CANCEL = 2101,
    IDC_OK = 2102
};

// Dialog dimensions
constexpr int DIALOG_WIDTH = 380;
constexpr int DIALOG_HEIGHT = 320;

// Custom colors for ChooseColor dialog
static COLORREF s_customColors[16] = {};

PaneSettingsDialog::PaneSettingsDialog(HWND parentWindow, Core::Config* config, TerminalSession* session)
    : m_parentWindow(parentWindow)
    , m_config(config)
    , m_session(session)
{
}

PaneSettingsDialog::~PaneSettingsDialog() = default;

INT_PTR CALLBACK PaneSettingsDialog::DialogProc(HWND, UINT, WPARAM, LPARAM) {
    return FALSE;
}

bool PaneSettingsDialog::Show() {
    if (!m_session || !m_config) {
        spdlog::error("PaneSettingsDialog: No session or config");
        return false;
    }

    m_settingsChanged = false;
    m_selectedProfile = m_session->GetProfileName();
    m_useCustomSettings = m_session->HasProfileOverride();

    // Get current profile colors
    const Core::Profile* profile = m_session->GetEffectiveProfile();
    if (profile) {
        m_foregroundColor = RGB(profile->colors.foreground.r, profile->colors.foreground.g, profile->colors.foreground.b);
        m_backgroundColor = RGB(profile->colors.background.r, profile->colors.background.g, profile->colors.background.b);
        m_cursorColor = RGB(profile->colors.cursor.r, profile->colors.cursor.g, profile->colors.cursor.b);
    }

    // Cache profile names
    m_profileNames = m_config->GetProfileNames();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        PaneSettingsDialog* dialog = reinterpret_cast<PaneSettingsDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                dialog = reinterpret_cast<PaneSettingsDialog*>(cs->lpCreateParams);
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
                            if (dialog->m_applyCallback) {
                                dialog->m_applyCallback();
                            }
                            return 0;
                        case IDC_CANCEL:
                            DestroyWindow(hwnd);
                            return 0;
                        case IDC_PROFILE_COMBO:
                            if (notification == CBN_SELCHANGE) {
                                dialog->OnProfileSelected(hwnd);
                            }
                            return 0;
                        case IDC_USE_CUSTOM: {
                            HWND check = GetDlgItem(hwnd, IDC_USE_CUSTOM);
                            dialog->m_useCustomSettings = (SendMessage(check, BM_GETCHECK, 0, 0) == BST_CHECKED);
                            // Enable/disable color swatches
                            for (int swatchId : kColorSwatchIds) {
                                EnableWindow(GetDlgItem(hwnd, swatchId), dialog->m_useCustomSettings);
                            }
                            return 0;
                        }
                        case IDC_COLOR_FOREGROUND:
                        case IDC_COLOR_BACKGROUND:
                        case IDC_COLOR_CURSOR: {
                            COLORREF* colorPtrs[] = {&dialog->m_foregroundColor, &dialog->m_backgroundColor,
                                                     &dialog->m_cursorColor};
                            for (int i = 0; i < 3; ++i) {
                                if (id == kColorSwatchIds[i]) {
                                    dialog->ShowColorPicker(hwnd, *colorPtrs[i], id);
                                    break;
                                }
                            }
                            return 0;
                        }
                    }
                }
                break;
            case WM_DRAWITEM:
                if (dialog) {
                    DRAWITEMSTRUCT* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                    if (dis->CtlType == ODT_BUTTON) {
                        COLORREF colors[] = {dialog->m_foregroundColor, dialog->m_backgroundColor,
                                             dialog->m_cursorColor};
                        for (int i = 0; i < 3; ++i) {
                            if (dis->CtlID == static_cast<UINT>(kColorSwatchIds[i])) {
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
                DestroyWindow(hwnd);
                return 0;
            case WM_DESTROY:
                return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    };
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"TerminalDX12PaneSettings";
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
        L"TerminalDX12PaneSettings",
        L"Pane Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
        m_parentWindow,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!hwnd) {
        spdlog::error("Failed to create pane settings window");
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

void PaneSettingsDialog::InitializeControls(HWND hwnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);
    LayoutParams L{hwnd, hInstance};

    // Profile section
    L.Section(L"Profile");
    L.Label(L"Profile:");
    L.Combo(IDC_PROFILE_COMBO, 180);
    L.NextRow();

    L.y += 10;
    L.Check(IDC_USE_CUSTOM, L"Use custom colors for this pane");
    L.NextRow();

    // Colors section (disabled by default if not using custom)
    L.y += 5;
    L.Section(L"Custom Colors");
    L.ColorSwatch(L"Foreground:", IDC_COLOR_FOREGROUND);
    L.ColorSwatch(L"Background:", IDC_COLOR_BACKGROUND);
    L.ColorSwatch(L"Cursor:", IDC_COLOR_CURSOR);

    // Buttons
    int buttonY = DIALOG_HEIGHT - 70;
    int buttonWidth = 80;
    int buttonSpacing = 10;
    int totalButtonWidth = buttonWidth * 3 + buttonSpacing * 2;
    int buttonX = (DIALOG_WIDTH - totalButtonWidth) / 2;

    CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                  buttonX, buttonY, buttonWidth, 28, hwnd, (HMENU)IDC_OK, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Apply", WS_CHILD | WS_VISIBLE,
                  buttonX + buttonWidth + buttonSpacing, buttonY, buttonWidth, 28, hwnd, (HMENU)IDC_APPLY, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
                  buttonX + (buttonWidth + buttonSpacing) * 2, buttonY, buttonWidth, 28, hwnd, (HMENU)IDC_CANCEL, hInstance, nullptr);
}

void PaneSettingsDialog::PopulateProfileList(HWND hwnd) {
    HWND combo = GetDlgItem(hwnd, IDC_PROFILE_COMBO);
    SendMessage(combo, CB_RESETCONTENT, 0, 0);

    for (const auto& name : m_profileNames) {
        std::wstring wname(name.begin(), name.end());
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wname.c_str()));
    }
}

void PaneSettingsDialog::LoadCurrentSettings(HWND hwnd) {
    // Populate profile dropdown
    PopulateProfileList(hwnd);

    // Select current profile
    HWND combo = GetDlgItem(hwnd, IDC_PROFILE_COMBO);
    for (size_t i = 0; i < m_profileNames.size(); ++i) {
        if (m_profileNames[i] == m_selectedProfile) {
            SendMessage(combo, CB_SETCURSEL, i, 0);
            break;
        }
    }

    // Set custom checkbox
    HWND check = GetDlgItem(hwnd, IDC_USE_CUSTOM);
    SendMessage(check, BM_SETCHECK, m_useCustomSettings ? BST_CHECKED : BST_UNCHECKED, 0);

    // Enable/disable color swatches based on custom checkbox
    for (int swatchId : kColorSwatchIds) {
        EnableWindow(GetDlgItem(hwnd, swatchId), m_useCustomSettings);
    }
}

void PaneSettingsDialog::OnProfileSelected(HWND hwnd) {
    HWND combo = GetDlgItem(hwnd, IDC_PROFILE_COMBO);
    int sel = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(m_profileNames.size())) {
        m_selectedProfile = m_profileNames[sel];

        // Update colors to match selected profile (for preview)
        const Core::Profile* profile = m_config->GetProfile(m_selectedProfile);
        if (profile && !m_useCustomSettings) {
            m_foregroundColor = RGB(profile->colors.foreground.r, profile->colors.foreground.g, profile->colors.foreground.b);
            m_backgroundColor = RGB(profile->colors.background.r, profile->colors.background.g, profile->colors.background.b);
            m_cursorColor = RGB(profile->colors.cursor.r, profile->colors.cursor.g, profile->colors.cursor.b);
            InvalidateColorSwatches(hwnd);
        }
    }
}

void PaneSettingsDialog::ApplySettings(HWND hwnd) {
    // Get selected profile
    HWND combo = GetDlgItem(hwnd, IDC_PROFILE_COMBO);
    int sel = static_cast<int>(SendMessage(combo, CB_GETCURSEL, 0, 0));
    if (sel >= 0 && sel < static_cast<int>(m_profileNames.size())) {
        m_selectedProfile = m_profileNames[sel];
    }

    // Apply profile to session
    m_session->SetProfileName(m_selectedProfile);

    // Apply custom colors if checkbox is set
    HWND check = GetDlgItem(hwnd, IDC_USE_CUSTOM);
    m_useCustomSettings = (SendMessage(check, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (m_useCustomSettings) {
        // Create a profile override with custom colors
        const Core::Profile* baseProfile = m_config->GetProfile(m_selectedProfile);
        if (baseProfile) {
            Core::Profile override = *baseProfile;
            override.colors.foreground = Core::Color(GetRValue(m_foregroundColor), GetGValue(m_foregroundColor), GetBValue(m_foregroundColor));
            override.colors.background = Core::Color(GetRValue(m_backgroundColor), GetGValue(m_backgroundColor), GetBValue(m_backgroundColor));
            override.colors.cursor = Core::Color(GetRValue(m_cursorColor), GetGValue(m_cursorColor), GetBValue(m_cursorColor));
            m_session->SetProfileOverride(override);
        }
    } else {
        m_session->ClearProfileOverride();
    }

    spdlog::info("Pane settings applied: profile='{}', customColors={}", m_selectedProfile, m_useCustomSettings);
}

void PaneSettingsDialog::ShowColorPicker(HWND hwnd, COLORREF& color, int controlId) {
    CHOOSECOLORW cc = {};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwnd;
    cc.lpCustColors = s_customColors;
    cc.rgbResult = color;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;

    if (ChooseColorW(&cc)) {
        color = cc.rgbResult;
        InvalidateRect(GetDlgItem(hwnd, controlId), nullptr, TRUE);
    }
}

} // namespace TerminalDX12::UI
