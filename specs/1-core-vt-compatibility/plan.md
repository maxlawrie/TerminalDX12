# Implementation Plan: Core VT Compatibility

**Branch**: `1-core-vt-compatibility` | **Date**: 2024-12-21 | **Spec**: [spec.md](spec.md)

## Summary

Implement complete VT100/ANSI escape sequence support for Claude Code, vim, and htop compatibility. Key changes include:
- Extend CellAttributes to store full 24-bit RGB colors and new flags (dim, strikethrough)
- Implement scroll region support in ScreenBuffer
- Add cursor save/restore with full state
- Implement device status responses
- Add missing private modes (DECTCEM, DECAWM, DECCKM)

## Technical Context

**Language/Version**: C++17 (Visual Studio 2022)  
**Primary Dependencies**: FreeType, spdlog, DirectX 12  
**Storage**: In-memory screen buffer with scrollback  
**Testing**: Google Test (gtest), Python integration tests  
**Target Platform**: Windows 10 1809+ (ConPTY requirement)  
**Performance Goals**: 60 FPS rendering, sub-ms input latency  
**Constraints**: <100MB memory for 10K line scrollback  

## Constitution Check

| Principle | Status | Notes |
|-----------|--------|-------|
| GPU-First Rendering | PASS | New attributes render via existing shader pipeline |
| VT/ANSI Compliance | IN PROGRESS | This phase implements missing sequences |
| Test-First Development | REQUIRED | Unit tests before each feature |
| Windows Native | PASS | ConPTY integration already complete |
| Unicode Support | PASS | No changes to Unicode handling |

## Project Structure

### Documentation (this feature)

```text
specs/1-core-vt-compatibility/
├── spec.md              # Feature specification
├── plan.md              # This file
├── research.md          # Technical decisions
├── data-model.md        # Data structure changes
└── quickstart.md        # Implementation guide
```

### Source Code (affected files)

```text
include/terminal/
├── ScreenBuffer.h       # Extended CellAttributes, scroll region, cursor state
└── VTStateMachine.h     # New handler declarations

src/terminal/
├── ScreenBuffer.cpp     # Scroll region logic, cursor save/restore
└── VTStateMachine.cpp   # New escape sequence handlers

src/core/
└── Application.cpp      # Dim/strikethrough rendering, RGB color resolution

shaders/
└── GlyphPixel.hlsl      # Dim intensity shader parameter

tests/unit/
├── VTStateMachine_test.cpp   # New sequence tests
└── ScreenBuffer_test.cpp     # Scroll region tests
```

## Implementation Phases

### Phase A: Data Model Changes (Foundation)

1. Extend `CellAttributes` in ScreenBuffer.h:
   - Add `FLAG_DIM` and `FLAG_STRIKETHROUGH` flags
   - Add `uint8_t fgR, fgG, fgB` for true color foreground
   - Add `uint8_t bgR, bgG, bgB` for true color background
   - Add `bool useTrueColorFg/Bg` flags

2. Add scroll region state to `ScreenBuffer`:
   - `int m_scrollTop, m_scrollBottom` (default: 0, rows-1)
   - `void SetScrollRegion(int top, int bottom)`
   - `void ScrollRegionUp/Down(int lines)`

3. Add cursor save/restore state:
   - `struct CursorState { x, y, attributes, originMode, autoWrap }`
   - `CursorState m_savedCursor`
   - `void SaveCursor()`, `void RestoreCursor()`

### Phase B: VT Sequence Handlers

1. **Cursor Control**:
   - `HandleCursorSave()` (CSI s) - save position only
   - `HandleCursorRestore()` (CSI u) - restore position only
   - `HandleDECSC()` (ESC 7) - save full state
   - `HandleDECRC()` (ESC 8) - restore full state
   - `HandleDeviceStatusReport()` (CSI 6n) - return cursor position

2. **Mode Settings**:
   - Extend `HandleMode()` for private modes:
     - Mode 1 (DECCKM): Application cursor keys
     - Mode 7 (DECAWM): Auto-wrap mode
     - Mode 25 (DECTCEM): Cursor visibility
     - Mode 2004: Bracketed paste mode

3. **Text Attributes**:
   - Add SGR 2 (dim) handling
   - Add SGR 9 (strikethrough) handling
   - Modify SGR 38/48 to store true RGB values

4. **Scroll Region**:
   - Implement `HandleSetScrollingRegion()` (DECSTBM)
   - Implement `HandleScrollUp()` (CSI n S)
   - Implement `HandleScrollDown()` (CSI n T)

5. **Device Responses**:
   - Implement `HandleDeviceAttributes()` - return VT100 identifier
   - Implement cursor position response (CPR)

### Phase C: Rendering Updates

1. Update `Application.cpp`:
   - Read true color RGB from CellAttributes when available
   - Apply dim factor (0.5 intensity) when FLAG_DIM set
   - Render strikethrough line when FLAG_STRIKETHROUGH set

2. Update shader or instance buffer:
   - Pass RGB values directly instead of palette lookup
   - Add dim multiplier uniform/constant

### Phase D: Testing

1. Unit tests for each new sequence
2. Integration tests with vim, htop
3. Manual testing with Claude Code

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Scroll region edge cases | Medium | Medium | Extensive xterm reference testing |
| Performance regression from RGB | Low | Low | Benchmark before/after |
| Breaking existing tests | Medium | Medium | Run tests after each change |

## Dependencies

- No new external dependencies
- Existing gtest infrastructure sufficient
- Existing DirectX 12 pipeline handles RGB colors
