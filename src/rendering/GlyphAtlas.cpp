#include "rendering/GlyphAtlas.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstring>

namespace TerminalDX12::Rendering {

// Helper function to calculate required intermediate buffer size for texture uploads
static UINT64 GetRequiredIntermediateSize(ID3D12Resource* resource, UINT firstSubresource, UINT numSubresources) {
    D3D12_RESOURCE_DESC desc = resource->GetDesc();
    UINT64 requiredSize = 0;

    ID3D12Device* device = nullptr;
    resource->GetDevice(IID_PPV_ARGS(&device));

    device->GetCopyableFootprints(&desc, firstSubresource, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);
    device->Release();

    return requiredSize;
}

GlyphAtlas::GlyphAtlas()
    : m_ftLibrary(nullptr)
    , m_ftFace(nullptr)
    , m_fontSize(0)
    , m_lineHeight(0)
    , m_currentX(0)
    , m_currentY(0)
    , m_currentRowHeight(0)
    , m_device(nullptr)
    , m_commandList(nullptr)
    , m_atlasDirty(false)
    , m_hbFont(nullptr)
    , m_ligaturesEnabled(false)
{
}

GlyphAtlas::~GlyphAtlas() {
    Shutdown();
}

bool GlyphAtlas::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList,
                           const std::string& fontPath, int fontSize) {
    m_device = device;
    m_commandList = commandList;
    m_fontSize = fontSize;

    // Initialize FreeType
    if (!InitializeFreeType(fontPath, fontSize)) {
        spdlog::error("Failed to initialize FreeType");
        return false;
    }

    // Initialize HarfBuzz font from FreeType face
    m_hbFont = hb_ft_font_create(m_ftFace, nullptr);
    if (!m_hbFont) {
        spdlog::error("Failed to create HarfBuzz font");
        return false;
    }
    spdlog::info("HarfBuzz font created successfully");

    // Create atlas texture
    if (!CreateAtlasTexture(device)) {
        spdlog::error("Failed to create atlas texture");
        return false;
    }

    // Create SRV heap and descriptor
    if (!CreateAtlasSRV(device)) {
        spdlog::error("Failed to create atlas SRV");
        return false;
    }

    // Allocate CPU-side atlas buffer (RGBA8)
    m_atlasBuffer = std::make_unique<uint8_t[]>(m_atlasWidth * m_atlasHeight * 4);
    std::memset(m_atlasBuffer.get(), 0, m_atlasWidth * m_atlasHeight * 4);

    spdlog::info("GlyphAtlas initialized: {}x{}, font size: {}",
                 m_atlasWidth, m_atlasHeight, m_fontSize);

    // Preload all ASCII printable glyphs at startup to avoid race conditions
    PreloadASCIIGlyphs();

    return true;
}

void GlyphAtlas::Shutdown() {
    m_glyphCache.clear();
    m_atlasBuffer.reset();

    // Clean up HarfBuzz (before FreeType since it references the face)
    if (m_hbFont) {
        hb_font_destroy(m_hbFont);
        m_hbFont = nullptr;
    }

    if (m_ftFace) {
        FT_Done_Face(m_ftFace);
        m_ftFace = nullptr;
    }

    if (m_ftLibrary) {
        FT_Done_FreeType(m_ftLibrary);
        m_ftLibrary = nullptr;
    }
}

bool GlyphAtlas::InitializeFreeType(const std::string& fontPath, int fontSize) {
    // Initialize FreeType library
    if (FT_Init_FreeType(&m_ftLibrary) != 0) {
        spdlog::error("Failed to initialize FreeType library");
        return false;
    }

    // Load font face
    if (FT_New_Face(m_ftLibrary, fontPath.c_str(), 0, &m_ftFace) != 0) {
        spdlog::error("Failed to load font: {}", fontPath);
        return false;
    }

    // Set font size (96 DPI)
    if (FT_Set_Char_Size(m_ftFace, 0, fontSize * 64, 96, 96) != 0) {
        spdlog::error("Failed to set font size: {}", fontSize);
        return false;
    }

    // Calculate line height from font metrics
    m_lineHeight = (m_ftFace->size->metrics.height >> 6);

    spdlog::info("Loaded font: {} (size: {}, line height: {})",
                 fontPath, fontSize, m_lineHeight);

    return true;
}

bool GlyphAtlas::CreateAtlasTexture(ID3D12Device* device) {
    // Create texture resource
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = m_atlasWidth;
    textureDesc.Height = m_atlasHeight;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&m_atlasTexture)
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create atlas texture");
        return false;
    }

    // Create upload buffer
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_atlasTexture.Get(), 0, 1);

    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadBufferSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_atlasUploadBuffer)
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create atlas upload buffer");
        return false;
    }

    return true;
}

