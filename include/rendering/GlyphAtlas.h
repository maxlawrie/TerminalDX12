#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace TerminalDX12::Rendering {

// Represents a single glyph in the atlas
struct GlyphInfo {
    // UV coordinates in the atlas (0.0 to 1.0)
    float u0, v0;  // Top-left
    float u1, v1;  // Bottom-right

    // Glyph metrics (in pixels)
    int width, height;
    int bearingX, bearingY;  // Offset from baseline
    int advance;             // Horizontal advance

    // Atlas position (in pixels)
    int atlasX, atlasY;
};

// Key for glyph cache lookup
struct GlyphKey {
    char32_t codepoint;
    int fontSize;
    bool bold;
    bool italic;

    bool operator==(const GlyphKey& other) const {
        return codepoint == other.codepoint &&
               fontSize == other.fontSize &&
               bold == other.bold &&
               italic == other.italic;
    }
};

// Hash function for GlyphKey
struct GlyphKeyHash {
    std::size_t operator()(const GlyphKey& key) const {
        return std::hash<char32_t>()(key.codepoint) ^
               (std::hash<int>()(key.fontSize) << 1) ^
               (std::hash<bool>()(key.bold) << 2) ^
               (std::hash<bool>()(key.italic) << 3);
    }
};

// Result of text shaping (used for ligatures)
struct ShapedGlyph {
    const GlyphInfo* glyph;
    float xOffset;      // Offset from base position (for combining)
    float yOffset;
    float xAdvance;     // How far to move for next glyph
    int clusterIndex;   // Index into original text
};

class GlyphAtlas {
public:
    GlyphAtlas();
    ~GlyphAtlas();

    // Initialize with D3D12 device and font path
    [[nodiscard]] bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                   const std::string& fontPath, int fontSize);
    void Shutdown();

    // Get or create a glyph in the atlas (uses default font size)
    const GlyphInfo* GetGlyph(char32_t codepoint, bool bold = false, bool italic = false);

    // Get or create a glyph at a specific font size (for per-pane fonts)
    const GlyphInfo* GetGlyph(char32_t codepoint, int fontSize, bool bold = false, bool italic = false);

    // Shape text with HarfBuzz for ligature support
    std::vector<ShapedGlyph> ShapeText(const std::u32string& text, bool bold = false, bool italic = false);

    // Get font metrics for a specific size
    int GetLineHeight(int fontSize) const;
    int GetCharWidth(int fontSize) const;

    // Check if ligatures are enabled
    bool LigaturesEnabled() const { return m_ligaturesEnabled; }
    void SetLigaturesEnabled(bool enabled) { m_ligaturesEnabled = enabled; }

    // Get the atlas texture SRV
    D3D12_GPU_DESCRIPTOR_HANDLE GetAtlasSRV() const { return m_atlasSRV; }

    // Get atlas dimensions
    int GetAtlasWidth() const { return m_atlasWidth; }
    int GetAtlasHeight() const { return m_atlasHeight; }

    // Get font metrics
    int GetFontSize() const { return m_fontSize; }
    int GetLineHeight() const { return m_lineHeight; }
    int GetCharWidth() const { return m_charWidth; }

    // Reinitialize with new font settings (for hot reload)
    [[nodiscard]] bool Reinitialize(const std::string& fontPath, int fontSize);

    // Upload atlas to GPU if dirty (pass current frame's command list)
    void UploadAtlasIfDirty(ID3D12GraphicsCommandList* commandList);

    // Preload ASCII glyphs to avoid race conditions during rendering
    void PreloadASCIIGlyphs();

    // Get solid white pixel glyph for rectangles/underlines
    const GlyphInfo* GetSolidGlyph() const { return &m_solidGlyph; }

    // Get descriptor heap
    ID3D12DescriptorHeap* GetSRVHeap() const { return m_srvHeap.Get(); }

private:
    [[nodiscard]] bool InitializeFreeType(const std::string& fontPath, int fontSize);
    [[nodiscard]] bool CreateAtlasTexture(ID3D12Device* device);
    [[nodiscard]] bool CreateAtlasSRV(ID3D12Device* device);

    // Set FreeType face to a specific size and cache metrics
    void SetFaceSize(int fontSize);

    // Add a glyph to the atlas at specific size
    GlyphInfo* AddGlyphToAtlas(char32_t codepoint, int fontSize, bool bold, bool italic);

    // Upload glyph bitmap to GPU
    void UploadGlyphToGPU(const GlyphInfo& glyph, const unsigned char* bitmap);

    // FreeType library and face
    FT_Library m_ftLibrary;
    FT_Face m_ftFace;

    // Font properties
    int m_fontSize;
    int m_lineHeight;
    int m_charWidth;

    // Atlas texture properties
    static constexpr int m_atlasWidth = 2048;
    static constexpr int m_atlasHeight = 2048;

    // Packing state (simple row-based packing)
    int m_currentX;
    int m_currentY;
    int m_currentRowHeight;

    // D3D12 resources
    ComPtr<ID3D12Resource> m_atlasTexture;
    ComPtr<ID3D12Resource> m_atlasUploadBuffer;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_atlasSRV;

    // Cached device and command list for updates
    ID3D12Device* m_device;
    ID3D12GraphicsCommandList* m_commandList;

    // Glyph cache
    std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> m_glyphCache;

    // Per-size font metrics
    struct SizeMetrics {
        int lineHeight;
        int charWidth;
    };
    mutable std::unordered_map<int, SizeMetrics> m_sizeMetrics;
    int m_currentFaceSize = 0;  // Currently set FreeType face size

    // Atlas CPU-side buffer for updates
    std::unique_ptr<uint8_t[]> m_atlasBuffer;
    bool m_atlasDirty;

    // Solid white pixel glyph for rectangles/underlines
    GlyphInfo m_solidGlyph;

    // HarfBuzz for text shaping/ligatures
    hb_font_t* m_hbFont;
    bool m_ligaturesEnabled;
};

} // namespace TerminalDX12::Rendering
