#include "nvenc_encoder.h"

namespace WGC {

NVENCEncoder::NVENCEncoder(std::shared_ptr<D3DDevice> d3dDevice)
    : d3dDevice_(d3dDevice) {
}

NVENCEncoder::~NVENCEncoder() {
    cleanup();
}

ErrorCode NVENCEncoder::initialize(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARNING("NVENC already initialized");
        return ErrorCode::Success;
    }
    
    LOG_INFO("Initializing NVENC encoder...");
    
    config_ = config;
    
    // Load NVENC API
    ErrorCode result = loadNVENCAPI();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to load NVENC API");
        return result;
    }
    
    // Open encoder
    result = openEncoder();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to open NVENC encoder");
        return result;
    }
    
    // Setup encoder parameters
    result = setupEncoderParams();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to setup encoder parameters");
        return result;
    }
    
    // Initialize encoder
    NVENCSTATUS nvStatus = nvencAPI_.nvEncInitializeEncoder(
        nvencEncoder_,
        &initParams_
    );
    
    if (!checkNVENCError(nvStatus, "Initialize encoder")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    // Create buffers
    result = createBuffers();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to create encoder buffers");
        return result;
    }
    
    initialized_ = true;
    encodeStartTime_ = getCurrentTimestampUs();
    stats_.reset();
    
    LOG_INFO("NVENC encoder initialized successfully");
    LOG_INFO("Codec: " + std::string(config_.codec == CodecType::H264 ? "H.264" : 
                                     config_.codec == CodecType::HEVC ? "HEVC" : "AV1"));
    LOG_INFO("Bitrate: " + std::to_string(config_.bitrate / 1000) + " kbps");
    LOG_INFO("Resolution: " + std::to_string(config_.targetWidth) + "x" + 
             std::to_string(config_.targetHeight));
    
    return ErrorCode::Success;
}

ErrorCode NVENCEncoder::encodeFrame(const Frame& frame, EncodedFrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Encoder not initialized");
        return ErrorCode::EncoderInitFailed;
    }
    
    uint64_t encodeStartTime = getCurrentTimestampUs();
    
    // Get available input resource
    int inputIdx = getAvailableInputResource();
    if (inputIdx < 0) {
        LOG_WARNING("No available input resources, dropping frame");
        stats_.framesDropped++;
        return ErrorCode::Success; // Not a fatal error
    }
    
    // Get available output buffer
    int outputIdx = getAvailableOutputBuffer();
    if (outputIdx < 0) {
        LOG_WARNING("No available output buffers, dropping frame");
        stats_.framesDropped++;
        return ErrorCode::Success;
    }
    
    auto& inputRes = inputResources_[inputIdx];
    auto& outputBuf = outputBuffers_[outputIdx];
    
    // Register and map input texture if needed
    if (!inputRes.registeredResource) {
        inputRes.registeredResource = registerInputResource(frame.texture);
        if (!inputRes.registeredResource) {
            LOG_ERROR("Failed to register input resource");
            return ErrorCode::EncoderInitFailed;
        }
    }
    
    inputRes.mappedResource = mapInputResource(inputRes.registeredResource);
    if (!inputRes.mappedResource) {
        LOG_ERROR("Failed to map input resource");
        return ErrorCode::EncoderInitFailed;
    }
    
    // Setup encode picture parameters
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.pictureStruct = NV_ENC_PIC_STRUCT_FRAME;
    picParams.inputBuffer = inputRes.mappedResource;
    picParams.bufferFmt = NV_ENC_BUFFER_FORMAT_ARGB;
    picParams.inputWidth = frame.width;
    picParams.inputHeight = frame.height;
    picParams.outputBitstream = outputBuf.bitstreamBuffer;
    picParams.inputTimeStamp = frame.timestamp;
    
    // Force IDR if requested
    if (forceIDR_) {
        picParams.encodePicFlags = NV_ENC_PIC_FLAG_FORCEIDR;
        forceIDR_ = false;
    }
    
    // Encode frame
    NVENCSTATUS nvStatus = nvencAPI_.nvEncEncodePicture(nvencEncoder_, &picParams);
    
    // Unmap input resource
    unmapInputResource(inputRes.mappedResource);
    inputRes.mappedResource = nullptr;
    inputRes.inUse = false;
    
    if (!checkNVENCError(nvStatus, "Encode picture")) {
        outputBuf.inUse = false;
        return ErrorCode::EncoderInitFailed;
    }
    
    // Process output
    ErrorCode result = processOutput(outputBuf.bitstreamBuffer, callback);
    outputBuf.inUse = false;
    
    if (result != ErrorCode::Success) {
        return result;
    }
    
    // Update statistics
    stats_.framesEncoded++;
    frameCount_++;
    uint64_t encodeLatency = getCurrentTimestampUs() - encodeStartTime;
    stats_.averageEncodeLatency = 
        (stats_.averageEncodeLatency * (stats_.framesEncoded - 1) + 
         encodeLatency / 1000.0) / stats_.framesEncoded;
    
    return ErrorCode::Success;
}

