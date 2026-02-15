// GameWorld.cpp - World, map, territory, and clan implementation

#include "GameWorld.h"
#include "CoverSystem.h"
#include <algorithm>
#include <cmath>

namespace vamp
{

// ---------------------------------------------------------------------------
// GameMap
// ---------------------------------------------------------------------------
void GameMap::Init(int w, int h)
{
    width  = w;
    height = h;
    tiles.resize(w * h);
}

MapTile& GameMap::At(int x, int y)
{
    return tiles[y * width + x];
}

const MapTile& GameMap::At(int x, int y) const
{
    return tiles[y * width + x];
}

bool GameMap::InBounds(int x, int y) const
{
    return x >= 0 && x < width && y >= 0 && y < height;
}

// ---------------------------------------------------------------------------
// Territory
// ---------------------------------------------------------------------------
bool Territory::ContainsTile(int x, int y) const
{
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

// ---------------------------------------------------------------------------
// GameWorld
// ---------------------------------------------------------------------------
void GameWorld::InitClans()
{
    clans.clear();

    ClanInfo nosferatu;
    nosferatu.faction = Faction::VampireClanNosferatu;
    nosferatu.name = "Nosferatu";
    nosferatu.description = "Dwellers of the deep metro. Masters of stealth and information.";
    nosferatu.favoredDiscipline1 = SkillId::Obfuscate;
    nosferatu.favoredDiscipline2 = SkillId::Auspex;
    clans.push_back(nosferatu);

    ClanInfo tremere;
    tremere.faction = Faction::VampireClanTremere;
    tremere.name = "Tremere";
    tremere.description = "Blood sorcerers. Hemocraft is their birthright.";
    tremere.favoredDiscipline1 = SkillId::Hemocraft;
    tremere.favoredDiscipline2 = SkillId::BloodMark;
    clans.push_back(tremere);

    ClanInfo brujah;
    brujah.faction = Faction::VampireClanBrujah;
    brujah.name = "Brujah";
    brujah.description = "Street fighters and anarchists. Fast, brutal, uncompromising.";
    brujah.favoredDiscipline1 = SkillId::Celerity;
    brujah.favoredDiscipline2 = SkillId::Blink;
    clans.push_back(brujah);

    ClanInfo ventrue;
    ventrue.faction = Faction::VampireClanVentrue;
    ventrue.name = "Ventrue";
    ventrue.description = "The aristocracy. They rule through wealth and mental domination.";
    ventrue.favoredDiscipline1 = SkillId::Domination;
    ventrue.favoredDiscipline2 = SkillId::Telekinesis;
    clans.push_back(ventrue);

    ClanInfo malkavian;
    malkavian.faction = Faction::VampireClanMalkavian;
    malkavian.name = "Malkavian";
    malkavian.description = "Seers and madmen. Their visions pierce the veil of reality.";
    malkavian.favoredDiscipline1 = SkillId::Auspex;
    malkavian.favoredDiscipline2 = SkillId::Domination;
    clans.push_back(malkavian);
}

const Territory* GameWorld::GetTerritoryAt(int x, int y) const
{
    for (const auto& t : territories)
    {
        if (t.ContainsTile(x, y))
            return &t;
    }
    return nullptr;
}

void GameWorld::AddHeat(int x, int y, int amount)
{
    for (auto& t : territories)
    {
        if (t.ContainsTile(x, y))
        {
            t.heatLevel = std::min(100, t.heatLevel + amount);
        }
    }
    globalHeat = std::min(100, globalHeat + amount / 2);
}

Character* GameWorld::GetCharacterAt(int x, int y)
{
    for (auto& c : characters)
    {
        if (c.isAlive && c.tileX == x && c.tileY == y)
            return &c;
    }
    return nullptr;
}

Character* GameWorld::FindCharacterByName(const std::string& name)
{
    for (auto& c : characters)
    {
        if (c.name == name)
            return &c;
    }
    return nullptr;
}

void GameWorld::RemoveDeadCharacters()
{
    characters.erase(
        std::remove_if(characters.begin(), characters.end(),
            [](const Character& c) { return !c.isAlive; }),
        characters.end());
}

std::vector<Character*> GameWorld::GetCharactersInRadius(int cx, int cy, int radius)
{
    std::vector<Character*> result;
    for (auto& c : characters)
    {
        if (!c.isAlive) continue;
        int dx = c.tileX - cx;
        int dy = c.tileY - cy;
        if (std::max(std::abs(dx), std::abs(dy)) <= radius)
            result.push_back(&c);
    }
    return result;
}

std::vector<Character*> GameWorld::GetCharactersByFaction(Faction faction)
{
    std::vector<Character*> result;
    for (auto& c : characters)
    {
        if (c.isAlive && c.faction == faction)
            result.push_back(&c);
    }
    return result;
}

void GameWorld::AdvanceDay()
{
    ++dayCount;
    // Heat decays slightly each day
    for (auto& t : territories)
    {
        t.heatLevel = std::max(0, t.heatLevel - 5);
    }
    globalHeat = std::max(0, globalHeat - 3);
}

} // namespace vamp
