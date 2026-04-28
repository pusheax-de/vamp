// SceneRenderer.cpp - Multi-pass scene renderer implementation

#include "SceneRenderer.h"
#include <cassert>
#include <cstring>

namespace engine
{

namespace
{
    static float DepthForLayer(RenderLayer layer)
    {
        switch (layer)
        {
        case RenderLayer::BackgroundPages:   return 0.95f;
        case RenderLayer::TileColorFill:     return 0.92f;
        case RenderLayer::GridLines:         return 0.88f;
        case RenderLayer::GroundTiles:       return 0.80f;
        case RenderLayer::WallsProps:        return 0.60f;
        case RenderLayer::Actors:            return 0.45f;
        case RenderLayer::Roofs:             return 0.30f;
        case RenderLayer::RoofActors:        return 0.20f;
        case RenderLayer::ScreenSpaceIcons:  return 0.0f;
        case RenderLayer::FogOverlay:        return 0.10f;
        default:                             return 0.50f;
        }
    }

    static void BindSpritePipeline(ID3D12GraphicsCommandList* cmdList,
                                   RendererD3D12& renderer,
                                   PipelineStates& pso,
                                   D3D12_GPU_VIRTUAL_ADDRESS frameCBVAddress,
                                   ID3D12PipelineState* pipelineState)
    {
        cmdList->SetGraphicsRootSignature(pso.GetMainRootSig());
        cmdList->SetPipelineState(pipelineState);
        cmdList->SetGraphicsRootConstantBufferView(0, frameCBVAddress);
        cmdList->SetGraphicsRootDescriptorTable(1, renderer.GetSRVHeap().GetGPUHandle(0));
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    }

