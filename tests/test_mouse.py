#!/usr/bin/env python3
"""
Mouse input tests for TerminalDX12.

Tests mouse click, drag selection, and scroll wheel functionality.
"""

import pytest
import time
import win32api
import win32con


def click(terminal, offset_x: int = 0, offset_y: int = 0):
    """Click at center of terminal, with optional offset."""
    rect = terminal.get_client_rect_screen()
    x = (rect[0] + rect[2]) // 2 + offset_x
    y = (rect[1] + rect[3]) // 2 + offset_y
    win32api.SetCursorPos((x, y))
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    time.sleep(0.3)


def scroll_wheel(terminal, direction: int, count: int = 10):
    """Scroll mouse wheel."""
    rect = terminal.get_client_rect_screen()
    center = ((rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2)
    win32api.SetCursorPos(center)
    for _ in range(count):
        win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, direction * 120, 0)
        time.sleep(0.1)
    time.sleep(0.5)


@pytest.mark.input
class TestMouseBasic:
    """Basic mouse interaction tests."""

    def test_click_keeps_text(self, terminal):
        """Clicking preserves text visibility."""
        terminal.send_command("echo CLICK_TEST")
        click(terminal)
        terminal.assert_renders("mouse_click", "CLICK")

    def test_click_and_type(self, terminal):
        """Clicking gives focus for typing."""
        click(terminal)
        terminal.send_command("echo FOCUS_OK")
        terminal.assert_renders("mouse_focus", "FOCUS")


@pytest.mark.input
@pytest.mark.slow
class TestMouseSelection:
    """Mouse text selection tests."""

    def test_drag_selection(self, terminal):
        """Horizontal text selection via mouse drag."""
        terminal.send_command("echo ABCDEFGHIJKLMNOPQRSTUVWXYZ")

        rect = terminal.get_client_rect_screen()
        start_x, end_x = rect[0] + 50, rect[0] + 250
        y = rect[1] + 100

        win32api.SetCursorPos((start_x, y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        for x in range(start_x, end_x, 10):
            win32api.SetCursorPos((x, y))
            time.sleep(0.02)
        win32api.SetCursorPos((end_x, y))
        time.sleep(0.1)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.3)

        terminal.assert_renders("mouse_selection")


@pytest.mark.input
@pytest.mark.scrolling
class TestMouseScroll:
    """Mouse scroll wheel tests."""

    def test_scroll_wheel(self, terminal):
        """Scroll wheel scrolls terminal content."""
        terminal.send_command('1..50 | % { echo "LINE_$_" }', wait=2)

        scroll_wheel(terminal, 1)  # Up
        terminal.assert_renders("scroll_up")

        scroll_wheel(terminal, -1)  # Down
        terminal.assert_renders("scroll_down", "LINE")
