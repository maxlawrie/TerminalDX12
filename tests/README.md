# TerminalDX12 Test Suite

Automated tests for the TerminalDX12 terminal emulator with screenshot capture and analysis.

## Features

- **Automated Testing**: Launches terminal and tests functionality
- **Screenshot Capture**: Takes screenshots at each test step
- **Image Analysis**: Verifies colors, text rendering, and visual attributes
- **Comprehensive Coverage**: Tests VT100 sequences, colors, scrollback, and more

## Setup

1. Install Python 3.8 or higher
2. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Running Tests

### Run all tests:
```bash
python test_terminal.py
```

### Test Results

Tests will:
- Launch the terminal automatically
- Send keyboard input to test various features
- Capture screenshots at each step
- Analyze screenshots for expected content
- Generate a test report

Screenshots are saved to `screenshots/` directory with descriptive names.

## Test Coverage

### Basic Functionality
- ✓ Terminal startup and initialization
- ✓ Keyboard input handling
- ✓ Text rendering

### VT100/ANSI Features
- ✓ Foreground colors (16-color palette)
- ✓ Background colors
- ✓ Bold text attribute
- ✓ Underline attribute
- ✓ Clear screen command

### Advanced Features
- ✓ Scrollback buffer navigation
- ✓ Directory listing with colors
- ✓ Cursor blinking animation

## Screenshot Analysis

The test suite uses image analysis to verify:
- **Color Detection**: Checks for specific RGB values in screenshots
- **Text Presence**: Verifies text is being rendered
- **Visual Differences**: Detects changes (for scrollback, cursor blink)

## Example Output

```
============================================================
Running test: Basic Startup
============================================================
Screenshot saved: screenshots/01_basic_startup.png
✓ Basic Startup: PASSED

============================================================
Running test: Color Rendering
============================================================
Screenshot saved: screenshots/03_color_red.png
Screenshot saved: screenshots/03_color_green.png
Screenshot saved: screenshots/03_color_blue.png
✓ Color Rendering: PASSED

...

============================================================
TEST SUMMARY
============================================================
✓ PASS: Basic Startup
✓ PASS: Keyboard Input
✓ PASS: Color Rendering
✓ PASS: Background Colors
✓ PASS: Clear Screen
✓ PASS: Directory Listing
✓ PASS: Scrollback Buffer
✓ PASS: Underline Attribute
✓ PASS: Bold Text
✓ PASS: Cursor Blinking

Results: 10/10 tests passed (100%)
Screenshots saved to: tests/screenshots
```

## Troubleshooting

### Terminal doesn't start
- Check that `TERMINAL_EXE` path is correct in `test_terminal.py`
- Ensure the terminal executable is built and available

### Screenshots are blank
- Make sure the terminal window is visible and in foreground
- Check display scaling settings (may affect screenshot capture)

### Color detection fails
- Adjust the `tolerance` value in `analyze_colors()` method
- Check the RGB values match your color palette

## Extending Tests

To add new tests:

1. Create a new test method in the `TerminalTester` class:
   ```python
   def test_new_feature(self):
       """Test description"""
       # Test code here
       self.send_keys("command")
       screenshot, _ = self.take_screenshot("test_name")
       # Analysis code
       return True  # or False
   ```

2. Add it to the test runner in `main()`:
   ```python
   tester.run_test("New Feature", tester.test_new_feature)
   ```

## Notes

- Tests run sequentially in a single terminal session
- Each test builds on previous state (use `cls` to clear if needed)
- Screenshots are overwritten on each test run
- Timing delays may need adjustment on slower systems
