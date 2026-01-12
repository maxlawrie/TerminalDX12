#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace TerminalDX12::Core {
    class Config;
}

namespace TerminalDX12::UI {

class Tab;
class TerminalSession;

/// @brief Manages multiple terminal tabs (each tab has its own pane tree)
class TabManager {
public:
    TabManager();
    ~TabManager();

    /// @brief Set config for profile support
    void SetConfig(Core::Config* config) { m_config = config; }

    /// @brief Create a new tab with an initial terminal session
    /// @param shell Shell command to run
    /// @param cols Terminal columns
    /// @param rows Terminal rows
    /// @param scrollbackLines Scrollback buffer size
    /// @return The created tab, or nullptr on failure
    Tab* CreateTab(const std::wstring& shell, int cols, int rows, int scrollbackLines = 10000);

    /// @brief Close a tab by ID
    void CloseTab(int tabId);

    /// @brief Close the active tab
    void CloseActiveTab();

    /// @brief Switch to a specific tab
    void SwitchToTab(int tabId);

    /// @brief Switch to next/previous tab
    void NextTab();
    void PreviousTab();

    /// @brief Get active tab
    Tab* GetActiveTab();
    const Tab* GetActiveTab() const;

    /// @brief Get tab by ID
    Tab* GetTab(int tabId);

    /// @brief Get all tabs
    const std::vector<std::unique_ptr<Tab>>& GetTabs() const { return m_tabs; }

    /// @brief Get active tab index
    int GetActiveTabIndex() const { return m_activeTabIndex; }

    /// @brief Get number of tabs
    size_t GetTabCount() const { return m_tabs.size(); }

    /// @brief Check if any tabs exist
    bool HasTabs() const { return !m_tabs.empty(); }

    /// @brief Set callback for when tab count changes (for UI updates)
    void SetTabChangedCallback(std::function<void()> callback) {
        m_tabChangedCallback = callback;
    }

    /// @brief Set callback for when active tab changes
    void SetActiveTabChangedCallback(std::function<void(Tab*)> callback) {
        m_activeTabChangedCallback = callback;
    }

    /// @brief Set callback for when any session's process exits
    void SetProcessExitCallback(std::function<void(int tabId, int sessionId, int exitCode)> callback) {
        m_processExitCallback = callback;
    }

    /// @brief Set callback for when a new session is created (to configure callbacks)
    void SetSessionCreatedCallback(std::function<void(TerminalSession*)> callback) {
        m_sessionCreatedCallback = callback;
    }

private:
    Core::Config* m_config = nullptr;
    std::vector<std::unique_ptr<Tab>> m_tabs;
    int m_activeTabIndex = -1;
    int m_nextTabId = 1;

    std::function<void()> m_tabChangedCallback;
    std::function<void(Tab*)> m_activeTabChangedCallback;
    std::function<void(int, int, int)> m_processExitCallback;
    std::function<void(TerminalSession*)> m_sessionCreatedCallback;
};

} // namespace TerminalDX12::UI
