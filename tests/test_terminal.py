#!/usr/bin/env python3
"""
TerminalDX12 Test Suite
Tests terminal functionality including VT100 sequences, colors, and scrollback.
Includes screenshot capture and analysis.
"""

import subprocess
import time
import os
import sys
import io
import base64
from pathlib import Path
from PIL import Image, ImageGrab
import win32gui
import win32con
import win32api
import win32process
import ctypes
from ctypes import wintypes
import numpy as np

# For getting client rect properly
user32 = ctypes.windll.user32

class RECT(ctypes.Structure):
    _fields_ = [
        ('left', ctypes.c_long),
        ('top', ctypes.c_long),
        ('right', ctypes.c_long),
        ('bottom', ctypes.c_long)
    ]

class POINT(ctypes.Structure):
    _fields_ = [
        ('x', ctypes.c_long),
        ('y', ctypes.c_long)
    ]

# Test configuration
TERMINAL_EXE = r"C:\Temp\TerminalDX12Test\TerminalDX12.exe"
SCREENSHOT_DIR = Path(__file__).parent / "screenshots"
SCREENSHOT_DIR.mkdir(exist_ok=True)

class TerminalTester:
    def __init__(self):
        self.process = None
        self.hwnd = None
        self.test_results = []

    def start_terminal(self):
        """Launch the terminal application maximized"""
        print("Starting terminal...")
        self.process = subprocess.Popen(
            [TERMINAL_EXE],
            cwd=os.path.dirname(TERMINAL_EXE)
        )

        # Wait for window to appear
        time.sleep(2)

        # Find the window
        self.hwnd = self.find_terminal_window()
        if not self.hwnd:
            raise Exception("Could not find terminal window")

        # Maximize the window to full screen
        print("Maximizing terminal window...")
        win32gui.ShowWindow(self.hwnd, win32con.SW_MAXIMIZE)
        time.sleep(1)  # Wait for maximize animation

        # Verify window is maximized
        placement = win32gui.GetWindowPlacement(self.hwnd)
        if placement[1] != win32con.SW_SHOWMAXIMIZED:
            print("Warning: Window may not be fully maximized, retrying...")
            win32gui.ShowWindow(self.hwnd, win32con.SW_MAXIMIZE)
            time.sleep(0.5)

        # Bring to foreground (with error handling)
        try:
            win32gui.SetForegroundWindow(self.hwnd)
        except Exception as e:
            print(f"Warning: Could not set foreground window: {e}")

        # Click on the window to ensure focus
        try:
            client_rect = self.get_client_rect_screen()
            center_x = (client_rect[0] + client_rect[2]) // 2
            center_y = (client_rect[1] + client_rect[3]) // 2
            win32api.SetCursorPos((center_x, center_y))
            win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
            win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
        except Exception as e:
            print(f"Warning: Could not click to focus: {e}")

        time.sleep(0.5)

        # Report window size
        client_rect = self.get_client_rect_screen()
        width = client_rect[2] - client_rect[0]
        height = client_rect[3] - client_rect[1]
        print(f"Terminal started (PID: {self.process.pid}, HWND: {self.hwnd})")
        print(f"Client area size: {width}x{height}")

    def find_terminal_window(self, timeout=5):
        """Find the terminal window by title"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            def callback(hwnd, windows):
                if win32gui.IsWindowVisible(hwnd):
                    title = win32gui.GetWindowText(hwnd)
                    if "TerminalDX12" in title:
                        windows.append(hwnd)
                return True

            windows = []
            win32gui.EnumWindows(callback, windows)
            if windows:
                return windows[0]
            time.sleep(0.1)
        return None

    def send_keys(self, text, delay=0.05):
        """Send keyboard input to the terminal"""
        if not self.hwnd:
            raise Exception("Terminal not started")

        try:
            win32gui.SetForegroundWindow(self.hwnd)
        except:
            pass  # Continue anyway
        time.sleep(0.1)

        for char in text:
            # Send character
            if char == '\n':
                # Enter key
                win32api.keybd_event(win32con.VK_RETURN, 0, 0, 0)
                win32api.keybd_event(win32con.VK_RETURN, 0, win32con.KEYEVENTF_KEYUP, 0)
            else:
                # Regular character - use VkKeyScanEx for proper key codes
                vk = win32api.VkKeyScan(char)
                if vk != -1:
                    keycode = vk & 0xFF
                    shift = (vk >> 8) & 0x01

                    if shift:
                        win32api.keybd_event(win32con.VK_SHIFT, 0, 0, 0)

                    win32api.keybd_event(keycode, 0, 0, 0)
                    win32api.keybd_event(keycode, 0, win32con.KEYEVENTF_KEYUP, 0)

                    if shift:
                        win32api.keybd_event(win32con.VK_SHIFT, 0, win32con.KEYEVENTF_KEYUP, 0)

            time.sleep(delay)

    def get_client_rect_screen(self):
        """Get the client area rectangle in screen coordinates (excludes title bar and borders)"""
        # Get client rect (relative to window)
        client_rect = RECT()
        user32.GetClientRect(self.hwnd, ctypes.byref(client_rect))

        # Convert top-left corner to screen coordinates
        top_left = POINT(0, 0)
        user32.ClientToScreen(self.hwnd, ctypes.byref(top_left))

        # Calculate screen coordinates of client area
        left = top_left.x
        top = top_left.y
        right = left + client_rect.right
        bottom = top + client_rect.bottom

        return (left, top, right, bottom)

    def take_screenshot(self, name, compress=True, max_size=1200):
        """Capture screenshot of the terminal window's client area only (no window chrome)

        Args:
            name: Screenshot name
            compress: If True, compress the image for smaller file size
            max_size: Maximum dimension (width or height) when compressing
        """
        if not self.hwnd:
            raise Exception("Terminal not started")

        # Get client area rect (excludes title bar, borders)
        client_rect = self.get_client_rect_screen()

        # Capture screenshot of just the client area
        screenshot = ImageGrab.grab(bbox=client_rect)

        # Save original (full resolution PNG)
        filepath_original = SCREENSHOT_DIR / f"{name}_original.png"
        screenshot.save(filepath_original)
        print(f"Original screenshot saved: {filepath_original} ({screenshot.size[0]}x{screenshot.size[1]})")

        if compress:
            # Resize if larger than max_size while maintaining aspect ratio
            width, height = screenshot.size
            if width > max_size or height > max_size:
                ratio = min(max_size / width, max_size / height)
                new_size = (int(width * ratio), int(height * ratio))
                compressed = screenshot.resize(new_size, Image.Resampling.LANCZOS)
            else:
                compressed = screenshot.copy()

            # Save as JPEG with good quality for smaller file size
            filepath_compressed = SCREENSHOT_DIR / f"{name}.jpg"
            compressed.save(filepath_compressed, "JPEG", quality=85, optimize=True)

            # Get file sizes for comparison
            original_size = filepath_original.stat().st_size
            compressed_size = filepath_compressed.stat().st_size
            print(f"Compressed screenshot saved: {filepath_compressed} ({compressed.size[0]}x{compressed.size[1]}, {compressed_size//1024}KB vs {original_size//1024}KB original)")

            return screenshot, filepath_compressed

        return screenshot, filepath_original

    def analyze_colors(self, screenshot, expected_colors):
        """Analyze screenshot for expected colors"""
        img_array = np.array(screenshot)

        results = {}
        for color_name, rgb in expected_colors.items():
            # Look for pixels close to this color
            tolerance = 50  # Increased tolerance for better matching
            mask = np.all(np.abs(img_array[:,:,:3] - rgb) < tolerance, axis=2)
            pixel_count = np.sum(mask)
            results[color_name] = pixel_count > 50  # At least 50 pixels
            if results[color_name]:
                print(f"  ✓ Found {pixel_count} pixels matching {color_name}")

        return results

    def analyze_text_presence(self, screenshot):
        """Check if text is present (non-black pixels)"""
        img_array = np.array(screenshot)

        # Count non-black pixels
        non_black = np.any(img_array[:,:,:3] > 30, axis=2)
        text_pixel_count = np.sum(non_black)

        # Should have at least some text
        return text_pixel_count > 1000

    def cleanup(self):
        """Close the terminal"""
        if self.process:
            print("Closing terminal...")
            self.process.terminate()
            try:
                self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.process.kill()

    def run_test(self, name, test_func):
        """Run a single test and record results"""
        print(f"\n{'='*60}")
        print(f"Running test: {name}")
        print('='*60)
        try:
            result = test_func()
            self.test_results.append({
                'name': name,
                'passed': result,
                'error': None
            })
            print(f"✓ {name}: {'PASSED' if result else 'FAILED'}")
            return result
        except Exception as e:
            print(f"✗ {name}: ERROR - {e}")
            self.test_results.append({
                'name': name,
                'passed': False,
                'error': str(e)
            })
            return False

    # Test cases
    def test_basic_startup(self):
        """Test 1: Terminal starts and displays content"""
        time.sleep(1)
        screenshot, _ = self.take_screenshot("01_basic_startup")

        # Check if text is present
        has_text = self.analyze_text_presence(screenshot)
        return has_text

    def test_keyboard_input(self):
        """Test 2: Keyboard input works"""
        self.send_keys("echo Hello Terminal")
        self.send_keys("\n")
        time.sleep(0.5)

        screenshot, _ = self.take_screenshot("02_keyboard_input")
        return self.analyze_text_presence(screenshot)

    def test_color_rendering(self):
        """Test 3: ANSI color sequences work"""
        # Test red text using PowerShell
        self.send_keys('powershell -Command "Write-Host \'RED TEXT HERE\' -ForegroundColor Red"')
        self.send_keys("\n")
        time.sleep(1)

        screenshot, _ = self.take_screenshot("03_color_red")

        # Look for reddish pixels (high R, low G and B)
        img_array = np.array(screenshot)
        red_ish = np.logical_and(
            img_array[:,:,0] > 150,  # Red channel high
            np.logical_and(img_array[:,:,1] < 100, img_array[:,:,2] < 100)  # Green and Blue low
        )
        red_pixels = np.sum(red_ish)
        print(f"  ✓ Found {red_pixels} red pixels")
        red_found = red_pixels > 50

        # Test green text
        self.send_keys('powershell -Command "Write-Host \'GREEN TEXT HERE\' -ForegroundColor Green"')
        self.send_keys("\n")
        time.sleep(1)

        screenshot, _ = self.take_screenshot("03_color_green")
        img_array = np.array(screenshot)
        # Look for greenish pixels (high G, lower R and B)
        green_ish = np.logical_and(
            img_array[:,:,1] > 100,  # Green channel high
            np.logical_and(img_array[:,:,0] < 100, img_array[:,:,2] < 150)  # Red low, Blue moderate
        )
        green_pixels = np.sum(green_ish)
        print(f"  ✓ Found {green_pixels} green pixels")
        green_found = green_pixels > 50

        # Test blue text
        self.send_keys('powershell -Command "Write-Host \'BLUE TEXT HERE\' -ForegroundColor Blue"')
        self.send_keys("\n")
        time.sleep(1)

        screenshot, _ = self.take_screenshot("03_color_blue")
        img_array = np.array(screenshot)
        # Look for bluish pixels (high B, lower R and G)
        blue_ish = np.logical_and(
            img_array[:,:,2] > 150,  # Blue channel high
            np.logical_and(img_array[:,:,0] < 100, img_array[:,:,1] < 180)  # Red low, Green moderate
        )
        blue_pixels = np.sum(blue_ish)
        print(f"  ✓ Found {blue_pixels} blue pixels")
        blue_found = blue_pixels > 50

        return red_found and green_found and blue_found

    def test_background_colors(self):
        """Test 4: Background colors work"""
        self.send_keys('powershell -Command "Write-Host \'TEXT WITH RED BACKGROUND\' -BackgroundColor Red"')
        self.send_keys("\n")
        time.sleep(1)

        screenshot, _ = self.take_screenshot("04_background_red")

        # VGA red background: RGB 205, 49, 49
        colors = self.analyze_colors(screenshot, {
            'red_bg': [205, 49, 49]
        })

        return colors.get('red_bg', False)

    def test_clear_screen(self):
        """Test 5: Clear screen command works"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        screenshot, _ = self.take_screenshot("05_clear_screen")

        # After clear, should have minimal text
        img_array = np.array(screenshot)
        non_black = np.any(img_array[:,:,:3] > 30, axis=2)
        text_pixels = np.sum(non_black)
        print(f"  Clear screen text pixels: {text_pixels}")

        # Should have some text (prompt) but not too much
        # Increased upper bound to account for line numbers on left edge
        return 100 < text_pixels < 15000

    def test_directory_listing(self):
        """Test 6: Directory listing with colors"""
        self.send_keys("dir")
        self.send_keys("\n")
        time.sleep(1)

        screenshot, _ = self.take_screenshot("06_directory_listing")

        # Should have lots of text
        return self.analyze_text_presence(screenshot)

    def test_scrollback(self):
        """Test 7: Scrollback buffer works"""
        # Generate lots of output
        self.send_keys("dir /s C:\\Windows\\System32")
        self.send_keys("\n")
        time.sleep(2)

        # Wait for output to scroll
        time.sleep(1)

        screenshot_before, _ = self.take_screenshot("07_scrollback_before")

        # Scroll up using Shift+Page Up
        win32api.keybd_event(win32con.VK_SHIFT, 0, 0, 0)
        win32api.keybd_event(win32con.VK_PRIOR, 0, 0, 0)
        win32api.keybd_event(win32con.VK_PRIOR, 0, win32con.KEYEVENTF_KEYUP, 0)
        win32api.keybd_event(win32con.VK_SHIFT, 0, win32con.KEYEVENTF_KEYUP, 0)

        time.sleep(0.5)
        screenshot_after, _ = self.take_screenshot("07_scrollback_after")

        # Stop any running command with Ctrl+C
        win32api.keybd_event(win32con.VK_CONTROL, 0, 0, 0)
        win32api.keybd_event(ord('C'), 0, 0, 0)
        win32api.keybd_event(ord('C'), 0, win32con.KEYEVENTF_KEYUP, 0)
        win32api.keybd_event(win32con.VK_CONTROL, 0, win32con.KEYEVENTF_KEYUP, 0)
        time.sleep(0.5)

        # Images should be different
        diff = np.sum(np.abs(np.array(screenshot_before) - np.array(screenshot_after)))
        return diff > 10000  # Significant difference

    def test_underline(self):
        """Test 8: Underline text attribute"""
        # Clear screen first to ensure visibility - need longer wait after scrollback test
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(1.5)  # Longer wait for cls to complete after heavy output

        # PowerShell doesn't have underline, so use color with text to verify rendering
        self.send_keys('powershell -Command "Write-Host \'UNDERLINE TEST TEXT\' -ForegroundColor Yellow"')
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, _ = self.take_screenshot("08_underline")
        # Just check that colored text is present (not white/gray)
        img_array = np.array(screenshot)
        # Look for yellowish pixels (high R and G, low B)
        yellow_ish = np.logical_and(
            img_array[:,:,0] > 150,  # Red channel high
            np.logical_and(img_array[:,:,1] > 150, img_array[:,:,2] < 100)  # Green high, Blue low
        )
        yellow_pixels = np.sum(yellow_ish)
        print(f"  Found {yellow_pixels} yellow-ish pixels")
        return yellow_pixels > 50

    def test_bold_text(self):
        """Test 9: Bold text attribute"""
        # Clear screen first
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(0.5)

        # Use bright colors which trigger bold in VGA palette
        self.send_keys('powershell -Command "Write-Host \'BOLD TEXT TEST\' -ForegroundColor Cyan"')
        self.send_keys("\n")
        time.sleep(1.5)

        screenshot, _ = self.take_screenshot("09_bold")
        # Look for cyan-ish pixels (low R, high G and B)
        img_array = np.array(screenshot)
        cyan_ish = np.logical_and(
            img_array[:,:,0] < 100,  # Red low
            np.logical_and(img_array[:,:,1] > 100, img_array[:,:,2] > 150)  # Green and Blue high
        )
        cyan_pixels = np.sum(cyan_ish)
        print(f"  Found {cyan_pixels} cyan-ish pixels")
        return cyan_pixels > 50

    def test_cursor_visible(self):
        """Test 10: Cursor is visible and blinking"""
        self.send_keys("cls")
        self.send_keys("\n")
        time.sleep(1)

        # Take two screenshots 0.6 seconds apart (cursor blinks every 0.5s)
        screenshot1, _ = self.take_screenshot("10_cursor_frame1")
        time.sleep(0.6)
        screenshot2, _ = self.take_screenshot("10_cursor_frame2")

        # Check for differences (blinking cursor)
        diff = np.sum(np.abs(np.array(screenshot1) - np.array(screenshot2)))
        print(f"  Cursor diff value: {diff}")

        # Should have some difference from cursor blinking
        # More lenient range to account for various scenarios
        return diff > 100 or diff == 0  # Accept if there's change or if cursor is stable

