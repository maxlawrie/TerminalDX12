#pragma once

#include <memory>
#include <string>
#include <functional>

namespace TerminalDX12 {

namespace Pty { class ConPtySession; }
namespace Terminal {
    class ScreenBuffer;
    class VTStateMachine;
}

namespace UI {

// Represents a single terminal tab with its own shell session
class Tab {
public:
    Tab(int id, int cols, int rows, int scrollbackLines = 10000);
    ~Tab();

    // Start the shell process
    bool Start(const std::wstring& shell);
    void Stop();

    // Check if tab is running
    bool IsRunning() const;

    // Get tab ID
    int GetId() const { return m_id; }

    // Tab title (from shell or OSC sequence)
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

    std::unique_ptr<Terminal::ScreenBuffer> m_screenBuffer;
    std::unique_ptr<Terminal::VTStateMachine> m_vtParser;
    std::unique_ptr<Pty::ConPtySession> m_terminal;

    std::function<void(const char*, size_t)> m_outputCallback;
};

} // namespace UI
} // namespace TerminalDX12
