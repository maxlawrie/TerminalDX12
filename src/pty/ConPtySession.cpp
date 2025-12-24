#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace TerminalDX12 {
namespace Pty {

ConPtySession::ConPtySession()
    : m_hPC(INVALID_HANDLE_VALUE)
    , m_hPipeIn(INVALID_HANDLE_VALUE)
    , m_hPipeOut(INVALID_HANDLE_VALUE)
    , m_hPipePtyIn(INVALID_HANDLE_VALUE)
    , m_hPipePtyOut(INVALID_HANDLE_VALUE)
    , m_hProcess(INVALID_HANDLE_VALUE)
    , m_hThread(INVALID_HANDLE_VALUE)
    , m_running(false)
    , m_cols(80)
    , m_rows(24)
{
}

ConPtySession::~ConPtySession() {
    Stop();
}

bool ConPtySession::Start(const std::wstring& commandline, int cols, int rows) {
    if (m_running) {
        spdlog::warn("ConPTY session already running");
        return false;
    }

    m_cols = cols;
    m_rows = rows;

    // Create pipes for ConPTY communication
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE };

    if (!CreatePipe(&m_hPipePtyIn, &m_hPipeIn, &sa, 0)) {
        spdlog::error("Failed to create input pipe: {}", GetLastError());
        return false;
    }

    if (!CreatePipe(&m_hPipeOut, &m_hPipePtyOut, &sa, 0)) {
        spdlog::error("Failed to create output pipe: {}", GetLastError());
        CloseHandle(m_hPipePtyIn);
        CloseHandle(m_hPipeIn);
        return false;
    }

    // Make sure the handles we keep are not inherited
    SetHandleInformation(m_hPipeIn, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(m_hPipeOut, HANDLE_FLAG_INHERIT, 0);

    // Create the pseudoconsole
    COORD consoleSize = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    HRESULT hr = CreatePseudoConsole(consoleSize, m_hPipePtyIn, m_hPipePtyOut, 0, &m_hPC);

    if (FAILED(hr)) {
        spdlog::error("Failed to create pseudoconsole: 0x{:08X}", hr);
        CloseHandle(m_hPipePtyIn);
        CloseHandle(m_hPipePtyOut);
        CloseHandle(m_hPipeIn);
        CloseHandle(m_hPipeOut);
        return false;
    }

    // Close the ConPTY ends of the pipes (ConPTY owns them now)
    CloseHandle(m_hPipePtyIn);
    CloseHandle(m_hPipePtyOut);
    m_hPipePtyIn = INVALID_HANDLE_VALUE;
    m_hPipePtyOut = INVALID_HANDLE_VALUE;

    // Prepare startup info with pseudoconsole attribute
    STARTUPINFOEXW siEx = {};
    siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    siEx.StartupInfo.hStdInput = nullptr;
    siEx.StartupInfo.hStdOutput = nullptr;
    siEx.StartupInfo.hStdError = nullptr;

    // Allocate attribute list
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);

    siEx.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));
    if (!siEx.lpAttributeList) {
        spdlog::error("Failed to allocate attribute list");
        ClosePseudoConsole(m_hPC);
        CloseHandle(m_hPipeIn);
        CloseHandle(m_hPipeOut);
        return false;
    }

    if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &attrListSize)) {
        spdlog::error("Failed to initialize attribute list: {}", GetLastError());
        free(siEx.lpAttributeList);
        ClosePseudoConsole(m_hPC);
        CloseHandle(m_hPipeIn);
        CloseHandle(m_hPipeOut);
        return false;
    }

    // Set the pseudoconsole attribute
    if (!UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                                   m_hPC, sizeof(HPCON), nullptr, nullptr)) {
        spdlog::error("Failed to update proc thread attribute: {}", GetLastError());
        DeleteProcThreadAttributeList(siEx.lpAttributeList);
        free(siEx.lpAttributeList);
        ClosePseudoConsole(m_hPC);
        CloseHandle(m_hPipeIn);
        CloseHandle(m_hPipeOut);
        return false;
    }

    // Create the process
    PROCESS_INFORMATION pi = {};
    std::wstring cmdline = commandline; // CreateProcessW needs non-const

    BOOL success = CreateProcessW(
        nullptr,                                    // Application name
        cmdline.data(),                             // Command line
        nullptr,                                    // Process security attributes
        nullptr,                                    // Thread security attributes
        FALSE,                                      // Inherit handles
        EXTENDED_STARTUPINFO_PRESENT,               // Creation flags
        nullptr,                                    // Inherit environment
        nullptr,                                    // Current directory
        &siEx.StartupInfo,                          // Startup info
        &pi                                         // Process information
    );

    // Cleanup attribute list
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);

    if (!success) {
        spdlog::error("Failed to create process: {}", GetLastError());
        ClosePseudoConsole(m_hPC);
        CloseHandle(m_hPipeIn);
        CloseHandle(m_hPipeOut);
        return false;
    }

    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;

    spdlog::info("ConPTY session started: PID={}, Command=\"{}\"",
                 pi.dwProcessId, std::string(commandline.begin(), commandline.end()));

    // Start I/O threads
    m_running = true;
    m_outputThread = std::thread(&ConPtySession::ReadOutputThread, this);
    m_monitorThread = std::thread(&ConPtySession::ProcessMonitorThread, this);

    return true;
}

