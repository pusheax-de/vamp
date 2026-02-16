#pragma once
// RenderQueue.h - Sortable render item collection for batched drawing

#include "EngineTypes.h"
#include <vector>
#include <algorithm>

namespace engine
{

// ---------------------------------------------------------------------------
// RenderItem - a single draw command in the queue
// ---------------------------------------------------------------------------
struct RenderItem
{
    RenderSortKey   sortKey;
    SpriteInstance  instance;
};

// ---------------------------------------------------------------------------
// RenderQueue - collects and sorts render items for a frame
// ---------------------------------------------------------------------------
class RenderQueue
{
public:
    void Clear()
    {
        m_items.clear();
    }

    void Submit(RenderLayer layer, float ySort, uint16_t priority,
                uint16_t materialId, const SpriteInstance& instance)
    {
        RenderItem item;
        item.sortKey  = RenderSortKey::Make(layer, ySort, priority, materialId);
        item.instance = instance;
        m_items.push_back(item);
    }

    void Sort()
    {
        std::sort(m_items.begin(), m_items.end(),
            [](const RenderItem& a, const RenderItem& b)
            {
                return a.sortKey < b.sortKey;
            });
    }

    const std::vector<RenderItem>& GetItems() const { return m_items; }
    size_t GetCount() const { return m_items.size(); }
    bool   IsEmpty() const { return m_items.empty(); }

    // Get a range of items for a specific layer
    struct LayerRange
    {
        size_t begin;
        size_t end;
    };

    LayerRange GetLayerRange(RenderLayer layer) const
    {
        uint64_t layerBits = static_cast<uint64_t>(layer) << 56;
        uint64_t nextLayerBits = (static_cast<uint64_t>(layer) + 1) << 56;

        LayerRange range = { m_items.size(), m_items.size() };

        for (size_t i = 0; i < m_items.size(); ++i)
        {
            if (m_items[i].sortKey.key >= layerBits && m_items[i].sortKey.key < nextLayerBits)
            {
                if (range.begin == m_items.size())
                    range.begin = i;
                range.end = i + 1;
            }
        }

        return range;
    }

private:
    std::vector<RenderItem> m_items;
};

} // namespace engine
