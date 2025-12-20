// test_screen_buffer.cpp - Unit tests for ScreenBuffer
// Comprehensive test suite covering all ScreenBuffer functionality

#include <gtest/gtest.h>
#include "terminal/ScreenBuffer.h"
#include <memory>

namespace TerminalDX12::Terminal {
namespace Tests {

// ============================================================================
// Test Fixture
// ============================================================================

class ScreenBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<ScreenBuffer>(80, 24);
    }

    std::unique_ptr<ScreenBuffer> buffer;
};

// ============================================================================
// Construction Tests
// ============================================================================

TEST_F(ScreenBufferTest, DefaultConstruction) {
    auto buf = std::make_unique<ScreenBuffer>();
    EXPECT_EQ(buf->GetCols(), 80);
    EXPECT_EQ(buf->GetRows(), 24);
}

TEST_F(ScreenBufferTest, CustomDimensions) {
    auto buf = std::make_unique<ScreenBuffer>(120, 40);
    EXPECT_EQ(buf->GetCols(), 120);
    EXPECT_EQ(buf->GetRows(), 40);
}

TEST_F(ScreenBufferTest, InitialCursorPosition) {
    int x, y;
    buffer->GetCursorPos(x, y);
    EXPECT_EQ(x, 0);
    EXPECT_EQ(y, 0);
}

TEST_F(ScreenBufferTest, InitialCursorVisible) {
    EXPECT_TRUE(buffer->IsCursorVisible());
}

TEST_F(ScreenBufferTest, InitialDirtyFlag) {
    // Buffer is dirty after construction
    EXPECT_TRUE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, InitialAttributes) {
    const auto& attr = buffer->GetCurrentAttributes();
    EXPECT_EQ(attr.foreground, 7);  // White
    EXPECT_EQ(attr.background, 0);  // Black
    EXPECT_EQ(attr.flags, 0);       // No flags
}

// ============================================================================
// Cursor Operations Tests
// ============================================================================

TEST_F(ScreenBufferTest, SetCursorPosBasic) {
    buffer->SetCursorPos(10, 5);
    EXPECT_EQ(buffer->GetCursorX(), 10);
    EXPECT_EQ(buffer->GetCursorY(), 5);
}

TEST_F(ScreenBufferTest, SetCursorPosClampNegativeX) {
    buffer->SetCursorPos(-5, 5);
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 5);
}

TEST_F(ScreenBufferTest, SetCursorPosClampNegativeY) {
    buffer->SetCursorPos(5, -5);
    EXPECT_EQ(buffer->GetCursorX(), 5);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(ScreenBufferTest, SetCursorPosClampBeyondCols) {
    buffer->SetCursorPos(100, 5);  // 80 cols
    EXPECT_EQ(buffer->GetCursorX(), 79);
    EXPECT_EQ(buffer->GetCursorY(), 5);
}

TEST_F(ScreenBufferTest, SetCursorPosClampBeyondRows) {
    buffer->SetCursorPos(5, 50);  // 24 rows
    EXPECT_EQ(buffer->GetCursorX(), 5);
    EXPECT_EQ(buffer->GetCursorY(), 23);
}

TEST_F(ScreenBufferTest, SetCursorPosMarksBufferDirty) {
    buffer->ClearDirty();
    EXPECT_FALSE(buffer->IsDirty());
    buffer->SetCursorPos(5, 5);
    EXPECT_TRUE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, CursorVisibilityToggle) {
    buffer->SetCursorVisible(false);
    EXPECT_FALSE(buffer->IsCursorVisible());
    buffer->SetCursorVisible(true);
    EXPECT_TRUE(buffer->IsCursorVisible());
}

TEST_F(ScreenBufferTest, GetCursorPosOutParams) {
    buffer->SetCursorPos(42, 17);
    int x, y;
    buffer->GetCursorPos(x, y);
    EXPECT_EQ(x, 42);
    EXPECT_EQ(y, 17);
}

// ============================================================================
// WriteChar Tests
// ============================================================================

TEST_F(ScreenBufferTest, WriteCharBasic) {
    buffer->WriteChar(U'A');
    const Cell& cell = buffer->GetCell(0, 0);
    EXPECT_EQ(cell.ch, U'A');
}

TEST_F(ScreenBufferTest, WriteCharAdvancesCursor) {
    buffer->WriteChar(U'A');
    EXPECT_EQ(buffer->GetCursorX(), 1);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(ScreenBufferTest, WriteCharWithAttributes) {
    CellAttributes attr;
    attr.foreground = 1;  // Red
    attr.background = 4;  // Blue
    attr.flags = CellAttributes::FLAG_BOLD;

    buffer->WriteChar(U'X', attr);
    const Cell& cell = buffer->GetCell(0, 0);
    EXPECT_EQ(cell.ch, U'X');
    EXPECT_EQ(cell.attr.foreground, 1);
    EXPECT_EQ(cell.attr.background, 4);
    EXPECT_TRUE(cell.attr.IsBold());
}

TEST_F(ScreenBufferTest, WriteCharUsesCurrentAttributes) {
    CellAttributes attr;
    attr.foreground = 2;  // Green
    buffer->SetCurrentAttributes(attr);

    buffer->WriteChar(U'G');
    const Cell& cell = buffer->GetCell(0, 0);
    EXPECT_EQ(cell.attr.foreground, 2);
}

TEST_F(ScreenBufferTest, WriteCharLineWrap) {
    // Write at end of line
    buffer->SetCursorPos(79, 0);  // Last column
    buffer->WriteChar(U'A');

    // Should wrap to next line
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 1);
}

TEST_F(ScreenBufferTest, WriteCharScrollAtBottom) {
    // Position at last row, last column
    buffer->SetCursorPos(79, 23);
    buffer->WriteChar(U'A');

    // Should scroll and stay at bottom
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 23);
}