    static void DrawSpriteInstances(ID3D12GraphicsCommandList* cmdList,
                                    RendererD3D12& renderer,
                                    const std::vector<SpriteInstance>& instances)
    {
        if (instances.empty())
            return;

        auto alloc = renderer.GetUploadManager().Allocate(
            instances.size() * sizeof(SpriteInstance), 16);
        std::memcpy(alloc.cpuPtr, instances.data(),
                    instances.size() * sizeof(SpriteInstance));

        D3D12_VERTEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = alloc.gpuAddress;
        ibv.SizeInBytes    = static_cast<UINT>(instances.size() * sizeof(SpriteInstance));
        ibv.StrideInBytes  = sizeof(SpriteInstance);
        cmdList->IASetVertexBuffers(1, 1, &ibv);

        cmdList->DrawInstanced(4, static_cast<UINT>(instances.size()), 0, 0);
    }
}

// ===========================================================================
// Init / Shutdown / Resize
// ===========================================================================

bool SceneRenderer::Init(RendererD3D12& renderer, const SceneRendererConfig& config)
{
    m_config = config;

    auto* device = renderer.GetDevice();
    auto& rtvHeap = renderer.GetRTVHeap();
    auto& srvHeap = renderer.GetSRVHeap();
    auto& dsvHeap = renderer.GetDSVHeap();

    // Reserve 4 contiguous SRV slots for the composite pass
    // (SceneColor, LightAccum, FogVisible, FogExplored)
    m_compositeSRVBase = srvHeap.Allocate();
    srvHeap.Allocate(); // slot +1
    srvHeap.Allocate(); // slot +2
    srvHeap.Allocate(); // slot +3

    if (!m_sceneColor.Create(device, rtvHeap, srvHeap,
                              config.fullResWidth, config.fullResHeight,
                              DXGI_FORMAT_R8G8B8A8_UNORM_SRGB))
        return false;

    if (!m_lightAccum.Create(device, rtvHeap, srvHeap,
                              config.halfResWidth, config.halfResHeight,
                              DXGI_FORMAT_R11G11B10_FLOAT))
        return false;

    if (!m_depthStencil.Create(device, dsvHeap,
                                config.fullResWidth, config.fullResHeight,
                                DXGI_FORMAT_D24_UNORM_S8_UINT))
        return false;

    if (!m_pso.Init(device, config.shaderDir))
        return false;

    return true;
}

void SceneRenderer::Shutdown(RendererD3D12& renderer)
{
    renderer.WaitForGPU();
    m_pso.Shutdown();
    m_sceneColor.Shutdown(renderer.GetRTVHeap(), renderer.GetSRVHeap());
    m_lightAccum.Shutdown(renderer.GetRTVHeap(), renderer.GetSRVHeap());
    m_depthStencil.Shutdown(renderer.GetDSVHeap());

    // Free the 4 composite SRV slots
    if (m_compositeSRVBase != UINT32_MAX)
    {
        auto& srvHeap = renderer.GetSRVHeap();
        srvHeap.Free(m_compositeSRVBase);
        srvHeap.Free(m_compositeSRVBase + 1);
        srvHeap.Free(m_compositeSRVBase + 2);
        srvHeap.Free(m_compositeSRVBase + 3);
        m_compositeSRVBase = UINT32_MAX;
    }
}

void SceneRenderer::Resize(RendererD3D12& renderer, uint32_t width, uint32_t height)
{
    Shutdown(renderer);
    m_config.SetFromFullRes(width, height);
    Init(renderer, m_config);
}

// ===========================================================================
// Constant buffer upload helpers
// ===========================================================================

void SceneRenderer::UploadFrameConstants(RendererD3D12& renderer)
{
    auto alloc = renderer.GetUploadManager().Allocate(
        sizeof(FrameConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    std::memcpy(alloc.cpuPtr, &m_frameConstants, sizeof(FrameConstants));
    m_frameCBVAddress = alloc.gpuAddress;
}

void SceneRenderer::UploadLightConstants(RendererD3D12& renderer,
                                          const PointLight2D& light)
{
    LightConstants lc;
    lc.position  = light.position;
    lc.pad0      = 0.0f;
    lc.pad1      = 0.0f;
    lc.color     = light.color;
    lc.radius    = light.radius;
    lc.intensity = light.intensity;
    lc.pad2[0] = lc.pad2[1] = lc.pad2[2] = 0.0f;

    auto alloc = renderer.GetUploadManager().Allocate(
        sizeof(LightConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    std::memcpy(alloc.cpuPtr, &lc, sizeof(LightConstants));
    m_lightCBVAddress = alloc.gpuAddress;
}

// ===========================================================================
// RenderFrame
// ===========================================================================

void SceneRenderer::RenderFrame(RendererD3D12& renderer,
                                 Camera2D& camera,
                                 BackgroundPager& bgPager,
                                 RenderQueue& renderQueue,
                                 LightSystem& lights,
                                 OccluderSet& occluders,
                                 FogRenderer& fog,
                                 RoofSystem& roofs,
                                 float time,
                                 const Grid* underlayGrid,
                                 const DirectX::XMFLOAT4* underlayGridColor)
{
    auto* cmdList = renderer.GetCommandList();
    auto* device  = renderer.GetDevice();

    (void)roofs; // Roof alpha used when submitting to RenderQueue by caller

    // Build frame constants
    m_frameConstants.viewProjection = camera.GetViewProjection();
    m_frameConstants.cameraPosition = { camera.GetWorldX(), camera.GetWorldY() };
    m_frameConstants.cameraZoom     = camera.GetZoom();
    m_frameConstants.time           = time;
    m_frameConstants.screenSize     = { static_cast<float>(m_config.fullResWidth),
                                         static_cast<float>(m_config.fullResHeight) };
    m_frameConstants.fogTexelSize   = { 1.0f / m_config.fogResWidth,
                                         1.0f / m_config.fogResHeight };
    m_frameConstants.ambientDarkening = 0.15f;
    m_frameConstants.fogColor = { 36.0f / 255.0f, 36.0f / 255.0f, 36.0f / 255.0f };
    m_frameConstants.fogWorldOrigin = { fog.GetWorldOriginX(), fog.GetWorldOriginY() };
    m_frameConstants.fogWorldSize   = { fog.GetWorldWidth(), fog.GetWorldHeight() };

    UploadFrameConstants(renderer);
    renderQueue.Sort();

    // --- Pass 0: Background page streaming ---
    bgPager.ProcessPendingLoads(device, cmdList,
                                 renderer.GetUploadManager(),
                                 renderer.GetSRVHeap());

    // --- Pass 1: Fog update ---
    if (fog.NeedsUpload())
    {
        fog.UploadToGPU(device, cmdList,
                         renderer.GetUploadManager(),
                         renderer.GetSRVHeap());
    }

    // --- Pass 2: Base scene draw -> SceneColor ---
    {
        // Ensure render targets are in RT state
        m_sceneColor.TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_depthStencil.Clear(cmdList, renderer.GetDSVHeap());

        const float clearColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_sceneColor.Clear(cmdList, renderer.GetRTVHeap(), clearColor);

        PassBackground(cmdList, renderer, camera, bgPager);
        PassBaseScene(cmdList, renderer, camera, renderQueue, underlayGrid, underlayGridColor);
    }

    // --- Pass 3: Dynamic lighting + shadows -> LightAccum ---
    {
        m_lightAccum.TransitionTo(cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);

        const float clearBlack[] = { 0.0f, 0.0f, 0.0f, 0.0f };
        m_lightAccum.Clear(cmdList, renderer.GetRTVHeap(), clearBlack);
        m_depthStencil.Clear(cmdList, renderer.GetDSVHeap(), 1.0f, 0);

        PassLighting(cmdList, renderer, camera, lights, occluders);
    }

    // --- Pass 4: Composite -> Backbuffer ---
    {
        renderer.TransitionBackBufferToRT();
        m_sceneColor.TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_lightAccum.TransitionTo(cmdList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

        PassComposite(cmdList, renderer, fog);
    }

    // --- Pass 5: Screen-space UI ---
    PassUI(cmdList, renderer, renderQueue);

    renderer.TransitionBackBufferToPresent();
}

// ===========================================================================
// Pass: Background pages
// ===========================================================================

void SceneRenderer::PassBackground(ID3D12GraphicsCommandList* cmdList,
                                    RendererD3D12& renderer,
                                    Camera2D& camera,
                                    BackgroundPager& bgPager)
{
    // Collect background image + paged background pages
    BackgroundPager::BackgroundImage bgImage;
    bool hasBgImage = bgPager.GetBackgroundImage(bgImage);
    auto pages = bgPager.GetVisiblePages();

    if (!hasBgImage && pages.empty())
        return;

    // Set render target
    auto rtvHandle = renderer.GetRTVHeap().GetCPUHandle(m_sceneColor.GetRTVIndex());
    auto dsvHandle = renderer.GetDSVHeap().GetCPUHandle(m_depthStencil.GetDSVIndex());
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Viewport and scissor for full-res SceneColor
    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(m_config.fullResWidth),
                           static_cast<float>(m_config.fullResHeight),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(m_config.fullResWidth),
                            static_cast<LONG>(m_config.fullResHeight) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Bind sprite PSO
    cmdList->SetGraphicsRootSignature(m_pso.GetMainRootSig());
    cmdList->SetPipelineState(m_pso.GetSpritePSO());
    cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(0));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Build instance data: background image first, then pages
    std::vector<SpriteInstance> instances;
    instances.reserve((hasBgImage ? 1 : 0) + pages.size());

    if (hasBgImage)
    {
        SpriteInstance inst;
        inst.position = { bgImage.worldX + bgImage.worldW * 0.5f,
                          bgImage.worldY + bgImage.worldH * 0.5f };
        inst.size     = { bgImage.worldW, bgImage.worldH };
        inst.uvRect   = { 0.0f, 0.0f, 1.0f, 1.0f };
        inst.color    = { 1.0f, 1.0f, 1.0f, 1.0f };
        inst.rotation = 0.0f;
        inst.sortY    = 0.0f;
        inst.textureIndex = bgImage.srvIndex;
        inst.depthZ   = DepthForLayer(RenderLayer::BackgroundPages);
        instances.push_back(inst);
    }

    for (const auto& page : pages)
    {
        SpriteInstance inst;
        inst.position = { page.worldX + page.worldW * 0.5f,
                          page.worldY + page.worldH * 0.5f };
        inst.size     = { page.worldW, page.worldH };
        inst.uvRect   = { 0.0f, 0.0f, 1.0f, 1.0f };
        inst.color    = { 1.0f, 1.0f, 1.0f, 1.0f };
        inst.rotation = 0.0f;
        inst.sortY    = 0.0f;
        inst.textureIndex = page.srvIndex;
        inst.depthZ   = DepthForLayer(RenderLayer::BackgroundPages);
        instances.push_back(inst);
    }

    // Upload instance buffer
    auto alloc = renderer.GetUploadManager().Allocate(
        instances.size() * sizeof(SpriteInstance), 16);
    std::memcpy(alloc.cpuPtr, instances.data(),
                instances.size() * sizeof(SpriteInstance));

    D3D12_VERTEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = alloc.gpuAddress;
    ibv.SizeInBytes    = static_cast<UINT>(instances.size() * sizeof(SpriteInstance));
    ibv.StrideInBytes  = sizeof(SpriteInstance);
    cmdList->IASetVertexBuffers(1, 1, &ibv); // Slot 1 = instance data

    cmdList->DrawInstanced(4, static_cast<UINT>(instances.size()), 0, 0);
}

// ===========================================================================
// Pass: Base scene (tiles, actors, roofs)
// ===========================================================================

void SceneRenderer::PassBaseScene(ID3D12GraphicsCommandList* cmdList,
                                   RendererD3D12& renderer,
                                   Camera2D& camera,
                                   const RenderQueue& renderQueue,
                                   const Grid* underlayGrid,
                                   const DirectX::XMFLOAT4* underlayGridColor)
{
    // Collect items from GroundTiles through RoofActors
    auto fillRange   = renderQueue.GetLayerRange(RenderLayer::TileColorFill);
    auto groundRange  = renderQueue.GetLayerRange(RenderLayer::GroundTiles);
    auto wallRange    = renderQueue.GetLayerRange(RenderLayer::WallsProps);
    auto actorRange   = renderQueue.GetLayerRange(RenderLayer::Actors);
    auto roofRange    = renderQueue.GetLayerRange(RenderLayer::Roofs);
    auto roofActRange = renderQueue.GetLayerRange(RenderLayer::RoofActors);

    size_t totalItems = (fillRange.end - fillRange.begin)
                      + (groundRange.end - groundRange.begin)
                      + (wallRange.end - wallRange.begin)
                      + (actorRange.end - actorRange.begin)
                      + (roofRange.end - roofRange.begin)
                      + (roofActRange.end - roofActRange.begin);

    if (totalItems == 0 && (underlayGrid == nullptr || underlayGridColor == nullptr))
        return;

    // Set render target, viewport, and scissor
    auto rtvHandle = renderer.GetRTVHeap().GetCPUHandle(m_sceneColor.GetRTVIndex());
    auto dsvHandle = renderer.GetDSVHeap().GetCPUHandle(m_depthStencil.GetDSVIndex());
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(m_config.fullResWidth),
                           static_cast<float>(m_config.fullResHeight),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(m_config.fullResWidth),
                            static_cast<LONG>(m_config.fullResHeight) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Build instance buffers by blend mode. TileColorFill, walls/props, ground
    // and overlays each go through the most appropriate sprite PSO.
    std::vector<SpriteInstance> fillInstances;
    std::vector<SpriteInstance> groundInstances;
    std::vector<SpriteInstance> cutoutInstances;
    std::vector<SpriteInstance> overlayInstances;
    fillInstances.reserve(fillRange.end - fillRange.begin);
    groundInstances.reserve(groundRange.end - groundRange.begin);
    cutoutInstances.reserve(wallRange.end - wallRange.begin);
    overlayInstances.reserve((actorRange.end - actorRange.begin)
                           + (roofRange.end - roofRange.begin)
                           + (roofActRange.end - roofActRange.begin));

    const auto& items = renderQueue.GetItems();
    for (size_t i = fillRange.begin; i < fillRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::TileColorFill);
        fillInstances.push_back(inst);
    }
    for (size_t i = groundRange.begin; i < groundRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::GroundTiles);
        groundInstances.push_back(inst);
    }
    for (size_t i = wallRange.begin; i < wallRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::WallsProps);
        cutoutInstances.push_back(inst);
    }
    for (size_t i = actorRange.begin; i < actorRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::Actors);
        overlayInstances.push_back(inst);
    }
    for (size_t i = roofRange.begin; i < roofRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::Roofs);
        overlayInstances.push_back(inst);
    }
    for (size_t i = roofActRange.begin; i < roofActRange.end; ++i)
    {
        SpriteInstance inst = items[i].instance;
        if (inst.depthZ <= 0.0f)
            inst.depthZ = DepthForLayer(RenderLayer::RoofActors);
        overlayInstances.push_back(inst);
    }

