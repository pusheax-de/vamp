#pragma once
// Armor.h - Armor definitions providing damage reduction

#include <cstdint>

namespace vamp
{

enum class ArmorType : uint8_t
{
    None,
    LightClothing,      // Leather jacket, hoodie
    KevlarVest,         // Standard ballistic vest
    TacticalArmor,      // Police/merc tactical gear
    HeavyArmor,         // Rare heavy plate carrier
    HemocraftCoating,   // Vampire blood armor (temporary, stacks via status)
    COUNT
};

struct ArmorData
{
    ArmorType   type;
    const char* name;
    int         damageReduction;    // Flat DR subtracted from incoming damage
    int         noisePenalty;       // Added to stealth noise (heavier = louder)
    int         movePenalty;        // Extra AP per tile of movement (0 or 1)
    int         agiPenalty;         // Penalty to AGI-based checks
};

inline const ArmorData& GetArmorData(ArmorType type)
{
    static const ArmorData table[] = {
        { ArmorType::None,              "No Armor",           0, 0, 0, 0 },
        { ArmorType::LightClothing,     "Light Clothing",     1, 0, 0, 0 },
        { ArmorType::KevlarVest,        "Kevlar Vest",        3, 1, 0, 0 },
        { ArmorType::TacticalArmor,     "Tactical Armor",     5, 2, 0, 1 },
        { ArmorType::HeavyArmor,        "Heavy Armor",        7, 3, 1, 2 },
        { ArmorType::HemocraftCoating,  "Hemocraft Coating",  4, 0, 0, 0 },
    };
    return table[static_cast<int>(type)];
}

} // namespace vamp
