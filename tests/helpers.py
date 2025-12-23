#!/usr/bin/env python3
"""
Helper classes for TerminalDX12 test suite.

Contains extracted components for:
- Screen analysis (color detection, text presence)
- Keyboard control (key sending)
- OCR verification (text matching)
"""

from typing import Dict, List, Tuple, Optional, Callable
from PIL import Image, ImageEnhance
import numpy as np
import asyncio
import ctypes
from ctypes import wintypes
import win32gui
import win32con
import win32api
import time

import subprocess
from pathlib import Path
from PIL import ImageGrab

from config import TestConfig, VGAColors

# OCR imports
try:
    from winocr import recognize_pil
    OCR_AVAILABLE = True
except ImportError:
    OCR_AVAILABLE = False


# ============================================================================
# ctypes structures
# ============================================================================

class RECT(ctypes.Structure):
    """Windows RECT structure."""
    _fields_ = [
        ('left', ctypes.c_long),
        ('top', ctypes.c_long),
        ('right', ctypes.c_long),
        ('bottom', ctypes.c_long)
    ]


class POINT(ctypes.Structure):
    """Windows POINT structure."""
    _fields_ = [
        ('x', ctypes.c_long),
        ('y', ctypes.c_long)
    ]


user32 = ctypes.windll.user32


# ============================================================================
# ScreenAnalyzer - Color detection and text presence analysis
# ============================================================================

