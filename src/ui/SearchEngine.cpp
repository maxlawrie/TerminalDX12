#include "ui/SearchEngine.h"
#include <algorithm>
#include <cwctype>
#include <spdlog/spdlog.h>

namespace TerminalDX12::UI {

std::wstring SearchEngine::GetRowText(Terminal::ScreenBuffer& buffer, int row) {
    int cols, rows;
    buffer.GetDimensions(cols, rows);
    int scrollbackUsed = buffer.GetScrollbackUsed();

    std::wstring text;
    text.reserve(cols);

    if (row < 0) {
        // Negative row = scrollback
        // row -1 is the most recent scrollback line (scrollbackUsed - 1)
        // row -scrollbackUsed is the oldest scrollback line (index 0)
        int scrollbackLine = scrollbackUsed + row;  // e.g., -1 + 7 = 6 (index 6 for 7 lines)
        if (scrollbackLine >= 0 && scrollbackLine < scrollbackUsed) {
            for (int x = 0; x < cols; ++x) {
                auto cell = buffer.GetScrollbackCell(x, scrollbackLine);
                text += static_cast<wchar_t>(cell.ch);
            }
        }
    } else {
        // Non-negative row = visible buffer
        for (int x = 0; x < cols; ++x) {
            auto cell = buffer.GetCell(x, row);
            text += static_cast<wchar_t>(cell.ch);
        }
    }

    return text;
}

std::vector<SearchMatch> SearchEngine::SearchLine(
    const std::wstring& lineText,
    const std::wstring& query,
    int rowIndex,
    bool useRegex,
    bool caseSensitive
) {
    std::vector<SearchMatch> matches;

    if (query.empty() || lineText.empty()) {
        return matches;
    }

    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!caseSensitive) {
                flags |= std::regex::icase;
            }

            std::wregex regex(query, flags);
            auto begin = std::wsregex_iterator(lineText.begin(), lineText.end(), regex);
            auto end = std::wsregex_iterator();

            for (auto it = begin; it != end; ++it) {
                const std::wsmatch& match = *it;
                matches.push_back({
                    static_cast<int>(match.position()),
                    rowIndex,
                    static_cast<int>(match.length())
                });
            }
        } catch (const std::regex_error&) {
            // This shouldn't happen if Search() validated the regex first
            // but handle it gracefully
        }
    } else {
        // Plain text search
        std::wstring searchText = lineText;
        std::wstring searchQuery = query;

        if (!caseSensitive) {
            std::transform(searchText.begin(), searchText.end(), searchText.begin(), towlower);
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), towlower);
        }

        size_t pos = 0;
        int queryLen = static_cast<int>(searchQuery.length());

        while ((pos = searchText.find(searchQuery, pos)) != std::wstring::npos) {
            matches.push_back({
                static_cast<int>(pos),
                rowIndex,
                queryLen
            });
            pos += 1;  // Move past this match to find overlapping matches
        }
    }

    return matches;
}

SearchResult SearchEngine::Search(
    Terminal::ScreenBuffer& buffer,
    const std::wstring& query,
    bool useRegex,
    bool caseSensitive,
    bool includeScrollback
) {
    SearchResult result;

    if (query.empty()) {
        return result;
    }

    // Validate regex if needed
    if (useRegex) {
        try {
            std::wregex::flag_type flags = std::regex::ECMAScript;
            if (!caseSensitive) {
                flags |= std::regex::icase;
            }
            std::wregex testRegex(query, flags);
        } catch (const std::regex_error& e) {
            result.isValid = false;
            // Convert error message to wide string
            std::string msg = e.what();
            result.errorMessage = std::wstring(msg.begin(), msg.end());
            spdlog::warn("Invalid regex pattern: {}", msg);
            return result;
        }
    }

    int cols, rows;
    buffer.GetDimensions(cols, rows);
    int scrollbackUsed = buffer.GetScrollbackUsed();

    // Calculate search range
    // Scrollback lines are negative: -scrollbackUsed to -1
    // Visible lines are: 0 to rows-1
    int startRow = includeScrollback ? -scrollbackUsed : 0;
    int endRow = rows;

    spdlog::debug("Searching rows {} to {} (scrollback: {})", startRow, endRow - 1, scrollbackUsed);

    // Search each row
    for (int row = startRow; row < endRow; ++row) {
        std::wstring lineText = GetRowText(buffer, row);
        auto lineMatches = SearchLine(lineText, query, row, useRegex, caseSensitive);
        result.matches.insert(result.matches.end(), lineMatches.begin(), lineMatches.end());
    }

    spdlog::info("Search found {} matches", result.matches.size());

    return result;
}

} // namespace TerminalDX12::UI
