#include "rendering/TextRenderer.h"
#include <spdlog/spdlog.h>

namespace {
// Convert UTF-8 string to UTF-32
std::u32string Utf8ToUtf32(const std::string& utf8) {
    std::u32string result;
    size_t i = 0;
    while (i < utf8.size()) {
        char32_t codepoint = 0;
        unsigned char c = utf8[i];
        if ((c & 0x80) == 0) {
            codepoint = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = (c & 0x1F) << 6;
            if (i + 1 < utf8.size()) codepoint |= (utf8[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = (c & 0x0F) << 12;
            if (i + 1 < utf8.size()) codepoint |= (utf8[i + 1] & 0x3F) << 6;
            if (i + 2 < utf8.size()) codepoint |= (utf8[i + 2] & 0x3F);
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            codepoint = (c & 0x07) << 18;
            if (i + 1 < utf8.size()) codepoint |= (utf8[i + 1] & 0x3F) << 12;
            if (i + 2 < utf8.size()) codepoint |= (utf8[i + 2] & 0x3F) << 6;
            if (i + 3 < utf8.size()) codepoint |= (utf8[i + 3] & 0x3F);
            i += 4;
        } else {
            i += 1; // Skip invalid byte
        }
        if (codepoint > 0) result += codepoint;
    }
    return result;
}
} // anonymous namespace

namespace TerminalDX12::Rendering {

TextRenderer::TextRenderer()
    : m_atlas(nullptr)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_rootSignature(nullptr)
    , m_pipelineState(nullptr)
    , m_device(nullptr)
{
}

TextRenderer::~TextRenderer() {
    Shutdown();
}

bool TextRenderer::Initialize(ID3D12Device* device, GlyphAtlas* atlas, int screenWidth, int screenHeight) {
    m_device = device;
    m_atlas = atlas;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    if (!CreateInstanceBuffer(device)) {
        spdlog::error("Failed to create instance buffer");
        return false;
    }

    spdlog::info("TextRenderer initialized: screen {}x{}", screenWidth, screenHeight);
    return true;
}

void TextRenderer::Shutdown() {
    m_instances.clear();
}

bool TextRenderer::CreateInstanceBuffer(ID3D12Device* device) {
    // Create instance buffer (large enough for MAX_INSTANCES)
    const UINT bufferSize = sizeof(GlyphInstance) * MAX_INSTANCES;

    // Create default heap buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC bufferDesc = {};
    bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufferDesc.Width = bufferSize;
    bufferDesc.Height = 1;
    bufferDesc.DepthOrArraySize = 1;
    bufferDesc.MipLevels = 1;
    bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    bufferDesc.SampleDesc.Count = 1;
    bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&m_instanceBuffer)
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create instance buffer");
        return false;
    }

    // Create upload heap buffer
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_instanceUploadBuffer)
    );

    if (FAILED(hr)) {
        spdlog::error("Failed to create instance upload buffer");
        return false;
    }

    // Initialize vertex buffer view
    m_instanceBufferView.BufferLocation = m_instanceBuffer->GetGPUVirtualAddress();
    m_instanceBufferView.SizeInBytes = bufferSize;
    m_instanceBufferView.StrideInBytes = sizeof(GlyphInstance);

    return true;
}

void TextRenderer::Clear() {
    m_instances.clear();
}

void TextRenderer::RenderText(const std::string& text, float x, float y, const XMFLOAT4& color) {
    // Convert UTF-8 to UTF-32
    std::u32string utf32Text = Utf8ToUtf32(text);
    RenderText(utf32Text, x, y, color);
}

void TextRenderer::RenderText(const std::u32string& text, float x, float y, const XMFLOAT4& color) {
    if (!m_atlas) return;

    float currentX = x, currentY = y;
    for (char32_t ch : text) {
        if (ch == U'\n') {
            currentX = x;
            currentY += m_atlas->GetLineHeight();
            continue;
        }
        const GlyphInfo* glyph = m_atlas->GetGlyph(ch);
        if (!glyph) glyph = m_atlas->GetGlyph(U' ');
        if (!glyph) continue;

        AddGlyphInstance(glyph, currentX, currentY, color);
        currentX += glyph->advance;
    }
}

void TextRenderer::RenderCharAtCell(const std::string& ch, float x, float y, const XMFLOAT4& color) {
    if (!m_atlas) return;
    std::u32string utf32 = Utf8ToUtf32(ch);
    if (utf32.empty()) return;
    const GlyphInfo* glyph = m_atlas->GetGlyph(utf32[0]);
    if (glyph) AddGlyphInstance(glyph, x, y, color);
}

