# Build Instructions for Windows Graphics Capture Streamer

This guide provides detailed instructions for building the Windows Graphics Capture Streamer from source.

## Prerequisites

### Required Software

1. **Visual Studio 2019 or later**
   - Download from: https://visualstudio.microsoft.com/
   - Required workloads:
     - Desktop development with C++
     - Windows 10 SDK (10.0.18362.0 or later)

2. **CMake 3.20 or later**
   - Download from: https://cmake.org/download/
   - Add to PATH during installation

3. **CUDA Toolkit**
   - Download from: https://developer.nvidia.com/cuda-downloads
   - Required for NVENC support
   - Version 11.0 or later recommended

4. **NVIDIA Video Codec SDK**
   - Download from: https://developer.nvidia.com/nvidia-video-codec-sdk
   - Extract to a known location
   - Note the path for later configuration

5. **Git** (optional, for cloning)
   - Download from: https://git-scm.com/

### System Requirements

- Windows 10 version 1903 or later
- NVIDIA GPU with NVENC support
- 8GB RAM minimum
- 2GB free disk space

## Step-by-Step Build Process

### 1. Prepare the Source Code

If you haven't already, navigate to the project directory:

```cmd
cd C:\path\to\webrtc-game-stream\windows_graphics_capture
```

### 2. Install Dependencies

#### JsonCpp (for configuration parsing)

```cmd
# Using vcpkg (recommended)
vcpkg install jsoncpp:x64-windows

# Or download and build manually from:
# https://github.com/open-source-parsers/jsoncpp
```

#### WebRTC (optional, for full WebRTC support)

```cmd
# WebRTC is complex to build. For now, the project uses a simplified WebSocket implementation.
# Full WebRTC integration requires:
# - Download WebRTC from https://webrtc.googlesource.com/src
# - Follow build instructions at https://webrtc.org/native-code/development/
```

### 3. Configure Environment Variables

Set the NVENC SDK path:

```cmd
set NVENC_SDK_PATH=C:\path\to\Video_Codec_SDK_12.0
```

### 4. Generate Build Files with CMake

#### Option A: Using CMake GUI

1. Open CMake GUI
2. Set source directory to: `C:\path\to\windows_graphics_capture`
3. Set build directory to: `C:\path\to\windows_graphics_capture\build`
4. Click "Configure"
5. Select "Visual Studio 16 2019" (or your version)
6. Select "x64" platform
7. Click "Generate"

#### Option B: Using Command Line

```cmd
# Create build directory
mkdir build
cd build

# Configure
cmake .. -G "Visual Studio 16 2019" -A x64

# Or for Visual Studio 2022
cmake .. -G "Visual Studio 17 2022" -A x64
```

### 5. Build the Project

#### Option A: Using Visual Studio

1. Open `build\WindowsGraphicsCapture.sln`
2. Select "Release" configuration
3. Build → Build Solution (Ctrl+Shift+B)

#### Option B: Using Command Line

```cmd
# From the build directory
cmake --build . --config Release

# Or for Debug build
cmake --build . --config Debug
```

### 6. Verify the Build

After successful build, you should find:

```
build\bin\Release\WindowsGraphicsCapture.exe
build\bin\Release\config.json
```

## Build Configurations

### Debug Build

For development and debugging:

```cmd
cmake --build . --config Debug
```

Features:
- Debug symbols included
- Assertions enabled
- D3D debug layer enabled
- Slower performance

### Release Build

For production use:

```cmd
cmake --build . --config Release
```

Features:
- Optimized code
- No debug symbols
- Maximum performance

### RelWithDebInfo Build

For profiling and optimization:

```cmd
cmake --build . --config RelWithDebInfo
```

Features:
- Optimized code
- Debug symbols included
- Good for profiling

## Troubleshooting Build Issues

### CMake Configuration Errors

**Error: "Could not find Windows SDK"**
```cmd
# Install Windows 10 SDK via Visual Studio Installer
# Or download from: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/
```

**Error: "CUDA not found"**
```cmd
# Ensure CUDA is installed and in PATH
# Add to PATH: C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.x\bin
```

### Compilation Errors

**Error: "nvEncodeAPI.h not found"**
```cmd
# Set NVENC SDK path
set NVENC_SDK_PATH=C:\path\to\Video_Codec_SDK
# Or add to CMakeLists.txt:
# include_directories("C:/path/to/Video_Codec_SDK/Interface")
```

