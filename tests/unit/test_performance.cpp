// test_performance.cpp - Performance and stress tests
// Tests for buffer operations under load

#include <gtest/gtest.h>
#include "terminal/ScreenBuffer.h"
#include "terminal/VTStateMachine.h"
#include <memory>
#include <chrono>
#include <string>

namespace TerminalDX12::Terminal {
namespace Tests {

// ============================================================================
// Test Fixture
// ============================================================================

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        buffer = std::make_unique<ScreenBuffer>(80, 24);
        parser = std::make_unique<VTStateMachine>(buffer.get());
    }

    // Helper to measure execution time
    template<typename Func>
    double MeasureMs(Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        func();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end - start;
        return duration.count();
    }

    std::unique_ptr<ScreenBuffer> buffer;
    std::unique_ptr<VTStateMachine> parser;
};

// ============================================================================
// Buffer Operation Performance
// ============================================================================

TEST_F(PerformanceTest, LargeBufferCreation) {
    double ms = MeasureMs([&]() {
        auto largeBuffer = std::make_unique<ScreenBuffer>(200, 100, 50000);
    });

    // Should complete in reasonable time (< 100ms)
    EXPECT_LT(ms, 100.0);
}

TEST_F(PerformanceTest, RapidCharacterWrites) {
    const int numChars = 10000;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numChars; ++i) {
            buffer->WriteChar(U'X');
        }
    });

    // Should be able to write 10k characters in < 50ms
    EXPECT_LT(ms, 50.0);
}

TEST_F(PerformanceTest, RapidScrolling) {
    const int numScrolls = 1000;

    // Fill buffer with content first
    for (int y = 0; y < buffer->GetRows(); ++y) {
        buffer->SetCursorPos(0, y);
        buffer->WriteString(U"Content on line");
    }

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numScrolls; ++i) {
            buffer->ScrollUp(1);
        }
    });

    // Should complete 1000 scrolls in < 100ms
    EXPECT_LT(ms, 100.0);
}

TEST_F(PerformanceTest, RapidCursorMovement) {
    const int numMoves = 100000;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numMoves; ++i) {
            buffer->SetCursorPos(i % 80, (i / 80) % 24);
        }
    });

    // Should complete 100k cursor moves in < 50ms
    EXPECT_LT(ms, 50.0);
}

TEST_F(PerformanceTest, RapidClearOperations) {
    const int numClears = 1000;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numClears; ++i) {
            buffer->Clear();
        }
    });

    // Should complete 1000 clears in < 100ms
    EXPECT_LT(ms, 100.0);
}

// ============================================================================
// Parser Performance
// ============================================================================

TEST_F(PerformanceTest, LargeTextInput) {
    std::string largeText(10000, 'A');

    double ms = MeasureMs([&]() {
        parser->ProcessInput(largeText.c_str(), largeText.size());
    });

    // Should process 10k characters in < 50ms
    EXPECT_LT(ms, 50.0);
}

TEST_F(PerformanceTest, ManyEscapeSequences) {
    const int numSequences = 1000;

    // Create a string with many escape sequences
    std::string input;
    for (int i = 0; i < numSequences; ++i) {
        input += "\x1b[31mX\x1b[0m";  // Red X then reset
    }

    double ms = MeasureMs([&]() {
        parser->ProcessInput(input.c_str(), input.size());
    });

    // Should process 1000 sequences in < 100ms
    EXPECT_LT(ms, 100.0);
}

TEST_F(PerformanceTest, ComplexSGRSequences) {
    const int numSequences = 1000;

    // Create complex SGR sequences
    std::string input;
    for (int i = 0; i < numSequences; ++i) {
        input += "\x1b[1;4;38;5;196;48;5;232mX";  // Bold, underline, 256-color fg/bg
    }

    double ms = MeasureMs([&]() {
        parser->ProcessInput(input.c_str(), input.size());
    });

    // Should complete in < 200ms
    EXPECT_LT(ms, 200.0);
}

TEST_F(PerformanceTest, CursorMovementSequences) {
    const int numSequences = 1000;

    std::string input;
    for (int i = 0; i < numSequences; ++i) {
        input += "\x1b[10;20H";  // CUP to row 10, col 20
    }

    double ms = MeasureMs([&]() {
        parser->ProcessInput(input.c_str(), input.size());
    });

    // Should complete in < 100ms
    EXPECT_LT(ms, 100.0);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(PerformanceTest, StressScrollbackFull) {
    // Use a buffer with limited scrollback
    auto smallScrollback = std::make_unique<ScreenBuffer>(80, 24, 100);
    auto stressParser = std::make_unique<VTStateMachine>(smallScrollback.get());

    // Generate enough output to fill and rotate scrollback multiple times
    std::string line(80, 'X');
    line += '\n';

    double ms = MeasureMs([&]() {
        for (int i = 0; i < 500; ++i) {
            stressParser->ProcessInput(line.c_str(), line.size());
        }
    });

    // Should handle scrollback rotation efficiently
    EXPECT_LT(ms, 500.0);
}

TEST_F(PerformanceTest, RapidBufferSwitching) {
    const int numSwitches = 1000;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numSwitches; ++i) {
            buffer->UseAlternativeBuffer(true);
            buffer->UseAlternativeBuffer(false);
        }
    });

    // Should complete 1000 switches in < 50ms
    EXPECT_LT(ms, 50.0);
}

TEST_F(PerformanceTest, RepeatedResize) {
    const int numResizes = 100;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numResizes; ++i) {
            buffer->Resize(120, 40);
            buffer->Resize(80, 24);
        }
    });

    // Should complete 100 resize pairs in < 200ms
    EXPECT_LT(ms, 200.0);
}

// ============================================================================
// Memory Tests
// ============================================================================

TEST_F(PerformanceTest, LargeScrollbackAllocation) {
    // Test that large scrollback doesn't cause issues
    auto largeScrollback = std::make_unique<ScreenBuffer>(80, 24, 100000);

    // Fill the scrollback
    for (int i = 0; i < 100; ++i) {
        largeScrollback->WriteString(U"Test line content here\n");
    }

    // Should still be functional
    EXPECT_EQ(largeScrollback->GetCols(), 80);
    EXPECT_EQ(largeScrollback->GetRows(), 24);
}

TEST_F(PerformanceTest, CellAccessPattern) {
    // Random access pattern
    const int numAccesses = 100000;

    double ms = MeasureMs([&]() {
        for (int i = 0; i < numAccesses; ++i) {
            int x = (i * 7) % 80;
            int y = (i * 13) % 24;
            buffer->GetCell(x, y);
        }
    });

    // Should complete 100k accesses in < 50ms
    EXPECT_LT(ms, 50.0);
}

} // namespace Tests
} // namespace TerminalDX12::Terminal
