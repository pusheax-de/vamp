// UploadManager.cpp - Ring-buffer upload heap implementation

#include "UploadManager.h"
#include <cassert>
#include <algorithm>

namespace engine
{

void UploadManager::Init(ID3D12Device* device, uint64_t ringSize)
{
    m_ringSize    = ringSize;
    m_writeOffset = 0;
    m_readOffset  = 0;
    m_markerHead  = 0;
    m_markerTail  = 0;
    m_lastCompletedFence = 0;

    // Create upload buffer
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width              = ringSize;
    bufDesc.Height             = 1;
    bufDesc.DepthOrArraySize   = 1;
    bufDesc.MipLevels          = 1;
    bufDesc.SampleDesc.Count   = 1;
    bufDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&m_buffer));
    assert(SUCCEEDED(hr));

    // Map persistently
    D3D12_RANGE readRange = { 0, 0 };
    hr = m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedPtr));
    assert(SUCCEEDED(hr));

    // Create fence for ring tracking
    hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    assert(SUCCEEDED(hr));

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(m_fenceEvent != nullptr);
}

void UploadManager::Shutdown()
{
    if (m_buffer)
    {
        m_buffer->Unmap(0, nullptr);
        m_mappedPtr = nullptr;
    }
    m_buffer.Reset();
    m_fence.Reset();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

void UploadManager::SignalFrame(ID3D12CommandQueue* queue, uint64_t fenceValue)
{
    queue->Signal(m_fence.Get(), fenceValue);

    // Store marker
    uint32_t nextHead = (m_markerHead + 1) % kMaxMarkers;
    assert(nextHead != m_markerTail); // Ring full — increase kMaxMarkers
    m_markers[m_markerHead] = { fenceValue, m_writeOffset };
    m_markerHead = nextHead;
}

void UploadManager::WaitForSpace(uint64_t requiredBytes)
{
    // Retire completed markers to advance read offset
    while (m_markerTail != m_markerHead)
    {
        uint64_t completed = m_fence->GetCompletedValue();
        if (m_markers[m_markerTail].fenceValue <= completed)
        {
            m_readOffset = m_markers[m_markerTail].ringOffset;
            m_lastCompletedFence = m_markers[m_markerTail].fenceValue;
            m_markerTail = (m_markerTail + 1) % kMaxMarkers;
        }
        else
        {
            break;
        }
    }

    // Calculate available space
    uint64_t available;
    if (m_writeOffset >= m_readOffset)
        available = m_ringSize - (m_writeOffset - m_readOffset);
    else
        available = m_readOffset - m_writeOffset;

    if (available >= requiredBytes)
        return;

    // Need to wait for GPU to catch up
    if (m_markerTail != m_markerHead)
    {
        uint64_t waitFence = m_markers[m_markerTail].fenceValue;
        if (m_fence->GetCompletedValue() < waitFence)
        {
            m_fence->SetEventOnCompletion(waitFence, m_fenceEvent);
            WaitForSingleObject(m_fenceEvent, INFINITE);
        }
        m_readOffset = m_markers[m_markerTail].ringOffset;
        m_lastCompletedFence = m_markers[m_markerTail].fenceValue;
        m_markerTail = (m_markerTail + 1) % kMaxMarkers;
    }
}

UploadManager::Allocation UploadManager::Allocate(uint64_t sizeBytes, uint64_t alignment)
{
    // Align write offset
    uint64_t aligned = (m_writeOffset + alignment - 1) & ~(alignment - 1);

    // Handle wrap-around
    if (aligned + sizeBytes > m_ringSize)
    {
        // Wrap to start
        aligned = 0;
        WaitForSpace(sizeBytes);
    }
    else
    {
        uint64_t needed = (aligned - m_writeOffset) + sizeBytes;
        WaitForSpace(needed);
    }

    Allocation alloc;
    alloc.cpuPtr         = m_mappedPtr + aligned;
    alloc.gpuAddress     = m_buffer->GetGPUVirtualAddress() + aligned;
    alloc.offsetInBuffer = aligned;
    alloc.size           = sizeBytes;

    m_writeOffset = aligned + sizeBytes;
    return alloc;
}

UploadManager::Allocation UploadManager::AllocateTextureUpload(uint64_t rowPitch,
                                                                uint32_t numRows)
{
    // D3D12 requires 256-byte aligned row pitch for texture uploads
    uint64_t alignedPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                            & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
    uint64_t totalSize = alignedPitch * numRows;
    return Allocate(totalSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
}

} // namespace engine
