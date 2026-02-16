#pragma once
// Texture2D.h - GPU texture resource wrapper with DDS loading support

#include "EngineTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <string>

namespace engine
{

// Forward declarations
class PersistentDescriptorAllocator;
class UploadManager;

// ---------------------------------------------------------------------------
// Texture2D - owns a D3D12 texture resource + SRV descriptor
// ---------------------------------------------------------------------------
class Texture2D
{
public:
    Texture2D() = default;
    ~Texture2D() = default;

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;
    Texture2D(Texture2D&& other);
    Texture2D& operator=(Texture2D&& other);

    // Create from DDS file (BC7/BC3/BC1 compressed)
    bool LoadFromDDS(ID3D12Device* device,
                     ID3D12GraphicsCommandList* cmdList,
                     UploadManager& uploadMgr,
                     PersistentDescriptorAllocator& srvHeap,
                     const std::wstring& filePath);

    // Create from raw RGBA8 data (for runtime-generated textures like fog masks)
    bool CreateFromRGBA(ID3D12Device* device,
                        ID3D12GraphicsCommandList* cmdList,
                        UploadManager& uploadMgr,
                        PersistentDescriptorAllocator& srvHeap,
                        uint32_t width, uint32_t height,
                        const void* pixelData);

    // Create an empty texture (for render-to-texture or dynamic updates)
    bool CreateEmpty(ID3D12Device* device,
                     PersistentDescriptorAllocator& srvHeap,
                     uint32_t width, uint32_t height,
                     DXGI_FORMAT format,
                     D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    // Update a subregion of the texture (for streaming page updates)
    void UpdateRegion(ID3D12GraphicsCommandList* cmdList,
                      UploadManager& uploadMgr,
                      uint32_t dstX, uint32_t dstY,
                      uint32_t width, uint32_t height,
                      const void* data, uint32_t rowPitch);

    void Shutdown(PersistentDescriptorAllocator& srvHeap);

    // Accessors
    ID3D12Resource*             GetResource() const { return m_resource.Get(); }
    uint32_t                    GetSRVIndex() const { return m_srvIndex; }
    uint32_t                    GetWidth() const { return m_width; }
    uint32_t                    GetHeight() const { return m_height; }
    DXGI_FORMAT                 GetFormat() const { return m_format; }
    bool                        IsValid() const { return m_resource != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_resource;
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_uploadBuffer; // Temporary, kept alive until upload completes
    uint32_t        m_srvIndex  = UINT32_MAX;
    uint32_t        m_width     = 0;
    uint32_t        m_height    = 0;
    DXGI_FORMAT     m_format    = DXGI_FORMAT_UNKNOWN;
};

} // namespace engine
