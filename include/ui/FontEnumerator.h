#pragma once

/**
 * @file FontEnumerator.h
 * @brief Enumerates monospace fonts available on the system using DirectWrite.
 */

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace TerminalDX12::UI {

/**
 * @brief Information about a system font.
 */
struct FontInfo {
    std::wstring familyName;    ///< Font family name (e.g., "Consolas")
    bool isMonospace = false;   ///< True if glyph widths are uniform
};

/**
 * @brief Enumerates system fonts, filtering for monospace fonts suitable for terminal use.
 *
 * Uses DirectWrite to enumerate fonts and detect monospace by comparing glyph widths
 * of 'i' and 'M' characters.
 */
class FontEnumerator {
public:
    FontEnumerator();
    ~FontEnumerator();

    /**
     * @brief Get all monospace fonts available on the system.
     * @return Vector of FontInfo for monospace fonts, sorted alphabetically.
     */
    std::vector<FontInfo> EnumerateMonospaceFonts();

    /**
     * @brief Check if a specific font family exists on the system.
     * @param familyName The font family name to check.
     * @return True if the font exists.
     */
    bool FontExists(const std::wstring& familyName);

    /**
     * @brief Get a fallback font if the requested font doesn't exist.
     * @param requestedFamily The originally requested font family.
     * @return The font to use (requestedFamily if it exists, otherwise "Consolas").
     *
     * Logs a warning if fallback is used.
     */
    std::wstring GetFontOrFallback(const std::wstring& requestedFamily);

    /**
     * @brief Default fallback font name.
     */
    static constexpr wchar_t kDefaultFont[] = L"Consolas";

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace TerminalDX12::UI
