#pragma once

#include "common.h"

namespace WGC {

/**
 * @brief Manages Direct3D 11 device and context for Windows Graphics Capture
 * 
 * This class handles:
 * - D3D11 device creation with BGRA support
 * - Device context management
 * - DXGI device and adapter queries
 * - Texture creation and manipulation
 * - Resource sharing between WGC and NVENC
 */
class D3DDevice {
public:
    D3DDevice();
    ~D3DDevice();

    /**
     * @brief Initialize the D3D11 device
     * @param enableDebug Enable D3D debug layer (for development)
     * @return ErrorCode indicating success or failure
     */
    ErrorCode initialize(bool enableDebug = false);

    /**
     * @brief Get the D3D11 device
     * @return ComPtr to ID3D11Device
     */
    ComPtr<ID3D11Device> getDevice() const { return d3dDevice_; }

    /**
     * @brief Get the D3D11 device context
     * @return ComPtr to ID3D11DeviceContext
     */
    ComPtr<ID3D11DeviceContext> getContext() const { return d3dContext_; }

    /**
     * @brief Get the DXGI device
     * @return ComPtr to IDXGIDevice
     */
    ComPtr<IDXGIDevice> getDXGIDevice() const { return dxgiDevice_; }

    /**
     * @brief Get the DXGI adapter
     * @return ComPtr to IDXGIAdapter
     */
    ComPtr<IDXGIAdapter> getAdapter() const { return adapter_; }

    /**
     * @brief Create a staging texture for CPU readback (if needed)
     * @param width Texture width
     * @param height Texture height
     * @param format Pixel format
     * @return ComPtr to staging texture
     */
    ComPtr<ID3D11Texture2D> createStagingTexture(
        int width, 
        int height, 
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM
    );

    /**
     * @brief Create a shared texture for zero-copy encoding
     * @param width Texture width
     * @param height Texture height
     * @param format Pixel format
     * @return ComPtr to shared texture
     */
    ComPtr<ID3D11Texture2D> createSharedTexture(
        int width, 
        int height, 
        DXGI_FORMAT format = DXGI_FORMAT_B8G8R8A8_UNORM
    );

    /**
     * @brief Copy texture from source to destination
     * @param src Source texture
     * @param dst Destination texture
     */
    void copyTexture(
        ComPtr<ID3D11Texture2D> src, 
        ComPtr<ID3D11Texture2D> dst
    );

    /**
     * @brief Copy a region of a texture
     * @param src Source texture
     * @param dst Destination texture
     * @param srcX Source X coordinate
     * @param srcY Source Y coordinate
     * @param width Region width
     * @param height Region height
     * @param dstX Destination X coordinate
     * @param dstY Destination Y coordinate
     */
    void copyTextureRegion(
        ComPtr<ID3D11Texture2D> src,
        ComPtr<ID3D11Texture2D> dst,
        int srcX, int srcY,
        int width, int height,
        int dstX, int dstY
    );

    /**
     * @brief Get texture description
     * @param texture Texture to query
     * @return D3D11_TEXTURE2D_DESC structure
     */
    D3D11_TEXTURE2D_DESC getTextureDesc(ComPtr<ID3D11Texture2D> texture);

    /**
     * @brief Convert WinRT Direct3D11 surface to ID3D11Texture2D
     * @param surface WinRT surface from WGC
     * @return ComPtr to ID3D11Texture2D
     */
    ComPtr<ID3D11Texture2D> surfaceToTexture(
        winrt::IDirect3DSurface const& surface
    );

    /**
     * @brief Check if device is valid and ready
     * @return true if device is initialized
     */
    bool isValid() const { return d3dDevice_ != nullptr; }

    /**
     * @brief Get the feature level of the device
     * @return D3D_FEATURE_LEVEL
     */
    D3D_FEATURE_LEVEL getFeatureLevel() const { return featureLevel_; }

    /**
     * @brief Flush the device context
     */
    void flush();

    /**
     * @brief Get adapter description
     * @return DXGI_ADAPTER_DESC with GPU information
     */
    DXGI_ADAPTER_DESC getAdapterDesc() const;

    /**
     * @brief Check if NVIDIA GPU is available
     * @return true if NVIDIA GPU detected
     */
    bool isNvidiaGPU() const;

    /**
     * @brief Get VRAM usage information
     * @param availableBytes Output: available VRAM in bytes
     * @param totalBytes Output: total VRAM in bytes
     * @return true if query succeeded
     */
    bool getVRAMUsage(size_t& availableBytes, size_t& totalBytes) const;

private:
    ComPtr<ID3D11Device> d3dDevice_;
    ComPtr<ID3D11DeviceContext> d3dContext_;
    ComPtr<IDXGIDevice> dxgiDevice_;
    ComPtr<IDXGIAdapter> adapter_;
    D3D_FEATURE_LEVEL featureLevel_;
    
    bool initialized_;
    mutable std::mutex mutex_;

    /**
     * @brief Create the D3D11 device with appropriate flags
     * @param enableDebug Enable debug layer
     * @return HRESULT
     */
    HRESULT createDevice(bool enableDebug);

    /**
     * @brief Query DXGI interfaces from D3D device
     * @return HRESULT
     */
    HRESULT queryDXGIInterfaces();
};

} // namespace WGC
