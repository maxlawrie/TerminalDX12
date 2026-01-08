#include "core/Application.h"
#include <Windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdlib>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

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
    (void)nShowCmd;

    // Set up file logging for debugging
    try {
        auto file_logger = spdlog::basic_logger_mt("default", "terminaldx12_debug.log", true);
        spdlog::set_default_logger(file_logger);
        spdlog::set_level(spdlog::level::info);
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("=== TerminalDX12 Starting ===");
    } catch (...) {
        // Continue without file logging if it fails
    }

    // Parse command line - use lpCmdLine as shell if provided, otherwise default to PowerShell
    std::wstring shell = L"powershell.exe";
    if (lpCmdLine && lpCmdLine[0] != L'\0') {
        shell = lpCmdLine;
    }

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

    if (!app.Initialize(shell)) {
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
