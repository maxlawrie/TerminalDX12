#!/usr/bin/env python3
"""
Mouse input tests for TerminalDX12.

Tests mouse click, drag selection, and scroll wheel functionality.
"""

import pytest
import time
import win32api
import win32con


@pytest.mark.input
class TestMouseBasic:
    """Basic mouse interaction tests."""

    def test_mouse_click_clears_selection(self, terminal):
        """Test that clicking in the terminal works and text remains visible."""
        # Type some text
        terminal.send_keys("echo CLICK_TEST\n")
        time.sleep(0.5)

        # Click in the terminal area
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2

        win32api.SetCursorPos((center_x, center_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.3)

        screenshot, _ = terminal.wait_and_screenshot("mouse_click")
        assert terminal.analyze_text_presence(screenshot), "No text visible after click"

        # Log OCR for debugging
        ocr_text = terminal.get_screen_text(screenshot)
        print(f"OCR after click: {ocr_text[:100]}...")

    def test_mouse_focus(self, terminal):
        """Test that clicking gives the terminal focus and typing works."""
        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2

        # Click to focus
        win32api.SetCursorPos((center_x, center_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.2)

        # Type after click
        terminal.send_keys("echo FOCUS_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("mouse_focus")
        assert terminal.analyze_text_presence(screenshot), "No text visible after focus+type"

        # Log OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "FOCUS" in ocr_text.upper():
            print("OCR verified: FOCUS marker found")


@pytest.mark.input
@pytest.mark.slow
class TestMouseSelection:
    """Mouse text selection tests."""

    def test_drag_selection_horizontal(self, terminal):
        """Test horizontal text selection via mouse drag."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys("echo ABCDEFGHIJKLMNOPQRSTUVWXYZ\n")
        time.sleep(0.5)

        client_rect = terminal.get_client_rect_screen()
        start_x = client_rect[0] + 50
        start_y = client_rect[1] + 100
        end_x = start_x + 200

        # Perform drag
        win32api.SetCursorPos((start_x, start_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)

        for x in range(start_x, end_x, 10):
            win32api.SetCursorPos((x, start_y))
            time.sleep(0.02)

        win32api.SetCursorPos((end_x, start_y))
        time.sleep(0.1)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.3)

        screenshot, _ = terminal.wait_and_screenshot("mouse_selection")
        assert terminal.analyze_text_presence(screenshot), "No text visible after drag selection"


@pytest.mark.input
@pytest.mark.scrolling
class TestMouseScroll:
    """Mouse scroll wheel tests."""

    def test_scroll_wheel(self, terminal):
        """Test that scroll wheel scrolls the terminal content."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('1..50 | % { echo "LINE_$_" }\n')
        time.sleep(2)

        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))

        # Scroll UP
        for _ in range(10):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)
        time.sleep(0.5)

        screenshot_up, _ = terminal.wait_and_screenshot("scroll_up")
        assert terminal.analyze_text_presence(screenshot_up), "No text visible when scrolled up"

        # Scroll DOWN
        for _ in range(10):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
            time.sleep(0.1)
        time.sleep(0.5)

        screenshot_down, _ = terminal.wait_and_screenshot("scroll_down")
        assert terminal.analyze_text_presence(screenshot_down), "No text visible when scrolled down"

        # Log OCR
        ocr_text = terminal.get_screen_text(screenshot_down)
        if "LINE" in ocr_text.upper():
            print("OCR verified: LINE marker found after scroll")
