#pragma once
// StatusEffect.h - Status effects that can be applied to characters

#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// Status effect types
// ---------------------------------------------------------------------------
enum class StatusType : uint8_t
{
    Bleeding,       // Lose HP per turn until bandaged
    Stunned,        // -2 to hit, -2 AP next turn
    Pinned,         // Lose 2 AP next turn (from suppression), END check to resist
    CrippledLeg,    // Movement cost doubled
    CrippledArm,    // -3 to ranged attacks, may drop weapon on crit
    Poisoned,       // Lose HP per turn, -1 to all checks
    Dominated,      // Under enemy control (vampire Domination)
    BloodMarked,    // Revealed through fog-of-war, debuffed
    Obfuscated,     // Invisible unless enemy passes PER check
    HemocraftArmor, // Temporary +DR from blood transmutation
    Frenzied,       // +2 melee damage, -2 to ranged, cannot flee
    COUNT
};

static const int STATUS_COUNT = static_cast<int>(StatusType::COUNT);

// ---------------------------------------------------------------------------
// A single active status effect on a character
// ---------------------------------------------------------------------------
struct StatusEffect
{
    StatusType type     = StatusType::Bleeding;
    int        duration = 0;    // Turns remaining (0 = expired / permanent until removed)
    int        potency  = 0;    // Strength of the effect (damage per turn, penalty amount, etc.)
    bool       active   = false;

    // Metadata helpers
    static const char* GetName(StatusType t);
    static bool        IsDebuff(StatusType t);
};

// ---------------------------------------------------------------------------
// Status effect stack on a character (one slot per type)
// ---------------------------------------------------------------------------
struct StatusEffectSet
{
    StatusEffect effects[STATUS_COUNT] = {};

    void Apply(StatusType type, int duration, int potency);
    void Remove(StatusType type);
    void TickAll();   // Called at start of turn: reduce durations, remove expired
    bool Has(StatusType type) const;
    int  GetPotency(StatusType type) const;
    int  GetAPPenalty() const;      // Sum of AP penalties from active effects
    int  GetHitPenalty() const;     // Sum of hit-chance penalties from active effects
    int  GetDRBonus() const;        // Bonus DR from effects like HemocraftArmor
    int  GetHPLossPerTurn() const;  // Bleed + Poison damage per turn
};

} // namespace vamp
