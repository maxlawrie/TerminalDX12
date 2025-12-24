#!/usr/bin/env python3
"""
Visual Regression Tests for TerminalDX12.

These tests capture screenshots and compare them against stored baselines
to detect unintended visual changes in terminal rendering.

Usage:
    # Run visual regression tests (compare against baselines)
    pytest test_visual_regression.py -v

    # Update baselines (run when visual changes are intentional)
    pytest test_visual_regression.py -v --update-baselines

    # Adjust threshold (default 0.1%)
    pytest test_visual_regression.py -v --visual-threshold=0.5
"""

import pytest
import time


@pytest.mark.visual
class TestVisualRegression:
    """Visual regression tests for terminal rendering."""

    def test_startup_screen(self, terminal, visual_regression, update_baselines):
        """Verify terminal startup screen renders correctly."""
        # Clear and wait for stable rendering
        terminal.send_keys("cls\n")
        time.sleep(1.0)

        screenshot, _ = terminal.wait_and_screenshot("visual_startup")

        if update_baselines:
            path = visual_regression.update_baseline("startup_screen", screenshot)
            print(f"Updated baseline: {path}")
        else:
            result = visual_regression.compare("startup_screen", screenshot)
            assert result, f"Visual regression failed: {result.message}"

    def test_basic_text_rendering(self, terminal, visual_regression, update_baselines):
        """Verify basic text rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Output predictable text pattern
        terminal.send_keys('echo "ABCDEFGHIJKLMNOPQRSTUVWXYZ"\n')
        terminal.send_keys('echo "abcdefghijklmnopqrstuvwxyz"\n')
        terminal.send_keys('echo "0123456789"\n')
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("visual_text")

        if update_baselines:
            path = visual_regression.update_baseline("basic_text", screenshot)
            print(f"Updated baseline: {path}")
        else:
            result = visual_regression.compare("basic_text", screenshot)
            assert result, f"Visual regression failed: {result.message}"

    def test_ansi_colors_rendering(self, terminal, visual_regression, update_baselines):
        """Verify ANSI color rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # PowerShell ANSI color output
        colors_cmd = '''
$esc = [char]27
Write-Host "${esc}[31mRED${esc}[0m ${esc}[32mGREEN${esc}[0m ${esc}[34mBLUE${esc}[0m"
Write-Host "${esc}[33mYELLOW${esc}[0m ${esc}[35mMAGENTA${esc}[0m ${esc}[36mCYAN${esc}[0m"
Write-Host "${esc}[1;31mBOLD RED${esc}[0m ${esc}[4;32mUNDERLINE GREEN${esc}[0m"
'''
        for line in colors_cmd.strip().split('\n'):
            terminal.send_keys(line + '\n')
            time.sleep(0.1)
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("visual_colors")

        if update_baselines:
            path = visual_regression.update_baseline("ansi_colors", screenshot)
            print(f"Updated baseline: {path}")
        else:
            result = visual_regression.compare("ansi_colors", screenshot)
            assert result, f"Visual regression failed: {result.message}"

    def test_unicode_rendering(self, terminal, visual_regression, update_baselines):
        """Verify Unicode character rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Unicode test patterns
        terminal.send_keys('echo "Box: ┌─┬─┐ │ ├─┼─┤ └─┴─┘"\n')
        terminal.send_keys('echo "Arrows: ← ↑ → ↓ ↔ ↕"\n')
        terminal.send_keys('echo "Math: ± × ÷ ≤ ≥ ≠ ∞"\n')
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("visual_unicode")

        if update_baselines:
            path = visual_regression.update_baseline("unicode_chars", screenshot)
            print(f"Updated baseline: {path}")
        else:
            result = visual_regression.compare("unicode_chars", screenshot)
            assert result, f"Visual regression failed: {result.message}"

    def test_cursor_rendering(self, terminal, visual_regression, update_baselines):
        """Verify cursor rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Position cursor with some text
        terminal.send_keys('echo "Cursor test:"\n')
        time.sleep(0.3)

        # The cursor should be visible at the prompt
        screenshot, _ = terminal.wait_and_screenshot("visual_cursor")

        if update_baselines:
            path = visual_regression.update_baseline("cursor", screenshot)
            print(f"Updated baseline: {path}")
        else:
            # Use higher threshold for cursor since it blinks
            result = visual_regression.compare("cursor", screenshot, threshold=1.0)
            assert result, f"Visual regression failed: {result.message}"

    def test_scrollback_rendering(self, terminal, visual_regression, update_baselines):
        """Verify scrollback buffer rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Generate enough output to trigger scrolling
        terminal.send_keys('for ($i=1; $i -le 30; $i++) { echo "Line $i" }\n')
        time.sleep(1.0)

        screenshot, _ = terminal.wait_and_screenshot("visual_scrollback")

        if update_baselines:
            path = visual_regression.update_baseline("scrollback", screenshot)
            print(f"Updated baseline: {path}")
        else:
            result = visual_regression.compare("scrollback", screenshot)
            assert result, f"Visual regression failed: {result.message}"

    def test_prompt_rendering(self, terminal, visual_regression, update_baselines):
        """Verify command prompt rendering is consistent."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Just show the prompt
        screenshot, _ = terminal.wait_and_screenshot("visual_prompt")

        if update_baselines:
            path = visual_regression.update_baseline("prompt", screenshot)
            print(f"Updated baseline: {path}")
        else:
            # Use higher threshold due to path/username variations
            result = visual_regression.compare("prompt", screenshot, threshold=5.0)
            assert result, f"Visual regression failed: {result.message}"


