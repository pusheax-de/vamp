// Dice.cpp - Random number generation and dice rolling utilities

#include "Dice.h"
#include <random>
#include <chrono>

namespace vamp
{

static std::mt19937 g_rng;

void DiceInit(uint32_t seed)
{
    if (seed == 0)
        seed = static_cast<uint32_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    g_rng.seed(seed);
}

int Roll(int sides)
{
    if (sides <= 0) return 0;
    std::uniform_int_distribution<int> dist(1, sides);
    return dist(g_rng);
}

int RollNd(int count, int sides)
{
    int total = 0;
    for (int i = 0; i < count; ++i)
        total += Roll(sides);
    return total;
}

bool Check3d6(int target)
{
    return Roll3d6() <= target;
}

int Roll3d6()
{
    return RollNd(3, 6);
}

bool IsCritSuccess(int roll)
{
    return roll <= 4;
}

bool IsCritFailure(int roll)
{
    return roll >= 17;
}

int RandRange(int lo, int hi)
{
    if (lo > hi) std::swap(lo, hi);
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(g_rng);
}

float RandFloat()
{
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(g_rng);
}

bool PercentCheck(int percent)
{
    return RandRange(1, 100) <= percent;
}

} // namespace vamp
