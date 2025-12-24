"""
Pytest configuration and fixtures for TerminalDX12 test suite.

This module provides:
- Terminal fixture for test isolation
- Session-scoped terminal for fast test execution
- Custom markers for test categorization
- Command-line options for test modes
- Helper class fixtures (ScreenAnalyzer, KeyboardController, OCRVerifier)
"""

import pytest
import subprocess
import time
import os
import sys
from pathlib import Path
from typing import Generator

# Add tests directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from config import TestConfig
from helpers import ScreenAnalyzer, KeyboardController, OCRVerifier, WindowHelper, TerminalTester
from visual_regression import VisualRegressionTester, BASELINES_DIR, DIFFS_DIR

__all__ = [
    'terminal_session',
    'terminal_isolated',
    'terminal',
    'screenshot_dir',
    'clear_screen',
    'ocr_available',
    'screen_analyzer',
    'keyboard_controller',
    'ocr_verifier',
    'visual_regression',
    'update_baselines',
]


def get_terminal_tester():
    """Return the TerminalTester class."""
    return TerminalTester


def pytest_addoption(parser):
    """Add custom command-line options."""
    parser.addoption(
        "--isolated",
        action="store_true",
        default=False,
        help="Run tests in isolated mode (fresh terminal per test)"
    )
    parser.addoption(
        "--fast",
        action="store_true",
        default=False,
        help="Run tests in fast mode (shared terminal session)"
    )
    parser.addoption(
        "--terminal-exe",
        action="store",
        default=None,
        help="Path to terminal executable"
    )
    parser.addoption(
        "--update-baselines",
        action="store_true",
        default=False,
        help="Update visual regression baselines instead of comparing"
    )
    parser.addoption(
        "--visual-threshold",
        action="store",
        type=float,
        default=0.1,
        help="Visual regression threshold percentage (default: 0.1)"
    )


def pytest_configure(config):
    """Configure pytest with custom markers."""
    config.addinivalue_line(
        "markers", "slow: marks tests as slow (deselect with '-m \"not slow\"')"
    )
    config.addinivalue_line(
        "markers", "color: marks tests that verify color rendering"
    )
    config.addinivalue_line(
        "markers", "input: marks tests that verify keyboard input"
    )
    config.addinivalue_line(
        "markers", "scrolling: marks tests that verify scrollback"
    )
    config.addinivalue_line(
        "markers", "attributes: marks tests that verify text attributes"
    )
    config.addinivalue_line(
        "markers", "ocr: marks tests that require OCR"
    )
    config.addinivalue_line(
        "markers", "visual: marks visual regression tests"
    )

    # Override terminal path if provided
    terminal_exe = config.getoption("--terminal-exe")
    if terminal_exe:
        TestConfig.TERMINAL_EXE = terminal_exe


@pytest.fixture(scope="session")
def terminal_session() -> Generator:
    """
    Session-scoped terminal fixture.

    Creates a single terminal instance for the entire test session.
    Use this for fast test execution when test isolation is not required.
    """
    TerminalTester = get_terminal_tester()
    tester = TerminalTester()

    try:
        tester.start_terminal()
        yield tester
    finally:
        tester.cleanup()


@pytest.fixture(scope="function")
def terminal_isolated() -> Generator:
    """
    Function-scoped terminal fixture.

    Creates a fresh terminal instance for each test.
    Use this when tests need complete isolation from each other.
    """
    TerminalTester = get_terminal_tester()
    tester = TerminalTester()

    try:
        tester.start_terminal()
        yield tester
    finally:
        tester.cleanup()


