#!/usr/bin/env python3
"""
Text attribute tests for TerminalDX12.

Tests bold, underline, inverse video, and other text attributes.
"""

import pytest
from helpers import ScreenAnalyzer

# SGR attribute test cases: (code, name, marker_text)
BASIC_ATTRIBUTES = [
    ("1", "bold", "BOLD_TEXT_TEST"),
    ("2", "dim", "DIM_TEXT_TEST"),
    ("4", "underline", "UNDERLINE_TEST"),
    ("7", "inverse", "INVERSE_TEST"),
]

# Combined attribute test cases: (codes, name, marker_text)
COMBINED_ATTRIBUTES = [
    ("1;4", "bold_underline", "BOLD_UNDERLINE"),
    ("1;32", "bold_color", "BOLD_GREEN"),
    ("1;4;31", "all_combined", "ALL_ATTRS"),
    ("4;31", "underline_color", "RED_UNDERLINE"),
]


@pytest.mark.attributes
class TestBasicAttributes:
    """Tests for individual text attributes."""

    @pytest.mark.parametrize("code,name,marker", BASIC_ATTRIBUTES)
    def test_attribute_visible(self, terminal, code, name, marker):
        """Text attribute is rendered and visible."""
        terminal.send_command(f"$e = [char]27; Write-Host \"${{e}}[{code}m{marker}${{e}}[0m\"")
        terminal.assert_renders(f"attr_{name}", marker.split("_")[0])


@pytest.mark.attributes
class TestBoldAttribute:
    """Tests for bold text rendering specifics."""

    def test_bold_brighter_than_normal(self, terminal):
        """Bold text should be brighter than normal text."""
        terminal.send_command("$e = [char]27; Write-Host \"NORMAL_TEXT\"", wait=0.2)
        terminal.send_command("$e = [char]27; Write-Host \"${e}[1mBOLD_BRIGHT${e}[0m\"")

        screenshot = terminal.assert_renders("attr_bold_brightness")
        analyzer = ScreenAnalyzer()
        white_pixels = analyzer.find_white_pixels(screenshot)
        assert white_pixels > 50, "Expected bright pixels for bold text"


@pytest.mark.attributes
class TestInverseAttribute:
    """Tests for inverse video (reverse) text rendering."""

    def test_inverse_swaps_colors(self, terminal):
        """Inverse video swaps foreground and background."""
        terminal.send_command("$e = [char]27; Write-Host \"${e}[7m  REVERSED_BG  ${e}[0m\"")

        screenshot = terminal.assert_renders("attr_inverse_swap")
        analyzer = ScreenAnalyzer()
        white_pixels = analyzer.find_white_pixels(screenshot)
        assert white_pixels > 100, f"Expected inverse background, found {white_pixels} light pixels"


@pytest.mark.attributes
class TestCombinedAttributes:
    """Tests for combined text attributes."""

    @pytest.mark.parametrize("codes,name,marker", COMBINED_ATTRIBUTES)
    def test_combined_attributes(self, terminal, codes, name, marker):
        """Combined attributes render correctly."""
        terminal.send_command(f"$e = [char]27; Write-Host \"${{e}}[{codes}m{marker}${{e}}[0m\"")
        terminal.assert_renders(f"attr_{name}", marker.split("_")[0])


@pytest.mark.attributes
class TestAttributeReset:
    """Tests for attribute reset behavior."""

    def test_reset_clears_all_attributes(self, terminal):
        """ESC[0m resets all attributes."""
        terminal.send_command("$e = [char]27; Write-Host \"${e}[1;31mRED${e}[0m NORMAL\"")
        terminal.assert_renders("attr_reset", "NORMAL")

    def test_sgr_reset_each_attribute(self, terminal):
        """Individual SGR codes can reset specific attributes."""
        terminal.send_command("$e = [char]27; Write-Host \"${e}[1mBOLD${e}[22m_NORMAL\"")
        terminal.assert_renders("attr_sgr_reset", "NORMAL")
