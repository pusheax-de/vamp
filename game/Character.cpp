// Character.cpp - Character sheet implementation

#include "Character.h"
#include "Dice.h"
#include <algorithm>

namespace vamp
{

void Character::Initialize()
{
    RecalcDerived();
    currentHP    = derived.maxHP;
    currentAP    = 0; // Set at BeginTurn
    if (isVampire)
    {
        maxBR        = 6 + bloodPotency;
        bloodReserve = maxBR;
    }
}

void Character::RecalcDerived()
{
    derived = DerivedStats::Calculate(attributes);
}

void Character::BeginTurn()
{
    if (!isAlive) return;

    // Tick status effects (reduce durations, expire)
    statuses.TickAll();

    // Apply damage-over-time (bleed, poison)
    int dot = statuses.GetHPLossPerTurn();
    if (dot > 0)
        TakeDamage(dot);

    if (IsDead())
    {
        isAlive = false;
        return;
    }

    // Refresh AP
    currentAP = GetMaxAP();

    // Noise resets
    noiseLevel = inventory.equipment.GetArmor().noisePenalty;
}

void Character::EndTurn()
{
    // Cleanup — nothing special for now
}

void Character::TakeDamage(int rawDamage)
{
    int dr = GetTotalDR();
    int actual = std::max(0, rawDamage - dr);
    currentHP -= actual;
    if (currentHP <= 0)
    {
        currentHP = 0;
        isAlive = false;
    }
}

void Character::Heal(int amount)
{
    currentHP = std::min(currentHP + amount, derived.maxHP);
}

int Character::GetTotalDR() const
{
    int dr = inventory.equipment.GetArmor().damageReduction;
    dr += statuses.GetDRBonus();
    return dr;
}

bool Character::SpendAP(int cost)
{
    if (currentAP < cost) return false;
    currentAP -= cost;
    return true;
}

int Character::GetMaxAP() const
{
    int ap = derived.apPerTurn - statuses.GetAPPenalty();
    return std::max(0, ap);
}

bool Character::SpendBR(int cost)
{
    if (!isVampire) return false;
    if (bloodReserve < cost) return false;
    bloodReserve -= cost;
    return true;
}

void Character::RestoreBR(int amount)
{
    if (!isVampire) return;
    bloodReserve = std::min(bloodReserve + amount, maxBR);
}

void Character::FullRestBR()
{
    if (!isVampire) return;
    bloodReserve = maxBR;
}

int Character::GetHitModifier() const
{
    int mod = 0;
    mod -= statuses.GetHitPenalty();
    // Armor AGI penalty
    mod -= inventory.equipment.GetArmor().agiPenalty;
    return mod;
}

int Character::GetMeleeDamageBonus() const
{
    // +1 damage per 2 STR above 5
    int str = attributes.STR();
    int bonus = (str - 5) / 2;
    if (statuses.Has(StatusType::Frenzied))
        bonus += 2;
    return std::max(0, bonus);
}

bool Character::SkillCheck(SkillId skill, int situational) const
{
    return skills.Check(skill, attributes, situational);
}

int Character::SkillEffective(SkillId skill, int situational) const
{
    return skills.EffectiveScore(skill, attributes, situational);
}

bool Character::MeetsSkillReq(SkillId skill, int rank) const
{
    return skills.MeetsRankRequirement(skill, rank);
}

} // namespace vamp
