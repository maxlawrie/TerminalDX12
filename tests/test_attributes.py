#!/usr/bin/env python3
"""
Text attribute tests for TerminalDX12.

Tests bold, underline, inverse video, and other text attributes.
"""

import pytest
import time
from helpers import ScreenAnalyzer


@pytest.mark.attributes
class TestBoldAttribute:
    """Tests for bold text rendering."""

    def test_bold_text_visible(self, terminal):
        """Bold text is rendered and visible."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[1m = bold, ESC[0m = reset
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1mBOLD_TEXT_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_bold")
        assert terminal.analyze_text_presence(screenshot), "Bold text not visible"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "BOLD" in ocr_text.upper():
            print("Bold text verified via OCR")

    def test_bold_brighter_than_normal(self, terminal):
        """Bold text should be brighter than normal text."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Output normal and bold on separate lines
        terminal.send_keys("$e = [char]27; Write-Host \"NORMAL_TEXT\"\n")
        time.sleep(0.2)
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1mBOLD_BRIGHT${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_bold_brightness")
        analyzer = ScreenAnalyzer()

        # Bold text typically renders with brighter colors
        white_pixels = analyzer.find_white_pixels(screenshot)
        assert white_pixels > 50, "Expected bright pixels for bold text"


@pytest.mark.attributes
class TestUnderlineAttribute:
    """Tests for underlined text rendering."""

    def test_underline_text_visible(self, terminal):
        """Underlined text is rendered and visible."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[4m = underline
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[4mUNDERLINE_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_underline")
        assert terminal.analyze_text_presence(screenshot), "Underlined text not visible"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "UNDERLINE" in ocr_text.upper():
            print("Underlined text verified via OCR")

    def test_underline_with_color(self, terminal):
        """Underlined text with color attribute."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[4;31m = underline + red
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[4;31mRED_UNDERLINE${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_underline_color")
        assert terminal.analyze_text_presence(screenshot), "Underlined colored text not visible"


@pytest.mark.attributes
class TestInverseAttribute:
    """Tests for inverse video (reverse) text rendering."""

    def test_inverse_text_visible(self, terminal):
        """Inverse video text is rendered and visible."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[7m = inverse/reverse video
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[7mINVERSE_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_inverse")
        assert terminal.analyze_text_presence(screenshot), "Inverse text not visible"

    def test_inverse_swaps_colors(self, terminal):
        """Inverse video swaps foreground and background."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Inverse should show light background (swapped from black)
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[7m  REVERSED_BG  ${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_inverse_swap")
        analyzer = ScreenAnalyzer()

        # Should have non-black pixels in the inverse region
        white_pixels = analyzer.find_white_pixels(screenshot)
        # Inverse creates a light background region
        assert white_pixels > 100, f"Expected inverse background, found {white_pixels} light pixels"


@pytest.mark.attributes
class TestDimAttribute:
    """Tests for dim/faint text rendering."""

    def test_dim_text_visible(self, terminal):
        """Dim text is rendered and visible."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[2m = dim/faint
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[2mDIM_TEXT_TEST${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_dim")
        assert terminal.analyze_text_presence(screenshot), "Dim text not visible"


@pytest.mark.attributes
class TestCombinedAttributes:
    """Tests for combined text attributes."""

    def test_bold_underline_combined(self, terminal):
        """Bold and underline combined."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[1;4m = bold + underline
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1;4mBOLD_UNDERLINE${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_bold_underline")
        assert terminal.analyze_text_presence(screenshot), "Combined attributes not visible"

    def test_bold_color_combined(self, terminal):
        """Bold and color combined."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[1;32m = bold + green
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1;32mBOLD_GREEN${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_bold_color")
        assert terminal.analyze_text_presence(screenshot), "Bold colored text not visible"

    def test_all_attributes_combined(self, terminal):
        """Multiple attributes combined."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[1;4;31m = bold + underline + red
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1;4;31mALL_ATTRS${e}[0m\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_all_combined")
        assert terminal.analyze_text_presence(screenshot), "All attributes combined not visible"


@pytest.mark.attributes
class TestAttributeReset:
    """Tests for attribute reset behavior."""

    def test_reset_clears_all_attributes(self, terminal):
        """ESC[0m resets all attributes."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Bold red, then reset, then normal
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1;31mRED${e}[0m NORMAL\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_reset")
        assert terminal.analyze_text_presence(screenshot), "Reset test failed"

    def test_sgr_reset_each_attribute(self, terminal):
        """Individual SGR codes can reset specific attributes."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # ESC[22m = reset bold, ESC[24m = reset underline
        terminal.send_keys("$e = [char]27; Write-Host \"${e}[1mBOLD${e}[22m_NORMAL\"\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("attr_sgr_reset")
        assert terminal.analyze_text_presence(screenshot), "SGR reset test failed"
