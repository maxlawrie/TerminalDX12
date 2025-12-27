# Building TerminalDX12

This guide explains how to build TerminalDX12 on Windows.

## Prerequisites

### 1. Visual Studio 2026

Download and install [Visual Studio 2026](https://visualstudio.microsoft.com/downloads/) with the following components:

- **Desktop development with C++**
- **Windows 10 SDK (10.0.19041.0 or later)**
- **C++ CMake tools for Windows**

### 2. vcpkg Package Manager

vcpkg is required to manage dependencies. Install it:

```powershell
# Clone vcpkg
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Bootstrap vcpkg
.\bootstrap-vcpkg.bat

# (Optional) Integrate with Visual Studio
.\vcpkg integrate install
```

### 3. Set Environment Variable

Set the `VCPKG_ROOT` environment variable:

**PowerShell:**
```powershell
# Temporary (current session)
$env:VCPKG_ROOT = "C:\vcpkg"

# Permanent (all sessions)
[System.Environment]::SetEnvironmentVariable('VCPKG_ROOT', 'C:\vcpkg', 'User')
```

**Command Prompt:**
```cmd
REM Temporary (current session)
set VCPKG_ROOT=C:\vcpkg

REM Permanent (use System Properties > Environment Variables)
setx VCPKG_ROOT "C:\vcpkg"
```

## Building the Project

### Method 1: Using the Build Script (Recommended)

**PowerShell:**
```powershell
cd TerminalDX12
.\build.ps1
```

**Command Prompt:**
```cmd
cd TerminalDX12
build.bat
```

The script will:
1. Check for vcpkg installation
2. Create a `build` directory
3. Configure the project with CMake
4. Build the Release configuration
5. Output the executable to `build\bin\Release\TerminalDX12.exe`

### Method 2: Manual Build

```powershell
# Navigate to project directory
cd TerminalDX12

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake -G "Visual Studio 18 2026" -A x64

# Build Release
cmake --build . --config Release

# Or build Debug
cmake --build . --config Debug
```

### Method 3: Visual Studio IDE

1. Open Visual Studio 2026
2. Select **"Open a local folder"**
3. Navigate to and select the `TerminalDX12` folder
4. Visual Studio will automatically detect CMakeLists.txt
5. Select build configuration (Release/Debug) from the toolbar
6. Build → Build All (Ctrl+Shift+B)

## Running the Application

After a successful build:

```powershell
# From the project root
.\build\bin\Release\TerminalDX12.exe

# Or navigate to the build directory
cd build\bin\Release
.\TerminalDX12.exe
```

## Expected Behavior

After a successful build, the application should:
- ✓ Create a window titled "TerminalDX12 - GPU-Accelerated Terminal Emulator"
- ✓ Initialize DirectX 12 renderer with GPU-accelerated glyph atlas
- ✓ Launch PowerShell (or specified shell) via ConPTY
- ✓ Display terminal output with full color support (16, 256, true color)
- ✓ Handle keyboard input and mouse interaction
- ✓ Support text selection and clipboard (Ctrl+C/V)
- ✓ Respond to window events (resize, close, minimize)
- ✓ Handle DPI scaling properly

## Troubleshooting

### vcpkg Dependencies Not Found

If CMake can't find dependencies:

```powershell
# Install dependencies manually
cd $env:VCPKG_ROOT
.\vcpkg install directx-headers:x64-windows
.\vcpkg install directxshadercompiler:x64-windows
.\vcpkg install directxtk12:x64-windows
.\vcpkg install freetype:x64-windows
.\vcpkg install harfbuzz:x64-windows
.\vcpkg install spdlog:x64-windows
.\vcpkg install gtest:x64-windows
.\vcpkg install nlohmann-json:x64-windows
```

### "Cannot open include file" Errors

Ensure:
1. VCPKG_ROOT is set correctly
2. CMake toolchain file is specified: `-DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake`
3. vcpkg is properly bootstrapped

### DirectX 12 Runtime Errors

If the application fails to initialize DirectX 12:
- Ensure your GPU supports DirectX 12
- Update graphics drivers
- Windows 10 version 1809 or later is required (for ConPTY support)
- Check Windows Update for latest updates

### Build Configuration Issues

If CMake configuration fails:
```powershell
# Clean build directory
cd TerminalDX12
Remove-Item -Recurse -Force build
mkdir build

# Reconfigure
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake -G "Visual Studio 18 2026" -A x64
```

## Build Outputs

After building, you'll find:

```
build/
├── bin/
│   └── Release/
│       └── TerminalDX12.exe    # Main executable
├── lib/                         # Static libraries (if any)
└── shaders/                     # Compiled shaders (when implemented)
```

## Debug Build

For debugging with Visual Studio:

```powershell
# Configure and build Debug
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake -G "Visual Studio 18 2026" -A x64
cmake --build . --config Debug

# Executable at: build\bin\Debug\TerminalDX12.exe
```

Debug builds include:
- Full debug symbols
- DirectX 12 debug layer enabled
- Additional error checking
- Slower performance

## Performance Testing

To test performance without debugging overhead, always use Release builds:

```powershell
cmake --build . --config Release
```

## Next Steps After Successful Build

Once the application builds and runs successfully:
1. Verify the window opens correctly
2. Check that it responds to resize/minimize events
3. Confirm DirectX 12 initialization (black screen = success)
4. Ready to implement text rendering and terminal emulation!

## Additional Resources

- [DirectX 12 Programming Guide](https://docs.microsoft.com/en-us/windows/win32/direct3d12/directx-12-programming-guide)
- [vcpkg Documentation](https://vcpkg.io/)
- [CMake Documentation](https://cmake.org/documentation/)

