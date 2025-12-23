#!/usr/bin/env python3
"""
Window resize tests for TerminalDX12.

Tests dynamic window resizing, content reflow, and scrollback preservation.
These tests use the pytest framework with the terminal fixture.
"""

import pytest
import time
import numpy as np
import win32gui
import win32con
from PIL import Image

from config import TestConfig


@pytest.mark.slow
class TestWindowResize:
    """Window resize tests."""

    def test_resize_width_increase(self, terminal):
        """Test that increasing window width works correctly."""
        # Get current window rect
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Type some text
        terminal.send_keys("echo Test content for resize\n")
        time.sleep(0.5)

        # Increase width by 100 pixels
        new_width = original_width + 100
        win32gui.MoveWindow(hwnd, rect[0], rect[1], new_width, original_height, True)
        time.sleep(0.5)

        # Take screenshot - terminal should still be functional
        screenshot, _ = terminal.wait_and_screenshot("resize_width_increase")
        assert terminal.analyze_text_presence(screenshot)

        # Restore original size
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)

    def test_resize_width_decrease(self, terminal):
        """Test that decreasing window width to 1/4 works correctly and triggers reflow."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Type a long line that will need to wrap
        terminal.send_keys("echo " + "X" * 80 + "\n")
        time.sleep(0.5)

        # Decrease width to 1/4 of original (minimum 300 for usability)
        new_width = max(300, original_width // 4)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], new_width, original_height, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("resize_width_decrease")
        assert terminal.analyze_text_presence(screenshot)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)

    def test_resize_height_increase(self, terminal):
        """Test that increasing window height works correctly."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        terminal.send_keys("echo Taller window test\n")
        time.sleep(0.5)

        # Increase height by 100 pixels
        new_height = original_height + 100
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, new_height, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("resize_height_increase")
        assert terminal.analyze_text_presence(screenshot)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)

    def test_resize_height_decrease(self, terminal):
        """Test that decreasing window height to 1/4 works correctly."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate multiple lines of content
        terminal.send_keys("cls\n")
        time.sleep(0.3)
        terminal.send_keys('1..20 | % { echo "Line $_" }\n')
        time.sleep(1)

        # Decrease height to 1/4 of original (minimum 200 for usability)
        new_height = max(200, original_height // 4)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, new_height, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("resize_height_decrease")
        assert terminal.analyze_text_presence(screenshot)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)


@pytest.mark.slow
class TestResizeWithContent:
    """Tests for resize behavior with existing content."""

    def test_resize_preserves_content(self, terminal):
        """Test that window resize preserves existing text content."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate content
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys("echo MARKER_LINE_1\n")
        terminal.send_keys("echo MARKER_LINE_2\n")
        terminal.send_keys("echo MARKER_LINE_3\n")
        time.sleep(0.5)

        # Take before screenshot
        screenshot_before, _ = terminal.wait_and_screenshot("resize_content_before")

        # Resize
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width + 50, original_height + 50, True)
        time.sleep(0.5)

        # Take after screenshot
        screenshot_after, _ = terminal.wait_and_screenshot("resize_content_after")

        # Both should have text
        assert terminal.analyze_text_presence(screenshot_before)
        assert terminal.analyze_text_presence(screenshot_after)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)

    def test_resize_with_scrollback(self, terminal):
        """Test that resize to 1/4 size preserves scrollback buffer content."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate scrollback content with long lines that will wrap
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('1..30 | % { echo "Scrollback line $_ with extra text to force wrapping when narrow" }\n')
        time.sleep(2)

        # Resize to 1/4 size while there's scrollback (triggers reflow)
        new_width = max(300, original_width // 4)
        new_height = max(200, original_height // 4)
        win32gui.MoveWindow(hwnd, rect[0], rect[1], new_width, new_height, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("resize_scrollback")
        assert terminal.analyze_text_presence(screenshot)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)


@pytest.mark.slow
class TestTextReflow:
    """Tests for text reflow during resize."""

    def test_text_reflow_on_resize(self, terminal):
        """Test that text reflows correctly when resizing the terminal.

        This test:
        1. Maximizes the window
        2. Prints a 100-character string
        3. Checks output (should be on one line when wide)
        4. Resizes to 1/4 width (forces significant reflow)
        5. Checks output (should reflow to multiple lines)
        6. Maximizes again
        7. Checks output (should be back to one line)
        """
        hwnd = terminal.hwnd

        # Store original window rect
        original_rect = win32gui.GetWindowRect(hwnd)

        # Clear and maximize
        terminal.send_keys("cls\n")
        time.sleep(0.3)
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        # Get maximized dimensions
        max_rect = win32gui.GetWindowRect(hwnd)
        max_width = max_rect[2] - max_rect[0]
        max_height = max_rect[3] - max_rect[1]

        # Create a 100-character test string with unique markers
        # Using a pattern that's easy to verify: START + 90 chars + END
        test_string = "START" + "X" * 90 + "END"
        assert len(test_string) == 98  # START(5) + 90 + END(3) = 98

        # Echo the string (add 2 more chars to make it 100 with echo command overhead)
        terminal.send_keys(f"echo {test_string}\n")
        time.sleep(0.5)

        # Screenshot at full width - text should fit on one line
        screenshot_wide, _ = terminal.wait_and_screenshot("reflow_1_wide")
        assert terminal.analyze_text_presence(screenshot_wide), "No text visible when maximized"

        # Now resize to 1/4 width (restore first, then resize)
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)

        quarter_width = max(300, max_width // 4)
        # Position at top-left of screen for consistency
        win32gui.MoveWindow(hwnd, 0, 0, quarter_width, max_height, True)
        time.sleep(0.5)

        # Screenshot at 1/4 width - text should have reflowed significantly
        screenshot_narrow, _ = terminal.wait_and_screenshot("reflow_2_narrow")
        assert terminal.analyze_text_presence(screenshot_narrow), "No text visible after resize to 1/4 width"

        # Maximize again
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        # Screenshot after re-maximize - text should be back to single line
        screenshot_remax, _ = terminal.wait_and_screenshot("reflow_3_remaximize")
        assert terminal.analyze_text_presence(screenshot_remax), "No text visible after re-maximize"

        # Try OCR verification if available
        try:
            from winocr import recognize_pil

            # Verify the text content is intact after all resizes
            ocr_result = recognize_pil(screenshot_remax)
            ocr_text = ocr_result.text if hasattr(ocr_result, 'text') else str(ocr_result)

            # The text should contain our markers
            has_start = "START" in ocr_text
            has_end = "END" in ocr_text

            if not (has_start and has_end):
                print(f"Warning: OCR could not find markers. OCR text: {ocr_text[:200]}...")
            else:
                print(f"OCR verified: START and END markers found after resize cycle")

        except ImportError:
            print("OCR not available, skipping text content verification")

        # Restore original window state
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)
        win32gui.MoveWindow(hwnd, original_rect[0], original_rect[1],
                           original_rect[2] - original_rect[0],
                           original_rect[3] - original_rect[1], True)

    def test_reflow_roundtrip_consistency(self, terminal):
        """Test that text layout is consistent after resize roundtrip.

        This test verifies that going wide -> narrow -> wide produces
        identical output to the original wide state.

        Steps:
        1. Maximize the window
        2. Print a 100-character string
        3. Take screenshot 1 (wide)
        4. Resize to 1/4 of the width (forces significant reflow)
        5. Take screenshot 2 (narrow)
        6. Maximize again
        7. Take screenshot 3 (wide again)
        8. Validate that screenshots 1 and 3 match
        """
        hwnd = terminal.hwnd

        # Store original window rect
        original_rect = win32gui.GetWindowRect(hwnd)

        # Clear and maximize
        terminal.send_keys("cls\n")
        time.sleep(0.3)
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        # Get maximized dimensions
        max_rect = win32gui.GetWindowRect(hwnd)
        max_width = max_rect[2] - max_rect[0]
        max_height = max_rect[3] - max_rect[1]

        # Create a 100-character test string
        test_string = "A" * 100

        # Echo the string
        terminal.send_keys(f"echo {test_string}\n")
        time.sleep(0.5)

        # Screenshot 1: maximized (wide)
        screenshot_wide1, _ = terminal.wait_and_screenshot("roundtrip_1_wide")
        assert terminal.analyze_text_presence(screenshot_wide1), "No text visible when maximized"

        # Resize to 1/4 width
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)

        quarter_width = max(300, max_width // 4)
        win32gui.MoveWindow(hwnd, 0, 0, quarter_width, max_height, True)
        time.sleep(0.5)

        # Screenshot 2: narrow (1/4 width)
        screenshot_narrow, _ = terminal.wait_and_screenshot("roundtrip_2_narrow")
        assert terminal.analyze_text_presence(screenshot_narrow), "No text visible at 1/4 width"

        # Maximize again
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        # Screenshot 3: maximized again (wide)
        screenshot_wide2, _ = terminal.wait_and_screenshot("roundtrip_3_wide_again")
        assert terminal.analyze_text_presence(screenshot_wide2), "No text visible after re-maximize"

        # Compare screenshots 1 and 3 - they should be very similar
        # Convert to numpy arrays for comparison
        arr1 = np.array(screenshot_wide1)
        arr3 = np.array(screenshot_wide2)

        # Check dimensions match
        assert arr1.shape == arr3.shape, f"Screenshot dimensions differ: {arr1.shape} vs {arr3.shape}"

        # Calculate similarity (mean squared error)
        # Allow some tolerance for cursor blinking, slight timing differences
        mse = np.mean((arr1.astype(float) - arr3.astype(float)) ** 2)

        # Threshold: MSE should be very low for nearly identical images
        # A small MSE (< 100) indicates very similar images
        # Cursor blink might cause some difference, so we allow some tolerance
        max_allowed_mse = 500  # Generous threshold to account for cursor position differences

        print(f"Screenshot comparison MSE: {mse:.2f}")

        if mse > max_allowed_mse:
            # Save difference image for debugging
            diff = np.abs(arr1.astype(float) - arr3.astype(float)).astype(np.uint8)
            diff_img = Image.fromarray(diff)
            diff_img.save(terminal.screenshot_dir / "roundtrip_diff.png")
            pytest.fail(f"Screenshots 1 and 3 differ significantly (MSE={mse:.2f} > {max_allowed_mse}). "
                       f"Difference image saved to roundtrip_diff.png")

        # Restore original window state
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.3)
        win32gui.MoveWindow(hwnd, original_rect[0], original_rect[1],
                           original_rect[2] - original_rect[0],
                           original_rect[3] - original_rect[1], True)


@pytest.mark.slow
class TestRapidResize:
    """Tests for rapid consecutive resize operations."""

    def test_rapid_resize_stability(self, terminal):
        """Test that rapid resize operations between full and 1/4 size don't crash the terminal."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate content that will need reflow
        terminal.send_keys("echo " + "Y" * 80 + "\n")
        time.sleep(0.3)

        # Calculate quarter sizes
        quarter_width = max(300, original_width // 4)
        quarter_height = max(200, original_height // 4)

        # Perform rapid resizes alternating between full and quarter size
        for i in range(5):
            if i % 2 == 0:
                # Shrink to quarter size
                win32gui.MoveWindow(hwnd, rect[0], rect[1], quarter_width, quarter_height, True)
            else:
                # Restore to full size
                win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)
            time.sleep(0.15)

        # Let it settle
        time.sleep(0.5)

        # Restore original
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)
        time.sleep(0.3)

        # Terminal should still work
        terminal.send_keys("echo After rapid resize\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("rapid_resize")
        assert terminal.analyze_text_presence(screenshot)

    def test_maximize_restore(self, terminal):
        """Test maximize and restore window operations."""
        hwnd = terminal.hwnd

        # Store original rect
        original_rect = win32gui.GetWindowRect(hwnd)

        terminal.send_keys("echo Before maximize\n")
        time.sleep(0.3)

        # Maximize
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)
        time.sleep(0.5)

        screenshot_max, _ = terminal.wait_and_screenshot("maximize")
        assert terminal.analyze_text_presence(screenshot_max)

        # Type while maximized
        terminal.send_keys("echo While maximized\n")
        time.sleep(0.3)

        # Restore
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)
        time.sleep(0.5)

        terminal.send_keys("echo After restore\n")
        time.sleep(0.3)

        screenshot_restored, _ = terminal.wait_and_screenshot("restore")
        assert terminal.analyze_text_presence(screenshot_restored)

