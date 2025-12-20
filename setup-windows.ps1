# Setup script for TerminalDX12 build environment
# Run this in PowerShell as Administrator

param(
    [switch]$SkipVisualStudio,
    [switch]$SkipVcpkg
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 - Windows Setup Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "WARNING: Not running as Administrator. Some installations may fail." -ForegroundColor Yellow
    Write-Host "Please run PowerShell as Administrator for best results." -ForegroundColor Yellow
    Write-Host ""
}

# Check for Visual Studio 2026 or 2022
Write-Host "Checking for Visual Studio 2026/2022..." -ForegroundColor Yellow
$vsPath2026 = "C:\Program Files\Microsoft Visual Studio\2026"
$vsPath2022 = "C:\Program Files\Microsoft Visual Studio\2022"

if (Test-Path $vsPath2026) {
    Write-Host "✓ Visual Studio 2026 found!" -ForegroundColor Green
} elseif (Test-Path $vsPath2022) {
    Write-Host "✓ Visual Studio 2022 found!" -ForegroundColor Green
} else {
    if ($SkipVisualStudio) {
        Write-Host "✗ Visual Studio 2026/2022 not found (skipped)" -ForegroundColor Yellow
    } else {
        Write-Host "✗ Visual Studio 2026/2022 not found" -ForegroundColor Red
        Write-Host ""
        Write-Host "Please install Visual Studio 2026 Community Edition (latest):" -ForegroundColor Yellow
        Write-Host "1. Download from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Cyan
        Write-Host "2. During installation, select:" -ForegroundColor Cyan
        Write-Host "   - Desktop development with C++" -ForegroundColor Cyan
        Write-Host "   - Windows 10 SDK (latest)" -ForegroundColor Cyan
        Write-Host "   - C++ CMake tools" -ForegroundColor Cyan
        Write-Host ""

        $response = Read-Host "Open download page in browser? (Y/N)"
        if ($response -eq "Y" -or $response -eq "y") {
            Start-Process "https://visualstudio.microsoft.com/downloads/"
        }

        Write-Host ""
        Write-Host "After installing Visual Studio, run this script again." -ForegroundColor Yellow
        exit 1
    }
}

# Check for Git
Write-Host ""
Write-Host "Checking for Git..." -ForegroundColor Yellow
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) {
    Write-Host "✓ Git found at: $($git.Source)" -ForegroundColor Green
} else {
    Write-Host "✗ Git not found" -ForegroundColor Red
    Write-Host "Installing Git via winget..." -ForegroundColor Yellow

    try {
        winget install --id Git.Git -e --source winget
        Write-Host "✓ Git installed!" -ForegroundColor Green
        Write-Host "NOTE: You may need to restart your terminal for Git to be available." -ForegroundColor Yellow
    } catch {
        Write-Host "Failed to install Git automatically." -ForegroundColor Red
        Write-Host "Please download and install from: https://git-scm.com/download/win" -ForegroundColor Yellow
        exit 1
    }
}

# Install vcpkg
Write-Host ""
Write-Host "Checking for vcpkg..." -ForegroundColor Yellow
if (Test-Path "C:\vcpkg") {
    Write-Host "✓ vcpkg found at C:\vcpkg" -ForegroundColor Green

    # Update vcpkg
    Write-Host "Updating vcpkg..." -ForegroundColor Yellow
    Push-Location C:\vcpkg
    git pull
    .\bootstrap-vcpkg.bat
    Pop-Location
} else {
    if ($SkipVcpkg) {
        Write-Host "✗ vcpkg not found (skipped)" -ForegroundColor Yellow
    } else {
        Write-Host "Installing vcpkg to C:\vcpkg..." -ForegroundColor Yellow

        try {
            # Clone vcpkg
            Push-Location C:\
            git clone https://github.com/Microsoft/vcpkg.git

            # Bootstrap
            Push-Location C:\vcpkg
            .\bootstrap-vcpkg.bat

            if ($LASTEXITCODE -eq 0) {
                Write-Host "✓ vcpkg installed successfully!" -ForegroundColor Green
            } else {
                throw "vcpkg bootstrap failed"
            }

            Pop-Location
            Pop-Location
        } catch {
            Write-Host "✗ Failed to install vcpkg" -ForegroundColor Red
            Write-Host "Error: $_" -ForegroundColor Red
            exit 1
        }
    }
}

# Set VCPKG_ROOT environment variable
Write-Host ""
Write-Host "Setting VCPKG_ROOT environment variable..." -ForegroundColor Yellow
$vcpkgRoot = "C:\vcpkg"
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', $vcpkgRoot, 'User')
$env:VCPKG_ROOT = $vcpkgRoot
Write-Host "✓ VCPKG_ROOT set to: $vcpkgRoot" -ForegroundColor Green

# Check for CMake (usually installed with Visual Studio)
Write-Host ""
Write-Host "Checking for CMake..." -ForegroundColor Yellow
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    Write-Host "✓ CMake found at: $($cmake.Source)" -ForegroundColor Green
} else {
    Write-Host "✗ CMake not found in PATH" -ForegroundColor Yellow
    Write-Host "CMake is usually installed with Visual Studio." -ForegroundColor Yellow
    Write-Host "If Visual Studio is installed, CMake should be available after restarting your terminal." -ForegroundColor Yellow
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Setup Summary" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$status = @()
if ((Test-Path $vsPath2026) -or (Test-Path $vsPath2022)) { $status += "✓ Visual Studio 2026/2022" } else { $status += "✗ Visual Studio 2026/2022" }
if (Get-Command git -ErrorAction SilentlyContinue) { $status += "✓ Git" } else { $status += "✗ Git" }
if (Test-Path "C:\vcpkg") { $status += "✓ vcpkg" } else { $status += "✗ vcpkg" }
if (Get-Command cmake -ErrorAction SilentlyContinue) { $status += "✓ CMake" } else { $status += "✗ CMake" }

foreach ($s in $status) {
    if ($s.StartsWith("✓")) {
        Write-Host $s -ForegroundColor Green
    } else {
        Write-Host $s -ForegroundColor Red
    }
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Ensure all items above show ✓" -ForegroundColor Cyan
Write-Host "2. Restart your PowerShell terminal (to refresh environment)" -ForegroundColor Cyan
Write-Host "3. Navigate to the TerminalDX12 directory" -ForegroundColor Cyan
Write-Host "4. Run: .\build.ps1" -ForegroundColor Cyan
Write-Host ""
