@echo off
REM Test runner script for TerminalDX12
REM Sets up PATH for CMake/CTest and runs the unit tests

setlocal

REM Find Visual Studio CMake path
set VS_CMAKE_PATH=
for %%d in (
    "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    "C:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
    "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
) do (
    if exist "%%~d\ctest.exe" (
        set VS_CMAKE_PATH=%%~d
        goto :found
    )
)

echo ERROR: Could not find Visual Studio CMake installation
echo Please ensure Visual Studio 2022 is installed with CMake support
exit /b 1

:found
echo Found CMake at: %VS_CMAKE_PATH%
set PATH=%VS_CMAKE_PATH%;%PATH%

REM Change to build directory
cd /d "%~dp0build"

REM Run tests
echo.
echo Running C++ unit tests...
echo.
ctest --output-on-failure -C Release

endlocal
