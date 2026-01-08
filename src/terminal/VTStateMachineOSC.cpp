// VTStateMachineOSC.cpp - OSC (Operating System Command) handling
// Part of VTStateMachine implementation

#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <spdlog/spdlog.h>
#include <vector>

namespace TerminalDX12::Terminal {

// Base64 encoding table
static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::string& input) {
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);

    for (size_t i = 0; i < input.size(); i += 3) {
        uint32_t n = static_cast<uint8_t>(input[i]) << 16;
        if (i + 1 < input.size()) n |= static_cast<uint8_t>(input[i + 1]) << 8;
        if (i + 2 < input.size()) n |= static_cast<uint8_t>(input[i + 2]);

        output.push_back(base64_chars[(n >> 18) & 0x3F]);
        output.push_back(base64_chars[(n >> 12) & 0x3F]);
        output.push_back((i + 1 < input.size()) ? base64_chars[(n >> 6) & 0x3F] : '=');
        output.push_back((i + 2 < input.size()) ? base64_chars[n & 0x3F] : '=');
    }
    return output;
}

static std::string Base64Decode(const std::string& input) {
    std::string output;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) T[static_cast<unsigned char>(base64_chars[i])] = i;

    int val = 0, valb = -8;
    for (unsigned char c : input) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return output;
}

void VTStateMachine::HandleOSC() {
    // OSC sequences: ESC ] Ps ; Pt BEL
    if (m_oscBuffer.empty()) {
        return;
    }

    // Parse the OSC type (number before first semicolon)
    size_t semicolon = m_oscBuffer.find(';');
    if (semicolon != std::string::npos) {
        std::string type = m_oscBuffer.substr(0, semicolon);
        std::string value = m_oscBuffer.substr(semicolon + 1);

        spdlog::debug("OSC sequence: type={}, value=\"{}\"", type, value);

        if (type == "133") {
            HandleOSC133(value);
        } else if (type == "10") {
            HandleOSC10(value);
        } else if (type == "11") {
            HandleOSC11(value);
        } else if (type == "8") {
            HandleOSC8(value);
        } else if (type == "52") {
            HandleOSC52(value);
        } else if (type == "4") {
            HandleOSC4(value);
        }
        // OSC 0, 1, 2 are window title - we could handle these if desired
    }

    m_oscBuffer.clear();
}

void VTStateMachine::HandleOSC133(const std::string& param) {
    // OSC 133 shell integration:
    // A = Prompt start, B = Prompt end, C = Command start, D = Command finished
    if (param.empty()) return;

    char marker = param[0];
    switch (marker) {
        case 'A':
            m_screenBuffer->MarkPromptStart();
            break;
        case 'B':
            m_screenBuffer->MarkInputStart();
            break;
        case 'C':
            m_screenBuffer->MarkCommandStart();
            break;
        case 'D': {
            int exitCode = -1;
            if (param.length() > 2 && param[1] == ';') {
                try {
                    exitCode = std::stoi(param.substr(2));
                } catch (...) {
                    exitCode = -1;
                }
            }
            m_screenBuffer->MarkCommandEnd(exitCode);
            break;
        }
        default:
            spdlog::debug("Unknown OSC 133 marker: {}", marker);
            break;
    }
}

void VTStateMachine::HandleOSC8(const std::string& param) {
    // OSC 8 - Hyperlink: OSC 8 ; params ; uri ST
    size_t semicolon = param.find(';');
    if (semicolon == std::string::npos) {
        return;
    }

    std::string params = param.substr(0, semicolon);
    std::string uri = param.substr(semicolon + 1);

    // Parse optional ID from params
    std::string linkId;
    if (!params.empty()) {
        size_t pos = 0;
        while (pos < params.length()) {
            size_t colonPos = params.find(':', pos);
            std::string kvPair = (colonPos == std::string::npos)
                ? params.substr(pos)
                : params.substr(pos, colonPos - pos);

            size_t eqPos = kvPair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = kvPair.substr(0, eqPos);
                std::string value = kvPair.substr(eqPos + 1);
                if (key == "id") {
                    linkId = value;
                }
            }

            if (colonPos == std::string::npos) break;
            pos = colonPos + 1;
        }
    }

    if (uri.empty()) {
        m_screenBuffer->ClearCurrentHyperlink();
        spdlog::debug("OSC 8: End hyperlink");
    } else {
        m_screenBuffer->AddHyperlink(uri, linkId);
        spdlog::debug("OSC 8: Start hyperlink uri=\"{}\" id=\"{}\"", uri, linkId);
    }
}

bool VTStateMachine::ParseOSCColor(const std::string& colorStr, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (colorStr.empty()) return false;

    if (colorStr[0] == '#' && colorStr.length() == 7) {
        try {
            r = static_cast<uint8_t>(std::stoi(colorStr.substr(1, 2), nullptr, 16));
            g = static_cast<uint8_t>(std::stoi(colorStr.substr(3, 2), nullptr, 16));
            b = static_cast<uint8_t>(std::stoi(colorStr.substr(5, 2), nullptr, 16));
            return true;
        } catch (...) {
            return false;
        }
    }

    if (colorStr.substr(0, 4) == "rgb:") {
        std::string rgb = colorStr.substr(4);
        size_t slash1 = rgb.find('/');
        size_t slash2 = rgb.rfind('/');
        if (slash1 != std::string::npos && slash2 != std::string::npos && slash1 != slash2) {
            try {
                std::string rStr = rgb.substr(0, slash1);
                std::string gStr = rgb.substr(slash1 + 1, slash2 - slash1 - 1);
                std::string bStr = rgb.substr(slash2 + 1);

                int rVal = std::stoi(rStr, nullptr, 16);
                int gVal = std::stoi(gStr, nullptr, 16);
                int bVal = std::stoi(bStr, nullptr, 16);

                if (rStr.length() == 4) rVal = rVal >> 8;
                if (gStr.length() == 4) gVal = gVal >> 8;
                if (bStr.length() == 4) bVal = bVal >> 8;

                r = static_cast<uint8_t>(rVal);
                g = static_cast<uint8_t>(gVal);
                b = static_cast<uint8_t>(bVal);
                return true;
            } catch (...) {
                return false;
            }
        }
    }

    return false;
}

