#!/usr/bin/env python3
"""
Visual Regression Testing for TerminalDX12.

Provides infrastructure for comparing screenshots against baselines
to detect unintended visual changes in terminal rendering.
"""

from typing import Tuple, Optional, Dict, Any
from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter
import numpy as np
import hashlib
import json
import time
import os

from config import TestConfig

__all__ = [
    'VisualRegressionTester',
    'VisualRegressionResult',
    'BASELINES_DIR',
    'DIFFS_DIR',
]

# Directories
BASELINES_DIR = Path(__file__).parent / "baselines"
DIFFS_DIR = TestConfig.SCREENSHOT_DIR / "diffs"


class VisualRegressionResult:
    """Result of a visual regression comparison."""

    def __init__(
        self,
        passed: bool,
        diff_percentage: float,
        diff_pixels: int,
        total_pixels: int,
        baseline_path: Optional[Path] = None,
        actual_path: Optional[Path] = None,
        diff_path: Optional[Path] = None,
        message: str = ""
    ):
        self.passed = passed
        self.diff_percentage = diff_percentage
        self.diff_pixels = diff_pixels
        self.total_pixels = total_pixels
        self.baseline_path = baseline_path
        self.actual_path = actual_path
        self.diff_path = diff_path
        self.message = message

    def __bool__(self) -> bool:
        return self.passed

    def __repr__(self) -> str:
        status = "PASS" if self.passed else "FAIL"
        return f"VisualRegressionResult({status}, diff={self.diff_percentage:.2f}%)"


