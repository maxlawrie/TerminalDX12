#pragma once

#include <string>

namespace TerminalDX12 {

namespace Terminal {
    class ScreenBuffer;
}

namespace UI {

/**
 * @brief Utility class for URL detection and handling in terminal text.
 */
class UrlHelper {
public:
    /**
     * @brief Detect a URL at the given cell position.
     * @param screenBuffer The screen buffer to search in.
     * @param cellX The X cell coordinate.
     * @param cellY The Y cell coordinate.
     * @return The detected URL, or empty string if none found.
     */
    static std::string DetectUrlAt(Terminal::ScreenBuffer* screenBuffer, int cellX, int cellY);

    /**
     * @brief Open a URL in the default browser.
     * @param url The URL to open.
     */
    static void OpenUrl(const std::string& url);

    /**
     * @brief Check if a character is valid in a URL.
     * @param ch The character to check.
     * @return True if the character is valid in a URL.
     */
    static bool IsUrlChar(char ch);
};

} // namespace UI
} // namespace TerminalDX12
