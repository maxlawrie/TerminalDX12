#include <gtest/gtest.h>
#include "ui/FontEnumerator.h"

using namespace TerminalDX12::UI;

class FontEnumeratorTest : public ::testing::Test {
protected:
    FontEnumerator enumerator;
};

// T015: Basic GTest setup for font enumerator tests
TEST_F(FontEnumeratorTest, EnumerateReturnsNonEmpty) {
    auto fonts = enumerator.EnumerateMonospaceFonts();
    // Every Windows system should have at least one monospace font (Consolas, Courier New)
    EXPECT_GT(fonts.size(), 0);
}

TEST_F(FontEnumeratorTest, EnumerateIncludesConsolas) {
    auto fonts = enumerator.EnumerateMonospaceFonts();
    bool hasConsolas = false;
    for (const auto& font : fonts) {
        if (font.familyName == L"Consolas") {
            hasConsolas = true;
            EXPECT_TRUE(font.isMonospace);
            break;
        }
    }
    EXPECT_TRUE(hasConsolas) << "Consolas should be present on Windows";
}

TEST_F(FontEnumeratorTest, EnumerateOnlyReturnsMonospace) {
    auto fonts = enumerator.EnumerateMonospaceFonts();
    for (const auto& font : fonts) {
        // Convert wstring to string for test output
        std::string narrowName;
        narrowName.reserve(font.familyName.size());
        for (wchar_t wc : font.familyName) {
            narrowName.push_back(static_cast<char>(wc & 0x7F));
        }
        EXPECT_TRUE(font.isMonospace) << "Font " << narrowName << " should be monospace";
    }
}

TEST_F(FontEnumeratorTest, EnumerateIsSortedAlphabetically) {
    auto fonts = enumerator.EnumerateMonospaceFonts();
    if (fonts.size() > 1) {
        for (size_t i = 1; i < fonts.size(); ++i) {
            EXPECT_LE(fonts[i-1].familyName, fonts[i].familyName)
                << "Fonts should be sorted alphabetically";
        }
    }
}

TEST_F(FontEnumeratorTest, FontExistsForConsolas) {
    EXPECT_TRUE(enumerator.FontExists(L"Consolas"));
}

TEST_F(FontEnumeratorTest, FontExistsReturnsFalseForFakeFont) {
    EXPECT_FALSE(enumerator.FontExists(L"ThisFontDoesNotExist12345"));
}

// T028b: Test font fallback behavior
TEST_F(FontEnumeratorTest, GetFontOrFallbackReturnsRequestedIfExists) {
    auto result = enumerator.GetFontOrFallback(L"Consolas");
    EXPECT_EQ(result, L"Consolas");
}

TEST_F(FontEnumeratorTest, GetFontOrFallbackReturnsConsolasForMissingFont) {
    auto result = enumerator.GetFontOrFallback(L"NonExistentFont12345");
    EXPECT_EQ(result, L"Consolas");
}

TEST_F(FontEnumeratorTest, DefaultFontConstantIsConsolas) {
    EXPECT_STREQ(FontEnumerator::kDefaultFont, L"Consolas");
}
