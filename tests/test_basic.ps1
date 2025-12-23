# TerminalDX12 Basic Test Suite (PowerShell)
# Simple tests that work without Python dependencies

$ErrorActionPreference = "Stop"

$TerminalExe = Join-Path (Split-Path -Parent $PSScriptRoot) "build\bin\Release\TerminalDX12.exe"
$TestResults = @()
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogFile = Join-Path $ScriptDir "test_basic_output.txt"

# Start transcript to capture all output
Start-Transcript -Path $LogFile -Force | Out-Null
Write-Host "Log file: $LogFile"
Write-Host "Started: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

function Verify-LogContains {
    param(
        [string]$TestName,
        [string[]]$ExpectedMarkers
    )

    # Stop and restart transcript to flush content
    Stop-Transcript | Out-Null
    Start-Transcript -Path $LogFile -Append | Out-Null

    # Read the log file
    if (-not (Test-Path $LogFile)) {
        Write-Host "  LOG VERIFICATION FAILED: Log file does not exist" -ForegroundColor Red
        return $false
    }

    $logContent = Get-Content $LogFile -Raw -ErrorAction SilentlyContinue
    if (-not $logContent) {
        Write-Host "  LOG VERIFICATION FAILED: Log file is empty" -ForegroundColor Red
        return $false
    }

    # Check for expected markers
    $allFound = $true
    foreach ($marker in $ExpectedMarkers) {
        if ($logContent -notlike "*$marker*") {
            Write-Host "  LOG VERIFICATION FAILED: Missing '$marker' in log" -ForegroundColor Red
            $allFound = $false
        }
    }

    if ($allFound) {
        Write-Host "  Log verification: OK" -ForegroundColor Gray
    }

    return $allFound
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "TerminalDX12 Basic Test Suite" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

function Test-TerminalExists {
    Write-Host "Test 1: Checking if terminal executable exists..." -NoNewline

    if (Test-Path $TerminalExe) {
        Write-Host " PASSED" -ForegroundColor Green
        $testResult = $true
    } else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Terminal not found at: $TerminalExe" -ForegroundColor Yellow
        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "TerminalExists" -ExpectedMarkers @("Test 1:", "terminal executable exists")
    return ($testResult -and $logVerified)
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
        $testResult = $true
    } else {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Missing files:" -ForegroundColor Yellow
        foreach ($file in $missing) {
            Write-Host "    - $file" -ForegroundColor Yellow
        }
        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "DependenciesExist" -ExpectedMarkers @("Test 2:", "dependencies exist")
    return ($testResult -and $logVerified)
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

            $testResult = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  Terminal exited with code: $($process.ExitCode)" -ForegroundColor Yellow
            $testResult = $false
        }
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "TerminalLaunches" -ExpectedMarkers @("Test 3:", "terminal launches")
    return ($testResult -and $logVerified)
}

function Test-WindowAppears {
    Write-Host "Test 4: Testing if window appears..." -NoNewline

    try {
        # Start the terminal
        $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal

        # Wait for window to appear
        Start-Sleep -Seconds 2

        # Check if the process has a main window
        $process.Refresh()
        if ($process.MainWindowHandle -ne [IntPtr]::Zero) {
            Write-Host " PASSED" -ForegroundColor Green
            $testResult = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            Write-Host "  Process has no main window handle" -ForegroundColor Yellow
            $testResult = $false
        }

        # Clean up
        if (-not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit(2000)
        }
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow

        # Try to clean up
        try {
            if ($process -and -not $process.HasExited) {
                $process.Kill()
            }
        } catch {}

        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "WindowAppears" -ExpectedMarkers @("Test 4:", "window appears")
    return ($testResult -and $logVerified)
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
            $testResult = $true
        } else {
            Write-Host " FAILED" -ForegroundColor Red
            $testResult = $false
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
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "MultipleInstances" -ExpectedMarkers @("Test 5:", "multiple instances")
    return ($testResult -and $logVerified)
}

function Test-QuickStartStop {
    Write-Host "Test 6: Testing quick start/stop cycles..." -NoNewline

    $testResult = $true

    for ($i = 1; $i -le 5; $i++) {
        try {
            $process = Start-Process -FilePath $TerminalExe -PassThru -WindowStyle Normal
            Start-Sleep -Milliseconds 500

            if ($process.HasExited) {
                $testResult = $false
                break
            }

            $process.Kill()
            $process.WaitForExit(1000)
        } catch {
            $testResult = $false
            break
        }
    }

    if ($testResult) {
        Write-Host " PASSED" -ForegroundColor Green
    } else {
        Write-Host " FAILED" -ForegroundColor Red
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "QuickStartStop" -ExpectedMarkers @("Test 6:", "quick start/stop")
    return ($testResult -and $logVerified)
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
                $testResult = $false
                break
            }
        }

        if (-not $process.HasExited) {
            Write-Host " PASSED" -ForegroundColor Green
            $testResult = $true

            # Clean up
            $process.Kill()
            $process.WaitForExit(2000)
        }
    } catch {
        Write-Host " FAILED" -ForegroundColor Red
        Write-Host "  Error: $_" -ForegroundColor Yellow
        $testResult = $false
    }

    # Verify log output
    $logVerified = Verify-LogContains -TestName "LongerSession" -ExpectedMarkers @("Test 7:", "longer session")
    return ($testResult -and $logVerified)
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
