#pragma once

#include "terminal/ScreenBuffer.h"
#include <string>
#include <vector>
#include <regex>
#include <optional>

namespace TerminalDX12::UI {

/**
 * @brief Position of a search match in the terminal buffer
 */
struct SearchMatch {
    int x;              ///< Column position (0-indexed)
    int y;              ///< Row position (0-indexed, negative for scrollback)
    int length;         ///< Length of match in characters
};

/**
 * @brief Search result containing matches and status information
 */
struct SearchResult {
    std::vector<SearchMatch> matches;   ///< All found matches
    std::wstring errorMessage;          ///< Error message if regex is invalid
    bool isValid = true;                ///< False if regex compilation failed
};

/**
 * @brief Search engine for finding text in terminal buffers
 *
 * Supports both plain text and regex search modes, with optional
 * case-insensitive matching. Can search through both visible buffer
 * and scrollback history.
 */
class SearchEngine {
public:
    /**
     * @brief Search for text in a screen buffer
     * @param buffer The screen buffer to search
     * @param query The search query (plain text or regex)
     * @param useRegex If true, treat query as a regular expression
     * @param caseSensitive If true, match case exactly
     * @param includeScrollback If true, search scrollback history too
     * @return SearchResult containing matches or error information
     */
    static SearchResult Search(
        Terminal::ScreenBuffer& buffer,
        const std::wstring& query,
        bool useRegex = false,
        bool caseSensitive = false,
        bool includeScrollback = true
    );

private:
    /**
     * @brief Extract text from a row of the buffer
     * @param buffer The screen buffer
     * @param row Row index (0 = first visible row, negative = scrollback)
     * @return The text content of the row
     */
    static std::wstring GetRowText(Terminal::ScreenBuffer& buffer, int row);

    /**
     * @brief Find all matches of a pattern in a single line
     * @param lineText The text to search
     * @param query The search query
     * @param rowIndex Row index for match positions
     * @param useRegex Use regex matching
     * @param caseSensitive Case sensitive matching
     * @return Vector of matches found in this line
     */
    static std::vector<SearchMatch> SearchLine(
        const std::wstring& lineText,
        const std::wstring& query,
        int rowIndex,
        bool useRegex,
        bool caseSensitive
    );
};

} // namespace TerminalDX12::UI
