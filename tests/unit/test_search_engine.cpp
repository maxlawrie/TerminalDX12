// test_search_engine.cpp - Unit tests for SearchEngine
// Tests regex search, scrollback search, and edge cases

#include <gtest/gtest.h>
#include "ui/SearchEngine.h"
#include "terminal/ScreenBuffer.h"
#include <memory>

namespace TerminalDX12::UI {
namespace Tests {

// ============================================================================
// Test Fixture
// ============================================================================

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<Terminal::ScreenBuffer>(80, 24, 100);
    }

    void WriteText(const std::wstring& text) {
        for (wchar_t ch : text) {
            if (ch == L'\n') {
                buffer->WriteChar(U'\r');
                buffer->WriteChar(U'\n');
            } else {
                buffer->WriteChar(static_cast<char32_t>(ch));
            }
        }
    }

    std::unique_ptr<Terminal::ScreenBuffer> buffer;
};

// ============================================================================
// Plain Text Search Tests
// ============================================================================

TEST_F(SearchEngineTest, PlainTextSearchFindsExactMatch) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"World", false, false, false);

    ASSERT_TRUE(result.isValid);
    ASSERT_EQ(result.matches.size(), 1);
    EXPECT_EQ(result.matches[0].x, 6);
    EXPECT_EQ(result.matches[0].y, 0);
    EXPECT_EQ(result.matches[0].length, 5);
}

TEST_F(SearchEngineTest, PlainTextSearchCaseInsensitive) {
    WriteText(L"Hello WORLD world World");

    auto result = SearchEngine::Search(*buffer, L"world", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 3);
}

TEST_F(SearchEngineTest, PlainTextSearchCaseSensitive) {
    WriteText(L"Hello WORLD world World");

    auto result = SearchEngine::Search(*buffer, L"World", false, true, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
    EXPECT_EQ(result.matches[0].x, 18);
}

TEST_F(SearchEngineTest, PlainTextSearchMultipleLines) {
    WriteText(L"Line one\nLine two\nLine three");

    auto result = SearchEngine::Search(*buffer, L"Line", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 3);
    EXPECT_EQ(result.matches[0].y, 0);
    EXPECT_EQ(result.matches[1].y, 1);
    EXPECT_EQ(result.matches[2].y, 2);
}

TEST_F(SearchEngineTest, PlainTextSearchNoMatches) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"Goodbye", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 0);
}

// ============================================================================
// Regex Search Tests
// ============================================================================

TEST_F(SearchEngineTest, RegexSearchBasicPattern) {
    WriteText(L"error123 error456 error789");

    auto result = SearchEngine::Search(*buffer, L"error\\d+", true, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 3);
}

TEST_F(SearchEngineTest, RegexSearchWordBoundary) {
    WriteText(L"cat category scatter");

    auto result = SearchEngine::Search(*buffer, L"\\bcat\\b", true, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
    EXPECT_EQ(result.matches[0].x, 0);
}

TEST_F(SearchEngineTest, RegexSearchCaseInsensitive) {
    WriteText(L"ERROR Error error");

    auto result = SearchEngine::Search(*buffer, L"error", true, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 3);
}

TEST_F(SearchEngineTest, RegexSearchCaseSensitive) {
    WriteText(L"ERROR Error error");

    auto result = SearchEngine::Search(*buffer, L"Error", true, true, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
    EXPECT_EQ(result.matches[0].x, 6);
}

TEST_F(SearchEngineTest, RegexSearchCharacterClass) {
    WriteText(L"abc 123 def 456 ghi");

    auto result = SearchEngine::Search(*buffer, L"[0-9]+", true, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 2);
}

TEST_F(SearchEngineTest, RegexSearchCapturesMatchLength) {
    WriteText(L"aaaa aaa aa a");

    auto result = SearchEngine::Search(*buffer, L"a+", true, false, false);

    ASSERT_TRUE(result.isValid);
    ASSERT_EQ(result.matches.size(), 4);
    EXPECT_EQ(result.matches[0].length, 4);
    EXPECT_EQ(result.matches[1].length, 3);
    EXPECT_EQ(result.matches[2].length, 2);
    EXPECT_EQ(result.matches[3].length, 1);
}

// ============================================================================
// Scrollback Search Tests
// ============================================================================

TEST_F(SearchEngineTest, ScrollbackSearchFindsOldContent) {
    // Fill buffer to push content into scrollback
    // Buffer is 24 rows, so write 30 lines to ensure scrollback
    for (int i = 0; i < 30; ++i) {
        WriteText(L"Line " + std::to_wstring(i) + L"\n");
    }

    auto result = SearchEngine::Search(*buffer, L"Line 0", false, false, true);

    ASSERT_TRUE(result.isValid);
    // Line 0 should be in scrollback (negative y)
    ASSERT_GE(result.matches.size(), 1);
    // The match for "Line 0" should have a negative y (in scrollback)
    bool foundInScrollback = false;
    for (const auto& match : result.matches) {
        if (match.y < 0) {
            foundInScrollback = true;
            break;
        }
    }
    EXPECT_TRUE(foundInScrollback) << "Expected to find 'Line 0' in scrollback";
}

TEST_F(SearchEngineTest, ScrollbackDisabledOnlySearchesVisible) {
    // Fill buffer to push content into scrollback
    for (int i = 0; i < 30; ++i) {
        WriteText(L"Line " + std::to_wstring(i) + L"\n");
    }

    auto result = SearchEngine::Search(*buffer, L"Line 0", false, false, false);

    ASSERT_TRUE(result.isValid);
    // With scrollback disabled, should not find Line 0 since it scrolled off
    // But "Line 10" (containing "0") might be found
    for (const auto& match : result.matches) {
        EXPECT_GE(match.y, 0) << "Found match in scrollback when disabled";
    }
}

TEST_F(SearchEngineTest, ScrollbackSearchWithRegex) {
    // Fill buffer with numbered lines
    for (int i = 0; i < 30; ++i) {
        WriteText(L"error" + std::to_wstring(i * 100) + L"\n");
    }

    auto result = SearchEngine::Search(*buffer, L"error\\d{3,}", true, false, true);

    ASSERT_TRUE(result.isValid);
    // Should find multi-digit errors including those in scrollback
    EXPECT_GE(result.matches.size(), 10);
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(SearchEngineTest, EmptyQueryReturnsNoMatches) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 0);
}

TEST_F(SearchEngineTest, InvalidRegexReturnsError) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"[invalid", true, false, false);

    EXPECT_FALSE(result.isValid);
    EXPECT_FALSE(result.errorMessage.empty());
    EXPECT_EQ(result.matches.size(), 0);
}

