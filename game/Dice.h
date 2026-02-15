#pragma once
// Dice.h - Random number generation and dice rolling utilities

#include <cstdint>

namespace vamp
{

// Seed the RNG (call once at startup)
void DiceInit(uint32_t seed = 0);

// Roll a single die: returns [1, sides]
int Roll(int sides);

// Roll NdS: e.g. RollNd(3,6) = 3d6
int RollNd(int count, int sides);

// Standard 3d6 check: returns true if 3d6 <= target
bool Check3d6(int target);

// Roll 3d6 and return the result
int Roll3d6();

// Returns true if roll was a critical success (roll <= 4)
bool IsCritSuccess(int roll);

// Returns true if roll was a critical failure (roll >= 17)
bool IsCritFailure(int roll);

// Random int in [lo, hi] inclusive
int RandRange(int lo, int hi);

// Random float in [0.0, 1.0)
float RandFloat();

// Percentage check: returns true if random [1,100] <= percent
bool PercentCheck(int percent);

} // namespace vamp
