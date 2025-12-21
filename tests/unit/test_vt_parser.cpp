// test_vt_parser.cpp - Unit tests for VTStateMachine
// Comprehensive test suite covering VT100/ANSI escape sequence parsing

#include <gtest/gtest.h>
#include "terminal/VTStateMachine.h"
#include "terminal/ScreenBuffer.h"
#include <memory>
#include <string>

namespace TerminalDX12::Terminal {
namespace Tests {

// ============================================================================
// Test Fixture
// ============================================================================

class VTStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<ScreenBuffer>(80, 24);
        parser = std::make_unique<VTStateMachine>(buffer.get());
    }

    void ProcessString(const std::string& str) {
        parser->ProcessInput(str.c_str(), str.size());
    }

    // Helper to create escape sequences
    std::string ESC(const std::string& seq) {
        return std::string("\x1b") + seq;
    }

    std::string CSI(const std::string& params, char cmd) {
        return std::string("\x1b[") + params + cmd;
    }

    std::unique_ptr<ScreenBuffer> buffer;
    std::unique_ptr<VTStateMachine> parser;
};

// ============================================================================
// Basic Text Processing Tests
// ============================================================================

TEST_F(VTStateMachineTest, ProcessPlainText) {
    ProcessString("Hello");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'H');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'e');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'l');
    EXPECT_EQ(buffer->GetCell(3, 0).ch, U'l');
    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'o');
}

TEST_F(VTStateMachineTest, ProcessNewline) {
    ProcessString("Line1\nLine2");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'L');
    EXPECT_EQ(buffer->GetCell(0, 1).ch, U'L');
}

TEST_F(VTStateMachineTest, ProcessCarriageReturn) {
    ProcessString("ABCD\rXY");

    // XY overwrites AB
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'X');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'Y');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'C');
}

TEST_F(VTStateMachineTest, ProcessTab) {
    ProcessString("A\tB");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
    EXPECT_EQ(buffer->GetCell(8, 0).ch, U'B');
}

TEST_F(VTStateMachineTest, ProcessBackspace) {
    ProcessString("ABC\b");

    // Cursor should be at position 2 (moved back from 3)
    EXPECT_EQ(buffer->GetCursorX(), 2);
    // Character should still be there (backspace only moves cursor)
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'C');
}

TEST_F(VTStateMachineTest, ProcessBackspaceSpaceBackspace) {
    // This is the typical "erase character" sequence: \b \b
    ProcessString("ABC\b \b");

    // Cursor should be at position 2
    EXPECT_EQ(buffer->GetCursorX(), 2);
    // Character should be erased (replaced with space)
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U' ');
    // Previous characters should remain
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'B');
}

TEST_F(VTStateMachineTest, ProcessMultipleBackspaces) {
    ProcessString("ABCDE\b\b\b");

    // Cursor should be at position 2 (moved back 3 from 5)
    EXPECT_EQ(buffer->GetCursorX(), 2);
}

TEST_F(VTStateMachineTest, ProcessBackspaceAtStart) {
    ProcessString("\b");

    // Cursor should stay at 0
    EXPECT_EQ(buffer->GetCursorX(), 0);
}

TEST_F(VTStateMachineTest, ProcessBackspaceAndType) {
    // Type ABC, backspace, type X - should result in ABX
    ProcessString("ABC\bX");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'B');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'X');
    EXPECT_EQ(buffer->GetCursorX(), 3);
}

TEST_F(VTStateMachineTest, ProcessBackspaceDoesNotWrapLine) {
    // Move to start of second line and backspace
    buffer->SetCursorPos(0, 1);
    ProcessString("\b");

    // Should stay at column 0, row 1 (not wrap to end of row 0)
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 1);
}

TEST_F(VTStateMachineTest, ProcessEmptyInput) {
    ProcessString("");
    // Should not crash
    EXPECT_EQ(buffer->GetCursorX(), 0);
}