    // 1) Tile color fills: opaque hex sprites under everything else.
    if (!fillInstances.empty())
    {
        BindSpritePipeline(cmdList, renderer, m_pso, m_frameCBVAddress, m_pso.GetSpriteCutoutPSO());
        DrawSpriteInstances(cmdList, renderer, fillInstances);
    }

    // 2) Grid lines: drawn between fills and ground textures so that walls,
    //    actors and roofs all naturally cover them.
    if (underlayGrid != nullptr && underlayGridColor != nullptr)
    {
        DrawGridOverlayToScene(renderer, camera, *underlayGrid,
                               underlayGridColor->x, underlayGridColor->y,
                               underlayGridColor->z, underlayGridColor->w);
    }

    // 3) Ground tile textures (alpha-blended).
    if (!groundInstances.empty())
    {
        BindSpritePipeline(cmdList, renderer, m_pso, m_frameCBVAddress, m_pso.GetSpritePSO());
        DrawSpriteInstances(cmdList, renderer, groundInstances);
    }

    // 4) Walls / placed objects (opaque cutout).
    if (!cutoutInstances.empty())
    {
        BindSpritePipeline(cmdList, renderer, m_pso, m_frameCBVAddress, m_pso.GetSpriteCutoutPSO());
        DrawSpriteInstances(cmdList, renderer, cutoutInstances);
    }

