#pragma once
// Camera2D.h - 2D camera with world/screen transforms and culling

#include "EngineTypes.h"
#include <DirectXMath.h>
#include <algorithm>

namespace engine
{

// ---------------------------------------------------------------------------
// Camera2D - orthographic camera for the 2D scene
// ---------------------------------------------------------------------------
class Camera2D
{
public:
    void SetViewport(uint32_t screenWidth, uint32_t screenHeight)
    {
        m_screenWidth  = screenWidth;
        m_screenHeight = screenHeight;
        RecalcMatrices();
    }

    void SetPosition(float worldX, float worldY)
    {
        m_worldX = worldX;
        m_worldY = worldY;
        RecalcMatrices();
    }

    void SetZoom(float zoom)
    {
        m_zoom = (std::max)(0.1f, zoom);
        RecalcMatrices();
    }

    void Pan(float dx, float dy)
    {
        m_worldX += dx / m_zoom;
        m_worldY += dy / m_zoom;
        RecalcMatrices();
    }

    // World-space bounds visible on screen (for culling)
    struct ViewBounds
    {
        float left, top, right, bottom;
    };

    ViewBounds GetViewBounds() const
    {
        float halfW = (m_screenWidth  * 0.5f) / m_zoom;
        float halfH = (m_screenHeight * 0.5f) / m_zoom;
        return { m_worldX - halfW, m_worldY - halfH,
                 m_worldX + halfW, m_worldY + halfH };
    }

    // Convert world position to screen position
    DirectX::XMFLOAT2 WorldToScreen(float wx, float wy) const
    {
        float sx = (wx - m_worldX) * m_zoom + m_screenWidth  * 0.5f;
        float sy = (wy - m_worldY) * m_zoom + m_screenHeight * 0.5f;
        return { sx, sy };
    }

    // Convert screen position to world position
    DirectX::XMFLOAT2 ScreenToWorld(float sx, float sy) const
    {
        float wx = (sx - m_screenWidth  * 0.5f) / m_zoom + m_worldX;
        float wy = (sy - m_screenHeight * 0.5f) / m_zoom + m_worldY;
        return { wx, wy };
    }

    // Check if a world-space AABB is visible
    bool IsVisible(float left, float top, float right, float bottom) const
    {
        ViewBounds vb = GetViewBounds();
        return !(right < vb.left || left > vb.right ||
                 bottom < vb.top || top > vb.bottom);
    }

    // Matrices for GPU (view * projection = orthographic)
    const DirectX::XMFLOAT4X4& GetViewProjection() const { return m_viewProj; }

    float GetZoom() const { return m_zoom; }
    float GetWorldX() const { return m_worldX; }
    float GetWorldY() const { return m_worldY; }
    uint32_t GetScreenWidth() const { return m_screenWidth; }
    uint32_t GetScreenHeight() const { return m_screenHeight; }

private:
    void RecalcMatrices()
    {
        float halfW = (m_screenWidth  * 0.5f) / m_zoom;
        float halfH = (m_screenHeight * 0.5f) / m_zoom;

        // Orthographic projection centered on camera position
        DirectX::XMMATRIX proj = DirectX::XMMatrixOrthographicOffCenterLH(
            m_worldX - halfW, m_worldX + halfW,
            m_worldY + halfH, m_worldY - halfH, // Flip Y for screen coords (top = smaller Y)
            0.0f, 1.0f);

        DirectX::XMStoreFloat4x4(&m_viewProj, XMMatrixTranspose(proj));
    }

    float       m_worldX        = 0.0f;
    float       m_worldY        = 0.0f;
    float       m_zoom          = 1.0f;
    uint32_t    m_screenWidth   = 1920;
    uint32_t    m_screenHeight  = 1080;

    DirectX::XMFLOAT4X4 m_viewProj;
};

} // namespace engine
