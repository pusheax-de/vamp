#pragma once
// Attributes.h - Core character attributes and derived stat calculations

#include <cstdint>

namespace vamp
{

// The six core attributes, range 1-12
enum class Attr : uint8_t
{
    STR,    // Strength: melee damage, carry, grapples
    AGI,    // Agility: accuracy, dodge, stealth, AP scaling
    END,    // Endurance: HP, resist bleed/poison, sprint
    PER,    // Perception: vision range, overwatch, detect hidden
    INT,    // Intelligence: hacking, crafting, medicine, ritual complexity
    CHA,    // Charisma: dialogue, prices, leadership, mental resistance
    COUNT
};

static const int ATTR_COUNT = static_cast<int>(Attr::COUNT);
static const int ATTR_MIN   = 1;
static const int ATTR_MAX   = 12;

// A simple bundle of the 6 attributes
struct Attributes
{
    int values[ATTR_COUNT] = { 5, 5, 5, 5, 5, 5 };

    int  Get(Attr a) const       { return values[static_cast<int>(a)]; }
    void Set(Attr a, int v)      { values[static_cast<int>(a)] = v; }

    int STR() const { return Get(Attr::STR); }
    int AGI() const { return Get(Attr::AGI); }
    int END() const { return Get(Attr::END); }
    int PER() const { return Get(Attr::PER); }
    int INT_() const { return Get(Attr::INT); } // INT_ to avoid macro clash
    int CHA() const { return Get(Attr::CHA); }
};

// Derived stats computed from attributes
struct DerivedStats
{
    int maxHP;          // 8 + END + floor(STR/2)
    int apPerTurn;      // 6 + floor(AGI/2)
    int initiative;     // AGI + PER (tie-break with 1d6)
    int visionRadius;   // 6 + floor(PER/2) tiles
    int carryCapacity;  // STR * 3

    static DerivedStats Calculate(const Attributes& attr)
    {
        DerivedStats d;
        d.maxHP         = 8 + attr.END() + attr.STR() / 2;
        d.apPerTurn     = 6 + attr.AGI() / 2;
        d.initiative    = attr.AGI() + attr.PER();
        d.visionRadius  = 6 + attr.PER() / 2;
        d.carryCapacity = attr.STR() * 3;
        return d;
    }
};

} // namespace vamp