class ScreenAnalyzer:
    """Analyzes screenshots for colors and text presence."""

    def __init__(self, color_tolerance: int = None):
        """
        Initialize screen analyzer.

        Args:
            color_tolerance: Tolerance for color matching (uses config default if None)
        """
        self.color_tolerance = color_tolerance or TestConfig.COLOR_TOLERANCE

    def analyze_colors(
        self,
        screenshot: Image.Image,
        expected_colors: Dict[str, Tuple[int, int, int]],
        min_pixels: int = 50
    ) -> Dict[str, bool]:
        """
        Analyze screenshot for expected colors.

        Args:
            screenshot: PIL Image to analyze
            expected_colors: Dict mapping color name to RGB tuple
            min_pixels: Minimum pixels to consider color present

        Returns:
            Dict mapping color name to bool (found or not)
        """
        img_array = np.array(screenshot)
        results = {}

        for color_name, rgb in expected_colors.items():
            mask = np.all(
                np.abs(img_array[:, :, :3] - rgb) < self.color_tolerance,
                axis=2
            )
            pixel_count = np.sum(mask)
            results[color_name] = pixel_count > min_pixels

        return results

    def analyze_text_presence(
        self,
        screenshot: Image.Image,
        min_pixels: int = None
    ) -> bool:
        """
        Check if text is present (non-black pixels).

        Args:
            screenshot: PIL Image to analyze
            min_pixels: Minimum non-black pixels to consider text present

        Returns:
            True if text is present
        """
        min_pixels = min_pixels or TestConfig.MIN_TEXT_PIXELS
        img_array = np.array(screenshot)
        non_black = np.any(img_array[:, :, :3] > 30, axis=2)
        text_pixel_count = np.sum(non_black)
        return text_pixel_count > min_pixels

    def find_color_pixels(
        self,
        screenshot: Image.Image,
        color_filter: Callable[[np.ndarray], np.ndarray]
    ) -> int:
        """
        Count pixels matching a custom color filter.

        Args:
            screenshot: PIL Image to analyze
            color_filter: Function that takes image array and returns boolean mask

        Returns:
            Number of matching pixels
        """
        img_array = np.array(screenshot)
        mask = color_filter(img_array)
        return np.sum(mask)

    def find_red_pixels(self, screenshot: Image.Image) -> int:
        """Count reddish pixels (high R, low G and B)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 0] > 150,
            np.logical_and(img[:, :, 1] < 100, img[:, :, 2] < 100)
        ))

    def find_green_pixels(self, screenshot: Image.Image) -> int:
        """Count greenish pixels (high G, low R and B)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 1] > 100,
            np.logical_and(img[:, :, 0] < 100, img[:, :, 2] < 150)
        ))

    def find_blue_pixels(self, screenshot: Image.Image) -> int:
        """Count bluish pixels (high B, low R and G)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 2] > 150,
            np.logical_and(img[:, :, 0] < 100, img[:, :, 1] < 180)
        ))

    def find_cyan_pixels(self, screenshot: Image.Image) -> int:
        """Count cyanish pixels (low R, high G and B)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 0] < 100,
            np.logical_and(img[:, :, 1] > 100, img[:, :, 2] > 150)
        ))

    def find_magenta_pixels(self, screenshot: Image.Image) -> int:
        """Count magenta pixels (high R and B, low G)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 0] > 150,
            np.logical_and(img[:, :, 1] < 100, img[:, :, 2] > 150)
        ))

    def find_yellow_pixels(self, screenshot: Image.Image) -> int:
        """Count yellowish pixels (high R and G, low B)."""
        return self.find_color_pixels(screenshot, lambda img: np.logical_and(
            img[:, :, 0] > 150,
            np.logical_and(img[:, :, 1] > 150, img[:, :, 2] < 100)
        ))

    def find_white_pixels(self, screenshot: Image.Image) -> int:
        """Count whitish pixels (all channels high)."""
        return self.find_color_pixels(
            screenshot,
            lambda img: np.all(img[:, :, :3] > 150, axis=2)
        )

    def find_black_pixels(self, screenshot: Image.Image) -> int:
        """Count blackish pixels (all channels low)."""
        return self.find_color_pixels(
            screenshot,
            lambda img: np.all(img[:, :, :3] < 30, axis=2)
        )

    def get_black_ratio(self, screenshot: Image.Image) -> float:
        """Get ratio of black pixels in screenshot."""
        img_array = np.array(screenshot)
        total_pixels = img_array.shape[0] * img_array.shape[1]
        black_pixels = self.find_black_pixels(screenshot)
        return black_pixels / total_pixels

    def compare_screenshots(
        self,
        screenshot1: Image.Image,
        screenshot2: Image.Image
    ) -> int:
        """
        Calculate difference between two screenshots.

        Args:
            screenshot1: First screenshot
            screenshot2: Second screenshot

        Returns:
            Total pixel difference value
        """
        diff = np.sum(np.abs(
            np.array(screenshot1).astype(np.int16) -
            np.array(screenshot2).astype(np.int16)
        ))
        return int(diff)


# ============================================================================
# KeyboardController - Keyboard input simulation
# ============================================================================

class KeyboardController:
    """Handles keyboard input simulation for Windows."""

    def __init__(self, hwnd: int = None, key_delay: float = None):
        """
        Initialize keyboard controller.

        Args:
            hwnd: Window handle to target
            key_delay: Delay between keystrokes (uses config default if None)
        """
        self.hwnd = hwnd
        self.key_delay = key_delay or TestConfig.KEY_DELAY

    def set_window(self, hwnd: int) -> None:
        """Set target window handle."""
        self.hwnd = hwnd

    def _ensure_focus(self) -> None:
        """Bring target window to foreground."""
        if self.hwnd:
            try:
                win32gui.SetForegroundWindow(self.hwnd)
            except Exception:
                pass  # Continue anyway
            time.sleep(0.1)

    def send_keys(
        self,
        text: str,
        delay: float = None,
        ensure_focus: bool = True
    ) -> None:
        """
        Send keyboard input.

        Args:
            text: Text to type (supports \\n for Enter)
            delay: Delay between keystrokes
            ensure_focus: If True, ensure window has focus first
        """
        if ensure_focus:
            self._ensure_focus()

        delay = delay or self.key_delay

        for char in text:
            if char == '\n':
                self._send_key(win32con.VK_RETURN)
            else:
                self._send_char(char)
            time.sleep(delay)

    def _send_key(self, vk_code: int, with_shift: bool = False) -> None:
        """Send a single key press."""
        if with_shift:
            win32api.keybd_event(win32con.VK_SHIFT, 0, 0, 0)

        win32api.keybd_event(vk_code, 0, 0, 0)
        win32api.keybd_event(vk_code, 0, win32con.KEYEVENTF_KEYUP, 0)

        if with_shift:
            win32api.keybd_event(win32con.VK_SHIFT, 0, win32con.KEYEVENTF_KEYUP, 0)

    def _send_char(self, char: str) -> None:
        """Send a single character."""
        vk = win32api.VkKeyScan(char)
        if vk != -1:
            keycode = vk & 0xFF
            shift = (vk >> 8) & 0x01
            self._send_key(keycode, with_shift=bool(shift))

    def send_special_key(
        self,
        vk_code: int,
        ctrl: bool = False,
        shift: bool = False,
        alt: bool = False
    ) -> None:
        """
        Send a special key with modifiers.

        Args:
            vk_code: Virtual key code
            ctrl: Hold Ctrl
            shift: Hold Shift
            alt: Hold Alt
        """
        self._ensure_focus()

        # Press modifiers
        if ctrl:
            win32api.keybd_event(win32con.VK_CONTROL, 0, 0, 0)
        if shift:
            win32api.keybd_event(win32con.VK_SHIFT, 0, 0, 0)
        if alt:
            win32api.keybd_event(win32con.VK_MENU, 0, 0, 0)

        # Press key
        win32api.keybd_event(vk_code, 0, 0, 0)
        win32api.keybd_event(vk_code, 0, win32con.KEYEVENTF_KEYUP, 0)

        # Release modifiers
        if alt:
            win32api.keybd_event(win32con.VK_MENU, 0, win32con.KEYEVENTF_KEYUP, 0)
        if shift:
            win32api.keybd_event(win32con.VK_SHIFT, 0, win32con.KEYEVENTF_KEYUP, 0)
        if ctrl:
            win32api.keybd_event(win32con.VK_CONTROL, 0, win32con.KEYEVENTF_KEYUP, 0)

    def send_ctrl_c(self) -> None:
        """Send Ctrl+C to interrupt running process."""
        self.send_special_key(ord('C'), ctrl=True)

    def send_page_up(self, with_shift: bool = False) -> None:
        """Send Page Up key."""
        self.send_special_key(win32con.VK_PRIOR, shift=with_shift)

    def send_page_down(self, with_shift: bool = False) -> None:
        """Send Page Down key."""
        self.send_special_key(win32con.VK_NEXT, shift=with_shift)


# ============================================================================
# OCRVerifier - OCR-based text verification
# ============================================================================

class OCRVerifier:
    """Handles OCR text extraction and verification."""

    def __init__(self):
        """Initialize OCR verifier."""
        self.available = OCR_AVAILABLE

    def _preprocess_for_ocr(self, img: Image.Image) -> Image.Image:
        """Preprocess image for better OCR accuracy."""
        if img.mode != 'RGB':
            img = img.convert('RGB')

        # Increase size for better OCR (2x)
        width, height = img.size
        img = img.resize((width * 2, height * 2), Image.Resampling.LANCZOS)

        # Increase contrast
        enhancer = ImageEnhance.Contrast(img)
        img = enhancer.enhance(2.0)

        # Increase sharpness
        enhancer = ImageEnhance.Sharpness(img)
        img = enhancer.enhance(2.0)

        return img

    async def _ocr_image_async(self, img: Image.Image) -> str:
        """Run OCR on an image (async)."""
        if not self.available:
            return ""
        img = self._preprocess_for_ocr(img)
        result = await recognize_pil(img, "en")
        return result.text

    def ocr_image(self, img: Image.Image) -> str:
        """Run OCR on an image (sync wrapper)."""
        if not self.available:
            return ""
        return asyncio.run(self._ocr_image_async(img))

    def _normalize_text(self, text: str) -> str:
        """Normalize OCR text for comparison."""
        # Convert to uppercase
        text = text.upper()
        # Remove extra whitespace
        text = ' '.join(text.split())
        # Common OCR substitutions
        replacements = {
            '0': 'O', '1': 'L', '|': 'L',
            'l': 'L', 'I': 'L',
            '_': '', '-': '',
            '<': 'K', 'm': 'M', 'W': 'VV',
        }
        for old, new in replacements.items():
            text = text.replace(old, new)
        # Remove remaining punctuation
        text = ''.join(c for c in text if c.isalnum() or c.isspace())
        return text

    def contains(self, ocr_text: str, expected: str, threshold: float = 0.8) -> bool:
        """
        Check if expected text is in OCR output (fuzzy match).

        Args:
            ocr_text: OCR extracted text
            expected: Expected text to find
            threshold: Word match threshold (0.0 to 1.0)

        Returns:
            True if expected text is found
        """
        haystack = self._normalize_text(ocr_text)
        needle = self._normalize_text(expected)

        # Direct contains check
        if needle in haystack:
            return True

        # Check for partial word matches
        needle_words = needle.split()
        if not needle_words:
            return False
        found_count = sum(1 for word in needle_words if word in haystack)
        return (found_count / len(needle_words)) >= threshold

    def verify_texts(
        self,
        screenshot: Image.Image,
        expected_texts: List[Tuple[str, str]]
    ) -> Tuple[bool, str]:
        """
        Verify that expected texts are found in screenshot via OCR.

        Args:
            screenshot: PIL Image to analyze
            expected_texts: List of (text, description) tuples

        Returns:
            (passed, details) tuple
        """
        if not self.available:
            return True, "OCR not available, skipping verification"

        ocr_text = self.ocr_image(screenshot)
        results = []
        all_passed = True

        for expected, description in expected_texts:
            found = self.contains(ocr_text, expected)
            status = "OK" if found else "FAIL"
            results.append(f"  {status}: '{expected}' ({description})")
            if not found:
                all_passed = False

        details = f"OCR Text: {ocr_text[:200]}...\n" + "\n".join(results)
        return all_passed, details


# ============================================================================
# WindowHelper - Window management utilities
# ============================================================================

class WindowHelper:
    """Utilities for window management."""

    @staticmethod
    def get_client_rect_screen(hwnd: int) -> Tuple[int, int, int, int]:
        """
        Get client area rectangle in screen coordinates.

        Args:
            hwnd: Window handle

        Returns:
            (left, top, right, bottom) tuple
        """
        client_rect = RECT()
        user32.GetClientRect(hwnd, ctypes.byref(client_rect))

        top_left = POINT(0, 0)
        user32.ClientToScreen(hwnd, ctypes.byref(top_left))

        left = top_left.x
        top = top_left.y
        right = left + client_rect.right
        bottom = top + client_rect.bottom

        return (left, top, right, bottom)

    @staticmethod
    def find_window_by_title(
        title_contains: str,
        timeout: float = 5.0
    ) -> Optional[int]:
        """
        Find a window by partial title match.

        Args:
            title_contains: Substring to search for in window title
            timeout: Maximum time to wait

        Returns:
            Window handle or None
        """
        start_time = time.time()
        while time.time() - start_time < timeout:
            windows: List[int] = []

            def callback(hwnd: int, windows: List[int]) -> bool:
                if win32gui.IsWindowVisible(hwnd):
                    title = win32gui.GetWindowText(hwnd)
                    if title_contains in title:
                        windows.append(hwnd)
                return True

            win32gui.EnumWindows(callback, windows)
            if windows:
                return windows[0]
            time.sleep(0.1)
        return None

    @staticmethod
    def maximize_window(hwnd: int) -> None:
        """Maximize a window."""
        win32gui.ShowWindow(hwnd, win32con.SW_MAXIMIZE)

    @staticmethod
    def restore_window(hwnd: int) -> None:
        """Restore a window from maximized/minimized state."""
        win32gui.ShowWindow(hwnd, win32con.SW_RESTORE)

    @staticmethod
    def click_window_center(hwnd: int) -> None:
        """Click in the center of a window's client area."""
        rect = WindowHelper.get_client_rect_screen(hwnd)
        center_x = (rect[0] + rect[2]) // 2
        center_y = (rect[1] + rect[3]) // 2

        win32api.SetCursorPos((center_x, center_y))
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
        win32api.mouse_event(win32con.MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)


