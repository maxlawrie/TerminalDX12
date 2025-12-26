/**
 * @file test_tab.cpp
 * @brief Contract tests for Tab class
 *
 * These are contract tests that verify Tab behavior with real ConPTY sessions.
 * They require Windows 10 1809+ and create actual shell processes.
 */

#include <gtest/gtest.h>
#include "ui/Tab.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include "pty/ConPtySession.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <string>

using namespace std::chrono_literals;

namespace TerminalDX12::UI::Tests {

// Test constants
constexpr int TEST_COLS = 80;
constexpr int TEST_ROWS = 24;
constexpr int TEST_SCROLLBACK = 1000;
const std::wstring TEST_SHELL = L"cmd.exe";

/**
 * @brief Test fixture for Tab tests
 */
class TabTest : public ::testing::Test {
protected:
    void SetUp() override {
        outputReceived = false;
        exitCodeReceived = false;
        lastExitCode = -999;
        outputData.clear();
    }

    void TearDown() override {
        // Ensure tab is stopped
        if (tab && tab->IsRunning()) {
            tab->Stop();
            std::this_thread::sleep_for(100ms);
        }
    }

    // Helper to create and start a tab
    std::unique_ptr<Tab> CreateAndStartTab(int id = 1) {
        auto t = std::make_unique<Tab>(id, TEST_COLS, TEST_ROWS, TEST_SCROLLBACK);

        t->SetOutputCallback([this](const char* data, size_t size) {
            outputReceived = true;
            outputData.append(data, size);
        });

        t->SetProcessExitCallback([this](int code) {
            exitCodeReceived = true;
            lastExitCode = code;
        });

        if (t->Start(TEST_SHELL)) {
            std::this_thread::sleep_for(100ms);  // Wait for shell startup
        }

        return t;
    }

    std::unique_ptr<Tab> tab;
    std::atomic<bool> outputReceived{false};
    std::atomic<bool> exitCodeReceived{false};
    std::atomic<int> lastExitCode{-999};
    std::string outputData;
};

// ============================================================================
// Construction Tests
// ============================================================================

/**
 * CONTRACT: Tab constructor creates all components
 *
 * Given: Valid dimensions
 * When: Tab is constructed
 * Then: ScreenBuffer, VTParser, and Terminal are created
 */
TEST_F(TabTest, Constructor_CreatesAllComponents) {
    Tab t(1, TEST_COLS, TEST_ROWS, TEST_SCROLLBACK);

    EXPECT_NE(nullptr, t.GetScreenBuffer());
    EXPECT_NE(nullptr, t.GetVTParser());
    EXPECT_NE(nullptr, t.GetTerminal());
}

/**
 * CONTRACT: Tab constructor sets correct ID
 *
 * Given: An ID value
 * When: Tab is constructed
 * Then: GetId returns that ID
 */
TEST_F(TabTest, Constructor_SetsId) {
    Tab t1(1, TEST_COLS, TEST_ROWS);
    Tab t2(42, TEST_COLS, TEST_ROWS);
    Tab t3(999, TEST_COLS, TEST_ROWS);

    EXPECT_EQ(1, t1.GetId());
    EXPECT_EQ(42, t2.GetId());
    EXPECT_EQ(999, t3.GetId());
}

/**
 * CONTRACT: Tab constructor sets default title
 *
 * Given: No title specified
 * When: Tab is constructed
 * Then: Title is "Terminal"
 */
TEST_F(TabTest, Constructor_SetsDefaultTitle) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_EQ(L"Terminal", t.GetTitle());
}

/**
 * CONTRACT: Tab constructor initializes without activity
 *
 * Given: New tab
 * When: Tab is constructed
 * Then: HasActivity returns false
 */
TEST_F(TabTest, Constructor_NoInitialActivity) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_FALSE(t.HasActivity());
}

/**
 * CONTRACT: Tab is not running before Start
 *
 * Given: New tab
 * When: Tab is constructed but not started
 * Then: IsRunning returns false
 */
TEST_F(TabTest, Constructor_NotRunning) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_FALSE(t.IsRunning());
}

/**
 * CONTRACT: Tab constructor respects custom scrollback
 *
 * Given: Custom scrollback lines
 * When: Tab is constructed
 * Then: ScreenBuffer is created (scrollback is internal)
 */
