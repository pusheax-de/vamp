// SleepSystem.cpp - Sleep, safehouse, and feeding implementation

#include "SleepSystem.h"
#include "Dice.h"
#include <algorithm>

namespace vamp
{

int SleepSystem::CalculateAmbushChance(const GameWorld& world, const Character& player,
                                        const Safehouse& safehouse) const
{
    int baseChance = 10; // 10% base

    // Heat in the territory
    const Territory* territory = world.GetTerritoryAt(safehouse.tileX, safehouse.tileY);
    int heat = territory ? territory->heatLevel : world.globalHeat;

    int chance = baseChance
               + heat / 2                          // Heat raises danger
               - safehouse.securityRating * 3       // Security lowers it
               + world.globalHeat / 5;             // Global heat contributes

    // Territory control by hostile factions increases danger
    if (territory && territory->controllingFaction != Faction::Player &&
        territory->controllingFaction != safehouse.controlledBy)
    {
        chance += 10;
    }

    // Hunger (low BR) makes you emit traces that can be tracked
    if (player.isVampire && player.bloodReserve < player.maxBR / 3)
    {
        chance += 10;
    }

    return std::max(0, std::min(95, chance));
}

SleepResult SleepSystem::AttemptSleep(GameWorld& world, Character& player,
                                       const Safehouse& safehouse)
{
    SleepResult result;

    // Check access cost
    if (player.inventory.money < safehouse.accessCost)
    {
        result.message = "Cannot afford this safehouse.";
        return result;
    }

    player.inventory.money -= safehouse.accessCost;

    // Roll for ambush
    int ambushChance = CalculateAmbushChance(world, player, safehouse);
    result.ambushThreshold = ambushChance;
    result.ambushRoll = RandRange(1, 100);

    if (result.ambushRoll <= ambushChance)
    {
        // Ambushed!
        result.wasAmbushed = true;
        result.sleptSuccessfully = false;

        // Partial rest: some recovery but wake with reduced AP
        ApplyPartialRest(player);

        result.message = "You were ambushed during sleep!";
        return result;
    }

    // Successful sleep
    result.sleptSuccessfully = true;

    // Full BR recovery
    if (player.isVampire)
    {
        int brBefore = player.bloodReserve;
        player.FullRestBR();
        result.brRestored = player.bloodReserve - brBefore;
    }

    // HP recovery: heal 25% of max HP
    int hpBefore = player.currentHP;
    player.Heal(player.derived.maxHP / 4);
    result.hpRestored = player.currentHP - hpBefore;

    // Clear some status effects
    player.statuses.Remove(StatusType::Stunned);
    player.statuses.Remove(StatusType::Pinned);
    player.statuses.Remove(StatusType::Frenzied);

    // Advance day
    world.AdvanceDay();

    result.message = "You slept peacefully and recovered.";
    return result;
}

bool SleepSystem::DiscoverSafehouse(Character& player, Safehouse& safehouse)
{
    if (safehouse.isDiscovered) return true;

    // Requires Streetwise check
    if (player.SkillCheck(SkillId::Streetwise, 0))
    {
        safehouse.isDiscovered = true;
        return true;
    }
    return false;
}

std::vector<const Safehouse*> SleepSystem::GetAvailableSafehouses(
    const std::vector<Safehouse>& safehouses) const
{
    std::vector<const Safehouse*> available;
    for (const auto& s : safehouses)
    {
        if (s.isDiscovered && s.isAvailable)
            available.push_back(&s);
    }
    return available;
}

bool SleepSystem::CanAfford(const Character& player, const Safehouse& safehouse) const
{
    return player.inventory.money >= safehouse.accessCost;
}

void SleepSystem::ApplyPartialRest(Character& player)
{
    // Partial rest: restore 50% BR, no HP recovery
    if (player.isVampire)
    {
        int restore = player.maxBR / 2;
        player.RestoreBR(restore);
    }

    // Wake with reduced AP (set at next BeginTurn — but mark with a penalty)
    // We apply stunned for 1 turn to represent grogginess
    player.statuses.Apply(StatusType::Stunned, 1, 1);
}

SleepSystem::FeedResult SleepSystem::AttemptFeed(GameWorld& world, Character& vampire,
                                                  Character& target)
{
    FeedResult result;

    if (!vampire.isVampire)
    {
        result.message = "Not a vampire.";
        return result;
    }

    // Must be adjacent
    int dist = std::max(std::abs(vampire.tileX - target.tileX),
                        std::abs(vampire.tileY - target.tileY));
    if (dist > 1)
    {
        result.message = "Target is too far to feed.";
        return result;
    }

    // If target is alive and not an ally
    if (!target.isAlive)
    {
        result.message = "Target is dead.";
        return result;
    }

    // Stealth check if target is aware
    bool detected = false;
    if (!target.statuses.Has(StatusType::Stunned) &&
        !target.statuses.Has(StatusType::Dominated))
    {
        // Need stealth or domination to feed unnoticed
        if (!vampire.SkillCheck(SkillId::Stealth, 0))
        {
            detected = true;
        }
    }

    // Feed
    int brGained = 2 + vampire.bloodPotency / 2;
    vampire.RestoreBR(brGained);
    result.brRestored = brGained;

    // Feeding deals damage to target
    int feedDamage = 3;
    target.TakeDamage(feedDamage);
    result.targetKilled = target.IsDead();

    if (detected)
    {
        result.heatGained = 8;
        world.AddHeat(vampire.tileX, vampire.tileY, result.heatGained);
    }

    result.success = true;
    result.message = detected ? "You fed, but were seen!" : "You fed silently.";
    return result;
}

} // namespace vamp
