#pragma once

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <fstream>
#include <sstream>

// Namespace aliases
namespace winrt {
    using namespace Windows::Foundation;
    using namespace Windows::Graphics::Capture;
    using namespace Windows::Graphics::DirectX;
    using namespace Windows::Graphics::DirectX::Direct3D11;
}

using Microsoft::WRL::ComPtr;

// Common constants
namespace WGC {
    constexpr int DEFAULT_WIDTH = 1920;
    constexpr int DEFAULT_HEIGHT = 1080;
    constexpr int DEFAULT_FPS = 60;
    constexpr int DEFAULT_BITRATE = 5000000; // 5 Mbps
    constexpr int FRAME_POOL_SIZE = 3;
    
    // Encoder presets
    enum class EncoderPreset {
        UltraLowLatency,
        LowLatency,
        Quality,
        HighQuality
    };
    
    // Codec types
    enum class CodecType {
        H264,
        HEVC,
        AV1
    };
    
    // Capture mode
    enum class CaptureMode {
        Window,
        Monitor,
        Primary
    };
    
    // Frame format
    enum class FrameFormat {
        BGRA8,
        RGBA8,
        NV12,
        YUV420
    };
    
    // Error codes
    enum class ErrorCode {
        Success = 0,
        InitializationFailed,
        CaptureStartFailed,
        EncoderInitFailed,
        WebRTCConnectionFailed,
        DeviceCreationFailed,
        FramePoolCreationFailed,
        InvalidConfiguration,
        PermissionDenied,
        UnsupportedFormat,
        NetworkError
    };
    
    // Streaming mode
    enum class StreamingMode {
        WebRTC,  // Direct WebRTC (requires libwebrtc)
        RTP      // RTP to intermediate server
    };
    
    // Configuration structure
    struct Config {
        // Capture settings
        CaptureMode captureMode = CaptureMode::Primary;
        int targetWidth = DEFAULT_WIDTH;
        int targetHeight = DEFAULT_HEIGHT;
        int targetFPS = DEFAULT_FPS;
        bool enableCursor = true;
        bool enableBorderless = true;
        
        // Encoder settings
        CodecType codec = CodecType::HEVC;
        EncoderPreset preset = EncoderPreset::LowLatency;
        int bitrate = DEFAULT_BITRATE;
        int gopSize = 60;
        bool enableBFrames = false;
        
        // Streaming settings
        StreamingMode streamingMode = StreamingMode::RTP;
        std::string rtpTargetHost = "localhost";
        int rtpTargetPort = 5000;
        
        // WebRTC settings (for direct WebRTC mode)
        std::string signalingServerUrl = "ws://localhost:9000";
        std::string stunServer = "stun:stun.l.google.com:19302";
        std::vector<std::string> turnServers;
        
        // Performance settings
        bool enableHardwareAcceleration = true;
        bool enableZeroCopy = true;
        int maxLatencyMs = 50;
        
        // Logging
        bool enableLogging = true;
        std::string logFilePath = "wgc_capture.log";
    };
    
    // Frame data structure
    struct Frame {
        ComPtr<ID3D11Texture2D> texture;
        uint64_t timestamp;
        int width;
        int height;
        FrameFormat format;
        bool isKeyFrame;
        
        Frame() : timestamp(0), width(0), height(0), 
                  format(FrameFormat::BGRA8), isKeyFrame(false) {}
    };
    
    // Encoded frame structure
    struct EncodedFrame {
        std::vector<uint8_t> data;
        uint64_t timestamp;
        bool isKeyFrame;
        int width;
        int height;
        
        EncodedFrame() : timestamp(0), isKeyFrame(false), 
                         width(0), height(0) {}
    };
    
    // Statistics structure
    struct Statistics {
        std::atomic<uint64_t> framesCaptured{0};
        std::atomic<uint64_t> framesEncoded{0};
        std::atomic<uint64_t> framesSent{0};
        std::atomic<uint64_t> framesDropped{0};
        std::atomic<double> averageCaptureLatency{0.0};
        std::atomic<double> averageEncodeLatency{0.0};
        std::atomic<double> averageNetworkLatency{0.0};
        std::atomic<double> currentFPS{0.0};
        std::atomic<uint64_t> totalBytesEncoded{0};
        