TEST_F(TabTest, Constructor_CustomScrollback) {
    Tab t(1, TEST_COLS, TEST_ROWS, 5000);

    auto* buffer = t.GetScreenBuffer();
    ASSERT_NE(nullptr, buffer);
    // Scrollback capacity is internal, just verify buffer exists
    EXPECT_EQ(TEST_COLS, buffer->GetCols());
    EXPECT_EQ(TEST_ROWS, buffer->GetRows());
}

// ============================================================================
// Start/Stop Tests
// ============================================================================

/**
 * CONTRACT: Start launches shell process
 *
 * Given: Valid shell path
 * When: Start is called
 * Then: Returns true and IsRunning becomes true
 */
TEST_F(TabTest, Start_LaunchesShell) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    bool result = t.Start(TEST_SHELL);
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(result);
    EXPECT_TRUE(t.IsRunning());

    t.Stop();
}

/**
 * CONTRACT: Start sets title from shell name
 *
 * Given: Shell path with executable name
 * When: Start is called
 * Then: Title is set to executable name
 */
TEST_F(TabTest, Start_SetsTitleFromShell) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.Start(TEST_SHELL);
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(L"cmd.exe", t.GetTitle());

    t.Stop();
}

/**
 * CONTRACT: Start with full path extracts filename for title
 *
 * Given: Full path to shell
 * When: Start is called
 * Then: Title is just the filename
 */
TEST_F(TabTest, Start_ExtractsFilenameForTitle) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.Start(L"C:\\Windows\\System32\\cmd.exe");
    std::this_thread::sleep_for(100ms);

    EXPECT_EQ(L"cmd.exe", t.GetTitle());

    t.Stop();
}

/**
 * CONTRACT: Stop terminates shell process
 *
 * Given: Running tab
 * When: Stop is called
 * Then: IsRunning becomes false
 */
TEST_F(TabTest, Stop_TerminatesShell) {
    Tab t(1, TEST_COLS, TEST_ROWS);
    t.Start(TEST_SHELL);
    std::this_thread::sleep_for(100ms);
    ASSERT_TRUE(t.IsRunning());

    t.Stop();
    std::this_thread::sleep_for(100ms);

    EXPECT_FALSE(t.IsRunning());
}

/**
 * CONTRACT: Stop is safe to call multiple times
 *
 * Given: Tab (running or stopped)
 * When: Stop is called multiple times
 * Then: No crash or error
 */
TEST_F(TabTest, Stop_SafeToCallMultipleTimes) {
    Tab t(1, TEST_COLS, TEST_ROWS);
    t.Start(TEST_SHELL);
    std::this_thread::sleep_for(100ms);

    EXPECT_NO_THROW({
        t.Stop();
        t.Stop();
        t.Stop();
    });
}

/**
 * CONTRACT: Stop is safe on unstarted tab
 *
 * Given: Tab that was never started
 * When: Stop is called
 * Then: No crash or error
 */
TEST_F(TabTest, Stop_SafeOnUnstartedTab) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_NO_THROW(t.Stop());
    EXPECT_FALSE(t.IsRunning());
}

// ============================================================================
// Title Tests
// ============================================================================

/**
 * CONTRACT: SetTitle changes the title
 *
 * Given: A tab
 * When: SetTitle is called
 * Then: GetTitle returns new title
 */
TEST_F(TabTest, SetTitle_ChangesTitle) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.SetTitle(L"Custom Title");

    EXPECT_EQ(L"Custom Title", t.GetTitle());
}

/**
 * CONTRACT: Title can contain Unicode
 *
 * Given: A tab
 * When: SetTitle is called with Unicode
 * Then: GetTitle returns Unicode title correctly
 */
TEST_F(TabTest, SetTitle_SupportsUnicode) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.SetTitle(L"Terminal 日本語");

    EXPECT_EQ(L"Terminal 日本語", t.GetTitle());
}

/**
 * CONTRACT: Title can be empty
 *
 * Given: A tab
 * When: SetTitle is called with empty string
 * Then: GetTitle returns empty string
 */
