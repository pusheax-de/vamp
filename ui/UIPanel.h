#pragma once
// UIPanel.h - Solid-color rectangular panel with optional border
//
// The simplest visible element. Draws a filled rectangle using the
// UIRenderer's white-pixel texture tinted by the panel color.

#include "UIElement.h"
#include "UIRenderer.h"

namespace ui
{

class UIPanel : public UIElement
{
public:
    void SetColor(const Color& c)           { m_fillColor = c; }
    void SetBorderColor(const Color& c)     { m_borderColor = c; }
    void SetBorderWidth(float w)            { m_borderWidth = w; }
    void SetPadding(float p)                { m_padding = p; }

    const Color& GetColor() const           { return m_fillColor; }
    float GetPadding() const                { return m_padding; }

protected:
    void OnRender(UIRenderer& renderer) override
    {
        const Rect& r = GetScreenRect();

        // Draw border (slightly larger rect behind the fill)
        if (m_borderWidth > 0.0f && m_borderColor.a > 0.0f)
        {
            Rect borderRect;
            borderRect.x = r.x - m_borderWidth;
            borderRect.y = r.y - m_borderWidth;
            borderRect.w = r.w + m_borderWidth * 2.0f;
            borderRect.h = r.h + m_borderWidth * 2.0f;
            renderer.DrawRect(borderRect, m_borderColor);
        }

        // Draw fill
        if (m_fillColor.a > 0.0f)
        {
            renderer.DrawRect(r, m_fillColor);
        }
    }

private:
    Color   m_fillColor     = Color::DarkGray();
    Color   m_borderColor   = Color::Clear();
    float   m_borderWidth   = 0.0f;
    float   m_padding       = 4.0f;
};

} // namespace ui
