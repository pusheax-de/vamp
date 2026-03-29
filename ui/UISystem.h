#pragma once
// UISystem.h - Root manager for the UI element tree
//
// Owns the UIRenderer, BitmapFont, and the root element.
// Provides the public API for game code to create UI, handle input, and render.

#include "UITypes.h"
#include "UIElement.h"
#include "UIRenderer.h"
#include "BitmapFont.h"
#include "UIPanel.h"
#include "UILabel.h"
#include <RendererD3D12.h>
#include <PipelineStates.h>
#include <memory>
#include <string>

namespace ui
{

// ---------------------------------------------------------------------------
// UISystem - the single entry point for all UI operations
// ---------------------------------------------------------------------------
class UISystem
{
public:
    // Initialize fonts, renderer, and root element
    bool Init(ID3D12Device* device,
              ID3D12GraphicsCommandList* cmdList,
              engine::UploadManager& uploadMgr,
              engine::PersistentDescriptorAllocator& srvHeap);

    void Shutdown(engine::PersistentDescriptorAllocator& srvHeap);

    // Set screen dimensions (call on resize)
    void SetScreenSize(uint32_t width, uint32_t height);

    // Process input. Returns true if the UI consumed the click.
    bool HandleMouseClick(float px, float py);

    // Per-frame update: layout the tree
    void Update();

    // Render the entire UI
    void Render(engine::RendererD3D12& renderer, engine::PipelineStates& pso);

    // --- Element creation helpers ---

    // Access the root element to build your UI tree
    UIElement* GetRoot() { return m_root.get(); }

    // Convenience: create a panel and return a raw pointer (root owns it)
    UIPanel* CreatePanel(UIElement* parent, const std::string& name,
                         float x, float y, float w, float h,
                         const Color& color = Color::DarkGray(),
                         Anchor anchor = Anchor::TopLeft);

    // Convenience: create a label and return a raw pointer
    UILabel* CreateLabel(UIElement* parent, const std::string& name,
                         const std::string& text,
                         float x, float y, float w, float h,
                         const Color& textColor = Color::White(),
                         TextAlign align = TextAlign::Left);

    // Access the default font
    const BitmapFont* GetDefaultFont() const { return &m_defaultFont; }

private:
    std::unique_ptr<UIElement>  m_root;
    UIRenderer                  m_renderer;
    BitmapFont                  m_defaultFont;
    uint32_t                    m_screenWidth  = 1280;
    uint32_t                    m_screenHeight = 720;
};

} // namespace ui
