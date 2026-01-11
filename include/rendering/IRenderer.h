#pragma once

#include <string>

namespace TerminalDX12::Rendering {

/**
 * @brief Abstract interface for terminal rendering
 *
 * This interface abstracts the rendering operations needed by the terminal,
 * allowing for different backend implementations (DX12, Vulkan, software, etc.)
 * and enabling unit testing with mock renderers.
 */
class IRenderer {
public:
    virtual ~IRenderer() = default;

    // Frame lifecycle
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;

    // Text rendering
    virtual void RenderText(const std::string& text, float x, float y,
                           float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) = 0;
    virtual void RenderChar(const std::string& ch, float x, float y,
                           float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f) = 0;
    virtual void RenderRect(float x, float y, float width, float height,
                           float r, float g, float b, float a = 1.0f) = 0;
    virtual void ClearText() = 0;

    // Font metrics (renamed to avoid Windows macro conflicts)
    virtual int GetGlyphWidth() const = 0;
    virtual int GetGlyphHeight() const = 0;

    // Resize
    virtual void Resize(int width, int height) = 0;
};

} // namespace TerminalDX12::Rendering
