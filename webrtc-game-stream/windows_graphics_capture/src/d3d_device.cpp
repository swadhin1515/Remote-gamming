#include "d3d_device.h"
#include <dxgi1_6.h>

namespace WGC {

D3DDevice::D3DDevice() 
    : initialized_(false), featureLevel_(D3D_FEATURE_LEVEL_11_0) {
}

D3DDevice::~D3DDevice() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (d3dContext_) {
        d3dContext_->ClearState();
        d3dContext_->Flush();
    }
    
    d3dContext_.Reset();
    dxgiDevice_.Reset();
    adapter_.Reset();
    d3dDevice_.Reset();
    
    initialized_ = false;
}

ErrorCode D3DDevice::initialize(bool enableDebug) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        LOG_WARNING("D3D device already initialized");
        return ErrorCode::Success;
    }
    
    LOG_INFO("Initializing Direct3D 11 device...");
    
    // Create D3D device
    HRESULT hr = createDevice(enableDebug);
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create D3D11 device");
        return ErrorCode::DeviceCreationFailed;
    }
    
    // Query DXGI interfaces
    hr = queryDXGIInterfaces();
    if (FAILED(hr)) {
        LOG_ERROR("Failed to query DXGI interfaces");
        return ErrorCode::DeviceCreationFailed;
    }
    
    // Log adapter information
    DXGI_ADAPTER_DESC adapterDesc = getAdapterDesc();
    std::wstring adapterName(adapterDesc.Description);
    std::string adapterNameStr(adapterName.begin(), adapterName.end());
    
    LOG_INFO("GPU: " + adapterNameStr);
    LOG_INFO("VRAM: " + std::to_string(adapterDesc.DedicatedVideoMemory / (1024 * 1024)) + " MB");
    LOG_INFO("Feature Level: " + std::to_string(featureLevel_ >> 12) + "." + 
             std::to_string((featureLevel_ >> 8) & 0xF));
    
    initialized_ = true;
    LOG_INFO("Direct3D 11 device initialized successfully");
    
    return ErrorCode::Success;
}

HRESULT D3DDevice::createDevice(bool enableDebug) {
    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };
    
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    
    if (enableDebug) {
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }
    
    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Use default adapter
        D3D_DRIVER_TYPE_HARDWARE,   // Hardware acceleration
        nullptr,                    // No software rasterizer
        createDeviceFlags,          // Device flags
        featureLevels,              // Feature levels to try
        ARRAYSIZE(featureLevels),   // Number of feature levels
        D3D11_SDK_VERSION,          // SDK version
        &d3dDevice_,                // Output device
        &featureLevel_,             // Output feature level
        &d3dContext_                // Output context
    );
    
    return hr;
}

HRESULT D3DDevice::queryDXGIInterfaces() {
    // Get DXGI device
    HRESULT hr = d3dDevice_.As(&dxgiDevice_);
    if (FAILED(hr)) {
        return hr;
    }
    
    // Get adapter
    hr = dxgiDevice_->GetAdapter(&adapter_);
    if (FAILED(hr)) {
        return hr;
    }
    
    return S_OK;
}

ComPtr<ID3D11Texture2D> D3DDevice::createStagingTexture(
    int width, 
    int height, 
    DXGI_FORMAT format
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Device not initialized");
        return nullptr;
    }
    
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3dDevice_->CreateTexture2D(&desc, nullptr, &texture);
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create staging texture");
        return nullptr;
    }
    
    return texture;
}

ComPtr<ID3D11Texture2D> D3DDevice::createSharedTexture(
    int width, 
    int height, 
    DXGI_FORMAT format
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        LOG_ERROR("Device not initialized");
        return nullptr;
    }
    
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3dDevice_->CreateTexture2D(&desc, nullptr, &texture);
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to create shared texture");
        return nullptr;
    }
    
    return texture;
}

void D3DDevice::copyTexture(
    ComPtr<ID3D11Texture2D> src, 
    ComPtr<ID3D11Texture2D> dst
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !src || !dst) {
        return;
    }
    
    d3dContext_->CopyResource(dst.Get(), src.Get());
}

void D3DDevice::copyTextureRegion(
    ComPtr<ID3D11Texture2D> src,
    ComPtr<ID3D11Texture2D> dst,
    int srcX, int srcY,
    int width, int height,
    int dstX, int dstY
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !src || !dst) {
        return;
    }
    
    D3D11_BOX srcBox = {};
    srcBox.left = srcX;
    srcBox.top = srcY;
    srcBox.right = srcX + width;
    srcBox.bottom = srcY + height;
    srcBox.front = 0;
    srcBox.back = 1;
    
    d3dContext_->CopySubresourceRegion(
        dst.Get(),
        0,
        dstX, dstY, 0,
        src.Get(),
        0,
        &srcBox
    );
}

D3D11_TEXTURE2D_DESC D3DDevice::getTextureDesc(ComPtr<ID3D11Texture2D> texture) {
    D3D11_TEXTURE2D_DESC desc = {};
    
    if (texture) {
        texture->GetDesc(&desc);
    }
    
    return desc;
}

ComPtr<ID3D11Texture2D> D3DDevice::surfaceToTexture(
    winrt::IDirect3DSurface const& surface
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !surface) {
        return nullptr;
    }
    
    // Get DXGI interface from WinRT surface
    auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
    
    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = access->GetInterface(IID_PPV_ARGS(&texture));
    
    if (FAILED(hr)) {
        LOG_ERROR("Failed to convert WinRT surface to D3D11 texture");
        return nullptr;
    }
    
    return texture;
}

void D3DDevice::flush() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (d3dContext_) {
        d3dContext_->Flush();
    }
}

DXGI_ADAPTER_DESC D3DDevice::getAdapterDesc() const {
    DXGI_ADAPTER_DESC desc = {};
    
    if (adapter_) {
        adapter_->GetDesc(&desc);
    }
    
    return desc;
}

bool D3DDevice::isNvidiaGPU() const {
    DXGI_ADAPTER_DESC desc = getAdapterDesc();
    
    // NVIDIA vendor ID is 0x10DE
    return desc.VendorId == 0x10DE;
}

bool D3DDevice::getVRAMUsage(size_t& availableBytes, size_t& totalBytes) const {
    if (!adapter_) {
        return false;
    }
    
    ComPtr<IDXGIAdapter3> adapter3;
    HRESULT hr = adapter_.As(&adapter3);
    
    if (FAILED(hr)) {
        return false;
    }
    
    DXGI_QUERY_VIDEO_MEMORY_INFO memoryInfo = {};
    hr = adapter3->QueryVideoMemoryInfo(
        0,
        DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
        &memoryInfo
    );
    
    if (FAILED(hr)) {
        return false;
    }
    
    availableBytes = memoryInfo.Budget - memoryInfo.CurrentUsage;
    totalBytes = memoryInfo.Budget;
    
    return true;
}

} // namespace WGC
