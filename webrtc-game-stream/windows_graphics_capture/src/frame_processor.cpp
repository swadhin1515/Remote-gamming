#include "frame_processor.h"

namespace WGC {

FrameProcessor::FrameProcessor(std::shared_ptr<D3DDevice> d3dDevice)
    : d3dDevice_(d3dDevice) {
}

FrameProcessor::~FrameProcessor() {
    conversionTexture_.Reset();
    scalingTexture_.Reset();
}

ErrorCode FrameProcessor::initialize(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    LOG_INFO("Initializing frame processor...");
    
    config_ = config;
    
    // Calculate frame interval for FPS limiting
    frameInterval_ = 1000000 / config_.targetFPS; // microseconds
    
    // Setup conversion resources
    ErrorCode result = setupConversionResources();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to setup conversion resources");
        return result;
    }
    
    stats_.reset();
    
    LOG_INFO("Frame processor initialized successfully");
    
    return ErrorCode::Success;
}

ErrorCode FrameProcessor::processFrame(const Frame& inputFrame, Frame& outputFrame) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    uint64_t processStartTime = getCurrentTimestampUs();
    
    // Check if we should process this frame based on FPS limit
    if (!shouldProcessFrame()) {
        return ErrorCode::Success; // Skip frame
    }
    
    // Copy input frame properties
    outputFrame = inputFrame;
    
    // Check if we need to resize
    if (inputFrame.width != config_.targetWidth || 
        inputFrame.height != config_.targetHeight) {
        
        ErrorCode result = resizeFrame(inputFrame, outputFrame, 
                                       config_.targetWidth, config_.targetHeight);
        if (result != ErrorCode::Success) {
            LOG_ERROR("Failed to resize frame");
            return result;
        }
    }
    
    // Update timestamp
    outputFrame.timestamp = getCurrentTimestampUs();
    lastFrameTime_ = outputFrame.timestamp;
    
    return ErrorCode::Success;
}

ErrorCode FrameProcessor::convertFormat(
    const Frame& input,
    Frame& output,
    FrameFormat targetFormat
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (input.format == targetFormat) {
        output = input;
        return ErrorCode::Success;
    }
    
    // Currently only support BGRA to NV12 conversion
    if (input.format == FrameFormat::BGRA8 && targetFormat == FrameFormat::NV12) {
        return convertBGRAToNV12(input.texture, output.texture);
    }
    
    LOG_ERROR("Unsupported format conversion");
    return ErrorCode::UnsupportedFormat;
}

ErrorCode FrameProcessor::resizeFrame(
    const Frame& input,
    Frame& output,
    int targetWidth,
    int targetHeight
) {
    if (!d3dDevice_->isValid()) {
        return ErrorCode::DeviceCreationFailed;
    }
    
    // Create or reuse scaling texture
    if (!scalingTexture_ || 
        d3dDevice_->getTextureDesc(scalingTexture_).Width != targetWidth ||
        d3dDevice_->getTextureDesc(scalingTexture_).Height != targetHeight) {
        
        scalingTexture_ = d3dDevice_->createSharedTexture(
            targetWidth, 
            targetHeight,
            DXGI_FORMAT_B8G8R8A8_UNORM
        );
        
        if (!scalingTexture_) {
            LOG_ERROR("Failed to create scaling texture");
            return ErrorCode::InitializationFailed;
        }
    }
    
    // Use D3D11 to scale (simplified - production would use proper scaling shader)
    d3dDevice_->copyTexture(input.texture, scalingTexture_);
    
    output.texture = scalingTexture_;
    output.width = targetWidth;
    output.height = targetHeight;
    output.format = input.format;
    output.timestamp = input.timestamp;
    output.isKeyFrame = input.isKeyFrame;
    
    return ErrorCode::Success;
}

bool FrameProcessor::shouldProcessFrame() {
    uint64_t currentTime = getCurrentTimestampUs();
    
    if (lastFrameTime_ == 0) {
        lastFrameTime_ = currentTime;
        return true;
    }
    
    uint64_t elapsed = currentTime - lastFrameTime_;
    
    return elapsed >= frameInterval_;
}

ErrorCode FrameProcessor::setupConversionResources() {
    // Create conversion texture if needed
    if (config_.targetWidth > 0 && config_.targetHeight > 0) {
        conversionTexture_ = d3dDevice_->createSharedTexture(
            config_.targetWidth,
            config_.targetHeight,
            DXGI_FORMAT_B8G8R8A8_UNORM
        );
        
        if (!conversionTexture_) {
            LOG_ERROR("Failed to create conversion texture");
            return ErrorCode::InitializationFailed;
        }
    }
    
    return ErrorCode::Success;
}

ErrorCode FrameProcessor::convertBGRAToNV12(
    ComPtr<ID3D11Texture2D> input,
    ComPtr<ID3D11Texture2D> output
) {
    // This is a placeholder - actual implementation would use compute shader
    // or Video Processor for efficient BGRA to NV12 conversion
    
    LOG_WARNING("BGRA to NV12 conversion not fully implemented");
    
    // For now, just copy the texture
    d3dDevice_->copyTexture(input, output);
    
    return ErrorCode::Success;
}

} // namespace WGC
