#pragma once
// InputSystem.h - Keyboard, mouse, and camera input for the 2D engine

#include "EngineTypes.h"
#include "Camera2D.h"
#include "Grid.h"
#include <DirectXMath.h>
#include <cstdint>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace engine
{

// ---------------------------------------------------------------------------
// MouseButton IDs
// ---------------------------------------------------------------------------
enum class MouseButton : uint8_t
{
    Left   = 0,
    Right  = 1,
    Middle = 2,
    COUNT  = 3
};

// ---------------------------------------------------------------------------
// InputSystem - polls keyboard/mouse, drives camera, resolves tile picks
//
// Usage:
//   1. Forward WM_* messages via HandleMessage() from WndProc
//   2. Call Update() once per frame before gameplay logic
//   3. Query key/mouse state with the accessor methods
//   4. Call EndFrame() at the very end of the frame
// ---------------------------------------------------------------------------
class InputSystem
{
public:
    // ----- Setup -----

    void Init()
    {
        std::memset(m_keyDown,      0, sizeof(m_keyDown));
        std::memset(m_keyPressed,   0, sizeof(m_keyPressed));
        std::memset(m_keyReleased,  0, sizeof(m_keyReleased));
        std::memset(m_keyDownPrev,  0, sizeof(m_keyDownPrev));
        std::memset(m_mouseDown,    0, sizeof(m_mouseDown));
        std::memset(m_mousePressed, 0, sizeof(m_mousePressed));
        std::memset(m_mouseReleased,0, sizeof(m_mouseReleased));
        std::memset(m_mouseDownPrev,0, sizeof(m_mouseDownPrev));
        m_mouseX = m_mouseY = 0;
        m_mouseDeltaX = m_mouseDeltaY = 0;
        m_scrollDelta = 0.0f;
        m_scrollAccum = 0.0f;
        m_dragStartX = m_dragStartY = 0;
        m_dragging = false;
    }

    // ----- Win32 message forwarding (call from WndProc) -----

    // Returns true if the message was consumed.
    bool HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam < 256)
                m_keyDown[wParam] = true;
            return true;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wParam < 256)
                m_keyDown[wParam] = false;
            return true;

        case WM_LBUTTONDOWN:
            m_mouseDown[static_cast<int>(MouseButton::Left)] = true;
            return true;
        case WM_LBUTTONUP:
            m_mouseDown[static_cast<int>(MouseButton::Left)] = false;
            return true;

        case WM_RBUTTONDOWN:
            m_mouseDown[static_cast<int>(MouseButton::Right)] = true;
            return true;
        case WM_RBUTTONUP:
            m_mouseDown[static_cast<int>(MouseButton::Right)] = false;
            return true;

        case WM_MBUTTONDOWN:
            m_mouseDown[static_cast<int>(MouseButton::Middle)] = true;
            m_dragStartX = m_mouseX;
            m_dragStartY = m_mouseY;
            m_dragging = true;
            return true;
        case WM_MBUTTONUP:
            m_mouseDown[static_cast<int>(MouseButton::Middle)] = false;
            m_dragging = false;
            return true;

        case WM_MOUSEMOVE:
        {
            int newX = static_cast<int>(static_cast<short>(LOWORD(lParam)));
            int newY = static_cast<int>(static_cast<short>(HIWORD(lParam)));
            m_mouseDeltaX += newX - m_mouseX;
            m_mouseDeltaY += newY - m_mouseY;
            m_mouseX = newX;
            m_mouseY = newY;
            return true;
        }

        case WM_MOUSEWHEEL:
            m_scrollAccum += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            return true;

        default:
            return false;
        }
    }

    // ----- Per-frame update (call once at frame start) -----

    void Update(float deltaTime, Camera2D& camera, const Grid& grid)
    {
        // Derive pressed / released edges
        for (int i = 0; i < 256; ++i)
        {
            m_keyPressed[i]  = m_keyDown[i] && !m_keyDownPrev[i];
            m_keyReleased[i] = !m_keyDown[i] && m_keyDownPrev[i];
        }
        for (int i = 0; i < static_cast<int>(MouseButton::COUNT); ++i)
        {
            m_mousePressed[i]  = m_mouseDown[i] && !m_mouseDownPrev[i];
            m_mouseReleased[i] = !m_mouseDown[i] && m_mouseDownPrev[i];
        }

        // Latch scroll
        m_scrollDelta = m_scrollAccum;
        m_scrollAccum = 0.0f;

        // --- Camera: WASD / arrow-key pan ---
        if (m_keyboardPanEnabled)
        {
            float panSpeed = m_cameraPanSpeed * deltaTime / camera.GetZoom();

            float panX = 0.0f, panY = 0.0f;
            if (IsKeyDown('W') || IsKeyDown(VK_UP))    panY -= panSpeed;
            if (IsKeyDown('S') || IsKeyDown(VK_DOWN))   panY += panSpeed;
            if (IsKeyDown('A') || IsKeyDown(VK_LEFT))   panX -= panSpeed;
            if (IsKeyDown('D') || IsKeyDown(VK_RIGHT))  panX += panSpeed;

            if (panX != 0.0f || panY != 0.0f)
                camera.SetPosition(camera.GetWorldX() + panX, camera.GetWorldY() + panY);
        }

        // --- Camera: edge-scroll (mouse near window edge) ---
        if (m_edgeScrollEnabled)
        {
            int edgeMargin = m_edgeScrollMargin;
            float edgeSpeed = m_edgeScrollSpeed * deltaTime / camera.GetZoom();
            float ex = 0.0f, ey = 0.0f;

            int sw = static_cast<int>(camera.GetScreenWidth());
            int sh = static_cast<int>(camera.GetScreenHeight());

            if (m_mouseX <= edgeMargin)        ex -= edgeSpeed;
            if (m_mouseX >= sw - edgeMargin)   ex += edgeSpeed;
            if (m_mouseY <= edgeMargin)        ey -= edgeSpeed;
            if (m_mouseY >= sh - edgeMargin)   ey += edgeSpeed;

            if (ex != 0.0f || ey != 0.0f)
                camera.SetPosition(camera.GetWorldX() + ex, camera.GetWorldY() + ey);
        }

        // --- Camera: right-mouse-button drag pan ---
        if (IsMouseDown(MouseButton::Right))
        {
            float dx = static_cast<float>(m_mouseDeltaX);
            float dy = static_cast<float>(m_mouseDeltaY);
            if (dx != 0.0f || dy != 0.0f)
                camera.Pan(-dx, -dy);
        }

        // --- Camera: middle-mouse drag pan ---
        if (m_dragging && IsMouseDown(MouseButton::Middle))
        {
            float dx = static_cast<float>(m_mouseDeltaX);
            float dy = static_cast<float>(m_mouseDeltaY);
            if (dx != 0.0f || dy != 0.0f)
                camera.Pan(-dx, -dy);  // Pan() already divides by zoom
        }

        // --- Camera: scroll-wheel zoom ---
        if (m_scrollDelta != 0.0f)
        {
            float zoomFactor = 1.0f + m_scrollDelta * m_zoomSensitivity;
            float newZoom = camera.GetZoom() * zoomFactor;
            newZoom = (newZoom < m_zoomMin) ? m_zoomMin : ((newZoom > m_zoomMax) ? m_zoomMax : newZoom);
            camera.SetZoom(newZoom);
        }

        // --- Resolve mouse world position and tile hover ---
        auto worldPos = camera.ScreenToWorld(static_cast<float>(m_mouseX),
                                              static_cast<float>(m_mouseY));
        m_mouseWorldX = worldPos.x;
        m_mouseWorldY = worldPos.y;

        grid.WorldToTile(m_mouseWorldX, m_mouseWorldY, m_hoverTileX, m_hoverTileY);
        m_hoverTileValid = grid.InBounds(m_hoverTileX, m_hoverTileY);

        // --- Tile click ---
        m_tileClicked = false;
        m_tileRightClicked = false;
        if (m_hoverTileValid)
        {
            if (IsMousePressed(MouseButton::Left))
            {
                m_tileClicked  = true;
                m_clickTileX   = m_hoverTileX;
                m_clickTileY   = m_hoverTileY;
            }
            // Right-click tile only on press, not while dragging the camera
            if (IsMousePressed(MouseButton::Right))
            {
                m_rightClickStartX = m_mouseX;
                m_rightClickStartY = m_mouseY;
            }
            if (IsMouseReleased(MouseButton::Right))
            {
                int dragDist = std::abs(m_mouseX - m_rightClickStartX)
                             + std::abs(m_mouseY - m_rightClickStartY);
                if (dragDist < 5)
                {
                    m_tileRightClicked = true;
                    m_rightClickTileX  = m_hoverTileX;
                    m_rightClickTileY  = m_hoverTileY;
                }
            }
        }

        // Reset per-frame deltas
        m_mouseDeltaX = 0;
        m_mouseDeltaY = 0;
    }

    // Call at the very end of the frame to snapshot previous state
    void EndFrame()
    {
        std::memcpy(m_keyDownPrev,   m_keyDown,   sizeof(m_keyDown));
        std::memcpy(m_mouseDownPrev, m_mouseDown,  sizeof(m_mouseDown));
    }

    // ----- Keyboard queries -----

    bool IsKeyDown(int vk)     const { return (vk >= 0 && vk < 256) && m_keyDown[vk]; }
    bool IsKeyPressed(int vk)  const { return (vk >= 0 && vk < 256) && m_keyPressed[vk]; }
    bool IsKeyReleased(int vk) const { return (vk >= 0 && vk < 256) && m_keyReleased[vk]; }

    // ----- Mouse queries -----

    bool IsMouseDown(MouseButton btn)     const { return m_mouseDown[static_cast<int>(btn)]; }
    bool IsMousePressed(MouseButton btn)  const { return m_mousePressed[static_cast<int>(btn)]; }
    bool IsMouseReleased(MouseButton btn) const { return m_mouseReleased[static_cast<int>(btn)]; }

    int   GetMouseX()       const { return m_mouseX; }
    int   GetMouseY()       const { return m_mouseY; }
    float GetScrollDelta()  const { return m_scrollDelta; }

    // Mouse position in world space (updated each frame)
    float GetMouseWorldX()  const { return m_mouseWorldX; }
    float GetMouseWorldY()  const { return m_mouseWorldY; }

    // ----- Tile hover / click queries -----

    int  GetHoverTileX()    const { return m_hoverTileX; }
    int  GetHoverTileY()    const { return m_hoverTileY; }
    bool IsHoverTileValid() const { return m_hoverTileValid; }

    bool WasTileClicked()        const { return m_tileClicked; }
    int  GetClickTileX()         const { return m_clickTileX; }
    int  GetClickTileY()         const { return m_clickTileY; }

    bool WasTileRightClicked()   const { return m_tileRightClicked; }
    int  GetRightClickTileX()    const { return m_rightClickTileX; }
    int  GetRightClickTileY()    const { return m_rightClickTileY; }

    // ----- Configuration -----

    void SetCameraPanSpeed(float pixelsPerSec)   { m_cameraPanSpeed   = pixelsPerSec; }
    void SetZoomSensitivity(float s)             { m_zoomSensitivity  = s; }
    void SetZoomRange(float minZ, float maxZ)    { m_zoomMin = minZ; m_zoomMax = maxZ; }
    void SetEdgeScrollEnabled(bool enabled)      { m_edgeScrollEnabled = enabled; }
    void SetEdgeScrollMargin(int pixels)         { m_edgeScrollMargin  = pixels; }
    void SetEdgeScrollSpeed(float pixelsPerSec)  { m_edgeScrollSpeed   = pixelsPerSec; }
    void SetKeyboardPanEnabled(bool enabled)     { m_keyboardPanEnabled = enabled; }