TEST_F(SearchEngineTest, InvalidRegexUnmatchedParen) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"(test", true, false, false);

    EXPECT_FALSE(result.isValid);
    EXPECT_FALSE(result.errorMessage.empty());
}

TEST_F(SearchEngineTest, InvalidRegexBadQuantifier) {
    WriteText(L"Hello World");

    auto result = SearchEngine::Search(*buffer, L"*abc", true, false, false);

    EXPECT_FALSE(result.isValid);
}

TEST_F(SearchEngineTest, EmptyBufferReturnsNoMatches) {
    // Don't write anything to buffer
    auto result = SearchEngine::Search(*buffer, L"test", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 0);
}

TEST_F(SearchEngineTest, SpecialRegexCharsInPlainText) {
    WriteText(L"a+b*c? (test) [arr]");

    // Plain text search should find literal +
    auto result = SearchEngine::Search(*buffer, L"a+b", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
}

TEST_F(SearchEngineTest, EscapedRegexSpecialChars) {
    WriteText(L"a+b*c? (test) [arr]");

    // Regex search for literal + requires escaping
    auto result = SearchEngine::Search(*buffer, L"a\\+b", true, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
}

TEST_F(SearchEngineTest, UnicodeSearch) {
    WriteText(L"Hello 世界 مرحبا");

    auto result = SearchEngine::Search(*buffer, L"世界", false, false, false);

    ASSERT_TRUE(result.isValid);
    EXPECT_EQ(result.matches.size(), 1);
}

TEST_F(SearchEngineTest, OverlappingMatches) {
    WriteText(L"aaaa");

    auto result = SearchEngine::Search(*buffer, L"aa", false, false, false);

    ASSERT_TRUE(result.isValid);
    // Should find: positions 0, 1, 2 (overlapping)
    EXPECT_EQ(result.matches.size(), 3);
}

// ============================================================================
// Match Position Accuracy
// ============================================================================

TEST_F(SearchEngineTest, MatchPositionIsCorrect) {
    WriteText(L"    test    ");

    auto result = SearchEngine::Search(*buffer, L"test", false, false, false);

    ASSERT_TRUE(result.isValid);
    ASSERT_EQ(result.matches.size(), 1);
    EXPECT_EQ(result.matches[0].x, 4);
    EXPECT_EQ(result.matches[0].y, 0);
    EXPECT_EQ(result.matches[0].length, 4);
}

TEST_F(SearchEngineTest, MultipleMatchesOnSameLine) {
    WriteText(L"foo bar foo baz foo");

    auto result = SearchEngine::Search(*buffer, L"foo", false, false, false);

    ASSERT_TRUE(result.isValid);
    ASSERT_EQ(result.matches.size(), 3);
    EXPECT_EQ(result.matches[0].x, 0);
    EXPECT_EQ(result.matches[1].x, 8);
    EXPECT_EQ(result.matches[2].x, 16);
}

} // namespace Tests
} // namespace TerminalDX12::UI