@pytest.fixture
def terminal(request, terminal_session, terminal_isolated) -> Generator:
    """
    Smart terminal fixture that chooses isolation level based on command-line options.

    Usage:
        def test_something(terminal):
            terminal.send_keys("hello")
            screenshot, _ = terminal.wait_and_screenshot("test")
            assert terminal.analyze_text_presence(screenshot)

    Command-line options:
        --isolated: Use fresh terminal per test (slower but isolated)
        --fast: Use shared terminal session (faster but tests may interfere)
        (default): Use shared terminal session
    """
    if request.config.getoption("--isolated"):
        # Clear screen before each test for partial isolation
        terminal_isolated.send_keys("cls\n")
        time.sleep(0.5)
        yield terminal_isolated
    else:
        # Clear screen before each test for consistency
        terminal_session.send_keys("cls\n")
        time.sleep(0.5)
        yield terminal_session


@pytest.fixture
def screenshot_dir() -> Path:
    """Fixture providing the screenshot directory path."""
    TestConfig.ensure_dirs()
    return TestConfig.SCREENSHOT_DIR


# ============================================================================
# Pytest Hooks
# ============================================================================

def pytest_collection_modifyitems(config, items):
    """Modify test collection based on markers and options."""
    # Skip OCR tests if winocr is not available
    try:
        from winocr import recognize_pil
        ocr_available = True
    except ImportError:
        ocr_available = False

    skip_ocr = pytest.mark.skip(reason="winocr not installed")

    for item in items:
        if "ocr" in item.keywords and not ocr_available:
            item.add_marker(skip_ocr)


def pytest_runtest_makereport(item, call):
    """Capture screenshots on test failure."""
    if call.when == "call" and call.excinfo is not None:
        # Test failed - try to capture a screenshot for debugging
        try:
            # Get terminal fixture if available
            terminal = item.funcargs.get("terminal") or \
                       item.funcargs.get("terminal_session") or \
                       item.funcargs.get("terminal_isolated")

            if terminal and terminal.hwnd:
                failure_name = f"FAILURE_{item.name}"
                terminal.wait_and_screenshot(failure_name, wait_stable=False)
                print(f"  Failure screenshot saved: {failure_name}")
        except Exception as e:
            print(f"  Could not capture failure screenshot: {e}")


# ============================================================================
# Helper Fixtures
# ============================================================================

@pytest.fixture
def clear_screen(terminal):
    """Fixture that clears the terminal screen before the test."""
    terminal.send_keys("cls\n")
    time.sleep(0.5)
    return terminal


@pytest.fixture
def ocr_available():
    """Fixture that indicates whether OCR is available."""
    try:
        from winocr import recognize_pil
        return True
    except ImportError:
        return False


# ============================================================================
# Helper Class Fixtures
# ============================================================================

@pytest.fixture(scope="session")
def screen_analyzer() -> ScreenAnalyzer:
    """Session-scoped screen analyzer for color and text detection."""
    return ScreenAnalyzer()


@pytest.fixture
def keyboard_controller(terminal) -> KeyboardController:
    """Keyboard controller bound to the terminal window."""
    controller = KeyboardController(hwnd=terminal.hwnd)
    return controller


@pytest.fixture(scope="session")
def ocr_verifier() -> OCRVerifier:
    """Session-scoped OCR verifier for text verification."""
    return OCRVerifier()


# ============================================================================
# Visual Regression Fixtures
# ============================================================================

@pytest.fixture(scope="session")
def visual_regression(request) -> VisualRegressionTester:
    """
    Session-scoped visual regression tester.

    Provides methods to compare screenshots against baselines.

    Usage:
        def test_rendering(terminal, visual_regression):
            screenshot, _ = terminal.wait_and_screenshot("test")
            result = visual_regression.compare("test_rendering", screenshot)
            assert result, result.message
    """
    threshold = request.config.getoption("--visual-threshold")
    return VisualRegressionTester(threshold=threshold)


@pytest.fixture
def update_baselines(request) -> bool:
    """
    Fixture indicating whether to update baselines.

    Usage:
        def test_visual(terminal, visual_regression, update_baselines):
            screenshot, _ = terminal.wait_and_screenshot("test")
            if update_baselines:
                visual_regression.update_baseline("test_visual", screenshot)
            else:
                result = visual_regression.compare("test_visual", screenshot)
                assert result, result.message
    """
    return request.config.getoption("--update-baselines")
