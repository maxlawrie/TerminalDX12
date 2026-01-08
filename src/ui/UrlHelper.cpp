#include "ui/UrlHelper.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <Windows.h>
#include <shellapi.h>

namespace TerminalDX12::UI {

bool UrlHelper::IsUrlChar(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '-' || ch == '.' || ch == '_' || ch == '~' ||
           ch == ':' || ch == '/' || ch == '?' || ch == '#' ||
           ch == '[' || ch == ']' || ch == '@' || ch == '!' ||
           ch == '$' || ch == '&' || ch == '\'' || ch == '(' ||
           ch == ')' || ch == '*' || ch == '+' || ch == ',' ||
           ch == ';' || ch == '=' || ch == '%';
}

std::string UrlHelper::DetectUrlAt(Terminal::ScreenBuffer* screenBuffer, int cellX, int cellY) {
    if (!screenBuffer) return "";

    int cols = screenBuffer->GetCols();

    // Build the text around the clicked position
    std::string lineText;
    for (int x = 0; x < cols; ++x) {
        auto cell = screenBuffer->GetCellWithScrollback(x, cellY);
        if (cell.ch < 128 && cell.ch > 0) {
            lineText += static_cast<char>(cell.ch);
        } else {
            lineText += ' ';  // Replace non-ASCII with space
        }
    }

    // URL prefixes to detect
    const char* prefixes[] = {"https://", "http://", "file://", "ftp://"};

    // Find all URLs in the line and check if cellX is within any of them
    for (const char* prefix : prefixes) {
        size_t pos = 0;
        while ((pos = lineText.find(prefix, pos)) != std::string::npos) {
            // Find the end of the URL
            size_t urlEnd = pos;
            while (urlEnd < lineText.length() && IsUrlChar(lineText[urlEnd])) {
                urlEnd++;
            }

            // Check if the clicked position is within this URL
            if (cellX >= static_cast<int>(pos) && cellX < static_cast<int>(urlEnd)) {
                return lineText.substr(pos, urlEnd - pos);
            }

            pos = urlEnd;
        }
    }

    return "";
}

void UrlHelper::OpenUrl(const std::string& url) {
    if (url.empty()) return;

    // Convert to wide string for ShellExecute
    int wideLen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    if (wideLen <= 0) return;

    std::wstring wideUrl(wideLen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wideUrl[0], wideLen);

    // Open URL in default browser
    HINSTANCE result = ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) > 32) {
        spdlog::info("Opened URL: {}", url);
    } else {
        spdlog::error("Failed to open URL: {}", url);
    }
}

} // namespace TerminalDX12::UI
