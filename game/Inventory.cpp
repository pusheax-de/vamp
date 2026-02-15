// Inventory.cpp - Inventory management implementation

#include "Inventory.h"
#include <algorithm>

namespace vamp
{

void Inventory::AddItem(const Item& item)
{
    // Try to stack if same templateId and type
    for (auto& existing : backpack)
    {
        if (existing.templateId == item.templateId && existing.type == item.type)
        {
            existing.quantity += item.quantity;
            return;
        }
    }
    backpack.push_back(item);
}

bool Inventory::RemoveItem(uint16_t templateId, int quantity)
{
    for (auto it = backpack.begin(); it != backpack.end(); ++it)
    {
        if (it->templateId == templateId)
        {
            if (it->quantity > quantity)
            {
                it->quantity -= quantity;
                return true;
            }
            else if (it->quantity == quantity)
            {
                backpack.erase(it);
                return true;
            }
            return false; // Not enough quantity
        }
    }
    return false;
}

bool Inventory::HasItem(uint16_t templateId) const
{
    return CountItem(templateId) > 0;
}

int Inventory::CountItem(uint16_t templateId) const
{
    for (const auto& item : backpack)
    {
        if (item.templateId == templateId)
            return item.quantity;
    }
    return 0;
}

} // namespace vamp
