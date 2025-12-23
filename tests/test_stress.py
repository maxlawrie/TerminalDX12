#!/usr/bin/env python3
"""
Stress tests for TerminalDX12.

Tests terminal stability under extreme conditions.
"""

import pytest
import time
import win32gui


@pytest.mark.slow
class TestStress:
    """Stress tests for terminal stability."""

    def test_rapid_input_burst(self, terminal):
        """Rapidly type 1000 characters and verify terminal stays responsive."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Rapid burst of characters
        burst = "X" * 100
        for _ in range(10):
            terminal.send_keys(burst)
            time.sleep(0.05)  # Minimal delay

        terminal.send_keys("\n")
        time.sleep(0.5)

        # Terminal should still be responsive
        terminal.send_keys("echo STRESS_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("stress_rapid_input")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after rapid input"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "STRESS" in ocr_text.upper():
            print("Stress test verified: terminal responsive after 1000 char burst")

    def test_rapid_newlines(self, terminal):
        """Rapidly send 100 newlines to stress scrolling."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Rapid newlines
        for i in range(100):
            terminal.send_keys(f"L{i}\n", delay=0.005)

        time.sleep(1)

        # Should still be responsive
        terminal.send_keys("echo SCROLL_STRESS_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("stress_newlines")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after rapid scrolling"

    def test_rapid_clear_screen(self, terminal):
        """Rapidly clear screen 20 times."""
        for _ in range(20):
            terminal.send_keys("cls\n")
            time.sleep(0.1)

        # Should still work
        terminal.send_keys("echo CLEAR_STRESS_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("stress_clear")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after rapid clears"

    def test_long_line_output(self, terminal):
        """Test output of very long lines."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Echo a very long line (500 chars)
        long_text = "A" * 500
        terminal.send_keys(f"echo {long_text}\n")
        time.sleep(1)

        screenshot, _ = terminal.wait_and_screenshot("stress_long_line")
        assert terminal.analyze_text_presence(screenshot), "Long line output failed"

    def test_escape_sequence_flood(self, terminal):
        """Flood terminal with color escape sequences."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Send many color changes via PowerShell
        # Using Write-Host with colors
        colors = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta"]
        for i in range(50):
            color = colors[i % len(colors)]
            terminal.send_keys(f"Write-Host '{i}' -ForegroundColor {color} -NoNewline; ", delay=0.01)

        terminal.send_keys("\n")
        time.sleep(1)

        screenshot, _ = terminal.wait_and_screenshot("stress_colors")
        assert terminal.analyze_text_presence(screenshot), "Color flood caused issues"

    def test_window_still_valid(self, terminal):
        """Verify window handle still valid after stress tests."""
        # After all stress tests, window should still exist
        assert terminal.hwnd is not None, "Window handle is None"
        assert win32gui.IsWindow(terminal.hwnd), "Window no longer valid"
        assert win32gui.IsWindowVisible(terminal.hwnd), "Window not visible"