class VisualRegressionTester:
    """
    Visual regression testing utility.

    Compares screenshots against stored baselines to detect visual changes.
    """

    def __init__(
        self,
        baselines_dir: Optional[Path] = None,
        diffs_dir: Optional[Path] = None,
        threshold: float = 0.1,
        blur_radius: int = 0,
        ignore_antialiasing: bool = True
    ):
        """
        Initialize visual regression tester.

        Args:
            baselines_dir: Directory for baseline images
            diffs_dir: Directory for diff images
            threshold: Maximum allowed difference percentage (0.0-100.0)
            blur_radius: Blur radius for anti-aliasing tolerance (0 = disabled)
            ignore_antialiasing: If True, ignore minor anti-aliasing differences
        """
        self.baselines_dir = baselines_dir or BASELINES_DIR
        self.diffs_dir = diffs_dir or DIFFS_DIR
        self.threshold = threshold
        self.blur_radius = blur_radius
        self.ignore_antialiasing = ignore_antialiasing

        # Ensure directories exist
        self.baselines_dir.mkdir(parents=True, exist_ok=True)
        self.diffs_dir.mkdir(parents=True, exist_ok=True)

        # Metadata file for baseline info
        self.metadata_file = self.baselines_dir / "metadata.json"
        self._metadata = self._load_metadata()

    def _load_metadata(self) -> Dict[str, Any]:
        """Load baseline metadata."""
        if self.metadata_file.exists():
            try:
                with open(self.metadata_file, 'r') as f:
                    return json.load(f)
            except (json.JSONDecodeError, IOError):
                return {}
        return {}

    def _save_metadata(self) -> None:
        """Save baseline metadata."""
        with open(self.metadata_file, 'w') as f:
            json.dump(self._metadata, f, indent=2)

    def _get_baseline_path(self, name: str) -> Path:
        """Get path for a baseline image."""
        return self.baselines_dir / f"{name}.png"

    def _get_diff_path(self, name: str) -> Path:
        """Get path for a diff image."""
        timestamp = int(time.time())
        return self.diffs_dir / f"{name}_diff_{timestamp}.png"

    def _get_actual_path(self, name: str) -> Path:
        """Get path for the actual (current) image."""
        timestamp = int(time.time())
        return self.diffs_dir / f"{name}_actual_{timestamp}.png"

    def _compute_hash(self, image: Image.Image) -> str:
        """Compute perceptual hash of an image."""
        # Resize to small size for hashing
        img = image.convert('L').resize((16, 16), Image.Resampling.LANCZOS)
        pixels = list(img.getdata())
        avg = sum(pixels) / len(pixels)
        bits = ''.join('1' if p > avg else '0' for p in pixels)
        return hashlib.md5(bits.encode()).hexdigest()[:16]

    def _preprocess_image(self, image: Image.Image) -> Image.Image:
        """Preprocess image for comparison."""
        # Convert to RGB if needed
        if image.mode != 'RGB':
            image = image.convert('RGB')

        # Apply blur if configured (helps with anti-aliasing)
        if self.blur_radius > 0:
            image = image.filter(ImageFilter.GaussianBlur(self.blur_radius))

        return image

    def _compute_diff(
        self,
        baseline: Image.Image,
        actual: Image.Image
    ) -> Tuple[Image.Image, int, int]:
        """
        Compute difference between two images.

        Returns:
            Tuple of (diff_image, diff_pixels, total_pixels)
        """
        # Ensure same size
        if baseline.size != actual.size:
            # Resize actual to match baseline
            actual = actual.resize(baseline.size, Image.Resampling.LANCZOS)

        # Preprocess
        baseline = self._preprocess_image(baseline)
        actual = self._preprocess_image(actual)

        # Compute difference
        diff = ImageChops.difference(baseline, actual)

        # Convert to numpy for analysis
        diff_array = np.array(diff)
        baseline_array = np.array(baseline)
        actual_array = np.array(actual)

        # Calculate per-pixel difference magnitude
        if self.ignore_antialiasing:
            # Use threshold to ignore minor differences (anti-aliasing)
            pixel_diff = np.sqrt(np.sum((baseline_array.astype(float) - actual_array.astype(float)) ** 2, axis=2))
            diff_mask = pixel_diff > 25  # Tolerance for anti-aliasing
        else:
            diff_mask = np.any(diff_array > 0, axis=2)

        diff_pixels = int(np.sum(diff_mask))
        total_pixels = baseline.size[0] * baseline.size[1]

        # Create visual diff image
        diff_visual = Image.new('RGB', baseline.size)
        diff_draw = ImageDraw.Draw(diff_visual)

        # Composite: baseline faded + red highlight on differences
        faded_baseline = Image.blend(
            baseline,
            Image.new('RGB', baseline.size, (128, 128, 128)),
            0.5
        )
        diff_visual = faded_baseline.copy()

        # Highlight differences in red
        diff_highlight = np.zeros((*baseline.size[::-1], 3), dtype=np.uint8)
        diff_highlight[diff_mask] = [255, 0, 0]
        diff_overlay = Image.fromarray(diff_highlight)
        diff_visual = Image.blend(diff_visual, diff_overlay, 0.5)

        return diff_visual, diff_pixels, total_pixels

    def compare(
        self,
        name: str,
        actual: Image.Image,
        threshold: Optional[float] = None
    ) -> VisualRegressionResult:
        """
        Compare a screenshot against its baseline.

        Args:
            name: Unique name for this visual test
            actual: Current screenshot to compare
            threshold: Override default threshold for this comparison

        Returns:
            VisualRegressionResult with comparison details
        """
        threshold = threshold if threshold is not None else self.threshold
        baseline_path = self._get_baseline_path(name)

        # Check if baseline exists
        if not baseline_path.exists():
            # No baseline - save actual as new baseline
            actual.save(baseline_path)
            self._metadata[name] = {
                'created': time.time(),
                'hash': self._compute_hash(actual),
                'size': actual.size
            }
            self._save_metadata()

            return VisualRegressionResult(
                passed=True,
                diff_percentage=0.0,
                diff_pixels=0,
                total_pixels=actual.size[0] * actual.size[1],
                baseline_path=baseline_path,
                message=f"New baseline created: {baseline_path}"
            )

        # Load baseline
        try:
            baseline = Image.open(baseline_path)
        except IOError as e:
            return VisualRegressionResult(
                passed=False,
                diff_percentage=100.0,
                diff_pixels=0,
                total_pixels=0,
                message=f"Failed to load baseline: {e}"
            )

        # Compute difference
        diff_image, diff_pixels, total_pixels = self._compute_diff(baseline, actual)
        diff_percentage = (diff_pixels / total_pixels) * 100 if total_pixels > 0 else 0

        passed = diff_percentage <= threshold

        # Save artifacts if failed
        actual_path = None
        diff_path = None

        if not passed:
            actual_path = self._get_actual_path(name)
            diff_path = self._get_diff_path(name)
            actual.save(actual_path)
            diff_image.save(diff_path)

        message = f"Diff: {diff_percentage:.2f}% ({diff_pixels}/{total_pixels} pixels)"
        if not passed:
            message += f" - exceeds threshold of {threshold}%"
            message += f"\n  Baseline: {baseline_path}"
            message += f"\n  Actual: {actual_path}"
            message += f"\n  Diff: {diff_path}"

        return VisualRegressionResult(
            passed=passed,
            diff_percentage=diff_percentage,
            diff_pixels=diff_pixels,
            total_pixels=total_pixels,
            baseline_path=baseline_path,
            actual_path=actual_path,
            diff_path=diff_path,
            message=message
        )

    def update_baseline(self, name: str, image: Image.Image) -> Path:
        """
        Update or create a baseline image.

        Args:
            name: Unique name for this baseline
            image: New baseline image

        Returns:
            Path to saved baseline
        """
        baseline_path = self._get_baseline_path(name)
        image.save(baseline_path)

        self._metadata[name] = {
            'created': time.time(),
            'updated': time.time(),
            'hash': self._compute_hash(image),
            'size': image.size
        }
        self._save_metadata()

        return baseline_path

    def delete_baseline(self, name: str) -> bool:
        """
        Delete a baseline image.

        Args:
            name: Name of baseline to delete

        Returns:
            True if deleted, False if not found
        """
        baseline_path = self._get_baseline_path(name)
        if baseline_path.exists():
            baseline_path.unlink()
            if name in self._metadata:
                del self._metadata[name]
                self._save_metadata()
            return True
        return False

    def list_baselines(self) -> Dict[str, Dict[str, Any]]:
        """
        List all baselines with metadata.

        Returns:
            Dict mapping baseline names to their metadata
        """
        baselines = {}
        for path in self.baselines_dir.glob("*.png"):
            name = path.stem
            baselines[name] = self._metadata.get(name, {
                'path': str(path),
                'size': Image.open(path).size if path.exists() else None
            })
        return baselines

    def cleanup_diffs(self, max_age_hours: float = 24) -> int:
        """
        Clean up old diff images.

        Args:
            max_age_hours: Maximum age of diff images to keep

        Returns:
            Number of files deleted
        """
        deleted = 0
        cutoff = time.time() - (max_age_hours * 3600)

        for path in self.diffs_dir.glob("*.png"):
            if path.stat().st_mtime < cutoff:
                path.unlink()
                deleted += 1

        return deleted
