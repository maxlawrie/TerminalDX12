# Implementation Quickstart: Core VT Compatibility

**Feature**: Core VT Compatibility  
**Date**: 2025-12-21

## Prerequisites

- Visual Studio 2022 with C++20 support
- vcpkg with dependencies installed
- Existing TerminalDX12 builds successfully
- All 176 unit tests passing

## Step-by-Step Implementation Order

### Step 1: Extend CellAttributes

**File**: `include/terminal/ScreenBuffer.h`

Add new flag constants and RGB fields to CellAttributes struct.

### Step 2: Add SGR 2 and SGR 9 Handling

**File**: `src/terminal/VTStateMachine.cpp` in `HandleSGR()`

Add cases for dim (SGR 2) and strikethrough (SGR 9).

### Step 3: Store True RGB Colors

**File**: `src/terminal/VTStateMachine.cpp` in `HandleSGR()`

Modify case 38 and case 48 to store RGB values directly.

### Step 4: Add Scroll Region Support

**Files**: `include/terminal/ScreenBuffer.h`, `src/terminal/ScreenBuffer.cpp`

Add scroll region members and ScrollRegionUp/Down methods.

### Step 5: Implement DECSTBM Handler

**File**: `src/terminal/VTStateMachine.cpp`

Implement HandleSetScrollingRegion to set scroll region bounds.

### Step 6: Add Cursor Save/Restore

**Files**: `include/terminal/VTStateMachine.h`, `src/terminal/VTStateMachine.cpp`

Add SavedCursorState struct and handlers for ESC 7/8 and CSI s/u.

### Step 7: Add Private Mode Handling

**File**: `src/terminal/VTStateMachine.cpp`

Extend HandleMode for DECCKM (1), DECAWM (7), DECTCEM (25), bracketed paste (2004).

### Step 8: Add Device Status Response

**Files**: `include/terminal/VTStateMachine.h`, `src/terminal/VTStateMachine.cpp`

Add response callback and implement DSR (CSI 5n, CSI 6n) and DA (CSI c).

### Step 9: Update Rendering for New Attributes

**File**: `src/core/Application.cpp` or `src/rendering/TextRenderer.cpp`

Update color resolution to use RGB when available, apply dim factor, render strikethrough.

### Step 10: Integration Testing

Run unit tests, test with vim, htop, and Claude Code.

## Verification Checklist

- [ ] All 176 existing tests pass
- [ ] New unit tests for each handler
- [ ] vim opens, edits, exits cleanly
- [ ] htop displays with fixed header
- [ ] True colors display correctly (not mapped to 16)
- [ ] Dim text visibly dimmer than normal
- [ ] Strikethrough line renders through text
- [ ] Cursor hide/show works
- [ ] Cursor position reports correctly

