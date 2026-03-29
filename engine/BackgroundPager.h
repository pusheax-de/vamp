#pragma once
// BackgroundPager.h - Paged background streaming with LRU GPU cache

#include "EngineTypes.h"
#include "Texture2D.h"
#include "Camera2D.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace engine
{

// Forward declarations
class PersistentDescriptorAllocator;
class UploadManager;

// ---------------------------------------------------------------------------
// BackgroundPager - manages paged background texture streaming
// ---------------------------------------------------------------------------
class BackgroundPager
{
public:
    struct PageKey
    {
        int32_t x, y;
        bool operator==(const PageKey& other) const { return x == other.x && y == other.y; }
    };

    struct PageKeyHash
    {
        size_t operator()(const PageKey& k) const
        {
            return std::hash<int64_t>()(static_cast<int64_t>(k.x) << 32 | (k.y & 0xFFFFFFFF));
        }
    };

    void Init(const std::wstring& sceneBasePath, int totalPagesX, int totalPagesY,
              uint32_t pageSizePixels = kPageSizePixels);

    void Shutdown(PersistentDescriptorAllocator& srvHeap);

    // --- Single background image support ---

    // Set a single background image that covers the specified world area.
    // The image is loaded from a PNG file and stretched to fit the given bounds.
    // Call after the renderer is initialized and a command list is recording.
    bool SetBackgroundImage(ID3D12Device* device,
                            ID3D12GraphicsCommandList* cmdList,
                            UploadManager& uploadMgr,
                            PersistentDescriptorAllocator& srvHeap,
                            const std::wstring& imagePath,
                            float worldX, float worldY,
                            float worldW, float worldH);

    // Check if a background image is set
    bool HasBackgroundImage() const { return m_bgImageReady; }

    // Get background image info for rendering
    struct BackgroundImage
    {
        float       worldX, worldY;
        float       worldW, worldH;
        uint32_t    srvIndex;
    };
    bool GetBackgroundImage(BackgroundImage& out) const;

    // Call each frame: determines which pages need to be loaded/evicted
    void Update(const Camera2D& camera, uint32_t frameNumber);

    // Load pending pages (call with a command list that's recording)
    void ProcessPendingLoads(ID3D12Device* device,
                             ID3D12GraphicsCommandList* cmdList,
                             UploadManager& uploadMgr,
                             PersistentDescriptorAllocator& srvHeap);

    // Get loaded pages for rendering
    struct LoadedPage
    {
        float       worldX, worldY;     // Top-left corner in world space
        float       worldW, worldH;     // Page size in world space
        uint32_t    srvIndex;           // SRV index for the texture
    };

    std::vector<LoadedPage> GetVisiblePages() const;

    int GetTotalPagesX() const { return m_totalPagesX; }
    int GetTotalPagesY() const { return m_totalPagesY; }

private:
    struct CachedPage
    {
        Texture2D   texture;
        PageKey     key;
        uint32_t    lastUsedFrame;
        bool        loaded;
    };

    std::wstring    m_basePath;
    int             m_totalPagesX       = 0;
    int             m_totalPagesY       = 0;
    uint32_t        m_pageSizePixels    = kPageSizePixels;
    uint32_t        m_currentFrame      = 0;

    // GPU cache: limited number of pages resident at once
    static constexpr uint32_t kMaxCachedPages = 64;
    std::vector<CachedPage>                         m_cache;
    std::unordered_map<PageKey, uint32_t, PageKeyHash>  m_pageToCache;

    // Pages that need loading this frame
    std::vector<PageKey> m_pendingLoads;

    // Visible pages (determined during Update)
    std::vector<PageKey> m_visibleKeys;

    // Single background image
    Texture2D   m_bgImageTex;
    float       m_bgImageWorldX = 0.0f;
    float       m_bgImageWorldY = 0.0f;
    float       m_bgImageWorldW = 0.0f;
    float       m_bgImageWorldH = 0.0f;
    bool        m_bgImageReady  = false;

    uint32_t FindOrEvictCacheSlot(const PageKey& key);
    std::wstring GetPageFilePath(int px, int py) const;
};

} // namespace engine