    // 5) Actors / roofs / roof actors (alpha-blended).
    if (!overlayInstances.empty())
    {
        BindSpritePipeline(cmdList, renderer, m_pso, m_frameCBVAddress, m_pso.GetSpritePSO());
        DrawSpriteInstances(cmdList, renderer, overlayInstances);
    }
}

// ===========================================================================
// Pass: Lighting + shadows
// ===========================================================================

void SceneRenderer::PassLighting(ID3D12GraphicsCommandList* cmdList,
                                  RendererD3D12& renderer,
                                  Camera2D& camera,
                                  LightSystem& lights,
                                  OccluderSet& occluders)
{
    if (lights.GetLightCount() == 0)
        return;

    auto lightRTV = renderer.GetRTVHeap().GetCPUHandle(m_lightAccum.GetRTVIndex());
    auto dsvHandle = renderer.GetDSVHeap().GetCPUHandle(m_depthStencil.GetDSVIndex());

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(m_config.halfResWidth),
                           static_cast<float>(m_config.halfResHeight),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(m_config.halfResWidth),
                            static_cast<LONG>(m_config.halfResHeight) };

    for (const auto& light : lights.GetLights())
    {
        // --- Stencil pass: render shadow volumes ---
        m_depthStencil.Clear(cmdList, renderer.GetDSVHeap(), 1.0f, 0);

        auto shadowQuads = lights.BuildShadowGeometry(light, occluders);
        if (!shadowQuads.empty())
        {
            cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
            cmdList->RSSetViewports(1, &vp);
            cmdList->RSSetScissorRects(1, &scissor);

            cmdList->SetGraphicsRootSignature(m_pso.GetMainRootSig());
            cmdList->SetPipelineState(m_pso.GetShadowVolumePSO());
            cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);
            cmdList->SetGraphicsRootDescriptorTable(1,
                renderer.GetSRVHeap().GetGPUHandle(0));
            cmdList->OMSetStencilRef(1);

            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // Build vertex buffer: each shadow quad ? 2 triangles (6 vertices)
            size_t vertCount = shadowQuads.size() * 6;
            auto alloc = renderer.GetUploadManager().Allocate(
                vertCount * sizeof(DirectX::XMFLOAT2), 16);

            auto* verts = static_cast<DirectX::XMFLOAT2*>(alloc.cpuPtr);
            for (size_t i = 0; i < shadowQuads.size(); ++i)
            {
                const auto& q = shadowQuads[i];
                // Triangle 1: v0, v1, v2
                verts[i * 6 + 0] = q.v[0];
                verts[i * 6 + 1] = q.v[1];
                verts[i * 6 + 2] = q.v[2];
                // Triangle 2: v0, v2, v3
                verts[i * 6 + 3] = q.v[0];
                verts[i * 6 + 4] = q.v[2];
                verts[i * 6 + 5] = q.v[3];
            }

            D3D12_VERTEX_BUFFER_VIEW vbv;
            vbv.BufferLocation = alloc.gpuAddress;
            vbv.SizeInBytes    = static_cast<UINT>(vertCount * sizeof(DirectX::XMFLOAT2));
            vbv.StrideInBytes  = sizeof(DirectX::XMFLOAT2);
            cmdList->IASetVertexBuffers(0, 1, &vbv);

            cmdList->DrawInstanced(static_cast<UINT>(vertCount), 1, 0, 0);
        }

        // --- Light pass: render radial gradient with stencil test ---
        cmdList->OMSetRenderTargets(1, &lightRTV, FALSE, &dsvHandle);
        cmdList->RSSetViewports(1, &vp);
        cmdList->RSSetScissorRects(1, &scissor);

        cmdList->SetGraphicsRootSignature(m_pso.GetLightRootSig());
        cmdList->SetPipelineState(m_pso.GetLightRadialPSO());
        cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);

        UploadLightConstants(renderer, light);
        cmdList->SetGraphicsRootConstantBufferView(1, m_lightCBVAddress);
        cmdList->OMSetStencilRef(0); // Pass where stencil == 0 (not shadowed)

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        cmdList->DrawInstanced(4, 1, 0, 0);
    }

    (void)camera;
}

