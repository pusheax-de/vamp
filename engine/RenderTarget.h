#pragma once
// RenderTarget.h - Render target wrapper (RTV + optional SRV + optional DSV)

#include "EngineTypes.h"
#include "DescriptorAllocator.h"
#include <d3d12.h>
#include <wrl/client.h>

namespace engine
{

// ---------------------------------------------------------------------------
// RenderTarget - wraps a texture usable as both render target and shader resource
// ---------------------------------------------------------------------------
class RenderTarget
{
public:
    bool Create(ID3D12Device* device,
                PersistentDescriptorAllocator& rtvHeap,
                PersistentDescriptorAllocator& srvHeap,
                uint32_t width, uint32_t height,
                DXGI_FORMAT format);

    void Shutdown(PersistentDescriptorAllocator& rtvHeap,
                  PersistentDescriptorAllocator& srvHeap);

    void TransitionTo(ID3D12GraphicsCommandList* cmdList,
                      D3D12_RESOURCE_STATES newState);

    void Clear(ID3D12GraphicsCommandList* cmdList,
               PersistentDescriptorAllocator& rtvHeap,
               const float clearColor[4]);

    // Accessors
    ID3D12Resource*             GetResource() const { return m_resource.Get(); }
    uint32_t                    GetRTVIndex() const { return m_rtvIndex; }
    uint32_t                    GetSRVIndex() const { return m_srvIndex; }
    uint32_t                    GetWidth() const { return m_width; }
    uint32_t                    GetHeight() const { return m_height; }
    DXGI_FORMAT                 GetFormat() const { return m_format; }
    D3D12_RESOURCE_STATES       GetCurrentState() const { return m_currentState; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_resource;
    uint32_t                m_rtvIndex      = UINT32_MAX;
    uint32_t                m_srvIndex      = UINT32_MAX;
    uint32_t                m_width         = 0;
    uint32_t                m_height        = 0;
    DXGI_FORMAT             m_format        = DXGI_FORMAT_UNKNOWN;
    D3D12_RESOURCE_STATES   m_currentState  = D3D12_RESOURCE_STATE_RENDER_TARGET;
};

// ---------------------------------------------------------------------------
// DepthStencilTarget - wraps a depth/stencil buffer
// ---------------------------------------------------------------------------
class DepthStencilTarget
{
public:
    bool Create(ID3D12Device* device,
                PersistentDescriptorAllocator& dsvHeap,
                uint32_t width, uint32_t height,
                DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT);

    void Shutdown(PersistentDescriptorAllocator& dsvHeap);

    void Clear(ID3D12GraphicsCommandList* cmdList,
               PersistentDescriptorAllocator& dsvHeap,
               float depth = 1.0f, uint8_t stencil = 0);

    // Accessors
    ID3D12Resource*     GetResource() const { return m_resource.Get(); }
    uint32_t            GetDSVIndex() const { return m_dsvIndex; }
    uint32_t            GetWidth() const { return m_width; }
    uint32_t            GetHeight() const { return m_height; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource>  m_resource;
    uint32_t    m_dsvIndex  = UINT32_MAX;
    uint32_t    m_width     = 0;
    uint32_t    m_height    = 0;
    DXGI_FORMAT m_format    = DXGI_FORMAT_UNKNOWN;
};

} // namespace engine