TEST_F(ScreenBufferTest, WriteCharNewline) {
    buffer->SetCursorPos(10, 5);
    buffer->WriteChar(U'\n');

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 6);
}

TEST_F(ScreenBufferTest, WriteCharCarriageReturn) {
    buffer->SetCursorPos(10, 5);
    buffer->WriteChar(U'\r');

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 5);
}

TEST_F(ScreenBufferTest, WriteCharTab) {
    buffer->SetCursorPos(3, 0);
    buffer->WriteChar(U'\t');

    // Tab stops at every 8 columns: 0, 8, 16, 24...
    // From position 3, next tab is 8
    EXPECT_EQ(buffer->GetCursorX(), 8);
}

TEST_F(ScreenBufferTest, WriteCharTabMultiple) {
    buffer->SetCursorPos(0, 0);
    buffer->WriteChar(U'\t');  // -> 8
    buffer->WriteChar(U'\t');  // -> 16
    EXPECT_EQ(buffer->GetCursorX(), 16);
}

TEST_F(ScreenBufferTest, WriteCharBackspace) {
    buffer->SetCursorPos(5, 0);
    buffer->WriteChar(U'\b');
    EXPECT_EQ(buffer->GetCursorX(), 4);
}

TEST_F(ScreenBufferTest, WriteCharBackspaceAtStart) {
    buffer->SetCursorPos(0, 0);
    buffer->WriteChar(U'\b');
    EXPECT_EQ(buffer->GetCursorX(), 0);  // Doesn't go negative
}

TEST_F(ScreenBufferTest, WriteCharMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->WriteChar(U'X');
    EXPECT_TRUE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, WriteCharUnicode) {
    buffer->WriteChar(U'\u00E9');  // é
    const Cell& cell = buffer->GetCell(0, 0);
    EXPECT_EQ(cell.ch, U'\u00E9');
}

TEST_F(ScreenBufferTest, WriteString) {
    buffer->WriteString(U"Hello");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'H');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'e');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'l');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'l');
    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'o');
    EXPECT_EQ(buffer->GetCursorX(), 5);
}

// ============================================================================
// Scrolling Tests
// ============================================================================

TEST_F(ScreenBufferTest, ScrollUpSingleLine) {
    // Write on first line
    buffer->WriteString(U"Line1");
    buffer->SetCursorPos(0, 1);
    buffer->WriteString(U"Line2");

    buffer->ScrollUp(1);

    // Line2 should now be on line 0
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'L');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'i');
    // Last line should be clear
    EXPECT_EQ(buffer->GetCell(0, 23).ch, U' ');
}

