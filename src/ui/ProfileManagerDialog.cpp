#include "ui/ProfileManagerDialog.h"
#include "core/Config.h"
#include <spdlog/spdlog.h>
#include <CommCtrl.h>
#include <Commdlg.h>
#include <windowsx.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace TerminalDX12::UI {

namespace {
constexpr int kColorSwatchIds[] = {3030, 3031, 3032}; // FG, BG, Cursor

void InvalidateColorSwatches(HWND hwnd) {
    for (int id : kColorSwatchIds)
        InvalidateRect(GetDlgItem(hwnd, id), nullptr, TRUE);
}
} // anonymous namespace

// Control IDs
enum {
    // Profile list section
    IDC_PROFILE_LIST = 3001,
    IDC_BTN_NEW = 3002,
    IDC_BTN_DUPLICATE = 3003,
    IDC_BTN_RENAME = 3004,
    IDC_BTN_DELETE = 3005,
    IDC_BTN_SET_DEFAULT = 3006,

    // Profile settings section
    IDC_FONT_COMBO = 3010,
    IDC_FONT_SIZE = 3011,
    IDC_SHELL_EDIT = 3012,
    IDC_SCROLLBACK = 3013,

    // Color swatches
    IDC_COLOR_FOREGROUND = 3030,
    IDC_COLOR_BACKGROUND = 3031,
    IDC_COLOR_CURSOR = 3032,

    // Dialog buttons
    IDC_SAVE_PROFILE = 3040,
    IDC_CLOSE = 3041
};

// Dialog dimensions
constexpr int DIALOG_WIDTH = 550;
constexpr int DIALOG_HEIGHT = 450;

// Custom colors for ChooseColor dialog
static COLORREF s_customColors[16] = {};

ProfileManagerDialog::ProfileManagerDialog(HWND parentWindow, Core::Config* config)
    : m_parentWindow(parentWindow)
    , m_config(config)
{
}

ProfileManagerDialog::~ProfileManagerDialog() = default;

