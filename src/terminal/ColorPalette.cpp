#include "terminal/ColorPalette.h"

namespace TerminalDX12::Terminal {

void ColorPalette::SetColor(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= kPaletteSize) return;
    m_colors[index].r = r;
    m_colors[index].g = g;
    m_colors[index].b = b;
    m_colors[index].modified = true;
}

const PaletteColor& ColorPalette::GetColor(int index) const {
    static PaletteColor defaultColor;
    if (index < 0 || index >= kPaletteSize) return defaultColor;
    return m_colors[index];
}

bool ColorPalette::IsColorModified(int index) const {
    if (index < 0 || index >= kPaletteSize) return false;
    return m_colors[index].modified;
}

void ColorPalette::ResetColor(int index) {
    if (index < 0 || index >= kPaletteSize) return;
    m_colors[index].modified = false;
}

void ColorPalette::ResetAll() {
    for (int i = 0; i < kPaletteSize; ++i) {
        m_colors[i].modified = false;
    }
}

} // namespace TerminalDX12::Terminal
