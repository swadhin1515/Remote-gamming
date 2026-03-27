#pragma once

#include "common.h"
#include <json/json.h>

namespace WGC {

/**
 * @brief Configuration file manager
 * 
 * This class handles:
 * - Loading configuration from JSON file
 * - Saving configuration to JSON file
 * - Configuration validation
 * - Default configuration generation
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();

    /**
     * @brief Load configuration from file
     * @param filePath Path to configuration file
     * @param config Output configuration structure
     * @return ErrorCode indicating success or failure
     */
    static ErrorCode loadConfig(const std::string& filePath, Config& config);

    /**
     * @brief Save configuration to file
     * @param filePath Path to configuration file
     * @param config Configuration to save
     * @return ErrorCode indicating success or failure
     */
    static ErrorCode saveConfig(const std::string& filePath, const Config& config);

    /**
     * @brief Create default configuration
     * @return Default Config structure
     */
    static Config createDefaultConfig();

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return ErrorCode indicating if config is valid
     */
    static ErrorCode validateConfig(const Config& config);

    /**
     * @brief Generate example configuration file
     * @param filePath Path where to save example config
     * @return ErrorCode indicating success or failure
     */
    static ErrorCode generateExampleConfig(const std::string& filePath);

private:
    /**
     * @brief Parse JSON to Config structure
     * @param json JSON value
     * @param config Output configuration
     * @return ErrorCode indicating success or failure
     */
    static ErrorCode parseJson(const Json::Value& json, Config& config);

    /**
     * @brief Convert Config to JSON
     * @param config Configuration
     * @return JSON value
     */
    static Json::Value configToJson(const Config& config);

    /**
     * @brief Parse capture mode from string
     * @param mode Mode string
     * @return CaptureMode enum
     */
    static CaptureMode parseCaptureModeString(const std::string& mode);

    /**
     * @brief Parse codec type from string
     * @param codec Codec string
     * @return CodecType enum
     */
    static CodecType parseCodecString(const std::string& codec);

    /**
     * @brief Parse encoder preset from string
     * @param preset Preset string
     * @return EncoderPreset enum
     */
    static EncoderPreset parsePresetString(const std::string& preset);

    /**
     * @brief Convert capture mode to string
     * @param mode CaptureMode enum
     * @return Mode string
     */
    static std::string captureModeToString(CaptureMode mode);

    /**
     * @brief Convert codec type to string
     * @param codec CodecType enum
     * @return Codec string
     */
    static std::string codecToString(CodecType codec);

    /**
     * @brief Convert encoder preset to string
     * @param preset EncoderPreset enum
     * @return Preset string
     */
    static std::string presetToString(EncoderPreset preset);
};

} // namespace WGC
