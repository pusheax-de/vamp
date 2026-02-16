// SceneRenderer.cpp - Multi-pass scene renderer implementation

#include "SceneRenderer.h"
#include <cassert>
#include <cstring>

namespace engine
{

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
                                 float time)
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
    m_frameConstants.pad[0] = m_frameConstants.pad[1] = m_frameConstants.pad[2] = 0.0f;

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
        const float clearColor[] = { 0.05f, 0.05f, 0.08f, 1.0f };
        m_sceneColor.Clear(cmdList, renderer.GetRTVHeap(), clearColor);
        m_depthStencil.Clear(cmdList, renderer.GetDSVHeap());

        PassBackground(cmdList, renderer, camera, bgPager);
        PassBaseScene(cmdList, renderer, camera, renderQueue);
    }

    // --- Pass 3: Dynamic lighting + shadows -> LightAccum ---
    {
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
    auto pages = bgPager.GetVisiblePages();
    if (pages.empty())
        return;

    // Set render target
    auto rtvHandle = renderer.GetRTVHeap().GetCPUHandle(m_sceneColor.GetRTVIndex());
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

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

    // Build instance data for background pages
    std::vector<SpriteInstance> instances;
    instances.reserve(pages.size());

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
        inst.pad      = 0;
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
                                   const RenderQueue& renderQueue)
{
    (void)camera;

    // Collect items from GroundTiles through RoofActors
    auto groundRange  = renderQueue.GetLayerRange(RenderLayer::GroundTiles);
    auto wallRange    = renderQueue.GetLayerRange(RenderLayer::WallsProps);
    auto actorRange   = renderQueue.GetLayerRange(RenderLayer::Actors);
    auto roofRange    = renderQueue.GetLayerRange(RenderLayer::Roofs);
    auto roofActRange = renderQueue.GetLayerRange(RenderLayer::RoofActors);

    size_t totalItems = (groundRange.end - groundRange.begin)
                      + (wallRange.end - wallRange.begin)
                      + (actorRange.end - actorRange.begin)
                      + (roofRange.end - roofRange.begin)
                      + (roofActRange.end - roofActRange.begin);

    if (totalItems == 0)
        return;

    // RT should already be set from PassBackground, but ensure it
    auto rtvHandle = renderer.GetRTVHeap().GetCPUHandle(m_sceneColor.GetRTVIndex());
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    cmdList->SetGraphicsRootSignature(m_pso.GetMainRootSig());
    cmdList->SetPipelineState(m_pso.GetSpritePSO());
    cmdList->SetGraphicsRootConstantBufferView(0, m_frameCBVAddress);
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(0));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Build instance buffer from all scene layers (already sorted)
    std::vector<SpriteInstance> instances;
    instances.reserve(totalItems);

    const auto& items = renderQueue.GetItems();
    for (size_t i = groundRange.begin; i < groundRange.end; ++i)
        instances.push_back(items[i].instance);
    for (size_t i = wallRange.begin; i < wallRange.end; ++i)
        instances.push_back(items[i].instance);
    for (size_t i = actorRange.begin; i < actorRange.end; ++i)
        instances.push_back(items[i].instance);
    for (size_t i = roofRange.begin; i < roofRange.end; ++i)
        instances.push_back(items[i].instance);
    for (size_t i = roofActRange.begin; i < roofActRange.end; ++i)
        instances.push_back(items[i].instance);

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

    // Bind SRV table: [SceneColor, LightAccum, FogVisible, FogExplored]
    // The composite root sig expects 4 contiguous SRVs. We point the table
    // at the SRV heap base; the shader indexes t0-t3 relative to this.
    // For simplicity, we just point at the persistent heap — the SRVs were
    // created at known indices. The fullscreen shader will sample them.
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(m_sceneColor.GetSRVIndex()));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0); // Fullscreen triangle

    (void)fog;
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
    cmdList->SetPipelineState(m_pso.GetSpritePSO());
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

} // namespace engine
