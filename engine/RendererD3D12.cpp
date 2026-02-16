// RendererD3D12.cpp - Core D3D12 renderer implementation

#include "RendererD3D12.h"
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace engine
{

// ===========================================================================
// Init / Shutdown
// ===========================================================================

bool RendererD3D12::Init(HWND hwnd, uint32_t width, uint32_t height)
{
    m_width  = width;
    m_height = height;

    CreateDevice();
    CreateCommandQueue();
    CreateSwapChain(hwnd);

    // Descriptor heaps
    m_srvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                    kMaxPersistentSRVs, true);
    m_rtvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                    64, false);
    m_dsvHeap.Init(m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV,
                    16, false);

    // Upload manager: 64MB ring buffer
    m_uploadMgr.Init(m_device.Get(), 64 * 1024 * 1024);

    // Fence
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
    assert(SUCCEEDED(hr));
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(m_fenceEvent);
    m_fenceValue = 0;

    CreateFrameResources();
    CreateBackBufferRTVs();

    return true;
}

void RendererD3D12::Shutdown()
{
    WaitForGPU();

    ReleaseBackBufferRTVs();

    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        m_frames[i].commandAllocator.Reset();
        m_frames[i].transientSRVHeap.Shutdown();
    }

    m_uploadMgr.Shutdown();
    m_srvHeap.Shutdown();
    m_rtvHeap.Shutdown();
    m_dsvHeap.Shutdown();

    m_commandList.Reset();
    m_swapChain.Reset();
    m_commandQueue.Reset();

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
    m_fence.Reset();
    m_device.Reset();
    m_dxgiFactory.Reset();
}

// ===========================================================================
// Resize
// ===========================================================================

void RendererD3D12::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    WaitForGPU();
    ReleaseBackBufferRTVs();

    HRESULT hr = m_swapChain->ResizeBuffers(
        kSwapChainBufferCount, width, height,
        DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    assert(SUCCEEDED(hr));

    m_width  = width;
    m_height = height;

    CreateBackBufferRTVs();
}

// ===========================================================================
// Frame lifecycle
// ===========================================================================

void RendererD3D12::BeginFrame()
{
    m_backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_frameIndex = m_backBufferIndex % kMaxFramesInFlight;

    auto& frame = m_frames[m_frameIndex];

    // Wait if this frame's GPU work isn't done yet
    if (m_fence->GetCompletedValue() < frame.fenceValue)
    {
        m_fence->SetEventOnCompletion(frame.fenceValue, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    frame.commandAllocator->Reset();
    frame.transientSRVHeap.Reset();
    m_commandList->Reset(frame.commandAllocator.Get(), nullptr);

    // Set the shader-visible descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_srvHeap.GetHeap() };
    m_commandList->SetDescriptorHeaps(1, heaps);
}

void RendererD3D12::EndFrame()
{
    // Present barrier handled by caller (TransitionBackBufferToPresent)
    m_commandList->Close();

    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0); // VSync on

    // Signal fence
    ++m_fenceValue;
    m_frames[m_frameIndex].fenceValue = m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);

    // Also signal upload manager
    m_uploadMgr.SignalFrame(m_commandQueue.Get(), m_fenceValue);
}

void RendererD3D12::WaitForGPU()
{
    ++m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    WaitForSingleObject(m_fenceEvent, INFINITE);
}

// ===========================================================================
// Back buffer helpers
// ===========================================================================

D3D12_CPU_DESCRIPTOR_HANDLE RendererD3D12::GetBackBufferRTV() const
{
    return m_rtvHeap.GetCPUHandle(m_backBufferRTVs[m_backBufferIndex]);
}

void RendererD3D12::TransitionBackBufferToRT()
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_backBuffers[m_backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
}

void RendererD3D12::TransitionBackBufferToPresent()
{
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_backBuffers[m_backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_commandList->ResourceBarrier(1, &barrier);
}

// ===========================================================================
// Internal creation helpers
// ===========================================================================

void RendererD3D12::CreateDevice()
{
#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
            debugController->EnableDebugLayer();
    }
#endif

    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory));
    assert(SUCCEEDED(hr));

    // Try to find a hardware adapter
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0;
         m_dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            continue;

        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                                         IID_PPV_ARGS(&m_device))))
            return;
    }

    // Fallback: WARP
    ComPtr<IDXGIAdapter> warpAdapter;
    m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter));
    hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0,
                            IID_PPV_ARGS(&m_device));
    assert(SUCCEEDED(hr));
}

void RendererD3D12::CreateCommandQueue()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    HRESULT hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    assert(SUCCEEDED(hr));
}

void RendererD3D12::CreateSwapChain(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width       = m_width;
    scDesc.Height      = m_height;
    scDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.SampleDesc  = { 1, 0 };
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = kSwapChainBufferCount;
    scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
        m_commandQueue.Get(), hwnd, &scDesc, nullptr, nullptr, &swapChain1);
    assert(SUCCEEDED(hr));

    hr = swapChain1.As(&m_swapChain);
    assert(SUCCEEDED(hr));

    // Disable Alt+Enter fullscreen
    m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
}

void RendererD3D12::CreateFrameResources()
{
    for (uint32_t i = 0; i < kMaxFramesInFlight; ++i)
    {
        HRESULT hr = m_device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_frames[i].commandAllocator));
        assert(SUCCEEDED(hr));

        m_frames[i].transientSRVHeap.Init(
            m_device.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            kMaxPerFrameSRVs, false); // Not shader-visible; copy into main heap
    }

    // Create the shared command list (uses frame 0's allocator initially)
    HRESULT hr = m_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_frames[0].commandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&m_commandList));
    assert(SUCCEEDED(hr));

    m_commandList->Close();
}

void RendererD3D12::CreateBackBufferRTVs()
{
    for (uint32_t i = 0; i < kSwapChainBufferCount; ++i)
    {
        HRESULT hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i]));
        assert(SUCCEEDED(hr));

        m_backBufferRTVs[i] = m_rtvHeap.Allocate();
        m_device->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr,
                                          m_rtvHeap.GetCPUHandle(m_backBufferRTVs[i]));
    }
}

void RendererD3D12::ReleaseBackBufferRTVs()
{
    for (uint32_t i = 0; i < kSwapChainBufferCount; ++i)
    {
        if (m_backBufferRTVs[i] != 0 || m_backBuffers[i])
        {
            m_rtvHeap.Free(m_backBufferRTVs[i]);
            m_backBufferRTVs[i] = 0;
        }
        m_backBuffers[i].Reset();
    }
}

} // namespace engine
