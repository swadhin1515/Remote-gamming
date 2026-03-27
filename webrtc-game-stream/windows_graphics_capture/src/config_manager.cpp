#include "config_manager.h"
#include <fstream>

namespace WGC {

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
}

ErrorCode ConfigManager::loadConfig(const std::string& filePath, Config& config) {
    LOG_INFO("Loading configuration from: " + filePath);
    
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open configuration file: " + filePath);
        return ErrorCode::InvalidConfiguration;
    }
    
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    if (!Json::parseFromStream(builder, file, &root, &errors)) {
        LOG_ERROR("Failed to parse configuration JSON: " + errors);
        return ErrorCode::InvalidConfiguration;
    }
    
    ErrorCode result = parseJson(root, config);
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to parse configuration");
        return result;
    }
    
    // Validate configuration
    result = validateConfig(config);
    if (result != ErrorCode::Success) {
        LOG_ERROR("Configuration validation failed");
        return result;
    }
    
    LOG_INFO("Configuration loaded successfully");
    
    return ErrorCode::Success;
}

ErrorCode ConfigManager::saveConfig(const std::string& filePath, const Config& config) {
    LOG_INFO("Saving configuration to: " + filePath);
    
    Json::Value root = configToJson(config);
    
    std::ofstream file(filePath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open file for writing: " + filePath);
        return ErrorCode::InvalidConfiguration;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "  ";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(root, &file);
    
    LOG_INFO("Configuration saved successfully");
    
    return ErrorCode::Success;
}

Config ConfigManager::createDefaultConfig() {
    Config config;
    
    // Capture settings
    config.captureMode = CaptureMode::Primary;
    config.targetWidth = DEFAULT_WIDTH;
    config.targetHeight = DEFAULT_HEIGHT;
    config.targetFPS = DEFAULT_FPS;
    config.enableCursor = true;
    config.enableBorderless = true;
    
    // Encoder settings
    config.codec = CodecType::HEVC;
    config.preset = EncoderPreset::LowLatency;
    config.bitrate = DEFAULT_BITRATE;
    config.gopSize = 60;
    config.enableBFrames = false;
    
    // WebRTC settings
    config.signalingServerUrl = "ws://localhost:9000";
    config.stunServer = "stun:stun.l.google.com:19302";
    
    // Performance settings
    config.enableHardwareAcceleration = true;
    config.enableZeroCopy = true;
    config.maxLatencyMs = 50;
    
    // Logging
    config.enableLogging = true;
    config.logFilePath = "wgc_capture.log";
    
    return config;
}

ErrorCode ConfigManager::validateConfig(const Config& config) {
    // Validate resolution
    if (config.targetWidth <= 0 || config.targetWidth > 7680) {
        LOG_ERROR("Invalid target width: " + std::to_string(config.targetWidth));
        return ErrorCode::InvalidConfiguration;
    }
    
    if (config.targetHeight <= 0 || config.targetHeight > 4320) {
        LOG_ERROR("Invalid target height: " + std::to_string(config.targetHeight));
        return ErrorCode::InvalidConfiguration;
    }
    
    // Validate FPS
    if (config.targetFPS <= 0 || config.targetFPS > 240) {
        LOG_ERROR("Invalid target FPS: " + std::to_string(config.targetFPS));
        return ErrorCode::InvalidConfiguration;
    }
    
    // Validate bitrate
    if (config.bitrate <= 0 || config.bitrate > 100000000) {
        LOG_ERROR("Invalid bitrate: " + std::to_string(config.bitrate));
        return ErrorCode::InvalidConfiguration;
    }
    
    // Validate signaling server URL
    if (config.signalingServerUrl.empty()) {
        LOG_ERROR("Signaling server URL is empty");
        return ErrorCode::InvalidConfiguration;
    }
    
    return ErrorCode::Success;
}

ErrorCode ConfigManager::generateExampleConfig(const std::string& filePath) {
    Config config = createDefaultConfig();
    return saveConfig(filePath, config);
}

ErrorCode ConfigManager::parseJson(const Json::Value& json, Config& config) {
    // Parse capture settings
    if (json.isMember("capture")) {
        const Json::Value& capture = json["capture"];
        
        if (capture.isMember("mode")) {
            config.captureMode = parseCaptureModeString(capture["mode"].asString());
        }
        if (capture.isMember("width")) {
            config.targetWidth = capture["width"].asInt();
        }
        if (capture.isMember("height")) {
            config.targetHeight = capture["height"].asInt();
        }
        if (capture.isMember("fps")) {
            config.targetFPS = capture["fps"].asInt();
        }
        if (capture.isMember("enableCursor")) {
            config.enableCursor = capture["enableCursor"].asBool();
        }
        if (capture.isMember("enableBorderless")) {
            config.enableBorderless = capture["enableBorderless"].asBool();
        }
    }
    
    // Parse encoder settings
    if (json.isMember("encoder")) {
        const Json::Value& encoder = json["encoder"];
        
        if (encoder.isMember("codec")) {
            config.codec = parseCodecString(encoder["codec"].asString());
        }
        if (encoder.isMember("preset")) {
            config.preset = parsePresetString(encoder["preset"].asString());
        }
        if (encoder.isMember("bitrate")) {
            config.bitrate = encoder["bitrate"].asInt();
        }
        if (encoder.isMember("gopSize")) {
            config.gopSize = encoder["gopSize"].asInt();
        }
        if (encoder.isMember("enableBFrames")) {
            config.enableBFrames = encoder["enableBFrames"].asBool();
        }
    }
    
    // Parse streaming settings
    if (json.isMember("streaming")) {
        const Json::Value& streaming = json["streaming"];
        
        if (streaming.isMember("mode")) {
            std::string mode = streaming["mode"].asString();
            config.streamingMode = (mode == "webrtc") ? StreamingMode::WebRTC : StreamingMode::RTP;
        }
        if (streaming.isMember("rtpTargetHost")) {
            config.rtpTargetHost = streaming["rtpTargetHost"].asString();
        }
        if (streaming.isMember("rtpTargetPort")) {
            config.rtpTargetPort = streaming["rtpTargetPort"].asInt();
        }
    }
    
    // Parse WebRTC settings
    if (json.isMember("webrtc")) {
        const Json::Value& webrtc = json["webrtc"];
        
        if (webrtc.isMember("signalingServerUrl")) {
            config.signalingServerUrl = webrtc["signalingServerUrl"].asString();
        }
        if (webrtc.isMember("stunServer")) {
            config.stunServer = webrtc["stunServer"].asString();
        }
        if (webrtc.isMember("turnServers")) {
            const Json::Value& turnServers = webrtc["turnServers"];
            for (const auto& server : turnServers) {
                config.turnServers.push_back(server.asString());
            }
        }
    }
    
    // Parse performance settings
    if (json.isMember("performance")) {
        const Json::Value& perf = json["performance"];
        
        if (perf.isMember("enableHardwareAcceleration")) {
            config.enableHardwareAcceleration = perf["enableHardwareAcceleration"].asBool();
        }
        if (perf.isMember("enableZeroCopy")) {
            config.enableZeroCopy = perf["enableZeroCopy"].asBool();
        }
        if (perf.isMember("maxLatencyMs")) {
            config.maxLatencyMs = perf["maxLatencyMs"].asInt();
        }
    }
    
    // Parse logging settings
    if (json.isMember("logging")) {
        const Json::Value& logging = json["logging"];
        
        if (logging.isMember("enabled")) {
            config.enableLogging = logging["enabled"].asBool();
        }
        if (logging.isMember("logFilePath")) {
            config.logFilePath = logging["logFilePath"].asString();
        }
    }
    
    return ErrorCode::Success;
}

Json::Value ConfigManager::configToJson(const Config& config) {
    Json::Value root;
    
    // Capture settings
    Json::Value capture;
    capture["mode"] = captureModeToString(config.captureMode);
    capture["width"] = config.targetWidth;
    capture["height"] = config.targetHeight;
    capture["fps"] = config.targetFPS;
    capture["enableCursor"] = config.enableCursor;
    capture["enableBorderless"] = config.enableBorderless;
    root["capture"] = capture;
    
    // Encoder settings
    Json::Value encoder;
    encoder["codec"] = codecToString(config.codec);
    encoder["preset"] = presetToString(config.preset);
    encoder["bitrate"] = config.bitrate;
    encoder["gopSize"] = config.gopSize;
    encoder["enableBFrames"] = config.enableBFrames;
    root["encoder"] = encoder;
    
    // Streaming settings
    Json::Value streaming;
    streaming["mode"] = (config.streamingMode == StreamingMode::WebRTC) ? "webrtc" : "rtp";
    streaming["rtpTargetHost"] = config.rtpTargetHost;
    streaming["rtpTargetPort"] = config.rtpTargetPort;
    root["streaming"] = streaming;
    
    // WebRTC settings
    Json::Value webrtc;
    webrtc["signalingServerUrl"] = config.signalingServerUrl;
    webrtc["stunServer"] = config.stunServer;
    Json::Value turnServers(Json::arrayValue);
    for (const auto& server : config.turnServers) {
        turnServers.append(server);
    }
    webrtc["turnServers"] = turnServers;
    root["webrtc"] = webrtc;
    
    // Performance settings
    Json::Value performance;
    performance["enableHardwareAcceleration"] = config.enableHardwareAcceleration;
    performance["enableZeroCopy"] = config.enableZeroCopy;
    performance["maxLatencyMs"] = config.maxLatencyMs;
    root["performance"] = performance;
    
    // Logging settings
    Json::Value logging;
    logging["enabled"] = config.enableLogging;
    logging["logFilePath"] = config.logFilePath;
    root["logging"] = logging;
    
    return root;
}

CaptureMode ConfigManager::parseCaptureModeString(const std::string& mode) {
    if (mode == "window") return CaptureMode::Window;
    if (mode == "monitor") return CaptureMode::Monitor;
    if (mode == "primary") return CaptureMode::Primary;
    return CaptureMode::Primary;
}

CodecType ConfigManager::parseCodecString(const std::string& codec) {
    if (codec == "h264") return CodecType::H264;
    if (codec == "hevc") return CodecType::HEVC;
    if (codec == "av1") return CodecType::AV1;
    return CodecType::HEVC;
}

EncoderPreset ConfigManager::parsePresetString(const std::string& preset) {
    if (preset == "ultra_low_latency") return EncoderPreset::UltraLowLatency;
    if (preset == "low_latency") return EncoderPreset::LowLatency;
    if (preset == "quality") return EncoderPreset::Quality;
    if (preset == "high_quality") return EncoderPreset::HighQuality;
    return EncoderPreset::LowLatency;
}

std::string ConfigManager::captureModeToString(CaptureMode mode) {
    switch (mode) {
        case CaptureMode::Window: return "window";
        case CaptureMode::Monitor: return "monitor";
        case CaptureMode::Primary: return "primary";
        default: return "primary";
    }
}

std::string ConfigManager::codecToString(CodecType codec) {
    switch (codec) {
        case CodecType::H264: return "h264";
        case CodecType::HEVC: return "hevc";
        case CodecType::AV1: return "av1";
        default: return "hevc";
    }
}

std::string ConfigManager::presetToString(EncoderPreset preset) {
    switch (preset) {
        case EncoderPreset::UltraLowLatency: return "ultra_low_latency";
        case EncoderPreset::LowLatency: return "low_latency";
        case EncoderPreset::Quality: return "quality";
        case EncoderPreset::HighQuality: return "high_quality";
        default: return "low_latency";
    }
}

} // namespace WGC