**Error: "winrt headers not found"**
```cmd
# Ensure Windows 10 SDK 1903+ is installed
# Update Visual Studio to latest version
```

**Error: "jsoncpp not found"**
```cmd
# Install via vcpkg
vcpkg install jsoncpp:x64-windows
# Then configure CMake with:
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### Linker Errors

**Error: "unresolved external symbol nvEncOpenEncodeSessionEx"**
```cmd
# Ensure nvEncodeAPI64.lib is linked
# Check CUDA installation
# Verify GPU supports NVENC
```

**Error: "unresolved external symbol CreateDirect3D11DeviceFromDXGIDevice"**
```cmd
# Link against windowsapp.lib
# Ensure Windows 10 SDK is properly installed
```

## Advanced Build Options

### Custom CUDA Path

```cmd
cmake .. -DCUDA_TOOLKIT_ROOT_DIR="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.8"
```

### Enable Debug Logging

```cmd
cmake .. -DENABLE_VERBOSE_LOGGING=ON
```

### Disable NVENC (use software encoder)

```cmd
cmake .. -DENABLE_NVENC=OFF
```

### Static Linking

```cmd
cmake .. -DBUILD_SHARED_LIBS=OFF
```

## Post-Build Steps

### 1. Copy Dependencies

If using dynamic libraries, copy required DLLs to the executable directory:

```cmd
# Copy CUDA DLLs
copy "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.x\bin\*.dll" build\bin\Release\

# Copy Visual C++ Runtime (if needed)
# Usually handled automatically by Visual Studio
```

### 2. Generate Configuration

```cmd
cd build\bin\Release
WindowsGraphicsCapture.exe --generate-config
```

### 3. Test the Build

```cmd
# Run with default config
WindowsGraphicsCapture.exe

# Or with custom config
WindowsGraphicsCapture.exe --config test_config.json
```

## Building for Distribution

### Create Installer (Optional)

Using NSIS or WiX Toolset:

```cmd
# Install WiX Toolset
# Create installer project
# Package executable and dependencies
```

### Create Portable Package

```cmd
# Create distribution folder
mkdir WindowsGraphicsCapture_v1.0.0
cd WindowsGraphicsCapture_v1.0.0

# Copy files
copy ..\build\bin\Release\WindowsGraphicsCapture.exe .
copy ..\build\bin\Release\config.json .
copy ..\README.md .
copy ..\LICENSE .

# Copy required DLLs
copy "C:\Windows\System32\msvcp140.dll" .
copy "C:\Windows\System32\vcruntime140.dll" .

# Create ZIP
# Use 7-Zip or Windows built-in compression
```

## Continuous Integration

### GitHub Actions Example

```yaml
name: Build Windows Graphics Capture

on: [push, pull_request]

jobs:
  build:
    runs-on: windows-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Setup MSBuild
      uses: microsoft/setup-msbuild@v1
    
    - name: Setup CMake
      uses: jwlawson/actions-setup-cmake@v1
    
    - name: Configure
      run: cmake -B build -G "Visual Studio 17 2022" -A x64
    
    - name: Build
      run: cmake --build build --config Release
    
    - name: Upload Artifact
      uses: actions/upload-artifact@v2
      with:
        name: WindowsGraphicsCapture
        path: build/bin/Release/
```

## Performance Optimization

### Compiler Flags

For maximum performance, add to CMakeLists.txt:

```cmake
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /O2          # Maximum optimization
        /Oi          # Enable intrinsics
        /GL          # Whole program optimization
        /arch:AVX2   # Use AVX2 instructions
    )
    
    target_link_options(${PROJECT_NAME} PRIVATE
        /LTCG        # Link-time code generation
    )
endif()
```

## Support

For build issues:
1. Check this document
2. Review CMake output for errors
3. Verify all prerequisites are installed
4. Check Visual Studio version compatibility
5. Ensure Windows SDK version is correct

## Additional Resources

- [CMake Documentation](https://cmake.org/documentation/)
- [Visual Studio C++ Documentation](https://docs.microsoft.com/en-us/cpp/)
- [CUDA Toolkit Documentation](https://docs.nvidia.com/cuda/)
- [Windows SDK Documentation](https://docs.microsoft.com/en-us/windows/win32/)
