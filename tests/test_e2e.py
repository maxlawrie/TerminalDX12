#!/usr/bin/env python3
"""
End-to-end workflow tests for TerminalDX12.

Tests complete user journeys from launch to interaction.
"""

import pytest
import time
import win32api
import win32con


@pytest.mark.slow
class TestE2EWorkflow:
    """End-to-end workflow tests."""

    def _scroll_wheel(self, terminal, direction: int, count: int = 5):
        """Scroll mouse wheel in terminal window."""
        rect = terminal.get_client_rect_screen()
        center = ((rect[0] + rect[2]) // 2, (rect[1] + rect[3]) // 2)
        win32api.SetCursorPos(center)
        for _ in range(count):
            win32api.mouse_event(win32con.MOUSEEVENTF_WHEEL, 0, 0, direction * 120, 0)
            time.sleep(0.1)
        time.sleep(0.3)

    def test_full_session_workflow(self, terminal):
        """Complete user workflow: type commands, verify output, scroll."""
        # Step 1: Execute command
        terminal.send_command("echo WORKFLOW_TEST_123")
        terminal.assert_renders("e2e_step1")

        # Step 2: Generate scrollback
        terminal.send_command('1..30 | % { echo "LINE_$_" }', wait=2)

        # Step 3: Scroll up and down
        self._scroll_wheel(terminal, 1)
        terminal.assert_renders("e2e_step2_scrolled")

        self._scroll_wheel(terminal, -1)

        # Step 4: Verify still responsive
        terminal.send_command("echo WORKFLOW_COMPLETE")
        terminal.assert_renders("e2e_complete", "WORKFLOW")

    def test_command_execution_cycle(self, terminal):
        """Test multiple command execution cycles."""
        commands = ["TEST_A", "TEST_B", "TEST_C"]
        for cmd in commands:
            terminal.send_command(f"echo {cmd}", wait=0.3)

        screenshot = terminal.assert_renders("e2e_commands")
        ocr = terminal.get_screen_text(screenshot)
        found = sum(1 for c in commands if c in ocr.upper())
        print(f"E2E commands: found {found}/{len(commands)} markers")

    def test_interactive_input(self, terminal):
        """Test interactive typing and backspace."""
        terminal.send_keys("echo HELLX")
        time.sleep(0.2)
        terminal.send_keys("\b")
        time.sleep(0.1)
        terminal.send_command("O_WORLD")
        terminal.assert_renders("e2e_interactive", "HELLO")
