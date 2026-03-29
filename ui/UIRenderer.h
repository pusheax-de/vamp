#pragma once
// UIRenderer.h - Low-level quad batching for UI elements
//
// Collects screen-space quads (solid rects and text glyphs) during the
// UI render pass, then flushes them as SpriteInstance batches through
// the existing sprite PSO with an orthographic projection.

#include "UITypes.h"
#include "BitmapFont.h"
#include <EngineTypes.h>
#include <RendererD3D12.h>
#include <Texture2D.h>
#include <DescriptorAllocator.h>
#include <UploadManager.h>
#include <PipelineStates.h>
#include <d3d12.h>
#include <DirectXMath.h>
#include <vector>
#include <cstdint>

namespace ui
{

// ---------------------------------------------------------------------------
// UIRenderer - batches UI quads and draws them to the backbuffer
// ---------------------------------------------------------------------------
class UIRenderer
{
public:
    // Initialize: create 1x1 white pixel texture for solid rects
    bool Init(ID3D12Device* device,
              ID3D12GraphicsCommandList* cmdList,
              engine::UploadManager& uploadMgr,
              engine::PersistentDescriptorAllocator& srvHeap);

    void Shutdown(engine::PersistentDescriptorAllocator& srvHeap);

    // Call at the start of each frame's UI pass
    void Begin(uint32_t screenWidth, uint32_t screenHeight);

    // Submit primitives
    void DrawRect(const Rect& rect, const Color& color);
    void DrawText(const BitmapFont& font, const char* text, float x, float y,
                  const Color& color);

    // Flush all batched quads to the GPU
    void End(engine::RendererD3D12& renderer, engine::PipelineStates& pso);

private:
    // Orthographic projection: maps screen pixels to NDC
    DirectX::XMFLOAT4X4 BuildOrthoProjection(uint32_t w, uint32_t h) const;

    // White pixel texture for solid-color rects
    engine::Texture2D   m_whiteTexture;

    // Batched instances grouped by texture index
    struct Batch
    {
        uint32_t textureIndex;
        std::vector<engine::SpriteInstance> instances;
    };

    // Find or create a batch for a given texture
    Batch& GetBatch(uint32_t texIndex);

    std::vector<Batch>  m_batches;
    uint32_t            m_screenWidth   = 0;
    uint32_t            m_screenHeight  = 0;
};

} // namespace ui