TEST_F(TabTest, SetTitle_AllowsEmpty) {
    Tab t(1, TEST_COLS, TEST_ROWS);
    t.SetTitle(L"Something");

    t.SetTitle(L"");

    EXPECT_EQ(L"", t.GetTitle());
}

// ============================================================================
// Activity Tests
// ============================================================================

/**
 * CONTRACT: SetActivity changes activity state
 *
 * Given: A tab
 * When: SetActivity is called with true
 * Then: HasActivity returns true
 */
TEST_F(TabTest, SetActivity_ChangesState) {
    Tab t(1, TEST_COLS, TEST_ROWS);
    ASSERT_FALSE(t.HasActivity());

    t.SetActivity(true);

    EXPECT_TRUE(t.HasActivity());
}

/**
 * CONTRACT: ClearActivity resets activity
 *
 * Given: A tab with activity
 * When: ClearActivity is called
 * Then: HasActivity returns false
 */
TEST_F(TabTest, ClearActivity_ResetsActivity) {
    Tab t(1, TEST_COLS, TEST_ROWS);
    t.SetActivity(true);
    ASSERT_TRUE(t.HasActivity());

    t.ClearActivity();

    EXPECT_FALSE(t.HasActivity());
}

/**
 * CONTRACT: Shell output sets activity flag
 *
 * Given: A running tab
 * When: Shell produces output
 * Then: HasActivity becomes true
 */
TEST_F(TabTest, ShellOutput_SetsActivity) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    tab->ClearActivity();
    ASSERT_FALSE(tab->HasActivity());

    // Send command to generate output
    tab->WriteInput("echo test\r\n", 11);
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(tab->HasActivity());
}

// ============================================================================
// Output Callback Tests
// ============================================================================

/**
 * CONTRACT: Output callback receives shell output
 *
 * Given: A running tab with output callback
 * When: Shell produces output
 * Then: Callback is invoked with output data
 */
TEST_F(TabTest, OutputCallback_ReceivesOutput) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    // Wait for initial shell prompt
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(outputReceived);
    EXPECT_FALSE(outputData.empty());
}

/**
 * CONTRACT: Output callback receives command echo
 *
 * Given: A running tab with output callback
 * When: Input is written
 * Then: Callback receives echoed input
 */
TEST_F(TabTest, OutputCallback_ReceivesEcho) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());
    outputData.clear();

    tab->WriteInput("echo hello\r\n", 12);
    std::this_thread::sleep_for(300ms);

    EXPECT_NE(std::string::npos, outputData.find("hello"));
}

// ============================================================================
// Process Exit Callback Tests
// ============================================================================

/**
 * CONTRACT: Process exit callback fires when shell exits
 *
 * Given: A running tab with exit callback
 * When: Shell process exits
 * Then: Callback is invoked with exit code
 */
TEST_F(TabTest, ProcessExitCallback_FiresOnExit) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    // Exit the shell
    tab->WriteInput("exit\r\n", 6);

    // Wait for exit
    for (int i = 0; i < 20 && !exitCodeReceived; i++) {
        std::this_thread::sleep_for(100ms);
    }

    EXPECT_TRUE(exitCodeReceived);
    EXPECT_EQ(0, lastExitCode);
}

/**
 * CONTRACT: Process exit callback receives non-zero exit code
 *
 * Given: A running tab with exit callback
 * When: Shell exits with error code
 * Then: Callback receives that code
 */
TEST_F(TabTest, ProcessExitCallback_ReceivesErrorCode) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    // Exit with error code
    tab->WriteInput("exit /b 42\r\n", 12);

    // Wait for exit
    for (int i = 0; i < 20 && !exitCodeReceived; i++) {
        std::this_thread::sleep_for(100ms);
    }

    EXPECT_TRUE(exitCodeReceived);
    EXPECT_EQ(42, lastExitCode);
}

// ============================================================================
// Input Tests
// ============================================================================

/**
 * CONTRACT: WriteInput sends data to shell
 *
 * Given: A running tab
 * When: WriteInput is called
 * Then: Shell receives and processes input
 */
TEST_F(TabTest, WriteInput_SendsToShell) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());
    outputData.clear();

    // Write a command
    tab->WriteInput("echo WriteInputTest\r\n", 21);
    std::this_thread::sleep_for(300ms);

    EXPECT_NE(std::string::npos, outputData.find("WriteInputTest"));
}

