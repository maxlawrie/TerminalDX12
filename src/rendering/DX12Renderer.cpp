#include "rendering/DX12Renderer.h"
#include "rendering/GlyphAtlas.h"
#include "rendering/TextRenderer.h"
#include "core/Window.h"
#include <stdexcept>
#include <spdlog/spdlog.h>
#include <d3dcompiler.h>
#include <fstream>
#include <Windows.h>

#pragma comment(lib, "d3d12.lib")

namespace {
    // Helper function to get the executable directory
    std::string GetExecutableDirectory() {
        char buffer[MAX_PATH];
        GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        std::string path(buffer);
        size_t lastSlash = path.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            return path.substr(0, lastSlash + 1);
        }
        return "";
    }
}
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace TerminalDX12 {
namespace Rendering {

DX12Renderer::DX12Renderer()
    : m_currentFrameIndex(0)
    , m_fenceEvent(nullptr)
    , m_fenceValue(0)
    , m_rtvDescriptorSize(0)
    , m_width(0)
    , m_height(0)
    , m_vsyncEnabled(true)
{
}

DX12Renderer::~DX12Renderer() {
    Shutdown();
}

bool DX12Renderer::Initialize(Core::Window* window) {
    if (!window) {
        return false;
    }

    m_width = window->GetWidth();
    m_height = window->GetHeight();

    if (!CreateDevice()) {
        return false;
    }

    if (!CreateCommandQueue()) {
        return false;
    }

    if (!CreateSwapChain(window->GetHandle(), m_width, m_height)) {
        return false;
    }

    if (!CreateRTVDescriptorHeap()) {
        return false;
    }

    if (!CreateFrameResources()) {
        return false;
    }

    if (!CreateFence()) {
        return false;
    }

    // Create text rendering pipeline
    if (!CreateTextRenderingPipeline()) {
        spdlog::error("Failed to create text rendering pipeline");
        return false;
    }

    return true;
}

void DX12Renderer::Shutdown() {
    // Wait for GPU to finish
    WaitForGPU();

    // Close fence event
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // Release resources (COM smart pointers handle cleanup)
}

bool DX12Renderer::CreateDevice() {
#if defined(_DEBUG)
    // Enable debug layer
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }
#endif

    // Create DXGI factory
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    // Find hardware adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Try to create device
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device));
        if (SUCCEEDED(hr)) {
            break;
        }
    }

    if (!m_device) {
        return false;
    }

    return true;
}

bool DX12Renderer::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    return SUCCEEDED(hr);
}

bool DX12Renderer::CreateSwapChain(HWND hwnd, int width, int height) {
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    // Describe swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = 0;

    ComPtr<IDXGISwapChain1> swapChain1;
    hr = factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        return false;
    }

    // Upgrade to IDXGISwapChain4
    hr = swapChain1.As(&m_swapChain);
    if (FAILED(hr)) {
        return false;
    }

    // Disable Alt+Enter fullscreen toggle (we'll implement our own)
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    m_currentFrameIndex = m_swapChain->GetCurrentBackBufferIndex();

    return true;
}

bool DX12Renderer::CreateRTVDescriptorHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    HRESULT hr = m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));
    if (FAILED(hr)) {
        return false;
    }

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    return true;
}

bool DX12Renderer::CreateFrameResources() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Create resources for each frame
    for (UINT i = 0; i < FRAME_COUNT; ++i) {
        auto& frame = m_frameResources[i];

        // Get render target from swap chain
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&frame.renderTarget));
        if (FAILED(hr)) {
            return false;
        }

        // Create RTV
        m_device->CreateRenderTargetView(frame.renderTarget.Get(), nullptr, rtvHandle);
        frame.rtvHandle = rtvHandle;
        rtvHandle.ptr += m_rtvDescriptorSize;

        // Create command allocator
        hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&frame.commandAllocator)
        );
        if (FAILED(hr)) {
            return false;
        }

        // Create command list (initially in closed state)
        hr = m_device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            frame.commandAllocator.Get(),
            nullptr,
            IID_PPV_ARGS(&frame.commandList)
        );
        if (FAILED(hr)) {
            return false;
        }

        // Close command list (it starts in open state)
        frame.commandList->Close();

        frame.fenceValue = 0;
    }

    return true;
}

bool DX12Renderer::CreateFence() {
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr)) {
        return false;
    }

    m_fenceValue = 1;

    // Create event for fence signaling
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) {
        return false;
    }

    return true;
}

