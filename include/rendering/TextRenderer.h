#pragma once

#include "rendering/GlyphAtlas.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace TerminalDX12::Rendering {

// Per-instance data for GPU rendering
struct GlyphInstance {
    XMFLOAT2 position;     // Screen position (pixels)
    XMFLOAT2 size;         // Glyph size (pixels)
    XMFLOAT2 uvMin;        // Atlas UV coordinates (top-left)
    XMFLOAT2 uvMax;        // Atlas UV coordinates (bottom-right)
    XMFLOAT4 color;        // Text color (RGBA)
};

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    bool Initialize(ID3D12Device* device, GlyphAtlas* atlas, int screenWidth, int screenHeight);
    void Shutdown();

    // Render text at position
    void RenderText(const std::string& text, float x, float y, const XMFLOAT4& color);
    void RenderText(const std::u32string& text, float x, float y, const XMFLOAT4& color);

    // Clear the instance buffer
    void Clear();

    // Upload instance data to GPU and record draw commands
    void Render(ID3D12GraphicsCommandList* commandList);

    // Update screen dimensions (for coordinate transformation)
    void SetScreenSize(int width, int height);

    // Get/set root signature and PSO (managed by DX12Renderer)
    void SetRootSignature(ID3D12RootSignature* rootSignature) { m_rootSignature = rootSignature; }
    void SetPipelineState(ID3D12PipelineState* pso) { m_pipelineState = pso; }

private:
    bool CreateInstanceBuffer(ID3D12Device* device);
    void UpdateInstanceBuffer();

    // Glyph atlas reference
    GlyphAtlas* m_atlas;

    // Screen dimensions for coordinate transformation
    int m_screenWidth;
    int m_screenHeight;

    // Instance data (CPU-side)
    std::vector<GlyphInstance> m_instances;
    static constexpr size_t MAX_INSTANCES = 10000;  // Max glyphs per frame

    // D3D12 resources
    ComPtr<ID3D12Resource> m_instanceBuffer;
    ComPtr<ID3D12Resource> m_instanceUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;

    // Pipeline state (set by DX12Renderer)
    ID3D12RootSignature* m_rootSignature;
    ID3D12PipelineState* m_pipelineState;

    // Cached device
    ID3D12Device* m_device;
};

} // namespace TerminalDX12::Rendering
