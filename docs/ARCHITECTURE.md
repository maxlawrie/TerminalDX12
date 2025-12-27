# TerminalDX12 Architecture

This document describes the high-level architecture of TerminalDX12.

## Overview

TerminalDX12 is a GPU-accelerated terminal emulator for Windows that uses DirectX 12 for rendering. The architecture follows a modular design with clear separation of concerns.

## Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                           Application                                │
│                    (Core::Application)                               │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │                     Event Loop                               │    │
│  │  ProcessMessages() → Update() → Render()                     │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────┘
          │                    │                    │
          ▼                    ▼                    ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│     Window      │  │   Tab Manager   │  │   DX12 Renderer │
│  (Core::Window) │  │ (UI::TabManager)│  │   (Rendering)   │
│                 │  │                 │  │                 │
│ - Win32 HWND    │  │ - Multiple tabs │  │ - Device/Queue  │
│ - DPI awareness │  │ - Tab switching │  │ - Swap chain    │
│ - Input events  │  │ - New tab/close │  │ - Text renderer │
└─────────────────┘  └────────┬────────┘  └─────────────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │      Tab        │
                    │   (UI::Tab)     │
                    │                 │
                    │ Owns per-tab:   │
                    └────────┬────────┘
                             │
          ┌──────────────────┼──────────────────┐
          ▼                  ▼                  ▼
┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐
│  ConPTY Session │  │  Screen Buffer  │  │ VT State Machine│
│ (Pty::ConPty)   │  │(Terminal::Scrn) │  │ (Terminal::VT)  │
│                 │  │                 │  │                 │
│ - Pseudo-console│  │ - Cell grid     │  │ - ANSI parsing  │
│ - Shell process │  │ - Scrollback    │  │ - Escape codes  │
│ - I/O pipes     │  │ - Cursor state  │  │ - SGR attributes│
└─────────────────┘  └─────────────────┘  └─────────────────┘
```

## Component Details

### Core Layer

#### Application (`Core::Application`)
The main coordinator class that:
- Initializes all subsystems
- Runs the main event loop
- Handles window events (resize, keyboard, mouse)
- Manages text selection and clipboard
- Coordinates rendering

#### Window (`Core::Window`)
Win32 window management:
- Creates HWND with proper class registration
- Per-monitor DPI awareness V2
- Routes Win32 messages to event callbacks
- Tracks resize state for smooth handling

### Rendering Layer

#### DX12Renderer (`Rendering::DX12Renderer`)
DirectX 12 rendering pipeline:
- Device and adapter management
- Triple-buffered swap chain (FLIP_DISCARD)
- Frame synchronization with fences
- Coordinates text and UI rendering

#### TextRenderer (`Rendering::TextRenderer`)
GPU-accelerated text rendering:
- FreeType font rasterization
- Glyph atlas management (texture cache)
- Instanced rendering for performance
- Supports bold, italic, underline, strikethrough

### Terminal Layer

#### ScreenBuffer (`Terminal::ScreenBuffer`)
Terminal screen state:
- 2D grid of cells with attributes
- Scrollback buffer (configurable size)
- Cursor position and visibility
- Dirty region tracking for efficient updates

#### VTStateMachine (`Terminal::VTStateMachine`)
VT/ANSI escape sequence parser:
- Processes CSI, OSC, DCS sequences
- Implements SGR (colors, attributes)
- Cursor movement and screen manipulation
- Mouse reporting modes

### PTY Layer

#### ConPtySession (`Pty::ConPtySession`)
Windows Pseudo Console (ConPTY) integration:
- Creates pseudo-console for shell process
- Spawns shell (powershell.exe, cmd.exe, etc.)
- Bidirectional I/O via pipes
- Handles terminal resize notifications

### UI Layer

#### TabManager (`UI::TabManager`)
Multiple terminal tab support:
- Creates and destroys tabs
- Switches between active tabs
- Tab bar rendering

## Data Flow

### Input Flow
```
Win32 Message → Window → Application → ConPtySession → Shell
                              ↓
                    VTStateMachine (if needed)
```

### Output Flow
```
Shell → ConPtySession → VTStateMachine → ScreenBuffer → TextRenderer → Display
```

### Resize Flow
```
WM_SIZE → Window → Application → ConPtySession (resize PTY)
                       ↓
              ScreenBuffer (reflow text)
                       ↓
              DX12Renderer (resize buffers)
```

## Threading Model

- **Main Thread**: Window messages, rendering, UI
- **PTY Read Thread**: Reads shell output, calls VT parser which writes to ScreenBuffer
- **Rendering**: Runs on main thread

### Thread Safety in ScreenBuffer

The `ScreenBuffer` uses a `std::recursive_mutex` to protect concurrent access from:
- Main thread: Reading cells for rendering, handling resize
- PTY thread: Writing cells via VT parser

**Critical invariant**: When resizing the buffer, the buffer must be swapped BEFORE
dimensions are updated. This prevents a race condition where:

```cpp
// WRONG - causes buffer overrun crash:
m_cols = newCols;    // Thread B sees new dimensions
m_rows = newRows;    // Thread B passes bounds check with new dims
m_buffer = newBuf;   // But buffer is still old size → crash!

// CORRECT - safe ordering:
m_buffer = newBuf;   // Buffer swapped first (larger)
m_cols = newCols;    // Now dimensions match buffer
m_rows = newRows;
```

This bug manifested as crashes when maximizing the window with TUI apps (nano, vim)
running, because:
1. Main thread started resize, updated dimensions
2. PTY thread wrote to cell position valid for new dimensions
3. But old smaller buffer was still in place → buffer overrun

### Cell Access Safety

`GetCellWithScrollback()` returns cells by value (not reference) to prevent dangling
references when the buffer is reallocated during resize. Additional bounds checking
against the actual buffer size provides defense-in-depth.

## Memory Management

- Smart pointers (`std::unique_ptr`, `std::shared_ptr`) for ownership
- RAII for DirectX 12 resources (ComPtr)
- Glyph atlas uses LRU eviction when full
- Scrollback buffer is ring-buffer based

## Key Design Decisions

1. **DirectX 12 over 11**: Better CPU parallelism, lower driver overhead
2. **ConPTY over legacy**: Modern API, proper VT support
3. **Single-threaded rendering**: Simpler, avoids GPU sync issues
4. **Tab-per-session**: Each tab has independent terminal state
5. **Instanced glyph rendering**: Efficient for thousands of glyphs
