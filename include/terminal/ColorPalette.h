#pragma once

#include <cstdint>

namespace TerminalDX12::Terminal {

/**
 * @brief Color palette entry (OSC 4)
 */
struct PaletteColor {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    bool modified = false;  // True if this entry was set via OSC 4
};

/**
 * @brief 256-color palette manager with OSC 4 support
 *
 * Manages the terminal's 256-color palette, allowing colors to be
 * modified via OSC 4 sequences and tracking which colors have been
 * customized vs using defaults.
 */
class ColorPalette {
public:
    static constexpr int kPaletteSize = 256;

    ColorPalette() = default;

    /**
     * @brief Set a palette color (OSC 4)
     */
    void SetColor(int index, uint8_t r, uint8_t g, uint8_t b);

    /**
     * @brief Get a palette color
     */
    const PaletteColor& GetColor(int index) const;

    /**
     * @brief Check if a color was modified via OSC 4
     */
    bool IsColorModified(int index) const;

    /**
     * @brief Reset a single color to unmodified state
     */
    void ResetColor(int index);

    /**
     * @brief Reset all colors to unmodified state
     */
    void ResetAll();

private:
    PaletteColor m_colors[kPaletteSize];
};

} // namespace TerminalDX12::Terminal