@pytest.mark.visual
class TestVisualRegressionUtilities:
    """Tests for visual regression utility functions."""

    def test_baseline_creation(self, terminal, visual_regression):
        """Test that new baselines are created automatically."""
        import uuid
        test_name = f"test_baseline_{uuid.uuid4().hex[:8]}"

        terminal.send_keys("cls\n")
        time.sleep(0.5)
        terminal.send_keys('echo "Baseline test"\n')
        time.sleep(0.3)

        screenshot, _ = terminal.wait_and_screenshot("baseline_test")

        # First comparison should create baseline
        result = visual_regression.compare(test_name, screenshot)
        assert result.passed, "First comparison should pass (creates baseline)"
        assert "New baseline created" in result.message

        # Cleanup
        visual_regression.delete_baseline(test_name)

    def test_diff_detection(self, terminal, visual_regression):
        """Test that visual differences are detected."""
        from PIL import Image
        import numpy as np

        # Create a baseline image
        baseline = Image.new('RGB', (100, 100), color='black')
        test_name = "test_diff_detection"
        visual_regression.update_baseline(test_name, baseline)

        # Create a different image (white instead of black)
        different = Image.new('RGB', (100, 100), color='white')

        # This should fail
        result = visual_regression.compare(test_name, different, threshold=0.1)
        assert not result.passed, "Different images should not pass"
        assert result.diff_percentage > 50, "Should detect significant difference"

        # Cleanup
        visual_regression.delete_baseline(test_name)

    def test_threshold_sensitivity(self, terminal, visual_regression):
        """Test that threshold parameter works correctly."""
        from PIL import Image
        import numpy as np

        # Create baseline
        baseline = Image.new('RGB', (100, 100), color='black')
        test_name = "test_threshold"
        visual_regression.update_baseline(test_name, baseline)

        # Create image with small difference (1% of pixels changed)
        different = baseline.copy()
        pixels = different.load()
        for x in range(10):  # 10x10 = 100 pixels = 1% of 10000
            for y in range(10):
                pixels[x, y] = (255, 255, 255)

        # Should fail with 0.1% threshold
        result = visual_regression.compare(test_name, different, threshold=0.1)
        assert not result.passed, "Should fail with strict threshold"

        # Should pass with 5% threshold
        result = visual_regression.compare(test_name, different, threshold=5.0)
        assert result.passed, "Should pass with relaxed threshold"

        # Cleanup
        visual_regression.delete_baseline(test_name)

    def test_list_baselines(self, visual_regression):
        """Test listing baselines."""
        baselines = visual_regression.list_baselines()
        assert isinstance(baselines, dict), "Should return a dict"
