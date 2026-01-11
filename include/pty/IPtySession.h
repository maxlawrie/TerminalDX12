#pragma once

#include <cstddef>
#include <string>

namespace TerminalDX12::Pty {

/**
 * @brief Abstract interface for pseudo-terminal sessions
 *
 * This interface abstracts the PTY operations needed by the application,
 * enabling testing with mock implementations that don't require actual
 * system pseudoconsoles.
 */
class IPtySession {
public:
    virtual ~IPtySession() = default;

    // Lifecycle
    virtual bool Start(const std::wstring& commandline, int cols, int rows) = 0;
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;

    // Input/Output
    virtual bool WriteInput(const char* data, size_t size) = 0;
    virtual bool WriteInput(const std::string& data) = 0;

    // Resize
    virtual bool Resize(int cols, int rows) = 0;
};

} // namespace TerminalDX12::Pty
