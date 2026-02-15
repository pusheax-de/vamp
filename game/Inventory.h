#pragma once
// Inventory.h - Item ownership, equipment slots, ammo tracking

#include "Weapon.h"
#include "Armor.h"
#include <vector>
#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// Generic item type
// ---------------------------------------------------------------------------
enum class ItemType : uint8_t
{
    Weapon,
    Armor,
    Ammo,
    Consumable,     // Bandage, stim, blood vial
    KeyItem,        // Quest items, keycards
    Misc,
    COUNT
};

// ---------------------------------------------------------------------------
// A single item instance
// ---------------------------------------------------------------------------
struct Item
{
    ItemType    type        = ItemType::Misc;
    uint16_t    templateId  = 0;        // Index into a per-type table
    int         quantity    = 1;
    const char* name        = "Unknown";
};

// ---------------------------------------------------------------------------
// Consumable sub-types
// ---------------------------------------------------------------------------
enum class ConsumableType : uint8_t
{
    Bandage,        // Stop bleeding
    Stimpack,       // Heal HP
    BloodVial,      // Restore BR
    Antidote,       // Cure poison
    Flashbang,      // Stun enemies in radius
    COUNT
};

struct ConsumableData
{
    ConsumableType  type;
    const char*     name;
    int             apCost;
    int             healAmount;     // HP restored (0 if not applicable)
    int             brRestored;     // Blood Reserve restored (0 if not applicable)
    int             effectRadius;   // 0 = self only
};

inline const ConsumableData& GetConsumableData(ConsumableType type)
{
    static const ConsumableData table[] = {
        { ConsumableType::Bandage,   "Bandage",    2,  0, 0, 0 },
        { ConsumableType::Stimpack,  "Stimpack",   2,  4, 0, 0 },
        { ConsumableType::BloodVial, "Blood Vial", 1,  0, 2, 0 },
        { ConsumableType::Antidote,  "Antidote",   2,  0, 0, 0 },
        { ConsumableType::Flashbang, "Flashbang",  2,  0, 0, 3 },
    };
    return table[static_cast<int>(type)];
}

// ---------------------------------------------------------------------------
// Equipment slots
// ---------------------------------------------------------------------------
struct Equipment
{
    WeaponType  primaryWeapon   = WeaponType::Unarmed;
    WeaponType  secondaryWeapon = WeaponType::Unarmed;
    ArmorType   armor           = ArmorType::None;
    int         primaryAmmo     = 0;
    int         secondaryAmmo   = 0;

    const WeaponData& GetPrimaryWeapon() const  { return GetWeaponData(primaryWeapon); }
    const WeaponData& GetSecondaryWeapon() const { return GetWeaponData(secondaryWeapon); }
    const ArmorData&  GetArmor() const          { return GetArmorData(armor); }
};

// ---------------------------------------------------------------------------
// Inventory: items the character is carrying
// ---------------------------------------------------------------------------
struct Inventory
{
    Equipment           equipment;
    std::vector<Item>   backpack;
    int                 money = 0;

    void AddItem(const Item& item);
    bool RemoveItem(uint16_t templateId, int quantity = 1);
    bool HasItem(uint16_t templateId) const;
    int  CountItem(uint16_t templateId) const;
};

} // namespace vamp
