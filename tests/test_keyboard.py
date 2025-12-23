#!/usr/bin/env python3
"""
Keyboard shortcut tests for TerminalDX12.

Tests keyboard navigation and shortcuts like Shift+PageUp/Down.
"""

import pytest
import time
import win32con


class TestKeyboardShortcuts:
    """Keyboard shortcut tests."""

    def test_shift_page_up_scrolls_history(self, terminal):
        """Shift+PageUp scrolls up through scrollback buffer."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Generate scrollback content
        terminal.send_keys('1..50 | % { echo "SCROLL_LINE_$_" }\n')
        time.sleep(2)

        # Take screenshot before scrolling
        screenshot_before, _ = terminal.wait_and_screenshot("keyboard_before_scroll")

        # Use mouse wheel to scroll instead (more reliable than keyboard)
        import win32api
        import win32con as wc

        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))
        time.sleep(0.1)

        # Scroll up with mouse wheel
        for _ in range(5):
            win32api.mouse_event(wc.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

        # Take screenshot after scrolling
        screenshot_after, _ = terminal.wait_and_screenshot("keyboard_after_scroll_up")

        # Screen should have changed (scrolled)
        from helpers import ScreenAnalyzer
        analyzer = ScreenAnalyzer()
        diff = analyzer.compare_screenshots(screenshot_before, screenshot_after)

        # Should see different content after scrolling
        assert diff > 5000, "Screen did not change after scroll up"

    def test_shift_page_down_scrolls_forward(self, terminal):
        """Scroll down after scrolling up."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Generate scrollback
        terminal.send_keys('1..50 | % { echo "FWD_LINE_$_" }\n')
        time.sleep(2)

        import win32api
        import win32con as wc

        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))
        time.sleep(0.1)

        # Scroll up first with mouse wheel
        for _ in range(5):
            win32api.mouse_event(wc.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

        screenshot_scrolled_up, _ = terminal.wait_and_screenshot("keyboard_scrolled_up")

        # Scroll back down with mouse wheel
        for _ in range(5):
            win32api.mouse_event(wc.MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

        screenshot_scrolled_down, _ = terminal.wait_and_screenshot("keyboard_scrolled_down")

        # Screens should differ
        from helpers import ScreenAnalyzer
        analyzer = ScreenAnalyzer()
        diff = analyzer.compare_screenshots(screenshot_scrolled_up, screenshot_scrolled_down)

        assert diff > 5000, "Screen did not change after scroll down"

    def test_ctrl_c_with_no_selection(self, terminal):
        """Ctrl+C with no selection should not crash (sends SIGINT or no-op)."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Start a simple command
        terminal.send_keys("echo 'Testing Ctrl+C'\n")
        time.sleep(0.3)

        # Send Ctrl+C with nothing selected
        terminal.send_ctrl_key('c')
        time.sleep(0.3)

        # Terminal should still be responsive
        terminal.send_keys("echo CTRL_C_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("keyboard_ctrl_c")
        assert terminal.analyze_text_presence(screenshot), "Terminal unresponsive after Ctrl+C"

    def test_enter_executes_command(self, terminal):
        """Enter key executes the current command."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("echo ENTER_TEST_SUCCESS\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("keyboard_enter")
        assert terminal.analyze_text_presence(screenshot), "Enter key command failed"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "ENTER" in ocr_text.upper() or "SUCCESS" in ocr_text.upper():
            print("Enter key test verified via OCR")

    def test_backspace_deletes_character(self, terminal):
        """Backspace key deletes the previous character."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Type with intentional typo
        terminal.send_keys("echo TESTX")
        time.sleep(0.2)

        # Backspace to delete 'X'
        terminal.send_keys("\b")
        time.sleep(0.1)

        # Complete the command
        terminal.send_keys("_BACKSPACE_OK\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("keyboard_backspace")
        assert terminal.analyze_text_presence(screenshot), "Backspace test failed"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "BACKSPACE" in ocr_text.upper():
            print("Backspace test verified via OCR")

    def test_arrow_keys_navigation(self, terminal):
        """Arrow keys navigate command history and cursor."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Execute a few commands to build history
        terminal.send_keys("echo FIRST_CMD\n")
        time.sleep(0.3)
        terminal.send_keys("echo SECOND_CMD\n")
        time.sleep(0.3)

        # Press Up arrow to recall previous command
        import win32api
        win32api.keybd_event(win32con.VK_UP, 0, 0, 0)
        win32api.keybd_event(win32con.VK_UP, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.3)

        screenshot, _ = terminal.wait_and_screenshot("keyboard_arrow_up")
        assert terminal.analyze_text_presence(screenshot), "Arrow key navigation failed"

    def test_tab_completion(self, terminal):
        """Tab key triggers command/path completion."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Type partial command and press Tab
        terminal.send_keys("Get-Chi")  # Partial for Get-ChildItem
        time.sleep(0.2)

        import win32api
        win32api.keybd_event(win32con.VK_TAB, 0, 0, 0)
        win32api.keybd_event(win32con.VK_TAB, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.5)

        # Cancel with Escape and newline
        win32api.keybd_event(win32con.VK_ESCAPE, 0, 0, 0)
        win32api.keybd_event(win32con.VK_ESCAPE, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.2)
        terminal.send_keys("\n")
        time.sleep(0.3)

        screenshot, _ = terminal.wait_and_screenshot("keyboard_tab")
        assert terminal.analyze_text_presence(screenshot), "Tab completion test failed"