// ===========================================================================
// Pass: Composite
// ===========================================================================

void SceneRenderer::PassComposite(ID3D12GraphicsCommandList* cmdList,
                                   RendererD3D12& renderer,
                                   FogRenderer& fog)
{
    auto* device = renderer.GetDevice();
    auto& srvHeap = renderer.GetSRVHeap();

    // Create SRV views directly into the 4 contiguous composite slots.
    // We cannot copy from the shader-visible heap (it's write-only on CPU),
    // so we re-create the views each frame into the reserved block.

    // Helper to create a Texture2D SRV at a specific heap index
    auto createSRV = [&](uint32_t heapIndex, ID3D12Resource* resource, DXGI_FORMAT format)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format                    = format;
        srvDesc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels       = 1;
        device->CreateShaderResourceView(resource, &srvDesc,
                                          srvHeap.GetCPUHandle(heapIndex));
    };

    // Slot 0: SceneColor
    createSRV(m_compositeSRVBase, m_sceneColor.GetResource(), m_sceneColor.GetFormat());

    // Slot 1: LightAccum
    createSRV(m_compositeSRVBase + 1, m_lightAccum.GetResource(), m_lightAccum.GetFormat());

    // Slot 2: FogVisible
    if (fog.GetVisibleResource())
        createSRV(m_compositeSRVBase + 2, fog.GetVisibleResource(), fog.GetTextureFormat());
    else
        createSRV(m_compositeSRVBase + 2, m_sceneColor.GetResource(), m_sceneColor.GetFormat());

    // Slot 3: FogExplored
    if (fog.GetExploredResource())
        createSRV(m_compositeSRVBase + 3, fog.GetExploredResource(), fog.GetTextureFormat());
    else
        createSRV(m_compositeSRVBase + 3, m_sceneColor.GetResource(), m_sceneColor.GetFormat());

    // Now render the fullscreen composite
    auto backRTV = renderer.GetBackBufferRTV();
    cmdList->OMSetRenderTargets(1, &backRTV, FALSE, nullptr);

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(renderer.GetWidth()),
                           static_cast<float>(renderer.GetHeight()),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(renderer.GetWidth()),
                            static_cast<LONG>(renderer.GetHeight()) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_pso.GetCompositeRootSig());
    cmdList->SetPipelineState(m_pso.GetCompositePSO());
    cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);

    // Bind the contiguous 4-SRV table
    cmdList->SetGraphicsRootDescriptorTable(1,
        srvHeap.GetGPUHandle(m_compositeSRVBase));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0); // Fullscreen triangle
}

