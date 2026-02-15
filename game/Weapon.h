#pragma once
// Weapon.h - Weapon definitions (modern guns, melee, vampire conjured)

#include <cstdint>

namespace vamp
{

enum class WeaponType : uint8_t
{
    // Melee
    Knife,
    Bat,
    Machete,
    Sword,

    // Pistols
    Pistol9mm,
    Revolver,

    // SMGs
    SMG,

    // Rifles
    AssaultRifle,
    SniperRifle,

    // Shotguns
    Shotgun,

    // Special / Vampire
    BloodSpike,     // Hemocraft conjured ranged weapon

    // Thrown
    Grenade,
    Molotov,

    Unarmed,
    COUNT
};

// ---------------------------------------------------------------------------
// Weapon stats
// ---------------------------------------------------------------------------
struct WeaponData
{
    WeaponType  type;
    const char* name;
    int         damageDice;     // Number of d6
    int         damageBonus;    // Flat bonus to damage
    int         apCostSingle;   // AP to fire/swing once
    int         apCostBurst;    // AP for burst fire (0 if not applicable)
    int         range;          // Effective range in tiles (0 = melee adjacent)
    int         maxRange;       // Maximum range (accuracy penalty beyond effective)
    int         magSize;        // Magazine capacity (0 = melee / unlimited)
    int         noiseLevel;     // 0 = silent, 10 = very loud
    int         hitBonus;       // Weapon accuracy modifier
    bool        canBurst;       // Supports burst / suppression fire
    bool        isMelee;
};

// Compile-time weapon table
inline const WeaponData& GetWeaponData(WeaponType type)
{
    //                                    type                name             dDice dBon apS apB rng mRng mag noise hit  burst melee
    static const WeaponData table[] = {
        { WeaponType::Knife,          "Knife",                 1,   1,   2,  0,  1,   1,   0,  0,   0, false, true  },
        { WeaponType::Bat,            "Baseball Bat",          1,   2,   3,  0,  1,   1,   0,  1,   0, false, true  },
        { WeaponType::Machete,        "Machete",               2,   0,   3,  0,  1,   1,   0,  1,   0, false, true  },
        { WeaponType::Sword,          "Sword",                 2,   1,   3,  0,  1,   1,   0,  1,   1, false, true  },
        { WeaponType::Pistol9mm,      "9mm Pistol",            2,   0,   3,  0,  8,  15,  15,  5,   1, false, false },
        { WeaponType::Revolver,       "Revolver",              2,   2,   3,  0,  8,  15,   6,  6,   0, false, false },
        { WeaponType::SMG,            "SMG",                   2,   0,   3,  4,  6,  12,  30,  7,   0, true,  false },
        { WeaponType::AssaultRifle,   "Assault Rifle",         2,   2,   3,  4, 12,  25,  30,  8,   1, true,  false },
        { WeaponType::SniperRifle,    "Sniper Rifle",          3,   2,   4,  0, 20,  35,   5,  9,   3, false, false },
        { WeaponType::Shotgun,        "Shotgun",               3,   1,   3,  0,  4,   8,   6,  8,   2, false, false },
        { WeaponType::BloodSpike,     "Blood Spike",           2,   1,   3,  0, 10,  15,   0,  2,   1, false, false },
        { WeaponType::Grenade,        "Grenade",               3,   3,   3,  0,  6,  10,   1, 10,   0, false, false },
        { WeaponType::Molotov,        "Molotov Cocktail",      2,   1,   3,  0,  5,   8,   1,  7,  -1, false, false },
        { WeaponType::Unarmed,        "Unarmed",               1,   0,   2,  0,  1,   1,   0,  0,  -1, false, true  },
    };
    return table[static_cast<int>(type)];
}

// Roll weapon damage: NdS + bonus
int inline RollWeaponDamage(const WeaponData& w)
{
    // Forward-declared in Dice.h; include Dice.h in .cpp that calls this
    int total = 0;
    // We'll compute in the combat system to avoid circular includes
    // This is just the formula: w.damageDice * d6 + w.damageBonus
    return total;
}

} // namespace vamp
