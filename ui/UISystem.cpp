// UISystem.cpp - UI tree management, layout, and rendering

#include "UISystem.h"

namespace ui
{

// ===========================================================================
// Init / Shutdown
// ===========================================================================

bool UISystem::Init(ID3D12Device* device,
                    ID3D12GraphicsCommandList* cmdList,
                    engine::UploadManager& uploadMgr,
                    engine::PersistentDescriptorAllocator& srvHeap)
{
    if (!m_renderer.Init(device, cmdList, uploadMgr, srvHeap))
        return false;

    if (!m_defaultFont.Create(device, cmdList, uploadMgr, srvHeap,
                              "Consolas", 16, false))
        return false;

    // Create invisible root element that spans the screen
    m_root = std::unique_ptr<UIElement>(new UIElement());
    m_root->SetName("root");
    m_root->SetPosition(0, 0);
    m_root->SetSize(static_cast<float>(m_screenWidth),
                    static_cast<float>(m_screenHeight));

    return true;
}

void UISystem::Shutdown(engine::PersistentDescriptorAllocator& srvHeap)
{
    m_root.reset();
    m_defaultFont.Shutdown(srvHeap);
    m_renderer.Shutdown(srvHeap);
}

// ===========================================================================
// Screen size
// ===========================================================================

void UISystem::SetScreenSize(uint32_t width, uint32_t height)
{
    m_screenWidth  = width;
    m_screenHeight = height;
    if (m_root)
        m_root->SetSize(static_cast<float>(width), static_cast<float>(height));
}

// ===========================================================================
// Input
// ===========================================================================

bool UISystem::HandleMouseClick(float px, float py)
{
    if (!m_root)
        return false;

    UIElement* hit = m_root->HitTest(px, py);
    return hit != nullptr && hit != m_root.get();
}

// ===========================================================================
// Update (layout pass)
// ===========================================================================

void UISystem::Update()
{
    if (!m_root)
        return;

    Rect screenRect = { 0.0f, 0.0f,
                        static_cast<float>(m_screenWidth),
                        static_cast<float>(m_screenHeight) };
    m_root->Layout(screenRect);
}

// ===========================================================================
// Render
// ===========================================================================

void UISystem::Render(engine::RendererD3D12& renderer, engine::PipelineStates& pso)
{
    if (!m_root)
        return;

    m_renderer.Begin(m_screenWidth, m_screenHeight);
    m_root->Render(m_renderer);
    m_renderer.End(renderer, pso);
}

// ===========================================================================
// Element creation helpers
// ===========================================================================

UIPanel* UISystem::CreatePanel(UIElement* parent, const std::string& name,
                               float x, float y, float w, float h,
                               const Color& color, Anchor anchor)
{
    auto panel = std::unique_ptr<UIPanel>(new UIPanel());
    panel->SetName(name);
    panel->SetPosition(x, y);
    panel->SetSize(w, h);
    panel->SetColor(color);
    panel->SetAnchor(anchor);

    UIPanel* ptr = panel.get();
    (parent ? parent : m_root.get())->AddChild(std::move(panel));
    return ptr;
}

UILabel* UISystem::CreateLabel(UIElement* parent, const std::string& name,
                               const std::string& text,
                               float x, float y, float w, float h,
                               const Color& textColor, TextAlign align)
{
    auto label = std::unique_ptr<UILabel>(new UILabel());
    label->SetName(name);
    label->SetPosition(x, y);
    label->SetSize(w, h);
    label->SetText(text);
    label->SetFont(&m_defaultFont);
    label->SetTextColor(textColor);
    label->SetTextAlign(align);

    UILabel* ptr = label.get();
    (parent ? parent : m_root.get())->AddChild(std::move(label));
    return ptr;
}

UIDropdown* UISystem::CreateDropdown(UIElement* parent, const std::string& name,
                                     float x, float y, float w,
                                     const std::vector<DropdownItem>& items,
                                     Anchor anchor)
{
    auto dropdown = std::unique_ptr<UIDropdown>(new UIDropdown());
    dropdown->SetName(name);
    dropdown->SetPosition(x, y);
    dropdown->SetSize(w, 20.0f); // Height is the closed button row
    dropdown->SetAnchor(anchor);
    dropdown->SetFont(&m_defaultFont);
    dropdown->SetItems(items);

    UIDropdown* ptr = dropdown.get();
    (parent ? parent : m_root.get())->AddChild(std::move(dropdown));
    m_dropdowns.push_back(ptr);
    return ptr;
}

void UISystem::HandleChar(char ch)
{
    for (auto* dd : m_dropdowns)
    {
        if (dd && dd->IsVisible() && dd->IsOpen())
        {
            dd->HandleChar(ch);
            return;
        }
    }
}

} // namespace ui
