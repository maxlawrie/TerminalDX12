// test_tab_manager.cpp - Contract tests for TabManager
// Tests TabManager functionality with real Tab instances (requires ConPTY)
//
// These are contract-style tests that verify TabManager behavior with actual
// shell processes. They require Windows 10 1809+ for ConPTY support.

#include <gtest/gtest.h>
#include "ui/TabManager.h"
#include "ui/Tab.h"
#include <thread>
#include <chrono>
#include <atomic>

namespace TerminalDX12::UI::Tests {

// ============================================================================
// Test Constants
// ============================================================================

// Use cmd.exe as it's always available on Windows
const std::wstring TEST_SHELL = L"cmd.exe";
const int TEST_COLS = 80;
const int TEST_ROWS = 24;
const int TEST_SCROLLBACK = 1000;

// Timeout for shell operations
const auto SHELL_STARTUP_TIMEOUT = std::chrono::milliseconds(2000);
const auto SHELL_SHUTDOWN_TIMEOUT = std::chrono::milliseconds(1000);

// ============================================================================
// Test Fixture
// ============================================================================

class TabManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager = std::make_unique<TabManager>();
        tabChangedCount = 0;
        activeTabChangedCount = 0;
        lastActiveTab = nullptr;
        processExitCount = 0;
        lastExitTabId = -1;
        lastExitCode = -1;
    }

    void TearDown() override {
        // Clean up all tabs
        while (manager->HasTabs()) {
            manager->CloseActiveTab();
        }
        manager.reset();
    }

    // Helper to create a tab and wait for it to start
    Tab* CreateTabAndWait() {
        Tab* tab = manager->CreateTab(TEST_SHELL, TEST_COLS, TEST_ROWS, TEST_SCROLLBACK);
        if (tab) {
            // Give shell time to initialize
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return tab;
    }

    // Setup callbacks for tracking
    void SetupCallbacks() {
        manager->SetTabChangedCallback([this]() {
            tabChangedCount++;
        });

        manager->SetActiveTabChangedCallback([this](Tab* tab) {
            activeTabChangedCount++;
            lastActiveTab = tab;
        });

        manager->SetProcessExitCallback([this](int tabId, int exitCode) {
            processExitCount++;
            lastExitTabId = tabId;
            lastExitCode = exitCode;
        });
    }

    std::unique_ptr<TabManager> manager;

    // Callback tracking
    std::atomic<int> tabChangedCount{0};
    std::atomic<int> activeTabChangedCount{0};
    Tab* lastActiveTab = nullptr;
    std::atomic<int> processExitCount{0};
    std::atomic<int> lastExitTabId{-1};
    std::atomic<int> lastExitCode{-1};
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(TabManagerTest, DefaultConstruction) {
    // Given: A newly constructed TabManager
    // Then: It should have no tabs
    EXPECT_EQ(manager->GetTabCount(), 0);
    EXPECT_FALSE(manager->HasTabs());
    EXPECT_EQ(manager->GetActiveTabIndex(), -1);
    EXPECT_EQ(manager->GetActiveTab(), nullptr);
}

TEST_F(TabManagerTest, GetTabsReturnsEmptyVector) {
    // Given: A newly constructed TabManager
    // When: Getting all tabs
    const auto& tabs = manager->GetTabs();

    // Then: The vector should be empty
    EXPECT_TRUE(tabs.empty());
}

// ============================================================================
// Tab Creation Tests
// ============================================================================

TEST_F(TabManagerTest, CreateFirstTab) {
    // Given: An empty TabManager
    // When: Creating the first tab
    Tab* tab = CreateTabAndWait();

    // Then: Tab should be created and active
    ASSERT_NE(tab, nullptr);
    EXPECT_TRUE(tab->IsRunning());
    EXPECT_EQ(manager->GetTabCount(), 1);
    EXPECT_TRUE(manager->HasTabs());
    EXPECT_EQ(manager->GetActiveTabIndex(), 0);
    EXPECT_EQ(manager->GetActiveTab(), tab);
}

TEST_F(TabManagerTest, CreateMultipleTabs) {
    // Given: An empty TabManager
    // When: Creating three tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();

    // Then: All tabs should exist
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);
    EXPECT_EQ(manager->GetTabCount(), 3);

    // And: First tab should still be active
    EXPECT_EQ(manager->GetActiveTabIndex(), 0);
    EXPECT_EQ(manager->GetActiveTab(), tab1);
}

