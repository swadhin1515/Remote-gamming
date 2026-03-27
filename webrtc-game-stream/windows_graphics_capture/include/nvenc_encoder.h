#pragma once

#include "common.h"
#include "d3d_device.h"
#include <nvEncodeAPI.h>

namespace WGC {

/**
 * @brief NVIDIA NVENC hardware encoder wrapper
 * 
 * This class handles:
 * - NVENC initialization and configuration
 * - Zero-copy encoding from D3D11 textures
 * - H.264, HEVC, and AV1 codec support
 * - Low-latency presets for streaming
 * - Bitrate control and GOP management
 * - Output buffer management
 */
class NVENCEncoder {
public:
    using EncodedFrameCallback = std::function<void(const EncodedFrame&)>;

    NVENCEncoder(std::shared_ptr<D3DDevice> d3dDevice);
    ~NVENCEncoder();

    /**
     * @brief Initialize NVENC encoder
     * @param config Encoder configuration
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(const Config& config);

    /**
     * @brief Encode a frame from D3D11 texture
     * @param frame Frame containing D3D11 texture
     * @param callback Function to call with encoded data
     * @return ErrorCode indicating success or failure
     */
    ErrorCode encodeFrame(const Frame& frame, EncodedFrameCallback callback);

    /**
     * @brief Flush encoder and get remaining frames
     * @param callback Function to call with encoded data
     * @return ErrorCode indicating success or failure
     */
    ErrorCode flush(EncodedFrameCallback callback);

    /**
     * @brief Reconfigure encoder (e.g., bitrate change)
     * @param config New configuration
     * @return ErrorCode indicating success or failure
     */
    ErrorCode reconfigure(const Config& config);

    /**
     * @brief Force next frame to be a keyframe (IDR)
     */
    void forceIDR();

    /**
     * @brief Check if encoder is initialized
     * @return true if ready to encode
     */
    bool isInitialized() const { return initialized_; }

    /**
     * @brief Get encoder statistics
     * @return Statistics structure
     */
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics() { stats_.reset(); }

    /**
     * @brief Get supported codecs on this system
     * @return Vector of supported codec types
     */
    static std::vector<CodecType> getSupportedCodecs();

    /**
     * @brief Check if NVENC is available
     * @return true if NVENC can be used
     */
    static bool isNVENCAvailable();

    /**
     * @brief Get NVENC driver version
     * @return Version string
     */
    static std::string getNVENCVersion();

private:
    std::shared_ptr<D3DDevice> d3dDevice_;
    
    // NVENC handles
    void* nvencEncoder_{nullptr};
    NV_ENCODE_API_FUNCTION_LIST nvencAPI_{};
    
    // Configuration
    Config config_;
    NV_ENC_INITIALIZE_PARAMS initParams_{};
    NV_ENC_CONFIG encodeConfig_{};
    
    // Input/output resources
    struct InputResource {
        NV_ENC_REGISTERED_PTR registeredResource;
        NV_ENC_INPUT_PTR mappedResource;
        ComPtr<ID3D11Texture2D> texture;
        bool inUse;
    };
    
    struct OutputBuffer {
        NV_ENC_OUTPUT_PTR bitstreamBuffer;
        bool inUse;
    };
    
    std::vector<InputResource> inputResources_;
    std::vector<OutputBuffer> outputBuffers_;
    
    // State
    std::atomic<bool> initialized_{false};
    std::atomic<bool> forceIDR_{false};
    int frameCount_{0};
    
    // Statistics
    Statistics stats_;
    uint64_t encodeStartTime_{0};
    
    // Synchronization
    mutable std::mutex mutex_;
    
    /**
     * @brief Load NVENC API function pointers
     * @return ErrorCode indicating success or failure
     */
    ErrorCode loadNVENCAPI();

    /**
     * @brief Open NVENC encoder session
     * @return ErrorCode indicating success or failure
     */
    ErrorCode openEncoder();

    /**
     * @brief Get encoder capabilities
     * @param codec Codec to query
     * @return true if codec is supported
     */
    bool getEncoderCaps(GUID codec);

    /**
     * @brief Setup encoder parameters based on config
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setupEncoderParams();

    /**
     * @brief Create input and output buffers
     * @return ErrorCode indicating success or failure
     */
    ErrorCode createBuffers();

    /**
     * @brief Register D3D11 texture as NVENC input
     * @param texture D3D11 texture to register
     * @return Registered resource handle
     */
    NV_ENC_REGISTERED_PTR registerInputResource(ComPtr<ID3D11Texture2D> texture);

    /**
     * @brief Unregister input resource
     * @param resource Resource to unregister
     */
    void unregisterInputResource(NV_ENC_REGISTERED_PTR resource);

    /**
     * @brief Map registered resource for encoding
     * @param resource Registered resource
     * @return Mapped input pointer
     */
    NV_ENC_INPUT_PTR mapInputResource(NV_ENC_REGISTERED_PTR resource);

    /**
     * @brief Unmap input resource after encoding
     * @param resource Mapped resource
     */
    void unmapInputResource(NV_ENC_INPUT_PTR resource);

    /**
     * @brief Get an available input resource slot
     * @return Index of available resource, or -1 if none
     */
    int getAvailableInputResource();

    /**
     * @brief Get an available output buffer slot
     * @return Index of available buffer, or -1 if none
     */
    int getAvailableOutputBuffer();

    /**
     * @brief Process encoded output
     * @param outputBuffer Output buffer to process
     * @param callback Function to call with encoded data
     * @return ErrorCode indicating success or failure
     */
    ErrorCode processOutput(
        NV_ENC_OUTPUT_PTR outputBuffer,
        EncodedFrameCallback callback
    );

    /**
     * @brief Convert codec type to NVENC GUID
     * @param codec Codec type
     * @return NVENC codec GUID
     */
    GUID codecToGUID(CodecType codec);

    /**
     * @brief Convert preset to NVENC GUID
     * @param preset Encoder preset
     * @return NVENC preset GUID
     */
    GUID presetToGUID(EncoderPreset preset);

    /**
     * @brief Setup rate control parameters
     * @param rateControl Rate control structure to fill
     */
    void setupRateControl(NV_ENC_RC_PARAMS& rateControl);

    /**
     * @brief Cleanup encoder resources
     */
    void cleanup();

    /**
     * @brief Check NVENC error and log
     * @param nvStatus NVENC status code
     * @param context Error context message
     * @return true if no error
     */
    bool checkNVENCError(NVENCSTATUS nvStatus, const std::string& context);
};

} // namespace WGC