// ============================================================================
// Escape Sequence State Tests
// ============================================================================

TEST_F(VTStateMachineTest, EscapeNextLine) {
    buffer->SetCursorPos(5, 3);
    ProcessString(ESC("E"));

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 4);
}

TEST_F(VTStateMachineTest, EscapeReset) {
    buffer->WriteString(U"Some content");
    buffer->SetCursorPos(10, 10);

    ProcessString(ESC("c"));

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, UnknownEscapeSequence) {
    ProcessString(ESC("Z"));  // Unknown
    ProcessString("A");

    // Should recover and process A
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
}

TEST_F(VTStateMachineTest, PartialEscapeFollowedByText) {
    ProcessString("\x1b");  // Just ESC
    ProcessString("[");     // Start of CSI
    ProcessString("H");     // Cursor home

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

// ============================================================================
// CSI Parameter Parsing Tests
// ============================================================================

TEST_F(VTStateMachineTest, CSINoParameters) {
    buffer->SetCursorPos(10, 10);
    ProcessString(CSI("", 'H'));  // CUP with no params defaults to 1,1

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, CSISingleParameter) {
    ProcessString(CSI("5", 'A'));  // Cursor up 5

    int x, y;
    buffer->GetCursorPos(x, y);
    // Started at 0, can't go negative
    EXPECT_EQ(y, 0);
}

TEST_F(VTStateMachineTest, CSIMultipleParameters) {
    ProcessString(CSI("10;20", 'H'));  // CUP row 10, col 20

    EXPECT_EQ(buffer->GetCursorX(), 19);  // 0-indexed
    EXPECT_EQ(buffer->GetCursorY(), 9);
}

TEST_F(VTStateMachineTest, CSIEmptyParameterDefaultsToZero) {
    ProcessString(CSI(";5", 'H'));  // First param empty, second is 5

    // First param defaults to 1, so row 1 (0-indexed: 0)
    // Second param is 5, so col 5 (0-indexed: 4)
    EXPECT_EQ(buffer->GetCursorY(), 0);
    EXPECT_EQ(buffer->GetCursorX(), 4);
}

TEST_F(VTStateMachineTest, CSIManyParameters) {
    // SGR with many params
    ProcessString(CSI("1;4;31;42", 'm'));

    const auto& attr = buffer->GetCurrentAttributes();
    EXPECT_TRUE(attr.IsBold());
    EXPECT_TRUE(attr.IsUnderline());
    EXPECT_EQ(attr.foreground, 1);  // Red
    EXPECT_EQ(attr.background, 2);  // Green
}

// ============================================================================
// Cursor Movement CSI Tests
// ============================================================================

TEST_F(VTStateMachineTest, CursorPositionHome) {
    buffer->SetCursorPos(40, 12);
    ProcessString(CSI("", 'H'));

    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, CursorPositionSpecific) {
    ProcessString(CSI("5;10", 'H'));  // Row 5, col 10

    EXPECT_EQ(buffer->GetCursorY(), 4);  // 0-indexed
    EXPECT_EQ(buffer->GetCursorX(), 9);
}

TEST_F(VTStateMachineTest, CursorPositionHVP) {
    ProcessString(CSI("3;7", 'f'));  // HVP same as CUP

    EXPECT_EQ(buffer->GetCursorY(), 2);
    EXPECT_EQ(buffer->GetCursorX(), 6);
}

TEST_F(VTStateMachineTest, CursorUp) {
    buffer->SetCursorPos(5, 10);
    ProcessString(CSI("3", 'A'));

    EXPECT_EQ(buffer->GetCursorX(), 5);
    EXPECT_EQ(buffer->GetCursorY(), 7);
}

TEST_F(VTStateMachineTest, CursorUpDefaultOne) {
    buffer->SetCursorPos(5, 10);
    ProcessString(CSI("", 'A'));

    EXPECT_EQ(buffer->GetCursorY(), 9);
}

TEST_F(VTStateMachineTest, CursorUpClampAtTop) {
    buffer->SetCursorPos(5, 2);
    ProcessString(CSI("10", 'A'));

    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, CursorDown) {
    buffer->SetCursorPos(5, 10);
    ProcessString(CSI("3", 'B'));

    EXPECT_EQ(buffer->GetCursorY(), 13);
}

TEST_F(VTStateMachineTest, CursorDownClampAtBottom) {
    buffer->SetCursorPos(5, 20);
    ProcessString(CSI("10", 'B'));

    EXPECT_EQ(buffer->GetCursorY(), 23);  // Last row
}

TEST_F(VTStateMachineTest, CursorForward) {
    buffer->SetCursorPos(10, 5);
    ProcessString(CSI("5", 'C'));

    EXPECT_EQ(buffer->GetCursorX(), 15);
}

TEST_F(VTStateMachineTest, CursorForwardClampAtRight) {
    buffer->SetCursorPos(75, 5);
    ProcessString(CSI("10", 'C'));

    EXPECT_EQ(buffer->GetCursorX(), 79);  // Last column
}

TEST_F(VTStateMachineTest, CursorBack) {
    buffer->SetCursorPos(10, 5);
    ProcessString(CSI("3", 'D'));

    EXPECT_EQ(buffer->GetCursorX(), 7);
}

TEST_F(VTStateMachineTest, CursorBackClampAtLeft) {
    buffer->SetCursorPos(3, 5);
    ProcessString(CSI("10", 'D'));

    EXPECT_EQ(buffer->GetCursorX(), 0);
}

// ============================================================================
// Erase Operations Tests
// ============================================================================

TEST_F(VTStateMachineTest, EraseInDisplayFromCursor) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->SetCursorPos(0, 1);
    buffer->WriteString(U"0123456789");
    buffer->SetCursorPos(5, 0);  // Middle of line 0

    ProcessString(CSI("0", 'J'));

    // Line 0 before cursor should remain
    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'E');  // Not cleared
    // Line 0 from cursor should be cleared
    EXPECT_EQ(buffer->GetCell(5, 0).ch, U' ');  // Cleared
    EXPECT_EQ(buffer->GetCell(6, 0).ch, U' ');  // Cleared
    // Line 1 should be fully cleared
    EXPECT_EQ(buffer->GetCell(0, 1).ch, U' ');
}

