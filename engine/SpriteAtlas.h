#pragma once
// SpriteAtlas.h - Sprite atlas for batched icon/character/tile rendering

#include "EngineTypes.h"
#include "Texture2D.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace engine
{

// ---------------------------------------------------------------------------
// Sprite region within an atlas
// ---------------------------------------------------------------------------
struct SpriteRegion
{
    float u0, v0, u1, v1;      // UV coordinates in the atlas
    float widthPixels;          // Original pixel width
    float heightPixels;         // Original pixel height
    float pivotX, pivotY;       // Pivot offset (0.5, 0.5 = center)
};

// ---------------------------------------------------------------------------
// SpriteAtlas - manages a texture atlas with named sprite regions
// ---------------------------------------------------------------------------
class SpriteAtlas
{
public:
    // Set the backing texture (must already be loaded)
    void SetTexture(Texture2D* texture)
    {
        m_texture = texture;
        if (texture && texture->IsValid())
        {
            m_atlasWidth  = static_cast<float>(texture->GetWidth());
            m_atlasHeight = static_cast<float>(texture->GetHeight());
        }
    }

    // Define a sprite region by name (pixel coordinates)
    void AddSprite(const std::string& name,
                   float x, float y, float w, float h,
                   float pivotX = 0.5f, float pivotY = 0.5f)
    {
        SpriteRegion region;
        region.u0 = x / m_atlasWidth;
        region.v0 = y / m_atlasHeight;
        region.u1 = (x + w) / m_atlasWidth;
        region.v1 = (y + h) / m_atlasHeight;
        region.widthPixels  = w;
        region.heightPixels = h;
        region.pivotX = pivotX;
        region.pivotY = pivotY;

        m_sprites[name] = region;
    }

    // Define a uniform grid of sprites (e.g., 16x16 character sheet)
    void AddGrid(const std::string& prefix,
                 float startX, float startY,
                 float cellW, float cellH,
                 int cols, int rows,
                 float pivotX = 0.5f, float pivotY = 0.5f)
    {
        for (int r = 0; r < rows; ++r)
        {
            for (int c = 0; c < cols; ++c)
            {
                std::string name = prefix + "_" + std::to_string(r * cols + c);
                AddSprite(name,
                          startX + c * cellW, startY + r * cellH,
                          cellW, cellH,
                          pivotX, pivotY);
            }
        }
    }

    // Look up a sprite by name
    const SpriteRegion* GetSprite(const std::string& name) const
    {
        auto it = m_sprites.find(name);
        if (it != m_sprites.end())
            return &it->second;
        return nullptr;
    }

    Texture2D*  GetTexture() const { return m_texture; }
    uint32_t    GetSRVIndex() const { return m_texture ? m_texture->GetSRVIndex() : UINT32_MAX; }

private:
    Texture2D*  m_texture       = nullptr;
    float       m_atlasWidth    = 1.0f;
    float       m_atlasHeight   = 1.0f;

    std::unordered_map<std::string, SpriteRegion> m_sprites;
};

} // namespace engine
