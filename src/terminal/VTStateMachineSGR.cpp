// VTStateMachineSGR.cpp - SGR (Select Graphic Rendition) handling
// Part of VTStateMachine implementation

#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>

namespace TerminalDX12::Terminal {

namespace {
// Map 256-color palette index to 16-color palette index
int Map256ColorTo16(int colorIndex) {
    if (colorIndex < 16) return colorIndex;
    if (colorIndex >= 232) {
        // Grayscale ramp (232-255) -> map to closest standard color
        int gray = (colorIndex - 232) * 255 / 23;
        if (gray < 64) return 0;        // Black
        if (gray < 192) return 8;       // Gray
        return 7;                        // White
    }
    // Color cube (16-231) -> map to closest 16-color
    int idx = colorIndex - 16;
    int r = idx / 36, g = (idx % 36) / 6, b = idx % 6;
    if (r > g && r > b) return (r > 3) ? 9 : 1;        // Red
    if (g > r && g > b) return (g > 3) ? 10 : 2;       // Green
    if (b > r && b > g) return (b > 3) ? 12 : 4;       // Blue
    if (r == g && r > b) return (r > 3) ? 11 : 3;      // Yellow
    if (r == b && r > g) return (r > 3) ? 13 : 5;      // Magenta
    if (g == b && g > r) return (g > 3) ? 14 : 6;      // Cyan
    return (r > 2) ? 15 : 7;                            // White/Gray
}
} // anonymous namespace

void VTStateMachine::HandleSGR() {
    // If no parameters, default to 0 (reset)
    if (m_params.empty()) {
        m_params.push_back(0);
    }

    // Debug log SGR parameters
    std::string paramStr;
    for (size_t i = 0; i < m_params.size(); ++i) {
        if (i > 0) paramStr += ";";
        paramStr += std::to_string(m_params[i]);
    }
    spdlog::debug("SGR sequence: ESC[{}m", paramStr);

    CellAttributes attr = m_screenBuffer->GetCurrentAttributes();

    for (size_t i = 0; i < m_params.size(); ++i) {
        int param = m_params[i];
        switch (param) {
            case 0:  // Reset
                attr.foreground = 7;   // White
                attr.background = 0;   // Black
                attr.flags = 0;
                attr.flags2 = 0;
                attr.underlineStyle = CellAttributes::UnderlineStyle::None;
                break;

            case 1:  // Bold
                attr.flags |= CellAttributes::FLAG_BOLD;
                break;

            case 2:  // Dim/Faint
                attr.flags |= CellAttributes::FLAG_DIM;
                break;

            case 3:  // Italic
                attr.flags |= CellAttributes::FLAG_ITALIC;
                break;

            case 4:  // Underline (single)
                attr.underlineStyle = CellAttributes::UnderlineStyle::Single;
                attr.flags |= CellAttributes::FLAG_UNDERLINE;
                break;

            case 5:  // Slow Blink
            case 6:  // Rapid Blink (treat same as slow blink)
                attr.flags2 |= CellAttributes::FLAG2_BLINK;
                break;

            case 7:  // Inverse
                attr.flags |= CellAttributes::FLAG_INVERSE;
                break;

            case 8:  // Hidden/Invisible
                attr.flags2 |= CellAttributes::FLAG2_HIDDEN;
                break;

            case 9:  // Strikethrough
                attr.flags |= CellAttributes::FLAG_STRIKETHROUGH;
                break;

            case 21:  // Double underline
                attr.underlineStyle = CellAttributes::UnderlineStyle::Double;
                attr.flags |= CellAttributes::FLAG_UNDERLINE;
                break;

            case 22:  // Not bold/dim - clears both
                attr.flags &= ~(CellAttributes::FLAG_BOLD | CellAttributes::FLAG_DIM);
                break;

            case 23:  // Not italic
                attr.flags &= ~CellAttributes::FLAG_ITALIC;
                break;

            case 24:  // Not underline
                attr.underlineStyle = CellAttributes::UnderlineStyle::None;
                attr.flags &= ~CellAttributes::FLAG_UNDERLINE;
                break;

            case 25:  // Not blinking
                attr.flags2 &= ~CellAttributes::FLAG2_BLINK;
                break;

            case 27:  // Not inverse
                attr.flags &= ~CellAttributes::FLAG_INVERSE;
                break;

            case 28:  // Not hidden
                attr.flags2 &= ~CellAttributes::FLAG2_HIDDEN;
                break;

            case 29:  // Not strikethrough
                attr.flags &= ~CellAttributes::FLAG_STRIKETHROUGH;
                break;

            // Foreground colors (30-37 for standard, 90-97 for bright)
            case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
                attr.foreground = param - 30;
                break;

            case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
                attr.foreground = param - 90 + 8;  // Bright colors are 8-15
                break;

            // Extended foreground color: 38;5;N (256-color) or 38;2;R;G;B (true color)
            case 38:
                if (i + 1 < m_params.size()) {
                    if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                        attr.foreground = Map256ColorTo16(m_params[i + 2]);
                        i += 2;
                    } else if (m_params[i + 1] == 2 && i + 4 < m_params.size()) {
                        attr.SetForegroundRGB(
                            static_cast<uint8_t>(m_params[i + 2]),
                            static_cast<uint8_t>(m_params[i + 3]),
                            static_cast<uint8_t>(m_params[i + 4]));
                        i += 4;
                    }
                }
                break;

            // Background colors (40-47 for standard, 100-107 for bright)
            case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
                attr.background = param - 40;
                break;

            case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
                attr.background = param - 100 + 8;
                break;

            // Extended background color: 48;5;N (256-color) or 48;2;R;G;B (true color)
            case 48:
                if (i + 1 < m_params.size()) {
                    if (m_params[i + 1] == 5 && i + 2 < m_params.size()) {
                        attr.background = Map256ColorTo16(m_params[i + 2]);
                        i += 2;
                    } else if (m_params[i + 1] == 2 && i + 4 < m_params.size()) {
                        attr.SetBackgroundRGB(
                            static_cast<uint8_t>(m_params[i + 2]),
                            static_cast<uint8_t>(m_params[i + 3]),
                            static_cast<uint8_t>(m_params[i + 4]));
                        i += 4;
                    }
                }
                break;

            case 39:  // Default foreground
                attr.foreground = 7;
                break;

            case 49:  // Default background
                attr.background = 0;
                break;
        }
    }

    m_screenBuffer->SetCurrentAttributes(attr);
}

} // namespace TerminalDX12::Terminal