void VTStateMachine::HandleOSC10(const std::string& param) {
    if (param == "?") {
        char response[32];
        snprintf(response, sizeof(response), "]10;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                 m_themeFgR, m_themeFgR, m_themeFgG, m_themeFgG, m_themeFgB, m_themeFgB);
        SendResponse(response);
    } else {
        uint8_t r, g, b;
        if (ParseOSCColor(param, r, g, b)) {
            m_themeFgR = r;
            m_themeFgG = g;
            m_themeFgB = b;
            m_hasThemeFg = true;
            spdlog::debug("OSC 10: Set foreground color to #{:02x}{:02x}{:02x}", r, g, b);
        }
    }
}

void VTStateMachine::HandleOSC11(const std::string& param) {
    if (param == "?") {
        char response[32];
        snprintf(response, sizeof(response), "]11;rgb:%02x%02x/%02x%02x/%02x%02x\x07",
                 m_themeBgR, m_themeBgR, m_themeBgG, m_themeBgG, m_themeBgB, m_themeBgB);
        SendResponse(response);
    } else {
        uint8_t r, g, b;
        if (ParseOSCColor(param, r, g, b)) {
            m_themeBgR = r;
            m_themeBgG = g;
            m_themeBgB = b;
            m_hasThemeBg = true;
            spdlog::debug("OSC 11: Set background color to #{:02x}{:02x}{:02x}", r, g, b);
        }
    }
}

void VTStateMachine::HandleOSC52(const std::string& param) {
    if (m_osc52Policy == Osc52Policy::Disabled) {
        spdlog::debug("OSC 52: Blocked by security policy (Disabled)");
        return;
    }

    size_t semicolon = param.find(';');
    if (semicolon == std::string::npos) {
        spdlog::debug("OSC 52: Invalid format (no semicolon)");
        return;
    }

    std::string selection = param.substr(0, semicolon);
    std::string data = param.substr(semicolon + 1);

    if (data == "?") {
        if (m_osc52Policy != Osc52Policy::ReadOnly && m_osc52Policy != Osc52Policy::ReadWrite) {
            spdlog::debug("OSC 52: Clipboard read blocked by security policy");
            return;
        }
        if (m_clipboardReadCallback) {
            std::string clipboardContent = m_clipboardReadCallback();
            std::string encoded = Base64Encode(clipboardContent);
            std::string response = "]52;" + selection + ";" + encoded + "\x07";
            SendResponse(response);
            spdlog::debug("OSC 52: Query clipboard, {} bytes", clipboardContent.size());
        }
    } else if (!data.empty()) {
        if (m_osc52Policy != Osc52Policy::WriteOnly && m_osc52Policy != Osc52Policy::ReadWrite) {
            spdlog::debug("OSC 52: Clipboard write blocked by security policy");
            return;
        }
        std::string decoded = Base64Decode(data);
        if (m_clipboardWriteCallback && !decoded.empty()) {
            m_clipboardWriteCallback(decoded);
            spdlog::debug("OSC 52: Set clipboard, {} bytes", decoded.size());
        }
    }
}

void VTStateMachine::HandleOSC4(const std::string& param) {
    size_t pos = 0;
    while (pos < param.length()) {
        size_t semicolon1 = param.find(';', pos);
        if (semicolon1 == std::string::npos) break;

        std::string indexStr = param.substr(pos, semicolon1 - pos);
        int index = 0;
        try {
            index = std::stoi(indexStr);
        } catch (...) {
            spdlog::debug("OSC 4: Invalid index: {}", indexStr);
            break;
        }

        if (index < 0 || index >= 256) {
            spdlog::debug("OSC 4: Index out of range: {}", index);
            break;
        }

        size_t semicolon2 = param.find(';', semicolon1 + 1);
        std::string colorStr;
        if (semicolon2 == std::string::npos) {
            colorStr = param.substr(semicolon1 + 1);
            pos = param.length();
        } else {
            colorStr = param.substr(semicolon1 + 1, semicolon2 - semicolon1 - 1);
            pos = semicolon2 + 1;
        }

        if (colorStr == "?") {
            const auto& paletteColor = m_screenBuffer->GetPaletteColor(index);
            char response[64];
            snprintf(response, sizeof(response), "]4;%d;rgb:%04x/%04x/%04x\x07",
                     index,
                     paletteColor.r * 257,
                     paletteColor.g * 257,
                     paletteColor.b * 257);
            SendResponse(response);
            spdlog::debug("OSC 4: Query color {}", index);
        } else {
            uint8_t r, g, b;
            if (ParseOSCColor(colorStr, r, g, b)) {
                m_screenBuffer->SetPaletteColor(index, r, g, b);
                spdlog::debug("OSC 4: Set color {} to #{:02x}{:02x}{:02x}", index, r, g, b);
            }
        }
    }
}

} // namespace TerminalDX12::Terminal
