#include "ui/TabManager.h"
#include "ui/Tab.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

TabManager::TabManager() = default;

TabManager::~TabManager() {
    // Tabs will be cleaned up automatically via unique_ptr
    m_tabs.clear();
}

Tab* TabManager::CreateTab(const std::wstring& shell, int cols, int rows, int scrollbackLines) {
    int tabId = m_nextTabId++;

    auto tab = std::make_unique<Tab>(tabId, cols, rows, scrollbackLines);

    if (!tab->Start(shell)) {
        spdlog::error("TabManager: Failed to create tab {}", tabId);
        return nullptr;
    }

    Tab* tabPtr = tab.get();
    m_tabs.push_back(std::move(tab));

    // If this is the first tab, make it active
    if (m_activeTabIndex < 0) {
        m_activeTabIndex = 0;
    }

    spdlog::info("TabManager: Created tab {} (total: {})", tabId, m_tabs.size());

    // Notify listeners
    if (m_tabChangedCallback) {
        m_tabChangedCallback();
    }

    return tabPtr;
}

void TabManager::CloseTab(int tabId) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [tabId](const std::unique_ptr<Tab>& tab) {
            return tab->GetId() == tabId;
        });

    if (it == m_tabs.end()) {
        spdlog::warn("TabManager: Tab {} not found", tabId);
        return;
    }

    int closedIndex = static_cast<int>(std::distance(m_tabs.begin(), it));

    // Stop the tab
    (*it)->Stop();

    // Remove the tab
    m_tabs.erase(it);

    spdlog::info("TabManager: Closed tab {} (remaining: {})", tabId, m_tabs.size());

    // Adjust active tab index
    if (m_tabs.empty()) {
        m_activeTabIndex = -1;
    } else if (closedIndex <= m_activeTabIndex) {
        // If we closed the active tab or one before it, adjust
        m_activeTabIndex = std::max(0, m_activeTabIndex - 1);
        if (m_activeTabIndex >= static_cast<int>(m_tabs.size())) {
            m_activeTabIndex = static_cast<int>(m_tabs.size()) - 1;
        }
    }

    // Notify listeners
    if (m_tabChangedCallback) {
        m_tabChangedCallback();
    }

    if (m_activeTabChangedCallback && !m_tabs.empty()) {
        m_activeTabChangedCallback(m_tabs[m_activeTabIndex].get());
    }
}

void TabManager::CloseActiveTab() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        CloseTab(m_tabs[m_activeTabIndex]->GetId());
    }
}

void TabManager::SwitchToTab(int tabId) {
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i]->GetId() == tabId) {
            if (m_activeTabIndex != static_cast<int>(i)) {
                m_activeTabIndex = static_cast<int>(i);

                // Clear activity indicator when switching to tab
                m_tabs[i]->ClearActivity();

                spdlog::debug("TabManager: Switched to tab {}", tabId);

                if (m_activeTabChangedCallback) {
                    m_activeTabChangedCallback(m_tabs[i].get());
                }
            }
            return;
        }
    }

    spdlog::warn("TabManager: Tab {} not found for switch", tabId);
}

void TabManager::NextTab() {
    if (m_tabs.size() <= 1) {
        return;
    }

    int newIndex = (m_activeTabIndex + 1) % static_cast<int>(m_tabs.size());
    SwitchToTab(m_tabs[newIndex]->GetId());
}

void TabManager::PreviousTab() {
    if (m_tabs.size() <= 1) {
        return;
    }

    int newIndex = m_activeTabIndex - 1;
    if (newIndex < 0) {
        newIndex = static_cast<int>(m_tabs.size()) - 1;
    }
    SwitchToTab(m_tabs[newIndex]->GetId());
}

Tab* TabManager::GetActiveTab() {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[m_activeTabIndex].get();
    }
    return nullptr;
}

const Tab* TabManager::GetActiveTab() const {
    if (m_activeTabIndex >= 0 && m_activeTabIndex < static_cast<int>(m_tabs.size())) {
        return m_tabs[m_activeTabIndex].get();
    }
    return nullptr;
}

Tab* TabManager::GetTab(int tabId) {
    for (auto& tab : m_tabs) {
        if (tab->GetId() == tabId) {
            return tab.get();
        }
    }
    return nullptr;
}

} // namespace TerminalDX12::UI
