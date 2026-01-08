#!/usr/bin/env python3
"""
Unicode visual rendering tests for TerminalDX12.

Tests CJK characters, emoji, box drawing, and other Unicode rendering.
"""

import pytest

# Basic Unicode test cases: (command, name, ocr_text)
BASIC_UNICODE = [
    ("echo 'ASCII_TEST_123'", "ascii", "ASCII"),
    ("Write-Host 'Caf\u00e9 ma\u00f1ana \u00fcber'", "latin_ext", None),
    ("Write-Host '$100 \u20ac50 \u00a330 \u00a51000'", "currency", None),
]

# Box drawing test cases
BOX_DRAWING = [
    ("\u250c\u2500\u2500\u2500\u2510", "simple_top"),
    ("\u2502 X \u2502", "simple_mid"),
    ("\u2514\u2500\u2500\u2500\u2518", "simple_bot"),
]

# CJK test cases: (chars, name)
CJK_CHARS = [
    ("\u4e2d\u6587\u6d4b\u8bd5", "chinese"),
    ("\u3042\u3044\u3046\u3048\u304a", "hiragana"),
    ("\ud55c\uae00 TEST", "hangul"),
]

# Special symbols: (chars, name)
SPECIAL_SYMBOLS = [
    ("\u00b1 \u00d7 \u00f7 \u221a \u221e \u2260 \u2264 \u2265", "math"),
    ("\u2190 \u2191 \u2192 \u2193 \u2194", "arrows"),
    ("\u03b1\u03b2\u03b3\u03b4\u03b5 = \u03c0", "greek"),
]


class TestBasicUnicode:
    """Tests for basic Unicode character rendering."""

    @pytest.mark.parametrize("command,name,ocr", BASIC_UNICODE)
    def test_basic_unicode(self, terminal, command, name, ocr):
        """Basic Unicode text renders correctly."""
        terminal.send_command(command)
        terminal.assert_renders(f"unicode_{name}", ocr)


class TestBoxDrawing:
    """Tests for box drawing characters."""

    def test_simple_box_renders(self, terminal):
        """Simple box drawing characters render."""
        for line, _ in BOX_DRAWING:
            terminal.send_command(f"Write-Host '{line}'", wait=0.2)
        terminal.assert_renders("unicode_box")

    def test_double_line_box_renders(self, terminal):
        """Double-line box drawing characters render."""
        terminal.send_command("Write-Host '\u2554\u2550\u2550\u2550\u2557'", wait=0.2)
        terminal.send_command("Write-Host '\u2551 Y \u2551'", wait=0.2)
        terminal.send_command("Write-Host '\u255a\u2550\u2550\u2550\u255d'")
        terminal.assert_renders("unicode_double_box")

    def test_block_elements_render(self, terminal):
        """Block element characters render."""
        terminal.send_command("Write-Host '\u2588\u2593\u2592\u2591 BLOCKS'")
        terminal.assert_renders("unicode_blocks", "BLOCKS")


class TestCJKCharacters:
    """Tests for CJK (Chinese, Japanese, Korean) character rendering."""

    @pytest.mark.parametrize("chars,name", CJK_CHARS)
    def test_cjk_renders(self, terminal, chars, name):
        """CJK characters render."""
        terminal.send_command(f"Write-Host '{chars}'")
        terminal.assert_renders(f"unicode_{name}")


class TestEmoji:
    """Tests for emoji rendering."""

    def test_basic_emoji_renders(self, terminal):
        """Basic emoji characters render."""
        terminal.send_command("Write-Host 'EMOJI: \u2764 \u2605 \u263a'")
        terminal.assert_renders("unicode_emoji_basic", "EMOJI")

    def test_emoji_with_text(self, terminal):
        """Emoji mixed with regular text."""
        terminal.send_command("Write-Host 'Hello \u2764 World \u2605'")
        terminal.assert_renders("unicode_emoji_mixed", "HELLO")


class TestSpecialSymbols:
    """Tests for special symbols and mathematical characters."""

    @pytest.mark.parametrize("chars,name", SPECIAL_SYMBOLS)
    def test_special_symbols(self, terminal, chars, name):
        """Special symbols render."""
        terminal.send_command(f"Write-Host '{chars}'")
        terminal.assert_renders(f"unicode_{name}")


class TestUnicodeEdgeCases:
    """Tests for Unicode edge cases."""

    def test_zero_width_joiner(self, terminal):
        """Zero-width joiner doesn't break rendering."""
        terminal.send_command("Write-Host 'TEST\u200dZWJ\u200dTEST'")
        terminal.assert_renders("unicode_zwj", "TEST")

    def test_combining_characters(self, terminal):
        """Combining characters render with base."""
        terminal.send_command("Write-Host 'cafe\u0301 test'")
        terminal.assert_renders("unicode_combining")

    def test_mixed_width_characters(self, terminal):
        """Mixed single and double-width characters."""
        terminal.send_command("Write-Host 'ABC\u4e2d\u6587DEF'")
        terminal.assert_renders("unicode_mixed_width")

    def test_long_unicode_line(self, terminal):
        """Long line with Unicode characters."""
        pattern = "\u2588\u2593\u2592\u2591" * 15
        terminal.send_command(f"Write-Host '{pattern}'")
        terminal.assert_renders("unicode_long_line")
