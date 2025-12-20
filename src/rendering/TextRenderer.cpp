#include "rendering/TextRenderer.h"
#include <spdlog/spdlog.h>
#include <codecvt>
#include <locale>

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
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string utf32Text = converter.from_bytes(text);
    RenderText(utf32Text, x, y, color);
}

void TextRenderer::RenderText(const std::u32string& text, float x, float y, const XMFLOAT4& color) {
    if (!m_atlas) {
        spdlog::error("GlyphAtlas not set");
        return;
    }

    float currentX = x;
    float currentY = y;

    for (char32_t ch : text) {
        // Handle newline
        if (ch == U'\n') {
            currentX = x;
            currentY += m_atlas->GetLineHeight();
            continue;
        }

        // Get glyph from atlas
        const GlyphInfo* glyph = m_atlas->GetGlyph(ch);
        if (!glyph) {
            // Use space character as fallback
            glyph = m_atlas->GetGlyph(U' ');
            if (!glyph) continue;
        }

        // Create instance data
        GlyphInstance instance;

        // Position (account for glyph bearing)
        instance.position.x = currentX + glyph->bearingX;
        instance.position.y = currentY - glyph->bearingY + m_atlas->GetFontSize();

        // Size
        instance.size.x = static_cast<float>(glyph->width);
        instance.size.y = static_cast<float>(glyph->height);

        // UV coordinates
        instance.uvMin.x = glyph->u0;
        instance.uvMin.y = glyph->v0;
        instance.uvMax.x = glyph->u1;
        instance.uvMax.y = glyph->v1;

        // Color
        instance.color = color;

        // Add to instance list
        if (m_instances.size() < MAX_INSTANCES) {
            m_instances.push_back(instance);
        } else {
            spdlog::warn("Max instances reached, skipping glyphs");
            break;
        }

        // Advance cursor
        currentX += glyph->advance;
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
    static int renderCallCount = 0;

    if (m_instances.empty()) {
        if (renderCallCount < 5) {
            spdlog::info("TextRenderer::Render - no instances to render");
            renderCallCount++;
        }
        return;
    }

    if (!m_rootSignature || !m_pipelineState || !m_atlas) {
        spdlog::error("TextRenderer::Render - missing resources (rootSig:{}, PSO:{}, atlas:{})",
                     (void*)m_rootSignature, (void*)m_pipelineState, (void*)m_atlas);
        return;
    }

    if (renderCallCount < 5) {
        spdlog::info("TextRenderer::Render - rendering {} instances", m_instances.size());
        renderCallCount++;
    }

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
