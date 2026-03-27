#include "wgc_capture.h"
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.System.h>
#include <Windows.Graphics.Capture.Interop.h>

namespace WGC {

WGCCapture::WGCCapture(std::shared_ptr<D3DDevice> d3dDevice)
    : d3dDevice_(d3dDevice) {
}

WGCCapture::~WGCCapture() {
    stopCapture();
    cleanupWinRT();
}

ErrorCode WGCCapture::initialize(const Config& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARNING("WGC already initialized");
        return ErrorCode::Success;
    }
    
    LOG_INFO("Initializing Windows Graphics Capture...");
    
    // Check if WGC is supported
    if (!isWGCSupported()) {
        LOG_ERROR("Windows Graphics Capture is not supported on this system");
        LOG_ERROR("Minimum required: " + getMinimumWindowsVersion());
        return ErrorCode::InitializationFailed;
    }
    
    // Initialize WinRT
    if (!initializeWinRT()) {
        LOG_ERROR("Failed to initialize WinRT");
        return ErrorCode::InitializationFailed;
    }
    
    config_ = config;
    
    // Setup capture target based on mode
    ErrorCode result = ErrorCode::Success;
    
    switch (config_.captureMode) {
        case CaptureMode::Primary:
            result = setCapturePrimaryMonitor();
            break;
            
        case CaptureMode::Monitor:
            result = setCapturePrimaryMonitor();
            break;
            
        case CaptureMode::Window:
            result = showPicker();
            break;
            
        default:
            LOG_ERROR("Invalid capture mode");
            return ErrorCode::InvalidConfiguration;
    }
    
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to setup capture target");
        return result;
    }
    
    initialized_ = true;
    LOG_INFO("Windows Graphics Capture initialized successfully");
    
    return ErrorCode::Success;
}

ErrorCode WGCCapture::startCapture(FrameCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("WGC not initialized");
        return ErrorCode::InitializationFailed;
    }
    
    if (isCapturing_) {
        LOG_WARNING("Capture already started");
        return ErrorCode::Success;
    }
    
    if (!captureItem_) {
        LOG_ERROR("No capture item selected");
        return ErrorCode::CaptureStartFailed;
    }
    
    frameCallback_ = callback;
    
    // Get initial size
    auto size = captureItem_.Size();
    currentWidth_ = size.Width;
    currentHeight_ = size.Height;
    
    LOG_INFO("Capture size: " + std::to_string(size.Width) + "x" + std::to_string(size.Height));
    
    // Create frame pool
    ErrorCode result = createFramePool(size);
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to create frame pool");
        return result;
    }
    
    // Create capture session
    result = createCaptureSession();
    if (result != ErrorCode::Success) {
        LOG_ERROR("Failed to create capture session");
        return result;
    }
    
    // Register frame arrived event
    frameArrivedToken_ = framePool_.FrameArrived(
        [this](auto&& sender, auto&& args) {
            onFrameArrived(sender, args);
        }
    );
    
    // Start capture
    captureSession_.StartCapture();
    
    isCapturing_ = true;
    captureStartTime_ = getCurrentTimestampUs();
    stats_.reset();
    
    LOG_INFO("Capture started successfully");
    
    return ErrorCode::Success;
}

void WGCCapture::stopCapture() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!isCapturing_) {
        return;
    }
    
    LOG_INFO("Stopping capture...");
    
    // Stop capture session
    if (captureSession_) {
        try {
            captureSession_.Close();
        } catch (...) {
            LOG_WARNING("Exception while closing capture session");
        }
        captureSession_ = nullptr;
    }
    
    // Unregister frame arrived event
    if (framePool_) {
        try {
            framePool_.FrameArrived(frameArrivedToken_);
        } catch (...) {
            LOG_WARNING("Exception while unregistering frame event");
        }
    }
    
    // Close frame pool
    if (framePool_) {
        try {
            framePool_.Close();
        } catch (...) {
            LOG_WARNING("Exception while closing frame pool");
        }
        framePool_ = nullptr;
    }
    
    isCapturing_ = false;
    
    LOG_INFO("Capture stopped");
}

