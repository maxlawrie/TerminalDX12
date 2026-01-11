#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>
#include "pty/IPtySession.h"

namespace TerminalDX12 {
namespace Pty {

class ConPtySession : public IPtySession {
public:
    ConPtySession();
    ~ConPtySession();

    // Callback for when data is received from the terminal
    using OutputCallback = std::function<void(const char* data, size_t size)>;

    // Callback for when the shell process exits
    using ProcessExitCallback = std::function<void(int exitCode)>;

    // Initialize and start the pseudoconsole session
    bool Start(const std::wstring& commandline, int cols, int rows) override;

    // Stop the session and cleanup
    void Stop() override;

    // Write input to the terminal
    bool WriteInput(const char* data, size_t size) override;
    bool WriteInput(const std::string& data) override;

    // Resize the terminal
    bool Resize(int cols, int rows) override;

    // Set callback for terminal output
    void SetOutputCallback(OutputCallback callback);

    // Set callback for process exit
    void SetProcessExitCallback(ProcessExitCallback callback);

    // Check if session is running
    bool IsRunning() const override { return m_running; }

private:
    // Thread functions for async I/O
    void ReadOutputThread();
    void ProcessMonitorThread();

    // ConPTY handles
    HPCON m_pseudoConsole;
    HANDLE m_hPipeIn;     // Our write end (terminal input)
    HANDLE m_hPipeOut;    // Our read end (terminal output)
    HANDLE m_hPipePtyIn;  // ConPTY read end
    HANDLE m_hPipePtyOut; // ConPTY write end

    // Process handles
    HANDLE m_hProcess;
    HANDLE m_hThread;

    // I/O threads
    std::thread m_outputThread;
    std::thread m_monitorThread;
    std::atomic<bool> m_running;

    // Output callback
    OutputCallback m_outputCallback;

    // Process exit callback
    ProcessExitCallback m_processExitCallback;

    // Terminal size
    int m_cols;
    int m_rows;
};

} // namespace Pty
} // namespace TerminalDX12
