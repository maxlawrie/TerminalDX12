#pragma once

#include <Windows.h>
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <vector>

namespace TerminalDX12 {
namespace Pty {

class ConPtySession {
public:
    ConPtySession();
    ~ConPtySession();

    // Callback for when data is received from the terminal
    using OutputCallback = std::function<void(const char* data, size_t size)>;

    // Initialize and start the pseudoconsole session
    bool Start(const std::wstring& commandline, int cols, int rows);

    // Stop the session and cleanup
    void Stop();

    // Write input to the terminal
    bool WriteInput(const char* data, size_t size);
    bool WriteInput(const std::string& data);

    // Resize the terminal
    bool Resize(int cols, int rows);

    // Set callback for terminal output
    void SetOutputCallback(OutputCallback callback);

    // Check if session is running
    bool IsRunning() const { return m_running; }

private:
    // Thread functions for async I/O
    void ReadOutputThread();
    void ProcessMonitorThread();

    // ConPTY handles
    HPCON m_hPC;
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

    // Terminal size
    int m_cols;
    int m_rows;
};

} // namespace Pty
} // namespace TerminalDX12