TEST_F(ScreenBufferTest, ScrollUpMultipleLines) {
    buffer->WriteString(U"Line0");
    buffer->SetCursorPos(0, 1);
    buffer->WriteString(U"Line1");
    buffer->SetCursorPos(0, 2);
    buffer->WriteString(U"Line2");

    buffer->ScrollUp(2);

    // Line2 should now be on line 0
    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'2');
}

TEST_F(ScreenBufferTest, ScrollUpClampsToBounds) {
    buffer->WriteString(U"Test");
    buffer->ScrollUp(100);  // More than rows

    // Buffer should be cleared
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
}

TEST_F(ScreenBufferTest, ScrollUpZeroOrNegative) {
    buffer->WriteString(U"Test");
    buffer->ScrollUp(0);
    buffer->ScrollUp(-1);

    // Content should remain unchanged
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'T');
}

TEST_F(ScreenBufferTest, ScrollUpPopulatesScrollback) {
    // Small buffer for easier testing
    auto smallBuf = std::make_unique<ScreenBuffer>(10, 5, 100);
    smallBuf->WriteString(U"Line0");

    smallBuf->ScrollUp(1);

    EXPECT_EQ(smallBuf->GetScrollOffset(), 0);
    smallBuf->SetScrollOffset(1);

    // Should be able to see the scrolled line
    const Cell& cell = smallBuf->GetCellWithScrollback(0, 0);
    EXPECT_EQ(cell.ch, U'L');
}

TEST_F(ScreenBufferTest, ScrollDownSingleLine) {
    buffer->WriteString(U"Line0");
    buffer->SetCursorPos(0, 1);
    buffer->WriteString(U"Line1");

    buffer->ScrollDown(1);

    // Top line should be clear
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    // Line0 should now be on line 1
    EXPECT_EQ(buffer->GetCell(0, 1).ch, U'L');
}

TEST_F(ScreenBufferTest, ScrollDownMultipleLines) {
    buffer->WriteString(U"Test");
    buffer->ScrollDown(2);

    // First two lines should be empty
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(0, 1).ch, U' ');
    // Content on line 2
    EXPECT_EQ(buffer->GetCell(0, 2).ch, U'T');
}

TEST_F(ScreenBufferTest, ScrollToBottom) {
    buffer->SetScrollOffset(5);
    buffer->ScrollToBottom();
    EXPECT_EQ(buffer->GetScrollOffset(), 0);
}

TEST_F(ScreenBufferTest, SetScrollOffsetClamps) {
    // No scrollback yet
    buffer->SetScrollOffset(100);
    EXPECT_EQ(buffer->GetScrollOffset(), 0);
}

TEST_F(ScreenBufferTest, ScrollUpMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->ScrollUp(1);
    EXPECT_TRUE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, ScrollDownMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->ScrollDown(1);
    EXPECT_TRUE(buffer->IsDirty());
}

// ============================================================================
// Clearing Tests
// ============================================================================

TEST_F(ScreenBufferTest, ClearScreen) {
    buffer->WriteString(U"Test content");
    buffer->SetCursorPos(10, 10);

    buffer->Clear();

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(ScreenBufferTest, ClearLineEntire) {
    buffer->WriteString(U"Test content on line 0");
    buffer->ClearLine(0);

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(5, 0).ch, U' ');
}

TEST_F(ScreenBufferTest, ClearLineRange) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->ClearLine(0, 2, 7);  // Clear C through H

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'B');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U' ');  // Cleared
    EXPECT_EQ(buffer->GetCell(7, 0).ch, U' ');  // Cleared
    EXPECT_EQ(buffer->GetCell(8, 0).ch, U'I');
    EXPECT_EQ(buffer->GetCell(9, 0).ch, U'J');
}

TEST_F(ScreenBufferTest, ClearLineInvalidY) {
    buffer->WriteString(U"Test");
    buffer->ClearLine(-1);   // Should be no-op
    buffer->ClearLine(100);  // Should be no-op

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'T');
}

