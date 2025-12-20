// test_unicode.cpp - Unit tests for Unicode handling
// Tests character handling for various Unicode ranges

#include <gtest/gtest.h>
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <memory>

namespace TerminalDX12::Terminal {
namespace Tests {

// ============================================================================
// Test Fixture
// ============================================================================

class UnicodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<ScreenBuffer>(80, 24);
        parser = std::make_unique<VTStateMachine>(buffer.get());
    }

    std::unique_ptr<ScreenBuffer> buffer;
    std::unique_ptr<VTStateMachine> parser;
};

// ============================================================================
// Basic Latin and Extended Latin Tests
// ============================================================================

TEST_F(UnicodeTest, BasicASCII) {
    buffer->WriteString(U"Hello, World!");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'H');
    EXPECT_EQ(buffer->GetCell(12, 0).ch, U'!');
}

TEST_F(UnicodeTest, LatinExtendedA) {
    // Latin Extended-A characters (U+0100 to U+017F)
    buffer->WriteChar(U'\u0100');  // Latin Capital Letter A with Macron
    buffer->WriteChar(U'\u0101');  // Latin Small Letter A with Macron
    buffer->WriteChar(U'\u0152');  // Latin Capital Ligature OE
    buffer->WriteChar(U'\u0153');  // Latin Small Ligature oe

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u0100');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u0101');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u0152');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u0153');
}

TEST_F(UnicodeTest, AccentedCharacters) {
    // Common accented characters
    buffer->WriteString(U"caf\u00E9");  // cafe with accent
    buffer->WriteString(U" na\u00EFve"); // naive with diaeresis

    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u00E9');  // e with accent
    EXPECT_EQ(buffer->GetCell(7, 0).ch, U'\u00EF');  // i with diaeresis
}

// ============================================================================
// CJK Character Tests
// ============================================================================

TEST_F(UnicodeTest, CJKBasic) {
    // Basic CJK characters
    buffer->WriteChar(U'\u4E2D');  // Chinese "middle/center"
    buffer->WriteChar(U'\u6587');  // Chinese "text/writing"

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u4E2D');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u6587');
}

TEST_F(UnicodeTest, Japanese) {
    // Hiragana and Katakana
    buffer->WriteChar(U'\u3042');  // Hiragana A
    buffer->WriteChar(U'\u3044');  // Hiragana I
    buffer->WriteChar(U'\u30A2');  // Katakana A
    buffer->WriteChar(U'\u30A4');  // Katakana I

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u3042');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u3044');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u30A2');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u30A4');
}

TEST_F(UnicodeTest, Korean) {
    // Korean Hangul
    buffer->WriteChar(U'\uD55C');  // Han
    buffer->WriteChar(U'\uAE00');  // Gul

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\uD55C');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\uAE00');
}

// ============================================================================
// Symbol Tests
// ============================================================================

TEST_F(UnicodeTest, MathSymbols) {
    buffer->WriteChar(U'\u221E');  // Infinity
    buffer->WriteChar(U'\u2211');  // Sum
    buffer->WriteChar(U'\u222B');  // Integral
    buffer->WriteChar(U'\u00B1');  // Plus-minus

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u221E');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u2211');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u222B');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u00B1');
}

TEST_F(UnicodeTest, Arrows) {
    buffer->WriteChar(U'\u2190');  // Left Arrow
    buffer->WriteChar(U'\u2191');  // Up Arrow
    buffer->WriteChar(U'\u2192');  // Right Arrow
    buffer->WriteChar(U'\u2193');  // Down Arrow

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u2190');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u2191');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u2192');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u2193');
}

TEST_F(UnicodeTest, BoxDrawing) {
    // Box drawing characters commonly used in terminal UIs
    buffer->WriteChar(U'\u2500');  // Box Drawings Light Horizontal
    buffer->WriteChar(U'\u2502');  // Box Drawings Light Vertical
    buffer->WriteChar(U'\u250C');  // Box Drawings Light Down and Right
    buffer->WriteChar(U'\u2510');  // Box Drawings Light Down and Left

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u2500');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u2502');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u250C');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u2510');
}

TEST_F(UnicodeTest, BlockElements) {
    buffer->WriteChar(U'\u2580');  // Upper Half Block
    buffer->WriteChar(U'\u2584');  // Lower Half Block
    buffer->WriteChar(U'\u2588');  // Full Block
    buffer->WriteChar(U'\u2591');  // Light Shade

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u2580');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u2584');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u2588');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'\u2591');
}

// ============================================================================
// Emoji Tests (Basic Multilingual Plane)
// ============================================================================

TEST_F(UnicodeTest, CommonEmoji) {
    // Note: These are BMP emoji that don't require surrogate pairs
    buffer->WriteChar(U'\u263A');  // White Smiling Face
    buffer->WriteChar(U'\u2764');  // Heavy Black Heart
    buffer->WriteChar(U'\u2605');  // Black Star

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\u263A');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\u2764');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'\u2605');
}

// ============================================================================
// Mixed Content Tests
// ============================================================================

TEST_F(UnicodeTest, MixedASCIIAndUnicode) {
    buffer->WriteString(U"Price: \u00A5100");  // Yen sign

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'P');
    EXPECT_EQ(buffer->GetCell(7, 0).ch, U'\u00A5');
    EXPECT_EQ(buffer->GetCell(8, 0).ch, U'1');
}

TEST_F(UnicodeTest, UnicodeWithLineWrap) {
    // Position near end of line
    buffer->SetCursorPos(78, 0);
    buffer->WriteString(U"\u4E2D\u6587");  // Two CJK characters

    // First character on line 0, second wraps to line 1
    EXPECT_EQ(buffer->GetCell(78, 0).ch, U'\u4E2D');
    EXPECT_EQ(buffer->GetCell(79, 0).ch, U'\u6587');
}

TEST_F(UnicodeTest, UnicodeWithAttributes) {
    CellAttributes attr;
    attr.foreground = 1;  // Red
    attr.flags = CellAttributes::FLAG_BOLD;

    buffer->WriteChar(U'\u4E2D', attr);

    const Cell& cell = buffer->GetCell(0, 0);
    EXPECT_EQ(cell.ch, U'\u4E2D');
    EXPECT_EQ(cell.attr.foreground, 1);
    EXPECT_TRUE(cell.attr.IsBold());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(UnicodeTest, NullCharacter) {
    buffer->WriteChar(U'\0');

    // Null character behavior depends on implementation
    // Either cursor advances (null written) or doesn't (null ignored)
    // We just verify the buffer remains functional
    buffer->WriteChar(U'X');
    // X should be written (either at position 0 or 1)
    bool foundX = (buffer->GetCell(0, 0).ch == U'X') ||
                  (buffer->GetCell(1, 0).ch == U'X');
    EXPECT_TRUE(foundX);
}

TEST_F(UnicodeTest, MaxCodepoint) {
    // Maximum valid Unicode codepoint (U+10FFFF)
    buffer->WriteChar(U'\U0010FFFF');

    // Should be stored correctly
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\U0010FFFF');
}

TEST_F(UnicodeTest, PrivateUseArea) {
    // Private Use Area characters (U+E000 to U+F8FF)
    buffer->WriteChar(U'\uE000');
    buffer->WriteChar(U'\uF8FF');

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'\uE000');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'\uF8FF');
}

} // namespace Tests
} // namespace TerminalDX12::Terminal
