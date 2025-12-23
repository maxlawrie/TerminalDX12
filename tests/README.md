# TerminalDX12 Test Suite

This directory contains the test suite for TerminalDX12, consisting of:
- **C++ Unit Tests** - Fast, headless tests for core terminal logic (176 tests)
- **Python Integration Tests** - Visual tests using screenshots and OCR (7 tests)
- **PowerShell Smoke Tests** - Build artifact and stability tests (4 tests)

## Quick Start

### Run All Tests

**From WSL (recommended):**
```bash
# Run C++ unit tests, then Python integration tests
cd /home/maxla/TerminalDX12/build && ctest --output-on-failure -C Release && cmd.exe /c "cd /d C:\\Temp\\TerminalDX12Test && python test_terminal.py"
```

### Run C++ Unit Tests Only

```bash
cd /home/maxla/TerminalDX12/build
ctest --output-on-failure -C Release
```

With verbose output:
```bash
ctest --output-on-failure -C Release -V
```

### Run Python Integration Tests Only

**From Windows CMD/PowerShell:**
```cmd
cd C:\Temp\TerminalDX12Test
python test_terminal.py
```

**From WSL:**
```bash
cmd.exe /c "cd /d C:\\Temp\\TerminalDX12Test && python test_terminal.py"
```

> **Note:** Python integration tests require a Windows display and will launch the terminal application visually. C++ unit tests run headless.

## Test Structure

```
tests/
  unit/                    # C++ unit tests (GTest)
    test_screen_buffer.cpp # ScreenBuffer tests (61 tests)
    test_vt_parser.cpp     # VTStateMachine tests (68 tests)
    test_unicode.cpp       # Unicode handling tests (27 tests)
    test_performance.cpp   # Performance benchmarks (20 tests)
  test_terminal.py         # Main Python visual tests (22 tests)
  test_mouse.py            # Mouse interaction tests
  test_resize.py           # Window resize tests
  conftest.py              # pytest fixtures
  config.py                # Test configuration
  helpers.py               # Extracted helper classes
  requirements.txt         # Python dependencies
```

## C++ Unit Tests

C++ tests use GoogleTest and can run **headless** (suitable for CI).

### Running C++ Tests

```bash
# From project root
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
cd build
ctest --output-on-failure -C Release
```

### Test Coverage

| File | Tests | Coverage |
|------|-------|----------|
| test_screen_buffer.cpp | 61 | Buffer ops, cursor, scrolling, resize, attributes, backspace |
| test_vt_parser.cpp | 68 | Escape sequences, colors, cursor movement, OSC, backspace |
| test_unicode.cpp | 27 | CJK, emoji, box drawing, symbols, edge cases |
| test_performance.cpp | 20 | Throughput, stress tests, memory |

**Total: 176 C++ unit tests**

## Python Integration Tests

Python tests **require a graphical display** (Windows only) and use OCR to verify rendered output.

### Setup

```bash
cd C:\Temp\TerminalDX12Test
pip install -r requirements.txt
```

Required packages: `pillow`, `pytesseract`, `pywin32`

### Running Python Tests

**Main test runner (22 visual tests):**
```bash
cd C:\Temp\TerminalDX12Test
python test_terminal.py
```

**From WSL:**
```bash
cmd.exe /c "cd /d C:\\Temp\\TerminalDX12Test && python test_terminal.py"
```

### Test Output

- Screenshots are saved to `screenshots/` directory
- Test log saved to `test_output.txt`
- Each test captures before/after screenshots for debugging

### Configuration

The test runner uses `config.py` for settings. Key configuration:

| Setting | Default | Description |
|---------|---------|-------------|
| `TERMINAL_EXE` | `C:\Temp\TerminalDX12Test\TerminalDX12.exe` | Path to terminal |
| `SCREENSHOT_DIR` | `./screenshots` | Screenshot output dir |
| `COLOR_TOLERANCE` | `50` | RGB tolerance for color matching |

## CI/CD

GitHub Actions runs **C++ unit tests** automatically on push/PR.

**Python visual tests require a display** and are intended for local development only. They cannot run in headless CI environments.

See `.github/workflows/tests.yml` for CI configuration.

## Adding New Tests

### C++ Unit Tests

1. Add test file to `tests/unit/`
2. Register in `tests/CMakeLists.txt`:
   ```cmake
   set(TEST_SOURCES
       unit/test_new_file.cpp
       # ... existing files
   )
   ```

### Python Tests

Add new tests to `test_terminal.py` in the `run_tests()` method:

```python
def test_my_feature(self):
    """Test description."""
    self.send_keys("echo test\n")
    time.sleep(0.5)
    screenshot, _ = self.take_screenshot("my_feature")

    # Use OCR to verify text
    ocr_text = self.ocr_screenshot(screenshot)
    assert "test" in ocr_text.lower()

    # Or check for specific colors
    red_pixels = self.find_color_pixels(screenshot, (205, 49, 49), tolerance=50)
    assert red_pixels > 100
```

## Helper Classes

The `helpers.py` module provides reusable components:

- **ScreenAnalyzer** - Color detection, text presence analysis
- **KeyboardController** - Keyboard input simulation
- **OCRVerifier** - OCR-based text verification
- **WindowHelper** - Window management utilities

## Troubleshooting

### Terminal doesn't start
- Check that `TERMINAL_EXE` path is correct (or set via environment variable)
- Ensure the terminal executable is built and available

### Screenshots are blank
- Make sure the terminal window is visible and in foreground
- Check display scaling settings (may affect screenshot capture)

### Color detection fails
- Adjust `COLOR_TOLERANCE` environment variable
- Check the RGB values match your color palette

### Tests timeout
- Increase `MAX_WAIT` environment variable
- Check for slow system performance

## Notes

- Python tests run sequentially for shared terminal session
- Use `cls` command to clear screen between tests if needed
- Screenshots are saved to `screenshots/` directory
- Timing may need adjustment on slower systems

## Current Test Status

### C++ Unit Tests: 176/176 passing (100%)

All C++ unit tests pass successfully, covering VT parser, screen buffer, Unicode, and performance.

### Python Integration Tests: 7/7 passing (100%)

Visual tests covering mouse input, scrolling, and window resize operations.

### PowerShell Smoke Tests: 4/4 passing (100%)

Build validation and stability tests for executable, dependencies, and multi-instance scenarios.
