#!/usr/bin/env python3
"""
Window resize and text reflow tests for TerminalDX12.

Tests resize behavior and stability.
"""

import pytest
import time
import win32gui


@pytest.mark.slow
class TestResize:
    """Window resize and text reflow tests."""

    def _get_dimensions(self, terminal):
        """Get window rect and quarter dimensions."""
        rect = win32gui.GetWindowRect(terminal.hwnd)
        width, height = rect[2] - rect[0], rect[3] - rect[1]
        quarter = (max(300, width // 4), max(200, height // 4))
        return rect, (width, height), quarter

    def _resize(self, hwnd, rect, width, height):
        """Resize window and wait for settle."""
        win32gui.MoveWindow(hwnd, rect[0], rect[1], width, height, True)
        time.sleep(0.5)

    def test_resize_to_quarter(self, terminal):
        """Test resizing window to 1/4 size with long text that must wrap."""
        rect, (orig_w, orig_h), (qtr_w, qtr_h) = self._get_dimensions(terminal)

        # Long line that will wrap
        terminal.send_command(f"echo RESIZE{'X' * 70}END")

        self._resize(terminal.hwnd, rect, qtr_w, qtr_h)
        terminal.assert_renders("resize_quarter")

        self._resize(terminal.hwnd, rect, orig_w, orig_h)
        terminal.assert_renders("resize_restored", "RESIZE")

    def test_resize_with_scrollback(self, terminal):
        """Test that resize preserves scrollback content."""
        rect, (orig_w, orig_h), (qtr_w, qtr_h) = self._get_dimensions(terminal)

        terminal.send_command('1..20 | % { echo "SCROLL_$_" }', wait=2)

        self._resize(terminal.hwnd, rect, qtr_w, qtr_h)
        terminal.assert_renders("scrollback_resized")

        self._resize(terminal.hwnd, rect, orig_w, orig_h)

    def test_rapid_resize_stability(self, terminal):
        """Test that rapid resize doesn't crash."""
        rect, (orig_w, orig_h), (qtr_w, qtr_h) = self._get_dimensions(terminal)

        terminal.send_command("echo RAPID_START", wait=0.3)

        # Rapid resize cycles
        for i in range(5):
            w, h = (qtr_w, qtr_h) if i % 2 == 0 else (orig_w, orig_h)
            win32gui.MoveWindow(terminal.hwnd, rect[0], rect[1], w, h, True)
            time.sleep(0.15)

        time.sleep(0.5)
        self._resize(terminal.hwnd, rect, orig_w, orig_h)

        terminal.send_command("echo RAPID_END")
        terminal.assert_renders("rapid_resize", "RAPID")
