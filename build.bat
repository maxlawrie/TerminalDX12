@echo off
REM Build script for TerminalDX12

echo ========================================
echo TerminalDX12 Build Script
echo ========================================
echo.

REM Check if VCPKG_ROOT is set
if not defined VCPKG_ROOT (
    echo ERROR: VCPKG_ROOT environment variable is not set!
    echo Please set it to your vcpkg installation directory.
    echo Example: set VCPKG_ROOT=C:\vcpkg
    exit /b 1
)

echo Using vcpkg from: %VCPKG_ROOT%
echo.

REM Create build directory
if not exist build mkdir build
cd build

REM Configure with CMake
echo Configuring project with CMake...
cmake .. ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
    -G "Visual Studio 18 2026" ^
    -A x64

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: CMake configuration failed!
    exit /b 1
)

echo.
echo Configuration successful!
echo.
echo Building Release configuration...
cmake --build . --config Release

if %ERRORLEVEL% neq 0 (
    echo.
    echo ERROR: Build failed!
    exit /b 1
)

echo.
echo ========================================
echo Build successful!
echo ========================================
echo Executable: build\bin\Release\TerminalDX12.exe
echo.

cd ..
