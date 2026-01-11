#include <gtest/gtest.h>
#include "core/Config.h"
#include <filesystem>
#include <fstream>

using namespace TerminalDX12::Core;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temp directory for test configs
        testDir = std::filesystem::temp_directory_path() / "terminaldx12_test";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override {
        // Clean up temp files
        std::filesystem::remove_all(testDir);
    }

    std::filesystem::path testDir;

    void WriteTestConfig(const std::string& filename, const std::string& content) {
        std::ofstream file(testDir / filename);
        file << content;
    }
};

// Color parsing tests
TEST_F(ConfigTest, ColorFromHex_ValidRGB) {
    auto color = Color::FromHex("#FF8000");
    ASSERT_TRUE(color.has_value());
    EXPECT_EQ(color->r, 255);
    EXPECT_EQ(color->g, 128);
    EXPECT_EQ(color->b, 0);
    EXPECT_EQ(color->a, 255);
}

TEST_F(ConfigTest, ColorFromHex_ValidRGBA) {
    auto color = Color::FromHex("#FF800080");
    ASSERT_TRUE(color.has_value());
    EXPECT_EQ(color->r, 255);
    EXPECT_EQ(color->g, 128);
    EXPECT_EQ(color->b, 0);
    EXPECT_EQ(color->a, 128);
}

TEST_F(ConfigTest, ColorFromHex_Invalid) {
    EXPECT_FALSE(Color::FromHex("").has_value());
    EXPECT_FALSE(Color::FromHex("FF8000").has_value());  // Missing #
    EXPECT_FALSE(Color::FromHex("#F80").has_value());    // Too short
    EXPECT_FALSE(Color::FromHex("#GGGGGG").has_value()); // Invalid hex
}

TEST_F(ConfigTest, ColorToHex_RGB) {
    Color c{255, 128, 0};
    EXPECT_EQ(c.ToHex(), "#FF8000");
}

TEST_F(ConfigTest, ColorToHex_RGBA) {
    Color c{255, 128, 0, 128};
    EXPECT_EQ(c.ToHex(), "#FF800080");
}

// KeyBinding parsing tests
TEST_F(ConfigTest, KeyBindingParse_Simple) {
    auto kb = KeyBinding::Parse("c");
    ASSERT_TRUE(kb.has_value());
    EXPECT_EQ(kb->key, "c");
    EXPECT_FALSE(kb->ctrl);
    EXPECT_FALSE(kb->shift);
    EXPECT_FALSE(kb->alt);
}

TEST_F(ConfigTest, KeyBindingParse_WithCtrl) {
    auto kb = KeyBinding::Parse("ctrl+c");
    ASSERT_TRUE(kb.has_value());
    EXPECT_EQ(kb->key, "c");
    EXPECT_TRUE(kb->ctrl);
    EXPECT_FALSE(kb->shift);
    EXPECT_FALSE(kb->alt);
}

TEST_F(ConfigTest, KeyBindingParse_WithAllModifiers) {
    auto kb = KeyBinding::Parse("ctrl+shift+alt+delete");
    ASSERT_TRUE(kb.has_value());
    EXPECT_EQ(kb->key, "delete");
    EXPECT_TRUE(kb->ctrl);
    EXPECT_TRUE(kb->shift);
    EXPECT_TRUE(kb->alt);
}

TEST_F(ConfigTest, KeyBindingParse_CaseInsensitive) {
    auto kb = KeyBinding::Parse("CTRL+SHIFT+C");
    ASSERT_TRUE(kb.has_value());
    EXPECT_EQ(kb->key, "c");
    EXPECT_TRUE(kb->ctrl);
    EXPECT_TRUE(kb->shift);
}

TEST_F(ConfigTest, KeyBindingToString) {
    KeyBinding kb{"c", true, true, false};
    EXPECT_EQ(kb.ToString(), "Ctrl+Shift+c");
}