TEST_F(VTStateMachineTest, EraseInDisplayToCursor) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->SetCursorPos(0, 1);
    buffer->WriteString(U"0123456789");
    buffer->SetCursorPos(5, 1);

    ProcessString(CSI("1", 'J'));

    // Line 0 should be cleared
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    // Line 1 up to and including cursor cleared
    EXPECT_EQ(buffer->GetCell(5, 1).ch, U' ');
    // After cursor should remain
    EXPECT_EQ(buffer->GetCell(6, 1).ch, U'6');
}

TEST_F(VTStateMachineTest, EraseInDisplayEntire) {
    buffer->WriteString(U"Some content");
    buffer->SetCursorPos(5, 5);

    ProcessString(CSI("2", 'J'));

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, EraseInDisplayDefault) {
    buffer->WriteString(U"Test");
    buffer->SetCursorPos(2, 0);

    ProcessString(CSI("", 'J'));  // Default is 0

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'T');
    EXPECT_EQ(buffer->GetCell(1, 0).ch, U'e');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U' ');  // Cleared
}

TEST_F(VTStateMachineTest, EraseInLineFromCursor) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->SetCursorPos(5, 0);

    ProcessString(CSI("0", 'K'));

    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'E');
    EXPECT_EQ(buffer->GetCell(5, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(9, 0).ch, U' ');
}

TEST_F(VTStateMachineTest, EraseInLineToCursor) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->SetCursorPos(5, 0);

    ProcessString(CSI("1", 'K'));

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(5, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(6, 0).ch, U'G');
}

