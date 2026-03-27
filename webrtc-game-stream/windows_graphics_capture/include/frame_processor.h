#pragma once

#include "common.h"
#include "d3d_device.h"

namespace WGC {

/**
 * @brief Frame processing and format conversion utilities
 * 
 * This class handles:
 * - Frame format conversion (BGRA to NV12, etc.)
 * - Frame scaling and resizing
 * - Frame rate limiting
 * - Frame queue management
 * - Performance monitoring
 */
class FrameProcessor {
public:
    FrameProcessor(std::shared_ptr<D3DDevice> d3dDevice);
    ~FrameProcessor();

    /**
     * @brief Initialize frame processor
     * @param config Configuration
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(const Config& config);

    /**
     * @brief Process a captured frame
     * @param inputFrame Input frame from WGC
     * @param outputFrame Output frame ready for encoding
     * @return ErrorCode indicating success or failure
     */
    ErrorCode processFrame(const Frame& inputFrame, Frame& outputFrame);

    /**
     * @brief Convert frame format
     * @param input Input frame
     * @param output Output frame
     * @param targetFormat Target pixel format
     * @return ErrorCode indicating success or failure
     */
    ErrorCode convertFormat(
        const Frame& input,
        Frame& output,
        FrameFormat targetFormat
    );

    /**
     * @brief Resize frame to target dimensions
     * @param input Input frame
     * @param output Output frame
     * @param targetWidth Target width
     * @param targetHeight Target height
     * @return ErrorCode indicating success or failure
     */
    ErrorCode resizeFrame(
        const Frame& input,
        Frame& output,
        int targetWidth,
        int targetHeight
    );

    /**
     * @brief Check if frame should be processed based on FPS limit
     * @return true if frame should be processed
     */
    bool shouldProcessFrame();

    /**
     * @brief Get processing statistics
     * @return Statistics structure
     */
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics
     */
    void resetStatistics() { stats_.reset(); }

private:
    std::shared_ptr<D3DDevice> d3dDevice_;
    Config config_;
    
    // Format conversion resources
    ComPtr<ID3D11Texture2D> conversionTexture_;
    ComPtr<ID3D11Texture2D> scalingTexture_;
    
    // Frame rate limiting
    uint64_t lastFrameTime_{0};
    uint64_t frameInterval_{0};
    
    // Statistics
    Statistics stats_;
    
    // Synchronization
    mutable std::mutex mutex_;
    
    /**
     * @brief Setup format conversion resources
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setupConversionResources();

    /**
     * @brief Convert BGRA to NV12 using GPU
     * @param input Input BGRA texture
     * @param output Output NV12 texture
     * @return ErrorCode indicating success or failure
     */
    ErrorCode convertBGRAToNV12(
        ComPtr<ID3D11Texture2D> input,
        ComPtr<ID3D11Texture2D> output
    );
};

} // namespace WGC
