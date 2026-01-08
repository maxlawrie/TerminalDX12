#include "ui/SelectionManager.h"
#include "terminal/ScreenBuffer.h"
#include "pty/ConPtySession.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace TerminalDX12::UI {

void SelectionManager::ClearSelection() {
    m_hasSelection = false;
    m_selecting = false;
    m_rectangleSelection = false;
    m_selectionStart = {0, 0};
    m_selectionEnd = {0, 0};
}

void SelectionManager::StartSelection(SelectionPos pos, bool rectangleMode) {
    m_rectangleSelection = rectangleMode;
    m_selectionStart = pos;
    m_selectionEnd = pos;
    m_selecting = true;
    m_hasSelection = false;
}

void SelectionManager::ExtendSelection(SelectionPos pos) {
    m_selectionEnd = pos;
    // Mark as having selection if we've moved from start
    if (m_selectionStart.x != m_selectionEnd.x || m_selectionStart.y != m_selectionEnd.y) {
        m_hasSelection = true;
    }
}

void SelectionManager::EndSelection() {
    m_selecting = false;
    // Check if we actually selected something (not just a click)
    if (m_selectionStart.x != m_selectionEnd.x || m_selectionStart.y != m_selectionEnd.y) {
        m_hasSelection = true;
    }
}

std::string SelectionManager::GetSelectedText(Terminal::ScreenBuffer* screenBuffer) const {
    if (!m_hasSelection || !screenBuffer) {
        return "";
    }

    // Normalize selection (ensure start <= end)
    int startY = std::min(m_selectionStart.y, m_selectionEnd.y);
    int endY = std::max(m_selectionStart.y, m_selectionEnd.y);
    int startX = std::min(m_selectionStart.x, m_selectionEnd.x);
    int endX = std::max(m_selectionStart.x, m_selectionEnd.x);

    std::u32string text;
    int cols = screenBuffer->GetCols();

    if (m_rectangleSelection) {
        // Rectangle selection: same column range for all rows
        for (int y = startY; y <= endY; ++y) {
            for (int x = startX; x <= endX; ++x) {
                auto cell = screenBuffer->GetCellWithScrollback(x, y);
                text += cell.ch;
            }
            // Add newline between rows (but not after last row)
            if (y < endY) {
                // Trim trailing spaces from line before adding newline
                while (!text.empty() && text.back() == U' ') {
                    text.pop_back();
                }
                text += U'\n';
            }
        }
    } else {
        // Normal line selection
        // For normal selection, use full coordinates
        int normalStartX, normalEndX;
        if (m_selectionStart.y < m_selectionEnd.y ||
            (m_selectionStart.y == m_selectionEnd.y && m_selectionStart.x <= m_selectionEnd.x)) {
            normalStartX = m_selectionStart.x;
            normalEndX = m_selectionEnd.x;
        } else {
            normalStartX = m_selectionEnd.x;
            normalEndX = m_selectionStart.x;
        }

        for (int y = startY; y <= endY; ++y) {
            int rowStartX = (y == startY) ? normalStartX : 0;
            int rowEndX = (y == endY) ? normalEndX : cols - 1;

            for (int x = rowStartX; x <= rowEndX; ++x) {
                auto cell = screenBuffer->GetCellWithScrollback(x, y);
                text += cell.ch;
            }

            // Add newline between rows (but not after last row)
            if (y < endY) {
                // Trim trailing spaces from line before adding newline
                while (!text.empty() && text.back() == U' ') {
                    text.pop_back();
                }
                text += U'\n';
            }
        }
    }

    // Trim trailing spaces from last line
    while (!text.empty() && text.back() == U' ') {
        text.pop_back();
    }

    // Convert to UTF-8
    std::string utf8;
    for (char32_t ch : text) {
        if (ch < 0x80) {
            utf8 += static_cast<char>(ch);
        } else if (ch < 0x800) {
            utf8 += static_cast<char>(0xC0 | (ch >> 6));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        } else if (ch < 0x10000) {
            utf8 += static_cast<char>(0xE0 | (ch >> 12));
            utf8 += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            utf8 += static_cast<char>(0xF0 | (ch >> 18));
            utf8 += static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
            utf8 += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            utf8 += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }

    return utf8;
}

void SelectionManager::CopySelectionToClipboard(Terminal::ScreenBuffer* screenBuffer, HWND hwnd) {
    if (!m_hasSelection || !hwnd) {
        return;
    }

    std::string text = GetSelectedText(screenBuffer);
    if (text.empty()) {
        return;
    }

    // Convert UTF-8 to wide string for clipboard
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) {
        return;
    }

    if (!OpenClipboard(hwnd)) {
        spdlog::error("Failed to open clipboard");
        return;
    }

    EmptyClipboard();

    // Allocate global memory for clipboard
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, wideLen * sizeof(wchar_t));
    if (!hGlobal) {
        CloseClipboard();
        spdlog::error("Failed to allocate clipboard memory");
        return;
    }

    wchar_t* pGlobal = static_cast<wchar_t*>(GlobalLock(hGlobal));
    if (pGlobal) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, pGlobal, wideLen);
        GlobalUnlock(hGlobal);
        SetClipboardData(CF_UNICODETEXT, hGlobal);
        spdlog::info("Copied {} characters to clipboard", text.length());
    }

    CloseClipboard();

    // Clear selection after copy
    ClearSelection();
}

