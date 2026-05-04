// BitmapFont.cpp - Generate font atlas from Windows GDI

#include "BitmapFont.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <vector>
#include <cstring>

namespace ui
{

bool BitmapFont::Create(ID3D12Device* device,
                        ID3D12GraphicsCommandList* cmdList,
                        engine::UploadManager& uploadMgr,
                        engine::PersistentDescriptorAllocator& srvHeap,
                        const char* fontName,
                        int fontSize,
                        bool bold)
{
    // --- Step 1: Create a GDI device context and font ---

    HDC hdc = CreateCompatibleDC(nullptr);
    if (!hdc)
        return false;

    int weight = bold ? FW_BOLD : FW_NORMAL;
    HFONT hFont = CreateFontA(
        -fontSize,          // Height (negative = character height)
        0,                  // Width (0 = auto)
        0, 0,               // Escapement, orientation
        weight,
        FALSE, FALSE, FALSE,
        ANSI_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontName);

    if (!hFont)
    {
        DeleteDC(hdc);
        return false;
    }

    HFONT oldFont = static_cast<HFONT>(SelectObject(hdc, hFont));

    // --- Step 2: Measure all glyphs to determine atlas size ---

    TEXTMETRICA tm;
    GetTextMetricsA(hdc, &tm);
    int cellHeight = tm.tmHeight;
    m_lineHeight = static_cast<float>(cellHeight);

    // Measure individual character widths
    int charWidths[kCharCount];
    int totalWidth = 0;
    for (int i = 0; i < kCharCount; ++i)
    {
        char ch = static_cast<char>(kFirstChar + i);
        SIZE sz;
        GetTextExtentPoint32A(hdc, &ch, 1, &sz);
        charWidths[i] = sz.cx;
        totalWidth += sz.cx + 2; // 2px padding between glyphs
    }

    // Pick atlas dimensions: aim for roughly square
    int atlasWidth = 256;
    while (atlasWidth < totalWidth)
        atlasWidth *= 2;

    // Calculate rows needed
    int cursorX = 0;
    int rows = 1;
    for (int i = 0; i < kCharCount; ++i)
    {
        int cw = charWidths[i] + 2;
        if (cursorX + cw > atlasWidth)
        {
            cursorX = 0;
            ++rows;
        }
        cursorX += cw;
    }

    int atlasHeight = 1;
    while (atlasHeight < rows * (cellHeight + 2))
        atlasHeight *= 2;

    // --- Step 3: Render glyphs into a DIB section ---

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = atlasWidth;
    bmi.bmiHeader.biHeight      = -atlasHeight; // Top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* dibBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &dibBits, nullptr, 0);
    if (!hBitmap)
    {
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        DeleteDC(hdc);
        return false;
    }

    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdc, hBitmap));

    // Clear to black
    std::memset(dibBits, 0, atlasWidth * atlasHeight * 4);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    // Render each glyph
    cursorX = 0;
    int cursorY = 0;
    float invW = 1.0f / static_cast<float>(atlasWidth);
    float invH = 1.0f / static_cast<float>(atlasHeight);

    for (int i = 0; i < kCharCount; ++i)
    {
        int cw = charWidths[i] + 2;
        if (cursorX + cw > atlasWidth)
        {
            cursorX = 0;
            cursorY += cellHeight + 2;
        }

        char ch = static_cast<char>(kFirstChar + i);
        TextOutA(hdc, cursorX + 1, cursorY + 1, &ch, 1);

        // Store glyph info
        m_glyphs[i].u0 = static_cast<float>(cursorX + 1) * invW;
        m_glyphs[i].v0 = static_cast<float>(cursorY + 1) * invH;
        m_glyphs[i].u1 = static_cast<float>(cursorX + 1 + charWidths[i]) * invW;
        m_glyphs[i].v1 = static_cast<float>(cursorY + 1 + cellHeight) * invH;
        m_glyphs[i].widthPx  = static_cast<float>(charWidths[i]);
        m_glyphs[i].heightPx = static_cast<float>(cellHeight);
        m_glyphs[i].advanceX = static_cast<float>(charWidths[i]);

        cursorX += cw;
    }

    GdiFlush();

    // --- Step 4: Convert DIB to RGBA with proper alpha ---

    std::vector<uint32_t> rgba(atlasWidth * atlasHeight);
    const uint32_t* src = static_cast<const uint32_t*>(dibBits);

    for (int i = 0; i < atlasWidth * atlasHeight; ++i)
    {
        // GDI ClearType renders colored subpixels. For a white-on-black render,
        // take the max channel as alpha, and set RGB to white.
        uint32_t pixel = src[i];
        uint8_t r = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = pixel & 0xFF;
        uint8_t a = r > g ? (r > b ? r : b) : (g > b ? g : b);

        // Premultiplied: store white * alpha
        rgba[i] = (static_cast<uint32_t>(a) << 24) |
                  (static_cast<uint32_t>(a) << 16) |
                  (static_cast<uint32_t>(a) << 8) |
                  static_cast<uint32_t>(a);
    }

    // --- Step 5: Upload to GPU texture ---

    bool ok = m_texture.CreateFromRGBA(device, cmdList, uploadMgr, srvHeap,
                                       static_cast<uint32_t>(atlasWidth),
                                       static_cast<uint32_t>(atlasHeight),
                                       rgba.data());
    if (!ok)
    {
        OutputDebugStringA("[BitmapFont] ERROR: failed to upload font atlas texture to GPU. Text will not render.\n");
    }

    // --- Cleanup GDI ---

    SelectObject(hdc, oldBitmap);
    SelectObject(hdc, oldFont);
    DeleteObject(hBitmap);
    DeleteObject(hFont);
    DeleteDC(hdc);

    return ok;
}

void BitmapFont::Shutdown(engine::PersistentDescriptorAllocator& srvHeap)
{
    m_texture.Shutdown(srvHeap);
}

const GlyphInfo& BitmapFont::GetGlyph(char ch) const
{
    int idx = static_cast<int>(ch) - kFirstChar;
    if (idx < 0 || idx >= kCharCount)
        idx = 0; // Space
    return m_glyphs[idx];
}

float BitmapFont::MeasureWidth(const char* text) const
{
    float w = 0.0f;
    while (*text)
    {
        w += GetGlyph(*text).advanceX;
        ++text;
    }
    return w;
}

} // namespace ui
