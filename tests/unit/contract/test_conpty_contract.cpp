/**
 * @file test_conpty_contract.cpp
 * @brief Contract tests for ConPTY session interface
 *
 * These tests verify the ConPTY interface contracts:
 * - Session lifecycle (Start/Stop)
 * - Input/Output handling
 * - Resize operations
 * - Callback behavior
 * - Error handling
 *
 * Contract tests ensure the ConPTY wrapper behaves correctly
 * according to the Windows ConPTY API expectations.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include "pty/ConPtySession.h"

using namespace TerminalDX12::Pty;
using namespace std::chrono_literals;

/**
 * Test fixture for ConPTY contract tests
 */
class ConPtyContractTest : public ::testing::Test {
protected:
    void SetUp() override {
        session = std::make_unique<ConPtySession>();
        outputReceived.clear();
        exitCodeReceived = -1;
        exitCallbackCalled = false;
    }

    void TearDown() override {
        if (session && session->IsRunning()) {
            session->Stop();
        }
        session.reset();
    }

    // Helper to collect output
    void SetupOutputCallback() {
        session->SetOutputCallback([this](const char* data, size_t size) {
            std::lock_guard<std::mutex> lock(outputMutex);
            outputReceived.append(data, size);
            outputCV.notify_all();
        });
    }

    // Helper to track process exit
    void SetupExitCallback() {
        session->SetProcessExitCallback([this](int exitCode) {
            std::lock_guard<std::mutex> lock(exitMutex);
            exitCodeReceived = exitCode;
            exitCallbackCalled = true;
            exitCV.notify_all();
        });
    }