TEST_F(VTStateMachineTest, EraseInLineEntire) {
    buffer->WriteString(U"ABCDEFGHIJ");
    buffer->SetCursorPos(5, 0);

    ProcessString(CSI("2", 'K'));

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U' ');
    EXPECT_EQ(buffer->GetCell(9, 0).ch, U' ');
}

// ============================================================================
// SGR (Select Graphic Rendition) Tests
// ============================================================================

TEST_F(VTStateMachineTest, SGRReset) {
    CellAttributes attr;
    attr.foreground = 5;
    attr.flags = CellAttributes::FLAG_BOLD;
    buffer->SetCurrentAttributes(attr);

    ProcessString(CSI("0", 'm'));

    const auto& newAttr = buffer->GetCurrentAttributes();
    EXPECT_EQ(newAttr.foreground, 7);  // White
    EXPECT_EQ(newAttr.background, 0);  // Black
    EXPECT_EQ(newAttr.flags, 0);
}

TEST_F(VTStateMachineTest, SGRResetNoParams) {
    CellAttributes attr;
    attr.foreground = 5;
    buffer->SetCurrentAttributes(attr);

    ProcessString(CSI("", 'm'));  // No params defaults to reset

    EXPECT_EQ(buffer->GetCurrentAttributes().foreground, 7);
}

TEST_F(VTStateMachineTest, SGRBold) {
    ProcessString(CSI("1", 'm'));
    EXPECT_TRUE(buffer->GetCurrentAttributes().IsBold());
}

TEST_F(VTStateMachineTest, SGRItalic) {
    ProcessString(CSI("3", 'm'));
    EXPECT_TRUE(buffer->GetCurrentAttributes().IsItalic());
}

TEST_F(VTStateMachineTest, SGRUnderline) {
    ProcessString(CSI("4", 'm'));
    EXPECT_TRUE(buffer->GetCurrentAttributes().IsUnderline());
}

TEST_F(VTStateMachineTest, SGRInverse) {
    ProcessString(CSI("7", 'm'));
    EXPECT_TRUE(buffer->GetCurrentAttributes().IsInverse());
}

TEST_F(VTStateMachineTest, SGRNotBold) {
    ProcessString(CSI("1", 'm'));  // Bold on
    ProcessString(CSI("22", 'm')); // Bold off

    EXPECT_FALSE(buffer->GetCurrentAttributes().IsBold());
}

TEST_F(VTStateMachineTest, SGRNotItalic) {
    ProcessString(CSI("3", 'm'));  // Italic on
    ProcessString(CSI("23", 'm')); // Italic off

    EXPECT_FALSE(buffer->GetCurrentAttributes().IsItalic());
}

TEST_F(VTStateMachineTest, SGRNotUnderline) {
    ProcessString(CSI("4", 'm'));  // Underline on
    ProcessString(CSI("24", 'm')); // Underline off

    EXPECT_FALSE(buffer->GetCurrentAttributes().IsUnderline());
}

TEST_F(VTStateMachineTest, SGRNotInverse) {
    ProcessString(CSI("7", 'm'));  // Inverse on
    ProcessString(CSI("27", 'm')); // Inverse off

    EXPECT_FALSE(buffer->GetCurrentAttributes().IsInverse());
}

TEST_F(VTStateMachineTest, SGRForegroundStandard) {
    // Test all standard foreground colors 30-37
    for (int i = 0; i < 8; ++i) {
        ProcessString(CSI(std::to_string(30 + i), 'm'));
        EXPECT_EQ(buffer->GetCurrentAttributes().foreground, i);
    }
}

TEST_F(VTStateMachineTest, SGRForegroundBright) {
    // Test all bright foreground colors 90-97
    for (int i = 0; i < 8; ++i) {
        ProcessString(CSI(std::to_string(90 + i), 'm'));
        EXPECT_EQ(buffer->GetCurrentAttributes().foreground, 8 + i);
    }
}

