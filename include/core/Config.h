#pragma once

#include <string>
#include <array>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace TerminalDX12::Core {

// Color represented as RGBA
struct Color {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;

    Color() = default;
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    // Parse from hex string like "#RRGGBB" or "#RRGGBBAA"
    static std::optional<Color> FromHex(const std::string& hex);
    std::string ToHex() const;
};

// Cursor style options
enum class CursorStyle {
    Block,
    Underline,
    Bar
};

// Font configuration
struct FontConfig {
    std::string family = "Consolas";
    int size = 16;
    int weight = 400;  // 400 = normal, 700 = bold
    bool ligatures = false;

    // Resolved font path (set during loading)
    std::string resolvedPath;
};

// Color palette configuration
struct ColorConfig {
    Color foreground{204, 204, 204};      // Light gray
    Color background{12, 12, 12};          // Near black
    Color cursor{0, 255, 0};               // Green
    Color selection{51, 153, 255, 128};    // Blue with alpha

    // 16-color ANSI palette (0-7 normal, 8-15 bright)
    std::array<Color, 16> palette = {{
        // Normal colors (0-7)
        {12, 12, 12},       // 0: Black
        {197, 15, 31},      // 1: Red
        {19, 161, 14},      // 2: Green
        {193, 156, 0},      // 3: Yellow
        {0, 55, 218},       // 4: Blue
        {136, 23, 152},     // 5: Magenta
        {58, 150, 221},     // 6: Cyan
        {204, 204, 204},    // 7: White
        // Bright colors (8-15)
        {118, 118, 118},    // 8: Bright Black
        {231, 72, 86},      // 9: Bright Red
        {22, 198, 12},      // 10: Bright Green
        {249, 241, 165},    // 11: Bright Yellow
        {59, 120, 255},     // 12: Bright Blue
        {180, 0, 158},      // 13: Bright Magenta
        {97, 214, 214},     // 14: Bright Cyan
        {242, 242, 242}     // 15: Bright White
    }};
};

// Key combination for keybindings
struct KeyBinding {
    std::string key;          // e.g., "c", "v", "up", "pageup"
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    static std::optional<KeyBinding> Parse(const std::string& binding);
    std::string ToString() const;
};

// Keybinding configuration
struct KeybindingConfig {
    std::unordered_map<std::string, KeyBinding> bindings;

    // Default keybindings
    void SetDefaults();
    std::optional<KeyBinding> GetBinding(const std::string& action) const;
};

// Terminal behavior configuration
struct TerminalConfig {
    std::wstring shell = L"powershell.exe";
    std::wstring workingDirectory;     // Empty = use current directory
    int scrollbackLines = 10000;
    CursorStyle cursorStyle = CursorStyle::Block;
    bool cursorBlink = true;
    int cursorBlinkMs = 530;
    float opacity = 1.0f;              // Window opacity (0.0-1.0)
};

// Main configuration class
class Config {
public:
    Config();
    ~Config() = default;

    // Load configuration from file
    bool Load(const std::wstring& path);

    // Load from default location (%APPDATA%\TerminalDX12\config.json)
    bool LoadDefault();

    // Save configuration to file
    bool Save(const std::wstring& path) const;

    // Create default config file if it doesn't exist
    bool CreateDefaultIfMissing();

    // Get configuration path
    static std::wstring GetDefaultConfigPath();

    // Accessors
    const FontConfig& GetFont() const { return m_font; }
    const ColorConfig& GetColors() const { return m_colors; }
    const KeybindingConfig& GetKeybindings() const { return m_keybindings; }
    const TerminalConfig& GetTerminal() const { return m_terminal; }

    // Mutable accessors for testing
    FontConfig& GetFontMut() { return m_font; }
    ColorConfig& GetColorsMut() { return m_colors; }
    KeybindingConfig& GetKeybindingsMut() { return m_keybindings; }
    TerminalConfig& GetTerminalMut() { return m_terminal; }

    // Check if config was loaded successfully
    bool IsLoaded() const { return m_loaded; }

    // Get any warnings from loading
    const std::vector<std::string>& GetWarnings() const { return m_warnings; }

private:
    bool ParseJson(const std::string& json);
    void SetDefaults();
    bool ResolveFontPath();

    FontConfig m_font;
    ColorConfig m_colors;
    KeybindingConfig m_keybindings;
    TerminalConfig m_terminal;

    bool m_loaded = false;
    std::vector<std::string> m_warnings;
};

} // namespace TerminalDX12::Core
