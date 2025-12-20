# TerminalDX12 Test Suite

This directory contains the test suite for TerminalDX12, consisting of:
- **C++ Unit Tests** - Fast, headless tests for core terminal logic
- **Python Integration Tests** - Visual tests that verify rendering

## Test Structure

```
tests/
  unit/                    # C++ unit tests (GTest)
    test_screen_buffer.cpp # ScreenBuffer tests (~55 tests)
    test_vt_parser.cpp     # VTStateMachine tests (~63 tests)
    test_unicode.cpp       # Unicode handling tests (~20 tests)
    test_performance.cpp   # Performance benchmarks (~15 tests)
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
| test_screen_buffer.cpp | ~55 | Buffer ops, cursor, scrolling, resize, attributes |
| test_vt_parser.cpp | ~63 | Escape sequences, colors, cursor movement, OSC |
| test_unicode.cpp | ~20 | CJK, emoji, combining chars, edge cases |
| test_performance.cpp | ~15 | Throughput, stress tests, memory |

## Python Integration Tests

Python tests use pytest and **require a graphical display** (Windows only).

### Setup

```bash
cd tests
pip install -r requirements.txt
```

### Running Python Tests

```bash
# Run all tests (shared terminal session - fast)
pytest -v

# Run with isolated terminal per test (slower but more reliable)
pytest -v --isolated

# Run specific test categories
pytest -v -m color      # Color rendering tests
pytest -v -m input      # Keyboard input tests
pytest -v -m scrolling  # Scrollback tests
pytest -v -m "not slow" # Skip slow tests

# Custom terminal path
pytest -v --terminal-exe "C:\path\to\TerminalDX12.exe"
```

### Legacy Runner

The original test runner is still available:
```bash
python test_terminal.py
```

### Test Markers

- `slow` - Long-running tests
- `color` - Color rendering verification
- `input` - Keyboard/mouse input tests
- `scrolling` - Scrollback buffer tests
- `attributes` - Text attribute tests (bold, underline)
- `ocr` - Tests requiring OCR (optional)

### Configuration

Tests can be configured via environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `TERMINAL_EXE` | `C:\Temp\TerminalDX12Test\TerminalDX12.exe` | Path to terminal |
| `SCREENSHOT_DIR` | `./screenshots` | Screenshot output dir |
| `STABILITY_TIME` | `0.3` | Screen stability wait (seconds) |
| `MAX_WAIT` | `5.0` | Max wait for conditions (seconds) |
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

1. Create `tests/test_*.py`
2. Use the `terminal` fixture:
   ```python
   def test_something(terminal):
       terminal.send_keys("echo hello\n")
       screenshot, _ = terminal.wait_and_screenshot("test")
       assert terminal.analyze_text_presence(screenshot)
   ```

## Helper Classes

The `helpers.py` module provides reusable components:

- **ScreenAnalyzer** - Color detection, text presence analysis
- **KeyboardController** - Keyboard input simulation
- **OCRVerifier** - OCR-based text verification
- **WindowHelper** - Window management utilities

These are available as pytest fixtures:
```python
def test_with_helpers(terminal, screen_analyzer, ocr_verifier):
    screenshot, _ = terminal.wait_and_screenshot("test")
    assert screen_analyzer.find_red_pixels(screenshot) > 50
```

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
