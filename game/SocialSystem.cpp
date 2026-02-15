// SocialSystem.cpp - Social system implementation

#include "SocialSystem.h"
#include "GameWorld.h"
#include "Dice.h"

namespace vamp
{

std::vector<const DialogueOption*> SocialSystem::GetAvailableOptions(
    const DialogueNode& node, const Character& player) const
{
    std::vector<const DialogueOption*> available;
    for (const auto& opt : node.options)
    {
        // Talking skill gates: option is visible only if rank >= required
        if (player.MeetsSkillReq(opt.requiredSkill, opt.requiredRank))
        {
            available.push_back(&opt);
        }
    }
    return available;
}

SocialResult SocialSystem::AttemptSocialCheck(const Character& player, const Character& npc,
                                               SkillId skill, int situational) const
{
    SocialResult result;

    // Player's effective score
    int playerScore = player.SkillEffective(skill, situational);

    // NPC resistance: CHA + relevant counter-skill
    SkillId counterSkill = SkillId::Talking; // Default
    switch (skill)
    {
    case SkillId::Persuasion:   counterSkill = SkillId::Streetwise;    break;
    case SkillId::Deception:    counterSkill = SkillId::Empathy;       break;
    case SkillId::Intimidation: counterSkill = SkillId::Intimidation;  break; // Resist intimidation with own
    case SkillId::Empathy:      counterSkill = SkillId::Deception;     break;
    default: break;
    }

    int npcScore = npc.SkillEffective(counterSkill);

    // Opposed roll
    int playerRoll = Roll3d6();
    int npcRoll    = Roll3d6();

    result.roll         = playerRoll;
    result.targetNumber = playerScore;
    result.critSuccess  = IsCritSuccess(playerRoll);
    result.critFailure  = IsCritFailure(playerRoll);

    int playerMargin = playerScore - playerRoll;
    int npcMargin    = npcScore - npcRoll;

    result.success = (playerMargin >= npcMargin) || result.critSuccess;
    if (result.critFailure)
        result.success = false;

    return result;
}

SocialResult SocialSystem::Persuade(const Character& player, const Character& npc,
                                     int situational) const
{
    SocialResult result = AttemptSocialCheck(player, npc, SkillId::Persuasion, situational);
    if (result.success)
    {
        result.message = "You make a compelling argument.";
    }
    else if (result.critFailure)
    {
        result.message = "You offend them deeply.";
        result.reputationDelta = -5;
    }
    else
    {
        result.message = "They're not convinced.";
    }
    return result;
}

SocialResult SocialSystem::Deceive(const Character& player, const Character& npc,
                                    int situational) const
{
    SocialResult result = AttemptSocialCheck(player, npc, SkillId::Deception, situational);
    if (result.success)
    {
        result.message = "They believe your story.";
    }
    else if (result.critFailure)
    {
        result.message = "They see right through you and are furious.";
        result.reputationDelta = -10;
        result.heatDelta = 5;
    }
    else
    {
        result.message = "They look suspicious.";
        result.reputationDelta = -2;
    }
    return result;
}

SocialResult SocialSystem::Intimidate(const Character& player, const Character& npc,
                                       int situational) const
{
    // Intimidation always adds some heat
    SocialResult result = AttemptSocialCheck(player, npc, SkillId::Intimidation, situational);
    result.heatDelta = 3; // Even on success, intimidation draws attention

    if (result.success)
    {
        result.message = "They cower before you.";
    }
    else if (result.critFailure)
    {
        result.message = "They laugh in your face and call for backup.";
        result.heatDelta = 10;
    }
    else
    {
        result.message = "They refuse to be intimidated.";
        result.heatDelta = 5;
    }
    return result;
}

SocialResult SocialSystem::ReadEmotions(const Character& player, const Character& npc) const
{
    SocialResult result;

    int playerScore = player.SkillEffective(SkillId::Empathy);
    int npcScore    = npc.SkillEffective(SkillId::Deception);

    int playerRoll = Roll3d6();
    result.roll = playerRoll;
    result.targetNumber = playerScore;

    // Not opposed per se — NPC's deception is passive difficulty
    int target = playerScore - npcScore / 2;
    result.success = (playerRoll <= target);
    result.critSuccess = IsCritSuccess(playerRoll);

    if (result.success || result.critSuccess)
    {
        result.message = "You sense their true feelings.";
        // Check for Dominated status
        if (npc.statuses.Has(StatusType::Dominated))
            result.message = "You sense they are being controlled by something.";
    }
    else
    {
        result.message = "Their expression is unreadable.";
    }

    return result;
}

SocialResult SocialSystem::GatherStreetInfo(const Character& player, int requiredRank) const
{
    SocialResult result;

    if (!player.MeetsSkillReq(SkillId::Streetwise, requiredRank))
    {
        result.success = false;
        result.message = "You don't have enough street contacts.";
        return result;
    }

    // Skill check with no opposition
    int target = player.SkillEffective(SkillId::Streetwise);
    result.roll = Roll3d6();
    result.targetNumber = target;
    result.success = (result.roll <= target);

    if (result.success)
    {
        result.message = "Your contacts come through with information.";
    }
    else
    {
        result.message = "The streets are quiet tonight.";
    }

    return result;
}

bool SocialSystem::CanAccessDialogueTier(const Character& player, int tier) const
{
    return player.MeetsSkillReq(SkillId::Talking, tier);
}

void SocialSystem::ApplyReputationChange(GameWorld& world, Faction faction, int delta) const
{
    for (auto& clan : world.clans)
    {
        if (clan.faction == faction)
        {
            clan.playerReputation = std::max(-100, std::min(100,
                clan.playerReputation + delta));
        }
    }
}

void SocialSystem::ApplyHeatFromSocial(GameWorld& world, int x, int y, int heatDelta) const
{
    if (heatDelta != 0)
        world.AddHeat(x, y, heatDelta);
}

} // namespace vamp
