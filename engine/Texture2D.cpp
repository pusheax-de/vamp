// Texture2D.cpp - GPU texture resource implementation

#include "Texture2D.h"
#include "DescriptorAllocator.h"
#include "UploadManager.h"
#include <cassert>
#include <fstream>
#include <vector>

namespace engine
{

// ---------------------------------------------------------------------------
// DDS file structures (minimal, for loading BC7/BC3/BC1)
// ---------------------------------------------------------------------------
namespace
{

#pragma pack(push, 1)
struct DDSPixelFormat
{
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t rgbBitCount;
    uint32_t rBitMask;
    uint32_t gBitMask;
    uint32_t bBitMask;
    uint32_t aBitMask;
};

struct DDSHeader
{
    uint32_t        size;
    uint32_t        flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth;
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDSPixelFormat  ddspf;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
};

struct DDSHeaderDXT10
{
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

static constexpr uint32_t kDDSMagic = 0x20534444; // "DDS "
static constexpr uint32_t kFourCC_DX10 = 0x30315844; // "DX10"
static constexpr uint32_t kFourCC_DXT1 = 0x31545844; // "DXT1"
static constexpr uint32_t kFourCC_DXT5 = 0x35545844; // "DXT5"

DXGI_FORMAT GetFormatFromDDS(const DDSHeader& header, const DDSHeaderDXT10* dx10)
{
    if (dx10)
        return static_cast<DXGI_FORMAT>(dx10->dxgiFormat);

    uint32_t fourCC = header.ddspf.fourCC;
    if (fourCC == kFourCC_DXT1) return DXGI_FORMAT_BC1_UNORM;
    if (fourCC == kFourCC_DXT5) return DXGI_FORMAT_BC3_UNORM;

    // Uncompressed RGBA8
    if (header.ddspf.rgbBitCount == 32)
        return DXGI_FORMAT_R8G8B8A8_UNORM;

    return DXGI_FORMAT_UNKNOWN;
}

uint32_t GetBlockSize(DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
        return 8;
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        return 16;
    default:
        return 0; // Not block-compressed
    }
}

bool IsBlockCompressed(DXGI_FORMAT format)
{
    return GetBlockSize(format) > 0;
}

uint64_t CalcSubresourceSize(uint32_t width, uint32_t height, DXGI_FORMAT format,
                              uint32_t& outRowPitch, uint32_t& outNumRows)
{
    if (IsBlockCompressed(format))
    {
        uint32_t blockSize = GetBlockSize(format);
        uint32_t blocksWide = (width + 3) / 4;
        uint32_t blocksHigh = (height + 3) / 4;
        outRowPitch = blocksWide * blockSize;
        outNumRows  = blocksHigh;
    }
    else
    {
        // Assume 4 bytes per pixel (RGBA8)
        outRowPitch = width * 4;
        outNumRows  = height;
    }
    return static_cast<uint64_t>(outRowPitch) * outNumRows;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------
Texture2D::Texture2D(Texture2D&& other)
    : m_resource(std::move(other.m_resource))
    , m_uploadBuffer(std::move(other.m_uploadBuffer))
    , m_srvIndex(other.m_srvIndex)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_format(other.m_format)
{
    other.m_srvIndex = UINT32_MAX;
    other.m_width    = 0;
    other.m_height   = 0;
    other.m_format   = DXGI_FORMAT_UNKNOWN;
}

Texture2D& Texture2D::operator=(Texture2D&& other)
{
    if (this != &other)
    {
        m_resource     = std::move(other.m_resource);
        m_uploadBuffer = std::move(other.m_uploadBuffer);
        m_srvIndex     = other.m_srvIndex;
        m_width        = other.m_width;
        m_height       = other.m_height;
        m_format       = other.m_format;

        other.m_srvIndex = UINT32_MAX;
        other.m_width    = 0;
        other.m_height   = 0;
        other.m_format   = DXGI_FORMAT_UNKNOWN;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// LoadFromDDS
// ---------------------------------------------------------------------------
bool Texture2D::LoadFromDDS(ID3D12Device* device,
                             ID3D12GraphicsCommandList* cmdList,
                             UploadManager& uploadMgr,
                             PersistentDescriptorAllocator& srvHeap,
                             const std::wstring& filePath)
{
    // Read entire file
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return false;

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0);

    std::vector<uint8_t> fileData(fileSize);
    file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    file.close();

    if (fileSize < sizeof(uint32_t) + sizeof(DDSHeader))
        return false;

    const uint8_t* ptr = fileData.data();

    // Verify magic
    uint32_t magic = *reinterpret_cast<const uint32_t*>(ptr);
    if (magic != kDDSMagic)
        return false;
    ptr += sizeof(uint32_t);

    const DDSHeader* header = reinterpret_cast<const DDSHeader*>(ptr);
    ptr += sizeof(DDSHeader);

    const DDSHeaderDXT10* dx10 = nullptr;
    if (header->ddspf.fourCC == kFourCC_DX10)
    {
        dx10 = reinterpret_cast<const DDSHeaderDXT10*>(ptr);
        ptr += sizeof(DDSHeaderDXT10);
    }

    m_format = GetFormatFromDDS(*header, dx10);
    if (m_format == DXGI_FORMAT_UNKNOWN)
        return false;

    m_width  = header->width;
    m_height = header->height;

    // Create GPU texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = m_width;
    texDesc.Height             = m_height;
    texDesc.DepthOrArraySize   = 1;
    texDesc.MipLevels          = 1; // Only load top mip for now
    texDesc.Format             = m_format;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    // Calculate upload size
    uint32_t rowPitch, numRows;
    uint64_t dataSize = CalcSubresourceSize(m_width, m_height, m_format, rowPitch, numRows);

    // Allocate from upload ring
    auto upload = uploadMgr.AllocateTextureUpload(rowPitch, numRows);

    // Copy pixel data with proper row pitch alignment
    uint64_t alignedRowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                                & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const uint8_t* srcData = ptr;
    uint8_t* dstData = static_cast<uint8_t*>(upload.cpuPtr);
    for (uint32_t row = 0; row < numRows; ++row)
    {
        memcpy(dstData + row * alignedRowPitch, srcData + row * rowPitch, rowPitch);
    }

    // Issue copy command
    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource        = m_resource.Get();
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = uploadMgr.GetBuffer();
    srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset             = upload.offsetInBuffer;
    srcLoc.PlacedFootprint.Footprint.Format   = m_format;
    srcLoc.PlacedFootprint.Footprint.Width    = m_width;
    srcLoc.PlacedFootprint.Footprint.Height   = m_height;
    srcLoc.PlacedFootprint.Footprint.Depth    = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(alignedRowPitch);

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // Transition to shader resource
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Create SRV
    m_srvIndex = srvHeap.Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                        = m_format;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels          = 1;
    srvDesc.Texture2D.MostDetailedMip    = 0;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc,
                                      srvHeap.GetCPUHandle(m_srvIndex));

    return true;
}

// ---------------------------------------------------------------------------
// CreateFromRGBA
// ---------------------------------------------------------------------------
bool Texture2D::CreateFromRGBA(ID3D12Device* device,
                                ID3D12GraphicsCommandList* cmdList,
                                UploadManager& uploadMgr,
                                PersistentDescriptorAllocator& srvHeap,
                                uint32_t width, uint32_t height,
                                const void* pixelData)
{
    m_width  = width;
    m_height = height;
    m_format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = width;
    texDesc.Height             = height;
    texDesc.DepthOrArraySize   = 1;
    texDesc.MipLevels          = 1;
    texDesc.Format             = m_format;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    uint32_t rowPitch = width * 4;
    auto upload = uploadMgr.AllocateTextureUpload(rowPitch, height);

    uint64_t alignedRowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                                & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const uint8_t* src = static_cast<const uint8_t*>(pixelData);
    uint8_t* dst = static_cast<uint8_t*>(upload.cpuPtr);
    for (uint32_t row = 0; row < height; ++row)
    {
        memcpy(dst + row * alignedRowPitch, src + row * rowPitch, rowPitch);
    }

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource        = m_resource.Get();
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = uploadMgr.GetBuffer();
    srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset             = upload.offsetInBuffer;
    srcLoc.PlacedFootprint.Footprint.Format   = m_format;
    srcLoc.PlacedFootprint.Footprint.Width    = width;
    srcLoc.PlacedFootprint.Footprint.Height   = height;
    srcLoc.PlacedFootprint.Footprint.Depth    = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(alignedRowPitch);

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    m_srvIndex = srvHeap.Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                        = m_format;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels          = 1;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc,
                                      srvHeap.GetCPUHandle(m_srvIndex));

    return true;
}

// ---------------------------------------------------------------------------
// CreateEmpty
// ---------------------------------------------------------------------------
bool Texture2D::CreateEmpty(ID3D12Device* device,
                             PersistentDescriptorAllocator& srvHeap,
                             uint32_t width, uint32_t height,
                             DXGI_FORMAT format,
                             D3D12_RESOURCE_FLAGS flags)
{
    m_width  = width;
    m_height = height;
    m_format = format;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = width;
    texDesc.Height             = height;
    texDesc.DepthOrArraySize   = 1;
    texDesc.MipLevels          = 1;
    texDesc.Format             = format;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags              = flags;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
        initialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, initialState,
        nullptr, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    // Create SRV (unless it's a UAV-only resource)
    if (!(flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    {
        m_srvIndex = srvHeap.Allocate();

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                        = format;
        srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels          = 1;

        device->CreateShaderResourceView(m_resource.Get(), &srvDesc,
                                          srvHeap.GetCPUHandle(m_srvIndex));
    }

    return true;
}

// ---------------------------------------------------------------------------
// UpdateRegion
// ---------------------------------------------------------------------------
void Texture2D::UpdateRegion(ID3D12GraphicsCommandList* cmdList,
                              UploadManager& uploadMgr,
                              uint32_t dstX, uint32_t dstY,
                              uint32_t width, uint32_t height,
                              const void* data, uint32_t rowPitch)
{
    assert(m_resource);

    auto upload = uploadMgr.AllocateTextureUpload(rowPitch, height);

    uint64_t alignedRowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                                & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    const uint8_t* src = static_cast<const uint8_t*>(data);
    uint8_t* dst = static_cast<uint8_t*>(upload.cpuPtr);
    for (uint32_t row = 0; row < height; ++row)
    {
        memcpy(dst + row * alignedRowPitch, src + row * rowPitch, rowPitch);
    }

    // Transition to copy dest
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource        = m_resource.Get();
    dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = uploadMgr.GetBuffer();
    srcLoc.Type      = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset             = upload.offsetInBuffer;
    srcLoc.PlacedFootprint.Footprint.Format   = m_format;
    srcLoc.PlacedFootprint.Footprint.Width    = width;
    srcLoc.PlacedFootprint.Footprint.Height   = height;
    srcLoc.PlacedFootprint.Footprint.Depth    = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(alignedRowPitch);

    D3D12_BOX srcBox = { 0, 0, 0, width, height, 1 };
    cmdList->CopyTextureRegion(&dstLoc, dstX, dstY, 0, &srcLoc, &srcBox);

    // Transition back
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

// ---------------------------------------------------------------------------
// Shutdown
// ---------------------------------------------------------------------------
void Texture2D::Shutdown(PersistentDescriptorAllocator& srvHeap)
{
    if (m_srvIndex != UINT32_MAX)
    {
        srvHeap.Free(m_srvIndex);
        m_srvIndex = UINT32_MAX;
    }
    m_resource.Reset();
    m_uploadBuffer.Reset();
}

} // namespace engine
