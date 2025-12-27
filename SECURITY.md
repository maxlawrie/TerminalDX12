# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |

## Reporting a Vulnerability

If you discover a security vulnerability in TerminalDX12, please report it responsibly:

1. **Do NOT open a public issue** for security vulnerabilities
2. **Email**: Create a private issue or contact the maintainers directly
3. **Provide details**: Include steps to reproduce, impact assessment, and any proof-of-concept

We will acknowledge receipt within 48 hours and provide an initial assessment within 7 days.

## Security Considerations

### Terminal Emulator Attack Surface

As a terminal emulator, TerminalDX12 processes untrusted input from shell output and remote connections. Key security considerations:

#### Escape Sequence Processing

- **OSC 52 (Clipboard)**: By default, OSC 52 is configured for write-only mode to prevent clipboard exfiltration attacks. Applications can write to the clipboard but cannot read from it without explicit user action.
- **Hyperlinks (OSC 8)**: URLs in hyperlinks are displayed on hover and require Ctrl+click to open, preventing accidental navigation.
- **Title Setting**: OSC sequences that set window titles are sanitized to prevent injection attacks.

#### Input Handling

- **Win32 Input Mode**: When enabled, keyboard input is sent to the shell in a structured format, reducing escape sequence injection risks.
- **Command Injection**: User input is passed through ConPTY, which handles proper escaping.

### DirectX 12 Rendering

- Shaders are compiled at build time, not loaded from external files
- No dynamic shader compilation from untrusted sources
- GPU resources are properly bounded and validated

### Process Isolation

- Shell processes run with the same privileges as the terminal application
- ConPTY provides Windows-native process isolation
- No elevated privilege operations are performed

## Security Best Practices for Users

1. **Keep Windows updated**: ConPTY security improvements are delivered through Windows updates
2. **Be cautious with untrusted output**: Malicious terminal sequences could attempt to exploit parsing bugs
3. **Verify URLs before clicking**: Hyperlinks in terminal output can disguise malicious URLs
4. **Use clipboard write-only mode**: This is the default; avoid enabling read mode unless necessary

## Hardening Options

### OSC 52 Clipboard Policy

Configure clipboard access in `Config.h`:

```cpp
enum class Osc52Policy {
    Disabled,   // Block all OSC 52 operations
    ReadOnly,   // Allow clipboard queries only (security risk)
    WriteOnly,  // Allow clipboard writes only (default, recommended)
    ReadWrite   // Allow both operations
};
```

### Future Hardening (Planned)

- Configurable escape sequence filtering
- Sandboxed shell process option
- Audit logging for suspicious sequences

## Acknowledgments

We appreciate security researchers who help improve TerminalDX12. Responsible disclosure will be acknowledged in release notes (unless anonymity is requested).
