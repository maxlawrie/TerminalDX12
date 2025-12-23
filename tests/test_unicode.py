#!/usr/bin/env python3
"""
Unicode visual rendering tests for TerminalDX12.

Tests CJK characters, emoji, box drawing, and other Unicode rendering.
"""

import pytest
import time


class TestBasicUnicode:
    """Tests for basic Unicode character rendering."""

    def test_ascii_text_renders(self, terminal):
        """Basic ASCII text renders correctly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("echo 'ASCII_TEST_123'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_ascii")
        assert terminal.analyze_text_presence(screenshot), "ASCII text not visible"

        ocr_text = terminal.get_screen_text(screenshot)
        if "ASCII" in ocr_text.upper() or "123" in ocr_text:
            print("ASCII text verified via OCR")

    def test_extended_latin_renders(self, terminal):
        """Extended Latin characters (accents) render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Extended Latin: é, ñ, ü, etc.
        terminal.send_keys("Write-Host 'Caf\u00e9 ma\u00f1ana \u00fcber'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_latin_ext")
        assert terminal.analyze_text_presence(screenshot), "Extended Latin not visible"

    def test_currency_symbols_render(self, terminal):
        """Currency symbols render correctly."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Currency: $, €, £, ¥
        terminal.send_keys("Write-Host '$100 \u20ac50 \u00a330 \u00a51000'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_currency")
        assert terminal.analyze_text_presence(screenshot), "Currency symbols not visible"


class TestBoxDrawing:
    """Tests for box drawing characters."""

    def test_simple_box_renders(self, terminal):
        """Simple box drawing characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Box drawing: ┌─┐│└┘
        terminal.send_keys("Write-Host '\u250c\u2500\u2500\u2500\u2510'\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host '\u2502 X \u2502'\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host '\u2514\u2500\u2500\u2500\u2518'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_box")
        assert terminal.analyze_text_presence(screenshot), "Box drawing not visible"

    def test_double_line_box_renders(self, terminal):
        """Double-line box drawing characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Double line box: ╔═╗║╚╝
        terminal.send_keys("Write-Host '\u2554\u2550\u2550\u2550\u2557'\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host '\u2551 Y \u2551'\n")
        time.sleep(0.2)
        terminal.send_keys("Write-Host '\u255a\u2550\u2550\u2550\u255d'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_double_box")
        assert terminal.analyze_text_presence(screenshot), "Double box drawing not visible"

    def test_block_elements_render(self, terminal):
        """Block element characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Block elements: █▓▒░
        terminal.send_keys("Write-Host '\u2588\u2593\u2592\u2591 BLOCKS'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_blocks")
        assert terminal.analyze_text_presence(screenshot), "Block elements not visible"


class TestCJKCharacters:
    """Tests for CJK (Chinese, Japanese, Korean) character rendering."""

    def test_chinese_characters_render(self, terminal):
        """Chinese characters render (may be double-width)."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Chinese: 中文测试
        terminal.send_keys("Write-Host '\u4e2d\u6587\u6d4b\u8bd5'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_chinese")
        assert terminal.analyze_text_presence(screenshot), "Chinese characters not visible"

    def test_japanese_hiragana_renders(self, terminal):
        """Japanese Hiragana characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Hiragana: あいうえお
        terminal.send_keys("Write-Host '\u3042\u3044\u3046\u3048\u304a'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_hiragana")
        assert terminal.analyze_text_presence(screenshot), "Hiragana not visible"

    def test_korean_hangul_renders(self, terminal):
        """Korean Hangul characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Hangul: 한글
        terminal.send_keys("Write-Host '\ud55c\uae00 TEST'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_hangul")
        assert terminal.analyze_text_presence(screenshot), "Hangul not visible"


class TestEmoji:
    """Tests for emoji rendering."""

    def test_basic_emoji_renders(self, terminal):
        """Basic emoji characters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Simple emoji that may be in the font
        terminal.send_keys("Write-Host 'EMOJI: \u2764 \u2605 \u263a'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_emoji_basic")
        assert terminal.analyze_text_presence(screenshot), "Basic emoji not visible"

    def test_emoji_with_text(self, terminal):
        """Emoji mixed with regular text."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        terminal.send_keys("Write-Host 'Hello \u2764 World \u2605'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_emoji_mixed")
        assert terminal.analyze_text_presence(screenshot), "Emoji with text not visible"

        # Verify text portion via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        if "HELLO" in ocr_text.upper() or "WORLD" in ocr_text.upper():
            print("Emoji mixed text verified via OCR")


class TestSpecialSymbols:
    """Tests for special symbols and mathematical characters."""

    def test_math_symbols_render(self, terminal):
        """Mathematical symbols render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Math: ±×÷√∞≠≤≥
        terminal.send_keys("Write-Host '\u00b1 \u00d7 \u00f7 \u221a \u221e \u2260 \u2264 \u2265'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_math")
        assert terminal.analyze_text_presence(screenshot), "Math symbols not visible"

    def test_arrows_render(self, terminal):
        """Arrow symbols render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Arrows: ←↑→↓↔
        terminal.send_keys("Write-Host '\u2190 \u2191 \u2192 \u2193 \u2194'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_arrows")
        assert terminal.analyze_text_presence(screenshot), "Arrow symbols not visible"

    def test_greek_letters_render(self, terminal):
        """Greek letters render."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Greek: αβγδεπ
        terminal.send_keys("Write-Host '\u03b1\u03b2\u03b3\u03b4\u03b5 = \u03c0'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_greek")
        assert terminal.analyze_text_presence(screenshot), "Greek letters not visible"


class TestUnicodeEdgeCases:
    """Tests for Unicode edge cases."""

    def test_zero_width_joiner(self, terminal):
        """Zero-width joiner doesn't break rendering."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Text with ZWJ (shouldn't cause issues)
        terminal.send_keys("Write-Host 'TEST\u200dZWJ\u200dTEST'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_zwj")
        assert terminal.analyze_text_presence(screenshot), "ZWJ test failed"

    def test_combining_characters(self, terminal):
        """Combining characters render with base."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # e + combining acute = é
        terminal.send_keys("Write-Host 'cafe\u0301 test'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_combining")
        assert terminal.analyze_text_presence(screenshot), "Combining characters not visible"

    def test_mixed_width_characters(self, terminal):
        """Mixed single and double-width characters."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Mix ASCII with CJK
        terminal.send_keys("Write-Host 'ABC\u4e2d\u6587DEF'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_mixed_width")
        assert terminal.analyze_text_presence(screenshot), "Mixed width chars not visible"

    def test_long_unicode_line(self, terminal):
        """Long line with Unicode characters."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Repeat Unicode pattern
        pattern = "\u2588\u2593\u2592\u2591" * 15
        terminal.send_keys(f"Write-Host '{pattern}'\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("unicode_long_line")
        assert terminal.analyze_text_presence(screenshot), "Long Unicode line not visible"
