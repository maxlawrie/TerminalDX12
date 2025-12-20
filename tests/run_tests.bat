@echo off
REM TerminalDX12 Test Runner
REM Runs the automated test suite

echo ========================================
echo TerminalDX12 Automated Test Suite
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python is not installed or not in PATH
    echo Please install Python 3.8 or higher
    pause
    exit /b 1
)

REM Check if dependencies are installed
pip show Pillow >nul 2>&1
if errorlevel 1 (
    echo Installing dependencies...
    pip install -r requirements.txt
    if errorlevel 1 (
        echo ERROR: Failed to install dependencies
        pause
        exit /b 1
    )
)

echo Running tests...
echo.

python test_terminal.py

echo.
echo ========================================
echo Tests complete!
echo Check the screenshots folder for visual verification
echo ========================================
pause