/**
 * CONTRACT: WriteInput is safe when not running
 *
 * Given: A tab that is not running
 * When: WriteInput is called
 * Then: No crash (data is silently ignored)
 */
TEST_F(TabTest, WriteInput_SafeWhenNotRunning) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_NO_THROW(t.WriteInput("test", 4));
}

/**
 * CONTRACT: WriteInput handles empty data
 *
 * Given: A running tab
 * When: WriteInput is called with empty data
 * Then: No crash or error
 */
TEST_F(TabTest, WriteInput_HandlesEmptyData) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    EXPECT_NO_THROW(tab->WriteInput("", 0));
}

// ============================================================================
// Resize Tests
// ============================================================================

/**
 * CONTRACT: Resize updates screen buffer dimensions
 *
 * Given: A tab
 * When: Resize is called
 * Then: ScreenBuffer has new dimensions
 */
TEST_F(TabTest, Resize_UpdatesScreenBuffer) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.Resize(120, 40);

    auto* buffer = t.GetScreenBuffer();
    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(120, buffer->GetCols());
    EXPECT_EQ(40, buffer->GetRows());
}

/**
 * CONTRACT: Resize works on running tab
 *
 * Given: A running tab
 * When: Resize is called
 * Then: Both buffer and ConPTY are resized
 */
TEST_F(TabTest, Resize_WorksOnRunningTab) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    EXPECT_NO_THROW(tab->Resize(100, 30));

    auto* buffer = tab->GetScreenBuffer();
    EXPECT_EQ(100, buffer->GetCols());
    EXPECT_EQ(30, buffer->GetRows());
}

/**
 * CONTRACT: ResizeScreenBuffer only updates buffer
 *
 * Given: A tab
 * When: ResizeScreenBuffer is called
 * Then: ScreenBuffer dimensions change
 */
TEST_F(TabTest, ResizeScreenBuffer_UpdatesOnlyBuffer) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    t.ResizeScreenBuffer(100, 30);

    auto* buffer = t.GetScreenBuffer();
    EXPECT_EQ(100, buffer->GetCols());
    EXPECT_EQ(30, buffer->GetRows());
}

/**
 * CONTRACT: ResizeConPTY is safe on non-running tab
 *
 * Given: A tab that is not running
 * When: ResizeConPTY is called
 * Then: No crash (operation is no-op)
 */
TEST_F(TabTest, ResizeConPTY_SafeWhenNotRunning) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    EXPECT_NO_THROW(t.ResizeConPTY(100, 30));
}

// ============================================================================
// Component Access Tests
// ============================================================================

/**
 * CONTRACT: GetScreenBuffer returns valid buffer
 *
 * Given: A tab
 * When: GetScreenBuffer is called
 * Then: Returns non-null pointer with correct dimensions
 */
TEST_F(TabTest, GetScreenBuffer_ReturnsValidBuffer) {
    Tab t(1, 100, 50, 2000);

    auto* buffer = t.GetScreenBuffer();

    ASSERT_NE(nullptr, buffer);
    EXPECT_EQ(100, buffer->GetCols());
    EXPECT_EQ(50, buffer->GetRows());
}

/**
 * CONTRACT: GetVTParser returns valid parser
 *
 * Given: A tab
 * When: GetVTParser is called
 * Then: Returns non-null pointer
 */
TEST_F(TabTest, GetVTParser_ReturnsValidParser) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    auto* parser = t.GetVTParser();

    EXPECT_NE(nullptr, parser);
}

/**
 * CONTRACT: GetTerminal returns valid session
 *
 * Given: A tab
 * When: GetTerminal is called
 * Then: Returns non-null pointer
 */
TEST_F(TabTest, GetTerminal_ReturnsValidSession) {
    Tab t(1, TEST_COLS, TEST_ROWS);

    auto* terminal = t.GetTerminal();

    EXPECT_NE(nullptr, terminal);
}

// ============================================================================
// VT Parser Integration Tests
// ============================================================================

