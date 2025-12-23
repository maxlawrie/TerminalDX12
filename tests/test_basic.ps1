#Requires -Version 5.1

# TerminalDX12 Basic Test Suite (PowerShell)
#
# These are fast smoke tests that verify basic build artifacts and stress scenarios
# not covered by the Python test suite. They work without Python dependencies.
#
# Tests covered by Python (test_mouse.py, test_resize.py):
#   - Terminal launches and stays running (via pytest fixture)
#   - Window appears with valid handle (via pytest fixture)
#   - Mouse input, scrolling, resize/reflow functionality
#
# Unique tests in this file:
#   - TerminalExists: Verify build output exists
#   - DependenciesExist: Verify DLLs and shaders are present
#   - MultipleInstances: Run multiple terminals simultaneously
#   - QuickStartStop: Rapid start/stop stress test

$ErrorActionPreference = "Stop"

$TerminalExe = Join-Path (Split-Path -Parent $PSScriptRoot) "build\bin\Release\TerminalDX12.exe"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogFile = Join-Path $ScriptDir "test_basic_output.txt"

# Start transcript to capture all output
Start-Transcript -Path $LogFile -Force | Out-Null
Write-Host "Log file: $LogFile"
Write-Host "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 Basic Test Suite" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

function Test-TerminalExists {
    Write-Host "Test 1: Checking if terminal executable exists..." -NoNewline

    if (Test-Path $TerminalExe) {
        Write-Host " PASSED" -ForegroundColor Green
        return $true
    } else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Terminal not found at: $TerminalExe" -ForegroundColor Yellow
        return $false
    }
}

function Test-DependenciesExist {
    Write-Host "Test 2: Checking if dependencies exist..." -NoNewline

    $exeDir = Split-Path $TerminalExe
    $requiredFiles = @(
        "freetype.dll",
        "spdlog.dll",
        "shaders\GlyphVertex.cso",
        "shaders\GlyphPixel.cso"
    )

    $allExist = $true
    $missing = @()

    foreach ($file in $requiredFiles) {
        $fullPath = Join-Path $exeDir $file
        if (-not (Test-Path $fullPath)) {
            $allExist = $false
            $missing += $file
        }
    }

    if ($allExist) {
        Write-Host " PASSED" -ForegroundColor Green
        return $true
    } else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Missing files:" -ForegroundColor Yellow
        foreach ($file in $missing) {
            Write-Host "    - $file" -ForegroundColor Yellow
        }
        return $false
    }
}

function Test-MultipleInstances {
    Write-Host "Test 3: Testing multiple instances..." -NoNewline

    try {
        # Start three instances
        $process1 = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 1
        $process2 = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 1
        $process3 = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 2

        # Check if all are running
        $allRunning = (-not $process1.HasExited) -and (-not $process2.HasExited) -and (-not $process3.HasExited)

        if ($allRunning) {
            Write-Host " PASSED" -ForegroundColor Green
            $testResult = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  One or more instances crashed" -ForegroundColor Yellow
            $testResult = $false
        }

        # Clean up
        foreach ($proc in @($process1, $process2, $process3)) {
            if ($proc -and -not $proc.HasExited) {
                $proc.Kill()
                $proc.WaitForExit(2000)
            }
        }

        return $testResult
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        return $false
    }
}

function Test-QuickStartStop {
    Write-Host "Test 4: Testing quick start/stop cycles (10 cycles)..." -NoNewline

    $testResult = $true

    for ($i = 1; $i -le 10; $i++) {
        try {
            $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
            Start-Sleep -Milliseconds 300

            if ($process.HasExited) {
                Write-Host " FAILED" -ForegroundColor Red
                Write-Host "  Terminal exited prematurely on cycle $i" -ForegroundColor Yellow
                $testResult = $false
                break
            }

            $process.Kill()
            $process.WaitForExit(1000)
        } catch {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  Error on cycle $i`: $_" -ForegroundColor Yellow
            $testResult = $false
            break
        }
    }

    if ($testResult) {
        Write-Host " PASSED" -ForegroundColor Green
    }

    return $testResult
}

# Run all tests
Write-Host ""
$results = [ordered]@{
    'TerminalExists' = Test-TerminalExists
    'DependenciesExist' = Test-DependenciesExist
    'MultipleInstances' = Test-MultipleInstances
    'QuickStartStop' = Test-QuickStartStop
}

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TEST SUMMARY" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

$passed = ($results.Values | Where-Object { $_ -eq $true }).Count
$total = $results.Count

foreach ($test in $results.GetEnumerator()) {
    $status = if ($test.Value) { "PASS" } else { "FAIL" }
    $color = if ($test.Value) { "Green" } else { "Red" }
    $symbol = if ($test.Value) { "[+]" } else { "[-]" }

    Write-Host "$symbol $($test.Key): $status" -ForegroundColor $color
}

Write-Host ""
Write-Host "Results: $passed/$total tests passed ($([math]::Round(100 * $passed / $total))%)" -ForegroundColor $(if ($passed -eq $total) { "Green" } else { "Yellow" })
Write-Host ""

# Exit code
Write-Host ""
Write-Host "Ended: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Host "Log file saved to: $LogFile"
Stop-Transcript | Out-Null

if ($passed -eq $total) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Some tests failed!" -ForegroundColor Red
    exit 1
}
