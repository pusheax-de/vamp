#pragma once
// DescriptorAllocator.h - CBV/SRV/UAV and RTV/DSV descriptor heap management

#include "EngineTypes.h"
#include <d3d12.h>
#include <wrl/client.h>
#include <mutex>
#include <vector>

namespace engine
{

// ---------------------------------------------------------------------------
// PersistentDescriptorAllocator - allocates from a fixed heap, free-list based
// ---------------------------------------------------------------------------
class PersistentDescriptorAllocator
{
public:
    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
              uint32_t maxDescriptors, bool shaderVisible);
    void Shutdown();

    // Allocate a single descriptor. Returns index into the heap.
    uint32_t Allocate();
    void     Free(uint32_t index);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32_t    m_descriptorSize    = 0;
    uint32_t    m_maxDescriptors    = 0;
    uint32_t    m_nextFree          = 0;
    bool        m_shaderVisible     = false;

    std::vector<uint32_t>   m_freeList;
    std::mutex              m_mutex;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
};

// ---------------------------------------------------------------------------
// LinearDescriptorAllocator - per-frame bump allocator, reset each frame
// ---------------------------------------------------------------------------
class LinearDescriptorAllocator
{
public:
    void Init(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type,
              uint32_t maxDescriptors, bool shaderVisible);
    void Shutdown();
    void Reset();

    // Allocate a contiguous range. Returns start index.
    uint32_t Allocate(uint32_t count = 1);

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

    ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    uint32_t    m_descriptorSize    = 0;
    uint32_t    m_maxDescriptors    = 0;
    uint32_t    m_current           = 0;
    bool        m_shaderVisible     = false;

    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart = {};
};

} // namespace engine