void SelectionManager::PasteFromClipboard(Pty::ConPtySession* terminal, HWND hwnd) {
    if (!terminal || !terminal->IsRunning() || !hwnd) {
        return;
    }

    if (!OpenClipboard(hwnd)) {
        spdlog::error("Failed to open clipboard");
        return;
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (!hData) {
        CloseClipboard();
        return;
    }

    const wchar_t* pData = static_cast<const wchar_t*>(GlobalLock(hData));
    if (pData) {
        // Convert wide string to UTF-8
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0) {
            std::string utf8(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, pData, -1, &utf8[0], utf8Len, nullptr, nullptr);

            // Remove null terminator
            if (!utf8.empty() && utf8.back() == '\0') {
                utf8.pop_back();
            }

            // Send to terminal
            terminal->WriteInput(utf8.c_str(), utf8.length());
            spdlog::info("Pasted {} characters from clipboard", utf8.length());
        }
        GlobalUnlock(hData);
    }

    CloseClipboard();
}

std::string SelectionManager::GetClipboardText(HWND hwnd) {
    if (!hwnd) return "";

    if (!OpenClipboard(hwnd)) {
        return "";
    }

    std::string result;
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        const wchar_t* pData = static_cast<const wchar_t*>(GlobalLock(hData));
        if (pData) {
            int utf8Len = WideCharToMultiByte(CP_UTF8, 0, pData, -1, nullptr, 0, nullptr, nullptr);
            if (utf8Len > 0) {
                result.resize(utf8Len);
                WideCharToMultiByte(CP_UTF8, 0, pData, -1, &result[0], utf8Len, nullptr, nullptr);
                if (!result.empty() && result.back() == '\0') {
                    result.pop_back();
                }
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
    return result;
}

void SelectionManager::SetClipboardText(const std::string& text, HWND hwnd) {
    if (!hwnd) return;

    // Convert UTF-8 to wide string
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (wideLen <= 0) return;

    if (!OpenClipboard(hwnd)) return;
    EmptyClipboard();

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, (wideLen + 1) * sizeof(wchar_t));
    if (hGlobal) {
        wchar_t* pGlobal = static_cast<wchar_t*>(GlobalLock(hGlobal));
        if (pGlobal) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), pGlobal, wideLen);
            pGlobal[wideLen] = L'\0';
            GlobalUnlock(hGlobal);
            SetClipboardData(CF_UNICODETEXT, hGlobal);
            spdlog::debug("OSC 52: Set clipboard with {} chars", text.size());
        }
    }
    CloseClipboard();
}