bool ProfileManagerDialog::Show() {
    if (!m_config) {
        spdlog::error("ProfileManagerDialog: No config");
        return false;
    }

    m_profilesChanged = false;
    m_profileNames = m_config->GetProfileNames();
    m_selectedProfile = m_config->GetDefaultProfileName();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        ProfileManagerDialog* dialog = reinterpret_cast<ProfileManagerDialog*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (msg) {
            case WM_CREATE: {
                CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                dialog = reinterpret_cast<ProfileManagerDialog*>(cs->lpCreateParams);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
                dialog->InitializeControls(hwnd);
                dialog->PopulateProfileList(hwnd);
                return 0;
            }
            case WM_COMMAND:
                if (dialog) {
                    int id = LOWORD(wParam);
                    int notification = HIWORD(wParam);

                    switch (id) {
                        case IDC_PROFILE_LIST:
                            if (notification == LBN_SELCHANGE) {
                                dialog->OnProfileSelected(hwnd);
                            }
                            return 0;
                        case IDC_BTN_NEW:
                            dialog->OnNewProfile(hwnd);
                            return 0;
                        case IDC_BTN_DUPLICATE:
                            dialog->OnDuplicateProfile(hwnd);
                            return 0;
                        case IDC_BTN_RENAME:
                            dialog->OnRenameProfile(hwnd);
                            return 0;
                        case IDC_BTN_DELETE:
                            dialog->OnDeleteProfile(hwnd);
                            return 0;
                        case IDC_BTN_SET_DEFAULT:
                            dialog->OnSetDefault(hwnd);
                            return 0;
                        case IDC_SAVE_PROFILE:
                            dialog->OnEditProfile(hwnd);
                            return 0;
                        case IDC_CLOSE:
                            DestroyWindow(hwnd);
                            return 0;
                        case IDC_COLOR_FOREGROUND:
                        case IDC_COLOR_BACKGROUND:
                        case IDC_COLOR_CURSOR: {
                            COLORREF* colorPtrs[] = {&dialog->m_editForeground, &dialog->m_editBackground,
                                                     &dialog->m_editCursor};
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
                        COLORREF colors[] = {dialog->m_editForeground, dialog->m_editBackground,
                                             dialog->m_editCursor};
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
    wc.lpszClassName = L"TerminalDX12ProfileManager";
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
        L"TerminalDX12ProfileManager",
        L"Profile Manager",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, DIALOG_WIDTH, DIALOG_HEIGHT,
        m_parentWindow,
        nullptr,
        GetModuleHandle(nullptr),
        this
    );

    if (!hwnd) {
        spdlog::error("Failed to create profile manager window");
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

    return m_profilesChanged;
}

void ProfileManagerDialog::InitializeControls(HWND hwnd) {
    HINSTANCE hInstance = GetModuleHandle(nullptr);

    // Left side: Profile list
    CreateWindowW(L"STATIC", L"Profiles:", WS_CHILD | WS_VISIBLE,
                  15, 15, 80, 20, hwnd, nullptr, hInstance, nullptr);

    CreateWindowW(L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_BORDER | LBS_NOTIFY,
        15, 38, 180, 200, hwnd, (HMENU)IDC_PROFILE_LIST, hInstance, nullptr);

    // Profile management buttons
    int btnY = 245;
    int btnW = 85;
    int btnH = 25;
    CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE,
                  15, btnY, btnW, btnH, hwnd, (HMENU)IDC_BTN_NEW, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Duplicate", WS_CHILD | WS_VISIBLE,
                  105, btnY, btnW, btnH, hwnd, (HMENU)IDC_BTN_DUPLICATE, hInstance, nullptr);

    btnY += 30;
    CreateWindowW(L"BUTTON", L"Rename", WS_CHILD | WS_VISIBLE,
                  15, btnY, btnW, btnH, hwnd, (HMENU)IDC_BTN_RENAME, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE,
                  105, btnY, btnW, btnH, hwnd, (HMENU)IDC_BTN_DELETE, hInstance, nullptr);

    btnY += 30;
    CreateWindowW(L"BUTTON", L"Set as Default", WS_CHILD | WS_VISIBLE,
                  15, btnY, 175, btnH, hwnd, (HMENU)IDC_BTN_SET_DEFAULT, hInstance, nullptr);

    // Right side: Profile settings
    int rightX = 210;
    int rightW = 310;

    CreateWindowW(L"STATIC", L"Profile Settings:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  rightX, 15, 200, 20, hwnd, nullptr, hInstance, nullptr);

    // Font settings
    int settingsY = 45;
    CreateWindowW(L"STATIC", L"Font:", WS_CHILD | WS_VISIBLE,
                  rightX, settingsY, 60, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        rightX + 70, settingsY - 3, 150, 200, hwnd, (HMENU)IDC_FONT_COMBO, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Size:", WS_CHILD | WS_VISIBLE,
                  rightX + 230, settingsY, 35, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        rightX + 268, settingsY - 3, 45, 22, hwnd, (HMENU)IDC_FONT_SIZE, hInstance, nullptr);

    // Shell
    settingsY += 35;
    CreateWindowW(L"STATIC", L"Shell:", WS_CHILD | WS_VISIBLE,
                  rightX, settingsY, 60, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        rightX + 70, settingsY - 3, 243, 22, hwnd, (HMENU)IDC_SHELL_EDIT, hInstance, nullptr);

    // Scrollback
    settingsY += 35;
    CreateWindowW(L"STATIC", L"Scrollback:", WS_CHILD | WS_VISIBLE,
                  rightX, settingsY, 70, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
        rightX + 70, settingsY - 3, 80, 22, hwnd, (HMENU)IDC_SCROLLBACK, hInstance, nullptr);

    // Colors
    settingsY += 40;
    CreateWindowW(L"STATIC", L"Colors:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                  rightX, settingsY, 200, 20, hwnd, nullptr, hInstance, nullptr);

    settingsY += 25;
    CreateWindowW(L"STATIC", L"Foreground:", WS_CHILD | WS_VISIBLE,
                  rightX, settingsY + 3, 80, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  rightX + 85, settingsY, 28, 28, hwnd, (HMENU)IDC_COLOR_FOREGROUND, hInstance, nullptr);

    CreateWindowW(L"STATIC", L"Background:", WS_CHILD | WS_VISIBLE,
                  rightX + 130, settingsY + 3, 80, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  rightX + 215, settingsY, 28, 28, hwnd, (HMENU)IDC_COLOR_BACKGROUND, hInstance, nullptr);

    settingsY += 35;
    CreateWindowW(L"STATIC", L"Cursor:", WS_CHILD | WS_VISIBLE,
                  rightX, settingsY + 3, 80, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                  rightX + 85, settingsY, 28, 28, hwnd, (HMENU)IDC_COLOR_CURSOR, hInstance, nullptr);

    // Save profile button
    settingsY += 45;
    CreateWindowW(L"BUTTON", L"Save Profile", WS_CHILD | WS_VISIBLE,
                  rightX + 100, settingsY, 120, 28, hwnd, (HMENU)IDC_SAVE_PROFILE, hInstance, nullptr);

    // Close button at bottom
    CreateWindowW(L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE,
                  DIALOG_WIDTH / 2 - 50, DIALOG_HEIGHT - 60, 100, 28, hwnd, (HMENU)IDC_CLOSE, hInstance, nullptr);

    // Populate font combo with common monospace fonts
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    const wchar_t* fonts[] = {L"Consolas", L"Cascadia Code", L"Cascadia Mono", L"Courier New",
                               L"Lucida Console", L"DejaVu Sans Mono", L"Source Code Pro"};
    for (const wchar_t* font : fonts) {
        SendMessageW(fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(font));
    }
}

void ProfileManagerDialog::PopulateProfileList(HWND hwnd) {
    HWND listBox = GetDlgItem(hwnd, IDC_PROFILE_LIST);
    SendMessage(listBox, LB_RESETCONTENT, 0, 0);

    m_profileNames = m_config->GetProfileNames();
    std::string defaultProfile = m_config->GetDefaultProfileName();

    int selectIndex = 0;
    for (size_t i = 0; i < m_profileNames.size(); ++i) {
        const std::string& name = m_profileNames[i];
        std::wstring displayName(name.begin(), name.end());
        if (name == defaultProfile) {
            displayName += L" (default)";
        }
        SendMessageW(listBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displayName.c_str()));
        if (name == m_selectedProfile) {
            selectIndex = static_cast<int>(i);
        }
    }

    SendMessage(listBox, LB_SETCURSEL, selectIndex, 0);
    OnProfileSelected(hwnd);
}

void ProfileManagerDialog::UpdateButtonStates(HWND hwnd) {
    bool hasSelection = !m_selectedProfile.empty();
    bool isDefault = (m_selectedProfile == "Default");
    bool isCurrentDefault = (m_selectedProfile == m_config->GetDefaultProfileName());

    EnableWindow(GetDlgItem(hwnd, IDC_BTN_DUPLICATE), hasSelection);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_RENAME), hasSelection && !isDefault);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_DELETE), hasSelection && !isDefault);
    EnableWindow(GetDlgItem(hwnd, IDC_BTN_SET_DEFAULT), hasSelection && !isCurrentDefault);
    EnableWindow(GetDlgItem(hwnd, IDC_SAVE_PROFILE), hasSelection);
}

