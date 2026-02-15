#pragma once
// SkillDefs.h - Skill identifiers, categories, and metadata

#include "Attributes.h"
#include <cstdint>

namespace vamp
{

// ---------------------------------------------------------------------------
// Skill categories
// ---------------------------------------------------------------------------
enum class SkillCategory : uint8_t
{
    Combat,
    Thief,
    Social,
    Vampire,    // Disciplines
    COUNT
};

// ---------------------------------------------------------------------------
// Every skill in the game
// ---------------------------------------------------------------------------
enum class SkillId : uint8_t
{
    // -- Combat --
    Firearms,       // AGI - ranged hit chance, called shots
    Melee,          // STR/AGI - close combat, disarms, shove
    Athletics,      // END/AGI - sprint, vault, resist knockdown
    Tactics,        // PER/INT - overwatch, flanking, analyze cover
    Explosives,     // INT - grenades, mines, breaching
    Medicine,       // INT - stop bleed, revive, craft stims

    // -- Thief / Underworld --
    Stealth,        // AGI - hide, silent move, takedowns
    Lockpicking,    // AGI/INT - doors, safes, hatches
    Pickpocket,     // AGI/CHA - steal, plant items
    Hacking,        // INT - cameras, gates, police systems
    Traps,          // INT/PER - detect/disable alarms, snares
    Disguise,       // CHA/INT - pass as worker/cop

    // -- Social --
    Talking,        // CHA - reveals dialogue branches
    Persuasion,     // CHA - cooperation, alliances
    Deception,      // CHA - lies, misdirection
    Intimidation,   // CHA/STR - threats, coercion (adds heat)
    Empathy,        // PER/CHA - detect lies, motives
    Streetwise,     // INT/CHA - black market, rumors, clan info

    // -- Vampire Disciplines --
    Blink,          // AGI - teleport
    Telekinesis,    // PER/INT - push/pull, disarm
    Hemocraft,      // INT - blood transmutation (armor, spike, seal)
    BloodMark,      // CHA/PER - tracking curse
    Auspex,         // PER - reveal, anti-stealth, sense weakness
    Domination,     // CHA - stun, command, turn humans
    Obfuscate,      // AGI/CHA - supernatural stealth
    Celerity,       // AGI - AP manipulation

    COUNT
};

static const int SKILL_COUNT = static_cast<int>(SkillId::COUNT);
static const int SKILL_MIN_RANK = 0;
static const int SKILL_MAX_RANK = 5;

// ---------------------------------------------------------------------------
// Metadata for each skill (compile-time table)
// ---------------------------------------------------------------------------
struct SkillMeta
{
    SkillId       id;
    const char*   name;
    SkillCategory category;
    Attr          primaryAttr;    // Main governing attribute
    Attr          secondaryAttr;  // Secondary (same as primary if none)
};

// Inline table - one entry per SkillId, keep in enum order
inline const SkillMeta& GetSkillMeta(SkillId id)
{
    static const SkillMeta table[] =
    {
        // Combat
        { SkillId::Firearms,      "Firearms",      SkillCategory::Combat,  Attr::AGI, Attr::AGI },
        { SkillId::Melee,         "Melee",         SkillCategory::Combat,  Attr::STR, Attr::AGI },
        { SkillId::Athletics,     "Athletics",     SkillCategory::Combat,  Attr::END, Attr::AGI },
        { SkillId::Tactics,       "Tactics",       SkillCategory::Combat,  Attr::PER, Attr::INT },
        { SkillId::Explosives,    "Explosives",    SkillCategory::Combat,  Attr::INT, Attr::INT },
        { SkillId::Medicine,      "Medicine",      SkillCategory::Combat,  Attr::INT, Attr::INT },

        // Thief
        { SkillId::Stealth,       "Stealth",       SkillCategory::Thief,   Attr::AGI, Attr::AGI },
        { SkillId::Lockpicking,   "Lockpicking",   SkillCategory::Thief,   Attr::AGI, Attr::INT },
        { SkillId::Pickpocket,    "Pickpocket",    SkillCategory::Thief,   Attr::AGI, Attr::CHA },
        { SkillId::Hacking,       "Hacking",       SkillCategory::Thief,   Attr::INT, Attr::INT },
        { SkillId::Traps,         "Traps",         SkillCategory::Thief,   Attr::INT, Attr::PER },
        { SkillId::Disguise,      "Disguise",      SkillCategory::Thief,   Attr::CHA, Attr::INT },

        // Social
        { SkillId::Talking,       "Talking",       SkillCategory::Social,  Attr::CHA, Attr::CHA },
        { SkillId::Persuasion,    "Persuasion",    SkillCategory::Social,  Attr::CHA, Attr::CHA },
        { SkillId::Deception,     "Deception",     SkillCategory::Social,  Attr::CHA, Attr::CHA },
        { SkillId::Intimidation,  "Intimidation",  SkillCategory::Social,  Attr::CHA, Attr::STR },
        { SkillId::Empathy,       "Empathy",       SkillCategory::Social,  Attr::PER, Attr::CHA },
        { SkillId::Streetwise,    "Streetwise",    SkillCategory::Social,  Attr::INT, Attr::CHA },

        // Vampire Disciplines
        { SkillId::Blink,         "Blink",         SkillCategory::Vampire, Attr::AGI, Attr::AGI },
        { SkillId::Telekinesis,   "Telekinesis",   SkillCategory::Vampire, Attr::PER, Attr::INT },
        { SkillId::Hemocraft,     "Hemocraft",     SkillCategory::Vampire, Attr::INT, Attr::INT },
        { SkillId::BloodMark,     "Blood Mark",    SkillCategory::Vampire, Attr::CHA, Attr::PER },
        { SkillId::Auspex,        "Auspex",        SkillCategory::Vampire, Attr::PER, Attr::PER },
        { SkillId::Domination,    "Domination",    SkillCategory::Vampire, Attr::CHA, Attr::CHA },
        { SkillId::Obfuscate,     "Obfuscate",     SkillCategory::Vampire, Attr::AGI, Attr::CHA },
        { SkillId::Celerity,      "Celerity",      SkillCategory::Vampire, Attr::AGI, Attr::AGI },
    };
    return table[static_cast<int>(id)];
}

} // namespace vamp
