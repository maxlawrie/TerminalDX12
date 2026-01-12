#pragma once

#include "core/Config.h"
#include <memory>
#include <string>
#include <functional>
#include <optional>

namespace TerminalDX12 {

namespace Pty { class ConPtySession; }
namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}

namespace UI {

// Represents a single terminal session with its own shell process
class TerminalSession {
public:
    TerminalSession(int id, int cols, int rows, int scrollbackLines = 10000,
                    Core::Config* config = nullptr, const std::string& profileName = "Default");
    ~TerminalSession();

    // Start the shell process
    bool Start(const std::wstring& shell);
    void Stop();

    // Profile management
    const std::string& GetProfileName() const { return m_profileName; }
    void SetProfileName(const std::string& name);
    const Core::Profile* GetEffectiveProfile() const;
    bool HasProfileOverride() const { return m_profileOverride.has_value(); }
    void SetProfileOverride(const Core::Profile& overrides);
    void ClearProfileOverride();

    // Check if session is running
    bool IsRunning() const;

    // Get session ID
    int GetId() const { return m_id; }

    // Session title (from shell or OSC sequence)
    const std::wstring& GetTitle() const { return m_title; }
    void SetTitle(const std::wstring& title) { m_title = title; }

    // Activity indicator
    bool HasActivity() const { return m_hasActivity; }
    void SetActivity(bool activity) { m_hasActivity = activity; }
    void ClearActivity() { m_hasActivity = false; }

    // Access terminal components
    Terminal::ScreenBuffer* GetScreenBuffer() { return m_screenBuffer.get(); }
    Terminal::VTStateMachine* GetVTParser() { return m_vtParser.get(); }
    Pty::ConPtySession* GetTerminal() { return m_terminal.get(); }

    // Write input to the shell
    void WriteInput(const char* data, size_t size);

    // Set output callback (called when shell produces output)
    void SetOutputCallback(std::function<void(const char*, size_t)> callback);

    // Set process exit callback (called when shell process exits)
    void SetProcessExitCallback(std::function<void(int exitCode)> callback);

    // Set clipboard callbacks for OSC 52
    void SetClipboardReadCallback(std::function<std::string()> callback);
    void SetClipboardWriteCallback(std::function<void(const std::string&)> callback);

    // Resize the terminal
    void Resize(int cols, int rows);

    // Resize only screen buffer (for immediate visual feedback)
    void ResizeScreenBuffer(int cols, int rows);

    // Resize only ConPTY (deferred to prevent scroll)
    void ResizeConPTY(int cols, int rows);

private:
    int m_id;
    int m_cols;
    int m_rows;

    std::wstring m_title;
    bool m_hasActivity = false;

    // Profile management
    Core::Config* m_config = nullptr;
    std::string m_profileName = "Default";
    std::optional<Core::Profile> m_profileOverride;

    std::unique_ptr<Terminal::ScreenBuffer> m_screenBuffer;
    std::unique_ptr<Terminal::VTStateMachine> m_vtParser;
    std::unique_ptr<Pty::ConPtySession> m_terminal;

    std::function<void(const char*, size_t)> m_outputCallback;
    std::function<void(int)> m_processExitCallback;
};

} // namespace UI
} // namespace TerminalDX12
