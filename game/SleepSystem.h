#pragma once
// SleepSystem.h - Safehouses, sleep, BR recovery, ambush mechanics

#include "Character.h"
#include "GameWorld.h"
#include <vector>
#include <string>

namespace vamp
{

// ---------------------------------------------------------------------------
// Safehouse - a location where the player can sleep
// ---------------------------------------------------------------------------
struct Safehouse
{
    std::string name;
    int         tileX           = 0;
    int         tileY           = 0;
    int         securityRating  = 0;    // 0-10: reduces ambush chance
    int         accessCost      = 0;    // Money or favor required
    Faction     controlledBy    = Faction::Civilian;
    bool        isDiscovered    = false; // Player must find it first
    bool        isAvailable     = true;  // Might be compromised
};

// ---------------------------------------------------------------------------
// Sleep result
// ---------------------------------------------------------------------------
struct SleepResult
{
    bool    sleptSuccessfully = false;
    bool    wasAmbushed       = false;
    int     ambushRoll        = 0;
    int     ambushThreshold   = 0;
    int     brRestored        = 0;
    int     hpRestored        = 0;
    std::string message;
};

// ---------------------------------------------------------------------------
// SleepSystem - manages rest, recovery, and ambush mechanics
// ---------------------------------------------------------------------------
class SleepSystem
{
public:
    // Attempt to sleep at a safehouse
    SleepResult AttemptSleep(GameWorld& world, Character& player, const Safehouse& safehouse);

    // Calculate ambush chance percentage
    int CalculateAmbushChance(const GameWorld& world, const Character& player,
                               const Safehouse& safehouse) const;

    // Discover a safehouse (via Streetwise or exploration)
    bool DiscoverSafehouse(Character& player, Safehouse& safehouse);

    // Get all discovered & available safehouses
    std::vector<const Safehouse*> GetAvailableSafehouses(
        const std::vector<Safehouse>& safehouses) const;

    // Check if player can afford to use a safehouse
    bool CanAfford(const Character& player, const Safehouse& safehouse) const;

    // Apply partial rest (interrupted sleep — less recovery)
    void ApplyPartialRest(Character& player);

    // Feeding: restore some BR by feeding on a human (risky)
    struct FeedResult
    {
        bool    success     = false;
        int     brRestored  = 0;
        int     heatGained  = 0;
        bool    targetKilled = false;
        std::string message;
    };

    FeedResult AttemptFeed(GameWorld& world, Character& vampire, Character& target);
};

} // namespace vamp
