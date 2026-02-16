#pragma once
// LightSystem.h - Dynamic 2D point lights with shadow casting

#include "EngineTypes.h"
#include "OccluderSet.h"
#include <vector>
#include <DirectXMath.h>
#include <cmath>

namespace engine
{

// ---------------------------------------------------------------------------
// ShadowQuad - extruded shadow volume from an occluder edge
// ---------------------------------------------------------------------------
struct ShadowQuad
{
    DirectX::XMFLOAT2 v[4]; // Four vertices of the shadow polygon
};

// ---------------------------------------------------------------------------
// LightSystem - manages dynamic lights and builds shadow geometry
// ---------------------------------------------------------------------------
class LightSystem
{
public:
    void Clear()
    {
        m_lights.clear();
    }

    void AddLight(float x, float y, float r, float g, float b,
                  float radius, float intensity = 1.0f, float flickerPhase = 0.0f)
    {
        if (m_lights.size() >= kMaxLights)
            return;

        PointLight2D light;
        light.position   = { x, y };
        light.color      = { r, g, b };
        light.radius     = radius;
        light.intensity  = intensity;
        light.flickerPhase = flickerPhase;
        m_lights.push_back(light);
    }

    // Update flicker animation
    void Update(float time)
    {
        for (auto& light : m_lights)
        {
            if (light.flickerPhase > 0.0f)
            {
                // Simple flicker: modulate intensity with sin + noise-like wobble
                float flicker = 0.85f + 0.15f * std::sin(time * 8.0f + light.flickerPhase * 6.28f);
                light.intensity = flicker;
            }
        }
    }

    // Build shadow quads for a specific light against occluders
    std::vector<ShadowQuad> BuildShadowGeometry(const PointLight2D& light,
                                                  const OccluderSet& occluders,
                                                  float extrudeDistance = 1000.0f) const
    {
        std::vector<ShadowQuad> shadows;

        auto nearby = occluders.GetSegmentsInRadius(light.position.x, light.position.y,
                                                     light.radius + extrudeDistance);

        for (const auto* seg : nearby)
        {
            // Check if segment faces away from the light (back-face test)
            float edgeDX = seg->b.x - seg->a.x;
            float edgeDY = seg->b.y - seg->a.y;
            float normalX = -edgeDY; // Left-hand normal
            float normalY = edgeDX;

            float midX = (seg->a.x + seg->b.x) * 0.5f;
            float midY = (seg->a.y + seg->b.y) * 0.5f;
            float toLightX = light.position.x - midX;
            float toLightY = light.position.y - midY;

            float dot = normalX * toLightX + normalY * toLightY;
            if (dot >= 0.0f)
                continue; // Faces toward light — no shadow

            // Extrude endpoints away from light to create shadow quad
            ShadowQuad quad;
            quad.v[0] = seg->a;
            quad.v[1] = seg->b;

            // Direction from light to each endpoint
            float dirAX = seg->a.x - light.position.x;
            float dirAY = seg->a.y - light.position.y;
            float lenA = std::sqrt(dirAX * dirAX + dirAY * dirAY);
            if (lenA > 0.0f) { dirAX /= lenA; dirAY /= lenA; }

            float dirBX = seg->b.x - light.position.x;
            float dirBY = seg->b.y - light.position.y;
            float lenB = std::sqrt(dirBX * dirBX + dirBY * dirBY);
            if (lenB > 0.0f) { dirBX /= lenB; dirBY /= lenB; }

            quad.v[2] = { seg->b.x + dirBX * extrudeDistance,
                          seg->b.y + dirBY * extrudeDistance };
            quad.v[3] = { seg->a.x + dirAX * extrudeDistance,
                          seg->a.y + dirAY * extrudeDistance };

            shadows.push_back(quad);
        }

        return shadows;
    }

    const std::vector<PointLight2D>& GetLights() const { return m_lights; }
    size_t GetLightCount() const { return m_lights.size(); }

private:
    std::vector<PointLight2D> m_lights;
};

} // namespace engine