TEST_F(TabManagerTest, TabsHaveUniqueIds) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();

    // Then: Each tab should have a unique ID
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);
    EXPECT_NE(tab1->GetId(), tab2->GetId());
    EXPECT_NE(tab2->GetId(), tab3->GetId());
    EXPECT_NE(tab1->GetId(), tab3->GetId());
}

TEST_F(TabManagerTest, TabCreationCallbackFires) {
    // Given: A TabManager with callbacks set up
    Tab* createdTab = nullptr;
    manager->SetTabCreatedCallback([&createdTab](Tab* tab) {
        createdTab = tab;
    });

    // When: Creating a tab
    Tab* tab = CreateTabAndWait();

    // Then: The callback should have been called with the new tab
    EXPECT_EQ(createdTab, tab);
}

TEST_F(TabManagerTest, TabChangedCallbackFiresOnCreate) {
    // Given: A TabManager with callbacks
    SetupCallbacks();

    // When: Creating a tab
    CreateTabAndWait();

    // Then: TabChanged callback should fire
    EXPECT_GE(tabChangedCount.load(), 1);
}

// ============================================================================
// Tab Retrieval Tests
// ============================================================================

TEST_F(TabManagerTest, GetTabById) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);

    // When: Getting tabs by ID
    Tab* found1 = manager->GetTab(tab1->GetId());
    Tab* found2 = manager->GetTab(tab2->GetId());

    // Then: Correct tabs should be returned
    EXPECT_EQ(found1, tab1);
    EXPECT_EQ(found2, tab2);
}

TEST_F(TabManagerTest, GetTabByInvalidIdReturnsNull) {
    // Given: A tab exists
    CreateTabAndWait();

    // When: Getting a tab with invalid ID
    Tab* notFound = manager->GetTab(99999);

    // Then: Should return nullptr
    EXPECT_EQ(notFound, nullptr);
}

TEST_F(TabManagerTest, GetTabsReturnsAllTabs) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);

    // When: Getting all tabs
    const auto& tabs = manager->GetTabs();

    // Then: All tabs should be in the vector
    EXPECT_EQ(tabs.size(), 2);
    EXPECT_EQ(tabs[0].get(), tab1);
    EXPECT_EQ(tabs[1].get(), tab2);
}

// ============================================================================
// Tab Switching Tests
// ============================================================================

TEST_F(TabManagerTest, SwitchToTabById) {
    // Given: Multiple tabs with the first active
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);
    EXPECT_EQ(manager->GetActiveTab(), tab1);

    // When: Switching to tab2
    manager->SwitchToTab(tab2->GetId());

    // Then: Tab2 should be active
    EXPECT_EQ(manager->GetActiveTab(), tab2);
    EXPECT_EQ(manager->GetActiveTabIndex(), 1);
}

TEST_F(TabManagerTest, SwitchToInvalidTabDoesNothing) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    EXPECT_EQ(manager->GetActiveTab(), tab1);

    // When: Switching to invalid tab ID
    manager->SwitchToTab(99999);

    // Then: Active tab should not change
    EXPECT_EQ(manager->GetActiveTab(), tab1);
    EXPECT_EQ(manager->GetActiveTabIndex(), 0);
}

TEST_F(TabManagerTest, NextTabCycles) {
    // Given: Three tabs with first active
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);

    // When: Cycling through with NextTab
    manager->NextTab();
    EXPECT_EQ(manager->GetActiveTab(), tab2);

    manager->NextTab();
    EXPECT_EQ(manager->GetActiveTab(), tab3);

    manager->NextTab();
    // Should wrap to first tab
    EXPECT_EQ(manager->GetActiveTab(), tab1);
}