void TextRenderer::AddGlyphInstance(const GlyphInfo* glyph, float x, float y, const XMFLOAT4& color) {
    if (m_instances.size() >= MAX_INSTANCES) return;
    GlyphInstance inst;
    inst.position = {x + glyph->bearingX, y - glyph->bearingY + m_atlas->GetFontSize()};
    inst.size = {static_cast<float>(glyph->width), static_cast<float>(glyph->height)};
    inst.uvMin = {glyph->u0, glyph->v0};
    inst.uvMax = {glyph->u1, glyph->v1};
    inst.color = color;
    m_instances.push_back(inst);
}

void TextRenderer::RenderRect(float x, float y, float width, float height,
                               float r, float g, float b, float a) {
    if (!m_atlas) {
        return;
    }

    // Use the full block character █ (U+2588) for solid rectangles
    const GlyphInfo* solid = m_atlas->GetGlyph(U'\u2588');  // Full block █
    if (!solid) {
        // Fallback to solid white pixel if block char not available
        solid = m_atlas->GetSolidGlyph();
        if (!solid) {
            return;
        }
    }

    // Create instance data for the rectangle
    GlyphInstance instance;

    // Position (top-left corner)
    instance.position.x = x;
    instance.position.y = y;

    // Size of the rectangle
    instance.size.x = width;
    instance.size.y = height;

    // UV coordinates point to the solid white pixel
    // The pixel shader will sample this white pixel and multiply by color
    instance.uvMin.x = solid->u0;
    instance.uvMin.y = solid->v0;
    instance.uvMax.x = solid->u1;
    instance.uvMax.y = solid->v1;

    // Color (with alpha)
    instance.color.x = r;
    instance.color.y = g;
    instance.color.z = b;
    instance.color.w = a;

    // Add to instance list
    if (m_instances.size() < MAX_INSTANCES) {
        m_instances.push_back(instance);
    }
}

void TextRenderer::SetScreenSize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

void TextRenderer::UpdateInstanceBuffer() {
    if (m_instances.empty()) {
        return;
    }

    // Map upload buffer and copy data
    void* pData;
    D3D12_RANGE readRange = {0, 0};  // We don't read from this resource
    HRESULT hr = m_instanceUploadBuffer->Map(0, &readRange, &pData);

    if (FAILED(hr)) {
        spdlog::error("Failed to map instance upload buffer");
        return;
    }

    memcpy(pData, m_instances.data(), sizeof(GlyphInstance) * m_instances.size());
    m_instanceUploadBuffer->Unmap(0, nullptr);
}

void TextRenderer::Render(ID3D12GraphicsCommandList* commandList) {
    if (m_instances.empty() || !m_rootSignature || !m_pipelineState || !m_atlas) return;

    // Upload atlas to GPU if it has new glyphs
    m_atlas->UploadAtlasIfDirty();

    // Update instance buffer with current data
    UpdateInstanceBuffer();

    // Copy from upload buffer to default buffer
    commandList->CopyBufferRegion(
        m_instanceBuffer.Get(),
        0,
        m_instanceUploadBuffer.Get(),
        0,
        sizeof(GlyphInstance) * m_instances.size()
    );

    // Transition instance buffer to vertex buffer state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_instanceBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);

    // Set pipeline state
    commandList->SetPipelineState(m_pipelineState);
    commandList->SetGraphicsRootSignature(m_rootSignature);

    // Set descriptor heap for atlas texture
    ID3D12DescriptorHeap* heaps[] = { m_atlas->GetSRVHeap() };
    commandList->SetDescriptorHeaps(1, heaps);

    // Set root parameters (screen dimensions, atlas texture, etc.)
    struct ScreenConstants {
        float screenWidth;
        float screenHeight;
        float padding[2];
    };

    ScreenConstants constants = {
        static_cast<float>(m_screenWidth),
        static_cast<float>(m_screenHeight),
        {0, 0}
    };

    commandList->SetGraphicsRoot32BitConstants(0, sizeof(ScreenConstants) / 4, &constants, 0);
    commandList->SetGraphicsRootDescriptorTable(1, m_atlas->GetAtlasSRV());

    // Set instance buffer
    commandList->IASetVertexBuffers(0, 1, &m_instanceBufferView);

    // Set primitive topology
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Draw instanced quads (4 vertices per quad, instanced by glyph count)
    commandList->DrawInstanced(4, static_cast<UINT>(m_instances.size()), 0, 0);

    // Transition back to copy dest for next frame
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    commandList->ResourceBarrier(1, &barrier);
}

} // namespace TerminalDX12::Rendering
