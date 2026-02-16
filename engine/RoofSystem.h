#pragma once
// RoofSystem.h - Roof footprints, hide/fade logic for building interiors

#include "EngineTypes.h"
#include <vector>
#include <cmath>

namespace engine
{

// ---------------------------------------------------------------------------
// RoofEntry - a single roof with its footprint and rendering state
// ---------------------------------------------------------------------------
struct RoofEntry
{
    RoofFootprint   footprint;
    float           targetAlpha;    // 0.0 = fully hidden, 1.0 = visible
    float           fadeSpeed;      // Alpha change per second

    RoofEntry()
        : targetAlpha(1.0f)
        , fadeSpeed(4.0f)           // ~0.25s to fade
    {}
};

// ---------------------------------------------------------------------------
// RoofSystem - manages all roofs in a scene
// ---------------------------------------------------------------------------
class RoofSystem
{
public:
    void Clear() { m_roofs.clear(); }

    void AddRoof(int x0, int y0, int x1, int y1, TextureHandle texture)
    {
        RoofEntry entry;
        entry.footprint.x0 = x0;
        entry.footprint.y0 = y0;
        entry.footprint.x1 = x1;
        entry.footprint.y1 = y1;
        entry.footprint.roofTexture  = texture;
        entry.footprint.currentAlpha = 1.0f;
        entry.footprint.isInterior   = false;
        m_roofs.push_back(entry);
    }

    // Call each frame to update fade state based on player tile position
    void Update(float deltaTime, int playerTileX, int playerTileY)
    {
        for (auto& roof : m_roofs)
        {
            bool inside = (playerTileX >= roof.footprint.x0 &&
                           playerTileX <= roof.footprint.x1 &&
                           playerTileY >= roof.footprint.y0 &&
                           playerTileY <= roof.footprint.y1);

            roof.footprint.isInterior = inside;
            roof.targetAlpha = inside ? 0.0f : 1.0f;

            // Smooth fade
            float diff = roof.targetAlpha - roof.footprint.currentAlpha;
            if (std::fabs(diff) > 0.001f)
            {
                float step = roof.fadeSpeed * deltaTime;
                if (diff > 0.0f)
                    roof.footprint.currentAlpha += (step < diff) ? step : diff;
                else
                    roof.footprint.currentAlpha += (-step > diff) ? -step : diff;
            }
            else
            {
                roof.footprint.currentAlpha = roof.targetAlpha;
            }
        }
    }

    const std::vector<RoofEntry>& GetRoofs() const { return m_roofs; }
    size_t GetCount() const { return m_roofs.size(); }

private:
    std::vector<RoofEntry> m_roofs;
};

} // namespace engine