void DX12Renderer::BeginFrame() {
    auto& frame = GetCurrentFrameResource();

    // Wait if GPU is still working on this frame
    if (m_fence->GetCompletedValue() < frame.fenceValue) {
        m_fence->SetEventOnCompletion(frame.fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    // Reset command allocator
    frame.commandAllocator->Reset();

    // Reset command list
    frame.commandList->Reset(frame.commandAllocator.Get(), nullptr);

    // Transition render target to render target state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frame.renderTarget.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    frame.commandList->ResourceBarrier(1, &barrier);

    // Clear render target
    const float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f}; // Black
    frame.commandList->ClearRenderTargetView(frame.rtvHandle, clearColor, 0, nullptr);

    // Set render target
    frame.commandList->OMSetRenderTargets(1, &frame.rtvHandle, FALSE, nullptr);

    // Set viewport and scissor rect
    D3D12_VIEWPORT viewport = {};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect = {};
    scissorRect.left = 0;
    scissorRect.top = 0;
    scissorRect.right = m_width;
    scissorRect.bottom = m_height;

    frame.commandList->RSSetViewports(1, &viewport);
    frame.commandList->RSSetScissorRects(1, &scissorRect);
}

void DX12Renderer::EndFrame() {
    auto& frame = GetCurrentFrameResource();

    // Render text
    if (m_textRenderer) {
        m_textRenderer->Render(frame.commandList.Get());
    }

    // Transition render target to present state
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frame.renderTarget.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    frame.commandList->ResourceBarrier(1, &barrier);

    // Close command list
    frame.commandList->Close();

    // Execute command list
    ID3D12CommandList* commandLists[] = {frame.commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, commandLists);
}

void DX12Renderer::Present() {
    // Present
    UINT syncInterval = m_vsyncEnabled ? 1 : 0;
    m_swapChain->Present(syncInterval, 0);

    // Signal fence
    MoveToNextFrame();
}

void DX12Renderer::Resize(int width, int height) {
    if (width == m_width && height == m_height) {
        return;
    }

    // Wait for GPU to finish
    WaitForGPU();

    // Release render targets
    for (auto& frame : m_frameResources) {
        frame.renderTarget.Reset();
    }

    // Resize swap chain
    HRESULT hr = m_swapChain->ResizeBuffers(
        FRAME_COUNT,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0
    );

    if (FAILED(hr)) {
        return;
    }

    m_width = width;
    m_height = height;

    // Recreate render targets
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FRAME_COUNT; ++i) {
        auto& frame = m_frameResources[i];

        hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&frame.renderTarget));
        if (FAILED(hr)) {
            return;
        }

        m_device->CreateRenderTargetView(frame.renderTarget.Get(), nullptr, rtvHandle);
        frame.rtvHandle = rtvHandle;
        rtvHandle.ptr += m_rtvDescriptorSize;
    }

    m_currentFrameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Update text renderer screen size
    if (m_textRenderer) {
        m_textRenderer->SetScreenSize(width, height);
    }
}

void DX12Renderer::WaitForGPU() {
    // Signal fence
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    // Wait for fence
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    WaitForSingleObject(m_fenceEvent, INFINITE);

    m_fenceValue++;
}

void DX12Renderer::MoveToNextFrame() {
    // Schedule signal
    const UINT64 currentFenceValue = m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

    // Update frame index
    m_currentFrameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Set fence value for current frame
    GetCurrentFrameResource().fenceValue = currentFenceValue;

    m_fenceValue++;
}

DX12Renderer::FrameResource& DX12Renderer::GetCurrentFrameResource() {
    return m_frameResources[m_currentFrameIndex];
}

bool DX12Renderer::CreateTextRenderingPipeline() {
    // Create glyph atlas (using Consolas font - should be available on Windows)
    m_glyphAtlas = std::make_unique<GlyphAtlas>();

    // Try to find a monospace font
    const char* fontPath = "C:\\Windows\\Fonts\\consola.ttf";  // Consolas
    std::ifstream fontTest(fontPath);
    if (!fontTest.good()) {
        fontPath = "C:\\Windows\\Fonts\\cour.ttf";  // Courier New fallback
    }

    if (!m_glyphAtlas->Initialize(m_device.Get(), GetCommandList(), fontPath, 16)) {
        spdlog::error("Failed to initialize glyph atlas");
        return false;
    }

    // Create text renderer
    m_textRenderer = std::make_unique<TextRenderer>();
    if (!m_textRenderer->Initialize(m_device.Get(), m_glyphAtlas.get(), m_width, m_height)) {
        spdlog::error("Failed to initialize text renderer");
        return false;
    }

    // Create root signature
    if (!CreateTextRootSignature()) {
        spdlog::error("Failed to create text root signature");
        return false;
    }

    // Create PSO
    if (!CreateTextPipelineState()) {
        spdlog::error("Failed to create text pipeline state");
        return false;
    }

    // Set pipeline state in text renderer
    m_textRenderer->SetRootSignature(m_textRootSignature.Get());
    m_textRenderer->SetPipelineState(m_textPipelineState.Get());

    spdlog::info("Text rendering pipeline created successfully");
    return true;
}

