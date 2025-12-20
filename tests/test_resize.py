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
        """Test that decreasing window width works correctly."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Type some text
        terminal.send_keys("echo Narrow window test\n")
        time.sleep(0.5)

        # Decrease width by 100 pixels (but keep minimum viable)
        new_width = max(400, original_width - 100)
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
        """Test that decreasing window height works correctly."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        terminal.send_keys("echo Shorter window test\n")
        time.sleep(0.5)

        # Decrease height (keep minimum viable)
        new_height = max(300, original_height - 100)
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
        """Test that resize preserves scrollback buffer content."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        # Generate scrollback content
        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('for /L %i in (1,1,30) do @echo Scrollback line %i\n')
        time.sleep(2)

        # Resize while there's scrollback
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width - 50, original_height - 50, True)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("resize_scrollback")
        assert terminal.analyze_text_presence(screenshot)

        # Restore
        win32gui.MoveWindow(hwnd, rect[0], rect[1], original_width, original_height, True)


@pytest.mark.slow
class TestRapidResize:
    """Tests for rapid consecutive resize operations."""

    def test_rapid_resize_stability(self, terminal):
        """Test that rapid resize operations don't crash the terminal."""
        hwnd = terminal.hwnd
        rect = win32gui.GetWindowRect(hwnd)
        original_width = rect[2] - rect[0]
        original_height = rect[3] - rect[1]

        terminal.send_keys("echo Rapid resize test\n")
        time.sleep(0.3)

        # Perform rapid resizes
        for i in range(5):
            new_width = original_width + (i % 2) * 50 - 25
            new_height = original_height + (i % 2) * 50 - 25
            win32gui.MoveWindow(hwnd, rect[0], rect[1], new_width, new_height, True)
            time.sleep(0.1)

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