    // Wait for output containing expected string
    bool WaitForOutput(const std::string& expected, int timeoutMs = 5000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::unique_lock<std::mutex> lock(outputMutex);

        while (outputReceived.find(expected) == std::string::npos) {
            if (outputCV.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    // Wait for process exit
    bool WaitForExit(int timeoutMs = 5000) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        std::unique_lock<std::mutex> lock(exitMutex);

        while (!exitCallbackCalled) {
            if (exitCV.wait_until(lock, deadline) == std::cv_status::timeout) {
                return false;
            }
        }
        return true;
    }

    std::unique_ptr<ConPtySession> session;
    std::string outputReceived;
    std::mutex outputMutex;
    std::condition_variable outputCV;

    int exitCodeReceived;
    std::atomic<bool> exitCallbackCalled;
    std::mutex exitMutex;
    std::condition_variable exitCV;
};

// ============================================================================
// Contract: Session Lifecycle
// ============================================================================

/**
 * CONTRACT: Start() with valid command returns true and sets IsRunning()
 *
 * Given: A new ConPtySession instance
 * When: Start() is called with a valid command
 * Then: Returns true and IsRunning() returns true
 */
TEST_F(ConPtyContractTest, Start_ValidCommand_ReturnsTrue) {
    EXPECT_FALSE(session->IsRunning());

    bool result = session->Start(L"cmd.exe", 80, 24);

    EXPECT_TRUE(result);
    EXPECT_TRUE(session->IsRunning());
}

/**
 * CONTRACT: Start() with PowerShell returns true
 *
 * Given: A new ConPtySession instance
 * When: Start() is called with PowerShell
 * Then: Returns true and session is running
 */
TEST_F(ConPtyContractTest, Start_PowerShell_ReturnsTrue) {
    bool result = session->Start(L"powershell.exe -NoProfile -NoLogo", 80, 24);

    EXPECT_TRUE(result);
    EXPECT_TRUE(session->IsRunning());
}

/**
 * CONTRACT: Start() on already running session returns false
 *
 * Given: A running ConPtySession
 * When: Start() is called again
 * Then: Returns false (session unchanged)
 */
TEST_F(ConPtyContractTest, Start_AlreadyRunning_ReturnsFalse) {
    session->Start(L"cmd.exe", 80, 24);
    ASSERT_TRUE(session->IsRunning());

    bool result = session->Start(L"cmd.exe", 80, 24);

    EXPECT_FALSE(result);
    EXPECT_TRUE(session->IsRunning()); // Still running
}

/**
 * CONTRACT: Stop() on running session sets IsRunning() to false
 *
 * Given: A running ConPtySession
 * When: Stop() is called
 * Then: IsRunning() returns false
 */
TEST_F(ConPtyContractTest, Stop_RunningSession_SetsNotRunning) {
    session->Start(L"cmd.exe", 80, 24);
    ASSERT_TRUE(session->IsRunning());

    session->Stop();

    EXPECT_FALSE(session->IsRunning());
}

/**
 * CONTRACT: Stop() on non-running session is safe (no-op)
 *
 * Given: A non-running ConPtySession
 * When: Stop() is called
 * Then: No crash, IsRunning() still false
 */
TEST_F(ConPtyContractTest, Stop_NotRunning_IsSafe) {
    EXPECT_FALSE(session->IsRunning());

    EXPECT_NO_THROW(session->Stop());

    EXPECT_FALSE(session->IsRunning());
}

/**
 * CONTRACT: Multiple Stop() calls are safe
 *
 * Given: A ConPtySession that was started and stopped
 * When: Stop() is called again
 * Then: No crash or error
 */
TEST_F(ConPtyContractTest, Stop_MultipleCalls_IsSafe) {
    session->Start(L"cmd.exe", 80, 24);
    session->Stop();

    EXPECT_NO_THROW(session->Stop());
    EXPECT_NO_THROW(session->Stop());

    EXPECT_FALSE(session->IsRunning());
}

/**
 * CONTRACT: Destructor calls Stop() implicitly
 *
 * Given: A running ConPtySession
 * When: The session is destroyed
 * Then: No resource leak (implicit Stop())
 */
TEST_F(ConPtyContractTest, Destructor_RunningSession_CleansUp) {
    {
        ConPtySession localSession;
        localSession.Start(L"cmd.exe", 80, 24);
        EXPECT_TRUE(localSession.IsRunning());
        // Destructor called here
    }
    // No crash = pass
    SUCCEED();
}

// ============================================================================
// Contract: Input Handling
// ============================================================================

/**
 * CONTRACT: WriteInput() on running session returns true
 *
 * Given: A running ConPtySession
 * When: WriteInput() is called with valid data
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, WriteInput_RunningSession_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms); // Wait for shell to initialize

    bool result = session->WriteInput("echo test\r\n");

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: WriteInput() on non-running session returns false
 *
 * Given: A non-running ConPtySession
 * When: WriteInput() is called
 * Then: Returns false
 */
TEST_F(ConPtyContractTest, WriteInput_NotRunning_ReturnsFalse) {
    EXPECT_FALSE(session->IsRunning());

    bool result = session->WriteInput("test");

    EXPECT_FALSE(result);
}

/**
 * CONTRACT: WriteInput() with empty string is valid
 *
 * Given: A running ConPtySession
 * When: WriteInput() is called with empty string
 * Then: Returns true (valid edge case)
 */
TEST_F(ConPtyContractTest, WriteInput_EmptyString_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    bool result = session->WriteInput("");

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: WriteInput() with char pointer and size works
 *
 * Given: A running ConPtySession
 * When: WriteInput(const char*, size_t) is called
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, WriteInput_CharPointer_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    const char* data = "test\r\n";
    bool result = session->WriteInput(data, strlen(data));

    EXPECT_TRUE(result);
}

// ============================================================================
// Contract: Output Handling
// ============================================================================

/**
 * CONTRACT: Output callback receives shell prompt
 *
 * Given: A running ConPtySession with output callback
 * When: Session starts with cmd.exe
 * Then: Callback receives output (shell prompt or banner)
 */
TEST_F(ConPtyContractTest, OutputCallback_ShellStart_ReceivesOutput) {
    SetupOutputCallback();
    session->Start(L"cmd.exe", 80, 24);

    bool received = WaitForOutput(">", 5000); // cmd.exe prompt contains ">"

    EXPECT_TRUE(received) << "Expected shell prompt, got: " << outputReceived;
}

/**
 * CONTRACT: Output callback receives command echo
 *
 * Given: A running ConPtySession with output callback
 * When: A command is written
 * Then: Callback receives the command echo or output
 */
TEST_F(ConPtyContractTest, OutputCallback_CommandEcho_ReceivesOutput) {
    SetupOutputCallback();
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(1000ms);

    session->WriteInput("echo CONTRACT_TEST_OUTPUT\r\n");

    bool received = WaitForOutput("CONTRACT_TEST_OUTPUT", 5000);
    EXPECT_TRUE(received) << "Expected echo output, got: " << outputReceived;
}

/**
 * CONTRACT: Output callback can be set before Start()
 *
 * Given: A new ConPtySession
 * When: Output callback is set before Start()
 * Then: Callback receives output after Start()
 */
TEST_F(ConPtyContractTest, OutputCallback_SetBeforeStart_ReceivesOutput) {
    SetupOutputCallback();

    session->Start(L"cmd.exe", 80, 24);

    bool received = WaitForOutput(">", 5000);
    EXPECT_TRUE(received);
}

/**
 * CONTRACT: Null output callback is safe
 *
 * Given: A running ConPtySession with no callback set
 * When: Output is generated
 * Then: No crash
 */
TEST_F(ConPtyContractTest, OutputCallback_NullCallback_IsSafe) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(1000ms);

    session->WriteInput("echo test\r\n");
    std::this_thread::sleep_for(500ms);

    // No crash = pass
    SUCCEED();
}

// ============================================================================
// Contract: Resize Operations
// ============================================================================

/**
 * CONTRACT: Resize() on running session returns true
 *
 * Given: A running ConPtySession
 * When: Resize() is called with valid dimensions
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, Resize_RunningSession_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    bool result = session->Resize(120, 40);

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Resize() on non-running session returns false
 *
 * Given: A non-running ConPtySession
 * When: Resize() is called
 * Then: Returns false
 */
TEST_F(ConPtyContractTest, Resize_NotRunning_ReturnsFalse) {
    EXPECT_FALSE(session->IsRunning());

    bool result = session->Resize(120, 40);

    EXPECT_FALSE(result);
}

/**
 * CONTRACT: Resize() to minimum dimensions succeeds
 *
 * Given: A running ConPtySession
 * When: Resize() is called with minimum dimensions (1x1)
 * Then: Returns true (ConPTY allows small sizes)
 */
TEST_F(ConPtyContractTest, Resize_MinimumDimensions_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    bool result = session->Resize(1, 1);

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Resize() to large dimensions succeeds
 *
 * Given: A running ConPtySession
 * When: Resize() is called with large dimensions
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, Resize_LargeDimensions_ReturnsTrue) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    bool result = session->Resize(500, 200);

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Multiple Resize() calls are supported
 *
 * Given: A running ConPtySession
 * When: Resize() is called multiple times
 * Then: All calls succeed
 */
TEST_F(ConPtyContractTest, Resize_MultipleCalls_AllSucceed) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    EXPECT_TRUE(session->Resize(100, 30));
    EXPECT_TRUE(session->Resize(120, 40));
    EXPECT_TRUE(session->Resize(80, 24));
}