// ===========================================================================
// Pass: UI
// ===========================================================================

void SceneRenderer::PassUI(ID3D12GraphicsCommandList* cmdList,
                            RendererD3D12& renderer,
                            const RenderQueue& renderQueue)
{
    auto uiRange = renderQueue.GetLayerRange(RenderLayer::ScreenSpaceIcons);
    size_t uiCount = uiRange.end - uiRange.begin;
    if (uiCount == 0)
        return;

    auto backRTV = renderer.GetBackBufferRTV();
    cmdList->OMSetRenderTargets(1, &backRTV, FALSE, nullptr);

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(renderer.GetWidth()),
                           static_cast<float>(renderer.GetHeight()),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(renderer.GetWidth()),
                            static_cast<LONG>(renderer.GetHeight()) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_pso.GetMainRootSig());
    cmdList->SetPipelineState(m_pso.GetSpriteScreenPSO());
    cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(0));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    const auto& items = renderQueue.GetItems();
    std::vector<SpriteInstance> instances;
    instances.reserve(uiCount);
    for (size_t i = uiRange.begin; i < uiRange.end; ++i)
        instances.push_back(items[i].instance);

    auto alloc = renderer.GetUploadManager().Allocate(
        instances.size() * sizeof(SpriteInstance), 16);
    std::memcpy(alloc.cpuPtr, instances.data(),
                instances.size() * sizeof(SpriteInstance));

    D3D12_VERTEX_BUFFER_VIEW ibv;
    ibv.BufferLocation = alloc.gpuAddress;
    ibv.SizeInBytes    = static_cast<UINT>(instances.size() * sizeof(SpriteInstance));
    ibv.StrideInBytes  = sizeof(SpriteInstance);
    cmdList->IASetVertexBuffers(1, 1, &ibv);

    cmdList->DrawInstanced(4, static_cast<UINT>(instances.size()), 0, 0);
}

// ===========================================================================
// Debug overlays (grid lines, wall highlights)
// ===========================================================================