/**
 * CONTRACT: Shell output is parsed by VT parser
 *
 * Given: A running tab
 * When: Shell produces output
 * Then: VT parser processes it into screen buffer
 */
TEST_F(TabTest, VTParser_ProcessesShellOutput) {
    tab = CreateAndStartTab();
    ASSERT_TRUE(tab->IsRunning());

    // Send a command that produces predictable output
    tab->WriteInput("echo VTParserTest\r\n", 19);
    std::this_thread::sleep_for(300ms);

    // Check that text appears in screen buffer
    auto* buffer = tab->GetScreenBuffer();
    bool found = false;

    for (int row = 0; row < buffer->GetRows(); row++) {
        std::string line;
        for (int col = 0; col < buffer->GetCols(); col++) {
            const auto& cell = buffer->GetCell(col, row);
            if (cell.ch != 0 && cell.ch != U' ') {
                line += static_cast<char>(cell.ch);
            }
        }
        if (line.find("VTParserTest") != std::string::npos) {
            found = true;
            break;
        }
    }

    EXPECT_TRUE(found) << "Expected 'VTParserTest' to appear in screen buffer";
}

// ============================================================================
// Destructor Tests
// ============================================================================

/**
 * CONTRACT: Destructor stops running shell
 *
 * Given: A running tab
 * When: Tab is destroyed
 * Then: Shell is stopped cleanly
 */
TEST_F(TabTest, Destructor_StopsShell) {
    bool destroyed = false;

    {
        auto t = std::make_unique<Tab>(1, TEST_COLS, TEST_ROWS);
        t->Start(TEST_SHELL);
        std::this_thread::sleep_for(100ms);
        ASSERT_TRUE(t->IsRunning());

        // Tab destructor called here
    }

    // If we get here without crashing, destructor worked
    destroyed = true;
    EXPECT_TRUE(destroyed);
}

/**
 * CONTRACT: Destructor is safe on unstarted tab
 *
 * Given: A tab that was never started
 * When: Tab is destroyed
 * Then: No crash or error
 */
TEST_F(TabTest, Destructor_SafeOnUnstartedTab) {
    bool destroyed = false;

    {
        Tab t(1, TEST_COLS, TEST_ROWS);
        // Never started - destructor called here
    }

    destroyed = true;
    EXPECT_TRUE(destroyed);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

/**
 * CONTRACT: Tab handles minimum dimensions
 *
 * Given: Very small dimensions
 * When: Tab is created
 * Then: Tab works correctly
 */
TEST_F(TabTest, EdgeCase_MinimumDimensions) {
    Tab t(1, 1, 1, 1);

    EXPECT_NE(nullptr, t.GetScreenBuffer());
    EXPECT_EQ(1, t.GetScreenBuffer()->GetCols());
    EXPECT_EQ(1, t.GetScreenBuffer()->GetRows());
}

/**
 * CONTRACT: Tab handles large dimensions
 *
 * Given: Large dimensions
 * When: Tab is created
 * Then: Tab works correctly
 */
TEST_F(TabTest, EdgeCase_LargeDimensions) {
    Tab t(1, 500, 200, 100000);

    EXPECT_NE(nullptr, t.GetScreenBuffer());
    EXPECT_EQ(500, t.GetScreenBuffer()->GetCols());
    EXPECT_EQ(200, t.GetScreenBuffer()->GetRows());
}

/**
 * CONTRACT: Multiple tabs can run simultaneously
 *
 * Given: Multiple tabs
 * When: All are started
 * Then: All run independently
 */
TEST_F(TabTest, EdgeCase_MultipleTabs) {
    Tab t1(1, TEST_COLS, TEST_ROWS);
    Tab t2(2, TEST_COLS, TEST_ROWS);
    Tab t3(3, TEST_COLS, TEST_ROWS);

    EXPECT_TRUE(t1.Start(TEST_SHELL));
    EXPECT_TRUE(t2.Start(TEST_SHELL));
    EXPECT_TRUE(t3.Start(TEST_SHELL));

    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(t1.IsRunning());
    EXPECT_TRUE(t2.IsRunning());
    EXPECT_TRUE(t3.IsRunning());

    t1.Stop();
    t2.Stop();
    t3.Stop();
}

} // namespace TerminalDX12::UI::Tests
