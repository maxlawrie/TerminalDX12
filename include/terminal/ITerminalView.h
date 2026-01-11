#pragma once

#include <cstdint>

namespace TerminalDX12::Terminal {

// Forward declarations
struct Cell;
struct PaletteColor;
struct Hyperlink;

/**
 * @brief Read-only interface to terminal state for rendering
 *
 * This interface provides access to the terminal's visual state without
 * exposing write operations. Used by RenderingPipeline to render terminal
 * content, enabling testing with mock implementations.
 */
class ITerminalView {
public:
    virtual ~ITerminalView() = default;

    // Dimensions
    virtual int GetCols() const = 0;
    virtual int GetRows() const = 0;
    virtual void GetDimensions(int& cols, int& rows) const = 0;

    // Cell access (accounts for scrollback offset)
    virtual Cell GetCellWithScrollback(int x, int y) const = 0;

    // Cursor state
    virtual int GetCursorX() const = 0;
    virtual int GetCursorY() const = 0;
    virtual bool IsCursorVisible() const = 0;

    // Scrolling
    virtual int GetScrollOffset() const = 0;

    // Dirty tracking
    virtual bool IsDirty() const = 0;
    virtual void ClearDirty() = 0;

    // Buffer state
    virtual bool IsUsingAlternativeBuffer() const = 0;

    // Color palette (OSC 4)
    virtual const PaletteColor& GetPaletteColor(int index) const = 0;

    // Hyperlinks (OSC 8)
    virtual const Hyperlink* GetHyperlink(uint16_t id) const = 0;
};

} // namespace TerminalDX12::Terminal
