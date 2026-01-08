#!/usr/bin/env python3
"""
Stress tests for TerminalDX12.

Tests terminal stability under extreme conditions.
"""

import pytest
import time
import win32gui

COLORS = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta"]


@pytest.mark.slow
class TestStress:
    """Stress tests for terminal stability."""

    def test_rapid_input_burst(self, terminal):
        """Rapidly type 1000 characters."""
        for _ in range(10):
            terminal.send_keys("X" * 100)
            time.sleep(0.05)
        terminal.send_command("")
        terminal.send_command("echo STRESS_OK")
        terminal.assert_renders("stress_rapid_input", "STRESS")

    def test_rapid_newlines(self, terminal):
        """Rapidly send 100 newlines to stress scrolling."""
        for i in range(100):
            terminal.send_keys(f"L{i}\n", delay=0.005)
        time.sleep(1)
        terminal.send_command("echo SCROLL_STRESS_OK")
        terminal.assert_renders("stress_newlines", "SCROLL")

    def test_rapid_clear_screen(self, terminal):
        """Rapidly clear screen 20 times."""
        for _ in range(20):
            terminal.send_keys("cls\n")
            time.sleep(0.1)
        terminal.send_command("echo CLEAR_STRESS_OK")
        terminal.assert_renders("stress_clear", "CLEAR")

    def test_long_line_output(self, terminal):
        """Test output of very long lines (500 chars)."""
        terminal.send_command(f"echo {'A' * 500}", wait=1)
        terminal.assert_renders("stress_long_line")

    def test_escape_sequence_flood(self, terminal):
        """Flood terminal with color escape sequences."""
        for i in range(50):
            color = COLORS[i % len(COLORS)]
            terminal.send_keys(f"Write-Host '{i}' -ForegroundColor {color} -NoNewline; ", delay=0.01)
        terminal.send_command("", wait=1)
        terminal.assert_renders("stress_colors")

    def test_window_still_valid(self, terminal):
        """Verify window handle still valid after stress tests."""
        assert terminal.hwnd is not None, "Window handle is None"
        assert win32gui.IsWindow(terminal.hwnd), "Window no longer valid"
        assert win32gui.IsWindowVisible(terminal.hwnd), "Window not visible"