void SceneRenderer::DrawGridOverlay(RendererD3D12& renderer, Camera2D& camera,
                                     const Grid& grid,
                                     float r, float g, float b, float a)
{
    if (grid.IsIsometric())
    {
        DrawGridOverlayIsometric(renderer, camera, grid, r, g, b, a);
        return;
    }

    auto* cmdList = renderer.GetCommandList();

    // Determine visible tile range
    auto vb = camera.GetViewBounds();
    int tileX0, tileY0, tileX1, tileY1;
    grid.WorldToTile(vb.left, vb.top, tileX0, tileY0);
    grid.WorldToTile(vb.right, vb.bottom, tileX1, tileY1);

    tileX0 = (std::max)(tileX0 - 1, 0);
    tileY0 = (std::max)(tileY0 - 1, 0);
    tileX1 = (std::min)(tileX1 + 1, grid.GetGridWidth());
    tileY1 = (std::min)(tileY1 + 1, grid.GetGridHeight());

    int cols = tileX1 - tileX0 + 1;
    int rows = tileY1 - tileY0 + 1;

    // Build grid line segments: horizontal + vertical
    // Each line = 2 vertices (XMFLOAT2)
    size_t lineCount = static_cast<size_t>(rows + 1) + static_cast<size_t>(cols + 1);
    size_t vertCount = lineCount * 2;

    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(vertCount);

    float ox = grid.GetOriginX();
    float oy = grid.GetOriginY();
    float ts = grid.GetTileSize();

    // Horizontal lines
    for (int y = tileY0; y <= tileY1; ++y)
    {
        float wy = oy + y * ts;
        float wx0 = ox + tileX0 * ts;
        float wx1 = ox + tileX1 * ts;
        verts.push_back({ wx0, wy });
        verts.push_back({ wx1, wy });
    }

    // Vertical lines
    for (int x = tileX0; x <= tileX1; ++x)
    {
        float wx = ox + x * ts;
        float wy0 = oy + tileY0 * ts;
        float wy1 = oy + tileY1 * ts;
        verts.push_back({ wx, wy0 });
        verts.push_back({ wx, wy1 });
    }

    DrawLineOverlay(renderer, verts.data(), verts.size(), r, g, b, a);
}

void SceneRenderer::DrawGridOverlayIsometric(RendererD3D12& renderer, Camera2D& camera,
                                               const Grid& grid,
                                               float r, float g, float b, float a)
{
    // For isometric grids, draw hex outlines for each visible tile.
    auto vb = camera.GetViewBounds();

    // Conservatively iterate all tiles (the grid is typically small).
    int gw = grid.GetGridWidth();
    int gh = grid.GetGridHeight();

    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(static_cast<size_t>(gw) * gh * 12);

    for (int ty = 0; ty < gh; ++ty)
    {
        for (int tx = 0; tx < gw; ++tx)
        {
            DirectX::XMFLOAT2 hex[6];
            grid.TileHexVertices(tx, ty, hex);

            // Quick frustum cull: check if hex AABB overlaps view
            float minX = hex[0].x; // left vertex
            float maxX = hex[3].x; // right vertex
            float minY = hex[1].y; // upper-left vertex
            float maxY = hex[4].y; // lower-right vertex
            if (maxX < vb.left || minX > vb.right ||
                maxY < vb.top || minY > vb.bottom)
                continue;

            // 6 edges of the hex
            for (int i = 0; i < 6; ++i)
            {
                verts.push_back(hex[i]);
                verts.push_back(hex[(i + 1) % 6]);
            }
        }
    }

    if (!verts.empty())
        DrawLineOverlay(renderer, verts.data(), verts.size(), r, g, b, a);
}

void SceneRenderer::DrawLineOverlay(RendererD3D12& renderer,
                                     const DirectX::XMFLOAT2* vertices,
                                     size_t vertexCount,
                                     float r, float g, float b, float a)
{
    DrawLineOverlayToTarget(renderer, renderer.GetBackBufferRTV(),
                            nullptr, renderer.GetWidth(), renderer.GetHeight(),
                            m_pso.GetGridOverlayPSO(),
                            vertices, vertexCount, r, g, b, a);
}

