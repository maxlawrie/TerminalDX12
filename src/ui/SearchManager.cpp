#include "ui/SearchManager.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <cwctype>

namespace TerminalDX12::UI {

void SearchManager::Open() {
    m_active = true;
    m_query.clear();
    m_matches.clear();
    m_currentIndex = -1;
    spdlog::info("Search mode opened");
}

void SearchManager::Close() {
    m_active = false;
    m_query.clear();
    m_matches.clear();
    m_currentIndex = -1;
    spdlog::info("Search mode closed");
}

void SearchManager::AppendChar(wchar_t ch) {
    m_query += ch;
}

void SearchManager::Backspace() {
    if (!m_query.empty()) {
        m_query.pop_back();
    }
}

void SearchManager::UpdateResults(Terminal::ScreenBuffer* screenBuffer) {
    m_matches.clear();
    m_currentIndex = -1;

    if (!screenBuffer || m_query.empty()) {
        return;
    }

    // Convert search query to lowercase for case-insensitive search
    std::wstring queryLower = m_query;
    for (auto& ch : queryLower) {
        ch = std::towlower(ch);
    }

    int rows, cols;
    screenBuffer->GetDimensions(cols, rows);
    int queryLen = static_cast<int>(queryLower.length());

    // Search through visible area
    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x <= cols - queryLen; ++x) {
            bool match = true;
            for (int i = 0; i < queryLen && match; ++i) {
                auto cell = screenBuffer->GetCellWithScrollback(x + i, y);
                wchar_t cellChar = static_cast<wchar_t>(cell.ch);
                if (std::towlower(cellChar) != queryLower[i]) {
                    match = false;
                }
            }
            if (match) {
                m_matches.push_back({x, y});
            }
        }
    }

    // If we found matches, select the first one
    if (!m_matches.empty()) {
        m_currentIndex = 0;
    }

    spdlog::info("Search found {} matches", m_matches.size());
}

void SearchManager::NextMatch() {
    if (m_matches.empty()) {
        return;
    }

    m_currentIndex = (m_currentIndex + 1) % static_cast<int>(m_matches.size());
    spdlog::debug("Search: next match {} of {}", m_currentIndex + 1, m_matches.size());
}

void SearchManager::PreviousMatch() {
    if (m_matches.empty()) {
        return;
    }

    m_currentIndex--;
    if (m_currentIndex < 0) {
        m_currentIndex = static_cast<int>(m_matches.size()) - 1;
    }
    spdlog::debug("Search: previous match {} of {}", m_currentIndex + 1, m_matches.size());
}

} // namespace TerminalDX12::UI
