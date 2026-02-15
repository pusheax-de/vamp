#pragma once
// Skill.h - Skill rank storage, effective score calculation, skill checks

#include "SkillDefs.h"
#include "Attributes.h"

namespace vamp
{

// ---------------------------------------------------------------------------
// SkillSet: holds ranks for every skill (0-5)
// ---------------------------------------------------------------------------
struct SkillSet
{
    int ranks[SKILL_COUNT] = {};

    int  GetRank(SkillId id) const      { return ranks[static_cast<int>(id)]; }
    void SetRank(SkillId id, int rank);
    void AddRank(SkillId id, int delta = 1);

    // Effective score = PrimaryAttr + Rank (+ optional situational modifier)
    int EffectiveScore(SkillId id, const Attributes& attr, int situational = 0) const;

    // 3d6 <= EffectiveScore
    bool Check(SkillId id, const Attributes& attr, int situational = 0) const;

    // Opposed check: this character vs opponent. Returns true if this character wins.
    bool OpposedCheck(SkillId mySkill, const Attributes& myAttr,
                      SkillId oppSkill, const Attributes& oppAttr,
                      int mySituational = 0, int oppSituational = 0) const;

    // Meets minimum rank requirement (for gated content)
    bool MeetsRankRequirement(SkillId id, int requiredRank) const;
};

} // namespace vamp