void SceneRenderer::DrawLineOverlayToTarget(RendererD3D12& renderer,
                                             D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                             const D3D12_CPU_DESCRIPTOR_HANDLE* dsv,
                                             uint32_t width, uint32_t height,
                                             ID3D12PipelineState* pipelineState,
                                             const DirectX::XMFLOAT2* vertices,
                                             size_t vertexCount,
                                             float r, float g, float b, float a)
{
    if (vertexCount < 2)
        return;

    auto* cmdList = renderer.GetCommandList();

    // Upload compact overlay constants: viewProjection + color
    struct OverlayConstants
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT4   color;
    };

    OverlayConstants oc;
    oc.viewProjection = m_frameConstants.viewProjection;
    oc.color = { r, g, b, a };

    auto cbAlloc = renderer.GetUploadManager().Allocate(
        sizeof(OverlayConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    std::memcpy(cbAlloc.cpuPtr, &oc, sizeof(OverlayConstants));

    // Upload vertex data
    auto vtxAlloc = renderer.GetUploadManager().Allocate(
        vertexCount * sizeof(DirectX::XMFLOAT2), 16);
    std::memcpy(vtxAlloc.cpuPtr, vertices, vertexCount * sizeof(DirectX::XMFLOAT2));

    cmdList->OMSetRenderTargets(1, &rtv, FALSE, dsv);

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(width),
                           static_cast<float>(height),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(width),
                            static_cast<LONG>(height) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_pso.GetMainRootSig());
    cmdList->SetPipelineState(pipelineState);
    cmdList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(0));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv;
    vbv.BufferLocation = vtxAlloc.gpuAddress;
    vbv.SizeInBytes    = static_cast<UINT>(vertexCount * sizeof(DirectX::XMFLOAT2));
    vbv.StrideInBytes  = sizeof(DirectX::XMFLOAT2);
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    cmdList->DrawInstanced(static_cast<UINT>(vertexCount), 1, 0, 0);
}

void SceneRenderer::DrawGridOverlayToScene(RendererD3D12& renderer, Camera2D& camera,
                                            const Grid& grid,
                                            float r, float g, float b, float a)
{
    auto sceneRTV = renderer.GetRTVHeap().GetCPUHandle(m_sceneColor.GetRTVIndex());
    auto sceneDSV = renderer.GetDSVHeap().GetCPUHandle(m_depthStencil.GetDSVIndex());

    if (grid.IsIsometric())
    {
        auto vb = camera.GetViewBounds();
        int gw = grid.GetGridWidth();
        int gh = grid.GetGridHeight();

        std::vector<DirectX::XMFLOAT2> verts;
        verts.reserve(static_cast<size_t>(gw) * gh * 12);

        for (int ty = 0; ty < gh; ++ty)
        {
            for (int tx = 0; tx < gw; ++tx)
            {
                DirectX::XMFLOAT2 hex[6];
                grid.TileHexVertices(tx, ty, hex);

                float minX = hex[0].x;
                float maxX = hex[3].x;
                float minY = hex[1].y;
                float maxY = hex[4].y;
                if (maxX < vb.left || minX > vb.right ||
                    maxY < vb.top || minY > vb.bottom)
                    continue;

                for (int i = 0; i < 6; ++i)
                {
                    verts.push_back(hex[i]);
                    verts.push_back(hex[(i + 1) % 6]);
                }
            }
        }

        if (!verts.empty())
        {
            DrawLineOverlayToTarget(renderer, sceneRTV,
                                    &sceneDSV, m_config.fullResWidth, m_config.fullResHeight,
                                    m_pso.GetGridOverlayScenePSO(),
                                    verts.data(), verts.size(), r, g, b, a);
        }
        return;
    }

    auto vb = camera.GetViewBounds();
    int tileX0, tileY0, tileX1, tileY1;
    grid.WorldToTile(vb.left, vb.top, tileX0, tileY0);
    grid.WorldToTile(vb.right, vb.bottom, tileX1, tileY1);

    tileX0 = (std::max)(tileX0 - 1, 0);
    tileY0 = (std::max)(tileY0 - 1, 0);
    tileX1 = (std::min)(tileX1 + 1, grid.GetGridWidth());
    tileY1 = (std::min)(tileY1 + 1, grid.GetGridHeight());

    int cols = tileX1 - tileX0 + 1;
    int rows = tileY1 - tileY0 + 1;
    size_t lineCount = static_cast<size_t>(rows + 1) + static_cast<size_t>(cols + 1);
    size_t vertCount = lineCount * 2;

    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(vertCount);

    float ox = grid.GetOriginX();
    float oy = grid.GetOriginY();
    float ts = grid.GetTileSize();

    for (int y = tileY0; y <= tileY1; ++y)
    {
        float wy = oy + y * ts;
        float wx0 = ox + tileX0 * ts;
        float wx1 = ox + tileX1 * ts;
        verts.push_back({ wx0, wy });
        verts.push_back({ wx1, wy });
    }

    for (int x = tileX0; x <= tileX1; ++x)
    {
        float wx = ox + x * ts;
        float wy0 = oy + tileY0 * ts;
        float wy1 = oy + tileY1 * ts;
        verts.push_back({ wx, wy0 });
        verts.push_back({ wx, wy1 });
    }

    DrawLineOverlayToTarget(renderer, sceneRTV,
                            &sceneDSV, m_config.fullResWidth, m_config.fullResHeight,
                            m_pso.GetGridOverlayScenePSO(),
                            verts.data(), verts.size(), r, g, b, a);
}

} // namespace engine