bool DX12Renderer::CreateTextRootSignature() {
    // Root parameter for screen constants
    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // Root constants for screen dimensions
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParameters[0].Constants.ShaderRegister = 0;
    rootParameters[0].Constants.RegisterSpace = 0;
    rootParameters[0].Constants.Num32BitValues = 4;  // float2 screenSize + float2 padding
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // Descriptor table for atlas texture
    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.RegisterSpace = 0;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static sampler for atlas texture
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Create root signature
    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = 2;
    rootSignatureDesc.pParameters = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &sampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    if (FAILED(hr)) {
        if (error) {
            spdlog::error("Root signature serialization failed: {}", (char*)error->GetBufferPointer());
        }
        return false;
    }

    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_textRootSignature));
    return SUCCEEDED(hr);
}

bool DX12Renderer::CreateTextPipelineState() {
    // Load compiled shaders
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

    // Get executable directory for shader paths
    std::string exeDir = GetExecutableDirectory();
    std::string vsPath = exeDir + "shaders\\GlyphVertex.cso";
    std::string psPath = exeDir + "shaders\\GlyphPixel.cso";

    spdlog::info("Loading shaders from: {}", exeDir);

    // Read vertex shader
    std::ifstream vsFile(vsPath, std::ios::binary);
    if (!vsFile.is_open()) {
        spdlog::error("Failed to open vertex shader: {}", vsPath);
        return false;
    }
    vsFile.seekg(0, std::ios::end);
    size_t vsSize = vsFile.tellg();
    vsFile.seekg(0, std::ios::beg);
    std::vector<char> vsData(vsSize);
    vsFile.read(vsData.data(), vsSize);
    vsFile.close();

    D3DCreateBlob(vsSize, &vertexShader);
    memcpy(vertexShader->GetBufferPointer(), vsData.data(), vsSize);

    // Read pixel shader
    std::ifstream psFile(psPath, std::ios::binary);
    if (!psFile.is_open()) {
        spdlog::error("Failed to open pixel shader: {}", psPath);
        return false;
    }
    psFile.seekg(0, std::ios::end);
    size_t psSize = psFile.tellg();
    psFile.seekg(0, std::ios::beg);
    std::vector<char> psData(psSize);
    psFile.read(psData.data(), psSize);
    psFile.close();

    D3DCreateBlob(psSize, &pixelShader);
    memcpy(pixelShader->GetBufferPointer(), psData.data(), psSize);

    // Define input layout for instance data
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "UVMIN", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "UVMAX", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 }
    };

    // Create PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = m_textRootSignature.Get();
    psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
    psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.RasterizerState.MultisampleEnable = FALSE;
    psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
    psoDesc.RasterizerState.ForcedSampleCount = 0;
    psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    spdlog::info("Creating graphics pipeline state with VS size: {}, PS size: {}", vsSize, psSize);

    HRESULT hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_textPipelineState));
    if (FAILED(hr)) {
        spdlog::error("CreateGraphicsPipelineState failed with HRESULT: 0x{:08X}", static_cast<unsigned int>(hr));
    } else {
        spdlog::info("Graphics pipeline state created successfully");
    }
    return SUCCEEDED(hr);
}

void DX12Renderer::RenderText(const std::string& text, float x, float y, float r, float g, float b, float a) {
    if (m_textRenderer) {
        DirectX::XMFLOAT4 color(r, g, b, a);
        m_textRenderer->RenderText(text, x, y, color);
        static int callCount = 0;
        if (callCount < 10) {
            spdlog::info("RenderText called: \"{}\" at ({}, {})", text, x, y);
            callCount++;
        }
    } else {
        spdlog::warn("RenderText called but m_textRenderer is null");
    }
}

void DX12Renderer::RenderChar(const std::string& ch, float x, float y, float r, float g, float b, float a) {
    if (m_textRenderer) {
        DirectX::XMFLOAT4 color(r, g, b, a);
        m_textRenderer->RenderCharAtCell(ch, x, y, color);
    }
}

void DX12Renderer::RenderRect(float x, float y, float width, float height, float r, float g, float b, float a) {
    if (m_textRenderer) {
        m_textRenderer->RenderRect(x, y, width, height, r, g, b, a);
    }
}

void DX12Renderer::ClearText() {
    if (m_textRenderer) {
        m_textRenderer->Clear();
    }
}

} // namespace Rendering
} // namespace TerminalDX12
