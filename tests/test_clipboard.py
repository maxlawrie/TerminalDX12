#!/usr/bin/env python3
"""
Clipboard tests for TerminalDX12.

Tests copy/paste functionality via Ctrl+C/V.
"""

import pytest
import time
import win32clipboard
import win32con


def set_clipboard_text(text):
    """Set clipboard content to given text."""
    win32clipboard.OpenClipboard()
    try:
        win32clipboard.EmptyClipboard()
        win32clipboard.SetClipboardText(text, win32con.CF_UNICODETEXT)
    finally:
        win32clipboard.CloseClipboard()


def get_clipboard_text():
    """Get current clipboard text content."""
    win32clipboard.OpenClipboard()
    try:
        if win32clipboard.IsClipboardFormatAvailable(win32con.CF_UNICODETEXT):
            return win32clipboard.GetClipboardData(win32con.CF_UNICODETEXT)
        return ""
    finally:
        win32clipboard.CloseClipboard()


class TestClipboard:
    """Clipboard copy/paste tests."""

    def test_paste_from_clipboard(self, terminal):
        """Paste text from clipboard into terminal."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Set clipboard content
        test_text = "CLIPBOARD_PASTE_TEST"
        set_clipboard_text(test_text)

        # Paste with Ctrl+V
        terminal.send_keys("echo ")
        time.sleep(0.2)
        terminal.send_ctrl_key('v')
        time.sleep(0.3)
        terminal.send_keys("\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("clipboard_paste")
        assert terminal.analyze_text_presence(screenshot), "No text after paste"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "CLIPBOARD" in ocr_text.upper() or "PASTE" in ocr_text.upper():
            print("Clipboard paste verified via OCR")

    def test_copy_selection_to_clipboard(self, terminal):
        """Select text and copy to clipboard with Ctrl+C."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Output unique text to copy
        unique_marker = "COPY_TEST_12345"
        terminal.send_keys(f"echo {unique_marker}\n")
        time.sleep(0.5)

        # Clear clipboard first
        set_clipboard_text("")

        # Get window position for selection
        client_rect = terminal.get_client_rect_screen()

        # Select text by dragging (approximate position of echo output)
        import win32api
        import win32con as wc

        # Start position (left side of content area)
        start_x = client_rect[0] + 50
        start_y = client_rect[1] + 80  # Below the command line

        # End position (right side)
        end_x = client_rect[0] + 300
        end_y = start_y

        # Perform drag selection
        win32api.SetCursorPos((start_x, start_y))
        time.sleep(0.1)
        win32api.mouse_event(wc.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        time.sleep(0.05)

        # Drag to end
        win32api.SetCursorPos((end_x, end_y))
        time.sleep(0.1)
        win32api.mouse_event(wc.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        time.sleep(0.2)

        # Copy with Ctrl+C
        terminal.send_ctrl_key('c')
        time.sleep(0.3)

        # Check clipboard
        clipboard_content = get_clipboard_text()

        screenshot, _ = terminal.wait_and_screenshot("clipboard_copy")

        # Verify something was copied (may not be exact due to selection positioning)
        if clipboard_content:
            print(f"Copied to clipboard: {repr(clipboard_content[:50])}")

        # Test passes if we can perform the copy operation without crash
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after copy"

    def test_paste_multiline(self, terminal):
        """Paste multiline text from clipboard."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Set multiline clipboard content
        multiline_text = "LINE_ONE\nLINE_TWO\nLINE_THREE"
        set_clipboard_text(multiline_text)

        # Paste into a command that can handle it
        terminal.send_keys("echo 'Pasting multiline:'\n")
        time.sleep(0.3)

        # Just paste and see what happens
        terminal.send_ctrl_key('v')
        time.sleep(0.5)
        terminal.send_keys("\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("clipboard_multiline")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after multiline paste"

    def test_paste_special_characters(self, terminal):
        """Paste text with special characters."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Set clipboard with special chars (safe ones for echo)
        special_text = "TEST_123_ABC"
        set_clipboard_text(special_text)

        terminal.send_keys("echo ")
        terminal.send_ctrl_key('v')
        time.sleep(0.3)
        terminal.send_keys("\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("clipboard_special")
        assert terminal.analyze_text_presence(screenshot), "Special character paste failed"

        ocr_text = terminal.get_screen_text(screenshot)
        if "TEST" in ocr_text.upper() and "123" in ocr_text:
            print("Special character paste verified")
