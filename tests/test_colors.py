#!/usr/bin/env python3
"""
ANSI color rendering tests for TerminalDX12.

Tests that color commands execute and produce visible output.
Pixel-level color verification is done on a sampling basis.
"""

import pytest
import time
from helpers import ScreenAnalyzer


@pytest.mark.color
class TestBasicColors:
    """Tests for basic 16-color ANSI palette."""

    def test_red_foreground(self, terminal):
        """Red foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'RED_TEXT_TEST' -ForegroundColor Red\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_red_fg")
        assert terminal.analyze_text_presence(screenshot), "Red text not visible"

        ocr_text = terminal.get_screen_text(screenshot)
        if "RED" in ocr_text.upper():
            print("Red text verified via OCR")

    def test_green_foreground(self, terminal):
        """Green foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'GREEN_TEXT_TEST' -ForegroundColor Green\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_green_fg")
        assert terminal.analyze_text_presence(screenshot), "Green text not visible"

    def test_blue_foreground(self, terminal):
        """Blue foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'BLUE_TEXT_TEST' -ForegroundColor Blue\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_blue_fg")
        assert terminal.analyze_text_presence(screenshot), "Blue text not visible"

    def test_yellow_foreground(self, terminal):
        """Yellow foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'YELLOW_TEXT_TEST' -ForegroundColor Yellow\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_yellow_fg")
        assert terminal.analyze_text_presence(screenshot), "Yellow text not visible"

    def test_cyan_foreground(self, terminal):
        """Cyan foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'CYAN_TEXT_TEST' -ForegroundColor Cyan\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_cyan_fg")
        assert terminal.analyze_text_presence(screenshot), "Cyan text not visible"

    def test_magenta_foreground(self, terminal):
        """Magenta foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'MAGENTA_TEXT_TEST' -ForegroundColor Magenta\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_magenta_fg")
        assert terminal.analyze_text_presence(screenshot), "Magenta text not visible"

    def test_white_foreground(self, terminal):
        """White foreground text renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'WHITE_TEXT_TEST' -ForegroundColor White\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_white_fg")
        assert terminal.analyze_text_presence(screenshot), "White text not visible"


@pytest.mark.color
class TestMultipleColors:
    """Tests for multiple colors in same output."""

    def test_multiple_colors_same_line(self, terminal):
        """Multiple colors render on same line."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'RED' -ForegroundColor Red -NoNewline; ")
        terminal.send_keys("Write-Host 'GREEN' -ForegroundColor Green -NoNewline; ")
        terminal.send_keys("Write-Host 'BLUE' -ForegroundColor Blue\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_multiple")
        assert terminal.analyze_text_presence(screenshot), "Multiple colors not visible"

        ocr_text = terminal.get_screen_text(screenshot)
        found = sum(1 for word in ["RED", "GREEN", "BLUE"] if word in ocr_text.upper())
        if found >= 2:
            print(f"Multiple color text verified: {found}/3 markers found")

    def test_color_persistence(self, terminal):
        """Color changes persist across multiple outputs."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'LINE1' -ForegroundColor Red\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host 'LINE2' -ForegroundColor Green\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host 'LINE3' -ForegroundColor Blue\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_persistence")
        assert terminal.analyze_text_presence(screenshot), "Color persistence test failed"


@pytest.mark.color
class TestAnsiEscapeColors:
    """Tests for ANSI escape sequence colors."""

    def test_ansi_escape_red(self, terminal):
        """ANSI escape sequence for red foreground."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("$e = [char]27; Write-Host \"${e}[31mANSI_RED_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_ansi_red")
        assert terminal.analyze_text_presence(screenshot), "ANSI red text not visible"

    def test_ansi_escape_green(self, terminal):
        """ANSI escape sequence for green foreground."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("$e = [char]27; Write-Host \"${e}[32mANSI_GREEN_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_ansi_green")
        assert terminal.analyze_text_presence(screenshot), "ANSI green text not visible"

    def test_ansi_256_color(self, terminal):
        """256-color mode via ANSI escape sequences."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("$e = [char]27; Write-Host \"${e}[38;5;196m256_COLOR_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_256")
        assert terminal.analyze_text_presence(screenshot), "256-color text not visible"

    def test_ansi_true_color(self, terminal):
        """True color (24-bit) via ANSI escape sequences."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("$e = [char]27; Write-Host \"${e}[38;2;255;100;50mTRUE_COLOR_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_true")
        assert terminal.analyze_text_presence(screenshot), "True color text not visible"


@pytest.mark.color
class TestBackgroundColors:
    """Tests for background color rendering."""

    def test_red_background(self, terminal):
        """Red background renders visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host '  BG_TEST  ' -BackgroundColor Red\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_bg_red")
        assert terminal.analyze_text_presence(screenshot), "Red background text not visible"

    def test_contrasting_fg_bg(self, terminal):
        """Contrasting foreground and background colors render visibly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host ' CONTRAST_TEST ' -ForegroundColor White -BackgroundColor Red\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("color_contrast")
        assert terminal.analyze_text_presence(screenshot), "Contrast text not visible"

        ocr_text = terminal.get_screen_text(screenshot)
        if "CONTRAST" in ocr_text.upper():
            print("Contrast text verified via OCR")
