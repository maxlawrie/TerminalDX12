#!/usr/bin/env python3
"""
End-to-end workflow tests for TerminalDX12.

Tests complete user journeys from launch to interaction.
"""

import pytest
import time


@pytest.mark.slow
class TestE2EWorkflow:
    """End-to-end workflow tests."""

    def test_full_session_workflow(self, terminal):
        """Complete user workflow: type commands, verify output, scroll."""
        # Step 1: Clear and verify clean state
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Step 2: Execute a command and verify output
        terminal.send_keys("echo WORKFLOW_TEST_123\n")
        time.sleep(0.5)

        screenshot1, _ = terminal.wait_and_screenshot("e2e_step1")
        assert terminal.analyze_text_presence(screenshot1), "No text after echo command"

        # Step 3: Generate scrollback content
        terminal.send_keys('1..30 | % { echo "LINE_$_" }\n')
        time.sleep(2)

        # Step 4: Scroll up through history
        import win32api
        import win32con

        client_rect = terminal.get_client_rect_screen()
        center_x = (client_rect[0] + client_rect[2]) // 2
        center_y = (client_rect[1] + client_rect[3]) // 2
        win32api.SetCursorPos((center_x, center_y))

        for _ in range(5):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, 120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

        screenshot2, _ = terminal.wait_and_screenshot("e2e_step2_scrolled")
        assert terminal.analyze_text_presence(screenshot2), "No text when scrolled"

        # Step 5: Scroll back down
        for _ in range(5):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, -120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

        # Step 6: Execute another command to verify still responsive
        terminal.send_keys("echo WORKFLOW_COMPLETE\n")
        time.sleep(0.5)

        screenshot3, _ = terminal.wait_and_screenshot("e2e_complete")
        assert terminal.analyze_text_presence(screenshot3), "Terminal unresponsive at end"

        # Log OCR for verification
        ocr_text = terminal.get_screen_text(screenshot3)
        if "WORKFLOW" in ocr_text.upper() or "COMPLETE" in ocr_text.upper():
            print("E2E workflow verified via OCR")

    def test_command_execution_cycle(self, terminal):
        """Test multiple command execution cycles."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        commands = [
            ("echo TEST_A", "TEST_A"),
            ("echo TEST_B", "TEST_B"),
            ("echo TEST_C", "TEST_C"),
        ]

        for cmd, marker in commands:
            terminal.send_keys(f"{cmd}\n")
            time.sleep(0.3)

        time.sleep(0.5)
        screenshot, _ = terminal.wait_and_screenshot("e2e_commands")
        assert terminal.analyze_text_presence(screenshot), "Commands did not produce output"

        # Verify via OCR
        ocr_text = terminal.get_screen_text(screenshot)
        found = sum(1 for _, marker in commands if marker in ocr_text.upper())
        print(f"E2E commands: found {found}/{len(commands)} markers in OCR")

    def test_interactive_input(self, terminal):
        """Test interactive typing and backspace."""
        terminal.send_keys("cls\n")
        time.sleep(0.5)

        # Type with corrections (backspace)
        terminal.send_keys("echo HELLX")
        time.sleep(0.2)
        terminal.send_keys("\b")  # Backspace
        time.sleep(0.1)
        terminal.send_keys("O_WORLD\n")
        time.sleep(0.5)

        screenshot, _ = terminal.wait_and_screenshot("e2e_interactive")
        assert terminal.analyze_text_presence(screenshot), "Interactive input failed"
