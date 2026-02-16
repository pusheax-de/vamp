#pragma once
// UploadManager.h - Ring-buffer upload heap for CPU?GPU transfers

#include "EngineTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

namespace engine
{

// ---------------------------------------------------------------------------
// UploadManager - persistent ring buffer on an upload heap
// ---------------------------------------------------------------------------
class UploadManager
{
public:
    void Init(ID3D12Device* device, uint64_t ringSize);
    void Shutdown();

    // Record a fence value for the current position so we know when it's safe to reuse
    void SignalFrame(ID3D12CommandQueue* queue, uint64_t fenceValue);

    // Wait until enough space is available in the ring
    void WaitForSpace(uint64_t requiredBytes);

    // Suballocate from the ring. Returns mapped CPU pointer and GPU virtual address.
    struct Allocation
    {
        void*                   cpuPtr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddress;
        uint64_t                offsetInBuffer;
        uint64_t                size;
    };

    Allocation Allocate(uint64_t sizeBytes, uint64_t alignment = 256);

    // Copy texture data: suballocate, fill, return info for CopyTextureRegion
    Allocation AllocateTextureUpload(uint64_t rowPitch, uint32_t numRows);

    ID3D12Resource* GetBuffer() const { return m_buffer.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_buffer;
    Microsoft::WRL::ComPtr<ID3D12Fence>     m_fence;
    HANDLE                                  m_fenceEvent = nullptr;

    uint8_t*    m_mappedPtr     = nullptr;
    uint64_t    m_ringSize      = 0;
    uint64_t    m_writeOffset   = 0;
    uint64_t    m_readOffset    = 0;

    // Frame tracking for ring reuse
    struct FrameMarker
    {
        uint64_t fenceValue;
        uint64_t ringOffset;
    };
    static constexpr uint32_t kMaxMarkers = 16;
    FrameMarker m_markers[kMaxMarkers] = {};
    uint32_t    m_markerHead = 0;
    uint32_t    m_markerTail = 0;
    uint64_t    m_lastCompletedFence = 0;
};

} // namespace engine
