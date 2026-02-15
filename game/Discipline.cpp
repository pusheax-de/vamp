// Discipline.cpp - Vampire discipline implementation

#include "Discipline.h"
#include "CoverSystem.h"
#include "Dice.h"
#include <algorithm>
#include <cmath>

namespace vamp
{

// ---------------------------------------------------------------------------
// Cost helpers
// ---------------------------------------------------------------------------
DisciplineSystem::DisciplineCost DisciplineSystem::GetBlinkCost(int rank) const
{
    // Rank 5 chain blink costs more
    if (rank >= 5) return { 4, 5 };
    return { 2, 3 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetTKCost(int rank) const
{
    int br = std::max(1, 3 - rank / 2); // Cheaper at high rank
    return { br, 3 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetHemocraftCost(int rank) const
{
    return { 3, 2 }; // Base cost for armor; spike and seal have own costs
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetBloodMarkCost(int rank) const
{
    int br = std::max(1, 3 - rank / 3);
    return { br, 2 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetAuspexCost(int rank) const
{
    return { 2, 2 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetDominationCost(int rank) const
{
    int br = std::max(1, 4 - rank);
    return { br, 3 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetObfuscateCost(int rank) const
{
    int br = std::max(1, 3 - rank / 2);
    return { br, 2 };
}

DisciplineSystem::DisciplineCost DisciplineSystem::GetCelerityCost(int rank) const
{
    return { 3, 0 }; // No AP cost — you're gaining AP
}

bool DisciplineSystem::PayCost(Character& caster, const DisciplineCost& cost,
                                DisciplineResult& result)
{
    if (!caster.isVampire)
    {
        result.message = "Not a vampire.";
        return false;
    }

    if (caster.bloodReserve < cost.brCost)
    {
        result.insufficientBR = true;
        result.message = "Not enough Blood Reserve.";
        return false;
    }

    if (caster.currentAP < cost.apCost)
    {
        result.insufficientAP = true;
        result.message = "Not enough Action Points.";
        return false;
    }

    caster.SpendBR(cost.brCost);
    if (cost.apCost > 0)
        caster.SpendAP(cost.apCost);

    result.brSpent = cost.brCost;
    result.apSpent = cost.apCost;
    return true;
}

// ---------------------------------------------------------------------------
// Blink
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseBlink(GameWorld& world, Character& caster,
                                             int destX, int destY)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Blink);
    if (rank < 1)
    {
        result.message = "Blink not learned.";
        return result;
    }

    // Range check
    int maxRange = (rank >= 3) ? 6 : 3;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY, destX, destY);
    if (dist > maxRange)
    {
        result.outOfRange = true;
        result.message = "Target too far for Blink.";
        return result;
    }

    // LoS check (rank < 3 requires LoS)
    if (rank < 3)
    {
        if (!CoverSystem::HasLineOfSight(world.map,
            caster.tileX, caster.tileY, destX, destY))
        {
            result.message = "No line of sight.";
            return result;
        }
    }

    // Destination must be passable and empty
    if (!world.map.InBounds(destX, destY) || !world.map.At(destX, destY).IsPassable())
    {
        result.message = "Destination blocked.";
        return result;
    }
    if (world.GetCharacterAt(destX, destY) != nullptr)
    {
        result.message = "Destination occupied.";
        return result;
    }

    auto cost = GetBlinkCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    caster.tileX = destX;
    caster.tileY = destY;
    result.success = true;
    result.message = "Blinked!";
    return result;
}

// ---------------------------------------------------------------------------
// Telekinesis
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseTelekinesisShove(GameWorld& world, Character& caster,
                                                        Character& target,
                                                        int pushDirX, int pushDirY)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Telekinesis);
    if (rank < 1)
    {
        result.message = "Telekinesis not learned.";
        return result;
    }

    int range = 4 + rank;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY,
                                          target.tileX, target.tileY);
    if (dist > range)
    {
        result.outOfRange = true;
        result.message = "Target out of TK range.";
        return result;
    }

    auto cost = GetTKCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    // Push target 1-2 tiles in the given direction
    int pushDist = 1 + rank / 3;
    for (int i = 0; i < pushDist; ++i)
    {
        int newX = target.tileX + pushDirX;
        int newY = target.tileY + pushDirY;
        if (world.map.InBounds(newX, newY) &&
            world.map.At(newX, newY).IsPassable() &&
            world.GetCharacterAt(newX, newY) == nullptr)
        {
            target.tileX = newX;
            target.tileY = newY;
        }
        else
        {
            // Slammed into wall — bonus damage
            target.TakeDamage(2);
            break;
        }
    }

    result.success = true;
    result.message = "Target shoved!";
    return result;
}

DisciplineResult DisciplineSystem::UseTelekinesisDisarm(GameWorld& world, Character& caster,
                                                         Character& target)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Telekinesis);
    if (rank < 2)
    {
        result.message = "Telekinesis rank 2 required to disarm.";
        return result;
    }

    int range = 4 + rank;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY,
                                          target.tileX, target.tileY);
    if (dist > range)
    {
        result.outOfRange = true;
        result.message = "Target out of TK range.";
        return result;
    }

    DisciplineCost cost = { 3, 3 }; // Disarm is expensive
    if (!PayCost(caster, cost, result))
        return result;

    // Opposed check: caster TK vs target STR
    int casterScore = caster.SkillEffective(SkillId::Telekinesis);
    int targetScore = target.attributes.STR() + target.skills.GetRank(SkillId::Athletics);

    int casterRoll = Roll3d6();
    int targetRoll = Roll3d6();

    if ((casterScore - casterRoll) >= (targetScore - targetRoll))
    {
        // Disarm: drop weapon to unarmed
        target.inventory.equipment.primaryWeapon = WeaponType::Unarmed;
        target.inventory.equipment.primaryAmmo = 0;
        result.success = true;
        result.message = "Weapon ripped away!";
    }
    else
    {
        result.success = false;
        result.message = "Target held onto their weapon.";
    }

    return result;
}

// ---------------------------------------------------------------------------
// Hemocraft
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseHemocraftArmor(Character& caster)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Hemocraft);
    if (rank < 1)
    {
        result.message = "Hemocraft not learned.";
        return result;
    }

    DisciplineCost cost = { 3, 2 };
    if (!PayCost(caster, cost, result))
        return result;

    int dr = 3 + rank;     // DR bonus scales with rank
    int duration = 3 + rank / 2;
    caster.statuses.Apply(StatusType::HemocraftArmor, duration, dr);

    result.success = true;
    result.message = "Blood coagulates into armor.";
    return result;
}

DisciplineResult DisciplineSystem::UseBloodSpike(GameWorld& world, Character& caster,
                                                  Character& target)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Hemocraft);
    if (rank < 1)
    {
        result.message = "Hemocraft not learned.";
        return result;
    }

    DisciplineCost cost = { 2, 3 };
    if (!PayCost(caster, cost, result))
        return result;

    int range = 10;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY,
                                          target.tileX, target.tileY);
    if (dist > range)
    {
        // Refund (in a real game, check range before paying)
        caster.RestoreBR(cost.brCost);
        result.outOfRange = true;
        result.message = "Target out of Blood Spike range.";
        return result;
    }

    // Blood Spike ignores half cover
    int hitTarget = caster.SkillEffective(SkillId::Hemocraft);
    CoverInfo cover = CoverSystem::QueryCoverBetween(world.map,
        caster.tileX, caster.tileY, target.tileX, target.tileY);
    int coverPenalty = (cover.level == CoverLevel::Full) ? 4 : 0; // Ignore half cover

    hitTarget -= coverPenalty;
    hitTarget -= std::max(0, dist - 6); // Range penalty beyond 6

    int roll = Roll3d6();
    if (roll <= hitTarget)
    {
        const WeaponData& spike = GetWeaponData(WeaponType::BloodSpike);
        int damage = RollNd(spike.damageDice, 6) + spike.damageBonus + rank;
        target.TakeDamage(damage);
        result.success = true;
        result.message = "Blood Spike pierces the target!";
    }
    else
    {
        result.success = false;
        result.message = "Blood Spike missed.";
    }

    return result;
}