// Config loading tests
TEST_F(ConfigTest, LoadFromNonExistentFile_UsesDefaults) {
    Config config;
    // Should use defaults when file doesn't exist
    std::wstring nonExistentPath = (testDir / "nonexistent.json").wstring();
    EXPECT_TRUE(config.Load(nonExistentPath));
    EXPECT_EQ(config.GetFont().family, "Consolas");
    EXPECT_EQ(config.GetFont().size, 16);
    EXPECT_EQ(config.GetTerminal().scrollbackLines, 10000);
}

TEST_F(ConfigTest, ParseJson_FontConfig) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "font": {
            "family": "JetBrains Mono",
            "size": 18,
            "weight": 700,
            "ligatures": true
        }
    })");

    EXPECT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetFont().family, "JetBrains Mono");
    EXPECT_EQ(config.GetFont().size, 18);
    EXPECT_EQ(config.GetFont().weight, 700);
    EXPECT_TRUE(config.GetFont().ligatures);
}

TEST_F(ConfigTest, ParseJson_ColorConfig) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "colors": {
            "foreground": "#FFFFFF",
            "background": "#000000",
            "cursor": "#00FF00",
            "palette": [
                "#282A36", "#FF5555", "#50FA7B", "#F1FA8C",
                "#BD93F9", "#FF79C6", "#8BE9FD", "#F8F8F2",
                "#6272A4", "#FF6E6E", "#69FF94", "#FFFFA5",
                "#D6ACFF", "#FF92DF", "#A4FFFF", "#FFFFFF"
            ]
        }
    })");

    EXPECT_TRUE(config.Load(configPath));

    const auto& colors = config.GetColors();
    EXPECT_EQ(colors.foreground.r, 255);
    EXPECT_EQ(colors.foreground.g, 255);
    EXPECT_EQ(colors.foreground.b, 255);

    EXPECT_EQ(colors.background.r, 0);
    EXPECT_EQ(colors.background.g, 0);
    EXPECT_EQ(colors.background.b, 0);

    // Check Dracula theme palette colors
    EXPECT_EQ(colors.palette[0].r, 0x28);
    EXPECT_EQ(colors.palette[1].r, 0xFF);  // Red
    EXPECT_EQ(colors.palette[2].g, 0xFA);  // Green
}

TEST_F(ConfigTest, ParseJson_TerminalConfig) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "terminal": {
            "shell": "cmd.exe",
            "scrollbackLines": 5000,
            "cursorStyle": "underline",
            "cursorBlink": false
        }
    })");

    EXPECT_TRUE(config.Load(configPath));

    const auto& terminal = config.GetTerminal();
    EXPECT_EQ(terminal.shell, L"cmd.exe");
    EXPECT_EQ(terminal.scrollbackLines, 5000);
    EXPECT_EQ(terminal.cursorStyle, CursorStyle::Underline);
    EXPECT_FALSE(terminal.cursorBlink);
}

TEST_F(ConfigTest, ParseJson_Keybindings) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "keybindings": {
            "copy": "ctrl+shift+c",
            "paste": "ctrl+shift+v",
            "custom_action": "alt+f4"
        }
    })");

    EXPECT_TRUE(config.Load(configPath));

    auto copy = config.GetKeybindings().GetBinding("copy");
    ASSERT_TRUE(copy.has_value());
    EXPECT_TRUE(copy->ctrl);
    EXPECT_TRUE(copy->shift);
    EXPECT_EQ(copy->key, "c");

    auto custom = config.GetKeybindings().GetBinding("custom_action");
    ASSERT_TRUE(custom.has_value());
    EXPECT_TRUE(custom->alt);
    EXPECT_EQ(custom->key, "f4");
}

TEST_F(ConfigTest, ParseJson_InvalidJson) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", "{ invalid json }");

    EXPECT_FALSE(config.Load(configPath));
    EXPECT_FALSE(config.GetWarnings().empty());
}

