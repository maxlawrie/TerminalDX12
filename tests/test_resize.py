#!/usr/bin/env python3
"""
Window resize and text reflow tests for TerminalDX12.

These 5 tests cover:
1. Basic resize to 1/4 size
2. Text reflow roundtrip consistency
3. Resize with scrollback content
4. Maximize/restore operations
5. Rapid resize stability
"""

import pytest
import time
import numpy as np
import win32gui
import win32con
from PIL import Image

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

    def test_text_reflow_roundtrip(self, terminal):
        """Test that text reflows correctly: maximize -> 1/4 width -> maximize.

        Verifies that the final state matches the initial state after a
        resize roundtrip that forces text to wrap and unwrap.
        """
        hwnd = terminal.hwnd
        original_rect = win32gui.GetWindowRect(hwnd)

        # Clear and maximize
        terminal.send_keys("cls\n")
        time.sleep(0.3)
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        max_rect = win32gui.GetWindowRect(hwnd)
        max_width = max_rect[2] - max_rect[0]
        max_height = max_rect[3] - max_rect[1]

        # Print a 100-character string
        test_string = "A" * 100
        terminal.send_keys(f"echo {test_string}\n")
        time.sleep(0.5)

        # Screenshot 1: maximized
        screenshot_wide1, _ = terminal.wait_and_screenshot("reflow_1_wide")
        assert terminal.analyze_text_presence(screenshot_wide1), "No text when maximized"

        # Resize to 1/4 width (force reflow)
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)
        quarter_width = max(300, max_width // 4)
        win32gui.MoveWindow(hwnd, 0, 0, quarter_width, max_height, True)
        time.sleep(0.5)

        screenshot_narrow, _ = terminal.wait_and_screenshot("reflow_2_narrow")
        assert terminal.analyze_text_presence(screenshot_narrow), "No text at 1/4 width"

        # Maximize again
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        # Screenshot 3: maximized again
        screenshot_wide2, _ = terminal.wait_and_screenshot("reflow_3_wide")
        assert terminal.analyze_text_presence(screenshot_wide2), "No text after re-maximize"

        # Compare screenshots 1 and 3 - should be very similar
        arr1 = np.array(screenshot_wide1)
        arr3 = np.array(screenshot_wide2)

        if arr1.shape == arr3.shape:
            mse = np.mean((arr1.astype(float) - arr3.astype(float)) ** 2)
            # Allow tolerance for cursor blink
            if mse > 500:
                diff = np.abs(arr1.astype(float) - arr3.astype(float)).astype(np.uint8)
                Image.fromarray(diff).save(terminal.screenshot_dir / "reflow_diff.png")
                pytest.fail(f"Screenshots differ after roundtrip (MSE={mse:.2f})")

        # Restore original window state
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)
        win32gui.MoveWindow(hwnd, original_rect[0], original_rect[1],
                           original_rect[2] - original_rect[0],
                           original_rect[3] - original_rect[1], True)

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

    def test_maximize_restore(self, terminal):
        """Test maximize and restore window operations."""
        hwnd = terminal.hwnd
        original_rect = win32gui.GetWindowRect(hwnd)

        terminal.send_keys("echo Before maximize\n")
        time.sleep(0.3)

        # Maximize
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        screenshot_max, _ = terminal.wait_and_screenshot("maximized")
        assert terminal.analyze_text_presence(screenshot_max), "No text when maximized"

        # Type while maximized
        terminal.send_keys("echo While maximized\n")
        time.sleep(0.3)

        # Restore
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)

        terminal.send_keys("echo After restore\n")
        time.sleep(0.3)

        screenshot_restored, _ = terminal.wait_and_screenshot("restored")
        assert terminal.analyze_text_presence(screenshot_restored), "No text after restore"

        # Restore original position
        win32gui.MoveWindow(hwnd, original_rect[0], original_rect[1],
                           original_rect[2] - original_rect[0],
                           original_rect[3] - original_rect[1], True)

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
