# TerminalDX12 Test Suite

This directory contains the test suite for TerminalDX12, consisting of:
- **C++ Unit Tests** - Fast, headless tests for core terminal logic (226 tests)
- **Python Integration Tests** - Visual tests using screenshots and OCR (72 tests)
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
  unit/                        # C++ unit tests (GTest)
    test_screen_buffer.cpp     # ScreenBuffer tests (61 tests)
    test_vt_parser.cpp         # VTStateMachine tests (68 tests)
    test_unicode.cpp           # Unicode handling tests (27 tests)
    test_performance.cpp       # Performance benchmarks (20 tests)
    contract/                  # Contract tests
      test_conpty_contract.cpp # ConPTY interface contract tests (35+ tests)
  baselines/                   # Visual regression baseline images
  test_terminal.py             # Main Python visual tests
  test_visual_regression.py    # Visual regression tests
  test_mouse.py                # Mouse interaction tests
  test_resize.py               # Window resize tests
  test_e2e.py                  # End-to-end workflow tests
  test_stress.py               # Stress and stability tests
  test_clipboard.py            # Clipboard copy/paste tests
  test_keyboard.py             # Keyboard shortcut tests
  test_colors.py               # ANSI color rendering tests
  test_attributes.py           # Text attribute tests (bold, underline, inverse)
  test_unicode.py              # Unicode visual rendering tests
  conftest.py                  # pytest fixtures
  config.py                    # Test configuration
  helpers.py                   # Extracted helper classes
  visual_regression.py         # Visual regression testing utilities
  requirements.txt             # Python dependencies
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
| test_screen_buffer.cpp | 72 | Buffer ops, cursor, scrolling, resize, attributes, backspace |
| test_vt_parser.cpp | 100 | Escape sequences, colors, cursor movement, OSC, mouse modes |
| test_config.cpp | 23 | Configuration parsing, validation, keybindings |
| test_unicode.cpp | 31 | CJK, emoji, box drawing, symbols, edge cases |

**Total: 230 C++ unit tests**

## Contract Tests

Contract tests verify the ConPTY interface behavior according to Windows API expectations.

### Running Contract Tests

```bash
# Run contract tests only
cd build/tests/Release
./TerminalDX12ContractTests.exe

# Or via CTest
ctest -R Contract --output-on-failure -C Release
```

### Contract Test Coverage

| Contract | Tests | Description |
|----------|-------|-------------|
| Session Lifecycle | 7 | Start/Stop/Restart behavior |
| Input Handling | 4 | WriteInput validation |
| Output Handling | 4 | Output callback contracts |
| Resize Operations | 5 | Resize behavior and edge cases |
| Process Exit | 3 | Exit callback and exit codes |
| Terminal Dimensions | 3 | Dimension validation |
| Unicode Support | 2 | UTF-8 input/output |
| Stress/Edge Cases | 3 | Rapid calls, large data, restart |

**Total: 31 contract tests**

### Contract Test Format

Tests follow Given/When/Then BDD format:

```cpp
/**
 * CONTRACT: Start() with valid command returns true and sets IsRunning()
 *
 * Given: A new ConPtySession instance
 * When: Start() is called with a valid command
 * Then: Returns true and IsRunning() returns true
 */
TEST_F(ConPtyContractTest, Start_ValidCommand_ReturnsTrue) {
    EXPECT_FALSE(session->IsRunning());
    bool result = session->Start(L"cmd.exe", 80, 24);
    EXPECT_TRUE(result);
    EXPECT_TRUE(session->IsRunning());
}
```

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

## Visual Regression Testing

Visual regression tests compare screenshots against stored baselines to detect unintended rendering changes.

### Running Visual Regression Tests

```bash
# Compare against baselines
pytest test_visual_regression.py -v

# Update baselines (when changes are intentional)
pytest test_visual_regression.py -v --update-baselines

# Adjust threshold (default 0.1%)
pytest test_visual_regression.py -v --visual-threshold=0.5
```

### How It Works

1. **First run**: Creates baseline images in `tests/baselines/`
2. **Subsequent runs**: Compares current screenshots against baselines
3. **On failure**: Saves diff images to `screenshots/diffs/` showing changes
4. **Update baselines**: Use `--update-baselines` when visual changes are intentional

### Test Coverage

| Test | Description | Threshold |
|------|-------------|-----------|
| `startup_screen` | Terminal startup rendering | 0.1% |
| `basic_text` | Text character rendering | 0.1% |
| `ansi_colors` | ANSI color output | 0.1% |
| `unicode_chars` | Unicode character rendering | 0.1% |
| `cursor` | Cursor rendering | 1.0% (blink) |
| `scrollback` | Scrollback buffer | 0.1% |
| `prompt` | Command prompt | 5.0% (path varies) |

### Baseline Management

```python
# In tests
from visual_regression import VisualRegressionTester

tester = VisualRegressionTester()

# List all baselines
baselines = tester.list_baselines()

# Delete a baseline
tester.delete_baseline("test_name")

# Clean up old diff images
tester.cleanup_diffs(max_age_hours=24)
```

## Helper Classes

The `helpers.py` module provides reusable components:

- **ScreenAnalyzer** - Color detection, text presence analysis
- **KeyboardController** - Keyboard input simulation
- **OCRVerifier** - OCR-based text verification
- **WindowHelper** - Window management utilities
- **TerminalTester** - Main test driver class
- **VisualRegressionTester** - Visual regression baseline comparison

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

### C++ Unit Tests: 230/230 passing (100%)

All C++ unit tests pass successfully, covering VT parser, screen buffer, Unicode, and performance.

### Python Integration Tests: 72/72 passing (100%)

Visual tests covering clipboard, keyboard shortcuts, ANSI colors, text attributes, Unicode rendering, mouse input, scrolling, window resize, E2E workflows, and stress testing.

### PowerShell Smoke Tests: 4/4 passing (100%)

Build validation and stability tests for executable, dependencies, and multi-instance scenarios.
