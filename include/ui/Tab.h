#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "ui/PaneManager.h"

namespace TerminalDX12::Core {
    class Config;
}

namespace TerminalDX12::UI {

class TerminalSession;

/// @brief Represents a top-level tab containing a pane tree of terminal sessions
class Tab {
public:
    Tab(int id, Core::Config* config = nullptr);
    ~Tab();

    /// @brief Get tab ID
    int GetId() const { return m_id; }

    /// @brief Get/set tab title
    const std::wstring& GetTitle() const { return m_title; }
    void SetTitle(const std::wstring& title) { m_title = title; }

    /// @brief Activity indicator (any session in this tab has activity)
    bool HasActivity() const;
    void ClearActivity();

    /// @brief Get the pane manager for this tab
    PaneManager& GetPaneManager() { return m_paneManager; }
    const PaneManager& GetPaneManager() const { return m_paneManager; }

    /// @brief Create a new terminal session and add it to this tab
    /// @param shell Shell command to run
    /// @param cols Terminal columns
    /// @param rows Terminal rows
    /// @param scrollbackLines Scrollback buffer size
    /// @param profileName Profile name to use (empty = use default)
    /// @return The created session, or nullptr on failure
    [[nodiscard]] TerminalSession* CreateSession(const std::wstring& shell, int cols, int rows,
                                                  int scrollbackLines = 10000, const std::string& profileName = "");

    /// @brief Split the focused pane with a new session
    /// @param direction Split direction
    /// @param shell Shell command for the new session
    /// @param cols Terminal columns
    /// @param rows Terminal rows
    /// @param scrollbackLines Scrollback buffer size
    /// @param profileName Profile name to use (empty = use default)
    /// @return The new pane, or nullptr on failure
    [[nodiscard]] Pane* SplitPane(SplitDirection direction, const std::wstring& shell, int cols, int rows,
                                   int scrollbackLines = 10000, const std::string& profileName = "");

    /// @brief Close the focused pane
    void ClosePane();

    /// @brief Get the focused terminal session
    TerminalSession* GetFocusedSession();

    /// @brief Check if any sessions are running
    [[nodiscard]] bool HasRunningSessions() const;

    /// @brief Get all sessions in this tab
    const std::vector<std::unique_ptr<TerminalSession>>& GetSessions() const { return m_sessions; }

    /// @brief Set callback for when a session's process exits
    void SetProcessExitCallback(std::function<void(int sessionId, int exitCode)> callback) {
        m_processExitCallback = callback;
    }

    /// @brief Set callback for configuring new sessions (clipboard, etc.)
    void SetSessionCreatedCallback(std::function<void(TerminalSession*)> callback) {
        m_sessionCreatedCallback = callback;
    }

private:
    int m_id;
    std::wstring m_title;
    int m_nextSessionId = 1;
    Core::Config* m_config = nullptr;

    PaneManager m_paneManager;
    std::vector<std::unique_ptr<TerminalSession>> m_sessions;

    std::function<void(int, int)> m_processExitCallback;
    std::function<void(TerminalSession*)> m_sessionCreatedCallback;

    /// @brief Remove a session by pointer (called when pane is closed)
    void RemoveSession(TerminalSession* session);
};

} // namespace TerminalDX12::UI