private:
    // Keyboard state
    bool m_keyDown[256]     = {};
    bool m_keyDownPrev[256] = {};
    bool m_keyPressed[256]  = {};
    bool m_keyReleased[256] = {};

    // Mouse button state
    bool m_mouseDown[3]     = {};
    bool m_mouseDownPrev[3] = {};
    bool m_mousePressed[3]  = {};
    bool m_mouseReleased[3] = {};

    // Mouse position (screen pixels)
    int m_mouseX = 0;
    int m_mouseY = 0;
    int m_mouseDeltaX = 0;
    int m_mouseDeltaY = 0;

    // Scroll wheel
    float m_scrollDelta = 0.0f;
    float m_scrollAccum = 0.0f;

    // Middle-mouse drag
    int  m_dragStartX = 0;
    int  m_dragStartY = 0;
    bool m_dragging   = false;

    // Mouse in world space
    float m_mouseWorldX = 0.0f;
    float m_mouseWorldY = 0.0f;

    // Tile hover
    int  m_hoverTileX     = 0;
    int  m_hoverTileY     = 0;
    bool m_hoverTileValid = false;

    // Tile click (this frame)
    bool m_tileClicked       = false;
    int  m_clickTileX        = 0;
    int  m_clickTileY        = 0;
    bool m_tileRightClicked  = false;
    int  m_rightClickTileX   = 0;
    int  m_rightClickTileY   = 0;
    int  m_rightClickStartX  = 0;
    int  m_rightClickStartY  = 0;

    // Camera control tuning
    float m_cameraPanSpeed   = 600.0f;  // Pixels per second at zoom=1
    float m_zoomSensitivity  = 0.15f;   // Per scroll notch
    float m_zoomMin          = 0.25f;
    float m_zoomMax          = 4.0f;

    // Keyboard pan toggle
    bool  m_keyboardPanEnabled = true;

    // Edge-scroll tuning
    bool  m_edgeScrollEnabled = true;
    int   m_edgeScrollMargin  = 20;     // Pixels from window edge
    float m_edgeScrollSpeed   = 500.0f; // Pixels per second at zoom=1
};

} // namespace engine
