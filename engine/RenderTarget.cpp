// RenderTarget.cpp - Render target and depth/stencil implementation

#include "RenderTarget.h"
#include <cassert>

namespace engine
{

// ===========================================================================
// RenderTarget
// ===========================================================================

bool RenderTarget::Create(ID3D12Device* device,
                           PersistentDescriptorAllocator& rtvHeap,
                           PersistentDescriptorAllocator& srvHeap,
                           uint32_t width, uint32_t height,
                           DXGI_FORMAT format)
{
    m_width  = width;
    m_height = height;
    m_format = format;
    m_currentState = D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = width;
    texDesc.Height             = height;
    texDesc.DepthOrArraySize   = 1;
    texDesc.MipLevels          = 1;
    texDesc.Format             = format;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format   = format;
    clearVal.Color[0] = 0.0f;
    clearVal.Color[1] = 0.0f;
    clearVal.Color[2] = 0.0f;
    clearVal.Color[3] = 0.0f;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearVal, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    // RTV
    m_rtvIndex = rtvHeap.Allocate();
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format               = format;
    rtvDesc.ViewDimension        = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice   = 0;
    device->CreateRenderTargetView(m_resource.Get(), &rtvDesc,
                                    rtvHeap.GetCPUHandle(m_rtvIndex));

    // SRV
    m_srvIndex = srvHeap.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format                        = format;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels          = 1;
    device->CreateShaderResourceView(m_resource.Get(), &srvDesc,
                                      srvHeap.GetCPUHandle(m_srvIndex));

    return true;
}

void RenderTarget::Shutdown(PersistentDescriptorAllocator& rtvHeap,
                             PersistentDescriptorAllocator& srvHeap)
{
    if (m_rtvIndex != UINT32_MAX) { rtvHeap.Free(m_rtvIndex); m_rtvIndex = UINT32_MAX; }
    if (m_srvIndex != UINT32_MAX) { srvHeap.Free(m_srvIndex); m_srvIndex = UINT32_MAX; }
    m_resource.Reset();
}

void RenderTarget::TransitionTo(ID3D12GraphicsCommandList* cmdList,
                                 D3D12_RESOURCE_STATES newState)
{
    if (m_currentState == newState)
        return;

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = m_resource.Get();
    barrier.Transition.StateBefore = m_currentState;
    barrier.Transition.StateAfter  = newState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    m_currentState = newState;
}

void RenderTarget::Clear(ID3D12GraphicsCommandList* cmdList,
                          PersistentDescriptorAllocator& rtvHeap,
                          const float clearColor[4])
{
    if (m_currentState != D3D12_RESOURCE_STATE_RENDER_TARGET)
        TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdList->ClearRenderTargetView(rtvHeap.GetCPUHandle(m_rtvIndex), clearColor, 0, nullptr);
}

// ===========================================================================
// DepthStencilTarget
// ===========================================================================

bool DepthStencilTarget::Create(ID3D12Device* device,
                                 PersistentDescriptorAllocator& dsvHeap,
                                 uint32_t width, uint32_t height,
                                 DXGI_FORMAT format)
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
    texDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearVal = {};
    clearVal.Format               = format;
    clearVal.DepthStencil.Depth   = 1.0f;
    clearVal.DepthStencil.Stencil = 0;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearVal, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr))
        return false;

    m_dsvIndex = dsvHeap.Allocate();
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format               = format;
    dsvDesc.ViewDimension        = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice   = 0;
    device->CreateDepthStencilView(m_resource.Get(), &dsvDesc,
                                    dsvHeap.GetCPUHandle(m_dsvIndex));

    return true;
}

void DepthStencilTarget::Shutdown(PersistentDescriptorAllocator& dsvHeap)
{
    if (m_dsvIndex != UINT32_MAX) { dsvHeap.Free(m_dsvIndex); m_dsvIndex = UINT32_MAX; }
    m_resource.Reset();
}

void DepthStencilTarget::Clear(ID3D12GraphicsCommandList* cmdList,
                                PersistentDescriptorAllocator& dsvHeap,
                                float depth, uint8_t stencil)
{
    cmdList->ClearDepthStencilView(
        dsvHeap.GetCPUHandle(m_dsvIndex),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        depth, stencil, 0, nullptr);
}

} // namespace engine
