#pragma once
// UITypes.h - Core types for the UI system

#include <cstdint>
#include <algorithm>

namespace ui
{

// ---------------------------------------------------------------------------
// Screen-space rectangle (pixels, origin top-left)
// ---------------------------------------------------------------------------
struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    float Right()  const { return x + w; }
    float Bottom() const { return y + h; }

    bool Contains(float px, float py) const
    {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    Rect Inset(float left, float top, float right, float bottom) const
    {
        Rect r;
        r.x = x + left;
        r.y = y + top;
        r.w = (std::max)(0.0f, w - left - right);
        r.h = (std::max)(0.0f, h - top - bottom);
        return r;
    }
};

// ---------------------------------------------------------------------------
// RGBA color (0-1 float)
// ---------------------------------------------------------------------------
struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    static Color White()       { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
    static Color Black()       { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
    static Color Clear()       { return { 0.0f, 0.0f, 0.0f, 0.0f }; }
    static Color Red()         { return { 1.0f, 0.0f, 0.0f, 1.0f }; }
    static Color Green()       { return { 0.0f, 1.0f, 0.0f, 1.0f }; }
    static Color DarkGray()    { return { 0.15f, 0.15f, 0.15f, 0.9f }; }
    static Color Blood()       { return { 0.5f, 0.05f, 0.05f, 1.0f }; }
};

// ---------------------------------------------------------------------------
// Anchor point for positioning relative to parent
// ---------------------------------------------------------------------------
enum class Anchor : uint8_t
{
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

// ---------------------------------------------------------------------------
// Text alignment within a label
// ---------------------------------------------------------------------------
enum class TextAlign : uint8_t
{
    Left,
    Center,
    Right
};

// ---------------------------------------------------------------------------
// Resolve anchor to a pixel offset within a parent rect
// ---------------------------------------------------------------------------
inline void ResolveAnchor(Anchor anchor, const Rect& parent,
                          float childW, float childH,
                          float offsetX, float offsetY,
                          float& outX, float& outY)
{
    float ax = parent.x;
    float ay = parent.y;

    switch (anchor)
    {
    case Anchor::TopLeft:       ax = parent.x;                          ay = parent.y;                          break;
    case Anchor::TopCenter:     ax = parent.x + (parent.w - childW) * 0.5f; ay = parent.y;                     break;
    case Anchor::TopRight:      ax = parent.x + parent.w - childW;     ay = parent.y;                          break;
    case Anchor::CenterLeft:    ax = parent.x;                          ay = parent.y + (parent.h - childH) * 0.5f; break;
    case Anchor::Center:        ax = parent.x + (parent.w - childW) * 0.5f; ay = parent.y + (parent.h - childH) * 0.5f; break;
    case Anchor::CenterRight:   ax = parent.x + parent.w - childW;     ay = parent.y + (parent.h - childH) * 0.5f; break;
    case Anchor::BottomLeft:    ax = parent.x;                          ay = parent.y + parent.h - childH;     break;
    case Anchor::BottomCenter:  ax = parent.x + (parent.w - childW) * 0.5f; ay = parent.y + parent.h - childH; break;
    case Anchor::BottomRight:   ax = parent.x + parent.w - childW;     ay = parent.y + parent.h - childH;     break;
    }

    outX = ax + offsetX;
    outY = ay + offsetY;
}

} // namespace ui
