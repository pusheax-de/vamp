// DescriptorAllocator.cpp - Descriptor heap management implementation

#include "DescriptorAllocator.h"
#include <cassert>

namespace engine
{

// ===========================================================================
// PersistentDescriptorAllocator
// ===========================================================================

void PersistentDescriptorAllocator::Init(ID3D12Device* device,
                                          D3D12_DESCRIPTOR_HEAP_TYPE type,
                                          uint32_t maxDescriptors,
                                          bool shaderVisible)
{
    m_maxDescriptors = maxDescriptors;
    m_shaderVisible  = shaderVisible;
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type           = type;
    desc.NumDescriptors = maxDescriptors;
    desc.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask       = 0;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    assert(SUCCEEDED(hr));

    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible)
        m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();

    m_nextFree = 0;
    m_freeList.clear();
}

void PersistentDescriptorAllocator::Shutdown()
{
    m_heap.Reset();
    m_freeList.clear();
}

uint32_t PersistentDescriptorAllocator::Allocate()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_freeList.empty())
    {
        uint32_t idx = m_freeList.back();
        m_freeList.pop_back();
        return idx;
    }

    assert(m_nextFree < m_maxDescriptors);
    return m_nextFree++;
}

void PersistentDescriptorAllocator::Free(uint32_t index)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_freeList.push_back(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE PersistentDescriptorAllocator::GetCPUHandle(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE PersistentDescriptorAllocator::GetGPUHandle(uint32_t index) const
{
    assert(m_shaderVisible);
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_gpuStart;
    handle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
    return handle;
}

// ===========================================================================
// LinearDescriptorAllocator
// ===========================================================================

void LinearDescriptorAllocator::Init(ID3D12Device* device,
                                      D3D12_DESCRIPTOR_HEAP_TYPE type,
                                      uint32_t maxDescriptors,
                                      bool shaderVisible)
{
    m_maxDescriptors = maxDescriptors;
    m_shaderVisible  = shaderVisible;
    m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);
    m_current        = 0;

    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type           = type;
    desc.NumDescriptors = maxDescriptors;
    desc.Flags          = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
                                         : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask       = 0;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap));
    assert(SUCCEEDED(hr));

    m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();
    if (shaderVisible)
        m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
}

void LinearDescriptorAllocator::Shutdown()
{
    m_heap.Reset();
}

void LinearDescriptorAllocator::Reset()
{
    m_current = 0;
}

uint32_t LinearDescriptorAllocator::Allocate(uint32_t count)
{
    assert(m_current + count <= m_maxDescriptors);
    uint32_t start = m_current;
    m_current += count;
    return start;
}

D3D12_CPU_DESCRIPTOR_HANDLE LinearDescriptorAllocator::GetCPUHandle(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_cpuStart;
    handle.ptr += static_cast<SIZE_T>(index) * m_descriptorSize;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE LinearDescriptorAllocator::GetGPUHandle(uint32_t index) const
{
    assert(m_shaderVisible);
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_gpuStart;
    handle.ptr += static_cast<UINT64>(index) * m_descriptorSize;
    return handle;
}

} // namespace engine
