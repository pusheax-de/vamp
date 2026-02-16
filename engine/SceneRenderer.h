#pragma once
// SceneRenderer.h - Multi-pass scene renderer orchestrating all render passes

#include "EngineTypes.h"
#include "RendererD3D12.h"
#include "RenderTarget.h"
#include "RenderQueue.h"
#include "Camera2D.h"
#include "Grid.h"
#include "BackgroundPager.h"
#include "LightSystem.h"
#include "OccluderSet.h"
#include "RoofSystem.h"
#include "FogRenderer.h"
#include "PipelineStates.h"
#include "Texture2D.h"
#include <vector>

namespace engine
{

// ---------------------------------------------------------------------------
// SceneRendererConfig - configuration for render target sizes
// ---------------------------------------------------------------------------
struct SceneRendererConfig
{
    uint32_t fullResWidth   = 2560;
    uint32_t fullResHeight  = 1440;
    uint32_t halfResWidth   = 1280;     // LightAccum
    uint32_t halfResHeight  = 720;
    uint32_t fogResWidth    = 640;      // Quarter-res fog
    uint32_t fogResHeight   = 360;
    std::wstring shaderDir;             // Path to HLSL shaders

    void SetFromFullRes(uint32_t w, uint32_t h)
    {
        fullResWidth  = w;
        fullResHeight = h;
        halfResWidth  = w / 2;
        halfResHeight = h / 2;
        fogResWidth   = w / 4;
        fogResHeight  = h / 4;
    }
};

// ---------------------------------------------------------------------------
// Per-frame constant buffer data (must match HLSL FrameConstants)
// ---------------------------------------------------------------------------
struct FrameConstants
{
    DirectX::XMFLOAT4X4 viewProjection;
    DirectX::XMFLOAT2   cameraPosition;
    float                cameraZoom;
    float                time;
    DirectX::XMFLOAT2   screenSize;
    DirectX::XMFLOAT2   fogTexelSize;      // 1.0 / fogRes
    float                ambientDarkening;  // 0.0 = fully dark, 1.0 = no darkening
    float                pad[3];
};

// ---------------------------------------------------------------------------
// Per-light constant buffer data (must match HLSL LightConstants)
// ---------------------------------------------------------------------------
struct LightConstants
{
    DirectX::XMFLOAT2 position;
    float              pad0;
    float              pad1;
    DirectX::XMFLOAT3 color;
    float              radius;
    float              intensity;
    float              pad2[3];
};

// ---------------------------------------------------------------------------
// SceneRenderer - orchestrates the multi-pass rendering pipeline
//
// Pass order:
//   0. Stream/update background pages (CPU->GPU)
//   1. Fog update (visibility computation + upload)
//   2. Base scene draw -> SceneColor RT
//   3. Dynamic lighting + shadows -> LightAccum RT
//   4. Composite -> Backbuffer
//   5. Screen-space UI -> Backbuffer
// ---------------------------------------------------------------------------
class SceneRenderer
{
public:
    bool Init(RendererD3D12& renderer, const SceneRendererConfig& config);
    void Shutdown(RendererD3D12& renderer);
    void Resize(RendererD3D12& renderer, uint32_t width, uint32_t height);

    // Execute full render frame
    void RenderFrame(RendererD3D12& renderer,
                     Camera2D& camera,
                     BackgroundPager& bgPager,
                     RenderQueue& renderQueue,
                     LightSystem& lights,
                     OccluderSet& occluders,
                     FogRenderer& fog,
                     RoofSystem& roofs,
                     float time);

    // Access render targets for custom passes
    RenderTarget&       GetSceneColor() { return m_sceneColor; }
    RenderTarget&       GetLightAccum() { return m_lightAccum; }
    DepthStencilTarget& GetDepthStencil() { return m_depthStencil; }
    PipelineStates&     GetPipelineStates() { return m_pso; }

private:
    void UploadFrameConstants(RendererD3D12& renderer);
    void UploadLightConstants(RendererD3D12& renderer, const PointLight2D& light);

    // Individual pass implementations
    void PassBackground(ID3D12GraphicsCommandList* cmdList,
                        RendererD3D12& renderer,
                        Camera2D& camera,
                        BackgroundPager& bgPager);

    void PassBaseScene(ID3D12GraphicsCommandList* cmdList,
                       RendererD3D12& renderer,
                       Camera2D& camera,
                       const RenderQueue& renderQueue);

    void PassLighting(ID3D12GraphicsCommandList* cmdList,
                      RendererD3D12& renderer,
                      Camera2D& camera,
                      LightSystem& lights,
                      OccluderSet& occluders);

    void PassComposite(ID3D12GraphicsCommandList* cmdList,
                       RendererD3D12& renderer,
                       FogRenderer& fog);

    void PassUI(ID3D12GraphicsCommandList* cmdList,
                RendererD3D12& renderer,
                const RenderQueue& renderQueue);

    // Render targets
    RenderTarget        m_sceneColor;       // Full-res RGBA8 SRGB
    RenderTarget        m_lightAccum;       // Half-res R11G11B10_FLOAT
    DepthStencilTarget  m_depthStencil;     // Full-res D24S8 (stencil for shadows)

    // PSOs and shaders
    PipelineStates      m_pso;

    // Per-frame GPU constants (uploaded each frame from upload ring)
    D3D12_GPU_VIRTUAL_ADDRESS m_frameCBVAddress = 0;
    D3D12_GPU_VIRTUAL_ADDRESS m_lightCBVAddress = 0;

    SceneRendererConfig m_config;
    FrameConstants      m_frameConstants;
};

} // namespace engine
