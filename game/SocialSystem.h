#pragma once
// SocialSystem.h - Dialogue checks, persuasion, deception, intimidation, empathy

#include "Character.h"
#include <string>
#include <vector>
#include <cstdint>

namespace vamp
{

// Forward declarations
struct GameWorld;

// ---------------------------------------------------------------------------
// Dialogue option - a single branch in a conversation
// ---------------------------------------------------------------------------
struct DialogueOption
{
    std::string text;                   // What the player says
    SkillId     requiredSkill   = SkillId::Talking;
    int         requiredRank    = 0;    // Minimum rank to see this option
    bool        requiresRoll    = false; // True = needs a skill check to succeed
    int         rollDifficulty  = 0;    // Situational modifier for the check
    std::string successText;            // NPC response on success
    std::string failureText;            // NPC response on failure

    // Effects
    int         reputationChange = 0;   // With the NPC's faction
    int         heatChange       = 0;   // Heat delta (intimidation adds heat)
    int         moneyChange      = 0;   // Price adjustment, bribe cost
    bool        revealsInfo      = false;
    std::string revealedInfo;           // Key information unlocked
};

// ---------------------------------------------------------------------------
// Dialogue node - a conversation state with multiple options
// ---------------------------------------------------------------------------
struct DialogueNode
{
    std::string                 npcText;            // What the NPC says
    std::vector<DialogueOption> options;             // Player choices
    bool                        isTerminal = false;  // End of conversation
};

// ---------------------------------------------------------------------------
// Social check result
// ---------------------------------------------------------------------------
struct SocialResult
{
    bool    success         = false;
    bool    critSuccess     = false;
    bool    critFailure     = false;
    int     roll            = 0;
    int     targetNumber    = 0;
    int     reputationDelta = 0;
    int     heatDelta       = 0;
    std::string message;
};

// ---------------------------------------------------------------------------
// SocialSystem - handles all social interactions
// ---------------------------------------------------------------------------
class SocialSystem
{
public:
    // Filter dialogue options based on player skills (hide options they can't see)
    std::vector<const DialogueOption*> GetAvailableOptions(
        const DialogueNode& node, const Character& player) const;

    // Attempt a social skill check (Persuasion, Deception, Intimidation, etc.)
    SocialResult AttemptSocialCheck(const Character& player, const Character& npc,
                                    SkillId skill, int situational = 0) const;

    // Specific social actions
    SocialResult Persuade(const Character& player, const Character& npc,
                          int situational = 0) const;
    SocialResult Deceive(const Character& player, const Character& npc,
                         int situational = 0) const;
    SocialResult Intimidate(const Character& player, const Character& npc,
                            int situational = 0) const;

    // Empathy: detect if NPC is lying or has hidden motives
    SocialResult ReadEmotions(const Character& player, const Character& npc) const;

    // Streetwise: gather info about the city, clans, safe locations
    SocialResult GatherStreetInfo(const Character& player, int requiredRank) const;

    // Talking: check if player can access a specific dialogue tier
    bool CanAccessDialogueTier(const Character& player, int tier) const;

    // Apply reputation change to a clan
    void ApplyReputationChange(GameWorld& world, Faction faction, int delta) const;

    // Apply heat change from social action (e.g., intimidation)
    void ApplyHeatFromSocial(GameWorld& world, int x, int y, int heatDelta) const;
};

} // namespace vamp
