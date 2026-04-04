// GenerateTestScene.cpp - Builds a test .vmp scene for development
//
// Layout: 32x32 tile grid (32px tiles) centered at origin.
//
//   - Black background (no background pages)
//   - Perimeter walls around the entire map
//   - Several interior wall structures to test fog-of-war:
//       * L-shaped corridor (top-left)
//       * Small room (center-right)
//       * T-junction wall (bottom area)
//       * Scattered pillars
//   - 5 NPCs: guard, patrol, merchant, quest giver, civilian
//   - Ground items: machete, shotgun, ammo, bandage
//   - 1 shop (the merchant's tile)
//   - 1 safehouse
//   - 1 transition zone (exit to "street.vmp")
//   - 1 trigger (ambush)
//   - 1 territory covering the whole map
//   - Several lights (flickering torches near walls)
//   - Player spawn at center of the map
//   - 1 roof over the small room

#include "GenerateTestScene.h"
#include <cstring>

namespace vamp
{

// Helper to set a fixed-size char array from a string literal
static void SetFixedStr(char* dst, size_t dstSize, const char* src)
{
    std::memset(dst, 0, dstSize);
    size_t len = std::strlen(src);
    if (len >= dstSize) len = dstSize - 1;
    std::memcpy(dst, src, len);
}

// Helper to make a wall tile
static MapTile MakeWall()
{
    MapTile t;
    t.terrain   = TerrainType::Wall;
    t.blocksLoS = true;
    t.blocksMove = true;
    t.moveCost  = 0;
    return t;
}

// Helper to make a floor tile
static MapTile MakeFloor()
{
    MapTile t;
    t.terrain   = TerrainType::Floor;
    t.blocksLoS = false;
    t.blocksMove = false;
    t.moveCost  = 1;
    return t;
}

// Helper to make a shadow tile
static MapTile MakeShadow()
{
    MapTile t;
    t.terrain   = TerrainType::Shadow;
    t.blocksLoS = false;
    t.blocksMove = false;
    t.moveCost  = 1;
    t.isShadow  = true;
    return t;
}

// Helper to make a door tile
static MapTile MakeDoor(bool locked)
{
    MapTile t;
    t.terrain   = TerrainType::Door;
    t.blocksLoS = false;  // Open door
    t.blocksMove = false;
    t.moveCost  = 1;
    t.isLocked  = locked;
    return t;
}

// Build an NPC character with reasonable defaults
static Character MakeNPC(const char* name, Faction faction, bool vampire,
                          int tileX, int tileY,
                          WeaponType weapon, ArmorType armor,
                          int str, int agi, int end, int per, int intl, int cha)
{
    Character c;
    c.name      = name;
    c.faction   = faction;
    c.isVampire = vampire;
    c.isAlive   = true;
    c.tileX     = tileX;
    c.tileY     = tileY;

    c.attributes.Set(Attr::STR, str);
    c.attributes.Set(Attr::AGI, agi);
    c.attributes.Set(Attr::END, end);
    c.attributes.Set(Attr::PER, per);
    c.attributes.Set(Attr::INT, intl);
    c.attributes.Set(Attr::CHA, cha);

    auto derived = DerivedStats::Calculate(c.attributes);
    c.currentHP   = derived.maxHP;
    c.currentAP   = derived.apPerTurn;
    c.bloodReserve = vampire ? 6 : 0;
    c.maxBR        = vampire ? 6 : 0;
    c.bloodPotency = vampire ? 1 : 0;

    c.inventory.equipment.primaryWeapon  = weapon;
    c.inventory.equipment.armor          = armor;
    c.inventory.equipment.primaryAmmo    = 30;
    c.inventory.money = 50;

    return c;
}

bool GenerateTestScene(const std::string& filePath)
{
    const int W = 32;
    const int H = 32;
    const float TILE = 32.0f;
    // Center the grid at origin: origin = -W/2 * TILE, -H/2 * TILE
    const float OX = -(W / 2) * TILE;  // -512
    const float OY = -(H / 2) * TILE;  // -512

    SceneData scene;

    // --- Header ---
    scene.header.magic      = kSceneMagic;
    scene.header.version    = kSceneVersion;
    scene.header.gridWidth  = W;
    scene.header.gridHeight = H;
    scene.header.tileSize   = TILE;
    scene.header.originX    = OX;
    scene.header.originY    = OY;
    SetFixedStr(scene.header.sceneName, sizeof(scene.header.sceneName),
                "Test Arena");

    // --- Tile grid ---
    // Start with all floor
    scene.tiles.resize(W * H, MakeFloor());

    auto setTile = [&](int x, int y, const MapTile& t)
    {
        if (x >= 0 && x < W && y >= 0 && y < H)
            scene.tiles[y * W + x] = t;
    };

    // Perimeter walls
    for (int x = 0; x < W; ++x)
    {
        setTile(x, 0,     MakeWall());
        setTile(x, H - 1, MakeWall());
    }
    for (int y = 0; y < H; ++y)
    {
        setTile(0, y,     MakeWall());
        setTile(W - 1, y, MakeWall());
    }

    // === Interior wall structures ===

    // 1) L-shaped corridor in top-left (tiles 3..10, 3..10)
    //    Horizontal wall from (3,7) to (9,7)
    for (int x = 3; x <= 9; ++x)
        setTile(x, 7, MakeWall());
    //    Vertical wall from (9,3) to (9,7)
    for (int y = 3; y <= 7; ++y)
        setTile(9, y, MakeWall());
    //    Door in the horizontal wall
    setTile(6, 7, MakeDoor(false));

    // 2) Small enclosed room center-right (tiles 20..25, 12..17)
    for (int x = 20; x <= 25; ++x)
    {
        setTile(x, 12, MakeWall());
        setTile(x, 17, MakeWall());
    }
    for (int y = 12; y <= 17; ++y)
    {
        setTile(20, y, MakeWall());
        setTile(25, y, MakeWall());
    }
    // Door on the west wall
    setTile(20, 14, MakeDoor(false));
    // Locked door on the south wall
    setTile(22, 17, MakeDoor(true));
    // Shadow inside the room
    for (int x = 21; x <= 24; ++x)
        for (int y = 13; y <= 16; ++y)
            setTile(x, y, MakeShadow());

    // 3) T-junction in bottom area (tiles 8..22, 24)
    for (int x = 8; x <= 22; ++x)
        setTile(x, 24, MakeWall());
    // Vertical stem up from center of T
    for (int y = 20; y <= 24; ++y)
        setTile(15, y, MakeWall());
    // Gaps in the T for passage
    setTile(12, 24, MakeFloor());
    setTile(18, 24, MakeFloor());

    // 4) Scattered pillars (single wall tiles)
    setTile(5,  15, MakeWall());
    setTile(10, 20, MakeWall());
    setTile(14, 10, MakeWall());
    setTile(27, 5,  MakeWall());
    setTile(27, 25, MakeWall());
    setTile(4,  27, MakeWall());

    // 5) Cover crates (half cover on specific floor tiles)
    auto setCover = [&](int x, int y, CoverLevel n, CoverLevel s,
                         CoverLevel e, CoverLevel w)
    {
        if (x >= 0 && x < W && y >= 0 && y < H)
        {
            scene.tiles[y * W + x].coverNorth = n;
            scene.tiles[y * W + x].coverSouth = s;
            scene.tiles[y * W + x].coverEast  = e;
            scene.tiles[y * W + x].coverWest  = w;
        }
    };
    // Crate cluster near center
    setCover(14, 15, CoverLevel::Half, CoverLevel::Half,
                     CoverLevel::None, CoverLevel::None);
    setCover(16, 15, CoverLevel::None, CoverLevel::None,
                     CoverLevel::Half, CoverLevel::Half);
    // Dumpster in alley
    setCover(3,  20, CoverLevel::Full, CoverLevel::Full,
                     CoverLevel::Full, CoverLevel::Full);

    // --- Player spawn (center of map) ---
    scene.playerSpawnX = 15;
    scene.playerSpawnY = 15;

    // --- 5 NPCs ---

    // NPC 1: Brujah guard near the L-corridor entrance
    {
        SceneNPC npc;
        npc.character = MakeNPC("Viktor", Faction::VampireClanBrujah, true,
                                 5, 8, WeaponType::AssaultRifle, ArmorType::TacticalArmor,
                                 8, 7, 7, 6, 4, 4);
        npc.behavior   = NPCBehavior::Guard;
        npc.isHostile  = true;
        npc.isEssential = false;
        npc.dialogueId = 0;
        SetFixedStr(npc.tag, sizeof(npc.tag), "guard_viktor");
        scene.npcs.push_back(npc);
    }

    // NPC 2: Police patrol in the bottom T-junction area
    {
        SceneNPC npc;
        npc.character = MakeNPC("Officer Reyes", Faction::Police, false,
                                 10, 26, WeaponType::Pistol9mm, ArmorType::KevlarVest,
                                 6, 6, 6, 7, 5, 5);
        npc.behavior    = NPCBehavior::Patrol;
        npc.isHostile   = false;
        npc.isEssential = false;
        npc.patrolRouteId = 0;
        SetFixedStr(npc.tag, sizeof(npc.tag), "patrol_reyes");
        scene.npcs.push_back(npc);
    }

    // NPC 3: Merchant in the enclosed room
    {
        SceneNPC npc;
        npc.character = MakeNPC("Anya the Fence", Faction::Civilian, false,
                                 22, 14, WeaponType::Pistol9mm, ArmorType::LightClothing,
                                 4, 5, 5, 6, 7, 8);
        npc.behavior    = NPCBehavior::Merchant;
        npc.isHostile   = false;
        npc.isEssential = true;
        npc.dialogueId  = 1;
        SetFixedStr(npc.tag, sizeof(npc.tag), "merchant_anya");
        scene.npcs.push_back(npc);
    }

    // NPC 4: Nosferatu quest giver lurking in shadows near pillar
    {
        SceneNPC npc;
        npc.character = MakeNPC("Ratko", Faction::VampireClanNosferatu, true,
                                 5, 14, WeaponType::Knife, ArmorType::None,
                                 5, 9, 6, 10, 7, 3);
        npc.character.isHidden = true;
        npc.behavior    = NPCBehavior::QuestGiver;
        npc.isHostile   = false;
        npc.isEssential = true;
        npc.dialogueId  = 2;
        SetFixedStr(npc.tag, sizeof(npc.tag), "quest_ratko");
        scene.npcs.push_back(npc);
    }

    // NPC 5: Civilian bystander near center
    {
        SceneNPC npc;
        npc.character = MakeNPC("Homeless Guy", Faction::Civilian, false,
                                 18, 20, WeaponType::Unarmed, ArmorType::None,
                                 4, 3, 4, 4, 3, 4);
        npc.behavior    = NPCBehavior::Civilian;
        npc.isHostile   = false;
        npc.isEssential = false;
        SetFixedStr(npc.tag, sizeof(npc.tag), "civ_homeless");
        scene.npcs.push_back(npc);
    }

    // --- Patrol route for Officer Reyes ---
    {
        PatrolRoute route;
        route.routeId = 0;
        route.loops   = true;
        route.waypoints.push_back({ 10, 26, 2.0f });
        route.waypoints.push_back({ 18, 26, 1.0f });
        route.waypoints.push_back({ 18, 28, 1.5f });
        route.waypoints.push_back({ 10, 28, 2.0f });
        scene.patrolRoutes.push_back(route);
    }

    // --- Ground items ---

    // Machete on the floor in the L-corridor
    {
        SceneGroundItem gi;
        gi.tileX      = 6;
        gi.tileY      = 5;
        gi.type       = ItemType::Weapon;
        gi.templateId = static_cast<uint16_t>(WeaponType::Machete);
        gi.quantity   = 1;
        scene.groundItems.push_back(gi);
    }

    // Shotgun near the T-junction
    {
        SceneGroundItem gi;
        gi.tileX      = 13;
        gi.tileY      = 22;
        gi.type       = ItemType::Weapon;
        gi.templateId = static_cast<uint16_t>(WeaponType::Shotgun);
        gi.quantity   = 1;
        scene.groundItems.push_back(gi);
    }

    // Shotgun ammo nearby
    {
        SceneGroundItem gi;
        gi.tileX      = 13;
        gi.tileY      = 23;
        gi.type       = ItemType::Ammo;
        gi.templateId = static_cast<uint16_t>(WeaponType::Shotgun);
        gi.quantity   = 6;
        scene.groundItems.push_back(gi);
    }

    // Bandage near player spawn
    {
        SceneGroundItem gi;
        gi.tileX      = 16;
        gi.tileY      = 16;
        gi.type       = ItemType::Consumable;
        gi.templateId = static_cast<uint16_t>(ConsumableType::Bandage);
        gi.quantity   = 2;
        scene.groundItems.push_back(gi);
    }

    // Blood vial in the shadow room
    {
        SceneGroundItem gi;
        gi.tileX      = 23;
        gi.tileY      = 15;
        gi.type       = ItemType::Consumable;
        gi.templateId = static_cast<uint16_t>(ConsumableType::BloodVial);
        gi.quantity   = 1;
        scene.groundItems.push_back(gi);
    }

    // Knife near the homeless guy
    {
        SceneGroundItem gi;
        gi.tileX      = 19;
        gi.tileY      = 20;
        gi.type       = ItemType::Weapon;
        gi.templateId = static_cast<uint16_t>(WeaponType::Knife);
        gi.quantity   = 1;
        scene.groundItems.push_back(gi);
    }

    // --- Quest item ---
    {
        SceneQuestItem qi;
        qi.tileX      = 24;
        qi.tileY      = 13;
        qi.questId    = 1;
        qi.templateId = 100;  // "Ancient Medallion"
        qi.collected  = false;
        SetFixedStr(qi.tag, sizeof(qi.tag), "quest_medallion");
        scene.questItems.push_back(qi);
    }

    // --- Shop (at the merchant's location) ---
    {
        SceneShop shop;
        shop.tileX       = 22;
        shop.tileY       = 14;
        shop.faction     = Faction::Civilian;
        shop.inventoryId = 0;
        SetFixedStr(shop.shopName, sizeof(shop.shopName), "Anya's Black Market");
        SetFixedStr(shop.tag, sizeof(shop.tag), "shop_anya");
        scene.shops.push_back(shop);
    }

    // --- Safehouse ---
    {
        Safehouse sh;
        sh.name           = "Abandoned Metro Car";
        sh.tileX          = 3;
        sh.tileY          = 28;
        sh.securityRating = 3;
        sh.accessCost     = 0;
        sh.controlledBy   = Faction::Civilian;
        sh.isDiscovered   = false;
        sh.isAvailable    = true;
        scene.safehouses.push_back(sh);
    }

    // --- Transition zone (exit on east wall) ---
    {
        SceneTransition trans;
        trans.x0 = 30; trans.y0 = 14;
        trans.x1 = 30; trans.y1 = 16;
        trans.targetSpawnX = 1;
        trans.targetSpawnY = 15;
        trans.requiresKey  = false;
        SetFixedStr(trans.targetScene, sizeof(trans.targetScene), "street.vmp");
        SetFixedStr(trans.tag, sizeof(trans.tag), "exit_east");
        scene.transitions.push_back(trans);
    }
    // Clear wall tiles at transition so player can walk through
    setTile(30, 14, MakeFloor());
    setTile(30, 15, MakeFloor());
    setTile(30, 16, MakeFloor());

    // --- Trigger: ambush zone near the T-junction ---
    {
        SceneTrigger trg;
        trg.x0 = 14; trg.y0 = 21;
        trg.x1 = 16; trg.y1 = 23;
        trg.type     = TriggerType::Ambush;
        trg.oneShot  = true;
        trg.enabled  = true;
        trg.scriptId = 1;
        SetFixedStr(trg.tag, sizeof(trg.tag), "ambush_tjunction");
        scene.triggers.push_back(trg);
    }

    // --- Trigger: lore pickup in the L-corridor ---
    {
        SceneTrigger trg;
        trg.x0 = 4; trg.y0 = 4;
        trg.x1 = 4; trg.y1 = 4;
        trg.type     = TriggerType::LorePickup;
        trg.oneShot  = true;
        trg.enabled  = true;
        trg.scriptId = 2;
        SetFixedStr(trg.tag, sizeof(trg.tag), "lore_corridor");
        scene.triggers.push_back(trg);
    }

    // --- Lights ---

    // World position from tile (isometric):
    //   halfW = TILE/2, halfH = TILE/2 / sqrt(3)  (true isometric)
    //   cx = OX + (H + tx - ty) * halfW
    //   cy = OY + (tx + ty) * halfH + halfH
    auto tileWorld = [&](int tx, int ty, float& wx, float& wy)
    {
        float halfW = TILE * 0.5f;
        float halfH = TILE * 0.5f / 1.7320508f; // true isometric: halfW / sqrt(3)
        wx = OX + (H + tx - ty) * halfW;
        wy = OY + (tx + ty) * halfH + halfH;
    };

    // Flickering torch at L-corridor entrance
    {
        SceneLight light;
        tileWorld(6, 8, light.worldX, light.worldY);
        light.r = 1.0f; light.g = 0.7f; light.b = 0.3f;
        light.radius    = 160.0f;
        light.intensity = 1.0f;
        light.flickerPhase = 0.0f;
        SetFixedStr(light.tag, sizeof(light.tag), "torch_lcorr");
        scene.lights.push_back(light);
    }

    // Cool light inside the enclosed room
    {
        SceneLight light;
        tileWorld(22, 15, light.worldX, light.worldY);
        light.r = 0.4f; light.g = 0.5f; light.b = 1.0f;
        light.radius    = 120.0f;
        light.intensity = 0.8f;
        light.flickerPhase = 0.3f;
        SetFixedStr(light.tag, sizeof(light.tag), "light_room");
        scene.lights.push_back(light);
    }

    // Street light near center
    {
        SceneLight light;
        tileWorld(15, 15, light.worldX, light.worldY);
        light.r = 1.0f; light.g = 0.95f; light.b = 0.8f;
        light.radius    = 250.0f;
        light.intensity = 1.0f;
        light.flickerPhase = 0.0f;
        SetFixedStr(light.tag, sizeof(light.tag), "light_center");
        scene.lights.push_back(light);
    }

    // Dim red light near T-junction
    {
        SceneLight light;
        tileWorld(15, 22, light.worldX, light.worldY);
        light.r = 1.0f; light.g = 0.2f; light.b = 0.2f;
        light.radius    = 130.0f;
        light.intensity = 0.7f;
        light.flickerPhase = 0.6f;
        SetFixedStr(light.tag, sizeof(light.tag), "light_tjunc");
        scene.lights.push_back(light);
    }

    // Torch near safehouse
    {
        SceneLight light;
        tileWorld(3, 28, light.worldX, light.worldY);
        light.r = 1.0f; light.g = 0.6f; light.b = 0.2f;
        light.radius    = 100.0f;
        light.intensity = 0.9f;
        light.flickerPhase = 0.15f;
        SetFixedStr(light.tag, sizeof(light.tag), "torch_safe");
        scene.lights.push_back(light);
    }

    // Light near east exit
    {
        SceneLight light;
        tileWorld(29, 15, light.worldX, light.worldY);
        light.r = 0.8f; light.g = 0.9f; light.b = 1.0f;
        light.radius    = 140.0f;
        light.intensity = 1.0f;
        light.flickerPhase = 0.0f;
        SetFixedStr(light.tag, sizeof(light.tag), "light_exit");
        scene.lights.push_back(light);
    }

    // --- Roof over the enclosed room ---
    {
        SceneRoof roof;
        roof.x0 = 20; roof.y0 = 12;
        roof.x1 = 25; roof.y1 = 17;
        roof.textureId = 0;
        SetFixedStr(roof.tag, sizeof(roof.tag), "roof_room");
        scene.roofs.push_back(roof);
    }

    // --- Territory covering the whole map ---
    {
        Territory ter;
        ter.name              = "The Undercroft";
        ter.controllingFaction = Faction::VampireClanNosferatu;
        ter.controlStrength   = 60;
        ter.heatLevel         = 15;
        ter.dangerRating      = 3;
        ter.x0 = 0;  ter.y0 = 0;
        ter.x1 = 31; ter.y1 = 31;
        scene.territories.push_back(ter);
    }

    // --- Global state ---
    scene.globalHeat = 10;
    scene.dayCount   = 3;

    // --- No background pages (black background) ---
    // scene.backgroundPages is empty

    // --- Background image ---
    scene.backgroundImagePath = "assets\\hangar\\hangar.png";

    // --- Write file ---
    return SceneFile::Save(filePath, scene);
}

} // namespace vamp