bool SelectionManager::IsWordChar(char32_t ch) const {
    // Word characters: alphanumeric, underscore, and some extended chars
    if (ch >= U'a' && ch <= U'z') return true;
    if (ch >= U'A' && ch <= U'Z') return true;
    if (ch >= U'0' && ch <= U'9') return true;
    if (ch == U'_' || ch == U'-') return true;
    // Allow URL-like characters for URL selection
    if (ch == U'/' || ch == U':' || ch == U'.' || ch == U'@' ||
        ch == U'?' || ch == U'=' || ch == U'&' || ch == U'%' ||
        ch == U'+' || ch == U'#' || ch == U'~') return true;
    return false;
}

void SelectionManager::SelectWord(Terminal::ScreenBuffer* screenBuffer, int cellX, int cellY) {
    if (!screenBuffer) return;

    int cols = screenBuffer->GetCols();

    // Get character at click position
    auto cell = screenBuffer->GetCellWithScrollback(cellX, cellY);

    // If clicking on whitespace, don't select anything
    if (cell.ch == U' ' || cell.ch == 0) {
        ClearSelection();
        return;
    }

    // Find word boundaries
    int startX = cellX;
    int endX = cellX;

    // Scan left
    while (startX > 0) {
        auto prevCell = screenBuffer->GetCellWithScrollback(startX - 1, cellY);
        if (!IsWordChar(prevCell.ch)) break;
        startX--;
    }

    // Scan right
    while (endX < cols - 1) {
        auto nextCell = screenBuffer->GetCellWithScrollback(endX + 1, cellY);
        if (!IsWordChar(nextCell.ch)) break;
        endX++;
    }

    // Set selection
    m_selectionStart = {startX, cellY};
    m_selectionEnd = {endX, cellY};
    m_hasSelection = (startX != endX);
    m_selecting = false;
}

void SelectionManager::SelectLine(Terminal::ScreenBuffer* screenBuffer, int cellY) {
    if (!screenBuffer) return;

    int cols = screenBuffer->GetCols();

    // Select entire line
    m_selectionStart = {0, cellY};
    m_selectionEnd = {cols - 1, cellY};
    m_hasSelection = true;
    m_selecting = false;
}

void SelectionManager::ShowContextMenu(int x, int y, HWND hwnd, Terminal::ScreenBuffer* screenBuffer,
                                       Pty::ConPtySession* terminal) {
    if (!hwnd) return;

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    // Menu item IDs
    constexpr UINT ID_COPY = 1;
    constexpr UINT ID_PASTE = 2;
    constexpr UINT ID_SELECT_ALL = 3;

    // Add menu items
    UINT copyFlags = MF_STRING | (m_hasSelection ? 0 : MF_GRAYED);
    AppendMenuW(hMenu, copyFlags, ID_COPY, L"Copy\tCtrl+C");

    // Check if clipboard has text
    bool hasClipboardText = false;
    if (OpenClipboard(hwnd)) {
        hasClipboardText = (GetClipboardData(CF_UNICODETEXT) != nullptr);
        CloseClipboard();
    }
    UINT pasteFlags = MF_STRING | (hasClipboardText ? 0 : MF_GRAYED);
    AppendMenuW(hMenu, pasteFlags, ID_PASTE, L"Paste\tCtrl+V");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_SELECT_ALL, L"Select All");

    // Get screen coordinates
    POINT pt = {x, y};
    ClientToScreen(hwnd, &pt);

    // Show menu and get selection
    UINT cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                              pt.x, pt.y, 0, hwnd, nullptr);

    DestroyMenu(hMenu);

    // Handle selection
    switch (cmd) {
        case ID_COPY:
            CopySelectionToClipboard(screenBuffer, hwnd);
            break;
        case ID_PASTE:
            PasteFromClipboard(terminal, hwnd);
            break;
        case ID_SELECT_ALL:
            if (screenBuffer) {
                m_selectionStart = {0, 0};
                m_selectionEnd = {screenBuffer->GetCols() - 1, screenBuffer->GetRows() - 1};
                m_hasSelection = true;
                m_selecting = false;
            }
            break;
    }
}

} // namespace TerminalDX12::UI