TEST_F(ScreenBufferTest, ClearLineRangeClamping) {
    buffer->WriteString(U"Test");
    buffer->ClearLine(0, -5, 200);  // Should clamp to valid range

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
}

TEST_F(ScreenBufferTest, ClearRect) {
    // Fill several lines
    for (int y = 0; y < 5; ++y) {
        buffer->SetCursorPos(0, y);
        buffer->WriteString(U"XXXXXXXXXX");
    }

    buffer->ClearRect(2, 1, 3, 2);  // 3x2 rectangle starting at (2,1)

    // Check rectangle is cleared
    EXPECT_EQ(buffer->GetCell(2, 1).ch, U' ');
    EXPECT_EQ(buffer->GetCell(4, 2).ch, U' ');

    // Check outside rectangle is unchanged
    EXPECT_EQ(buffer->GetCell(1, 1).ch, U'X');
    EXPECT_EQ(buffer->GetCell(5, 1).ch, U'X');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'X');
    EXPECT_EQ(buffer->GetCell(2, 3).ch, U'X');
}

TEST_F(ScreenBufferTest, ClearMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->Clear();
    EXPECT_TRUE(buffer->IsDirty());
}

// ============================================================================
// Alternative Buffer Tests
// ============================================================================

TEST_F(ScreenBufferTest, InitialBufferState) {
    EXPECT_FALSE(buffer->IsUsingAlternativeBuffer());
}

TEST_F(ScreenBufferTest, SwitchToAlternativeBuffer) {
    buffer->WriteString(U"Primary");

    buffer->UseAlternativeBuffer(true);
    EXPECT_TRUE(buffer->IsUsingAlternativeBuffer());

    // Primary content should be hidden
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
}

TEST_F(ScreenBufferTest, SwitchBackFromAlternativeBuffer) {
    buffer->WriteString(U"Primary");
    buffer->UseAlternativeBuffer(true);
    buffer->WriteString(U"Alt");
    buffer->UseAlternativeBuffer(false);

    EXPECT_FALSE(buffer->IsUsingAlternativeBuffer());
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'P');
}

TEST_F(ScreenBufferTest, AlternativeBufferIsolation) {
    buffer->WriteString(U"Primary");

    // Switch to alt buffer and write content
    buffer->UseAlternativeBuffer(true);
    buffer->SetCursorPos(0, 0);
    buffer->WriteString(U"Alternate");

    // Verify we're in alt buffer
    EXPECT_TRUE(buffer->IsUsingAlternativeBuffer());

    // Switch back to primary
    buffer->UseAlternativeBuffer(false);

    // Primary content should be restored (starts with 'P')
    EXPECT_FALSE(buffer->IsUsingAlternativeBuffer());
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'P');
}

TEST_F(ScreenBufferTest, DoubleSwitchToAlt) {
    buffer->UseAlternativeBuffer(true);
    buffer->UseAlternativeBuffer(true);  // Should be no-op
    EXPECT_TRUE(buffer->IsUsingAlternativeBuffer());
}

TEST_F(ScreenBufferTest, AlternativeBufferMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->UseAlternativeBuffer(true);
    EXPECT_TRUE(buffer->IsDirty());
}

// ============================================================================
// Resize Tests
// ============================================================================

TEST_F(ScreenBufferTest, ResizeLarger) {
    buffer->WriteString(U"Test");
    buffer->Resize(100, 30);

    EXPECT_EQ(buffer->GetCols(), 100);
    EXPECT_EQ(buffer->GetRows(), 30);
    // Content preserved
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'T');
}

TEST_F(ScreenBufferTest, ResizeSmaller) {
    buffer->WriteString(U"Test content here");
    buffer->Resize(40, 12);

    EXPECT_EQ(buffer->GetCols(), 40);
    EXPECT_EQ(buffer->GetRows(), 12);
    // Content preserved within new bounds
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'T');
}

TEST_F(ScreenBufferTest, ResizeSameSize) {
    buffer->WriteString(U"Test");
    buffer->ClearDirty();

    buffer->Resize(80, 24);  // Same size

    // Should be no-op
    EXPECT_FALSE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, ResizeCursorClamping) {
    buffer->SetCursorPos(70, 20);
    buffer->Resize(50, 15);

    EXPECT_EQ(buffer->GetCursorX(), 49);  // Clamped to cols-1
    EXPECT_EQ(buffer->GetCursorY(), 14);  // Clamped to rows-1
}

