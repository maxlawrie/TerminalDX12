# Changelog

All notable changes to TerminalDX12 are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Multi-frame deferred resize for stability during window maximize
- ConPTY contract tests for Windows pseudo-terminal validation
- Visual regression testing infrastructure
- Code coverage integration in CI workflow
- Doxygen API documentation generation

### Fixed
- Crash on window maximize while TUI apps (nano, vim) are running
- Thread safety: Added mutex locks to scroll region operations (SetScrollRegion, ResetScrollRegion, ScrollRegionUp, ScrollRegionDown) to prevent race conditions when VT parser and resize occur concurrently
- DX12 swap chain resize timing issues with triple buffering

## [0.1.0] - 2025-12-22

### Added

#### Terminal Emulation
- VT100/VT220 escape sequence support
- ANSI mode parsing (SM/RM - CSI n h/l)
- Origin Mode (DECOM) for scroll region-relative cursor
- Cursor blink mode and cursor style control (block, underline, bar)
- Blink and hidden SGR text attributes
- Double underline and underline style support

#### Text Rendering
- GPU-accelerated text rendering via DirectX 12
- FreeType font rasterization with glyph atlas caching
- 256-color and true color (24-bit RGB) support
- Bold, italic, underline, strikethrough text styles

#### Selection & Clipboard
- Mouse text selection with double-click word select
- Triple-click line selection
- Alt+drag rectangle/block selection mode
- OSC 52 clipboard integration (read/write)
- Middle-button paste support

#### Hyperlinks & URLs
- OSC 8 hyperlink support with Ctrl+click to open
- Automatic URL detection in terminal output

#### Colors
- OSC 4 dynamic color palette modification
- OSC 10/11 foreground/background color queries
- Configurable 16-color ANSI palette

#### Keyboard & Input
- Win32 input mode for proper key handling
- Function keys F1-F12 support
- Ctrl+1-9 tab switching
- Alt+Arrow pane navigation

#### UI Features
- Tab management with multiple terminal sessions
- Split pane support (horizontal/vertical)
- Click-to-focus pane switching
- Pane zoom (maximize focused pane)
- In-terminal search with Ctrl+F

#### Mouse Features
- SGR mouse reporting mode
- Mouse button and motion tracking
- Scroll wheel support with configurable multiplier

### Technical
- ConPTY integration for Windows pseudo-terminal
- Triple-buffered swap chain for smooth rendering
- Mutex-protected screen buffer for thread safety
- Deferred resize handling for stability

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on contributing to this project.