void WGCCapture::getCurrentResolution(int& width, int& height) const {
    width = currentWidth_;
    height = currentHeight_;
}

ErrorCode WGCCapture::showPicker() {
    try {
        // Create picker
        winrt::GraphicsCapturePicker picker;
        
        // Show picker and wait for selection
        auto item = picker.PickSingleItemAsync().get();
        
        if (!item) {
            LOG_ERROR("No item selected from picker");
            return ErrorCode::PermissionDenied;
        }
        
        captureItem_ = item;
        
        LOG_INFO("Capture item selected from picker");
        
        return ErrorCode::Success;
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Failed to show picker: " + winrt::to_string(e.message()));
        return ErrorCode::PermissionDenied;
    }
}

ErrorCode WGCCapture::setCaptureTarget(HWND hwnd) {
    try {
        captureItem_ = createCaptureItemForWindow(hwnd);
        
        if (!captureItem_) {
            LOG_ERROR("Failed to create capture item for window");
            return ErrorCode::CaptureStartFailed;
        }
        
        LOG_INFO("Capture target set to window");
        
        return ErrorCode::Success;
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Failed to set capture target: " + winrt::to_string(e.message()));
        return ErrorCode::CaptureStartFailed;
    }
}

ErrorCode WGCCapture::setCapturePrimaryMonitor() {
    try {
        captureItem_ = createCaptureItemForMonitor();
        
        if (!captureItem_) {
            LOG_ERROR("Failed to create capture item for monitor");
            return ErrorCode::CaptureStartFailed;
        }
        
        LOG_INFO("Capture target set to primary monitor");
        
        return ErrorCode::Success;
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Failed to set capture target: " + winrt::to_string(e.message()));
        return ErrorCode::CaptureStartFailed;
    }
}

void WGCCapture::enableCursor(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (captureSession_) {
        captureSession_.IsCursorCaptureEnabled(enable);
    }
    
    config_.enableCursor = enable;
}

void WGCCapture::enableBorder(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (captureSession_) {
        captureSession_.IsBorderRequired(enable);
    }
    
    config_.enableBorderless = !enable;
}

ErrorCode WGCCapture::createFramePool(winrt::SizeInt32 size) {
    try {
        // Get Direct3D device from D3D11 device
        auto dxgiDevice = d3dDevice_->getDXGIDevice();
        
        winrt::com_ptr<::IInspectable> inspectable;
        CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.Get(), inspectable.put());
        
        auto d3dDevice = inspectable.as<winrt::IDirect3DDevice>();
        
        // Create frame pool
        framePool_ = winrt::Direct3D11CaptureFramePool::Create(
            d3dDevice,
            winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            FRAME_POOL_SIZE,
            size
        );
        
        if (!framePool_) {
            LOG_ERROR("Failed to create frame pool");
            return ErrorCode::FramePoolCreationFailed;
        }
        
        return ErrorCode::Success;
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Exception creating frame pool: " + winrt::to_string(e.message()));
        return ErrorCode::FramePoolCreationFailed;
    }
}

ErrorCode WGCCapture::createCaptureSession() {
    try {
        captureSession_ = framePool_.CreateCaptureSession(captureItem_);
        
        if (!captureSession_) {
            LOG_ERROR("Failed to create capture session");
            return ErrorCode::CaptureStartFailed;
        }
        
        // Configure session
        captureSession_.IsCursorCaptureEnabled(config_.enableCursor);
        captureSession_.IsBorderRequired(!config_.enableBorderless);
        
        return ErrorCode::Success;
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Exception creating capture session: " + winrt::to_string(e.message()));
        return ErrorCode::CaptureStartFailed;
    }
}