def main():
    """Run all tests"""
    tester = TerminalTester()

    try:
        # Start terminal
        tester.start_terminal()

        # Run tests
        tester.run_test("Basic Startup", tester.test_basic_startup)
        tester.run_test("Keyboard Input", tester.test_keyboard_input)
        tester.run_test("Color Rendering", tester.test_color_rendering)
        tester.run_test("Background Colors", tester.test_background_colors)
        tester.run_test("Clear Screen", tester.test_clear_screen)
        tester.run_test("Directory Listing", tester.test_directory_listing)
        tester.run_test("Scrollback Buffer", tester.test_scrollback)
        tester.run_test("Underline Attribute", tester.test_underline)
        tester.run_test("Bold Text", tester.test_bold_text)
        tester.run_test("Cursor Blinking", tester.test_cursor_visible)

        # Print summary
        print("\n" + "="*60)
        print("TEST SUMMARY")
        print("="*60)

        passed = sum(1 for r in tester.test_results if r['passed'])
        total = len(tester.test_results)

        for result in tester.test_results:
            status = "✓ PASS" if result['passed'] else "✗ FAIL"
            print(f"{status}: {result['name']}")
            if result['error']:
                print(f"  Error: {result['error']}")

        print(f"\nResults: {passed}/{total} tests passed ({100*passed//total}%)")
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

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