DisciplineResult DisciplineSystem::UseBloodSeal(GameWorld& world, Character& caster,
                                                 int doorX, int doorY)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Hemocraft);
    if (rank < 1)
    {
        result.message = "Hemocraft not learned.";
        return result;
    }

    if (!world.map.InBounds(doorX, doorY))
    {
        result.message = "Invalid location.";
        return result;
    }

    MapTile& tile = world.map.At(doorX, doorY);
    if (tile.terrain != TerrainType::Door)
    {
        result.message = "Not a door.";
        return result;
    }

    DisciplineCost cost = { 2, 2 };
    if (!PayCost(caster, cost, result))
        return result;

    tile.isLocked  = true;
    tile.blocksMove = true;
    tile.blocksLoS  = true;

    result.success = true;
    result.message = "Door sealed with blood magic.";
    return result;
}

// ---------------------------------------------------------------------------
// Blood Mark
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseBloodMark(GameWorld& world, FogOfWar& fow,
                                                 Character& caster, Character& target)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::BloodMark);
    if (rank < 1)
    {
        result.message = "Blood Mark not learned.";
        return result;
    }

    int range = 8 + rank * 2;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY,
                                          target.tileX, target.tileY);
    if (dist > range)
    {
        result.outOfRange = true;
        result.message = "Target out of Blood Mark range.";
        return result;
    }

    auto cost = GetBloodMarkCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    // Apply Blood Mark status
    int duration = 3 + rank * 2;  // Duration scales with rank
    int debuff   = 1 + rank / 2;  // Hit penalty on marked target
    target.statuses.Apply(StatusType::BloodMarked, duration, debuff);

    // Reveal target through fog
    fow.RevealTile(target.tileX, target.tileY);

    result.success = true;
    result.message = "Target marked with blood.";
    return result;
}