void WGCCapture::onFrameArrived(
    winrt::Direct3D11CaptureFramePool const& sender,
    winrt::IInspectable const& args
) {
    uint64_t frameStartTime = getCurrentTimestampUs();
    
    try {
        auto frame = sender.TryGetNextFrame();
        
        if (!frame) {
            stats_.framesDropped++;
            return;
        }
        
        // Check for size change
        auto contentSize = frame.ContentSize();
        if (contentSize.Width != currentWidth_ || contentSize.Height != currentHeight_) {
            LOG_INFO("Resolution changed: " + std::to_string(contentSize.Width) + 
                     "x" + std::to_string(contentSize.Height));
            recreateFramePool(contentSize);
            currentWidth_ = contentSize.Width;
            currentHeight_ = contentSize.Height;
        }
        
        // Convert frame
        Frame capturedFrame = convertFrame(frame);
        
        // Update statistics
        stats_.framesCaptured++;
        uint64_t captureLatency = getCurrentTimestampUs() - frameStartTime;
        stats_.averageCaptureLatency = 
            (stats_.averageCaptureLatency * (stats_.framesCaptured - 1) + 
             captureLatency / 1000.0) / stats_.framesCaptured;
        
        stats_.currentFPS = calculateFPS(stats_.framesCaptured, captureStartTime_);
        
        // Call user callback
        if (frameCallback_) {
            frameCallback_(capturedFrame);
        }
        
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Exception in frame arrived: " + winrt::to_string(e.message()));
        stats_.framesDropped++;
    } catch (...) {
        LOG_ERROR("Unknown exception in frame arrived");
        stats_.framesDropped++;
    }
}

void WGCCapture::onCaptureItemClosed(
    winrt::GraphicsCaptureItem const& sender,
    winrt::IInspectable const& args
) {
    LOG_INFO("Capture item closed");
    stopCapture();
}

void WGCCapture::recreateFramePool(winrt::SizeInt32 newSize) {
    try {
        framePool_.Recreate(
            framePool_.Device(),
            winrt::DirectXPixelFormat::B8G8R8A8UIntNormalized,
            FRAME_POOL_SIZE,
            newSize
        );
    } catch (const winrt::hresult_error& e) {
        LOG_ERROR("Failed to recreate frame pool: " + winrt::to_string(e.message()));
    }
}

Frame WGCCapture::convertFrame(winrt::Direct3D11CaptureFrame const& captureFrame) {
    Frame frame;
    
    frame.timestamp = getCurrentTimestampUs();
    frame.width = captureFrame.ContentSize().Width;
    frame.height = captureFrame.ContentSize().Height;
    frame.format = FrameFormat::BGRA8;
    frame.isKeyFrame = false;
    
    // Convert WinRT surface to D3D11 texture
    auto surface = captureFrame.Surface();
    frame.texture = d3dDevice_->surfaceToTexture(surface);
    
    return frame;
}

bool WGCCapture::initializeWinRT() {
    try {
        // WinRT should already be initialized in main
        return true;
    } catch (...) {
        return false;
    }
}

void WGCCapture::cleanupWinRT() {
    // Cleanup is handled by destructors
}

winrt::GraphicsCaptureItem WGCCapture::createCaptureItemForWindow(HWND hwnd) {
    auto interop = winrt::get_activation_factory<
        winrt::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();
    
    winrt::GraphicsCaptureItem item{nullptr};
    interop->CreateForWindow(
        hwnd,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        winrt::put_abi(item)
    );
    
    return item;
}

winrt::GraphicsCaptureItem WGCCapture::createCaptureItemForMonitor() {
    // Get primary monitor
    HMONITOR hMonitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
    
    auto interop = winrt::get_activation_factory<
        winrt::GraphicsCaptureItem,
        IGraphicsCaptureItemInterop>();
    
    winrt::GraphicsCaptureItem item{nullptr};
    interop->CreateForMonitor(
        hMonitor,
        winrt::guid_of<ABI::Windows::Graphics::Capture::IGraphicsCaptureItem>(),
        winrt::put_abi(item)
    );
    
    return item;
}

bool WGCCapture::isWGCSupported() {
    try {
        return winrt::GraphicsCaptureSession::IsSupported();
    } catch (...) {
        return false;
    }
}

} // namespace WGC
