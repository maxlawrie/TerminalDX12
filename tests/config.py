"""
Test Configuration for TerminalDX12 Test Suite

Configuration can be overridden via environment variables:
    TERMINAL_EXE: Path to the terminal executable
    SCREENSHOT_DIR: Directory for screenshot storage
    STARTUP_WAIT: Time to wait for terminal startup (seconds)
    COMMAND_WAIT: Time to wait after sending commands (seconds)
    STABILITY_TIME: Time screen must be stable before screenshot (seconds)
    MAX_WAIT: Maximum time to wait for screen stability (seconds)
    COLOR_TOLERANCE: RGB tolerance for color matching
    MIN_TEXT_PIXELS: Minimum pixels to consider text present
    OCR_THRESHOLD: OCR fuzzy match threshold (0.0-1.0)
"""

import os
from pathlib import Path

__all__ = [
    'TestConfig',
    'VGAColors',
]


class TestConfig:
    """Centralized test configuration with environment variable overrides."""

    # Paths
    TERMINAL_EXE: str = os.environ.get(
        'TERMINAL_EXE',
        str(Path(__file__).parent.parent / "build" / "bin" / "Release" / "TerminalDX12.exe")
    )
    SCREENSHOT_DIR: Path = Path(os.environ.get(
        'SCREENSHOT_DIR',
        Path(__file__).parent / "screenshots"
    ))
    LOG_FILE: Path = Path(os.environ.get(
        'LOG_FILE',
        Path(__file__).parent / "test_output.txt"
    ))

    # Timing - these can be tuned based on system performance
    STARTUP_WAIT: float = float(os.environ.get('STARTUP_WAIT', '2.0'))
    COMMAND_WAIT: float = float(os.environ.get('COMMAND_WAIT', '0.5'))
    CLEAR_WAIT: float = float(os.environ.get('CLEAR_WAIT', '0.5'))
    RENDER_WAIT: float = float(os.environ.get('RENDER_WAIT', '0.5'))
    STABILITY_TIME: float = float(os.environ.get('STABILITY_TIME', '0.3'))
    MAX_WAIT: float = float(os.environ.get('MAX_WAIT', '5.0'))
    POLL_INTERVAL: float = float(os.environ.get('POLL_INTERVAL', '0.1'))
    KEY_DELAY: float = float(os.environ.get('KEY_DELAY', '0.05'))

    # Thresholds
    COLOR_TOLERANCE: int = int(os.environ.get('COLOR_TOLERANCE', '50'))
    MIN_TEXT_PIXELS: int = int(os.environ.get('MIN_TEXT_PIXELS', '100'))
    OCR_THRESHOLD: float = float(os.environ.get('OCR_THRESHOLD', '0.8'))
    SCREEN_CHANGE_THRESHOLD: int = int(os.environ.get('SCREEN_CHANGE_THRESHOLD', '1000'))

    # Screenshot settings
    MAX_SCREENSHOT_SIZE: int = int(os.environ.get('MAX_SCREENSHOT_SIZE', '1200'))
    JPEG_QUALITY: int = int(os.environ.get('JPEG_QUALITY', '85'))

    @classmethod
    def ensure_dirs(cls) -> None:
        """Ensure required directories exist."""
        cls.SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)

    @classmethod
    def print_config(cls) -> None:
        """Print current configuration for debugging."""
        print("Test Configuration:")
        print(f"  TERMINAL_EXE: {cls.TERMINAL_EXE}")
        print(f"  SCREENSHOT_DIR: {cls.SCREENSHOT_DIR}")
        print(f"  STARTUP_WAIT: {cls.STARTUP_WAIT}s")
        print(f"  COMMAND_WAIT: {cls.COMMAND_WAIT}s")
        print(f"  STABILITY_TIME: {cls.STABILITY_TIME}s")
        print(f"  COLOR_TOLERANCE: {cls.COLOR_TOLERANCE}")


# VGA color palette (16 standard colors)
class VGAColors:
    """Standard VGA 16-color palette RGB values."""
    BLACK = (0, 0, 0)
    RED = (205, 49, 49)
    GREEN = (0, 205, 0)
    YELLOW = (205, 205, 0)
    BLUE = (0, 0, 238)
    MAGENTA = (205, 0, 205)
    CYAN = (0, 205, 205)
    WHITE = (229, 229, 229)
    BRIGHT_BLACK = (127, 127, 127)
    BRIGHT_RED = (255, 0, 0)
    BRIGHT_GREEN = (0, 255, 0)
    BRIGHT_YELLOW = (255, 255, 0)
    BRIGHT_BLUE = (92, 92, 255)
    BRIGHT_MAGENTA = (255, 0, 255)
    BRIGHT_CYAN = (0, 255, 255)
    BRIGHT_WHITE = (255, 255, 255)

    # Mapping from color index to RGB
    PALETTE = [
        BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE,
        BRIGHT_BLACK, BRIGHT_RED, BRIGHT_GREEN, BRIGHT_YELLOW,
        BRIGHT_BLUE, BRIGHT_MAGENTA, BRIGHT_CYAN, BRIGHT_WHITE
    ]
