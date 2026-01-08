#!/usr/bin/env python3
"""
ANSI color rendering tests for TerminalDX12.

Tests that color commands execute and produce visible output.
"""

import pytest

# Basic 16-color palette test cases
FOREGROUND_COLORS = ["Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White"]
ANSI_COLORS = [
    ("31", "red"),
    ("32", "green"),
    ("33", "yellow"),
    ("34", "blue"),
    ("35", "magenta"),
    ("36", "cyan"),
]


@pytest.mark.color
class TestBasicColors:
    """Tests for basic 16-color ANSI palette."""

    @pytest.mark.parametrize("color", FOREGROUND_COLORS)
    def test_foreground_color(self, terminal, color):
        """Foreground color text renders visibly."""
        terminal.send_command(f"Write-Host '{color.upper()}_TEST' -ForegroundColor {color}")
        terminal.assert_renders(f"color_{color.lower()}_fg", color.upper())


@pytest.mark.color
class TestMultipleColors:
    """Tests for multiple colors in same output."""

    def test_multiple_colors_same_line(self, terminal):
        """Multiple colors render on same line."""
        terminal.send_keys("Write-Host 'RED' -ForegroundColor Red -NoNewline; ")
        terminal.send_keys("Write-Host 'GREEN' -ForegroundColor Green -NoNewline; ")
        terminal.send_command("Write-Host 'BLUE' -ForegroundColor Blue")
        terminal.assert_renders("color_multiple", "RED")

    def test_color_persistence(self, terminal):
        """Color changes persist across multiple outputs."""
        for i, color in enumerate(["Red", "Green", "Blue"], 1):
            terminal.send_command(f"Write-Host 'LINE{i}' -ForegroundColor {color}", wait=0.2)
        terminal.assert_renders("color_persistence")


@pytest.mark.color
class TestAnsiEscapeColors:
    """Tests for ANSI escape sequence colors."""

    @pytest.mark.parametrize("code,name", ANSI_COLORS)
    def test_ansi_escape_color(self, terminal, code, name):
        """ANSI escape sequence colors render."""
        terminal.send_command(f"$e = [char]27; Write-Host \"${{e}}[{code}mANSI_{name.upper()}_TEST${{e}}[0m\"")
        terminal.assert_renders(f"color_ansi_{name}", f"ANSI_{name.upper()}")

    def test_ansi_256_color(self, terminal):
        """256-color mode via ANSI escape sequences."""
        terminal.send_command("$e = [char]27; Write-Host \"${e}[38;5;196m256_COLOR_TEST${e}[0m\"")
        terminal.assert_renders("color_256", "256_COLOR")

    def test_ansi_true_color(self, terminal):
        """True color (24-bit) via ANSI escape sequences."""
        terminal.send_command("$e = [char]27; Write-Host \"${e}[38;2;255;100;50mTRUE_COLOR_TEST${e}[0m\"")
        terminal.assert_renders("color_true", "TRUE_COLOR")


@pytest.mark.color
class TestBackgroundColors:
    """Tests for background color rendering."""

    def test_red_background(self, terminal):
        """Red background renders visibly."""
        terminal.send_command("Write-Host '  BG_TEST  ' -BackgroundColor Red")
        terminal.assert_renders("color_bg_red", "BG_TEST")

    def test_contrasting_fg_bg(self, terminal):
        """Contrasting foreground and background colors render visibly."""
        terminal.send_command("Write-Host ' CONTRAST_TEST ' -ForegroundColor White -BackgroundColor Red")
        terminal.assert_renders("color_contrast", "CONTRAST")
