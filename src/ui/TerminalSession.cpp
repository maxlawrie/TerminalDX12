#include "ui/TerminalSession.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>

namespace TerminalDX12::UI {

TerminalSession::TerminalSession(int id, int cols, int rows, int scrollbackLines)
    : m_id(id)
    , m_cols(cols)
    , m_rows(rows)
    , m_title(L"Terminal")
{
    // Create screen buffer
    m_screenBuffer = std::make_unique<Terminal::ScreenBuffer>(cols, rows, scrollbackLines);

    // Create VT parser
    m_vtParser = std::make_unique<Terminal::VTStateMachine>(m_screenBuffer.get());

    // Create terminal session
    m_terminal = std::make_unique<Pty::ConPtySession>();

    spdlog::debug("TerminalSession {} created: {}x{}", id, cols, rows);
}

TerminalSession::~TerminalSession() {
    Stop();
    spdlog::debug("TerminalSession {} destroyed", m_id);
}

bool TerminalSession::Start(const std::wstring& shell) {
    if (!m_terminal) {
        return false;
    }

    // Set output callback to process VT sequences
    m_terminal->SetOutputCallback([this](const char* data, size_t size) {
        // Parse VT sequences
        if (m_vtParser) {
            m_vtParser->ProcessInput(data, size);
        }

        // Mark activity for background sessions
        m_hasActivity = true;

        // Forward to external callback if set
        if (m_outputCallback) {
            m_outputCallback(data, size);
        }
    });

    // Wire up response callback for device queries
    m_vtParser->SetResponseCallback([this](const std::string& response) {
        if (m_terminal && m_terminal->IsRunning()) {
            m_terminal->WriteInput(response.c_str(), response.size());
        }
    });

    // Wire up process exit callback
    m_terminal->SetProcessExitCallback([this](int exitCode) {
        spdlog::info("TerminalSession {}: Process exited with code {}", m_id, exitCode);
        if (m_processExitCallback) {
            m_processExitCallback(exitCode);
        }
    });

    // Start the shell
    if (!m_terminal->Start(shell, m_cols, m_rows)) {
        spdlog::error("TerminalSession {}: Failed to start shell: {}", m_id,
                      std::string(shell.begin(), shell.end()));
        return false;
    }

    // Set title to shell name
    size_t pos = shell.find_last_of(L"\\/");
    m_title = (pos != std::wstring::npos) ? shell.substr(pos + 1) : shell;

    spdlog::info("TerminalSession {}: Started shell: {}", m_id,
                 std::string(shell.begin(), shell.end()));
    return true;
}

void TerminalSession::Stop() {
    if (m_terminal) {
        m_terminal->Stop();
    }
}

bool TerminalSession::IsRunning() const {
    return m_terminal && m_terminal->IsRunning();
}

void TerminalSession::WriteInput(const char* data, size_t size) {
    if (m_terminal && m_terminal->IsRunning()) {
        m_terminal->WriteInput(data, size);
    }
}

void TerminalSession::SetOutputCallback(std::function<void(const char*, size_t)> callback) {
    m_outputCallback = callback;
}

void TerminalSession::SetProcessExitCallback(std::function<void(int)> callback) {
    m_processExitCallback = callback;
}

void TerminalSession::SetClipboardReadCallback(std::function<std::string()> callback) {
    if (m_vtParser) {
        m_vtParser->SetClipboardReadCallback(callback);
    }
}

void TerminalSession::SetClipboardWriteCallback(std::function<void(const std::string&)> callback) {
    if (m_vtParser) {
        m_vtParser->SetClipboardWriteCallback(callback);
    }
}

void TerminalSession::Resize(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;

    if (m_screenBuffer) {
        m_screenBuffer->Resize(cols, rows);
    }

    if (m_terminal && m_terminal->IsRunning()) {
        m_terminal->Resize(cols, rows);
    }
}

void TerminalSession::ResizeScreenBuffer(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;

    if (m_screenBuffer) {
        m_screenBuffer->Resize(cols, rows);
    }
}

void TerminalSession::ResizeConPTY(int cols, int rows) {
    if (m_terminal && m_terminal->IsRunning()) {
        m_terminal->Resize(cols, rows);
    }
}

} // namespace TerminalDX12::UI