bool GlyphAtlas::CreateAtlasSRV(ID3D12Device* device) {
    // Create SRV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvHeap));
    if (FAILED(hr)) {
        spdlog::error("Failed to create SRV descriptor heap");
        return false;
    }

    // Create SRV
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
    device->CreateShaderResourceView(m_atlasTexture.Get(), &srvDesc, cpuHandle);

    m_atlasSRV = m_srvHeap->GetGPUDescriptorHandleForHeapStart();

    return true;
}

const GlyphInfo* GlyphAtlas::GetGlyph(char32_t codepoint, bool bold, bool italic) {
    // Validate codepoint - must be valid Unicode (0-0x10FFFF)
    // Invalid codepoints can come from uninitialized memory during resize
    if (codepoint > 0x10FFFF || (codepoint < 0x20 && codepoint != U'\t')) {
        // Return space glyph for invalid codepoints
        codepoint = U' ';
    }

    // Check cache
    GlyphKey key{codepoint, m_fontSize, bold, italic};
    auto it = m_glyphCache.find(key);

    if (it != m_glyphCache.end()) {
        return &it->second;
    }

    // Add new glyph to atlas
    return AddGlyphToAtlas(codepoint, bold, italic);
}

GlyphInfo* GlyphAtlas::AddGlyphToAtlas(char32_t codepoint, bool bold, bool italic) {
    // Load glyph from FreeType
    FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace, codepoint);

    if (glyphIndex == 0) {
        spdlog::warn("Glyph not found for codepoint: U+{:04X}", static_cast<uint32_t>(codepoint));
        return nullptr;
    }

    // Load and render glyph
    FT_Int32 loadFlags = FT_LOAD_RENDER;
    if (FT_Load_Glyph(m_ftFace, glyphIndex, loadFlags) != 0) {
        spdlog::error("Failed to load glyph: U+{:04X}", static_cast<uint32_t>(codepoint));
        return nullptr;
    }

    FT_GlyphSlot slot = m_ftFace->glyph;
    FT_Bitmap& bitmap = slot->bitmap;

    // Check if glyph fits in current row
    if (m_currentX + bitmap.width > m_atlasWidth) {
        // Move to next row
        m_currentX = 0;
        m_currentY += m_currentRowHeight;
        m_currentRowHeight = 0;
    }

    // Check if atlas is full
    if (m_currentY + bitmap.rows > m_atlasHeight) {
        spdlog::error("Atlas is full! Cannot add more glyphs.");
        return nullptr;
    }

    // Create glyph info
    GlyphInfo glyphInfo;
    glyphInfo.width = bitmap.width;
    glyphInfo.height = bitmap.rows;
    glyphInfo.bearingX = slot->bitmap_left;
    glyphInfo.bearingY = slot->bitmap_top;
    glyphInfo.advance = slot->advance.x >> 6;  // Convert from 26.6 fixed point
    glyphInfo.atlasX = m_currentX;
    glyphInfo.atlasY = m_currentY;

    // Calculate UV coordinates
    glyphInfo.u0 = static_cast<float>(m_currentX) / m_atlasWidth;
    glyphInfo.v0 = static_cast<float>(m_currentY) / m_atlasHeight;
    glyphInfo.u1 = static_cast<float>(m_currentX + bitmap.width) / m_atlasWidth;
    glyphInfo.v1 = static_cast<float>(m_currentY + bitmap.rows) / m_atlasHeight;

    // Copy glyph bitmap to atlas buffer (grayscale to RGBA)
    for (unsigned int y = 0; y < bitmap.rows; ++y) {
        for (unsigned int x = 0; x < bitmap.width; ++x) {
            int atlasIndex = ((m_currentY + y) * m_atlasWidth + (m_currentX + x)) * 4;
            unsigned char value = bitmap.buffer[y * bitmap.pitch + x];

            m_atlasBuffer[atlasIndex + 0] = 255;      // R (white)
            m_atlasBuffer[atlasIndex + 1] = 255;      // G
            m_atlasBuffer[atlasIndex + 2] = 255;      // B
            m_atlasBuffer[atlasIndex + 3] = value;    // A (alpha from glyph)
        }
    }

    // Update packing position
    m_currentX += bitmap.width + 1;  // +1 for padding
    m_currentRowHeight = std::max(m_currentRowHeight, static_cast<int>(bitmap.rows) + 1);
    m_atlasDirty = true;

    // Add to cache
    GlyphKey key{codepoint, m_fontSize, bold, italic};
    m_glyphCache[key] = glyphInfo;

    // Upload to GPU if needed (deferred for now - will upload all at once)

    return &m_glyphCache[key];
}

