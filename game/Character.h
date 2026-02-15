#pragma once
// Character.h - Complete character sheet: attributes, skills, health, vampire state

#include "Attributes.h"
#include "Skill.h"
#include "StatusEffect.h"
#include "Inventory.h"
#include <string>
#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// Faction / allegiance
// ---------------------------------------------------------------------------
enum class Faction : uint8_t
{
    Player,
    VampireClanNosferatu,
    VampireClanTremere,
    VampireClanBrujah,
    VampireClanVentrue,
    VampireClanMalkavian,
    Police,
    Mercenary,
    Civilian,
    COUNT
};

// ---------------------------------------------------------------------------
// Character — the full "character sheet"
// ---------------------------------------------------------------------------
struct Character
{
    // Identity
    std::string name;
    Faction     faction     = Faction::Civilian;
    bool        isVampire   = false;
    bool        isAlive     = true;

    // Core
    Attributes      attributes;
    DerivedStats    derived;         // Recalculate after attribute changes
    SkillSet        skills;

    // Health & Resources
    int currentHP   = 0;
    int currentAP   = 0;       // Remaining AP this turn
    int bloodReserve = 0;      // BR — vampire only
    int maxBR        = 0;      // 6 + bloodPotency
    int bloodPotency = 0;      // 0-4, increases with age/power

    // Status
    StatusEffectSet statuses;

    // Gear
    Inventory inventory;

    // Position on map
    int tileX = 0;
    int tileY = 0;

    // Stealth state
    bool isHidden   = false;
    int  noiseLevel = 0;       // Current noise output

    // ---------------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------------
    void Initialize();                      // Compute derived stats, set HP/AP/BR
    void RecalcDerived();                   // Recompute derived stats from attributes
    void BeginTurn();                       // Start-of-turn: refresh AP, tick statuses, apply DoTs
    void EndTurn();                         // End-of-turn cleanup

    // ---------------------------------------------------------------------------
    // Health
    // ---------------------------------------------------------------------------
    void TakeDamage(int rawDamage);         // Apply damage after DR
    void Heal(int amount);
    bool IsDead() const { return currentHP <= 0; }
    int  GetTotalDR() const;                // Armor DR + status DR

    // ---------------------------------------------------------------------------
    // Action Points
    // ---------------------------------------------------------------------------
    bool SpendAP(int cost);                 // Returns false if insufficient
    int  GetMaxAP() const;                  // Derived AP - status penalties

    // ---------------------------------------------------------------------------
    // Blood Reserve (Vampire)
    // ---------------------------------------------------------------------------
    bool SpendBR(int cost);
    void RestoreBR(int amount);
    void FullRestBR();                      // Sleep: restore to max

    // ---------------------------------------------------------------------------
    // Combat helpers
    // ---------------------------------------------------------------------------
    int  GetHitModifier() const;            // Sum of weapon bonus, status penalties, etc.
    int  GetMeleeDamageBonus() const;       // STR-based bonus for melee

    // ---------------------------------------------------------------------------
    // Skill shorthand
    // ---------------------------------------------------------------------------
    bool SkillCheck(SkillId skill, int situational = 0) const;
    int  SkillEffective(SkillId skill, int situational = 0) const;
    bool MeetsSkillReq(SkillId skill, int rank) const;
};

} // namespace vamp
