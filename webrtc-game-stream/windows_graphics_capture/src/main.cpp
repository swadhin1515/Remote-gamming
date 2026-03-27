#include "common.h"
#include "config_manager.h"
#include "d3d_device.h"
#include "wgc_capture.h"
#include "nvenc_encoder.h"
#include "webrtc_client.h"
#include "frame_processor.h"

#include <iostream>
#include <csignal>
#include <atomic>

using namespace WGC;

// Global flag for graceful shutdown
std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    LOG_INFO("Received signal " + std::to_string(signal) + ", shutting down...");
    g_running = false;
}

void printUsage(const char* programName) {
    std::cout << "Windows Graphics Capture Streamer\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -c, --config <file>    Configuration file path (default: config.json)\n";
    std::cout << "  -g, --generate-config  Generate example configuration file\n";
    std::cout << "  -h, --help            Show this help message\n";
    std::cout << "  -v, --version         Show version information\n";
    std::cout << "\n";
    std::cout << "Example:\n";
    std::cout << "  " << programName << " --config my_config.json\n";
    std::cout << "  " << programName << " --generate-config\n";
}

void printVersion() {
    std::cout << "Windows Graphics Capture Streamer v1.0.0\n";
    std::cout << "Built for Windows 10 1903+ / Windows 11\n";
    std::cout << "Supports: H.264, HEVC, AV1 (with NVENC)\n";
}

