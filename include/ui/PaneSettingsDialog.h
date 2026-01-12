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

class TerminalSession;

// Pane settings dialog for configuring per-pane appearance and behavior
class PaneSettingsDialog {
public:
    PaneSettingsDialog(HWND parentWindow, Core::Config* config, TerminalSession* session);
    ~PaneSettingsDialog();

    // Show the dialog (modal)
    // Returns true if settings were changed and applied
    bool Show();

    // Callback for when settings change (for live preview)
    void SetApplyCallback(std::function<void()> callback) {
        m_applyCallback = callback;
    }

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void InitializeControls(HWND hwnd);
    void LoadCurrentSettings(HWND hwnd);
    void PopulateProfileList(HWND hwnd);
    void OnProfileSelected(HWND hwnd);
    void ApplySettings(HWND hwnd);
    void ShowColorPicker(HWND hwnd, COLORREF& color, int controlId);

    HWND m_parentWindow;
    Core::Config* m_config;
    TerminalSession* m_session;
    std::function<void()> m_applyCallback;
    bool m_settingsChanged = false;

    // Current state
    std::string m_selectedProfile;
    bool m_useCustomSettings = false;

    // Color swatch state (for custom overrides)
    COLORREF m_foregroundColor = RGB(204, 204, 204);
    COLORREF m_backgroundColor = RGB(12, 12, 12);
    COLORREF m_cursorColor = RGB(0, 255, 0);

    // Profile names cache
    std::vector<std::string> m_profileNames;
};

} // namespace UI
} // namespace TerminalDX12
