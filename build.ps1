#Requires -Version 5.1

<#
.SYNOPSIS
    Builds TerminalDX12 using CMake and Visual Studio.
.DESCRIPTION
    Configures and builds the TerminalDX12 project using CMake with vcpkg integration.
    Requires VCPKG_ROOT environment variable to be set.
.PARAMETER Configuration
    Build configuration: Debug or Release. Default is Release.
.PARAMETER Clean
    If specified, removes the build directory before building.
.PARAMETER Generator
    Visual Studio generator to use. Default is "Visual Studio 18 2026".
.EXAMPLE
    .\build.ps1
    Builds Release configuration.
.EXAMPLE
    .\build.ps1 -Configuration Debug -Clean
    Cleans and builds Debug configuration.
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Clean,

    [string]$Generator = "Visual Studio 18 2026"
)

$ErrorActionPreference = "Stop"

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
Write-Host "Configuration: $Configuration" -ForegroundColor Green
Write-Host ""

# Clean if requested
if ($Clean -and (Test-Path "build")) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force "build"
}

# Create build directory
if (-not (Test-Path "build")) {
    New-Item -ItemType Directory -Path "build" | Out-Null
}

try {
    Push-Location build

    # Configure with CMake
    Write-Host "Configuring project with CMake..." -ForegroundColor Yellow
    cmake .. `
        -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
        -G $Generator `
        -A x64

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ERROR: CMake configuration failed!" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "Configuration successful!" -ForegroundColor Green
    Write-Host ""
    Write-Host "Building $Configuration configuration..." -ForegroundColor Yellow
    cmake --build . --config $Configuration

    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host "ERROR: Build failed!" -ForegroundColor Red
        exit 1
    }

    Write-Host ""
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "========================================" -ForegroundColor Cyan
    Write-Host "Executable: build\bin\$Configuration\TerminalDX12.exe" -ForegroundColor Yellow
    Write-Host ""
}
finally {
    Pop-Location
}