void ProfileManagerDialog::OnProfileSelected(HWND hwnd) {
    HWND listBox = GetDlgItem(hwnd, IDC_PROFILE_LIST);
    int sel = static_cast<int>(SendMessage(listBox, LB_GETCURSEL, 0, 0));

    if (sel >= 0 && sel < static_cast<int>(m_profileNames.size())) {
        m_selectedProfile = m_profileNames[sel];
        const Core::Profile* profile = m_config->GetProfile(m_selectedProfile);
        if (profile) {
            LoadProfileSettings(hwnd, *profile);
        }
    } else {
        m_selectedProfile.clear();
    }

    UpdateButtonStates(hwnd);
}

void ProfileManagerDialog::LoadProfileSettings(HWND hwnd, const Core::Profile& profile) {
    // Font family
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    std::wstring fontFamily(profile.font.family.begin(), profile.font.family.end());
    int idx = static_cast<int>(SendMessageW(fontCombo, CB_FINDSTRINGEXACT, -1, reinterpret_cast<LPARAM>(fontFamily.c_str())));
    if (idx == CB_ERR) {
        // Add the font if not in list
        idx = static_cast<int>(SendMessageW(fontCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(fontFamily.c_str())));
    }
    SendMessage(fontCombo, CB_SETCURSEL, idx, 0);

    // Font size
    SetDlgItemInt(hwnd, IDC_FONT_SIZE, profile.font.size, FALSE);

    // Shell
    SetDlgItemTextW(hwnd, IDC_SHELL_EDIT, profile.terminal.shell.c_str());

    // Scrollback
    SetDlgItemInt(hwnd, IDC_SCROLLBACK, profile.terminal.scrollbackLines, FALSE);

    // Colors
    m_editForeground = RGB(profile.colors.foreground.r, profile.colors.foreground.g, profile.colors.foreground.b);
    m_editBackground = RGB(profile.colors.background.r, profile.colors.background.g, profile.colors.background.b);
    m_editCursor = RGB(profile.colors.cursor.r, profile.colors.cursor.g, profile.colors.cursor.b);
    InvalidateColorSwatches(hwnd);
}

