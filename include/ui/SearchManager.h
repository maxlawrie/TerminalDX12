#pragma once

#include <string>
#include <vector>

namespace TerminalDX12::Terminal {
    class ScreenBuffer;
}

namespace TerminalDX12::UI {

/// @brief Position in the terminal grid
struct CellPos {
    int x = 0;
    int y = 0;
};

/// @brief Manages terminal search functionality
class SearchManager {
public:
    SearchManager() = default;

    /// @brief Open search mode
    void Open();

    /// @brief Close search mode and clear results
    void Close();

    /// @brief Check if search mode is active
    bool IsActive() const { return m_active; }

    /// @brief Get the current search query
    const std::wstring& GetQuery() const { return m_query; }

    /// @brief Add a character to the search query
    void AppendChar(wchar_t ch);

    /// @brief Remove the last character from the search query
    void Backspace();

    /// @brief Update search results based on current query
    /// @param screenBuffer The screen buffer to search in
    void UpdateResults(Terminal::ScreenBuffer* screenBuffer);

    /// @brief Navigate to the next match
    void NextMatch();

    /// @brief Navigate to the previous match
    void PreviousMatch();

    /// @brief Get all match positions
    const std::vector<CellPos>& GetMatches() const { return m_matches; }

    /// @brief Get current match index (-1 if no matches)
    int GetCurrentMatchIndex() const { return m_currentIndex; }

    /// @brief Get total match count
    size_t GetMatchCount() const { return m_matches.size(); }

private:
    bool m_active = false;
    std::wstring m_query;
    std::vector<CellPos> m_matches;
    int m_currentIndex = -1;
};

} // namespace TerminalDX12::UI