# ============================================================================
# TerminalTester - Main test driver class
# ============================================================================

class TerminalTester:
    """
    Main test driver for TerminalDX12 testing.

    Manages terminal lifecycle and provides high-level testing methods.
    """

    def __init__(self, terminal_exe: str = None):
        """
        Initialize the terminal tester.

        Args:
            terminal_exe: Path to terminal executable (uses config default if None)
        """
        self.terminal_exe = terminal_exe or TestConfig.TERMINAL_EXE
        self.screenshot_dir = TestConfig.SCREENSHOT_DIR
        self.process: Optional[subprocess.Popen] = None
        self.hwnd: Optional[int] = None
        self._keyboard = KeyboardController()
        self._analyzer = ScreenAnalyzer()
        TestConfig.ensure_dirs()

    def start_terminal(self) -> bool:
        """
        Start the terminal process and wait for window.

        Returns:
            True if terminal started successfully
        """
        try:
            self.process = subprocess.Popen(
                [self.terminal_exe],
                creationflags=subprocess.CREATE_NEW_CONSOLE
            )
            time.sleep(TestConfig.STARTUP_WAIT)

            # Find the terminal window
            self.hwnd = WindowHelper.find_window_by_title("TerminalDX12", timeout=5.0)
            if not self.hwnd:
                # Try finding by process
                self._find_window_by_process()

            if self.hwnd:
                self._keyboard.set_window(self.hwnd)
                # Bring to foreground
                try:
                    win32gui.SetForegroundWindow(self.hwnd)
                except Exception:
                    pass
                return True
            return False
        except Exception as e:
            print(f"Failed to start terminal: {e}")
            return False

    def _find_window_by_process(self) -> None:
        """Find window by process ID if title search fails."""
        if not self.process:
            return

        def callback(hwnd, windows):
            if win32gui.IsWindowVisible(hwnd):
                try:
                    _, pid = win32process.GetWindowThreadProcessId(hwnd)
                    if pid == self.process.pid:
                        windows.append(hwnd)
                except Exception:
                    pass
            return True

        try:
            import win32process
            windows = []
            win32gui.EnumWindows(callback, windows)
            if windows:
                self.hwnd = windows[0]
        except ImportError:
            pass

    def cleanup(self) -> None:
        """Clean up terminal process."""
        if self.process:
            try:
                self.process.terminate()
                self.process.wait(timeout=2)
            except Exception:
                try:
                    self.process.kill()
                except Exception:
                    pass
            self.process = None
        self.hwnd = None

    def send_keys(self, text: str, delay: float = None) -> None:
        """
        Send keyboard input to the terminal.

        Args:
            text: Text to type (supports \\n for Enter)
            delay: Delay between keystrokes
        """
        self._keyboard.send_keys(text, delay=delay)

    def wait_and_screenshot(
        self,
        name: str,
        wait_stable: bool = True,
        max_wait: float = None
    ) -> Tuple[Image.Image, Path]:
        """
        Wait for screen stability and capture screenshot.

        Args:
            name: Base name for screenshot file
            wait_stable: If True, wait for screen to stabilize
            max_wait: Maximum time to wait for stability

        Returns:
            Tuple of (PIL Image, Path to saved file)
        """
        if wait_stable:
            self._wait_for_stability(max_wait or TestConfig.MAX_WAIT)

        screenshot = self._capture_screenshot()

        # Save screenshot
        filename = f"{name}_{int(time.time())}.png"
        filepath = self.screenshot_dir / filename
        screenshot.save(filepath)

        return screenshot, filepath

    def _wait_for_stability(self, max_wait: float) -> None:
        """Wait for screen to stop changing."""
        start_time = time.time()
        last_screenshot = self._capture_screenshot()
        stable_since = time.time()

        while time.time() - start_time < max_wait:
            time.sleep(TestConfig.POLL_INTERVAL)
            current = self._capture_screenshot()

            diff = self._analyzer.compare_screenshots(last_screenshot, current)
            if diff < TestConfig.SCREEN_CHANGE_THRESHOLD:
                if time.time() - stable_since >= TestConfig.STABILITY_TIME:
                    return
            else:
                stable_since = time.time()

            last_screenshot = current

    def _capture_screenshot(self) -> Image.Image:
        """Capture screenshot of terminal window."""
        if not self.hwnd:
            return Image.new('RGB', (100, 100), color='black')

        try:
            rect = WindowHelper.get_client_rect_screen(self.hwnd)
            return ImageGrab.grab(bbox=rect)
        except Exception:
            return Image.new('RGB', (100, 100), color='black')

    def analyze_text_presence(self, screenshot: Image.Image) -> bool:
        """
        Check if text is present in screenshot.

        Args:
            screenshot: PIL Image to analyze

        Returns:
            True if text is present
        """
        return self._analyzer.analyze_text_presence(screenshot)

    def verify_text(self, screenshot: Image.Image, expected: str, threshold: float = 0.7) -> Tuple[bool, str]:
        """
        Verify expected text is visible in screenshot using OCR.

        Args:
            screenshot: PIL Image to analyze
            expected: Text to look for
            threshold: Fuzzy match threshold (0.0-1.0)

        Returns:
            Tuple of (success, ocr_text)
        """
        if not OCR_AVAILABLE:
            # Fall back to basic text presence check
            return self._analyzer.analyze_text_presence(screenshot), "(OCR not available)"

        ocr = OCRVerifier()
        ocr_text = ocr.ocr_image(screenshot)
        found = ocr.contains(ocr_text, expected, threshold)
        return found, ocr_text

    def get_screen_text(self, screenshot: Image.Image = None) -> str:
        """
        Get all text from screenshot using OCR.

        Args:
            screenshot: PIL Image to analyze (captures new one if None)

        Returns:
            OCR extracted text
        """
        if screenshot is None:
            screenshot = self._capture_screenshot()

        if not OCR_AVAILABLE:
            return "(OCR not available)"

        ocr = OCRVerifier()
        return ocr.ocr_image(screenshot)

    def get_client_rect_screen(self) -> Tuple[int, int, int, int]:
        """
        Get client area rectangle in screen coordinates.

        Returns:
            (left, top, right, bottom) tuple
        """
        if not self.hwnd:
            return (0, 0, 100, 100)
        return WindowHelper.get_client_rect_screen(self.hwnd)