void ProfileManagerDialog::SaveProfileSettings(HWND hwnd, Core::Profile& profile) {
    // Font family
    HWND fontCombo = GetDlgItem(hwnd, IDC_FONT_COMBO);
    wchar_t fontBuf[256] = {};
    GetWindowTextW(fontCombo, fontBuf, 256);
    std::wstring wfont(fontBuf);
    profile.font.family = std::string(wfont.begin(), wfont.end());

    // Font size
    profile.font.size = GetDlgItemInt(hwnd, IDC_FONT_SIZE, nullptr, FALSE);
    if (profile.font.size < 6) profile.font.size = 6;
    if (profile.font.size > 72) profile.font.size = 72;

    // Shell
    wchar_t shellBuf[512] = {};
    GetDlgItemTextW(hwnd, IDC_SHELL_EDIT, shellBuf, 512);
    profile.terminal.shell = shellBuf;

    // Scrollback
    profile.terminal.scrollbackLines = GetDlgItemInt(hwnd, IDC_SCROLLBACK, nullptr, FALSE);

    // Colors
    profile.colors.foreground = Core::Color(GetRValue(m_editForeground), GetGValue(m_editForeground), GetBValue(m_editForeground));
    profile.colors.background = Core::Color(GetRValue(m_editBackground), GetGValue(m_editBackground), GetBValue(m_editBackground));
    profile.colors.cursor = Core::Color(GetRValue(m_editCursor), GetGValue(m_editCursor), GetBValue(m_editCursor));
}

void ProfileManagerDialog::OnNewProfile(HWND hwnd) {
    // Simple input dialog for profile name
    wchar_t nameBuf[256] = L"New Profile";

    // Use a simple message box approach - ask for name
    // In a full implementation, you'd create a proper input dialog
    // For now, generate a unique name
    int counter = 1;
    std::string newName = "New Profile";
    while (m_config->GetProfile(newName) != nullptr) {
        newName = "New Profile " + std::to_string(++counter);
    }

    // Create profile from default settings
    Core::Profile newProfile(newName);
    newProfile.font = m_config->GetFont();
    newProfile.colors = m_config->GetColors();
    newProfile.terminal = m_config->GetTerminal();

    m_config->AddProfile(newProfile);
    m_profilesChanged = true;
    m_selectedProfile = newName;

    PopulateProfileList(hwnd);
    spdlog::info("Created new profile: {}", newName);
}

