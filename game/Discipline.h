#pragma once
// Discipline.h - Vampire disciplines: Blink, Telekinesis, Hemocraft, BloodMark,
//                Auspex, Domination, Obfuscate, Celerity

#include "Character.h"
#include "GameWorld.h"
#include "FogOfWar.h"

namespace vamp
{

// ---------------------------------------------------------------------------
// Discipline use result
// ---------------------------------------------------------------------------
struct DisciplineResult
{
    bool        success         = false;
    bool        insufficientBR  = false;
    bool        insufficientAP  = false;
    bool        outOfRange      = false;
    int         brSpent         = 0;
    int         apSpent         = 0;
    const char* message         = "";
};

// ---------------------------------------------------------------------------
// DisciplineSystem - resolves all vampire discipline actions
// ---------------------------------------------------------------------------
class DisciplineSystem
{
public:
    // -----------------------------------------------------------------------
    // Blink (Teleport)
    // -----------------------------------------------------------------------
    // Rank 1: Blink 3 tiles, LoS required (2 BR, 3 AP)
    // Rank 3: Blink to any visible cover tile within 6 (2 BR, 3 AP)
    // Rank 5: Chain Blink twice in one turn (4 BR total, 5 AP)
    DisciplineResult UseBlink(GameWorld& world, Character& caster,
                              int destX, int destY);

    // -----------------------------------------------------------------------
    // Telekinesis (Push/Pull/Disarm)
    // -----------------------------------------------------------------------
    // Push enemy out of cover, pull items, disarm weapons
    DisciplineResult UseTelekinesisShove(GameWorld& world, Character& caster,
                                         Character& target, int pushDirX, int pushDirY);
    DisciplineResult UseTelekinesisDisarm(GameWorld& world, Character& caster,
                                          Character& target);

    // -----------------------------------------------------------------------
    // Hemocraft (Blood Transmutation)
    // -----------------------------------------------------------------------
    // Coagulate Armor: +DR for 3 turns (3 BR, 2 AP)
    DisciplineResult UseHemocraftArmor(Character& caster);
    // Blood Spike: ranged pierce ignores half cover (2 BR, 3 AP)
    DisciplineResult UseBloodSpike(GameWorld& world, Character& caster, Character& target);
    // Blood Seal: lock a door magically (2 BR, 2 AP)
    DisciplineResult UseBloodSeal(GameWorld& world, Character& caster, int doorX, int doorY);

    // -----------------------------------------------------------------------
    // Blood Mark (Tracking / Curse)
    // -----------------------------------------------------------------------
    // Mark target: reveals through fog + debuff. Duration scales with rank.
    DisciplineResult UseBloodMark(GameWorld& world, FogOfWar& fow,
                                   Character& caster, Character& target);

    // -----------------------------------------------------------------------
    // Auspex (Reveal / Anti-stealth)
    // -----------------------------------------------------------------------
    // Pulse: reveal enemies in radius (2 BR, 2 AP)
    DisciplineResult UseAuspexPulse(GameWorld& world, FogOfWar& fow, Character& caster);
    // Sense Weakness: +crit chance vs marked target (passive, checked in combat)
    bool HasSenseWeakness(const Character& caster, const Character& target) const;

    // -----------------------------------------------------------------------
    // Domination (Social Combat / Mind Control)
    // -----------------------------------------------------------------------
    // Stun a weak-willed human (opposed CHA check)
    DisciplineResult UseDominationStun(GameWorld& world, Character& caster, Character& target);
    // Command: force target to perform one action
    DisciplineResult UseDominationCommand(GameWorld& world, Character& caster, Character& target);

    // -----------------------------------------------------------------------
    // Obfuscate (Supernatural Stealth)
    // -----------------------------------------------------------------------
    // Become unseen. Shooting breaks it unless rank >= 4.
    DisciplineResult UseObfuscate(Character& caster);
    // Check if an observer can see through obfuscation
    bool CanSeeThrough(const Character& observer, const Character& obfuscated) const;

    // -----------------------------------------------------------------------
    // Celerity (AP Manipulation)
    // -----------------------------------------------------------------------
    // Spend BR to gain extra AP this turn
    DisciplineResult UseCelerity(Character& caster);

private:
    // Cost tables (BR cost, AP cost) per discipline at base rank
    struct DisciplineCost
    {
        int brCost;
        int apCost;
    };

    DisciplineCost GetBlinkCost(int rank) const;
    DisciplineCost GetTKCost(int rank) const;
    DisciplineCost GetHemocraftCost(int rank) const;
    DisciplineCost GetBloodMarkCost(int rank) const;
    DisciplineCost GetAuspexCost(int rank) const;
    DisciplineCost GetDominationCost(int rank) const;
    DisciplineCost GetObfuscateCost(int rank) const;
    DisciplineCost GetCelerityCost(int rank) const;

    bool PayCost(Character& caster, const DisciplineCost& cost, DisciplineResult& result);
};

} // namespace vamp
