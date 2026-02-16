#pragma once
// RendererD3D12.h - Core D3D12 renderer: device, swap chain, frame contexts, command lists

#include "EngineTypes.h"
#include "DescriptorAllocator.h"
#include "UploadManager.h"
#include "RenderTarget.h"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <cstdint>

namespace engine
{

using Microsoft::WRL::ComPtr;

// ---------------------------------------------------------------------------
// Per-frame resources (double-buffered)
// ---------------------------------------------------------------------------
struct FrameContext
{
    ComPtr<ID3D12CommandAllocator>      commandAllocator;
    LinearDescriptorAllocator           transientSRVHeap;   // Per-frame linear SRV
    uint64_t                            fenceValue = 0;
};

// ---------------------------------------------------------------------------
// RendererD3D12 - the core rendering device and swap chain
// ---------------------------------------------------------------------------
class RendererD3D12
{
public:
    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    // Resize swap chain (call on WM_SIZE)
    void Resize(uint32_t width, uint32_t height);

    // Frame lifecycle
    void BeginFrame();
    ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }
    void EndFrame();

    // Wait for all GPU work to complete
    void WaitForGPU();

    // Access to core objects
    ID3D12Device*               GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue*         GetCommandQueue() const { return m_commandQueue.Get(); }
    FrameContext&               GetCurrentFrame() { return m_frames[m_frameIndex]; }

    // Descriptor heaps
    PersistentDescriptorAllocator& GetSRVHeap() { return m_srvHeap; }
    PersistentDescriptorAllocator& GetRTVHeap() { return m_rtvHeap; }
    PersistentDescriptorAllocator& GetDSVHeap() { return m_dsvHeap; }

    // Upload manager
    UploadManager& GetUploadManager() { return m_uploadMgr; }

    // Swap chain info
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    uint32_t GetFrameIndex() const { return m_frameIndex; }
    uint32_t GetBackBufferIndex() const { return m_backBufferIndex; }

    // Get current back buffer RTV handle
    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTV() const;

    // Transition back buffer for presentation
    void TransitionBackBufferToRT();
    void TransitionBackBufferToPresent();

private:
    void CreateDevice();
    void CreateCommandQueue();
    void CreateSwapChain(HWND hwnd);
    void CreateFrameResources();
    void CreateBackBufferRTVs();
    void ReleaseBackBufferRTVs();

    // Device
    ComPtr<ID3D12Device>            m_device;
    ComPtr<IDXGIFactory4>           m_dxgiFactory;
    ComPtr<IDXGISwapChain3>         m_swapChain;
    ComPtr<ID3D12CommandQueue>      m_commandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;

    // Frame resources
    FrameContext    m_frames[kMaxFramesInFlight];
    uint32_t        m_frameIndex        = 0;
    uint32_t        m_backBufferIndex   = 0;

    // Fence for CPU/GPU sync
    ComPtr<ID3D12Fence>     m_fence;
    HANDLE                  m_fenceEvent    = nullptr;
    uint64_t                m_fenceValue    = 0;

    // Back buffers
    static constexpr uint32_t kSwapChainBufferCount = 2;
    ComPtr<ID3D12Resource>  m_backBuffers[kSwapChainBufferCount];
    uint32_t                m_backBufferRTVs[kSwapChainBufferCount] = {};

    // Descriptor heaps
    PersistentDescriptorAllocator m_srvHeap;    // Shader-visible SRV/CBV/UAV
    PersistentDescriptorAllocator m_rtvHeap;    // RTV
    PersistentDescriptorAllocator m_dsvHeap;    // DSV

    // Upload manager
    UploadManager m_uploadMgr;

    // Dimensions
    uint32_t m_width    = 0;
    uint32_t m_height   = 0;
};

} // namespace engine