ErrorCode NVENCEncoder::flush(EncodedFrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::EncoderInitFailed;
    }
    
    LOG_INFO("Flushing encoder...");
    
    // Send EOS
    NV_ENC_PIC_PARAMS picParams = {};
    picParams.version = NV_ENC_PIC_PARAMS_VER;
    picParams.encodePicFlags = NV_ENC_PIC_FLAG_EOS;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncEncodePicture(nvencEncoder_, &picParams);
    
    if (!checkNVENCError(nvStatus, "Flush encoder")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    LOG_INFO("Encoder flushed");
    
    return ErrorCode::Success;
}

ErrorCode NVENCEncoder::reconfigure(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return ErrorCode::EncoderInitFailed;
    }
    
    LOG_INFO("Reconfiguring encoder...");
    
    // Setup reconfigure parameters
    NV_ENC_RECONFIGURE_PARAMS reconfigParams = {};
    reconfigParams.version = NV_ENC_RECONFIGURE_PARAMS_VER;
    reconfigParams.reInitEncodeParams = initParams_;
    
    // Update bitrate if changed
    if (config.bitrate != config_.bitrate) {
        reconfigParams.reInitEncodeParams.encodeConfig->rcParams.averageBitRate = config.bitrate;
        reconfigParams.reInitEncodeParams.encodeConfig->rcParams.maxBitRate = config.bitrate;
    }
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncReconfigureEncoder(
        nvencEncoder_,
        &reconfigParams
    );
    
    if (!checkNVENCError(nvStatus, "Reconfigure encoder")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    config_ = config;
    
    LOG_INFO("Encoder reconfigured successfully");
    
    return ErrorCode::Success;
}

void NVENCEncoder::forceIDR() {
    forceIDR_ = true;
}

std::vector<CodecType> NVENCEncoder::getSupportedCodecs() {
    std::vector<CodecType> codecs;
    
    // This is a simplified version - actual implementation would query NVENC
    codecs.push_back(CodecType::H264);
    codecs.push_back(CodecType::HEVC);
    
    return codecs;
}

bool NVENCEncoder::isNVENCAvailable() {
    // Try to load NVENC library
    HMODULE nvencLib = LoadLibraryA("nvEncodeAPI64.dll");
    if (!nvencLib) {
        return false;
    }
    
    FreeLibrary(nvencLib);
    return true;
}

std::string NVENCEncoder::getNVENCVersion() {
    return "12.0"; // Placeholder - actual version would be queried
}

