// Skill.cpp - Skill rank storage, effective score calculation, skill checks

#include "Skill.h"
#include "Dice.h"
#include <algorithm>

namespace vamp
{

void SkillSet::SetRank(SkillId id, int rank)
{
    ranks[static_cast<int>(id)] = std::max(SKILL_MIN_RANK, std::min(rank, SKILL_MAX_RANK));
}

void SkillSet::AddRank(SkillId id, int delta)
{
    SetRank(id, GetRank(id) + delta);
}

int SkillSet::EffectiveScore(SkillId id, const Attributes& attr, int situational) const
{
    const SkillMeta& meta = GetSkillMeta(id);
    int attrValue = attr.Get(meta.primaryAttr);
    return attrValue + GetRank(id) + situational;
}

bool SkillSet::Check(SkillId id, const Attributes& attr, int situational) const
{
    int target = EffectiveScore(id, attr, situational);
    return Check3d6(target);
}

bool SkillSet::OpposedCheck(SkillId mySkill, const Attributes& myAttr,
                            SkillId oppSkill, const Attributes& oppAttr,
                            int mySituational, int oppSituational) const
{
    int myTarget  = EffectiveScore(mySkill, myAttr, mySituational);
    int oppTarget = oppAttr.Get(GetSkillMeta(oppSkill).primaryAttr)
                    + 0 /* opponent rank not stored here — caller provides full SkillSet */
                    + oppSituational;
    // Quick opposed: both roll, compare margin of success
    int myRoll  = Roll3d6();
    int oppRoll = Roll3d6();
    int myMargin  = myTarget - myRoll;
    int oppMargin = oppTarget - oppRoll;
    return myMargin >= oppMargin;
}

bool SkillSet::MeetsRankRequirement(SkillId id, int requiredRank) const
{
    return GetRank(id) >= requiredRank;
}

} // namespace vamp