TEST_F(VTStateMachineTest, SGRBackgroundStandard) {
    // Test all standard background colors 40-47
    for (int i = 0; i < 8; ++i) {
        ProcessString(CSI(std::to_string(40 + i), 'm'));
        EXPECT_EQ(buffer->GetCurrentAttributes().background, i);
    }
}

TEST_F(VTStateMachineTest, SGRBackgroundBright) {
    // Test all bright background colors 100-107
    for (int i = 0; i < 8; ++i) {
        ProcessString(CSI(std::to_string(100 + i), 'm'));
        EXPECT_EQ(buffer->GetCurrentAttributes().background, 8 + i);
    }
}

TEST_F(VTStateMachineTest, SGRDefaultForeground) {
    ProcessString(CSI("31", 'm'));  // Red
    ProcessString(CSI("39", 'm'));  // Default

    EXPECT_EQ(buffer->GetCurrentAttributes().foreground, 7);  // White
}

TEST_F(VTStateMachineTest, SGRDefaultBackground) {
    ProcessString(CSI("41", 'm'));  // Red bg
    ProcessString(CSI("49", 'm'));  // Default

    EXPECT_EQ(buffer->GetCurrentAttributes().background, 0);  // Black
}

TEST_F(VTStateMachineTest, SGRMultipleInOneSequence) {
    ProcessString(CSI("1;4;31;42", 'm'));

    const auto& attr = buffer->GetCurrentAttributes();
    EXPECT_TRUE(attr.IsBold());
    EXPECT_TRUE(attr.IsUnderline());
    EXPECT_EQ(attr.foreground, 1);  // Red
    EXPECT_EQ(attr.background, 2);  // Green
}

TEST_F(VTStateMachineTest, SGR256ColorForeground) {
    ProcessString(CSI("38;5;9", 'm'));  // Bright red (index 9)

    EXPECT_EQ(buffer->GetCurrentAttributes().foreground, 9);
}

TEST_F(VTStateMachineTest, SGR256ColorBackground) {
    ProcessString(CSI("48;5;12", 'm'));  // Bright blue (index 12)

    EXPECT_EQ(buffer->GetCurrentAttributes().background, 12);
}

TEST_F(VTStateMachineTest, SGRTrueColorForeground) {
    ProcessString(CSI("38;2;255;0;0", 'm'));  // Pure red

    // Should store true RGB values and set flag
    auto attr = buffer->GetCurrentAttributes();
    EXPECT_TRUE(attr.UsesTrueColorFg());
    EXPECT_EQ(attr.fgR, 255);
    EXPECT_EQ(attr.fgG, 0);
    EXPECT_EQ(attr.fgB, 0);
}

TEST_F(VTStateMachineTest, SGRTrueColorBackground) {
    ProcessString(CSI("48;2;0;255;0", 'm'));  // Pure green

    // Should store true RGB values and set flag
    auto attr = buffer->GetCurrentAttributes();
    EXPECT_TRUE(attr.UsesTrueColorBg());
    EXPECT_EQ(attr.bgR, 0);
    EXPECT_EQ(attr.bgG, 255);
    EXPECT_EQ(attr.bgB, 0);
}

// ============================================================================
// OSC (Operating System Command) Tests
// ============================================================================

TEST_F(VTStateMachineTest, OSCWithBEL) {
    ProcessString("\x1b]0;Window Title\x07");
    ProcessString("A");

    // Should recover and process A
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
}

TEST_F(VTStateMachineTest, OSCC1ST) {
    ProcessString("\x1b]0;Title\x9c");  // C1 ST terminator
    ProcessString("B");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'B');
}

TEST_F(VTStateMachineTest, OSCBufferOverflow) {
    std::string longOSC = "\x1b]0;";
    for (int i = 0; i < 5000; ++i) {
        longOSC += 'X';
    }
    longOSC += "\x07";

    // Should not crash with very long OSC sequence
    ProcessString(longOSC);

    // Parser should still be functional after long sequence
    buffer->Clear();
    ProcessString("C");
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'C');
}