// ============================================================================
// Contract: Process Exit Handling
// ============================================================================

/**
 * CONTRACT: Exit callback is called when process exits normally
 *
 * Given: A running ConPtySession with exit callback
 * When: The shell process exits (via "exit" command)
 * Then: Exit callback is called with exit code
 */
TEST_F(ConPtyContractTest, ExitCallback_NormalExit_CalledWithCode) {
    SetupExitCallback();
    session->Start(L"cmd.exe /c exit 0", 80, 24);

    bool exitReceived = WaitForExit(5000);

    EXPECT_TRUE(exitReceived);
    EXPECT_TRUE(exitCallbackCalled);
    EXPECT_EQ(exitCodeReceived, 0);
}

/**
 * CONTRACT: Exit callback receives non-zero exit code
 *
 * Given: A running ConPtySession with exit callback
 * When: Process exits with non-zero code
 * Then: Exit callback receives the correct code
 */
TEST_F(ConPtyContractTest, ExitCallback_NonZeroExit_ReceivesCode) {
    SetupExitCallback();
    session->Start(L"cmd.exe /c exit 42", 80, 24);

    bool exitReceived = WaitForExit(5000);

    EXPECT_TRUE(exitReceived);
    EXPECT_EQ(exitCodeReceived, 42);
}