TEST_F(TabManagerTest, PreviousTabCycles) {
    // Given: Three tabs with first active
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);

    // When: Going to previous (should wrap to last)
    manager->PreviousTab();
    EXPECT_EQ(manager->GetActiveTab(), tab3);

    manager->PreviousTab();
    EXPECT_EQ(manager->GetActiveTab(), tab2);

    manager->PreviousTab();
    EXPECT_EQ(manager->GetActiveTab(), tab1);
}

TEST_F(TabManagerTest, NextTabWithSingleTabDoesNothing) {
    // Given: Only one tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // When: Calling NextTab
    manager->NextTab();

    // Then: Same tab should be active
    EXPECT_EQ(manager->GetActiveTab(), tab);
}

TEST_F(TabManagerTest, PreviousTabWithSingleTabDoesNothing) {
    // Given: Only one tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // When: Calling PreviousTab
    manager->PreviousTab();

    // Then: Same tab should be active
    EXPECT_EQ(manager->GetActiveTab(), tab);
}

TEST_F(TabManagerTest, SwitchingClearsActivityIndicator) {
    // Given: Two tabs, second has activity
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    tab2->SetActivity(true);
    EXPECT_TRUE(tab2->HasActivity());

    // When: Switching to tab2
    manager->SwitchToTab(tab2->GetId());

    // Then: Activity should be cleared
    EXPECT_FALSE(tab2->HasActivity());
}

TEST_F(TabManagerTest, ActiveTabChangedCallbackFires) {
    // Given: Tabs with callback
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    SetupCallbacks();
    activeTabChangedCount = 0;

    // When: Switching tabs
    manager->SwitchToTab(tab2->GetId());

    // Then: Callback should fire with new active tab
    EXPECT_GE(activeTabChangedCount.load(), 1);
    EXPECT_EQ(lastActiveTab, tab2);
}

// ============================================================================
// Tab Closing Tests
// ============================================================================

TEST_F(TabManagerTest, CloseTabById) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    int tab1Id = tab1->GetId();

    // When: Closing first tab by ID
    manager->CloseTab(tab1Id);

    // Then: Only second tab should remain
    EXPECT_EQ(manager->GetTabCount(), 1);
    EXPECT_EQ(manager->GetActiveTab(), tab2);
    EXPECT_EQ(manager->GetTab(tab1Id), nullptr);
}

TEST_F(TabManagerTest, CloseActiveTab) {
    // Given: Multiple tabs
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    EXPECT_EQ(manager->GetActiveTab(), tab1);

    // When: Closing active tab
    manager->CloseActiveTab();

    // Then: Second tab should now be active
    EXPECT_EQ(manager->GetTabCount(), 1);
    EXPECT_EQ(manager->GetActiveTab(), tab2);
}

TEST_F(TabManagerTest, CloseLastTab) {
    // Given: Only one tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // When: Closing the only tab
    manager->CloseActiveTab();

    // Then: No tabs should remain
    EXPECT_EQ(manager->GetTabCount(), 0);
    EXPECT_FALSE(manager->HasTabs());
    EXPECT_EQ(manager->GetActiveTabIndex(), -1);
    EXPECT_EQ(manager->GetActiveTab(), nullptr);
}

TEST_F(TabManagerTest, CloseMiddleTabAdjustsActiveIndex) {
    // Given: Three tabs with second active
    Tab* tab1 = CreateTabAndWait();
    Tab* tab2 = CreateTabAndWait();
    Tab* tab3 = CreateTabAndWait();
    ASSERT_NE(tab1, nullptr);
    ASSERT_NE(tab2, nullptr);
    ASSERT_NE(tab3, nullptr);
    manager->SwitchToTab(tab2->GetId());
    EXPECT_EQ(manager->GetActiveTabIndex(), 1);

    // When: Closing the middle (active) tab
    manager->CloseActiveTab();

    // Then: First tab should become active (index adjusts down)
    EXPECT_EQ(manager->GetTabCount(), 2);
    EXPECT_EQ(manager->GetActiveTab(), tab1);
    EXPECT_EQ(manager->GetActiveTabIndex(), 0);
}