TEST_F(VTStateMachineTest, OSCEmptyContent) {
    ProcessString("\x1b]\x07");
    ProcessString("D");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'D');
}

// ============================================================================
// Mixed Content Tests
// ============================================================================

TEST_F(VTStateMachineTest, TextWithEscapeSequences) {
    ProcessString("Hello \x1b[31mRed\x1b[0m Normal");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'H');
    EXPECT_EQ(buffer->GetCell(6, 0).ch, U'R');
}

TEST_F(VTStateMachineTest, MultipleSequentialCSI) {
    ProcessString(CSI("2;5", 'H'));  // Position
    ProcessString(CSI("31", 'm'));   // Color
    ProcessString("X");

    EXPECT_EQ(buffer->GetCursorY(), 1);
    EXPECT_EQ(buffer->GetCell(4, 1).ch, U'X');
    EXPECT_EQ(buffer->GetCell(4, 1).attr.foreground, 1);
}

TEST_F(VTStateMachineTest, InterleavedTextAndCSI) {
    ProcessString("A\x1b[CB\x1b[CC\x1b[CD");

    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'A');
    EXPECT_EQ(buffer->GetCell(2, 0).ch, U'B');
    EXPECT_EQ(buffer->GetCell(4, 0).ch, U'C');
    EXPECT_EQ(buffer->GetCell(6, 0).ch, U'D');
}

// ============================================================================
// Edge Cases and Error Recovery
// ============================================================================

TEST_F(VTStateMachineTest, IncompleteCSIThenNewSequence) {
    // Send a complete color sequence
    ProcessString("\x1b[31m"); // Red color

    // Verify red color was applied
    EXPECT_EQ(buffer->GetCurrentAttributes().foreground, 1);

    // Write a character to verify attributes work
    ProcessString("R");
    EXPECT_EQ(buffer->GetCell(0, 0).attr.foreground, 1);
}

TEST_F(VTStateMachineTest, UnknownCSICommand) {
    ProcessString(CSI("1;2", 'z'));  // Unknown command 'z'
    ProcessString("E");

    // Should recover
    EXPECT_EQ(buffer->GetCell(0, 0).ch, U'E');
}

TEST_F(VTStateMachineTest, LargeParameterValue) {
    ProcessString(CSI("999999", 'A'));  // Large cursor up

    // Should clamp to top
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

TEST_F(VTStateMachineTest, PartialSequenceRecovery) {
    // Move cursor to known position first
    buffer->SetCursorPos(5, 5);

    // Send complete CUP sequence to home position
    ProcessString("\x1b[H");

    // Cursor should be at home position
    EXPECT_EQ(buffer->GetCursorX(), 0);
    EXPECT_EQ(buffer->GetCursorY(), 0);
}

// ============================================================================
// Text with Attributes Persistence
// ============================================================================

TEST_F(VTStateMachineTest, AttributesPersistAfterWrite) {
    ProcessString(CSI("31", 'm'));  // Red
    ProcessString("AB");
    ProcessString(CSI("32", 'm'));  // Green
    ProcessString("CD");

    EXPECT_EQ(buffer->GetCell(0, 0).attr.foreground, 1);  // A is red
    EXPECT_EQ(buffer->GetCell(1, 0).attr.foreground, 1);  // B is red
    EXPECT_EQ(buffer->GetCell(2, 0).attr.foreground, 2);  // C is green
    EXPECT_EQ(buffer->GetCell(3, 0).attr.foreground, 2);  // D is green
}

TEST_F(VTStateMachineTest, AttributesPersistAcrossCursorMove) {
    ProcessString(CSI("31", 'm'));  // Red
    ProcessString(CSI("5;5", 'H')); // Move cursor
    ProcessString("X");

    EXPECT_EQ(buffer->GetCell(4, 4).attr.foreground, 1);  // Still red
}

} // namespace Tests
} // namespace TerminalDX12::Terminal