/**
 * CONTRACT: Exit callback sets IsRunning() to false
 *
 * Given: A running ConPtySession with exit callback
 * When: Process exits
 * Then: IsRunning() returns false
 */
TEST_F(ConPtyContractTest, ExitCallback_ProcessExit_SetsNotRunning) {
    SetupExitCallback();
    session->Start(L"cmd.exe /c exit 0", 80, 24);

    WaitForExit(5000);

    EXPECT_FALSE(session->IsRunning());
}

// ============================================================================
// Contract: Terminal Dimensions
// ============================================================================

/**
 * CONTRACT: Start() with standard dimensions (80x24) succeeds
 *
 * Given: A new ConPtySession
 * When: Start() is called with 80x24
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, Start_StandardDimensions_Succeeds) {
    bool result = session->Start(L"cmd.exe", 80, 24);
    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Start() with wide dimensions succeeds
 *
 * Given: A new ConPtySession
 * When: Start() is called with wide terminal (200x50)
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, Start_WideDimensions_Succeeds) {
    bool result = session->Start(L"cmd.exe", 200, 50);
    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Start() with minimum dimensions succeeds
 *
 * Given: A new ConPtySession
 * When: Start() is called with minimum dimensions
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, Start_MinimumDimensions_Succeeds) {
    bool result = session->Start(L"cmd.exe", 1, 1);
    EXPECT_TRUE(result);
}

// ============================================================================
// Contract: Unicode Support
// ============================================================================

/**
 * CONTRACT: WriteInput() handles UTF-8 data
 *
 * Given: A running ConPtySession
 * When: WriteInput() is called with UTF-8 encoded text
 * Then: Returns true (data accepted)
 */
TEST_F(ConPtyContractTest, WriteInput_UTF8Data_Accepted) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    // UTF-8 encoded Unicode text
    std::string utf8Text = "echo \xE4\xBD\xA0\xE5\xA5\xBD\xE4\xB8\x96\xE7\x95\x8C\r\n";
    bool result = session->WriteInput(utf8Text);

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Output callback receives UTF-8 encoded output
 *
 * Given: A running ConPtySession with output callback
 * When: Shell produces Unicode output
 * Then: Callback receives UTF-8 encoded data
 */
TEST_F(ConPtyContractTest, OutputCallback_UTF8Output_Received) {
    SetupOutputCallback();
    session->Start(L"powershell.exe -NoProfile -Command \"Write-Output 'Test'\"", 80, 24);

    bool received = WaitForOutput("Test", 5000);

    EXPECT_TRUE(received);
}

// ============================================================================
// Contract: Stress/Edge Cases
// ============================================================================

/**
 * CONTRACT: Rapid WriteInput() calls are handled
 *
 * Given: A running ConPtySession
 * When: Multiple WriteInput() calls in rapid succession
 * Then: All calls return true
 */
TEST_F(ConPtyContractTest, WriteInput_RapidCalls_AllSucceed) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(session->WriteInput("x"));
    }
}

/**
 * CONTRACT: Large input is handled
 *
 * Given: A running ConPtySession
 * When: WriteInput() is called with large data
 * Then: Returns true
 */
TEST_F(ConPtyContractTest, WriteInput_LargeData_Accepted) {
    session->Start(L"cmd.exe", 80, 24);
    std::this_thread::sleep_for(500ms);

    std::string largeInput(4096, 'A');
    bool result = session->WriteInput(largeInput);

    EXPECT_TRUE(result);
}

/**
 * CONTRACT: Session can be restarted after Stop()
 *
 * Given: A ConPtySession that was started and stopped
 * When: Start() is called again
 * Then: Returns true (session restarts)
 */
TEST_F(ConPtyContractTest, Start_AfterStop_Succeeds) {
    session->Start(L"cmd.exe", 80, 24);
    session->Stop();
    EXPECT_FALSE(session->IsRunning());

    bool result = session->Start(L"cmd.exe", 80, 24);

    EXPECT_TRUE(result);
    EXPECT_TRUE(session->IsRunning());
}