// ---------------------------------------------------------------------------
// Auspex
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseAuspexPulse(GameWorld& world, FogOfWar& fow,
                                                   Character& caster)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Auspex);
    if (rank < 1)
    {
        result.message = "Auspex not learned.";
        return result;
    }

    auto cost = GetAuspexCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    int radius = caster.derived.visionRadius + rank * 2;
    fow.RevealRadius(world.map, caster.tileX, caster.tileY, radius);

    // Reveal hidden characters in radius
    auto nearby = world.GetCharactersInRadius(caster.tileX, caster.tileY, radius);
    for (auto* c : nearby)
    {
        if (c == &caster) continue;
        if (c->isHidden)
        {
            c->isHidden = false; // Anti-stealth pulse
        }
        if (c->statuses.Has(StatusType::Obfuscated))
        {
            // Auspex can pierce obfuscation at high rank
            if (rank >= 3)
                c->statuses.Remove(StatusType::Obfuscated);
        }
    }

    result.success = true;
    result.message = "Auspex pulse reveals the area.";
    return result;
}

bool DisciplineSystem::HasSenseWeakness(const Character& caster, const Character& target) const
{
    if (caster.skills.GetRank(SkillId::Auspex) < 2) return false;
    return target.statuses.Has(StatusType::BloodMarked);
}

