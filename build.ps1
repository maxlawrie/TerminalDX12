# Build script for TerminalDX12 (PowerShell)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if VCPKG_ROOT is set
if (-not $env:VCPKG_ROOT) {
    Write-Host "ERROR: VCPKG_ROOT environment variable is not set!" -ForegroundColor Red
    Write-Host "Please set it to your vcpkg installation directory." -ForegroundColor Yellow
    Write-Host "Example: `$env:VCPKG_ROOT = 'C:\vcpkg'" -ForegroundColor Yellow
    exit 1
}

Write-Host "Using vcpkg from: $env:VCPKG_ROOT" -ForegroundColor Green
Write-Host ""

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# Configure with CMake
Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
cmake .. `
    -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
    -G "Visual Studio 18 2026" `
    -A x64

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "Configuration successful!" -ForegroundColor Green
Write-Host ""
Write-Host "Building Release configuration..." -ForegroundColor Yellow
cmake --build . --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "ERROR: Build failed!" -ForegroundColor Red
    Set-Location ..
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Build successful!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Executable: build\bin\Release\TerminalDX12.exe" -ForegroundColor Yellow
Write-Host ""

Set-Location ..