ErrorCode NVENCEncoder::loadNVENCAPI() {
    // Load NVENC library
    HMODULE nvencLib = LoadLibraryA("nvEncodeAPI64.dll");
    if (!nvencLib) {
        LOG_ERROR("Failed to load nvEncodeAPI64.dll");
        return ErrorCode::EncoderInitFailed;
    }
    
    // Get API function
    typedef NVENCSTATUS (NVENCAPI* NvEncodeAPICreateInstanceFunc)(NV_ENCODE_API_FUNCTION_LIST*);
    auto createInstance = (NvEncodeAPICreateInstanceFunc)GetProcAddress(
        nvencLib,
        "NvEncodeAPICreateInstance"
    );
    
    if (!createInstance) {
        LOG_ERROR("Failed to get NvEncodeAPICreateInstance");
        FreeLibrary(nvencLib);
        return ErrorCode::EncoderInitFailed;
    }
    
    // Create API instance
    nvencAPI_ = {};
    nvencAPI_.version = NV_ENCODE_API_FUNCTION_LIST_VER;
    
    NVENCSTATUS nvStatus = createInstance(&nvencAPI_);
    if (!checkNVENCError(nvStatus, "Create API instance")) {
        FreeLibrary(nvencLib);
        return ErrorCode::EncoderInitFailed;
    }
    
    return ErrorCode::Success;
}

