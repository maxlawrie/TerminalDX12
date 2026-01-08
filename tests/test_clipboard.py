#!/usr/bin/env python3
"""
Clipboard tests for TerminalDX12.

Tests copy/paste functionality via Ctrl+C/V.
"""

import pytest
import time
import win32api
import win32clipboard
import win32con


def set_clipboard_text(text: str) -> None:
    """Set clipboard content to given text."""
    win32clipboard.OpenClipboard()
    try:
        win32clipboard.EmptyClipboard()
        win32clipboard.SetClipboardText(text, win32con.CF_UNICODETEXT)
    finally:
        win32clipboard.CloseClipboard()


def get_clipboard_text() -> str:
    """Get current clipboard text content."""
    win32clipboard.OpenClipboard()
    try:
        if win32clipboard.IsClipboardFormatAvailable(win32con.CF_UNICODETEXT):
            return win32clipboard.GetClipboardData(win32con.CF_UNICODETEXT)
        return ""
    finally:
        win32clipboard.CloseClipboard()


class TestPaste:
    """Clipboard paste tests."""

    def test_paste_simple(self, terminal):
        """Paste text from clipboard into terminal."""
        set_clipboard_text("CLIPBOARD_PASTE_TEST")
        terminal.send_keys("echo ")
        time.sleep(0.2)
        terminal.send_ctrl_key('v')
        time.sleep(0.3)
        terminal.send_command("")
        terminal.assert_renders("clipboard_paste", "CLIPBOARD")

    def test_paste_multiline(self, terminal):
        """Paste multiline text from clipboard."""
        set_clipboard_text("LINE_ONE\nLINE_TWO\nLINE_THREE")
        terminal.send_command("echo 'Pasting multiline:'", wait=0.3)
        terminal.send_ctrl_key('v')
        time.sleep(0.5)
        terminal.send_command("")
        terminal.assert_renders("clipboard_multiline")

    def test_paste_special_chars(self, terminal):
        """Paste text with special characters."""
        set_clipboard_text("TEST_123_ABC")
        terminal.send_keys("echo ")
        terminal.send_ctrl_key('v')
        time.sleep(0.3)
        terminal.send_command("")
        terminal.assert_renders("clipboard_special", "TEST")


class TestCopy:
    """Clipboard copy tests."""

    def test_copy_selection(self, terminal):
        """Select text and copy to clipboard with Ctrl+C."""
        unique_marker = "COPY_TEST_12345"
        terminal.send_command(f"echo {unique_marker}")

        set_clipboard_text("")  # Clear clipboard

        # Select text by dragging
        rect = terminal.get_client_rect_screen()
        start = (rect[0] + 50, rect[1] + 80)
        end = (rect[0] + 300, rect[1] + 80)

        win32api.SetCursorPos(start)
        time.sleep(0.1)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        time.sleep(0.05)
        win32api.SetCursorPos(end)
        time.sleep(0.1)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.2)

        terminal.send_ctrl_key('c')
        time.sleep(0.3)

        clipboard = get_clipboard_text()
        if clipboard:
            print(f"Copied to clipboard: {repr(clipboard[:50])}")

        terminal.assert_renders("clipboard_copy")
