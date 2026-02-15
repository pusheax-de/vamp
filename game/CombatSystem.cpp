// CombatSystem.cpp - Turn-based combat resolution implementation

#include "CombatSystem.h"
#include "Dice.h"
#include <algorithm>
#include <cmath>

namespace vamp
{

// ---------------------------------------------------------------------------
// Turn Management
// ---------------------------------------------------------------------------
void CombatSystem::BeginCombat(GameWorld& world, const std::vector<Character*>& participants)
{
    m_combatActive = true;
    m_overwatchers.clear();
    m_combatStates.clear();
    m_currentTurnIndex = 0;

    // Build turn order
    m_turnOrder.clear();
    for (auto* c : participants)
    {
        if (c && c->isAlive)
        {
            m_turnOrder.push_back(c);
            CharacterCombatState cs;
            cs.character = c;
            cs.aimBonus  = 0;
            m_combatStates.push_back(cs);
        }
    }

    // Sort by initiative (descending)
    auto order = RollInitiative(m_turnOrder);
    m_turnOrder = order;
}

void CombatSystem::EndCombat()
{
    m_combatActive = false;
    m_overwatchers.clear();
    m_combatStates.clear();
    m_turnOrder.clear();
}

std::vector<Character*> CombatSystem::RollInitiative(std::vector<Character*>& participants)
{
    struct InitEntry
    {
        Character* c;
        int        initValue;
    };
    std::vector<InitEntry> entries;
    for (auto* c : participants)
    {
        int init = c->attributes.AGI() + c->attributes.PER() + Roll(6);
        entries.push_back({ c, init });
    }
    std::sort(entries.begin(), entries.end(),
        [](const InitEntry& a, const InitEntry& b) { return a.initValue > b.initValue; });

    std::vector<Character*> result;
    for (auto& e : entries)
        result.push_back(e.c);
    return result;
}

void CombatSystem::BeginTurn(Character& c)
{
    c.BeginTurn();

    // Reset aim if the character moved or was hit
    auto* state = GetCombatState(c);
    if (state)
        state->aimBonus = 0;
}

void CombatSystem::EndTurn(Character& c)
{
    c.EndTurn();
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------
bool CombatSystem::MoveCharacter(GameWorld& world, Character& c, int destX, int destY)
{
    if (!world.map.InBounds(destX, destY)) return false;
    const MapTile& tile = world.map.At(destX, destY);
    if (!tile.IsPassable()) return false;
    if (world.GetCharacterAt(destX, destY) != nullptr) return false;

    int cost = tile.moveCost;
    if (c.statuses.Has(StatusType::CrippledLeg))
        cost *= 2;

    if (!c.SpendAP(cost)) return false;

    c.tileX = destX;
    c.tileY = destY;
    c.isHidden = false;    // Moving breaks stealth
    c.noiseLevel += 2;     // Normal movement generates noise

    // Reset aim bonus on move
    auto* state = GetCombatState(c);
    if (state)
        state->aimBonus = 0;

    // Check if any overwatchers fire
    CheckOverwatch(world, c, destX, destY);

    return true;
}

bool CombatSystem::CrouchMove(GameWorld& world, Character& c, int destX, int destY)
{
    if (!world.map.InBounds(destX, destY)) return false;
    const MapTile& tile = world.map.At(destX, destY);
    if (!tile.IsPassable()) return false;

    int cost = tile.moveCost + 1; // Crouch costs +1 AP per tile
    if (c.statuses.Has(StatusType::CrippledLeg))
        cost *= 2;

    if (!c.SpendAP(cost)) return false;

    c.tileX = destX;
    c.tileY = destY;
    c.noiseLevel += 1; // Quieter than normal move

    // Don't break stealth on crouch-move
    CheckOverwatch(world, c, destX, destY);

    return true;
}

void CombatSystem::TakeCover(Character& c)
{
    c.SpendAP(1);
    // Cover is determined by tile data — this just spends the AP to "hunker down"
    // The actual cover benefit is calculated during attack resolution
}

// ---------------------------------------------------------------------------
// Ranged Combat
// ---------------------------------------------------------------------------
int CombatSystem::CalculateHitTarget(const Character& attacker, const Character& target,
                                     const GameWorld& world, int aimBonus, bool isBurst)
{
    const WeaponData& weapon = attacker.inventory.equipment.GetPrimaryWeapon();

    int effectiveSkill = attacker.SkillEffective(
        weapon.isMelee ? SkillId::Melee : SkillId::Firearms);

    // Range penalty: -1 per tile beyond effective range
    int dist = CoverSystem::TileDistance(attacker.tileX, attacker.tileY,
                                          target.tileX, target.tileY);
    int rangePenalty = 0;
    if (dist > weapon.range)
        rangePenalty = dist - weapon.range;

    // Cover penalty
    CoverInfo cover = CoverSystem::QueryCoverBetween(world.map,
        attacker.tileX, attacker.tileY, target.tileX, target.tileY);

    // Visibility (target in shadow / hidden)
    int visPenalty = 0;
    if (target.isHidden) visPenalty += 4;

    // Build target number: 3d6 <= this
    int targetNum = effectiveSkill
                  + aimBonus
                  + weapon.hitBonus
                  + attacker.GetHitModifier()
                  - rangePenalty
                  - cover.hitPenalty
                  - visPenalty;

    // Burst fire gives +2 to hit but costs more AP
    if (isBurst) targetNum += 2;

    return targetNum;
}

int CombatSystem::CalculateDamage(const WeaponData& weapon, bool critHit, int meleeBonusDmg)
{
    int damage = RollNd(weapon.damageDice, 6) + weapon.damageBonus + meleeBonusDmg;
    if (critHit)
        damage = static_cast<int>(damage * 1.5f);
    return std::max(1, damage);
}

void CombatSystem::ApplyWoundEffects(Character& target, int damage, bool critHit)
{
    // Bleeding: on any hit that deals 4+ damage
    if (damage >= 4)
    {
        target.statuses.Apply(StatusType::Bleeding, 0, 1); // Permanent until bandaged
    }

    // Stun: on hits dealing 6+ damage
    if (damage >= 6)
    {
        target.statuses.Apply(StatusType::Stunned, 2, 1);
    }

    // Cripple: on critical hits
    if (critHit)
    {
        // 50/50 leg or arm
        if (Roll(2) == 1)
            target.statuses.Apply(StatusType::CrippledLeg, 0, 1);
        else
            target.statuses.Apply(StatusType::CrippledArm, 0, 1);
    }
}

AttackResult CombatSystem::ShootSingle(GameWorld& world, Character& attacker, Character& target,
                                        int aimBonus)
{
    AttackResult result;
    const int AP_COST = 3;

    if (!attacker.SpendAP(AP_COST))
        return result;

    // Check ammo
    auto& equip = attacker.inventory.equipment;
    if (equip.GetPrimaryWeapon().magSize > 0 && equip.primaryAmmo <= 0)
        return result; // Need to reload

    if (equip.GetPrimaryWeapon().magSize > 0)
        equip.primaryAmmo--;

    // Check LoS
    if (!CoverSystem::HasLineOfSight(world.map,
        attacker.tileX, attacker.tileY, target.tileX, target.tileY))
        return result;

    // Get combat state aim bonus
    auto* state = GetCombatState(attacker);
    int totalAim = aimBonus + (state ? state->aimBonus : 0);

    result.targetNumber = CalculateHitTarget(attacker, target, world, totalAim, false);
    result.roll = Roll3d6();
    result.hit = (result.roll <= result.targetNumber);
    result.critSuccess = IsCritSuccess(result.roll);
    result.critFailure = IsCritFailure(result.roll);

    // Critical failure: weapon jam
    if (result.critFailure)
    {
        result.weaponJammed = true;
        return result;
    }

    if (result.hit || result.critSuccess)
    {
        result.damageDealt = CalculateDamage(equip.GetPrimaryWeapon(), result.critSuccess);
        target.TakeDamage(result.damageDealt);
        ApplyWoundEffects(target, result.damageDealt, result.critSuccess);
        result.targetKilled = target.IsDead();
        result.causedBleeding = target.statuses.Has(StatusType::Bleeding);
        result.causedStun = target.statuses.Has(StatusType::Stunned);
    }

    // Noise
    attacker.noiseLevel += equip.GetPrimaryWeapon().noiseLevel;

    // Generate heat
    world.AddHeat(attacker.tileX, attacker.tileY, equip.GetPrimaryWeapon().noiseLevel);

    // Reset aim after firing
    if (state) state->aimBonus = 0;

    // Break stealth
    attacker.isHidden = false;

    return result;
}

AttackResult CombatSystem::ShootBurst(GameWorld& world, Character& attacker, Character& target)
{
    AttackResult result;
    const int AP_COST = 4;

    const WeaponData& weapon = attacker.inventory.equipment.GetPrimaryWeapon();
    if (!weapon.canBurst) return result;
    if (!attacker.SpendAP(AP_COST)) return result;

    auto& equip = attacker.inventory.equipment;
    // Burst uses 3 ammo
    if (equip.primaryAmmo < 3) return result;
    equip.primaryAmmo -= 3;

    if (!CoverSystem::HasLineOfSight(world.map,
        attacker.tileX, attacker.tileY, target.tileX, target.tileY))
        return result;

    result.targetNumber = CalculateHitTarget(attacker, target, world, 0, true);
    result.roll = Roll3d6();
    result.hit = (result.roll <= result.targetNumber);
    result.critSuccess = IsCritSuccess(result.roll);
    result.critFailure = IsCritFailure(result.roll);

    if (result.critFailure)
    {
        result.weaponJammed = true;
        return result;
    }

    if (result.hit || result.critSuccess)
    {
        result.damageDealt = CalculateDamage(weapon, result.critSuccess);
        target.TakeDamage(result.damageDealt);
        ApplyWoundEffects(target, result.damageDealt, result.critSuccess);
        result.targetKilled = target.IsDead();
    }

    // Suppression effect even on miss
    if (!result.hit)
    {
        ApplySuppression(target);
    }

    attacker.noiseLevel += weapon.noiseLevel + 2;
    world.AddHeat(attacker.tileX, attacker.tileY, weapon.noiseLevel + 2);
    attacker.isHidden = false;

    auto* state = GetCombatState(attacker);
    if (state) state->aimBonus = 0;

    return result;
}

bool CombatSystem::Reload(Character& c)
{
    const int AP_COST = 2;
    if (!c.SpendAP(AP_COST)) return false;

    auto& equip = c.inventory.equipment;
    equip.primaryAmmo = equip.GetPrimaryWeapon().magSize;
    return true;
}

int CombatSystem::AddAim(Character& c)
{
    const int AP_COST = 1;
    const int MAX_AIM = 6;

    if (!c.SpendAP(AP_COST)) return 0;

    auto* state = GetCombatState(c);
    if (!state) return 0;

    state->aimBonus = std::min(state->aimBonus + 2, MAX_AIM);
    return state->aimBonus;
}

// ---------------------------------------------------------------------------
// Melee Combat
// ---------------------------------------------------------------------------
AttackResult CombatSystem::MeleeAttack(GameWorld& world, Character& attacker, Character& target)
{
    AttackResult result;

    int dist = CoverSystem::TileDistance(attacker.tileX, attacker.tileY,
                                          target.tileX, target.tileY);
    if (dist > 1) return result; // Must be adjacent

    const WeaponData& weapon = attacker.inventory.equipment.GetPrimaryWeapon();
    int apCost = weapon.isMelee ? weapon.apCostSingle : 2; // Default 2 for unarmed
    if (!attacker.SpendAP(apCost)) return result;

    int effectiveSkill = attacker.SkillEffective(SkillId::Melee);
    result.targetNumber = effectiveSkill + weapon.hitBonus + attacker.GetHitModifier();
    result.roll = Roll3d6();
    result.hit = (result.roll <= result.targetNumber);
    result.critSuccess = IsCritSuccess(result.roll);
    result.critFailure = IsCritFailure(result.roll);

    if (result.critFailure)
    {
        // Melee crit fail: attacker is exposed, lose balance
        attacker.statuses.Apply(StatusType::Stunned, 1, 1);
        return result;
    }

    if (result.hit || result.critSuccess)
    {
        int meleeDmgBonus = attacker.GetMeleeDamageBonus();
        result.damageDealt = CalculateDamage(weapon, result.critSuccess, meleeDmgBonus);
        target.TakeDamage(result.damageDealt);
        ApplyWoundEffects(target, result.damageDealt, result.critSuccess);
        result.targetKilled = target.IsDead();
    }

    attacker.noiseLevel += 1; // Melee is quiet
    attacker.isHidden = false;

    return result;
}

// ---------------------------------------------------------------------------
// Overwatch
// ---------------------------------------------------------------------------
void CombatSystem::SetOverwatch(Character& c, int dirX, int dirY)
{
    const int AP_COST = 2;
    if (!c.SpendAP(AP_COST)) return;

    OverwatchEntry entry;
    entry.watcher = &c;
    entry.dirX    = dirX;
    entry.dirY    = dirY;
    entry.active  = true;
    entry.fired   = false;
    m_overwatchers.push_back(entry);
}

void CombatSystem::CheckOverwatch(GameWorld& world, Character& mover, int newX, int newY)
{
    for (auto& ow : m_overwatchers)
    {
        if (!ow.active || ow.fired) continue;
        if (ow.watcher == &mover) continue;
        if (ow.watcher->faction == mover.faction) continue;
        if (!ow.watcher->isAlive) continue;

        // Check if mover is within vision and LoS
        int dist = CoverSystem::TileDistance(ow.watcher->tileX, ow.watcher->tileY, newX, newY);
        if (dist > ow.watcher->derived.visionRadius) continue;

        if (!CoverSystem::HasLineOfSight(world.map,
            ow.watcher->tileX, ow.watcher->tileY, newX, newY))
            continue;

        // Fire!
        ow.fired = true;

        // Overwatch shot is at -2 penalty (snap shot)
        int prevAP = ow.watcher->currentAP;
        ow.watcher->currentAP = 3; // Give enough AP for one shot
        // ShootSingle will handle hit calculation
        ShootSingle(world, *ow.watcher, mover, -2);
        ow.watcher->currentAP = prevAP; // Restore (overwatch doesn't cost AP on trigger)
    }
}

void CombatSystem::ClearOverwatch(Character& c)
{
    m_overwatchers.erase(
        std::remove_if(m_overwatchers.begin(), m_overwatchers.end(),
            [&c](const OverwatchEntry& ow) { return ow.watcher == &c; }),
        m_overwatchers.end());
}

// ---------------------------------------------------------------------------
// Stealth
// ---------------------------------------------------------------------------
bool CombatSystem::AttemptHide(GameWorld& world, Character& c)
{
    const int AP_COST = 2;
    if (!c.SpendAP(AP_COST)) return false;

    // Must be in shadow or behind cover
    const MapTile& tile = world.map.At(c.tileX, c.tileY);
    bool inShadow = tile.isShadow;
    bool hasCover = (tile.coverNorth != CoverLevel::None ||
                     tile.coverSouth != CoverLevel::None ||
                     tile.coverEast  != CoverLevel::None ||
                     tile.coverWest  != CoverLevel::None);

    if (!inShadow && !hasCover)
        return false; // Can't hide in the open

    int situational = 0;
    if (inShadow) situational += 2;
    if (hasCover) situational += 1;

    if (c.SkillCheck(SkillId::Stealth, situational))
    {
        c.isHidden = true;
        return true;
    }
    return false;
}

bool CombatSystem::DetectHidden(const Character& observer, const Character& hidden,
                                 const GameMap& map)
{
    if (!hidden.isHidden) return true; // Not hidden, always visible

    int dist = CoverSystem::TileDistance(observer.tileX, observer.tileY,
                                          hidden.tileX, hidden.tileY);
    int situational = -dist; // Harder to detect at range
    situational -= hidden.skills.GetRank(SkillId::Stealth); // Hidden char's stealth skill

    return observer.SkillCheck(SkillId::Tactics, situational);
}

// ---------------------------------------------------------------------------
// Suppression
// ---------------------------------------------------------------------------
void CombatSystem::ApplySuppression(Character& target)
{
    // Target must pass END check or become Pinned
    if (!target.SkillCheck(SkillId::Athletics, 0))
    {
        target.statuses.Apply(StatusType::Pinned, 1, 1);
    }
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------
bool CombatSystem::UseConsumable(Character& c, ConsumableType type)
{
    const ConsumableData& data = GetConsumableData(type);
    if (!c.SpendAP(data.apCost)) return false;

    switch (type)
    {
    case ConsumableType::Bandage:
        c.statuses.Remove(StatusType::Bleeding);
        // Higher Medicine rank also removes Stunned
        if (c.skills.GetRank(SkillId::Medicine) >= 3)
            c.statuses.Remove(StatusType::Stunned);
        break;

    case ConsumableType::Stimpack:
        c.Heal(data.healAmount);
        break;

    case ConsumableType::BloodVial:
        c.RestoreBR(data.brRestored);
        break;

    case ConsumableType::Antidote:
        c.statuses.Remove(StatusType::Poisoned);
        break;

    case ConsumableType::Flashbang:
        // Applied in a radius — caller should handle area effect
        break;

    default:
        break;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------
CombatSystem::CharacterCombatState* CombatSystem::GetCombatState(Character& c)
{
    for (auto& cs : m_combatStates)
    {
        if (cs.character == &c)
            return &cs;
    }
    return nullptr;
}

} // namespace vamp