TEST_F(ScreenBufferTest, ResizeMarksBufferDirty) {
    buffer->ClearDirty();
    buffer->Resize(100, 30);
    EXPECT_TRUE(buffer->IsDirty());
}

// ============================================================================
// Cell Access Tests
// ============================================================================

TEST_F(ScreenBufferTest, GetCellOutOfBounds) {
    const Cell& cell1 = buffer->GetCell(-1, 0);
    const Cell& cell2 = buffer->GetCell(0, -1);
    const Cell& cell3 = buffer->GetCell(100, 0);
    const Cell& cell4 = buffer->GetCell(0, 100);

    // All should return empty cell
    EXPECT_EQ(cell1.ch, U' ');
    EXPECT_EQ(cell2.ch, U' ');
    EXPECT_EQ(cell3.ch, U' ');
    EXPECT_EQ(cell4.ch, U' ');
}

TEST_F(ScreenBufferTest, GetCellMutableMarksBufferDirty) {
    buffer->ClearDirty();
    Cell& cell = buffer->GetCellMutable(0, 0);
    cell.ch = U'X';
    EXPECT_TRUE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, GetBufferReturnsCorrectBuffer) {
    buffer->WriteString(U"Test");
    const auto& buf = buffer->GetBuffer();

    EXPECT_EQ(buf.size(), 80 * 24);
    EXPECT_EQ(buf[0].ch, U'T');
}

// ============================================================================
// Dirty Flag Tests
// ============================================================================

TEST_F(ScreenBufferTest, ClearDirtyFlag) {
    EXPECT_TRUE(buffer->IsDirty());
    buffer->ClearDirty();
    EXPECT_FALSE(buffer->IsDirty());
}

TEST_F(ScreenBufferTest, MarkDirty) {
    buffer->ClearDirty();
    buffer->MarkDirty();
    EXPECT_TRUE(buffer->IsDirty());
}

// ============================================================================
// Attribute Tests
// ============================================================================

TEST_F(ScreenBufferTest, CellAttributeFlags) {
    CellAttributes attr;

    EXPECT_FALSE(attr.IsBold());
    EXPECT_FALSE(attr.IsItalic());
    EXPECT_FALSE(attr.IsUnderline());
    EXPECT_FALSE(attr.IsInverse());

    attr.flags = CellAttributes::FLAG_BOLD | CellAttributes::FLAG_UNDERLINE;

    EXPECT_TRUE(attr.IsBold());
    EXPECT_FALSE(attr.IsItalic());
    EXPECT_TRUE(attr.IsUnderline());
    EXPECT_FALSE(attr.IsInverse());
}

TEST_F(ScreenBufferTest, SetAndGetCurrentAttributes) {
    CellAttributes attr;
    attr.foreground = 5;
    attr.background = 3;
    attr.flags = CellAttributes::FLAG_ITALIC;

    buffer->SetCurrentAttributes(attr);
    const auto& retrieved = buffer->GetCurrentAttributes();

    EXPECT_EQ(retrieved.foreground, 5);
    EXPECT_EQ(retrieved.background, 3);
    EXPECT_TRUE(retrieved.IsItalic());
}

// ============================================================================
// Newline Scroll Tests
// ============================================================================

TEST_F(ScreenBufferTest, NewlineAtBottomScrolls) {
    buffer->SetCursorPos(0, 23);  // Last row
    buffer->WriteChar(U'\n');

    // Should still be at last row after scroll
    EXPECT_EQ(buffer->GetCursorY(), 23);
    EXPECT_EQ(buffer->GetCursorX(), 0);
}

TEST_F(ScreenBufferTest, TabAtEndOfLine) {
    buffer->SetCursorPos(75, 0);
    buffer->WriteChar(U'\t');

    // Should clamp to last column
    EXPECT_EQ(buffer->GetCursorX(), 79);
}

} // namespace Tests
} // namespace TerminalDX12::Terminal
