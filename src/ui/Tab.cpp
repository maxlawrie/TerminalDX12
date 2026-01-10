#include "ui/Tab.h"
#include "ui/TerminalSession.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

Tab::Tab(int id)
    : m_id(id)
    , m_title(L"Tab " + std::to_wstring(id))
{
    spdlog::debug("Tab {} created", id);
}

Tab::~Tab() {
    // Stop all sessions
    for (auto& session : m_sessions) {
        if (session) {
            session->Stop();
        }
    }
    spdlog::debug("Tab {} destroyed", m_id);
}

bool Tab::HasActivity() const {
    for (const auto& session : m_sessions) {
        if (session && session->HasActivity()) {
            return true;
        }
    }
    return false;
}

void Tab::ClearActivity() {
    for (auto& session : m_sessions) {
        if (session) {
            session->ClearActivity();
        }
    }
}

TerminalSession* Tab::CreateSession(const std::wstring& shell, int cols, int rows, int scrollbackLines) {
    int sessionId = m_nextSessionId++;
    auto session = std::make_unique<TerminalSession>(sessionId, cols, rows, scrollbackLines);

    // Wire up process exit callback
    session->SetProcessExitCallback([this, sessionId](int exitCode) {
        spdlog::info("Tab {}: Session {} exited with code {}", m_id, sessionId, exitCode);
        if (m_processExitCallback) {
            m_processExitCallback(sessionId, exitCode);
        }
    });

    // Allow external configuration (clipboard callbacks, etc.)
    if (m_sessionCreatedCallback) {
        m_sessionCreatedCallback(session.get());
    }

    // Start the shell
    if (!session->Start(shell)) {
        spdlog::error("Tab {}: Failed to start session with shell: {}", m_id,
                      std::string(shell.begin(), shell.end()));
        return nullptr;
    }

    // Update tab title from first session's title
    if (m_sessions.empty()) {
        m_title = session->GetTitle();
    }

    TerminalSession* ptr = session.get();
    m_sessions.push_back(std::move(session));

    // If this is the first session, initialize the pane manager
    if (m_sessions.size() == 1) {
        m_paneManager.Initialize(ptr);
    }

    spdlog::info("Tab {}: Created session {} ({}x{})", m_id, sessionId, cols, rows);
    return ptr;
}

Pane* Tab::SplitPane(SplitDirection direction, const std::wstring& shell, int cols, int rows, int scrollbackLines) {
    // Create a new session for the split
    TerminalSession* newSession = CreateSession(shell, cols, rows, scrollbackLines);
    if (!newSession) {
        return nullptr;
    }

    // Split the focused pane with the new session
    return m_paneManager.SplitFocusedPane(direction, newSession);
}

void Tab::ClosePane() {
    TerminalSession* sessionToClose = m_paneManager.CloseFocusedPane();
    if (sessionToClose) {
        RemoveSession(sessionToClose);
    }
}

TerminalSession* Tab::GetFocusedSession() {
    Pane* focusedPane = m_paneManager.GetFocusedPane();
    if (focusedPane && focusedPane->IsLeaf()) {
        return focusedPane->GetSession();
    }
    return nullptr;
}

bool Tab::HasRunningSessions() const {
    return std::any_of(m_sessions.begin(), m_sessions.end(),
        [](const auto& session) { return session && session->IsRunning(); });
}

void Tab::RemoveSession(TerminalSession* session) {
    if (!session) return;

    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
        [session](const auto& s) { return s.get() == session; });

    if (it != m_sessions.end()) {
        (*it)->Stop();
        m_sessions.erase(it);
        spdlog::debug("Tab {}: Removed session, {} sessions remaining", m_id, m_sessions.size());
    }
}

} // namespace TerminalDX12::UI