void GlyphAtlas::UploadGlyphToGPU(const GlyphInfo& glyph, const unsigned char* bitmap) {
    // This method would handle incremental uploads
    // For MVP, we'll upload the entire atlas when dirty
    // Note: Full atlas upload works for now. Region-based updates could improve performance with many glyphs.
}

void GlyphAtlas::UploadAtlasIfDirty() {
    static bool firstCall = true;
    if (firstCall) {
        spdlog::info("UploadAtlasIfDirty called - dirty:{}, cmdList:{}, buffer:{}",
                     m_atlasDirty, (void*)m_commandList, (void*)m_atlasBuffer.get());
        firstCall = false;
    }

    if (!m_atlasDirty || !m_commandList || !m_atlasBuffer) {
        return;
    }

    spdlog::info("UploadAtlasIfDirty - starting atlas upload");

    // Map upload buffer
    void* pData;
    D3D12_RANGE readRange = {0, 0};
    HRESULT hr = m_atlasUploadBuffer->Map(0, &readRange, &pData);
    if (FAILED(hr)) {
        spdlog::error("Failed to map atlas upload buffer: 0x{:08X}", hr);
        return;
    }

    spdlog::info("Upload buffer mapped successfully");

    // Copy atlas data to upload buffer
    const UINT64 uploadPitch = (m_atlasWidth * 4 + 255) & ~255;  // Align to 256 bytes
    const UINT64 rowPitch = m_atlasWidth * 4;

    for (int y = 0; y < m_atlasHeight; ++y) {
        memcpy(
            (uint8_t*)pData + y * uploadPitch,
            m_atlasBuffer.get() + y * rowPitch,
            rowPitch
        );
    }
    spdlog::info("Atlas data copied to upload buffer");

    m_atlasUploadBuffer->Unmap(0, nullptr);
    spdlog::info("Upload buffer unmapped");

    // Transition texture to COPY_DEST if needed
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_atlasTexture.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);

    // Copy upload buffer to texture
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
    footprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    footprint.Footprint.Width = m_atlasWidth;
    footprint.Footprint.Height = m_atlasHeight;
    footprint.Footprint.Depth = 1;
    footprint.Footprint.RowPitch = (UINT)uploadPitch;

    D3D12_TEXTURE_COPY_LOCATION dst = {};
    dst.pResource = m_atlasTexture.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src = {};
    src.pResource = m_atlasUploadBuffer.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.PlacedFootprint = footprint;

    m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    spdlog::info("CopyTextureRegion completed");

    // Transition texture back to PIXEL_SHADER_RESOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    m_commandList->ResourceBarrier(1, &barrier);
    spdlog::info("Resource barrier completed");

    m_atlasDirty = false;
    spdlog::info("Atlas texture uploaded to GPU successfully!");
}