void printStatistics(
    const Statistics& captureStats,
    const Statistics& encoderStats,
    const Statistics& webrtcStats
) {
    std::cout << "\n=== Statistics ===\n";
    std::cout << "Capture:\n";
    std::cout << "  Frames captured: " << captureStats.framesCaptured << "\n";
    std::cout << "  Frames dropped: " << captureStats.framesDropped << "\n";
    std::cout << "  Current FPS: " << captureStats.currentFPS << "\n";
    std::cout << "  Avg capture latency: " << captureStats.averageCaptureLatency << " ms\n";
    
    std::cout << "\nEncoder:\n";
    std::cout << "  Frames encoded: " << encoderStats.framesEncoded << "\n";
    std::cout << "  Total bytes: " << encoderStats.totalBytesEncoded << "\n";
    std::cout << "  Avg encode latency: " << encoderStats.averageEncodeLatency << " ms\n";
    
    std::cout << "\nWebRTC:\n";
    std::cout << "  Frames sent: " << webrtcStats.framesSent << "\n";
    std::cout << "  Avg network latency: " << webrtcStats.averageNetworkLatency << " ms\n";
    std::cout << "==================\n\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string configFile = "config.json";
    bool generateConfig = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version") {
            printVersion();
            return 0;
        }
        else if (arg == "-g" || arg == "--generate-config") {
            generateConfig = true;
        }
        else if (arg == "-c" || arg == "--config") {
            if (i + 1 < argc) {
                configFile = argv[++i];
            } else {
                std::cerr << "Error: --config requires a file path\n";
                return 1;
            }
        }
    }
    
    // Generate example config if requested
    if (generateConfig) {
        std::cout << "Generating example configuration file: config_example.json\n";
        auto result = ConfigManager::generateExampleConfig("config_example.json");
        if (result == ErrorCode::Success) {
            std::cout << "Example configuration generated successfully!\n";
            std::cout << "Edit the file and rename it to config.json to use it.\n";
            return 0;
        } else {
            std::cerr << "Failed to generate example configuration: " 
                      << errorCodeToString(result) << "\n";
            return 1;
        }
    }
    
    // Load configuration
    Config config;
    auto result = ConfigManager::loadConfig(configFile, config);
    if (result != ErrorCode::Success) {
        std::cerr << "Failed to load configuration from " << configFile << ": "
                  << errorCodeToString(result) << "\n";
        std::cerr << "Use --generate-config to create an example configuration file.\n";
        return 1;
    }
    
    // Initialize logger
    Logger::getInstance().init(config.logFilePath, config.enableLogging);
    LOG_INFO("Windows Graphics Capture Streamer starting...");
    LOG_INFO("Configuration loaded from: " + configFile);
    
    // Setup signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // Initialize COM for WinRT
        winrt::init_apartment();
        LOG_INFO("WinRT initialized");
        
        // Create D3D device
        LOG_INFO("Creating Direct3D device...");
        auto d3dDevice = std::make_shared<D3DDevice>();
        result = d3dDevice->initialize(false);
        if (result != ErrorCode::Success) {
            LOG_ERROR("Failed to initialize D3D device: " + errorCodeToString(result));
            return 1;
        }
        LOG_INFO("Direct3D device created successfully");
        
        // Check for NVIDIA GPU
        if (d3dDevice->isNvidiaGPU()) {
            LOG_INFO("NVIDIA GPU detected");
        } else {
            LOG_WARNING("NVIDIA GPU not detected - NVENC may not be available");
        }
        
        // Create WGC capture
        LOG_INFO("Initializing Windows Graphics Capture...");
        auto capture = std::make_shared<WGCCapture>(d3dDevice);
        result = capture->initialize(config);
        if (result != ErrorCode::Success) {
            LOG_ERROR("Failed to initialize WGC: " + errorCodeToString(result));
            return 1;
        }
        LOG_INFO("Windows Graphics Capture initialized");
        
        // Create NVENC encoder
        LOG_INFO("Initializing NVENC encoder...");
        auto encoder = std::make_shared<NVENCEncoder>(d3dDevice);
        result = encoder->initialize(config);
        if (result != ErrorCode::Success) {
            LOG_ERROR("Failed to initialize NVENC: " + errorCodeToString(result));
            return 1;
        }
        LOG_INFO("NVENC encoder initialized");
        
        // Initialize streaming based on mode
        std::shared_ptr<WebRTCClient> webrtcClient;
        std::shared_ptr<RTPSender> rtpSender;
        
        if (config.streamingMode == StreamingMode::RTP) {
            // RTP mode - send to intermediate server
            LOG_INFO("Initializing RTP sender...");
            rtpSender = std::make_shared<RTPSender>();
            
            uint8_t payloadType = (config.codec == CodecType::H264) ? 96 : 97;
            result = rtpSender->initialize(config.rtpTargetHost, config.rtpTargetPort, payloadType);
            if (result != ErrorCode::Success) {
                LOG_ERROR("Failed to initialize RTP sender: " + errorCodeToString(result));
                return 1;
            }
            LOG_INFO("RTP sender initialized");
            LOG_INFO("Streaming to: " + config.rtpTargetHost + ":" + std::to_string(config.rtpTargetPort));
        } else {
            // WebRTC mode - direct peer-to-peer
            LOG_INFO("Initializing WebRTC client...");
            webrtcClient = std::make_shared<WebRTCClient>();
            result = webrtcClient->initialize(config);
            if (result != ErrorCode::Success) {
                LOG_ERROR("Failed to initialize WebRTC: " + errorCodeToString(result));
                return 1;
            }
            LOG_INFO("WebRTC client initialized");
            
            // Connect to signaling server
            LOG_INFO("Connecting to signaling server: " + config.signalingServerUrl);
            result = webrtcClient->connect();
            if (result != ErrorCode::Success) {
                LOG_ERROR("Failed to connect to signaling server: " + errorCodeToString(result));
                return 1;
            }
            LOG_INFO("Connected to signaling server");
            
            // Setup connection state callback
            webrtcClient->setConnectionStateCallback([](const std::string& state) {
                LOG_INFO("WebRTC connection state: " + state);
            });
            
            // Setup data channel callback for control messages
            webrtcClient->setDataChannelCallback([](const std::string& message) {
                LOG_INFO("Received control message: " + message);
                // Handle control messages (e.g., bitrate adjustment, resolution change)
            });
        }
        
        // Start capture with frame callback
        LOG_INFO("Starting capture...");
        result = capture->startCapture([&](const Frame& frame) {
            // Encode frame
            encoder->encodeFrame(frame, [&](const EncodedFrame& encodedFrame) {
                // Send encoded frame based on streaming mode
                if (rtpSender) {
                    rtpSender->sendFrame(encodedFrame);
                } else if (webrtcClient) {
                    webrtcClient->sendFrame(encodedFrame);
                }
            });
        });
        
        if (result != ErrorCode::Success) {
            LOG_ERROR("Failed to start capture: " + errorCodeToString(result));
            return 1;
        }
        
        LOG_INFO("Capture started successfully!");
        if (config.streamingMode == StreamingMode::RTP) {
            LOG_INFO("Streaming mode: RTP");
            LOG_INFO("Target: " + config.rtpTargetHost + ":" + std::to_string(config.rtpTargetPort));
        } else {
            LOG_INFO("Streaming mode: WebRTC");
            LOG_INFO("Signaling server: " + config.signalingServerUrl);
        }
        LOG_INFO("Press Ctrl+C to stop...");
        
        // Main loop - print statistics periodically
        auto lastStatsTime = std::chrono::steady_clock::now();
        const auto statsInterval = std::chrono::seconds(5);
        
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            auto now = std::chrono::steady_clock::now();
            if (now - lastStatsTime >= statsInterval) {
                printStatistics(
                    capture->getStatistics(),
                    encoder->getStatistics(),
                    webrtcClient->getStatistics()
                );
                lastStatsTime = now;
            }
            
            // Check if still connected (WebRTC mode only)
            if (webrtcClient && !webrtcClient->isConnected()) {
                LOG_WARNING("WebRTC connection lost, attempting to reconnect...");
                webrtcClient->connect();
            }
        }
        
        // Cleanup
        LOG_INFO("Stopping capture...");
        capture->stopCapture();
        
        LOG_INFO("Flushing encoder...");
        encoder->flush([&](const EncodedFrame& encodedFrame) {
            webrtcClient->sendFrame(encodedFrame);
        });
        
        if (webrtcClient) {
            LOG_INFO("Disconnecting WebRTC...");
            webrtcClient->disconnect();
        }
        
        if (rtpSender) {
            LOG_INFO("Closing RTP sender...");
            rtpSender->close();
        }
        
        // Print final statistics
        std::cout << "\n=== Final Statistics ===\n";
        Statistics streamStats;
        if (webrtcClient) {
            streamStats = webrtcClient->getStatistics();
        } else if (rtpSender) {
            streamStats = rtpSender->getStatistics();
        }
        printStatistics(
            capture->getStatistics(),
            encoder->getStatistics(),
            streamStats
        );
        
        LOG_INFO("Shutdown complete");
        winrt::uninit_apartment();
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Exception: ") + e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown exception occurred");
        return 1;
    }
    
    return 0;
}