void ProfileManagerDialog::OnDuplicateProfile(HWND hwnd) {
    if (m_selectedProfile.empty()) return;

    const Core::Profile* source = m_config->GetProfile(m_selectedProfile);
    if (!source) return;

    // Generate unique name
    int counter = 1;
    std::string newName = m_selectedProfile + " Copy";
    while (m_config->GetProfile(newName) != nullptr) {
        newName = m_selectedProfile + " Copy " + std::to_string(++counter);
    }

    Core::Profile newProfile = *source;
    newProfile.name = newName;
    m_config->AddProfile(newProfile);
    m_profilesChanged = true;
    m_selectedProfile = newName;

    PopulateProfileList(hwnd);
    spdlog::info("Duplicated profile '{}' as '{}'", source->name, newName);
}

void ProfileManagerDialog::OnRenameProfile(HWND hwnd) {
    if (m_selectedProfile.empty() || m_selectedProfile == "Default") return;

    // For simplicity, append " Renamed" - a full implementation would show an input dialog
    std::string newName = m_selectedProfile + " Renamed";
    int counter = 1;
    while (m_config->GetProfile(newName) != nullptr) {
        newName = m_selectedProfile + " Renamed " + std::to_string(++counter);
    }

    Core::Profile* profile = m_config->GetProfileMut(m_selectedProfile);
    if (profile) {
        std::string oldName = profile->name;

        // Create new profile with new name, remove old
        Core::Profile renamed = *profile;
        renamed.name = newName;
        m_config->RemoveProfile(oldName);
        m_config->AddProfile(renamed);

        m_profilesChanged = true;
        m_selectedProfile = newName;
        PopulateProfileList(hwnd);
        spdlog::info("Renamed profile '{}' to '{}'", oldName, newName);
    }
}

void ProfileManagerDialog::OnDeleteProfile(HWND hwnd) {
    if (m_selectedProfile.empty() || m_selectedProfile == "Default") return;

    int result = MessageBoxW(hwnd,
        (L"Delete profile '" + std::wstring(m_selectedProfile.begin(), m_selectedProfile.end()) + L"'?").c_str(),
        L"Confirm Delete",
        MB_YESNO | MB_ICONWARNING);

    if (result == IDYES) {
        std::string deletedName = m_selectedProfile;
        m_config->RemoveProfile(m_selectedProfile);
        m_profilesChanged = true;
        m_selectedProfile = "Default";
        PopulateProfileList(hwnd);
        spdlog::info("Deleted profile: {}", deletedName);
    }
}

void ProfileManagerDialog::OnSetDefault(HWND hwnd) {
    if (m_selectedProfile.empty()) return;

    m_config->SetDefaultProfileName(m_selectedProfile);
    m_profilesChanged = true;
    PopulateProfileList(hwnd);
    spdlog::info("Set default profile to: {}", m_selectedProfile);
}

void ProfileManagerDialog::OnEditProfile(HWND hwnd) {
    if (m_selectedProfile.empty()) return;

    Core::Profile* profile = m_config->GetProfileMut(m_selectedProfile);
    if (profile) {
        SaveProfileSettings(hwnd, *profile);
        m_profilesChanged = true;

        // Save config to disk
        m_config->Save(m_config->GetDefaultConfigPath());

        if (m_changeCallback) {
            m_changeCallback();
        }

        MessageBoxW(hwnd, L"Profile saved.", L"Profile Manager", MB_OK | MB_ICONINFORMATION);
        spdlog::info("Saved profile: {}", m_selectedProfile);
    }
}

void ProfileManagerDialog::ShowColorPicker(HWND hwnd, COLORREF& color, int controlId) {
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