TEST_F(ConfigTest, ParseJson_InvalidFontSize) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "font": {
            "size": 200
        }
    })");

    EXPECT_TRUE(config.Load(configPath));
    // Should warn and use default
    EXPECT_FALSE(config.GetWarnings().empty());
    EXPECT_EQ(config.GetFont().size, 16);  // Default
}

TEST_F(ConfigTest, ParseJson_InvalidColor) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "colors": {
            "foreground": "not-a-color"
        }
    })");

    EXPECT_TRUE(config.Load(configPath));
    // Should warn and use default
    EXPECT_FALSE(config.GetWarnings().empty());
}

TEST_F(ConfigTest, ParseJson_InvalidCursorStyle) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    WriteTestConfig("config.json", R"({
        "terminal": {
            "cursorStyle": "invalid"
        }
    })");

    EXPECT_TRUE(config.Load(configPath));
    // Should warn but keep default
    EXPECT_FALSE(config.GetWarnings().empty());
    EXPECT_EQ(config.GetTerminal().cursorStyle, CursorStyle::Block);
}

TEST_F(ConfigTest, Save_AndReload) {
    Config config1;
    config1.GetFontMut().family = "Fira Code";
    config1.GetFontMut().size = 14;
    config1.GetColorsMut().cursor = Color{255, 0, 255};
    config1.GetTerminalMut().scrollbackLines = 20000;

    std::wstring configPath = (testDir / "config.json").wstring();
    EXPECT_TRUE(config1.Save(configPath));

    // Reload and verify
    Config config2;
    EXPECT_TRUE(config2.Load(configPath));
    EXPECT_EQ(config2.GetFont().family, "Fira Code");
    EXPECT_EQ(config2.GetFont().size, 14);
    EXPECT_EQ(config2.GetColors().cursor.r, 255);
    EXPECT_EQ(config2.GetColors().cursor.g, 0);
    EXPECT_EQ(config2.GetColors().cursor.b, 255);
    EXPECT_EQ(config2.GetTerminal().scrollbackLines, 20000);
}

TEST_F(ConfigTest, CursorStyles) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    // Test block
    WriteTestConfig("config.json", R"({"terminal": {"cursorStyle": "block"}})");
    ASSERT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetTerminal().cursorStyle, CursorStyle::Block);

    // Test underline
    WriteTestConfig("config.json", R"({"terminal": {"cursorStyle": "underline"}})");
    ASSERT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetTerminal().cursorStyle, CursorStyle::Underline);

    // Test bar
    WriteTestConfig("config.json", R"({"terminal": {"cursorStyle": "bar"}})");
    ASSERT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetTerminal().cursorStyle, CursorStyle::Bar);
}

TEST_F(ConfigTest, DefaultKeybindings) {
    Config config;
    const auto& kb = config.GetKeybindings();

    auto copy = kb.GetBinding("copy");
    ASSERT_TRUE(copy.has_value());
    EXPECT_TRUE(copy->ctrl);
    EXPECT_EQ(copy->key, "c");

    auto paste = kb.GetBinding("paste");
    ASSERT_TRUE(paste.has_value());
    EXPECT_TRUE(paste->ctrl);
    EXPECT_EQ(paste->key, "v");
}

TEST_F(ConfigTest, ScrollbackLimitsClamped) {
    Config config;
    std::wstring configPath = (testDir / "config.json").wstring();

    // Test negative value
    WriteTestConfig("config.json", R"({"terminal": {"scrollbackLines": -100}})");
    ASSERT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetTerminal().scrollbackLines, 0);

    // Test excessive value
    WriteTestConfig("config.json", R"({"terminal": {"scrollbackLines": 999999}})");
    ASSERT_TRUE(config.Load(configPath));
    EXPECT_EQ(config.GetTerminal().scrollbackLines, 100000);
    EXPECT_FALSE(config.GetWarnings().empty());
}