ErrorCode NVENCEncoder::openEncoder() {
    // Open encode session
    NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS openParams = {};
    openParams.version = NV_ENC_OPEN_ENCODE_SESSION_EX_PARAMS_VER;
    openParams.deviceType = NV_ENC_DEVICE_TYPE_DIRECTX;
    openParams.device = d3dDevice_->getDevice().Get();
    openParams.apiVersion = NVENCAPI_VERSION;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncOpenEncodeSessionEx(
        &openParams,
        &nvencEncoder_
    );
    
    if (!checkNVENCError(nvStatus, "Open encode session")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    return ErrorCode::Success;
}

bool NVENCEncoder::getEncoderCaps(GUID codec) {
    NV_ENC_CAPS_PARAM capsParam = {};
    capsParam.version = NV_ENC_CAPS_PARAM_VER;
    capsParam.capsToQuery = NV_ENC_CAPS_SUPPORTED_RATECONTROL_MODES;
    
    int capsValue = 0;
    NVENCSTATUS nvStatus = nvencAPI_.nvEncGetEncodeCaps(
        nvencEncoder_,
        codec,
        &capsParam,
        &capsValue
    );
    
    return SUCCEEDED(nvStatus) && capsValue != 0;
}

ErrorCode NVENCEncoder::setupEncoderParams() {
    // Initialize encode params
    initParams_ = {};
    initParams_.version = NV_ENC_INITIALIZE_PARAMS_VER;
    initParams_.encodeGUID = codecToGUID(config_.codec);
    initParams_.presetGUID = presetToGUID(config_.preset);
    initParams_.encodeWidth = config_.targetWidth;
    initParams_.encodeHeight = config_.targetHeight;
    initParams_.darWidth = config_.targetWidth;
    initParams_.darHeight = config_.targetHeight;
    initParams_.frameRateNum = config_.targetFPS;
    initParams_.frameRateDen = 1;
    initParams_.enablePTD = 1;
    initParams_.reportSliceOffsets = 0;
    initParams_.enableSubFrameWrite = 0;
    initParams_.maxEncodeWidth = config_.targetWidth;
    initParams_.maxEncodeHeight = config_.targetHeight;
    
    // Get preset config
    NV_ENC_PRESET_CONFIG presetConfig = {};
    presetConfig.version = NV_ENC_PRESET_CONFIG_VER;
    presetConfig.presetCfg.version = NV_ENC_CONFIG_VER;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncGetEncodePresetConfig(
        nvencEncoder_,
        initParams_.encodeGUID,
        initParams_.presetGUID,
        &presetConfig
    );
    
    if (!checkNVENCError(nvStatus, "Get preset config")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    encodeConfig_ = presetConfig.presetCfg;
    initParams_.encodeConfig = &encodeConfig_;
    
    // Setup rate control
    setupRateControl(encodeConfig_.rcParams);
    
    // GOP settings
    encodeConfig_.gopLength = config_.gopSize;
    encodeConfig_.frameIntervalP = config_.enableBFrames ? 3 : 1;
    
    return ErrorCode::Success;
}

ErrorCode NVENCEncoder::createBuffers() {
    // Create input resources (will be registered on first use)
    inputResources_.resize(FRAME_POOL_SIZE);
    for (auto& res : inputResources_) {
        res.registeredResource = nullptr;
        res.mappedResource = nullptr;
        res.inUse = false;
    }
    
    // Create output buffers
    outputBuffers_.resize(FRAME_POOL_SIZE);
    for (auto& buf : outputBuffers_) {
        NV_ENC_CREATE_BITSTREAM_BUFFER createBitstreamBuffer = {};
        createBitstreamBuffer.version = NV_ENC_CREATE_BITSTREAM_BUFFER_VER;
        
        NVENCSTATUS nvStatus = nvencAPI_.nvEncCreateBitstreamBuffer(
            nvencEncoder_,
            &createBitstreamBuffer
        );
        
        if (!checkNVENCError(nvStatus, "Create bitstream buffer")) {
            return ErrorCode::EncoderInitFailed;
        }
        
        buf.bitstreamBuffer = createBitstreamBuffer.bitstreamBuffer;
        buf.inUse = false;
    }
    
    return ErrorCode::Success;
}

NV_ENC_REGISTERED_PTR NVENCEncoder::registerInputResource(ComPtr<ID3D11Texture2D> texture) {
    NV_ENC_REGISTER_RESOURCE registerResource = {};
    registerResource.version = NV_ENC_REGISTER_RESOURCE_VER;
    registerResource.resourceType = NV_ENC_INPUT_RESOURCE_TYPE_DIRECTX;
    registerResource.resourceToRegister = texture.Get();
    registerResource.width = config_.targetWidth;
    registerResource.height = config_.targetHeight;
    registerResource.bufferFormat = NV_ENC_BUFFER_FORMAT_ARGB;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncRegisterResource(
        nvencEncoder_,
        &registerResource
    );
    
    if (!checkNVENCError(nvStatus, "Register resource")) {
        return nullptr;
    }
    
    return registerResource.registeredResource;
}

void NVENCEncoder::unregisterInputResource(NV_ENC_REGISTERED_PTR resource) {
    if (resource) {
        nvencAPI_.nvEncUnregisterResource(nvencEncoder_, resource);
    }
}

NV_ENC_INPUT_PTR NVENCEncoder::mapInputResource(NV_ENC_REGISTERED_PTR resource) {
    NV_ENC_MAP_INPUT_RESOURCE mapInputResource = {};
    mapInputResource.version = NV_ENC_MAP_INPUT_RESOURCE_VER;
    mapInputResource.registeredResource = resource;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncMapInputResource(
        nvencEncoder_,
        &mapInputResource
    );
    
    if (!checkNVENCError(nvStatus, "Map input resource")) {
        return nullptr;
    }
    
    return mapInputResource.mappedResource;
}

void NVENCEncoder::unmapInputResource(NV_ENC_INPUT_PTR resource) {
    if (resource) {
        nvencAPI_.nvEncUnmapInputResource(nvencEncoder_, resource);
    }
}

int NVENCEncoder::getAvailableInputResource() {
    for (size_t i = 0; i < inputResources_.size(); i++) {
        if (!inputResources_[i].inUse) {
            inputResources_[i].inUse = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

int NVENCEncoder::getAvailableOutputBuffer() {
    for (size_t i = 0; i < outputBuffers_.size(); i++) {
        if (!outputBuffers_[i].inUse) {
            outputBuffers_[i].inUse = true;
            return static_cast<int>(i);
        }
    }
    return -1;
}

ErrorCode NVENCEncoder::processOutput(
    NV_ENC_OUTPUT_PTR outputBuffer,
    EncodedFrameCallback callback
) {
    // Lock bitstream
    NV_ENC_LOCK_BITSTREAM lockBitstream = {};
    lockBitstream.version = NV_ENC_LOCK_BITSTREAM_VER;
    lockBitstream.outputBitstream = outputBuffer;
    
    NVENCSTATUS nvStatus = nvencAPI_.nvEncLockBitstream(nvencEncoder_, &lockBitstream);
    
    if (!checkNVENCError(nvStatus, "Lock bitstream")) {
        return ErrorCode::EncoderInitFailed;
    }
    
    // Create encoded frame
    EncodedFrame encodedFrame;
    encodedFrame.data.resize(lockBitstream.bitstreamSizeInBytes);
    memcpy(encodedFrame.data.data(), lockBitstream.bitstreamBufferPtr, 
           lockBitstream.bitstreamSizeInBytes);
    encodedFrame.timestamp = lockBitstream.outputTimeStamp;
    encodedFrame.isKeyFrame = (lockBitstream.pictureType == NV_ENC_PIC_TYPE_IDR);
    encodedFrame.width = config_.targetWidth;
    encodedFrame.height = config_.targetHeight;
    
    // Unlock bitstream
    nvencAPI_.nvEncUnlockBitstream(nvencEncoder_, outputBuffer);
    
    // Update statistics
    stats_.totalBytesEncoded += encodedFrame.data.size();
    
    // Call callback
    if (callback) {
        callback(encodedFrame);
    }
    
    return ErrorCode::Success;
}

GUID NVENCEncoder::codecToGUID(CodecType codec) {
    switch (codec) {
        case CodecType::H264:
            return NV_ENC_CODEC_H264_GUID;
        case CodecType::HEVC:
            return NV_ENC_CODEC_HEVC_GUID;
        default:
            return NV_ENC_CODEC_H264_GUID;
    }
}

GUID NVENCEncoder::presetToGUID(EncoderPreset preset) {
    switch (preset) {
        case EncoderPreset::UltraLowLatency:
            return NV_ENC_PRESET_LOW_LATENCY_HQ_GUID;
        case EncoderPreset::LowLatency:
            return NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
        case EncoderPreset::Quality:
            return NV_ENC_PRESET_HQ_GUID;
        case EncoderPreset::HighQuality:
            return NV_ENC_PRESET_LOSSLESS_HP_GUID;
        default:
            return NV_ENC_PRESET_LOW_LATENCY_DEFAULT_GUID;
    }
}

void NVENCEncoder::setupRateControl(NV_ENC_RC_PARAMS& rateControl) {
    rateControl.rateControlMode = NV_ENC_PARAMS_RC_CBR;
    rateControl.averageBitRate = config_.bitrate;
    rateControl.maxBitRate = config_.bitrate;
    rateControl.vbvBufferSize = config_.bitrate / config_.targetFPS;
    rateControl.vbvInitialDelay = rateControl.vbvBufferSize;
    rateControl.enableMinQP = 1;
    rateControl.enableMaxQP = 1;
    rateControl.minQP = {20, 20, 20};
    rateControl.maxQP = {51, 51, 51};
}

void NVENCEncoder::cleanup() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return;
    }
    
    LOG_INFO("Cleaning up NVENC encoder...");
    
    // Unregister input resources
    for (auto& res : inputResources_) {
        if (res.registeredResource) {
            unregisterInputResource(res.registeredResource);
        }
    }
    inputResources_.clear();
    
    // Destroy output buffers
    for (auto& buf : outputBuffers_) {
        if (buf.bitstreamBuffer) {
            nvencAPI_.nvEncDestroyBitstreamBuffer(nvencEncoder_, buf.bitstreamBuffer);
        }
    }
    outputBuffers_.clear();
    
    // Destroy encoder
    if (nvencEncoder_) {
        nvencAPI_.nvEncDestroyEncoder(nvencEncoder_);
        nvencEncoder_ = nullptr;
    }
    
    initialized_ = false;
    
    LOG_INFO("NVENC encoder cleaned up");
}

bool NVENCEncoder::checkNVENCError(NVENCSTATUS nvStatus, const std::string& context) {
    if (nvStatus != NV_ENC_SUCCESS) {
        LOG_ERROR(context + " failed with NVENC error: " + std::to_string(nvStatus));
        return false;
    }
    return true;
}

} // namespace WGC
