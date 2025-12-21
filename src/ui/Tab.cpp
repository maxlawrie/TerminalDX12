#include "ui/Tab.h"
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>

namespace TerminalDX12::UI {

Tab::Tab(int id, int cols, int rows, int scrollbackLines)
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

    spdlog::debug("Tab {} created: {}x{}", id, cols, rows);
}

Tab::~Tab() {
    Stop();
    spdlog::debug("Tab {} destroyed", m_id);
}

bool Tab::Start(const std::wstring& shell) {
    if (!m_terminal) {
        return false;
    }

    // Set output callback to process VT sequences
    m_terminal->SetOutputCallback([this](const char* data, size_t size) {
        // Parse VT sequences
        if (m_vtParser) {
            m_vtParser->ProcessInput(data, size);
        }

        // Mark activity for background tabs
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

    // Start the shell
    if (!m_terminal->Start(shell, m_cols, m_rows)) {
        spdlog::error("Tab {}: Failed to start shell: {}", m_id,
                      std::string(shell.begin(), shell.end()));
        return false;
    }

    // Set title to shell name
    size_t pos = shell.find_last_of(L"\\/");
    m_title = (pos != std::wstring::npos) ? shell.substr(pos + 1) : shell;

    spdlog::info("Tab {}: Started shell: {}", m_id,
                 std::string(shell.begin(), shell.end()));
    return true;
}

void Tab::Stop() {
    if (m_terminal) {
        m_terminal->Stop();
    }
}

bool Tab::IsRunning() const {
    return m_terminal && m_terminal->IsRunning();
}

void Tab::WriteInput(const char* data, size_t size) {
    if (m_terminal && m_terminal->IsRunning()) {
        m_terminal->WriteInput(data, size);
    }
}

void Tab::SetOutputCallback(std::function<void(const char*, size_t)> callback) {
    m_outputCallback = callback;
}

void Tab::Resize(int cols, int rows) {
    m_cols = cols;
    m_rows = rows;

    if (m_screenBuffer) {
        m_screenBuffer->Resize(cols, rows);
    }

    if (m_terminal && m_terminal->IsRunning()) {
        m_terminal->Resize(cols, rows);
    }
}

} // namespace TerminalDX12::UI