TEST_F(TabManagerTest, CloseInvalidTabDoesNothing) {
    // Given: One tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // When: Closing non-existent tab
    manager->CloseTab(99999);

    // Then: Tab should still exist
    EXPECT_EQ(manager->GetTabCount(), 1);
    EXPECT_EQ(manager->GetActiveTab(), tab);
}

TEST_F(TabManagerTest, TabChangedCallbackFiresOnClose) {
    // Given: Tabs with callback
    CreateTabAndWait();
    CreateTabAndWait();
    SetupCallbacks();
    tabChangedCount = 0;

    // When: Closing a tab
    manager->CloseActiveTab();

    // Then: Callback should fire
    EXPECT_GE(tabChangedCount.load(), 1);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(TabManagerTest, CloseActiveTabWithNoTabsDoesNotCrash) {
    // Given: No tabs
    EXPECT_FALSE(manager->HasTabs());

    // When: Trying to close active tab
    // Then: Should not crash
    EXPECT_NO_THROW(manager->CloseActiveTab());
}

TEST_F(TabManagerTest, NextTabWithNoTabsDoesNotCrash) {
    // Given: No tabs
    EXPECT_FALSE(manager->HasTabs());

    // When/Then: Should not crash
    EXPECT_NO_THROW(manager->NextTab());
}

TEST_F(TabManagerTest, PreviousTabWithNoTabsDoesNotCrash) {
    // Given: No tabs
    EXPECT_FALSE(manager->HasTabs());

    // When/Then: Should not crash
    EXPECT_NO_THROW(manager->PreviousTab());
}

// ============================================================================
// Tab Properties Tests
// ============================================================================

TEST_F(TabManagerTest, TabHasDefaultTitle) {
    // Given: A new tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // Then: Should have a title (shell name or "Terminal")
    EXPECT_FALSE(tab->GetTitle().empty());
}

TEST_F(TabManagerTest, TabTitleCanBeSet) {
    // Given: A tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // When: Setting custom title
    tab->SetTitle(L"My Custom Tab");

    // Then: Title should be updated
    EXPECT_EQ(tab->GetTitle(), L"My Custom Tab");
}

TEST_F(TabManagerTest, TabActivityIndicator) {
    // Given: A tab with no activity
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // Initially may have activity from shell startup
    tab->ClearActivity();
    EXPECT_FALSE(tab->HasActivity());

    // When: Setting activity
    tab->SetActivity(true);

    // Then: Activity flag should be set
    EXPECT_TRUE(tab->HasActivity());

    // When: Clearing activity
    tab->ClearActivity();

    // Then: Activity flag should be cleared
    EXPECT_FALSE(tab->HasActivity());
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(TabManagerTest, RapidTabCreationAndDeletion) {
    // Given: Empty manager
    // When: Rapidly creating and deleting tabs
    for (int i = 0; i < 5; i++) {
        Tab* tab = CreateTabAndWait();
        ASSERT_NE(tab, nullptr);
        EXPECT_TRUE(tab->IsRunning());
    }

    EXPECT_EQ(manager->GetTabCount(), 5);

    // Close all tabs
    while (manager->HasTabs()) {
        manager->CloseActiveTab();
    }

    EXPECT_EQ(manager->GetTabCount(), 0);
}

TEST_F(TabManagerTest, TabHasScreenBuffer) {
    // Given: A tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // Then: Should have a screen buffer
    EXPECT_NE(tab->GetScreenBuffer(), nullptr);
}

TEST_F(TabManagerTest, TabHasVTParser) {
    // Given: A tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // Then: Should have a VT parser
    EXPECT_NE(tab->GetVTParser(), nullptr);
}

TEST_F(TabManagerTest, TabHasTerminalSession) {
    // Given: A tab
    Tab* tab = CreateTabAndWait();
    ASSERT_NE(tab, nullptr);

    // Then: Should have a terminal session
    EXPECT_NE(tab->GetTerminal(), nullptr);
}

} // namespace TerminalDX12::UI::Tests
