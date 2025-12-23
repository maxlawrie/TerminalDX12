#Requires -Version 5.1

<#
.SYNOPSIS
    Builds TerminalDX12 using Visual Studio Insiders/Preview.
.DESCRIPTION
    Configures and builds the TerminalDX12 project using CMake with VS Insiders.
    Automatically detects VS Insiders installation path and CMake location.
.PARAMETER Configuration
    Build configuration: Debug or Release. Default is Release.
.PARAMETER Clean
    If specified, removes the build directory before building.
.PARAMETER VcpkgRoot
    Path to vcpkg installation. Default is C:\vcpkg.
.EXAMPLE
    .\build-insiders.ps1
    Builds Release configuration with VS Insiders.
.EXAMPLE
    .\build-insiders.ps1 -Configuration Debug -Clean
    Cleans and builds Debug configuration.
#>

[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Clean,

    [string]$VcpkgRoot = "C:\vcpkg"
)

$ErrorActionPreference = "Stop"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 Build Script (VS Insiders)" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find Visual Studio Insiders/Preview installation
function Find-VSInsiders {
    $searchPaths = @(
        "C:\Program Files\Microsoft Visual Studio\18\Insiders",
        "C:\Program Files\Microsoft Visual Studio\2026\Preview",
        "C:\Program Files\Microsoft Visual Studio\2022\Preview",
        "C:\Program Files (x86)\Microsoft Visual Studio\18\Insiders",
        "C:\Program Files (x86)\Microsoft Visual Studio\2026\Preview"
    )
    
    foreach ($path in $searchPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    return $null
}

# Find CMake in VS installation
function Find-CMake {
    param([string]$VSPath)
    
    $cmakePath = Join-Path $VSPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $cmakePath) {
        return $cmakePath
    }
    
    # Fallback to PATH
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }
    
    return $null
}

# Find VS installation
$vsPath = Find-VSInsiders
if (-not $vsPath) {
    Write-Host "ERROR: Visual Studio Insiders/Preview not found!" -ForegroundColor Red
    Write-Host "Searched locations:" -ForegroundColor Yellow
    Write-Host "  - C:\Program Files\Microsoft Visual Studio\18\Insiders" -ForegroundColor Yellow
    Write-Host "  - C:\Program Files\Microsoft Visual Studio\2026\Preview" -ForegroundColor Yellow
    exit 1
}
Write-Host "Using Visual Studio from: $vsPath" -ForegroundColor Green

# Find CMake
$cmake = Find-CMake -VSPath $vsPath
if (-not $cmake) {
    Write-Host "ERROR: CMake not found in VS installation or PATH!" -ForegroundColor Red
    exit 1
}
Write-Host "Using CMake from: $cmake" -ForegroundColor Green

# Set vcpkg
if (-not (Test-Path $VcpkgRoot)) {
    Write-Host "ERROR: vcpkg not found at $VcpkgRoot" -ForegroundColor Red
    exit 1
}
$env:VCPKG_ROOT = $VcpkgRoot
Write-Host "Using vcpkg from: $VcpkgRoot" -ForegroundColor Green
Write-Host "Configuration: $Configuration" -ForegroundColor Green
Write-Host ""

# Determine generator based on VS version
$generator = if ($vsPath -match "\18\\") { "Visual Studio 18 2026" } else { "Visual Studio 17 2022" }

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
    & $cmake .. `
        -DCMAKE_TOOLCHAIN_FILE="$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
        -G $generator `
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
    & $cmake --build . --config $Configuration

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
