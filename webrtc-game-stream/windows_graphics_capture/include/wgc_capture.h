#pragma once

#include "common.h"
#include "d3d_device.h"

namespace WGC {

/**
 * @brief Windows Graphics Capture implementation
 * 
 * This class handles:
 * - Window/monitor selection and capture
 * - Frame pool management
 * - Capture session lifecycle
 * - Frame arrival callbacks
 * - Dynamic resolution changes
 * - Cursor capture
 */
class WGCCapture {
public:
    using FrameCallback = std::function<void(const Frame&)>;

    WGCCapture(std::shared_ptr<D3DDevice> d3dDevice);
    ~WGCCapture();

    /**
     * @brief Initialize Windows Graphics Capture
     * @param config Capture configuration
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(const Config& config);

    /**
     * @brief Start capturing frames
     * @param callback Function to call when new frame arrives
     * @return ErrorCode indicating success or failure
     */
    ErrorCode startCapture(FrameCallback callback);

    /**
     * @brief Stop capturing frames
     */
    void stopCapture();

    /**
     * @brief Check if capture is currently active
     * @return true if capturing
     */
    bool isCapturing() const { return isCapturing_; }

    /**
     * @brief Get current capture resolution
     * @param width Output: current width
     * @param height Output: current height
     */
    void getCurrentResolution(int& width, int& height) const;

    /**
     * @brief Get capture statistics
     * @return Statistics structure
     */
    Statistics getStatistics() const { return stats_; }

    /**
     * @brief Reset statistics counters
     */
    void resetStatistics() { stats_.reset(); }

    /**
     * @brief Show window/monitor picker dialog
     * @return ErrorCode indicating success or failure
     */
    ErrorCode showPicker();

    /**
     * @brief Set capture target by window handle
     * @param hwnd Window handle to capture
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setCaptureTarget(HWND hwnd);

    /**
     * @brief Set capture target to primary monitor
     * @return ErrorCode indicating success or failure
     */
    ErrorCode setCapturePrimaryMonitor();

    /**
     * @brief Enable or disable cursor capture
     * @param enable true to capture cursor
     */
    void enableCursor(bool enable);

    /**
     * @brief Enable or disable border capture
     * @param enable true to capture window borders
     */
    void enableBorder(bool enable);

private:
    std::shared_ptr<D3DDevice> d3dDevice_;
    
    // WinRT objects
    winrt::GraphicsCaptureItem captureItem_{nullptr};
    winrt::Direct3D11CaptureFramePool framePool_{nullptr};
    winrt::GraphicsCaptureSession captureSession_{nullptr};
    
    // Frame handling
    FrameCallback frameCallback_;
    winrt::event_token frameArrivedToken_;
    
    // State
    std::atomic<bool> isCapturing_{false};
    std::atomic<bool> initialized_{false};
    Config config_;
    
    // Current capture dimensions
    std::atomic<int> currentWidth_{0};
    std::atomic<int> currentHeight_{0};
    
    // Statistics
    Statistics stats_;
    uint64_t captureStartTime_{0};
    
    // Synchronization
    mutable std::mutex mutex_;
    
    /**
     * @brief Create the frame pool for receiving frames
     * @param size Initial size of capture area
     * @return ErrorCode indicating success or failure
     */
    ErrorCode createFramePool(winrt::SizeInt32 size);

    /**
     * @brief Create and start the capture session
     * @return ErrorCode indicating success or failure
     */
    ErrorCode createCaptureSession();

    /**
     * @brief Handle frame arrived event
     * @param sender Frame pool that generated the event
     * @param args Event arguments
     */
    void onFrameArrived(
        winrt::Direct3D11CaptureFramePool const& sender,
        winrt::IInspectable const& args
    );

    /**
     * @brief Handle capture item closed event
     * @param sender Capture item
     * @param args Event arguments
     */
    void onCaptureItemClosed(
        winrt::GraphicsCaptureItem const& sender,
        winrt::IInspectable const& args
    );

    /**
     * @brief Recreate frame pool when resolution changes
     * @param newSize New capture size
     */
    void recreateFramePool(winrt::SizeInt32 newSize);

    /**
     * @brief Convert WinRT capture frame to our Frame structure
     * @param captureFrame WinRT frame from frame pool
     * @return Frame structure with texture and metadata
     */
    Frame convertFrame(winrt::Direct3D11CaptureFrame const& captureFrame);

    /**
     * @brief Initialize WinRT apartment for COM
     * @return true if successful
     */
    bool initializeWinRT();

    /**
     * @brief Cleanup WinRT resources
     */
    void cleanupWinRT();

    /**
     * @brief Get GraphicsCaptureItem from window handle
     * @param hwnd Window handle
     * @return GraphicsCaptureItem or nullptr
     */
    winrt::GraphicsCaptureItem createCaptureItemForWindow(HWND hwnd);

    /**
     * @brief Get GraphicsCaptureItem for primary monitor
     * @return GraphicsCaptureItem or nullptr
     */
    winrt::GraphicsCaptureItem createCaptureItemForMonitor();

    /**
     * @brief Check if Windows Graphics Capture is supported
     * @return true if WGC is available on this system
     */
    static bool isWGCSupported();

    /**
     * @brief Get minimum required Windows version
     * @return Version string
     */
    static std::string getMinimumWindowsVersion() { return "Windows 10 1903"; }
};

} // namespace WGC