// ---------------------------------------------------------------------------
// Domination
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseDominationStun(GameWorld& world, Character& caster,
                                                      Character& target)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Domination);
    if (rank < 1)
    {
        result.message = "Domination not learned.";
        return result;
    }

    // Only works on humans (non-vampires) or weak-willed vampires
    if (target.isVampire && target.attributes.CHA() + target.skills.GetRank(SkillId::Domination) > 8)
    {
        result.message = "Target's will is too strong.";
        return result;
    }

    int range = 6;
    int dist = CoverSystem::TileDistance(caster.tileX, caster.tileY,
                                          target.tileX, target.tileY);
    if (dist > range)
    {
        result.outOfRange = true;
        result.message = "Target out of range.";
        return result;
    }

    auto cost = GetDominationCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    // Opposed check: caster CHA + Domination vs target CHA + willpower (use CHA as proxy)
    int casterScore = caster.SkillEffective(SkillId::Domination);
    int targetScore = target.attributes.CHA() + 5; // 5 = base willpower

    int casterRoll = Roll3d6();
    int targetRoll = Roll3d6();

    if ((casterScore - casterRoll) >= (targetScore - targetRoll))
    {
        target.statuses.Apply(StatusType::Stunned, 1 + rank / 2, 1);
        result.success = true;
        result.message = "Target stunned by mental force.";
    }
    else
    {
        result.success = false;
        result.message = "Target resisted domination.";
    }

    return result;
}

DisciplineResult DisciplineSystem::UseDominationCommand(GameWorld& world, Character& caster,
                                                         Character& target)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Domination);
    if (rank < 3)
    {
        result.message = "Domination rank 3 required to command.";
        return result;
    }

    // Must not be a vampire
    if (target.isVampire)
    {
        result.message = "Cannot command another vampire.";
        return result;
    }

    DisciplineCost cost = { 4, 3 };
    if (!PayCost(caster, cost, result))
        return result;

    int casterScore = caster.SkillEffective(SkillId::Domination);
    int targetScore = target.attributes.CHA() + 3;

    int casterRoll = Roll3d6();
    int targetRoll = Roll3d6();

    if ((casterScore - casterRoll) >= (targetScore - targetRoll))
    {
        target.statuses.Apply(StatusType::Dominated, 2, 1);
        result.success = true;
        result.message = "Target is under your command.";
    }
    else
    {
        result.success = false;
        result.message = "Target resisted your command.";
    }

    return result;
}

// ---------------------------------------------------------------------------
// Obfuscate
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseObfuscate(Character& caster)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Obfuscate);
    if (rank < 1)
    {
        result.message = "Obfuscate not learned.";
        return result;
    }

    auto cost = GetObfuscateCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    int duration = 2 + rank;
    caster.statuses.Apply(StatusType::Obfuscated, duration, rank);
    caster.isHidden = true;

    result.success = true;
    result.message = "You fade from sight.";
    return result;
}

bool DisciplineSystem::CanSeeThrough(const Character& observer,
                                      const Character& obfuscated) const
{
    if (!obfuscated.statuses.Has(StatusType::Obfuscated))
        return true; // Not obfuscated

    int obfRank = obfuscated.statuses.GetPotency(StatusType::Obfuscated);
    int perScore = observer.attributes.PER() + observer.skills.GetRank(SkillId::Auspex);

    // PER check with penalty based on obfuscate potency
    return Check3d6(perScore - obfRank * 2);
}

// ---------------------------------------------------------------------------
// Celerity
// ---------------------------------------------------------------------------
DisciplineResult DisciplineSystem::UseCelerity(Character& caster)
{
    DisciplineResult result;
    int rank = caster.skills.GetRank(SkillId::Celerity);
    if (rank < 1)
    {
        result.message = "Celerity not learned.";
        return result;
    }

    auto cost = GetCelerityCost(rank);
    if (!PayCost(caster, cost, result))
        return result;

    // Gain bonus AP this turn
    int bonusAP = 2 + rank; // Rank 1 = +3 AP, Rank 5 = +7 AP
    caster.currentAP += bonusAP;

    result.success = true;
    result.message = "Time seems to slow around you.";
    return result;
}

} // namespace vamp
