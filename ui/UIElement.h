#pragma once
// UIElement.h - Base class for all UI elements
//
// Provides a tree structure, screen-space layout, visibility, and hit testing.
// Concrete elements (UIPanel, UILabel) derive from this.

#include "UITypes.h"
#include <vector>
#include <memory>
#include <string>

namespace ui
{

// Forward declaration
class UIRenderer;

// ---------------------------------------------------------------------------
// UIElement - base node in the UI tree
// ---------------------------------------------------------------------------
class UIElement
{
public:
    virtual ~UIElement() = default;

    // --- Tree management ---

    UIElement* AddChild(std::unique_ptr<UIElement> child)
    {
        child->m_parent = this;
        m_children.push_back(std::move(child));
        return m_children.back().get();
    }

    void RemoveAllChildren() { m_children.clear(); }

    UIElement* GetParent() const { return m_parent; }
    const std::vector<std::unique_ptr<UIElement>>& GetChildren() const { return m_children; }

    // --- Layout properties ---

    void SetPosition(float x, float y)  { m_offsetX = x; m_offsetY = y; }
    void SetSize(float w, float h)      { m_width = w; m_height = h; }
    void SetAnchor(Anchor anchor)       { m_anchor = anchor; }
    void SetVisible(bool vis)           { m_visible = vis; }
    void SetName(const std::string& n)  { m_name = n; }

    float GetWidth() const  { return m_width; }
    float GetHeight() const { return m_height; }
    bool  IsVisible() const { return m_visible; }
    const std::string& GetName() const { return m_name; }

    // Resolved screen rect (computed during layout pass)
    const Rect& GetScreenRect() const { return m_screenRect; }

    // --- Layout / Render / Hit test (called by UISystem) ---

    void Layout(const Rect& parentRect)
    {
        if (!m_visible)
            return;

        float resolvedX, resolvedY;
        ResolveAnchor(m_anchor, parentRect, m_width, m_height,
                      m_offsetX, m_offsetY, resolvedX, resolvedY);

        m_screenRect = { resolvedX, resolvedY, m_width, m_height };

        OnLayout(m_screenRect);

        for (auto& child : m_children)
            child->Layout(m_screenRect);
    }

    void Render(UIRenderer& renderer)
    {
        if (!m_visible)
            return;

        OnRender(renderer);

        for (auto& child : m_children)
            child->Render(renderer);
    }

    UIElement* HitTest(float px, float py)
    {
        if (!m_visible)
            return nullptr;

        // Check children in reverse order (top-most first)
        for (int i = static_cast<int>(m_children.size()) - 1; i >= 0; --i)
        {
            UIElement* hit = m_children[i]->HitTest(px, py);
            if (hit)
                return hit;
        }

        if (m_screenRect.Contains(px, py))
            return this;

        return nullptr;
    }

protected:
    // Override in derived classes
    virtual void OnLayout(const Rect& /*screenRect*/) {}
    virtual void OnRender(UIRenderer& /*renderer*/) {}

private:
    UIElement*  m_parent = nullptr;
    std::vector<std::unique_ptr<UIElement>> m_children;

    // Layout input
    Anchor      m_anchor    = Anchor::TopLeft;
    float       m_offsetX   = 0.0f;
    float       m_offsetY   = 0.0f;
    float       m_width     = 100.0f;
    float       m_height    = 100.0f;
    bool        m_visible   = true;
    std::string m_name;

    // Layout output
    Rect        m_screenRect;
};

} // namespace ui
