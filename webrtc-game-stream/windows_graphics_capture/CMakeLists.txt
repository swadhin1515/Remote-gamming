cmake_minimum_required(VERSION 3.20)
project(WindowsGraphicsCapture VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows-specific settings
if(NOT WIN32)
    message(FATAL_ERROR "This project requires Windows 10 version 1903 or later")
endif()

# Require Windows 10 SDK version 1903 or later
set(CMAKE_SYSTEM_VERSION 10.0.18362.0)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(CUDA REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CUDA_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/wgc_capture.cpp
    src/d3d_device.cpp
    src/nvenc_encoder.cpp
    src/webrtc_client.cpp
    src/frame_processor.cpp
    src/config_manager.cpp
    src/rtp_sender.cpp
)

# Header files
set(HEADERS
    include/wgc_capture.h
    include/d3d_device.h
    include/nvenc_encoder.h
    include/webrtc_client.h
    include/frame_processor.h
    include/config_manager.h
    include/rtp_sender.h
    include/common.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    # Windows Runtime
    windowsapp
    
    # DirectX
    d3d11
    dxgi
    d3dcompiler
    
    # CUDA/NVENC
    ${CUDA_LIBRARIES}
    ${CUDA_nvcuvid_LIBRARY}
    
    # Windows Media Foundation (fallback encoder)
    mfplat
    mfuuid
    mfreadwrite
    
    # WebRTC (you'll need to build or download WebRTC libraries)
    # webrtc
    
    # WebSocket
    ws2_32
    
    # Other Windows APIs
    ole32
    oleaut32
    uuid
)

# Compiler definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _WIN32_WINNT=0x0A00  # Windows 10
)

# Compiler options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W4
        /WX-  # Don't treat warnings as errors for now
        /permissive-
        /Zc:__cplusplus
        /await
        /EHsc
    )
endif()

# Copy config files to build directory
configure_file(
    ${CMAKE_SOURCE_DIR}/config/config.json
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/config.json
    COPYONLY
)

# Installation
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES config/config.json
    DESTINATION bin
)

# Print configuration
message(STATUS "")
message(STATUS "Windows Graphics Capture Configuration:")
message(STATUS "  CMake version: ${CMAKE_VERSION}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CUDA version: ${CUDA_VERSION}")
message(STATUS "")