void GlyphAtlas::PreloadASCIIGlyphs() {
    spdlog::info("Preloading ASCII glyphs (32-126)...");

    // First, create a solid white pixel at (0,0) for underlines/rectangles
    // This reserves the first pixel in the atlas for solid color rendering
    if (m_atlasBuffer) {
        // Write a 1x1 white pixel at position (0,0)
        uint8_t* pixel = m_atlasBuffer.get();
        pixel[0] = 255;  // R
        pixel[1] = 255;  // G
        pixel[2] = 255;  // B
        pixel[3] = 255;  // A

        // Initialize the solid glyph info
        m_solidGlyph.u0 = 0.0f;
        m_solidGlyph.v0 = 0.0f;
        m_solidGlyph.u1 = 1.0f / static_cast<float>(m_atlasWidth);
        m_solidGlyph.v1 = 1.0f / static_cast<float>(m_atlasHeight);
        m_solidGlyph.width = 1;
        m_solidGlyph.height = 1;
        m_solidGlyph.bearingX = 0;
        m_solidGlyph.bearingY = 0;
        m_solidGlyph.advance = 1;
        m_solidGlyph.atlasX = 0;
        m_solidGlyph.atlasY = 0;

        // Make sure the atlas packer doesn't overwrite our solid pixel
        if (m_currentX == 0 && m_currentY == 0) {
            m_currentX = 2;  // Start packing after our solid pixel
        }

        m_atlasDirty = true;
        spdlog::info("Created solid white pixel for underline rendering");
    }

    int loaded = 0;
    int failed = 0;

    // Preload all printable ASCII characters
    for (char32_t ch = 32; ch <= 126; ++ch) {
        const GlyphInfo* glyph = GetGlyph(ch, false, false);
        if (glyph) {
            loaded++;
        } else {
            failed++;
            spdlog::warn("Failed to preload glyph for char '{}' (U+{:04X})",
                        static_cast<char>(ch), static_cast<uint32_t>(ch));
        }
    }

    // Preload box-drawing characters (U+2500-257F)
    spdlog::info("Preloading box-drawing characters (U+2500-257F)...");
    for (char32_t ch = 0x2500; ch <= 0x257F; ++ch) {
        const GlyphInfo* glyph = GetGlyph(ch, false, false);
        if (glyph) {
            loaded++;
        } else {
            failed++;
        }
    }

    // Preload block elements (U+2580-259F)
    spdlog::info("Preloading block elements (U+2580-259F)...");
    for (char32_t ch = 0x2580; ch <= 0x259F; ++ch) {
        const GlyphInfo* glyph = GetGlyph(ch, false, false);
        if (glyph) {
            loaded++;
        } else {
            failed++;
        }
    }

    // Preload geometric shapes commonly used in TUIs (U+25A0-25FF)
    spdlog::info("Preloading geometric shapes (U+25A0-25FF)...");
    for (char32_t ch = 0x25A0; ch <= 0x25FF; ++ch) {
        const GlyphInfo* glyph = GetGlyph(ch, false, false);
        if (glyph) {
            loaded++;
        } else {
            failed++;
        }
    }

    spdlog::info("Preloaded {} glyphs total ({} failed)", loaded, failed);
}

std::vector<ShapedGlyph> GlyphAtlas::ShapeText(const std::u32string& text, bool bold, bool italic) {
    std::vector<ShapedGlyph> result;

    if (text.empty() || !m_hbFont) {
        return result;
    }

    // If ligatures disabled, just return simple glyph lookups
    if (!m_ligaturesEnabled) {
        result.reserve(text.size());
        for (size_t i = 0; i < text.size(); ++i) {
            ShapedGlyph sg;
            sg.glyph = GetGlyph(text[i], bold, italic);
            sg.xOffset = 0.0f;
            sg.yOffset = 0.0f;
            sg.xAdvance = sg.glyph ? static_cast<float>(sg.glyph->advance) : 0.0f;
            sg.clusterIndex = static_cast<int>(i);
            result.push_back(sg);
        }
        return result;
    }

    // Create HarfBuzz buffer
    hb_buffer_t* buffer = hb_buffer_create();
    if (!buffer) {
        return result;
    }

    // Set buffer properties
    hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(buffer, HB_SCRIPT_COMMON);
    hb_buffer_set_language(buffer, hb_language_get_default());

    // Add text to buffer (convert u32string to proper format)
    for (char32_t ch : text) {
        hb_buffer_add(buffer, ch, hb_buffer_get_length(buffer));
    }

    // Enable OpenType ligature features
    hb_feature_t features[] = {
        {HB_TAG('l','i','g','a'), 1, 0, static_cast<unsigned>(-1)},  // Standard ligatures
        {HB_TAG('c','l','i','g'), 1, 0, static_cast<unsigned>(-1)},  // Contextual ligatures
        {HB_TAG('c','a','l','t'), 1, 0, static_cast<unsigned>(-1)},  // Contextual alternates
    };

    // Shape the text
    hb_shape(m_hbFont, buffer, features, sizeof(features) / sizeof(features[0]));

    // Get shaping results
    unsigned int glyphCount;
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    result.reserve(glyphCount);

    for (unsigned int i = 0; i < glyphCount; ++i) {
        ShapedGlyph sg;

        // Get glyph by its glyph ID (not codepoint) - for ligatures
        // For now, use the cluster codepoint as fallback
        uint32_t cluster = glyphInfo[i].cluster;
        char32_t codepoint = (cluster < text.size()) ? text[cluster] : U' ';

        sg.glyph = GetGlyph(codepoint, bold, italic);
        sg.xOffset = static_cast<float>(glyphPos[i].x_offset) / 64.0f;
        sg.yOffset = static_cast<float>(glyphPos[i].y_offset) / 64.0f;
        sg.xAdvance = static_cast<float>(glyphPos[i].x_advance) / 64.0f;
        sg.clusterIndex = static_cast<int>(cluster);

        result.push_back(sg);
    }

    hb_buffer_destroy(buffer);
    return result;
}

} // namespace TerminalDX12::Rendering
