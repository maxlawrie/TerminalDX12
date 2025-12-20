# Build script for TerminalDX12 with VS 2026 Insiders

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Paths
$vcpkgRoot = "C:\vcpkg"
$cmake = "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
$projectRoot = $PSScriptRoot

# Set VCPKG_ROOT
$env:VCPKG_ROOT = $vcpkgRoot

Write-Host "Using vcpkg from: $vcpkgRoot" -ForegroundColor Green
Write-Host "Using CMake from: $cmake" -ForegroundColor Green
Write-Host ""

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

Set-Location build

# Configure with CMake
Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
& $cmake .. `
    -DCMAKE_TOOLCHAIN_FILE="$vcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
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
& $cmake --build . --config Release

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