        void reset() {
            framesCaptured = 0;
            framesEncoded = 0;
            framesSent = 0;
            framesDropped = 0;
            averageCaptureLatency = 0.0;
            averageEncodeLatency = 0.0;
            averageNetworkLatency = 0.0;
            currentFPS = 0.0;
            totalBytesEncoded = 0;
        }
    };
    
    // Logger utility
    class Logger {
    public:
        enum class Level {
            Debug,
            Info,
            Warning,
            Error
        };
        
        static Logger& getInstance() {
            static Logger instance;
            return instance;
        }
        
        void init(const std::string& logFile, bool enabled) {
            std::lock_guard<std::mutex> lock(mutex_);
            enabled_ = enabled;
            if (enabled && !logFile.empty()) {
                logStream_.open(logFile, std::ios::app);
            }
        }
        
        void log(Level level, const std::string& message) {
            if (!enabled_) return;
            
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            
            std::stringstream ss;
            ss << "[" << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S") << "] ";
            
            switch (level) {
                case Level::Debug:   ss << "[DEBUG] "; break;
                case Level::Info:    ss << "[INFO]  "; break;
                case Level::Warning: ss << "[WARN]  "; break;
                case Level::Error:   ss << "[ERROR] "; break;
            }
            
            ss << message << std::endl;
            
            std::string logLine = ss.str();
            std::cout << logLine;
            
            if (logStream_.is_open()) {
                logStream_ << logLine;
                logStream_.flush();
            }
        }
        
        ~Logger() {
            if (logStream_.is_open()) {
                logStream_.close();
            }
        }
        
    private:
        Logger() : enabled_(false) {}
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;
        
        bool enabled_;
        std::ofstream logStream_;
        std::mutex mutex_;
    };
    
    // Helper macros
    #define LOG_DEBUG(msg) WGC::Logger::getInstance().log(WGC::Logger::Level::Debug, msg)
    #define LOG_INFO(msg) WGC::Logger::getInstance().log(WGC::Logger::Level::Info, msg)
    #define LOG_WARNING(msg) WGC::Logger::getInstance().log(WGC::Logger::Level::Warning, msg)
    #define LOG_ERROR(msg) WGC::Logger::getInstance().log(WGC::Logger::Level::Error, msg)
    
    // Error handling
    inline std::string errorCodeToString(ErrorCode code) {
        switch (code) {
            case ErrorCode::Success: return "Success";
            case ErrorCode::InitializationFailed: return "Initialization failed";
            case ErrorCode::CaptureStartFailed: return "Capture start failed";
            case ErrorCode::EncoderInitFailed: return "Encoder initialization failed";
            case ErrorCode::WebRTCConnectionFailed: return "WebRTC connection failed";
            case ErrorCode::DeviceCreationFailed: return "Device creation failed";
            case ErrorCode::FramePoolCreationFailed: return "Frame pool creation failed";
            case ErrorCode::InvalidConfiguration: return "Invalid configuration";
            case ErrorCode::PermissionDenied: return "Permission denied";
            case ErrorCode::UnsupportedFormat: return "Unsupported format";
            case ErrorCode::NetworkError: return "Network error";
            default: return "Unknown error";
        }
    }
    
    // HRESULT checker
    inline bool checkHR(HRESULT hr, const std::string& context) {
        if (FAILED(hr)) {
            std::stringstream ss;
            ss << context << " failed with HRESULT: 0x" << std::hex << hr;
            LOG_ERROR(ss.str());
            return false;
        }
        return true;
    }
    
    // Timing utilities
    inline uint64_t getCurrentTimestampUs() {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
    }
    
    inline double calculateFPS(uint64_t frameCount, uint64_t startTime) {
        uint64_t currentTime = getCurrentTimestampUs();
        double elapsedSeconds = (currentTime - startTime) / 1000000.0;
        return elapsedSeconds > 0 ? frameCount / elapsedSeconds : 0.0;
    }
}
