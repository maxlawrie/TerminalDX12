#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <memory>

namespace TerminalDX12 {

namespace Core { class Config; }

namespace UI {

// Settings dialog for configuring terminal options
class SettingsDialog {
public:
    SettingsDialog(HWND parentWindow, Core::Config* config);
    ~SettingsDialog();

    // Show the dialog (modal)
    // Returns true if settings were changed and applied
    bool Show();

    // Callback for when settings change (for live preview)
    void SetPreviewCallback(std::function<void()> callback) {
        m_previewCallback = callback;
    }

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    INT_PTR HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void InitializeControls(HWND hwnd);
    void LoadCurrentSettings(HWND hwnd);
    void ApplySettings(HWND hwnd);
    void PreviewSettings(HWND hwnd);
    void RestoreSettings();

    // Tab page initialization
    void InitFontTab(HWND hwnd);
    void InitColorsTab(HWND hwnd);
    void InitTerminalTab(HWND hwnd);

    // Control value readers
    std::wstring GetEditText(HWND hwnd, int controlId);
    int GetEditInt(HWND hwnd, int controlId, int defaultValue);
    bool GetCheckState(HWND hwnd, int controlId);

    // Color picker helper
    void ShowColorPicker(HWND hwnd, COLORREF& color, int controlId);

    // Reset to defaults
    void ResetToDefaults(HWND hwnd);

    HWND m_parentWindow;
    Core::Config* m_config;
    std::unique_ptr<Core::Config> m_originalConfig;  // Backup for cancel
    std::function<void()> m_previewCallback;
    bool m_settingsChanged = false;

    // Color swatch state
    COLORREF m_foregroundColor = RGB(204, 204, 204);
    COLORREF m_backgroundColor = RGB(12, 12, 12);
    COLORREF m_cursorColor = RGB(0, 255, 0);
    COLORREF m_selectionColor = RGB(51, 153, 255);
};

} // namespace UI
} // namespace TerminalDX12
