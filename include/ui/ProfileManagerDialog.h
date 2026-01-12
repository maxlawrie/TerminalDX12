#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <vector>

namespace TerminalDX12 {
namespace Core {
    class Config;
    struct Profile;
}
namespace UI {

/**
 * @brief Dialog for managing terminal profiles.
 *
 * Allows users to:
 * - View list of all profiles
 * - Create new profiles (from defaults or copy existing)
 * - Rename profiles
 * - Delete profiles (except Default)
 * - Set default profile
 * - Edit profile settings (font, colors, terminal options)
 */
class ProfileManagerDialog {
public:
    ProfileManagerDialog(HWND parentWindow, Core::Config* config);
    ~ProfileManagerDialog();

    /**
     * @brief Show the dialog (modal).
     * @return true if profiles were modified.
     */
    bool Show();

    /**
     * @brief Set callback for when profiles change.
     */
    void SetChangeCallback(std::function<void()> callback) { m_changeCallback = callback; }

private:
    void InitializeControls(HWND hwnd);
    void PopulateProfileList(HWND hwnd);
    void UpdateButtonStates(HWND hwnd);
    void OnProfileSelected(HWND hwnd);
    void OnNewProfile(HWND hwnd);
    void OnDuplicateProfile(HWND hwnd);
    void OnRenameProfile(HWND hwnd);
    void OnDeleteProfile(HWND hwnd);
    void OnSetDefault(HWND hwnd);
    void OnEditProfile(HWND hwnd);
    void ShowColorPicker(HWND hwnd, COLORREF& color, int controlId);

    // Profile editing helpers
    void LoadProfileSettings(HWND hwnd, const Core::Profile& profile);
    void SaveProfileSettings(HWND hwnd, Core::Profile& profile);

    HWND m_parentWindow;
    Core::Config* m_config;
    std::function<void()> m_changeCallback;
    bool m_profilesChanged = false;

    // Currently selected profile
    std::string m_selectedProfile;
    std::vector<std::string> m_profileNames;

    // Editing state for current profile
    COLORREF m_editForeground = RGB(204, 204, 204);
    COLORREF m_editBackground = RGB(12, 12, 12);
    COLORREF m_editCursor = RGB(0, 255, 0);
};

} // namespace UI
} // namespace TerminalDX12
