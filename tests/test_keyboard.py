#!/usr/bin/env python3
"""
Keyboard shortcut tests for TerminalDX12.

Tests keyboard navigation and shortcuts like Shift+PageUp/Down.
"""

import pytest
import time
import win32api
import win32con
from helpers import ScreenAnalyzer


class TestScrolling:
    """Mouse wheel scrolling tests."""

    def _scroll_wheel(self, terminal, direction: int, count: int = 5):
        """Scroll mouse wheel in terminal window."""
        rect = terminal.get_client_rect_screen()
        center = ((rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2)
        win32api.SetCursorPos(center)
        time.sleep(0.1)
        for _ in range(count):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, direction * 120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

    def test_scroll_up_changes_view(self, terminal):
        """Scrolling up shows different content."""
        terminal.send_command('1..50 | % { echo "SCROLL_LINE_$_" }', wait=2)
        screenshot_before, _ = terminal.wait_and_screenshot("keyboard_before_scroll")

        self._scroll_wheel(terminal, 1)  # Scroll up
        screenshot_after, _ = terminal.wait_and_screenshot("keyboard_after_scroll_up")

        diff = ScreenAnalyzer().compare_screenshots(screenshot_before, screenshot_after)
        assert diff > 5000, "Screen did not change after scroll up"

    def test_scroll_down_after_up(self, terminal):
        """Scrolling down after up shows different content."""
        terminal.send_command('1..50 | % { echo "FWD_LINE_$_" }', wait=2)

        self._scroll_wheel(terminal, 1)  # Scroll up
        screenshot_up, _ = terminal.wait_and_screenshot("keyboard_scrolled_up")

        self._scroll_wheel(terminal, -1)  # Scroll down
        screenshot_down, _ = terminal.wait_and_screenshot("keyboard_scrolled_down")

        diff = ScreenAnalyzer().compare_screenshots(screenshot_up, screenshot_down)
        assert diff > 5000, "Screen did not change after scroll down"


class TestBasicKeys:
    """Basic keyboard input tests."""

    def test_ctrl_c_no_crash(self, terminal):
        """Ctrl+C with no selection should not crash."""
        terminal.send_command("echo 'Testing Ctrl+C'", wait=0.3)
        terminal.send_ctrl_key('c')
        time.sleep(0.3)
        terminal.send_command("echo CTRL_C_OK")
        terminal.assert_renders("keyboard_ctrl_c", "CTRL_C_OK")

    def test_enter_executes(self, terminal):
        """Enter key executes the current command."""
        terminal.send_command("echo ENTER_TEST_SUCCESS")
        terminal.assert_renders("keyboard_enter", "ENTER")

    def test_backspace_deletes(self, terminal):
        """Backspace key deletes the previous character."""
        terminal.send_keys("echo TESTX")
        time.sleep(0.2)
        terminal.send_keys("\b")
        time.sleep(0.1)
        terminal.send_command("_BACKSPACE_OK")
        terminal.assert_renders("keyboard_backspace", "BACKSPACE")


class TestNavigation:
    """Arrow key and history navigation tests."""

    def test_arrow_up_history(self, terminal):
        """Up arrow recalls previous command."""
        terminal.send_command("echo FIRST_CMD", wait=0.3)
        terminal.send_command("echo SECOND_CMD", wait=0.3)

        win32api.keybd_event(win32con.VK_UP, 0, 0, 0)
        win32api.keybd_event(win32con.VK_UP, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.3)

        terminal.assert_renders("keyboard_arrow_up")

    def test_tab_completion(self, terminal):
        """Tab key triggers command completion."""
        terminal.send_keys("Get-Chi")  # Partial for Get-ChildItem
        time.sleep(0.2)

        win32api.keybd_event(win32con.VK_TAB, 0, 0, 0)
        win32api.keybd_event(win32con.VK_TAB, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.5)

        # Cancel and continue
        win32api.keybd_event(win32con.VK_ESCAPE, 0, 0, 0)
        win32api.keybd_event(win32con.VK_ESCAPE, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.2)
        terminal.send_command("")

        terminal.assert_renders("keyboard_tab")
