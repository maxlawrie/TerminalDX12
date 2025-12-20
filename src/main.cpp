#include "core/Application.h"
#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdlib>

using namespace TerminalDX12;
using Microsoft::WRL::ComPtr;

int WINAPI wWinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nShowCmd)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nShowCmd;

    // Enable debug layer in debug builds
#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();
        }
    }
#endif

    // Create and run application
    Core::Application app;

    if (!app.Initialize()) {
        MessageBoxW(nullptr,
            L"Failed to initialize application!",
            L"Error",
            MB_OK | MB_ICONERROR);
        return EXIT_FAILURE;
    }

    int exitCode = app.Run();

    app.Shutdown();

    return exitCode;
}
