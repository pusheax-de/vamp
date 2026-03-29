// BackgroundPager.cpp - Paged background streaming implementation

#include "BackgroundPager.h"
#include "DescriptorAllocator.h"
#include "UploadManager.h"
#include <algorithm>
#include <cmath>

namespace engine
{

void BackgroundPager::Init(const std::wstring& sceneBasePath,
                            int totalPagesX, int totalPagesY,
                            uint32_t pageSizePixels)
{
    m_basePath       = sceneBasePath;
    m_totalPagesX    = totalPagesX;
    m_totalPagesY    = totalPagesY;
    m_pageSizePixels = pageSizePixels;
    m_currentFrame   = 0;

    m_cache.resize(kMaxCachedPages);
    for (auto& slot : m_cache)
    {
        slot.loaded = false;
        slot.lastUsedFrame = 0;
        slot.key = { -1, -1 };
    }
    m_pageToCache.clear();
    m_pendingLoads.clear();
    m_visibleKeys.clear();
}

void BackgroundPager::Shutdown(PersistentDescriptorAllocator& srvHeap)
{
    for (auto& slot : m_cache)
    {
        if (slot.loaded)
            slot.texture.Shutdown(srvHeap);
    }
    m_cache.clear();
    m_pageToCache.clear();

    if (m_bgImageReady)
    {
        m_bgImageTex.Shutdown(srvHeap);
        m_bgImageReady = false;
    }
}

void BackgroundPager::Update(const Camera2D& camera, uint32_t frameNumber)
{
    m_currentFrame = frameNumber;
    m_visibleKeys.clear();
    m_pendingLoads.clear();

    // Determine which pages are visible (with 1-page margin for prefetch)
    auto bounds = camera.GetViewBounds();
    float pageWorld = static_cast<float>(m_pageSizePixels);

    int minPX = static_cast<int>(std::floor(bounds.left / pageWorld)) - 1;
    int minPY = static_cast<int>(std::floor(bounds.top / pageWorld)) - 1;
    int maxPX = static_cast<int>(std::floor(bounds.right / pageWorld)) + 1;
    int maxPY = static_cast<int>(std::floor(bounds.bottom / pageWorld)) + 1;

    minPX = (std::max)(0, minPX);
    minPY = (std::max)(0, minPY);
    maxPX = (std::min)(m_totalPagesX - 1, maxPX);
    maxPY = (std::min)(m_totalPagesY - 1, maxPY);

    for (int py = minPY; py <= maxPY; ++py)
    {
        for (int px = minPX; px <= maxPX; ++px)
        {
            PageKey key = { px, py };
            m_visibleKeys.push_back(key);

            auto it = m_pageToCache.find(key);
            if (it != m_pageToCache.end())
            {
                // Already cached — mark as used
                m_cache[it->second].lastUsedFrame = m_currentFrame;
            }
            else
            {
                // Need to load
                m_pendingLoads.push_back(key);
            }
        }
    }
}

void BackgroundPager::ProcessPendingLoads(ID3D12Device* device,
                                           ID3D12GraphicsCommandList* cmdList,
                                           UploadManager& uploadMgr,
                                           PersistentDescriptorAllocator& srvHeap)
{
    // Limit loads per frame to avoid stalls
    constexpr uint32_t kMaxLoadsPerFrame = 4;
    uint32_t loadCount = 0;

    for (const auto& key : m_pendingLoads)
    {
        if (loadCount >= kMaxLoadsPerFrame)
            break;

        uint32_t slot = FindOrEvictCacheSlot(key);
        if (slot == UINT32_MAX)
            continue;

        // Evict old texture if needed
        if (m_cache[slot].loaded)
        {
            m_pageToCache.erase(m_cache[slot].key);
            m_cache[slot].texture.Shutdown(srvHeap);
            m_cache[slot].loaded = false;
        }

        // Load new page
        std::wstring filePath = GetPageFilePath(key.x, key.y);
        bool loaded = m_cache[slot].texture.LoadFromDDS(device, cmdList, uploadMgr,
                                                         srvHeap, filePath);
        if (loaded)
        {
            m_cache[slot].key           = key;
            m_cache[slot].lastUsedFrame = m_currentFrame;
            m_cache[slot].loaded        = true;
            m_pageToCache[key]          = slot;
            ++loadCount;
        }
    }
}

std::vector<BackgroundPager::LoadedPage> BackgroundPager::GetVisiblePages() const
{
    std::vector<LoadedPage> result;
    float pageWorld = static_cast<float>(m_pageSizePixels);

    for (const auto& key : m_visibleKeys)
    {
        auto it = m_pageToCache.find(key);
        if (it == m_pageToCache.end())
            continue;

        const auto& slot = m_cache[it->second];
        if (!slot.loaded)
            continue;

        LoadedPage page;
        page.worldX   = key.x * pageWorld;
        page.worldY   = key.y * pageWorld;
        page.worldW   = pageWorld;
        page.worldH   = pageWorld;
        page.srvIndex = slot.texture.GetSRVIndex();
        result.push_back(page);
    }

    return result;
}

bool BackgroundPager::SetBackgroundImage(ID3D12Device* device,
                                          ID3D12GraphicsCommandList* cmdList,
                                          UploadManager& uploadMgr,
                                          PersistentDescriptorAllocator& srvHeap,
                                          const std::wstring& imagePath,
                                          float worldX, float worldY,
                                          float worldW, float worldH)
{
    if (m_bgImageReady)
    {
        m_bgImageTex.Shutdown(srvHeap);
        m_bgImageReady = false;
    }

    if (!m_bgImageTex.LoadFromPNG(device, cmdList, uploadMgr, srvHeap, imagePath))
        return false;

    m_bgImageWorldX = worldX;
    m_bgImageWorldY = worldY;
    m_bgImageWorldW = worldW;
    m_bgImageWorldH = worldH;
    m_bgImageReady  = true;
    return true;
}

bool BackgroundPager::GetBackgroundImage(BackgroundImage& out) const
{
    if (!m_bgImageReady || !m_bgImageTex.IsValid())
        return false;

    out.worldX   = m_bgImageWorldX;
    out.worldY   = m_bgImageWorldY;
    out.worldW   = m_bgImageWorldW;
    out.worldH   = m_bgImageWorldH;
    out.srvIndex = m_bgImageTex.GetSRVIndex();
    return true;
}

uint32_t BackgroundPager::FindOrEvictCacheSlot(const PageKey& key)
{
    // Find empty slot
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_cache.size()); ++i)
    {
        if (!m_cache[i].loaded)
            return i;
    }

    // Evict LRU
    uint32_t lruIndex = 0;
    uint32_t lruFrame = UINT32_MAX;
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_cache.size()); ++i)
    {
        // Don't evict currently visible pages
        bool isVisible = false;
        for (const auto& vk : m_visibleKeys)
        {
            if (vk == m_cache[i].key) { isVisible = true; break; }
        }
        if (isVisible)
            continue;

        if (m_cache[i].lastUsedFrame < lruFrame)
        {
            lruFrame = m_cache[i].lastUsedFrame;
            lruIndex = i;
        }
    }

    if (lruFrame == UINT32_MAX)
        return UINT32_MAX; // All slots are visible — can't evict

    return lruIndex;
}

std::wstring BackgroundPager::GetPageFilePath(int px, int py) const
{
    // Convention: basepath/page_X_Y.dds
    return m_basePath + L"/page_" + std::to_wstring(px) + L"_" + std::to_wstring(py) + L".dds";
}

} // namespace engine
