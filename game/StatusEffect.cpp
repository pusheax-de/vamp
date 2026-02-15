// StatusEffect.cpp - Status effect implementation

#include "StatusEffect.h"
#include <algorithm>

namespace vamp
{

// ---------------------------------------------------------------------------
// StatusEffect helpers
// ---------------------------------------------------------------------------
const char* StatusEffect::GetName(StatusType t)
{
    switch (t)
    {
    case StatusType::Bleeding:       return "Bleeding";
    case StatusType::Stunned:        return "Stunned";
    case StatusType::Pinned:         return "Pinned";
    case StatusType::CrippledLeg:    return "Crippled Leg";
    case StatusType::CrippledArm:    return "Crippled Arm";
    case StatusType::Poisoned:       return "Poisoned";
    case StatusType::Dominated:      return "Dominated";
    case StatusType::BloodMarked:    return "Blood Marked";
    case StatusType::Obfuscated:     return "Obfuscated";
    case StatusType::HemocraftArmor: return "Hemocraft Armor";
    case StatusType::Frenzied:       return "Frenzied";
    default:                         return "Unknown";
    }
}

bool StatusEffect::IsDebuff(StatusType t)
{
    switch (t)
    {
    case StatusType::Obfuscated:
    case StatusType::HemocraftArmor:
        return false;
    default:
        return true;
    }
}

// ---------------------------------------------------------------------------
// StatusEffectSet
// ---------------------------------------------------------------------------
void StatusEffectSet::Apply(StatusType type, int duration, int potency)
{
    auto& e = effects[static_cast<int>(type)];
    e.type     = type;
    e.duration = std::max(e.duration, duration); // Refresh, don't stack shorter
    e.potency  = std::max(e.potency, potency);   // Keep stronger
    e.active   = true;
}

void StatusEffectSet::Remove(StatusType type)
{
    auto& e = effects[static_cast<int>(type)];
    e.active   = false;
    e.duration = 0;
    e.potency  = 0;
}

void StatusEffectSet::TickAll()
{
    for (int i = 0; i < STATUS_COUNT; ++i)
    {
        auto& e = effects[i];
        if (!e.active) continue;
        if (e.duration > 0)
        {
            --e.duration;
            if (e.duration <= 0)
            {
                e.active  = false;
                e.potency = 0;
            }
        }
        // duration == 0 means permanent until explicitly removed
    }
}

bool StatusEffectSet::Has(StatusType type) const
{
    return effects[static_cast<int>(type)].active;
}

int StatusEffectSet::GetPotency(StatusType type) const
{
    const auto& e = effects[static_cast<int>(type)];
    return e.active ? e.potency : 0;
}

int StatusEffectSet::GetAPPenalty() const
{
    int penalty = 0;
    if (Has(StatusType::Stunned)) penalty += 2;
    if (Has(StatusType::Pinned))  penalty += 2;
    return penalty;
}

int StatusEffectSet::GetHitPenalty() const
{
    int penalty = 0;
    if (Has(StatusType::Stunned))     penalty += 2;
    if (Has(StatusType::CrippledArm)) penalty += 3;
    if (Has(StatusType::Poisoned))    penalty += 1;
    return penalty;
}

int StatusEffectSet::GetDRBonus() const
{
    int dr = 0;
    if (Has(StatusType::HemocraftArmor))
        dr += GetPotency(StatusType::HemocraftArmor);
    return dr;
}

int StatusEffectSet::GetHPLossPerTurn() const
{
    int loss = 0;
    if (Has(StatusType::Bleeding))
        loss += std::max(1, GetPotency(StatusType::Bleeding));
    if (Has(StatusType::Poisoned))
        loss += std::max(1, GetPotency(StatusType::Poisoned));
    return loss;
}

} // namespace vamp
