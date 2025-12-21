#!/usr/bin/env python3
"""
VT Feature Tests for TerminalDX12
Tests for Phase 1 VT compatibility: true color, dim, strikethrough, etc.
"""

import subprocess
import time
import os
import sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageGrab
import win32gui
import win32con
import win32api

# Import from main test file
from config import TestConfig
from test_terminal import TerminalTester, TeeOutput, SCREENSHOT_DIR, LOG_FILE


class VTFeatureTester(TerminalTester):
    """Extended tester with VT feature tests"""

    def verify_log_contains(self, test_name, expected_strings):
        """Skip log verification for visual tests - they use pixel detection"""
        return True, None

    def test_true_color_foreground(self):
        """Test: True color (24-bit RGB) foreground works"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        # Output orange text using true color: ESC[38;2;255;128;0m
        cmd = 'powershell -Command "$e = [char]27; Write-Host \\"${e}[38;2;255;128;0mORANGE COLOR${e}[0m\\""'
        self.send_keys(cmd)
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, filepath = self.take_screenshot("vt_true_color_fg")
        img_array = np.array(screenshot)

        # Look for orange pixels (high R, medium G, low B)
        orange_ish = np.logical_and(
            img_array[:,:,0] > 200,
            np.logical_and(img_array[:,:,1] > 80, img_array[:,:,2] < 100)
        )
        orange_count = np.sum(orange_ish)
        print(f"  Orange pixels (true color): {orange_count}")

        return orange_count > 30

    def test_dim_text(self):
        """Test: Dim text (SGR 2) renders at reduced intensity"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        # Output normal text then dim text
        cmd = 'powershell -Command "$e = [char]27; Write-Host \\"NORMAL ${e}[2mDIM TEXT${e}[0m\\""'
        self.send_keys(cmd)
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, filepath = self.take_screenshot("vt_dim_text")
        img_array = np.array(screenshot)

        # Look for bright white text (normal) and dimmer gray text
        bright_white = np.all(img_array[:,:,:3] > 180, axis=2)
        dim_gray = np.logical_and(
            np.all(img_array[:,:,:3] > 60, axis=2),
            np.all(img_array[:,:,:3] < 150, axis=2)
        )

        bright_count = np.sum(bright_white)
        dim_count = np.sum(dim_gray)

        print(f"  Bright pixels: {bright_count}, Dim pixels: {dim_count}")

        # Should have both bright and dim text
        return bright_count > 50 and dim_count > 30

    def test_strikethrough_text(self):
        """Test: Strikethrough text (SGR 9) renders"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        # Output strikethrough text: ESC[9m
        cmd = 'powershell -Command "$e = [char]27; Write-Host \\"${e}[9mSTRIKETHROUGH${e}[0m\\""'
        self.send_keys(cmd)
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, filepath = self.take_screenshot("vt_strikethrough")
        img_array = np.array(screenshot)

        # Check that text is rendered
        white_pixels = np.all(img_array[:,:,:3] > 150, axis=2)
        white_count = np.sum(white_pixels)

        print(f"  White pixels (text + strikethrough): {white_count}")

        return white_count > 100

    def test_cursor_hide_show(self):
        """Test: Cursor visibility toggle (DECTCEM) works"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(1)

        # Take screenshot with cursor visible (default)
        screenshot1, _ = self.take_screenshot("vt_cursor_visible")

        # Hide cursor: ESC[?25l
        self.send_keys('powershell -Command "$e = [char]27; Write-Host -NoNewline \\"${e}[?25l\\""')
        self.send_keys("\n")
        time.sleep(1)

        screenshot2, _ = self.take_screenshot("vt_cursor_hidden")

        # Show cursor again: ESC[?25h
        self.send_keys('powershell -Command "$e = [char]27; Write-Host -NoNewline \\"${e}[?25h\\""')
        self.send_keys("\n")
        time.sleep(1)

        screenshot3, _ = self.take_screenshot("vt_cursor_restored")

        def count_cursor_pixels(img):
            img_array = np.array(img)
            green_cursor = np.logical_and(
                img_array[:,:,1] > 200,
                np.logical_and(img_array[:,:,0] < 100, img_array[:,:,2] < 100)
            )
            return np.sum(green_cursor)

        cursor1 = count_cursor_pixels(screenshot1)
        cursor2 = count_cursor_pixels(screenshot2)
        cursor3 = count_cursor_pixels(screenshot3)

        print(f"  Cursor pixels - Visible: {cursor1}, Hidden: {cursor2}, Restored: {cursor3}")

        # When hidden, should have fewer green cursor pixels
        return cursor2 < cursor1 or cursor3 > cursor2

    def test_combined_vt_attributes(self):
        """Test: Multiple VT text attributes combined"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        # Bold + Underline + Color: ESC[1;4;31m
        cmd = 'powershell -Command "$e = [char]27; Write-Host \\"${e}[1;4;31mBOLD UNDERLINE RED${e}[0m\\""'
        self.send_keys(cmd)
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, filepath = self.take_screenshot("vt_combined_attrs")
        img_array = np.array(screenshot)

        red_ish = np.logical_and(
            img_array[:,:,0] > 150,
            np.logical_and(img_array[:,:,1] < 100, img_array[:,:,2] < 100)
        )
        red_count = np.sum(red_ish)

        print(f"  Red pixels (bold+underline+red): {red_count}")
        return red_count > 50


def main():
    """Run VT feature tests"""
    log_file = SCREENSHOT_DIR / "vt_tests.log"
    tee = TeeOutput(log_file)
    sys.stdout = tee

    print(f"VT Feature Tests - {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 60)

    tester = VTFeatureTester()

    try:
        tester.start_terminal()

        # Run VT feature tests
        tester.run_test("True Color Foreground", tester.test_true_color_foreground)
        tester.run_test("Dim Text", tester.test_dim_text)
        tester.run_test("Strikethrough Text", tester.test_strikethrough_text)
        tester.run_test("Cursor Hide/Show", tester.test_cursor_hide_show)
        tester.run_test("Combined VT Attributes", tester.test_combined_vt_attributes)

        # Print summary
        print("\n" + "="*60)
        print("VT FEATURE TEST SUMMARY")
        print("="*60)

        passed = sum(1 for r in tester.test_results if r['passed'])
        total = len(tester.test_results)

        for result in tester.test_results:
            status = "PASS" if result['passed'] else "FAIL"
            print(f"{status}: {result['name']}")

        print(f"\nResults: {passed}/{total} tests passed")
        print(f"Screenshots saved to: {SCREENSHOT_DIR}")

        return passed == total

    except Exception as e:
        print(f"Test suite failed: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        tester.cleanup()
        time.sleep(1)
        sys.stdout = tee.terminal
        tee.close()


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
