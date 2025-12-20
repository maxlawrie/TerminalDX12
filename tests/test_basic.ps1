# TerminalDX12 Basic Test Suite (PowerShell)
# Simple tests that work without Python dependencies

$ErrorActionPreference = "Stop"

$TerminalExe = "C:\Temp\TerminalDX12Test\TerminalDX12.exe"
$TestResults = @()

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

function Test-TerminalLaunches {
    Write-Host "Test 3: Testing if terminal launches..." -NoNewline

    try {
        # Start the terminal
        $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal

        # Wait a bit for it to initialize
        Start-Sleep -Seconds 3

        # Check if it's still running
        if (-not $process.HasExited) {
            Write-Host " PASSED" -ForegroundColor Green

            # Kill it
            $process.Kill()
            $process.WaitForExit(2000)

            return $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  Terminal exited with code: $($process.ExitCode)" -ForegroundColor Yellow
            return $false
        }
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        return $false
    }
}

function Test-WindowAppears {
    Write-Host "Test 4: Testing if window appears..." -NoNewline

    try {
        # Start the terminal
        $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal

        # Wait for window to appear
        Start-Sleep -Seconds 2

        # Try to find the window
        Add-Type @"
            using System;
            using System.Runtime.InteropServices;
            public class WindowFinder {
                [DllImport("user32.dll", SetLastError = true)]
                public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
            }
"@

        # Look for window with "TerminalDX12" in title
        $hwnd = [WindowFinder]::FindWindow($null, "TerminalDX12 - GPU-Accelerated Terminal Emulator")

        if ($hwnd -ne [IntPtr]::Zero) {
            Write-Host " PASSED" -ForegroundColor Green
            $result = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  Window not found with expected title" -ForegroundColor Yellow
            $result = $false
        }

        # Clean up
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit(2000)
        }

        return $result
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow

        # Try to clean up
        try {
            if ($process -and -not $process.HasExited) {
                $process.Kill()
            }
        } catch {}

        return $false
    }
}

function Test-MultipleInstances {
    Write-Host "Test 5: Testing multiple instances..." -NoNewline

    try {
        # Start two instances
        $process1 = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 2
        $process2 = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
        Start-Sleep -Seconds 2

        # Check if both are running
        if ((-not $process1.HasExited) -and (-not $process2.HasExited)) {
            Write-Host " PASSED" -ForegroundColor Green
            $result = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            $result = $false
        }

        # Clean up
        if (-not $process1.HasExited) {
            $process1.Kill()
            $process1.WaitForExit(2000)
        }
        if (-not $process2.HasExited) {
            $process2.Kill()
            $process2.WaitForExit(2000)
        }

        return $result
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        return $false
    }
}

function Test-QuickStartStop {
    Write-Host "Test 6: Testing quick start/stop cycles..." -NoNewline

    $allPassed = $true

    for ($i = 1; $i -le 5; $i++) {
        try {
            $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
            Start-Sleep -Milliseconds 500

            if ($process.HasExited) {
                $allPassed = $false
                break
            }

            $process.Kill()
            $process.WaitForExit(1000)
        } catch {
            $allPassed = $false
            break
        }
    }

    if ($allPassed) {
        Write-Host " PASSED" -ForegroundColor Green
    } else {
        Write-Host " FAILED" -ForegroundColor Red
    }

    return $allPassed
}

function Test-LongerSession {
    Write-Host "Test 7: Testing longer session (10 seconds)..." -NoNewline

    try {
        $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal

        # Let it run for 10 seconds
        for ($i = 0; $i -lt 10; $i++) {
            Start-Sleep -Seconds 1

            if ($process.HasExited) {
                Write-Host " FAILED" -ForegroundColor Red
                Write-Host "  Terminal crashed after $i seconds (exit code: $($process.ExitCode))" -ForegroundColor Yellow
                return $false
            }
        }

        Write-Host " PASSED" -ForegroundColor Green

        # Clean up
        $process.Kill()
        $process.WaitForExit(2000)

        return $true
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        return $false
    }
}

# Run all tests
Write-Host ""
$results = @{
    'TerminalExists' = Test-TerminalExists
    'DependenciesExist' = Test-DependenciesExist
    'TerminalLaunches' = Test-TerminalLaunches
    'WindowAppears' = Test-WindowAppears
    'MultipleInstances' = Test-MultipleInstances
    'QuickStartStop' = Test-QuickStartStop
    'LongerSession' = Test-LongerSession
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
if ($passed -eq $total) {
    Write-Host "All tests passed!" -ForegroundColor Green
    exit 0
} else {
    Write-Host "Some tests failed!" -ForegroundColor Red
    exit 1
}