void ConPtySession::Stop() {
    if (!m_running) {
        return;
    }

    spdlog::info("Stopping ConPTY session");
    m_running = false;

    // First close the pseudoconsole - this signals the child process to exit
    // and causes ReadFile to return cleanly
    if (m_hPC != INVALID_HANDLE_VALUE) {
        ClosePseudoConsole(m_hPC);
        m_hPC = INVALID_HANDLE_VALUE;
    }

    // Now close pipe handles to unblock any remaining reads
    if (m_hPipeOut != INVALID_HANDLE_VALUE) {
        CancelIoEx(m_hPipeOut, nullptr);
        CloseHandle(m_hPipeOut);
        m_hPipeOut = INVALID_HANDLE_VALUE;
    }

    if (m_hPipeIn != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hPipeIn);
        m_hPipeIn = INVALID_HANDLE_VALUE;
    }

    // Wait for threads to finish
    if (m_outputThread.joinable()) {
        m_outputThread.join();
    }

    if (m_monitorThread.joinable()) {
        m_monitorThread.join();
    }

    // Terminate process if still running
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = INVALID_HANDLE_VALUE;
    }

    if (m_hThread != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hThread);
        m_hThread = INVALID_HANDLE_VALUE;
    }
}

bool ConPtySession::WriteInput(const char* data, size_t size) {
    if (!m_running || m_hPipeIn == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD written = 0;
    BOOL success = WriteFile(m_hPipeIn, data, static_cast<DWORD>(size), &written, nullptr);

    if (!success) {
        spdlog::error("Failed to write to ConPTY: {}", GetLastError());
        return false;
    }

    return written == size;
}

bool ConPtySession::WriteInput(const std::string& data) {
    return WriteInput(data.c_str(), data.size());
}

bool ConPtySession::Resize(int cols, int rows) {
    if (!m_running || m_hPC == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_cols = cols;
    m_rows = rows;

    COORD newSize = { static_cast<SHORT>(cols), static_cast<SHORT>(rows) };
    HRESULT hr = ResizePseudoConsole(m_hPC, newSize);

    if (FAILED(hr)) {
        spdlog::error("Failed to resize pseudoconsole: 0x{:08X}", hr);
        return false;
    }

    spdlog::info("ConPTY resized to {}x{}", cols, rows);
    return true;
}

void ConPtySession::SetOutputCallback(OutputCallback callback) {
    m_outputCallback = callback;
}

void ConPtySession::SetProcessExitCallback(ProcessExitCallback callback) {
    m_processExitCallback = callback;
}

void ConPtySession::ReadOutputThread() {
    spdlog::debug("Output read thread started");

    const size_t bufferSize = 4096;
    std::vector<char> buffer(bufferSize);

    while (m_running) {
        DWORD bytesRead = 0;
        BOOL success = ReadFile(m_hPipeOut, buffer.data(), static_cast<DWORD>(bufferSize),
                                &bytesRead, nullptr);

        if (!success || bytesRead == 0) {
            if (m_running) {
                DWORD error = GetLastError();
                if (error != ERROR_BROKEN_PIPE) {
                    spdlog::error("ReadFile failed: {}", error);
                }
            }
            break;
        }

        // Call output callback if set
        if (m_outputCallback && bytesRead > 0) {
            m_outputCallback(buffer.data(), bytesRead);
        }
    }

    spdlog::debug("Output read thread stopped");
}

void ConPtySession::ProcessMonitorThread() {
    spdlog::debug("Process monitor thread started");

    DWORD exitCode = 0;
    if (m_hProcess != INVALID_HANDLE_VALUE) {
        WaitForSingleObject(m_hProcess, INFINITE);

        GetExitCodeProcess(m_hProcess, &exitCode);
        spdlog::info("Process exited with code: {}", exitCode);
    }

    m_running = false;

    // Notify callback that process has exited
    if (m_processExitCallback) {
        m_processExitCallback(static_cast<int>(exitCode));
    }

    spdlog::debug("Process monitor thread stopped");
}

} // namespace Pty
} // namespace TerminalDX12
