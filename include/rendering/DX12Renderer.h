#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <memory>
#include <array>
#include <string>

using Microsoft::WRL::ComPtr;

namespace TerminalDX12 {

// Forward declarations
namespace Core { class Window; }

namespace Rendering {

// Forward declarations
class GlyphAtlas;
class TextRenderer;

class DX12Renderer {
public:
    DX12Renderer();
    ~DX12Renderer();

    [[nodiscard]] bool Initialize(Core::Window* window);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Present();

    void Resize(int width, int height);

    // Text rendering
    void RenderText(const std::string& text, float x, float y, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
    void RenderChar(const std::string& ch, float x, float y, float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);
    void RenderRect(float x, float y, float width, float height, float r, float g, float b, float a = 1.0f);
    void ClearText();

    // Font management
    bool ReloadFont(const std::string& fontPath, int fontSize);
    int GetCharWidth() const;
    int GetLineHeight() const;

    ID3D12Device* GetDevice() { return m_device.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() { return GetCurrentFrameResource().commandList.Get(); }

private:
    static constexpr UINT FRAME_COUNT = 3; // Triple buffering

    struct FrameResource {
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ComPtr<ID3D12GraphicsCommandList> commandList;
        ComPtr<ID3D12Resource> renderTarget;
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle;
        UINT64 fenceValue;
    };

    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRTVDescriptorHeap();
    bool CreateFrameResources();
    bool CreateFence();

    // Text rendering pipeline
    bool CreateTextRenderingPipeline();
    bool CreateTextRootSignature();
    bool CreateTextPipelineState();

    void WaitForGPU();
    void MoveToNextFrame();
    FrameResource& GetCurrentFrameResource();

    // Core D3D12 objects
    ComPtr<ID3D12Device2> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGISwapChain4> m_swapChain;

    // Descriptor heaps
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize;

    // Frame resources
    std::array<FrameResource, FRAME_COUNT> m_frameResources;
    UINT m_currentFrameIndex;

    // Synchronization
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    // Window dimensions
    int m_width;
    int m_height;
    bool m_vsyncEnabled;

    // Resize state - defer resize if in a frame
    bool m_inFrame = false;
    bool m_pendingResize = false;
    int m_pendingWidth = 0;
    int m_pendingHeight = 0;

    // Text rendering
    std::unique_ptr<GlyphAtlas> m_glyphAtlas;
    std::unique_ptr<TextRenderer> m_textRenderer;
    ComPtr<ID3D12RootSignature> m_textRootSignature;
    ComPtr<ID3D12PipelineState> m_textPipelineState;
};

} // namespace Rendering
} // namespace TerminalDX12
