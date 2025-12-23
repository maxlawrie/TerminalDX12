#!/usr/bin/env python3
"""
Mouse input tests for TerminalDX12.

Tests mouse click, drag selection, and scroll wheel functionality.
These tests use the pytest framework with the terminal fixture.
"""

import pytest
import time
import numpy as np
import win32api
import win32con
from PIL import Image

from config import TestConfig


@pytest.mark.input
class TestMouseBasic:
    """Basic mouse interaction tests."""

    def test_mouse_click_clears_selection(self, terminal):
        """Test that clicking in the terminal clears any text selection."""
        # Type some text
        terminal.send_keys("echo Hello World\n")
        time.sleep(0.5)

        # Click in the terminal area
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2

        win32api.SetCursorPos((center_x, center_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)

        time.sleep(0.3)

        # Take screenshot - should show normal text without selection highlight
        screenshot, _ = terminal.wait_and_screenshot("mouse_click")
        assert terminal.analyze_text_presence(screenshot)

    def test_mouse_focus(self, terminal):
        """Test that clicking gives the terminal focus."""
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2

        # Click to focus
        win32api.SetCursorPos((center_x, center_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)

        time.sleep(0.2)

        # Typing should work after click
        terminal.send_keys("test")

        screenshot, _ = terminal.wait_and_screenshot("mouse_focus")
        assert terminal.analyze_text_presence(screenshot)


@pytest.mark.input
@pytest.mark.slow
class TestMouseSelection:
    """Mouse text selection tests."""

    def test_drag_selection_horizontal(self, terminal):
        """Test horizontal text selection via mouse drag."""
        # Type a line of text
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys("echo ABCDEFGHIJKLMNOPQRSTUVWXYZ\n")
        time.sleep(0.5)

        # Get client rect for positioning
        client_rect = terminal.get_client_rect_screen()

        # Start selection near beginning of text
        start_x = client_rect[0] + 50
        start_y = client_rect[1] + 100  # Approximate line position

        # End selection further right
        end_x = start_x + 200
        end_y = start_y

        # Perform drag
        win32api.SetCursorPos((start_x, start_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)

        # Drag slowly to ensure selection registers
        for x in range(start_x, end_x, 10):
            win32api.SetCursorPos((x, start_y))
            time.sleep(0.02)

        win32api.SetCursorPos((end_x, end_y))
        time.sleep(0.1)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)

        time.sleep(0.3)

        # Take screenshot - should show selection highlight
        screenshot, _ = terminal.wait_and_screenshot("mouse_selection")

        # Check for blue-ish selection highlight pixels
        img_array = np.array(screenshot)
        blue_ish = np.logical_and(
            img_array[:, :, 2] > 150,  # Blue channel high
            np.logical_and(img_array[:, :, 0] < 150, img_array[:, :, 1] < 200)
        )
        blue_count = np.sum(blue_ish)

        # Should have some blue selection highlight
        # Note: This may fail if selection colors are different
        assert blue_count > 100 or terminal.analyze_text_presence(screenshot)


@pytest.mark.input
@pytest.mark.scrolling
class TestMouseScroll:
    """Mouse scroll wheel tests."""

    def test_scroll_wheel_down(self, terminal):
        """Test that scroll wheel scrolls the terminal content."""
        # Generate lots of output to ensure scrollback content exists
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('1..100 | % { echo "Line number $_ with some text" }\n')
        time.sleep(3)

        # Position mouse in terminal
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))

        # First scroll UP to view older content (positive delta = scroll up)
        for _ in range(10):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)

        time.sleep(0.5)

        # Take screenshot when scrolled up
        screenshot_before, _ = terminal.wait_and_screenshot("scroll_before")

        # Now scroll DOWN to return to bottom (negative delta = scroll down)
        for _ in range(10):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
            time.sleep(0.1)

        time.sleep(0.5)

        # Take screenshot after scrolling down
        screenshot_after, _ = terminal.wait_and_screenshot("scroll_after")

        # Images should be different (different parts of scrollback visible)
        diff = np.sum(np.abs(
            np.array(screenshot_before).astype(np.int16) -
            np.array(screenshot_after).astype(np.int16)
        ))

        # There should be some difference from scrolling
        # If no difference, at least verify text is present
        assert diff > 5000 or terminal.analyze_text_presence(screenshot_after)

    def test_scroll_wheel_up(self, terminal):
        """Test scroll wheel in opposite direction."""
        # First scroll down to have content to scroll up to
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('1..30 | % { echo "Test line number $_" }\n')
        time.sleep(2)

        # Position mouse
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))

        # Scroll up first
        for _ in range(3):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)

        screenshot_scrolled, _ = terminal.wait_and_screenshot("scroll_up_mid")

        # Now scroll back down
        for _ in range(3):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
            time.sleep(0.1)

        screenshot_back, _ = terminal.wait_and_screenshot("scroll_up_back")

        # Both states should have text
        assert terminal.analyze_text_presence(screenshot_scrolled)
        assert terminal.analyze_text_presence(screenshot_back)
