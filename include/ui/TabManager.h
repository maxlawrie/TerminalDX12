#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

namespace TerminalDX12::UI {

class Tab;

// Manages multiple terminal tabs
class TabManager {
public:
    TabManager();
    ~TabManager();

    // Create a new tab
    Tab* CreateTab(const std::wstring& shell, int cols, int rows, int scrollbackLines = 10000);

    // Close a tab by ID
    void CloseTab(int tabId);

    // Close the active tab
    void CloseActiveTab();

    // Switch to a specific tab
    void SwitchToTab(int tabId);

    // Switch to next/previous tab
    void NextTab();
    void PreviousTab();

    // Get active tab
    Tab* GetActiveTab();
    const Tab* GetActiveTab() const;

    // Get tab by ID
    Tab* GetTab(int tabId);

    // Get all tabs
    const std::vector<std::unique_ptr<Tab>>& GetTabs() const { return m_tabs; }

    // Get active tab index
    int GetActiveTabIndex() const { return m_activeTabIndex; }

    // Get number of tabs
    size_t GetTabCount() const { return m_tabs.size(); }

    // Check if any tabs exist
    bool HasTabs() const { return !m_tabs.empty(); }

    // Set callback for when tab count changes (for UI updates)
    void SetTabChangedCallback(std::function<void()> callback) {
        m_tabChangedCallback = callback;
    }

    // Set callback for when active tab changes
    void SetActiveTabChangedCallback(std::function<void(Tab*)> callback) {
        m_activeTabChangedCallback = callback;
    }

    // Set callback for when a tab's process exits (passes tab ID and exit code)
    void SetProcessExitCallback(std::function<void(int tabId, int exitCode)> callback) {
        m_processExitCallback = callback;
    }

    // Set callback for when a new tab is created (to configure callbacks)
    void SetTabCreatedCallback(std::function<void(Tab*)> callback) {
        m_tabCreatedCallback = callback;
    }

private:
    std::vector<std::unique_ptr<Tab>> m_tabs;
    int m_activeTabIndex = -1;
    int m_nextTabId = 1;

    std::function<void()> m_tabChangedCallback;
    std::function<void(Tab*)> m_activeTabChangedCallback;
    std::function<void(int, int)> m_processExitCallback;
    std::function<void(Tab*)> m_tabCreatedCallback;
};

} // namespace TerminalDX12::UI
