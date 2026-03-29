#pragma once
// UILabel.h - Text element rendered using BitmapFont

#include "UIElement.h"
#include "UIRenderer.h"
#include "BitmapFont.h"
#include <string>

namespace ui
{

class UILabel : public UIElement
{
public:
    void SetText(const std::string& text)   { m_text = text; }
    void SetFont(const BitmapFont* font)    { m_font = font; }
    void SetTextColor(const Color& c)       { m_textColor = c; }
    void SetTextAlign(TextAlign align)      { m_align = align; }

    const std::string& GetText() const      { return m_text; }

    // Auto-size the element to fit the text
    void SizeToFit()
    {
        if (!m_font)
            return;
        float w = m_font->MeasureWidth(m_text.c_str());
        float h = m_font->GetLineHeight();
        SetSize(w, h);
    }

protected:
    void OnRender(UIRenderer& renderer) override
    {
        if (!m_font || m_text.empty())
            return;

        const Rect& r = GetScreenRect();

        // Calculate starting X based on alignment
        float textWidth = m_font->MeasureWidth(m_text.c_str());
        float startX = r.x;
        switch (m_align)
        {
        case TextAlign::Center: startX = r.x + (r.w - textWidth) * 0.5f; break;
        case TextAlign::Right:  startX = r.x + r.w - textWidth;          break;
        default:                                                          break;
        }

        // Center text vertically within the element
        float lineH = m_font->GetLineHeight();
        float startY = r.y + (r.h - lineH) * 0.5f;

        renderer.DrawText(*m_font, m_text.c_str(), startX, startY, m_textColor);
    }

private:
    const BitmapFont*   m_font      = nullptr;
    std::string         m_text;
    Color               m_textColor = Color::White();
    TextAlign           m_align     = TextAlign::Left;
};

} // namespace ui
