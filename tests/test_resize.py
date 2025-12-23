#!/usr/bin/env python3
"""
Window resize and text reflow tests for TerminalDX12.

These 3 tests cover:
1. Basic resize to 1/4 size
2. Resize with scrollback content
3. Rapid resize stability
"""

import pytest
import time
import win32gui

from config import TestConfig


@pytest.mark.slow
class TestResize:
    """Window resize and text reflow tests."""

    def test_resize_to_quarter(self, terminal):
        """Test resizing window to 1/4 size with long text that must wrap."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Type a long line that will need to wrap when narrow
        terminal.send_keys("cls\n")
        time.sleep(0.3)
        terminal.send_keys("echo " + "X" * 80 + "\n")
        time.sleep(0.5)

        # Shrink to 1/4 size
        quarter_width = max(300, original_width // 4)
        quarter_height = max(200, original_height // 4)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], quarter_width, quarter_height, True)
        time.sleep(0.5)

        screenshot_small, _ = terminal.wait_and_screenshot("resize_quarter")
        assert terminal.analyze_text_presence(screenshot_small), "No text visible at 1/4 size"

        # Restore to original size
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)
        time.sleep(0.3)

        screenshot_restored, _ = terminal.wait_and_screenshot("resize_restored")
        assert terminal.analyze_text_presence(screenshot_restored), "No text visible after restore"

    def test_resize_with_scrollback(self, terminal):
        """Test that resize to 1/4 size preserves scrollback content."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate scrollback content with long lines
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('1..30 | % { echo "Scrollback line $_ with extra text to force wrapping" }\n')
        time.sleep(2)

        # Resize to 1/4 size
        quarter_width = max(300, original_width // 4)
        quarter_height = max(200, original_height // 4)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], quarter_width, quarter_height, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("scrollback_resized")
        assert terminal.analyze_text_presence(screenshot), "No text visible after resize with scrollback"

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)

    def test_rapid_resize_stability(self, terminal):
        """Test that rapid resize between full and 1/4 size doesn't crash."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate content that will reflow
        terminal.send_keys("echo " + "Y" * 80 + "\n")
        time.sleep(0.3)

        quarter_width = max(300, original_width // 4)
        quarter_height = max(200, original_height // 4)

        # Rapid resize cycles
        for i in range(5):
            if i % 2 == 0:
                win32gui.MoveWindow(hwnd, rect[0], rect[1], quarter_width, quarter_height, True)
            else:
                win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)
            time.sleep(0.15)

        # Settle and restore
        time.sleep(0.5)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)
        time.sleep(0.3)

        # Terminal should still work
        terminal.send_keys("echo After rapid resize\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("rapid_resize")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after rapid resize"
