// UIRenderer.cpp - Quad batching and rendering implementation

#include "UIRenderer.h"
#include <cstring>
#include <cassert>
#include <cmath> // For std::floor

namespace ui
{

// ===========================================================================
// Init / Shutdown
// ===========================================================================

bool UIRenderer::Init(ID3D12Device* device,
                      ID3D12GraphicsCommandList* cmdList,
                      engine::UploadManager& uploadMgr,
                      engine::PersistentDescriptorAllocator& srvHeap)
{
    // Create a 1x1 white pixel texture for solid-color rects
    uint32_t white = 0xFFFFFFFF;
    if (!m_whiteTexture.CreateFromRGBA(device, cmdList, uploadMgr, srvHeap, 1, 1, &white))
        return false;

    return true;
}

void UIRenderer::Shutdown(engine::PersistentDescriptorAllocator& srvHeap)
{
    m_whiteTexture.Shutdown(srvHeap);
}

// ===========================================================================
// Frame lifecycle
// ===========================================================================

void UIRenderer::Begin(uint32_t screenWidth, uint32_t screenHeight)
{
    m_screenWidth  = screenWidth;
    m_screenHeight = screenHeight;
    m_batches.clear();
}

void UIRenderer::End(engine::RendererD3D12& renderer, engine::PipelineStates& pso)
{
    if (m_batches.empty())
        return;

    auto* cmdList = renderer.GetCommandList();

    // Build orthographic projection constant buffer
    // The sprite VS uses viewProjection to transform positions.
    // We provide an ortho matrix that maps pixel coords to clip space.
    struct UICBData
    {
        DirectX::XMFLOAT4X4 viewProjection;
        DirectX::XMFLOAT2   cameraPosition;
        float                cameraZoom;
        float                time;
        DirectX::XMFLOAT2   screenSize;
        DirectX::XMFLOAT2   fogTexelSize;
        float                ambientDarkening;
        DirectX::XMFLOAT3   fogColor;
        DirectX::XMFLOAT2   fogWorldOrigin;
        DirectX::XMFLOAT2   fogWorldSize;
    };

    UICBData cb;
    std::memset(&cb, 0, sizeof(cb));
    cb.viewProjection = BuildOrthoProjection(m_screenWidth, m_screenHeight);
    cb.cameraPosition = { 0.0f, 0.0f };
    cb.cameraZoom     = 1.0f;
    cb.time           = 0.0f;
    cb.screenSize     = { static_cast<float>(m_screenWidth),
                          static_cast<float>(m_screenHeight) };
    cb.fogTexelSize   = { 0.0f, 0.0f };
    cb.ambientDarkening = 1.0f;
    cb.fogColor       = { 0.0f, 0.0f, 0.0f };
    cb.fogWorldOrigin = { 0.0f, 0.0f };
    cb.fogWorldSize   = { 1.0f, 1.0f };

    auto cbAlloc = renderer.GetUploadManager().Allocate(
        sizeof(UICBData), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    std::memcpy(cbAlloc.cpuPtr, &cb, sizeof(UICBData));

    // Set render target to backbuffer
    auto backRTV = renderer.GetBackBufferRTV();
    cmdList->OMSetRenderTargets(1, &backRTV, FALSE, nullptr);

    D3D12_VIEWPORT vp = { 0, 0,
                           static_cast<float>(m_screenWidth),
                           static_cast<float>(m_screenHeight),
                           0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0,
                            static_cast<LONG>(m_screenWidth),
                            static_cast<LONG>(m_screenHeight) };
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Bind sprite PSO (reuse the main root sig + screen-space sprite PSO)
    cmdList->SetGraphicsRootSignature(pso.GetMainRootSig());
    cmdList->SetPipelineState(pso.GetSpriteScreenPointPSO());
    cmdList->SetGraphicsRootConstantBufferView(0, cbAlloc.gpuAddress);
    cmdList->SetGraphicsRootDescriptorTable(1,
        renderer.GetSRVHeap().GetGPUHandle(0));

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Draw each batch
    for (const auto& batch : m_batches)
    {
        if (batch.instances.empty())
            continue;

        auto instAlloc = renderer.GetUploadManager().Allocate(
            batch.instances.size() * sizeof(engine::SpriteInstance), 16);
        std::memcpy(instAlloc.cpuPtr, batch.instances.data(),
                    batch.instances.size() * sizeof(engine::SpriteInstance));

        D3D12_VERTEX_BUFFER_VIEW ibv;
        ibv.BufferLocation = instAlloc.gpuAddress;
        ibv.SizeInBytes    = static_cast<UINT>(batch.instances.size() * sizeof(engine::SpriteInstance));
        ibv.StrideInBytes  = sizeof(engine::SpriteInstance);
        cmdList->IASetVertexBuffers(1, 1, &ibv);

        cmdList->DrawInstanced(4, static_cast<UINT>(batch.instances.size()), 0, 0);
    }
}

// ===========================================================================
// Primitive submission
// ===========================================================================

void UIRenderer::DrawRect(const Rect& rect, const Color& color)
{
    // Snap to integer pixel coordinates for crisp edges
    float x = std::floor(rect.x);
    float y = std::floor(rect.y);
    float w = std::floor(rect.w + 0.5f);
    float h = std::floor(rect.h + 0.5f);

    engine::SpriteInstance inst;
    // The sprite VS expects center position and full size
    inst.position     = { x + w * 0.5f, y + h * 0.5f };
    inst.size         = { w, h };
    inst.uvRect       = { 0.0f, 0.0f, 1.0f, 1.0f };
    inst.color        = { color.r, color.g, color.b, color.a };
    inst.rotation     = 0.0f;
    inst.sortY        = 0.0f;
    inst.textureIndex = m_whiteTexture.GetSRVIndex();
    inst.pad          = 0;

    GetBatch(m_whiteTexture.GetSRVIndex()).instances.push_back(inst);
}

void UIRenderer::DrawText(const BitmapFont& font, const char* text,
                          float x, float y, const Color& color)
{
    uint32_t texIdx = font.GetSRVIndex();
    auto& batch = GetBatch(texIdx);
    float lineH = font.GetLineHeight();

    // Snap starting position to integer pixels
    float cx = std::floor(x);
    float sy = std::floor(y);

    while (*text)
    {
        char ch = *text++;
        const GlyphInfo& g = font.GetGlyph(ch);

        if (ch != ' ')
        {
            float gw = std::floor(g.widthPx + 0.5f);
            float gh = std::floor(g.heightPx + 0.5f);

            engine::SpriteInstance inst;
            inst.position     = { cx + gw * 0.5f, sy + gh * 0.5f };
            inst.size         = { gw, gh };
            inst.uvRect       = { g.u0, g.v0, g.u1, g.v1 };
            inst.color        = { color.r, color.g, color.b, color.a };
            inst.rotation     = 0.0f;
            inst.sortY        = 0.0f;
            inst.textureIndex = texIdx;
            inst.pad          = 0;

            batch.instances.push_back(inst);
        }

        cx += std::floor(g.advanceX + 0.5f);
    }
}

// ===========================================================================
// Helpers
// ===========================================================================

UIRenderer::Batch& UIRenderer::GetBatch(uint32_t texIndex)
{
    // Find existing batch for this texture
    for (auto& b : m_batches)
    {
        if (b.textureIndex == texIndex)
            return b;
    }

    // Create new batch
    m_batches.push_back({ texIndex, {} });
    return m_batches.back();
}

DirectX::XMFLOAT4X4 UIRenderer::BuildOrthoProjection(uint32_t w, uint32_t h) const
{
    // Maps (0,0)-(w,h) screen pixels to (-1,+1)-(+1,-1) clip space.
    // The sprite VS uses mul(float4, matrix) so we need the transposed
    // (row-major) form, matching Camera2D::RecalcMatrices().
    float L = 0.0f;
    float R = static_cast<float>(w);
    float T = 0.0f;
    float B = static_cast<float>(h);

    DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(
        L, R, B, T, 0.0f, 1.0f);

    DirectX::XMFLOAT4X4 m;
    DirectX::XMStoreFloat4x4(&m, DirectX::XMMatrixTranspose(proj));
    return m;
}

} // namespace ui
