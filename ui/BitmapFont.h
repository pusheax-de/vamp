#pragma once
// BitmapFont.h - Runtime bitmap font atlas generated from Windows GDI
//
// Renders ASCII glyphs (32-126) into an RGBA texture atlas at startup,
// then provides glyph metrics and UV rects for text rendering.

#include "UITypes.h"
#include <EngineTypes.h>
#include <Texture2D.h>
#include <DescriptorAllocator.h>
#include <UploadManager.h>
#include <d3d12.h>
#include <string>
#include <cstdint>

namespace ui
{

// ---------------------------------------------------------------------------
// Glyph metrics for a single character
// ---------------------------------------------------------------------------
struct GlyphInfo
{
    float u0, v0, u1, v1;  // UV coordinates in atlas
    float widthPx;          // Pixel width of this glyph
    float heightPx;         // Pixel height (same for all glyphs in a font)
    float advanceX;         // Horizontal advance to next character
};

// ---------------------------------------------------------------------------
// BitmapFont - owns a texture atlas and glyph table for one font/size
// ---------------------------------------------------------------------------
class BitmapFont
{
public:
    static constexpr int kFirstChar = 32;   // Space
    static constexpr int kLastChar  = 126;  // Tilde
    static constexpr int kCharCount = kLastChar - kFirstChar + 1;

    // Generate the font atlas from a system font.
    // Call once during initialization with an active command list.
    bool Create(ID3D12Device* device,
                ID3D12GraphicsCommandList* cmdList,
                engine::UploadManager& uploadMgr,
                engine::PersistentDescriptorAllocator& srvHeap,
                const char* fontName = "Consolas",
                int fontSize = 16,
                bool bold = false);

    void Shutdown(engine::PersistentDescriptorAllocator& srvHeap);

    // Query glyph info for a character (returns space glyph for out-of-range)
    const GlyphInfo& GetGlyph(char ch) const;

    // Measure a string in pixels
    float MeasureWidth(const char* text) const;
    float GetLineHeight() const { return m_lineHeight; }

    // Texture access
    uint32_t GetSRVIndex() const { return m_texture.GetSRVIndex(); }
    bool     IsValid() const { return m_texture.IsValid(); }

private:
    engine::Texture2D   m_texture;
    GlyphInfo           m_glyphs[kCharCount];
    float               m_lineHeight = 0.0f;
};

} // namespace ui
