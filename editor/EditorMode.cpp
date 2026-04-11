// EditorMode.cpp - In-game tile/item editor implementation

#include "EditorMode.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

// Forward declarations
static void EnsureTerrainTexture(EditorState& editor,
                                 vamp::TerrainType terrain,
                                 engine::RendererD3D12& renderer);
static void EditorSubmitGroundTiles(EditorState& editor,
                                    const vamp::SceneData& scene,
                                    const engine::Grid& grid,
                                    engine::RenderQueue& renderQueue,
                                    engine::RendererD3D12& renderer);
static void EnsureObjectTexture(EditorState& editor,
                                vamp::SceneObjectType type,
                                const char* imagePath,
                                engine::RendererD3D12& renderer);
static void EditorSubmitObjects(EditorState& editor,
                                const vamp::SceneData& scene,
                                const engine::Grid& grid,
                                engine::RenderQueue& renderQueue,
                                engine::RendererD3D12& renderer);
static const char* SceneObjectDisplayName(vamp::SceneObjectType type);
static const char* SceneObjectPlacementName(vamp::SceneObjectPlacement placement);
static void FillObjectImagePath(vamp::SceneObjectType type, char (&imagePath)[128]);
static bool PlaceSelectedObject(vamp::SceneData& scene,
                                EditorState& editor,
                                vamp::SceneObjectType type,
                                const char* sourceLabel);
static int FindTopmostObjectAtTile(const vamp::SceneData& scene, int tx, int ty);
static void RefreshContextMenuItems(EditorState& editor, const vamp::SceneData& scene);
static bool GetPrimarySelectedTile(const EditorState& editor, int& tx, int& ty);
static std::vector<int> GetGroundItemIndicesAtTile(const vamp::SceneData& scene, int tx, int ty);
struct TilePlacedEntry
{
    bool isGroundItem = false;
    int  index = -1;
};
static std::vector<TilePlacedEntry> GetPlacedEntriesAtTile(const vamp::SceneData& scene, int tx, int ty);
static void SyncFocusedTileSelection(const vamp::SceneData& scene, EditorState& editor);
static const char* GroundItemName(const vamp::SceneGroundItem& gi);
static const char* ContextMenuPageTitle(EditorContextMenuPage page);
static void ApplyTerrainFromDropdown(int terrainId,
                                     vamp::SceneData& scene,
                                     EditorState& editor,
                                     vamp::SceneLoader& sceneLoader,
                                     engine::OccluderSet& occluders,
                                     const engine::Grid& grid);

// ---------------------------------------------------------------------------
// Helpers: build a MapTile from terrain type with sensible defaults
// ---------------------------------------------------------------------------
static vamp::MapTile MakeTile(vamp::TerrainType terrain)
{
    vamp::MapTile t;
    t.terrain = terrain;

    switch (terrain)
    {
    case vamp::TerrainType::Wall:
        t.blocksLoS  = true;
        t.blocksMove = true;
        t.moveCost   = 0;
        break;
    case vamp::TerrainType::Door:
        t.blocksLoS  = false;
        t.blocksMove = false;
        t.moveCost   = 1;
        break;
    case vamp::TerrainType::Rubble:
        t.moveCost = 2;
        break;
    case vamp::TerrainType::Water:
        t.moveCost = 2;
        break;
    case vamp::TerrainType::Shadow:
        t.isShadow = true;
        t.moveCost = 1;
        break;
    default:
        t.moveCost = 1;
        break;
    }
    return t;
}

// ---------------------------------------------------------------------------
// Terrain name for title bar
// ---------------------------------------------------------------------------
static const char* EditorTerrainName(vamp::TerrainType t)
{
    switch (t)
    {
    case vamp::TerrainType::Floor:      return "Floor";
    case vamp::TerrainType::Street:     return "Street";
    case vamp::TerrainType::Rubble:     return "Rubble";
    case vamp::TerrainType::Water:      return "Water";
    case vamp::TerrainType::Wall:       return "Wall";
    case vamp::TerrainType::Door:       return "Door";
    case vamp::TerrainType::MetroTrack: return "Metro";
    case vamp::TerrainType::Shadow:     return "Shadow";
    default:                            return "?";
    }
}

static const char* SceneObjectDisplayName(vamp::SceneObjectType type)
{
    switch (type)
    {
    case vamp::SceneObjectType::Hangar:   return "Hangar";
    case vamp::SceneObjectType::WallVert: return "Wall Vertical";
    default:                              return "Object";
    }
}

static const char* SceneObjectPlacementName(vamp::SceneObjectPlacement placement)
{
    switch (placement)
    {
    case vamp::SceneObjectPlacement::YMin:    return "Y Min";
    case vamp::SceneObjectPlacement::YMiddle: return "Y Middle";
    case vamp::SceneObjectPlacement::YMax:    return "Y Max";
    default:                                  return "Y Max";
    }
}

static const char* TerrainTexturePath(vamp::TerrainType terrain)
{
    switch (terrain)
    {
    case vamp::TerrainType::Floor:      return "assets\\floor\\floor.png";
    case vamp::TerrainType::Street:     return "assets\\floor\\street.png";
    case vamp::TerrainType::Rubble:     return "assets\\floor\\rubble.png";
    case vamp::TerrainType::Water:      return "assets\\floor\\water.png";
    case vamp::TerrainType::Wall:       return "assets\\floor\\wall.png";
    case vamp::TerrainType::Door:       return "assets\\floor\\door.png";
    case vamp::TerrainType::MetroTrack: return "assets\\floor\\metrotrack.png";
    case vamp::TerrainType::Shadow:     return "assets\\floor\\shadow.png";
    default:                            return "assets\\floor\\floor.png";
    }
}

static void FillObjectImagePath(vamp::SceneObjectType type, char (&imagePath)[128])
{
    std::memset(imagePath, 0, sizeof(imagePath));

    const char* path = "";
    switch (type)
    {
    case vamp::SceneObjectType::Hangar:
        path = "assets\\hangar\\hangar.png";
        break;
    case vamp::SceneObjectType::WallVert:
        path = "assets\\wall\\wall_vert.png";
        break;
    default:
        break;
    }

    size_t len = std::strlen(path);
    if (len >= sizeof(imagePath)) len = sizeof(imagePath) - 1;
    std::memcpy(imagePath, path, len);
}

static bool PlaceSelectedObject(vamp::SceneData& scene,
                                EditorState& editor,
                                vamp::SceneObjectType type,
                                const char* sourceLabel)
{
    if (editor.selectedTiles.empty())
        return false;

    int placed = 0;
    for (const auto& tc : editor.selectedTiles)
    {
        vamp::SceneObject obj;
        obj.type = type;
        obj.x0   = tc.x;
        obj.y0   = tc.y;
        obj.x1   = tc.x;
        obj.y1   = tc.y;
        obj.placement = vamp::SceneObjectPlacement::YMax;
        FillObjectImagePath(type, obj.imagePath);
        std::memset(obj.tag, 0, sizeof(obj.tag));
        scene.objects.push_back(obj);
        ++placed;
    }

    editor.dirty = true;
    std::ostringstream ss;
    ss << "[Editor] Placed " << SceneObjectDisplayName(type)
       << " on " << placed << " tile(s)";
    if (sourceLabel && sourceLabel[0])
        ss << " " << sourceLabel;
    ss << "\n";
    OutputDebugStringA(ss.str().c_str());

    editor.ClearSelection();
    return true;
}

static int FindTopmostObjectAtTile(const vamp::SceneData& scene, int tx, int ty)
{
    for (int i = static_cast<int>(scene.objects.size()) - 1; i >= 0; --i)
    {
        const vamp::SceneObject& obj = scene.objects[i];
        if (tx >= obj.x0 && tx <= obj.x1 &&
            ty >= obj.y0 && ty <= obj.y1)
        {
            return i;
        }
    }
    return -1;
}

static bool GetPrimarySelectedTile(const EditorState& editor, int& tx, int& ty)
{
    if (editor.selectedTiles.empty())
        return false;

    tx = editor.selectedTiles.back().x;
    ty = editor.selectedTiles.back().y;
    return true;
}

static std::vector<int> GetGroundItemIndicesAtTile(const vamp::SceneData& scene, int tx, int ty)
{
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(scene.groundItems.size()); ++i)
    {
        if (scene.groundItems[i].tileX == tx && scene.groundItems[i].tileY == ty)
            indices.push_back(i);
    }
    return indices;
}

static std::vector<TilePlacedEntry> GetPlacedEntriesAtTile(const vamp::SceneData& scene, int tx, int ty)
{
    std::vector<TilePlacedEntry> entries;
    for (int i = 0; i < static_cast<int>(scene.groundItems.size()); ++i)
    {
        if (scene.groundItems[i].tileX == tx && scene.groundItems[i].tileY == ty)
            entries.push_back({ true, i });
    }

    for (int i = 0; i < static_cast<int>(scene.objects.size()); ++i)
    {
        const vamp::SceneObject& obj = scene.objects[i];
        if (tx >= obj.x0 && tx <= obj.x1 &&
            ty >= obj.y0 && ty <= obj.y1)
        {
            entries.push_back({ false, i });
        }
    }

    return entries;
}

static const char* ContextMenuPageTitle(EditorContextMenuPage page)
{
    switch (page)
    {
    case EditorContextMenuPage::Root:           return "Tile Actions";
    case EditorContextMenuPage::Ground:         return "Ground";
    case EditorContextMenuPage::PlaceItem:      return "Place Item";
    case EditorContextMenuPage::PlaceObject:    return "Place Object";
    case EditorContextMenuPage::SelectedItem:   return "Selected Item";
    case EditorContextMenuPage::SelectedObject: return "Selected Object";
    default:                                    return "Tile Actions";
    }
}

static void SyncFocusedTileSelection(const vamp::SceneData& scene, EditorState& editor)
{
    int tx = 0;
    int ty = 0;
    if (!GetPrimarySelectedTile(editor, tx, ty))
    {
        editor.selectedPlacedOrdinal = -1;
        editor.selectedGroundItemOrdinal = -1;
        editor.selectedObjectIndex = -1;
        return;
    }

    std::vector<TilePlacedEntry> entries = GetPlacedEntriesAtTile(scene, tx, ty);
    if (entries.empty())
    {
        editor.selectedPlacedOrdinal = -1;
        editor.selectedGroundItemOrdinal = -1;
        editor.selectedObjectIndex = -1;
    }
    else
    {
        if (editor.selectedPlacedOrdinal < 0)
            editor.selectedPlacedOrdinal = 0;
        editor.selectedPlacedOrdinal %= static_cast<int>(entries.size());

        int groundOrdinal = -1;
        for (int i = 0; i <= editor.selectedPlacedOrdinal; ++i)
        {
            if (entries[i].isGroundItem)
                ++groundOrdinal;
        }

        const TilePlacedEntry& selected = entries[editor.selectedPlacedOrdinal];
        if (selected.isGroundItem)
        {
            editor.selectedGroundItemOrdinal = groundOrdinal;
            editor.selectedObjectIndex = -1;
        }
        else
        {
            editor.selectedGroundItemOrdinal = -1;
            editor.selectedObjectIndex = selected.index;
        }
    }
}

static std::string BuildSelectedGroundItemLabel(const vamp::SceneData& scene,
                                                const EditorState& editor,
                                                int tx, int ty)
{
    std::vector<int> groundItems = GetGroundItemIndicesAtTile(scene, tx, ty);
    if (groundItems.empty())
        return std::string();

    int ordinal = editor.selectedGroundItemOrdinal;
    if (ordinal < 0 || ordinal >= static_cast<int>(groundItems.size()))
        ordinal = 0;

    const vamp::SceneGroundItem& gi = scene.groundItems[groundItems[ordinal]];
    std::ostringstream ss;
    ss << "Selected Item " << (ordinal + 1) << "/" << groundItems.size()
       << ": " << GroundItemName(gi);
    if (gi.quantity > 1)
        ss << " x" << gi.quantity;
    return ss.str();
}

static std::string BuildSelectedPlacedLabel(const vamp::SceneData& scene,
                                            const EditorState& editor,
                                            int tx, int ty)
{
    std::vector<TilePlacedEntry> entries = GetPlacedEntriesAtTile(scene, tx, ty);
    if (entries.empty())
        return std::string();

    int ordinal = editor.selectedPlacedOrdinal;
    if (ordinal < 0 || ordinal >= static_cast<int>(entries.size()))
        ordinal = 0;

    const TilePlacedEntry& selected = entries[ordinal];
    std::ostringstream ss;
    ss << "Selected " << (ordinal + 1) << "/" << entries.size() << ": ";
    if (selected.isGroundItem)
    {
        const vamp::SceneGroundItem& gi = scene.groundItems[selected.index];
        ss << GroundItemName(gi);
        if (gi.quantity > 1)
            ss << " x" << gi.quantity;
    }
    else
    {
        const vamp::SceneObject& obj = scene.objects[selected.index];
        ss << SceneObjectDisplayName(obj.type)
           << " (" << SceneObjectPlacementName(obj.placement) << ")";
    }
    return ss.str();
}

static void RefreshContextMenuItems(EditorState& editor, const vamp::SceneData& scene)
{
    if (!editor.itemDropdown)
        return;

    std::vector<ui::DropdownItem> itemItems;
    int primaryX = 0;
    int primaryY = 0;
    bool hasPrimaryTile = GetPrimarySelectedTile(editor, primaryX, primaryY);
    std::vector<int> groundItems = hasPrimaryTile
        ? GetGroundItemIndicesAtTile(scene, primaryX, primaryY)
        : std::vector<int>();

    switch (editor.contextMenuPage)
    {
    case EditorContextMenuPage::Root:
        itemItems.push_back({ 7000, "Ground >" });
        itemItems.push_back({ 7001, "Place Item >" });
        itemItems.push_back({ 7002, "Place Object >" });
        if (editor.selectedGroundItemOrdinal >= 0 &&
            editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
            itemItems.push_back({ 7003, "Selected Item >" });
        if (editor.selectedObjectIndex >= 0 &&
            editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
            itemItems.push_back({ 7004, "Selected Object >" });
        itemItems.push_back({ 9999, "[Delete First Item/Object At Tile]" });
        break;

    case EditorContextMenuPage::Ground:
        itemItems.push_back({ 7099, "< Back" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Floor),      "Floor" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Street),     "Street" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Rubble),     "Rubble" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Water),      "Water" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Wall),       "Wall" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Door),       "Door" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::MetroTrack), "Metro Track" });
        itemItems.push_back({ 5000 + static_cast<int>(vamp::TerrainType::Shadow),     "Shadow" });
        break;

    case EditorContextMenuPage::PlaceItem:
        itemItems.push_back({ 7099, "< Back" });
        itemItems.push_back({ 2000 + static_cast<int>(vamp::ConsumableType::Bandage),   "Bandage" });
        itemItems.push_back({ 2000 + static_cast<int>(vamp::ConsumableType::Stimpack),  "Stimpack" });
        itemItems.push_back({ 2000 + static_cast<int>(vamp::ConsumableType::Antidote),  "Antidote" });
        itemItems.push_back({ 2000 + static_cast<int>(vamp::ConsumableType::Flashbang), "Flashbang" });
        itemItems.push_back({ 2000 + static_cast<int>(vamp::ConsumableType::BloodVial), "Blood Vial" });
        break;

    case EditorContextMenuPage::PlaceObject:
        itemItems.push_back({ 7099, "< Back" });
        itemItems.push_back({ 1000, "Hangar" });
        itemItems.push_back({ 1001, "Wall Vertical" });
        break;

    case EditorContextMenuPage::SelectedItem:
        itemItems.push_back({ 7099, "< Back" });
        if (editor.selectedGroundItemOrdinal >= 0 &&
            editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
        {
            itemItems.push_back({ 4000, "Place On Selection" });
            itemItems.push_back({ 9997, "[Delete Selected Ground Item]" });
        }
        break;

    case EditorContextMenuPage::SelectedObject:
        itemItems.push_back({ 7099, "< Back" });
        if (editor.selectedObjectIndex >= 0 &&
            editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
        {
            itemItems.push_back({ 3000 + static_cast<int>(vamp::SceneObjectPlacement::YMin),    "Placement: Y Min" });
            itemItems.push_back({ 3000 + static_cast<int>(vamp::SceneObjectPlacement::YMiddle), "Placement: Y Middle" });
            itemItems.push_back({ 3000 + static_cast<int>(vamp::SceneObjectPlacement::YMax),    "Placement: Y Max" });
            itemItems.push_back({ 9998, "[Delete Selected Object]" });
        }
        break;
    }

    editor.itemDropdown->SetItems(itemItems);
    editor.itemDropdown->SetPlaceholder(ContextMenuPageTitle(editor.contextMenuPage));
}

// ---------------------------------------------------------------------------
// Helpers: rebuild occluders from current scene tile data
// ---------------------------------------------------------------------------
static void RebuildOccluders(vamp::SceneLoader& sceneLoader,
                             const vamp::SceneData& scene,
                             engine::OccluderSet& occluders,
                             const engine::Grid& grid)
{
    auto losMap = sceneLoader.BuildLoSBlockMap();
    uint32_t w = scene.header.gridWidth;
    uint32_t h = scene.header.gridHeight;
    std::vector<char> losChars(w * h, 0);
    for (uint32_t i = 0; i < w * h; ++i)
        losChars[i] = losMap[i] ? 1 : 0;

    occluders.Clear();
    if (grid.IsIsometric())
        occluders.BuildFromTileGridIsometric(reinterpret_cast<const bool*>(losChars.data()),
                                             static_cast<int>(w), static_cast<int>(h), grid);
    else
        occluders.BuildFromTileGrid(reinterpret_cast<const bool*>(losChars.data()),
                                    static_cast<int>(w), static_cast<int>(h),
                                    scene.header.tileSize,
                                    scene.header.originX, scene.header.originY);
}

// ---------------------------------------------------------------------------
// Helpers: terrain hotkey mapping (tile selection mode)
// ---------------------------------------------------------------------------
static bool HandleTerrainHotkey(const engine::InputSystem& input,
                                vamp::SceneData& scene,
                                EditorState& editor,
                                vamp::SceneLoader& sceneLoader,
                                engine::OccluderSet& occluders,
                                const engine::Grid& grid)
{
    struct Mapping { int vk; vamp::TerrainType type; };
    static const Mapping mappings[] = {
        { 'F', vamp::TerrainType::Floor },
        { 'T', vamp::TerrainType::Street },
        { 'R', vamp::TerrainType::Rubble },
        { 'W', vamp::TerrainType::Water },
        { 'L', vamp::TerrainType::Wall },
        { 'D', vamp::TerrainType::Door },
        { 'M', vamp::TerrainType::MetroTrack },
        { 'H', vamp::TerrainType::Shadow },
    };

    for (const auto& m : mappings)
    {
        if (input.IsKeyPressed(m.vk))
        {
            bool losChanged = false;
            int applied = 0;

            for (const auto& tc : editor.selectedTiles)
            {
                if (!scene.InBounds(tc.x, tc.y))
                    continue;

                bool oldBlocksLoS = scene.TileAt(tc.x, tc.y).blocksLoS;
                scene.TileAt(tc.x, tc.y) = MakeTile(m.type);
                bool newBlocksLoS = MakeTile(m.type).blocksLoS;
                if (oldBlocksLoS != newBlocksLoS)
                    losChanged = true;
                ++applied;
            }

            if (applied > 0)
            {
                editor.dirty = true;

                if (losChanged)
                {
                    RebuildOccluders(sceneLoader, scene, occluders, grid);
                    OutputDebugStringA("[Editor] Occluders rebuilt.\n");
                }

                std::ostringstream ss;
                ss << "[Editor] " << applied << " tile(s) -> "
                   << EditorTerrainName(m.type) << "\n";
                OutputDebugStringA(ss.str().c_str());
            }

            editor.ClearSelection();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helpers: item hotkey mapping (item selection mode)
// ---------------------------------------------------------------------------
static bool HandleItemHotkey(const engine::InputSystem& input,
                             vamp::SceneData& scene,
                             EditorState& editor)
{
    SyncFocusedTileSelection(scene, editor);

    if (input.IsKeyPressed('P'))
    {
        int tx = 0;
        int ty = 0;
        if (GetPrimarySelectedTile(editor, tx, ty))
        {
            std::vector<int> groundItems = GetGroundItemIndicesAtTile(scene, tx, ty);
            if (editor.selectedGroundItemOrdinal >= 0 &&
                editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
            {
                const vamp::SceneGroundItem source = scene.groundItems[groundItems[editor.selectedGroundItemOrdinal]];
                int placed = 0;
                for (const auto& tc : editor.selectedTiles)
                {
                    vamp::SceneGroundItem gi = source;
                    gi.tileX = tc.x;
                    gi.tileY = tc.y;
                    scene.groundItems.push_back(gi);
                    ++placed;
                }

                if (placed > 0)
                {
                    editor.dirty = true;
                    std::ostringstream ss;
                    ss << "[Editor] Copied selected item to " << placed << " tile(s)\n";
                    OutputDebugStringA(ss.str().c_str());
                }
                return true;
            }
        }
    }

    if (input.IsKeyPressed(VK_DELETE))
    {
        int deleted = 0;
        if (editor.selectedGroundItemOrdinal >= 0)
        {
            int tx = 0;
            int ty = 0;
            if (GetPrimarySelectedTile(editor, tx, ty))
            {
                std::vector<int> groundItems = GetGroundItemIndicesAtTile(scene, tx, ty);
                if (editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
                {
                    scene.groundItems.erase(scene.groundItems.begin() + groundItems[editor.selectedGroundItemOrdinal]);
                    editor.selectedGroundItemOrdinal = -1;
                    ++deleted;
                }
            }
        }
        else if (editor.selectedObjectIndex >= 0)
        {
            scene.objects.erase(scene.objects.begin() + editor.selectedObjectIndex);
            editor.selectedObjectIndex = -1;
            ++deleted;
        }

        if (deleted > 0)
        {
            editor.dirty = true;
            SyncFocusedTileSelection(scene, editor);
            OutputDebugStringA("[Editor] Deleted selected item/object.\n");
        }
        else
        {
            OutputDebugStringA("[Editor] Nothing selected to delete.\n");
        }
        return true;
    }

    // Object placement: 1 = Hangar, 2 = Wall Vertical
    if (input.IsKeyPressed('1') && !editor.selectedTiles.empty())
    {
        PlaceSelectedObject(scene, editor, vamp::SceneObjectType::Hangar, "");
        return true;
    }
    if (input.IsKeyPressed('2') && !editor.selectedTiles.empty())
    {
        PlaceSelectedObject(scene, editor, vamp::SceneObjectType::WallVert, "");
        return true;
    }

    // Consumable placement hotkeys
    struct ConsMapping { int vk; vamp::ConsumableType type; };
    static const ConsMapping consMappings[] = {
        { 'B', vamp::ConsumableType::Bandage },
        { 'S', vamp::ConsumableType::Stimpack },
        { 'A', vamp::ConsumableType::Antidote },
        { 'F', vamp::ConsumableType::Flashbang },
        { 'V', vamp::ConsumableType::BloodVial },
    };

    for (const auto& m : consMappings)
    {
        if (input.IsKeyPressed(m.vk))
        {
            int placed = 0;
            for (const auto& tc : editor.selectedTiles)
            {
                vamp::SceneGroundItem gi;
                gi.tileX      = tc.x;
                gi.tileY      = tc.y;
                gi.type       = vamp::ItemType::Consumable;
                gi.templateId = static_cast<uint16_t>(m.type);
                gi.quantity   = 1;
                scene.groundItems.push_back(gi);
                ++placed;
            }

            if (placed > 0)
            {
                editor.dirty = true;
                const auto& data = vamp::GetConsumableData(m.type);
                std::ostringstream ss;
                ss << "[Editor] Placed " << data.name << " at "
                   << placed << " tile(s)\n";
                OutputDebugStringA(ss.str().c_str());
            }

            editor.ClearSelection();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Draw selection highlight (hex outline for each selected tile)
// ---------------------------------------------------------------------------
static void EditorDrawSelection(engine::SceneRenderer& sceneRenderer,
                                engine::RendererD3D12& renderer,
                                engine::Grid& grid,
                                const EditorState& editor)
{
    if (editor.selection == EditorSelection::None || editor.selectedTiles.empty())
        return;

    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(editor.selectedTiles.size() * 12);

    for (const auto& tc : editor.selectedTiles)
    {
        if (!grid.InBounds(tc.x, tc.y))
            continue;

        DirectX::XMFLOAT2 hex[6];
        grid.TileHexVertices(tc.x, tc.y, hex);

        for (int i = 0; i < 6; ++i)
        {
            verts.push_back(hex[i]);
            verts.push_back(hex[(i + 1) % 6]);
        }
    }

    if (verts.empty())
        return;

    // Yellow for tile selection, green for item selection
    float r = (editor.selection == EditorSelection::Tile) ? 1.0f : 0.2f;
    float g = 1.0f;
    float b = 0.0f;

    sceneRenderer.DrawLineOverlay(renderer, verts.data(), verts.size(), r, g, b, 1.0f);
}

// ---------------------------------------------------------------------------
// Helpers: resolve a ground item's display name
// ---------------------------------------------------------------------------
static const char* GroundItemName(const vamp::SceneGroundItem& gi)
{
    switch (gi.type)
    {
    case vamp::ItemType::Weapon:
        if (static_cast<int>(gi.templateId) < static_cast<int>(vamp::WeaponType::COUNT))
            return vamp::GetWeaponData(static_cast<vamp::WeaponType>(gi.templateId)).name;
        return "Weapon";
    case vamp::ItemType::Consumable:
        if (static_cast<int>(gi.templateId) < static_cast<int>(vamp::ConsumableType::COUNT))
            return vamp::GetConsumableData(static_cast<vamp::ConsumableType>(gi.templateId)).name;
        return "Consumable";
    case vamp::ItemType::Ammo:
        return "Ammo";
    case vamp::ItemType::Armor:
        return "Armor";
    case vamp::ItemType::KeyItem:
        return "Key Item";
    case vamp::ItemType::Misc:
    default:
        return "Item";
    }
}

// ---------------------------------------------------------------------------
// Build the overlay text lines for the editor HUD
// ---------------------------------------------------------------------------
static void EditorUpdateOverlay(EditorState& editor,
                                const engine::InputSystem& input,
                                const engine::Grid& grid,
                                const vamp::SceneData* scene)
{
    // Line 1: Hover coordinates
    {
        std::ostringstream ss;
        ss << "Hover: [" << input.GetHoverTileX() << ", "
           << input.GetHoverTileY() << "]";
        if (editor.dirty)
            ss << "  *";
        if (editor.uiHoverLabel)
            editor.uiHoverLabel->SetText(ss.str());
    }

    // Line 2: Current tile contents / selection focus
    {
        std::ostringstream ss;
        int tx = 0;
        int ty = 0;
        bool haveTile = GetPrimarySelectedTile(editor, tx, ty);
        if (!haveTile && scene && input.IsHoverTileValid())
        {
            tx = input.GetHoverTileX();
            ty = input.GetHoverTileY();
            haveTile = true;
        }

        if (scene && haveTile && scene->InBounds(tx, ty))
        {
            ss << "Tile [" << tx << ", " << ty << "]: "
               << EditorTerrainName(scene->TileAt(tx, ty).terrain);

            std::string itemLabel = BuildSelectedPlacedLabel(*scene, editor, tx, ty);
            if (!itemLabel.empty())
                ss << " | " << itemLabel;

            int objectIndex = FindTopmostObjectAtTile(*scene, tx, ty);
            if (objectIndex >= 0 && objectIndex != editor.selectedObjectIndex)
            {
                const vamp::SceneObject& obj = scene->objects[objectIndex];
                ss << " | Object: " << SceneObjectDisplayName(obj.type)
                   << " (" << SceneObjectPlacementName(obj.placement) << ")";
            }
        }
        if (editor.uiTileInfoLabel)
            editor.uiTileInfoLabel->SetText(ss.str());
    }

    // Line 3: Common shortcuts
    {
        std::ostringstream ss;
        ss << "LMB=Select Tile  Shift+LMB=Add  RMB=Context Menu  Wheel=Cycle Tile Items  "
           << "P=Copy Selected Item  1/2=Objects  B/S/A/F/V=Consumables  "
           << "F/T/R/W/L/D/M/H=Ground  Del=Delete Selected  Ctrl+S=Save  Esc=Cancel";
        if (editor.uiHintsLabel)
            editor.uiHintsLabel->SetText(ss.str());
    }
}

// ---------------------------------------------------------------------------
// Update window title (simplified -- details are in the overlay now)
// ---------------------------------------------------------------------------
static void EditorUpdateTitle(HWND hWnd, const EditorState& editor)
{
    std::ostringstream ss;
    ss << "Vampire Editor";
    if (editor.dirty)
        ss << " *";
    SetWindowTextA(hWnd, ss.str().c_str());
}

// ---------------------------------------------------------------------------
// Draw wall overlay (same logic as DrawWallOverlay in vampire.cpp)
// ---------------------------------------------------------------------------
static void EditorDrawWalls(engine::SceneRenderer& sceneRenderer,
                            engine::RendererD3D12& renderer,
                            engine::Grid& grid,
                            const vamp::SceneData& scene)
{
    int gridW = scene.header.gridWidth;
    int gridH = scene.header.gridHeight;

    std::vector<DirectX::XMFLOAT2> wallVerts;
    wallVerts.reserve(gridW * gridH);

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            if (scene.tiles[y * gridW + x].terrain != vamp::TerrainType::Wall)
                continue;

            DirectX::XMFLOAT2 hex[6];
            grid.TileHexVertices(x, y, hex);

            for (int edge = 0; edge < 6; ++edge)
            {
                int nx, ny;
                engine::Grid::HexNeighbor(x, y, edge, nx, ny);
                bool neighborIsWall = (nx >= 0 && nx < gridW && ny >= 0 && ny < gridH)
                    && scene.tiles[ny * gridW + nx].terrain == vamp::TerrainType::Wall;
                if (!neighborIsWall)
                {
                    wallVerts.push_back(hex[edge]);
                    wallVerts.push_back(hex[(edge + 1) % 6]);
                }
            }
        }
    }

    if (!wallVerts.empty())
        sceneRenderer.DrawLineOverlay(renderer, wallVerts.data(), wallVerts.size(),
                                       0.45f, 0.25f, 0.1f, 0.8f);
}

// ---------------------------------------------------------------------------
// Initialize editor UI overlay
// ---------------------------------------------------------------------------
void EditorInitUI(ui::UISystem& uiSystem, EditorState& editor)
{
    // Semi-transparent panel in top-left corner
    const float panelW = 620.0f;
    const float lineH  = 20.0f;
    const float pad    = 6.0f;
    const float panelH = lineH * 3.0f + pad * 2.0f;
    const float dropdownY = panelH + pad * 3.0f;
    const float terrainDropdownX = pad;
    const float itemDropdownX = pad + 220.0f;

    auto* panel = uiSystem.CreatePanel(nullptr, "editor_hud",
                                        pad, pad, panelW, panelH,
                                        { 0.08f, 0.08f, 0.08f, 0.75f },
                                        ui::Anchor::TopLeft);
    panel->SetBorderColor({ 0.4f, 0.4f, 0.4f, 0.6f });
    panel->SetBorderWidth(1.0f);
    editor.uiPanel = panel;

    editor.uiHoverLabel = uiSystem.CreateLabel(
        panel, "editor_hover", "",
        pad, pad, panelW - pad * 2.0f, lineH,
        { 0.9f, 0.9f, 0.5f, 1.0f }, ui::TextAlign::Left);

    editor.uiTileInfoLabel = uiSystem.CreateLabel(
        panel, "editor_tileinfo", "",
        pad, pad + lineH, panelW - pad * 2.0f, lineH,
        ui::Color::White(), ui::TextAlign::Left);

    editor.uiHintsLabel = uiSystem.CreateLabel(
        panel, "editor_hints", "",
        pad, pad + lineH * 2.0f, panelW - pad * 2.0f, lineH,
        { 0.7f, 0.7f, 0.7f, 1.0f }, ui::TextAlign::Left);

    // --- Terrain dropdown (shown when tiles are selected via LMB) ---
    {
        std::vector<ui::DropdownItem> terrainItems;
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Floor),      "Floor" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Street),     "Street" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Rubble),     "Rubble" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Water),      "Water" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Wall),       "Wall" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Door),       "Door" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::MetroTrack), "Metro Track" });
        terrainItems.push_back({ static_cast<int>(vamp::TerrainType::Shadow),     "Shadow" });

        editor.terrainDropdown = uiSystem.CreateDropdown(
            nullptr, "editor_terrain_dd",
            terrainDropdownX, dropdownY, 200.0f,
            terrainItems, ui::Anchor::TopLeft);
        editor.terrainDropdown->SetPlaceholder("Set Terrain...");
        editor.terrainDropdown->SetVisible(false);
    }

    // --- Item dropdown (shown when tiles are selected via RMB) ---
    {
        editor.itemDropdown = uiSystem.CreateDropdown(
            nullptr, "editor_item_dd",
            itemDropdownX, dropdownY, 200.0f,
            std::vector<ui::DropdownItem>(), ui::Anchor::TopLeft);
        editor.itemDropdown->SetPlaceholder("Place Item...");
        editor.itemDropdown->SetVisible(false);
    }
}

// ---------------------------------------------------------------------------
// Apply terrain from dropdown selection
// ---------------------------------------------------------------------------
static void ApplyTerrainFromDropdown(int terrainId,
                                      vamp::SceneData& scene,
                                      EditorState& editor,
                                      vamp::SceneLoader& sceneLoader,
                                      engine::OccluderSet& occluders,
                                      const engine::Grid& grid)
{
    vamp::TerrainType type = static_cast<vamp::TerrainType>(terrainId);
    bool losChanged = false;
    int applied = 0;

    for (const auto& tc : editor.selectedTiles)
    {
        if (!scene.InBounds(tc.x, tc.y))
            continue;

        bool oldBlocksLoS = scene.TileAt(tc.x, tc.y).blocksLoS;
        scene.TileAt(tc.x, tc.y) = MakeTile(type);
        bool newBlocksLoS = MakeTile(type).blocksLoS;
        if (oldBlocksLoS != newBlocksLoS)
            losChanged = true;
        ++applied;
    }

    if (applied > 0)
    {
        editor.dirty = true;

        if (losChanged)
        {
            RebuildOccluders(sceneLoader, scene, occluders, grid);
            OutputDebugStringA("[Editor] Occluders rebuilt.\n");
        }

        std::ostringstream ss;
        ss << "[Editor] " << applied << " tile(s) -> "
           << EditorTerrainName(type) << " (dropdown)\n";
        OutputDebugStringA(ss.str().c_str());
    }
    SyncFocusedTileSelection(scene, editor);
}

// ---------------------------------------------------------------------------
// Apply item placement from dropdown selection
// ---------------------------------------------------------------------------
static void ApplyItemFromDropdown(int itemId,
                                   vamp::SceneData& scene,
                                   EditorState& editor,
                                   vamp::SceneLoader& sceneLoader,
                                   engine::OccluderSet& occluders,
                                    const engine::Grid& grid)
{
    SyncFocusedTileSelection(scene, editor);

    if (itemId == 7099)
    {
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }

    if (itemId == 7000) { editor.contextMenuPage = EditorContextMenuPage::Ground; return; }
    if (itemId == 7001) { editor.contextMenuPage = EditorContextMenuPage::PlaceItem; return; }
    if (itemId == 7002) { editor.contextMenuPage = EditorContextMenuPage::PlaceObject; return; }
    if (itemId == 7003) { editor.contextMenuPage = EditorContextMenuPage::SelectedItem; return; }
    if (itemId == 7004) { editor.contextMenuPage = EditorContextMenuPage::SelectedObject; return; }

    if (itemId >= 5000 && itemId < 6000)
    {
        ApplyTerrainFromDropdown(itemId - 5000, scene, editor, sceneLoader, occluders, grid);
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::Ground;
        return;
    }

    if (itemId == 4000)
    {
        int tx = 0;
        int ty = 0;
        if (GetPrimarySelectedTile(editor, tx, ty))
        {
            std::vector<int> groundItems = GetGroundItemIndicesAtTile(scene, tx, ty);
            if (editor.selectedGroundItemOrdinal >= 0 &&
                editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
            {
                const vamp::SceneGroundItem source = scene.groundItems[groundItems[editor.selectedGroundItemOrdinal]];
                int placed = 0;
                for (const auto& tc : editor.selectedTiles)
                {
                    vamp::SceneGroundItem gi = source;
                    gi.tileX = tc.x;
                    gi.tileY = tc.y;
                    scene.groundItems.push_back(gi);
                    ++placed;
                }

                if (placed > 0)
                {
                    editor.dirty = true;
                    std::ostringstream ss;
                    ss << "[Editor] Copied selected item to " << placed << " tile(s) (context)\n";
                    OutputDebugStringA(ss.str().c_str());
                }
            }
        }
        editor.contextMenuPage = EditorContextMenuPage::SelectedItem;
        return;
    }

    if (itemId == 9997)
    {
        int tx = 0;
        int ty = 0;
        if (GetPrimarySelectedTile(editor, tx, ty))
        {
            std::vector<int> groundItems = GetGroundItemIndicesAtTile(scene, tx, ty);
            if (editor.selectedGroundItemOrdinal >= 0 &&
                editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
            {
                scene.groundItems.erase(scene.groundItems.begin() + groundItems[editor.selectedGroundItemOrdinal]);
                editor.selectedGroundItemOrdinal = -1;
                editor.dirty = true;
                OutputDebugStringA("[Editor] Deleted selected ground item (context)\n");
            }
        }
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::SelectedItem;
        return;
    }

    if (itemId >= 3000 && itemId < 3010 &&
        editor.selectedObjectIndex >= 0 &&
        editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
    {
        vamp::SceneObjectPlacement placement =
            static_cast<vamp::SceneObjectPlacement>(itemId - 3000);
        scene.objects[editor.selectedObjectIndex].placement = placement;
        editor.dirty = true;

        std::ostringstream ss;
        ss << "[Editor] " << SceneObjectDisplayName(scene.objects[editor.selectedObjectIndex].type)
           << " placement -> " << SceneObjectPlacementName(placement) << " (dropdown)\n";
        OutputDebugStringA(ss.str().c_str());
        editor.contextMenuPage = EditorContextMenuPage::SelectedObject;
        return;
    }

    if (itemId == 9998 &&
        editor.selectedObjectIndex >= 0 &&
        editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
    {
        scene.objects.erase(scene.objects.begin() + editor.selectedObjectIndex);
        editor.selectedObjectIndex = -1;
        editor.dirty = true;
        OutputDebugStringA("[Editor] Deleted selected object (dropdown)\n");
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }

    // Delete action
    if (itemId == 9999)
    {
        int deleted = 0;
        for (const auto& tc : editor.selectedTiles)
        {
            auto& items = scene.groundItems;
            auto it = std::find_if(items.begin(), items.end(),
                [&tc](const vamp::SceneGroundItem& gi) {
                    return gi.tileX == tc.x && gi.tileY == tc.y;
                });
            if (it != items.end())
            {
                items.erase(it);
                ++deleted;
            }

            auto& objs = scene.objects;
            auto oit = std::find_if(objs.begin(), objs.end(),
                [&tc](const vamp::SceneObject& obj) {
                    return tc.x >= obj.x0 && tc.x <= obj.x1 &&
                           tc.y >= obj.y0 && tc.y <= obj.y1;
                });
            if (oit != objs.end())
            {
                objs.erase(oit);
                ++deleted;
            }
        }

        if (deleted > 0)
        {
            editor.dirty = true;
            std::ostringstream ss;
            ss << "[Editor] Deleted " << deleted << " item(s)/object(s) (dropdown)\n";
            OutputDebugStringA(ss.str().c_str());
        }
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }

    // Object placements
    if (itemId == 1000 && !editor.selectedTiles.empty())
    {
        PlaceSelectedObject(scene, editor, vamp::SceneObjectType::Hangar, "(dropdown)");
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }
    if (itemId == 1001 && !editor.selectedTiles.empty())
    {
        PlaceSelectedObject(scene, editor, vamp::SceneObjectType::WallVert, "(dropdown)");
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }

    // Consumable placement (IDs 2000+)
    if (itemId >= 2000 && itemId < 3000)
    {
        vamp::ConsumableType ctype = static_cast<vamp::ConsumableType>(itemId - 2000);
        int placed = 0;
        for (const auto& tc : editor.selectedTiles)
        {
            vamp::SceneGroundItem gi;
            gi.tileX      = tc.x;
            gi.tileY      = tc.y;
            gi.type       = vamp::ItemType::Consumable;
            gi.templateId = static_cast<uint16_t>(ctype);
            gi.quantity   = 1;
            scene.groundItems.push_back(gi);
            ++placed;
        }

        if (placed > 0)
        {
            editor.dirty = true;
            const auto& data = vamp::GetConsumableData(ctype);
            std::ostringstream ss;
            ss << "[Editor] Placed " << data.name << " at "
               << placed << " tile(s) (dropdown)\n";
            OutputDebugStringA(ss.str().c_str());
        }

        editor.contextMenuPage = EditorContextMenuPage::PlaceItem;
        return;
    }
}

// ---------------------------------------------------------------------------
// Main editor frame
// ---------------------------------------------------------------------------
void EditorFrame(engine::RendererD3D12& renderer,
    engine::SceneRenderer& sceneRenderer,
    engine::Camera2D& camera,
    engine::Grid& grid,
    engine::BackgroundPager& bgPager,
    engine::RenderQueue& renderQueue,
    engine::LightSystem& lights,
    engine::OccluderSet& occluders,
    engine::FogRenderer& fog,
    engine::RoofSystem& roofs,
    engine::InputSystem& input,
    vamp::SceneLoader& sceneLoader,
    EditorState& editor,
    ui::UISystem& uiSystem,
    HWND hWnd,
    float& time,
    LARGE_INTEGER& timerFreq,
    LARGE_INTEGER& timerLast) {
    // --- Delta time ---
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = static_cast<float>(now.QuadPart - timerLast.QuadPart)
        / static_cast<float>(timerFreq.QuadPart);
    timerLast = now;
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;
    time += deltaTime;

    // --- Input ---
    const bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    input.SetRMBDragEnabled(false);
    input.SetZoomEnabled(ctrlHeld);
    input.Update(deltaTime, camera, grid);

    // --- Esc: cancel current selection ---
    if (input.IsKeyPressed(VK_ESCAPE)) {
        if (editor.itemDropdown && editor.itemDropdown->IsOpen())
            editor.itemDropdown->Close();
        if (editor.selection != EditorSelection::None) {
            editor.ClearSelection();
            OutputDebugStringA("[Editor] Selection cancelled.\n");
        }
    }

    // --- Ctrl+S: save ---
    bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (input.IsKeyPressed('S') && ctrlHeld) {
        if (sceneLoader.IsLoaded() && !editor.scenePath.empty()) {
            if (vamp::SceneFile::Save(editor.scenePath, sceneLoader.GetSceneData())) {
                editor.dirty = false;
                OutputDebugStringA("[Editor] Scene saved.\n");
            }
            else {
                OutputDebugStringA("[Editor] ERROR: Failed to save scene!\n");
            }
        }
    }

    if (sceneLoader.IsLoaded())
        SyncFocusedTileSelection(sceneLoader.GetSceneData(), editor);

    // --- Context menu + selection ---
    {
        bool anyDropdownOpen = false;
        bool leftClickConsumedByUI = false;
        if (editor.itemDropdown)
        {
            editor.itemDropdown->SetVisible(editor.itemDropdown->IsOpen());
            editor.itemDropdown->SetMousePos(
                static_cast<float>(input.GetMouseX()),
                static_cast<float>(input.GetMouseY()));
            if (editor.itemDropdown->IsOpen())
                anyDropdownOpen = true;
        }

        // Forward character input to open dropdowns
        if (anyDropdownOpen && input.HasChar())
        {
            uiSystem.HandleChar(input.GetChar());
            input.ConsumeChar();
        }

        // Handle dropdown scroll
        if (anyDropdownOpen && input.GetScrollDelta() != 0.0f)
        {
            float mx = static_cast<float>(input.GetMouseX());
            float my = static_cast<float>(input.GetMouseY());
            if (editor.terrainDropdown && editor.terrainDropdown->IsOpen())
                editor.terrainDropdown->HandleScroll(mx, my, input.GetScrollDelta());
            if (editor.itemDropdown && editor.itemDropdown->IsOpen())
                editor.itemDropdown->HandleScroll(mx, my, input.GetScrollDelta());
        }

        // Handle dropdown clicks (LMB press)
        if (input.IsMousePressed(engine::MouseButton::Left))
        {
            float mx = static_cast<float>(input.GetMouseX());
            float my = static_cast<float>(input.GetMouseY());

            // Pending selection from dropdown callback
            static int s_pendingTerrainId = -1;
            static int s_pendingItemId    = -1;

            // Set up one-shot callbacks
            if (editor.itemDropdown && editor.itemDropdown->IsVisible())
            {
                editor.itemDropdown->SetOnSelect(
                    [](int id, const std::string&) { s_pendingItemId = id; });
                if (editor.itemDropdown->HandleClick(mx, my))
                {
                    if (s_pendingItemId >= 0 && sceneLoader.IsLoaded())
                    {
                        const bool reopenForNavigation =
                            (s_pendingItemId == 7099) ||
                            (s_pendingItemId >= 7000 && s_pendingItemId <= 7004);
                        ApplyItemFromDropdown(s_pendingItemId,
                            sceneLoader.GetSceneData(), editor,
                            sceneLoader, occluders, grid);
                        RefreshContextMenuItems(editor, sceneLoader.GetSceneData());
                        if (reopenForNavigation)
                        {
                            editor.itemDropdown->SetVisible(true);
                            editor.itemDropdown->Open();
                        }
                    }
                    s_pendingItemId = -1;
                    leftClickConsumedByUI = true;
                }
            }
        }

        // --- LMB: select tiles ---
        if (!leftClickConsumedByUI && input.WasTileClicked() && input.IsHoverTileValid()) {
            int tx = input.GetClickTileX();
            int ty = input.GetClickTileY();

            if (shiftHeld && editor.selection == EditorSelection::Tile) {
                if (editor.HasTile(tx, ty)) {
                    auto& tiles = editor.selectedTiles;
                    tiles.erase(std::remove(tiles.begin(), tiles.end(), TileCoord{tx, ty}), tiles.end());
                    if (tiles.empty())
                        editor.selection = EditorSelection::None;
                }
                else {
                    editor.AddTile(tx, ty);
                }
            }
            else {
                editor.SetSingleTile(tx, ty);
                editor.selection = EditorSelection::Tile;
                editor.selectedPlacedOrdinal = -1;
            }

            if (sceneLoader.IsLoaded())
                SyncFocusedTileSelection(sceneLoader.GetSceneData(), editor);

            std::ostringstream ss;
            ss << "[Editor] Tile selection: " << editor.selectedTiles.size() << " tile(s)\n";
            OutputDebugStringA(ss.str().c_str());
        }
        // --- RMB: open context menu for hovered/selected tile ---
        else if (input.WasTileRightClicked()) {
            if (editor.selection == EditorSelection::Tile &&
                !editor.selectedTiles.empty() &&
                sceneLoader.IsLoaded())
            {
                SyncFocusedTileSelection(sceneLoader.GetSceneData(), editor);
                editor.contextMenuPage = EditorContextMenuPage::Root;
                RefreshContextMenuItems(editor, sceneLoader.GetSceneData());
                if (editor.itemDropdown)
                {
                    editor.itemDropdown->SetPosition(
                        static_cast<float>(input.GetMouseX()),
                        static_cast<float>(input.GetMouseY()));
                    editor.itemDropdown->SetVisible(true);
                    editor.itemDropdown->Open();
                }
            }
        }

        if (!ctrlHeld && !anyDropdownOpen && sceneLoader.IsLoaded() &&
            input.GetScrollDelta() != 0.0f)
        {
            int tx = 0;
            int ty = 0;
            if (GetPrimarySelectedTile(editor, tx, ty))
            {
                std::vector<TilePlacedEntry> entries = GetPlacedEntriesAtTile(sceneLoader.GetSceneData(), tx, ty);
                if (entries.size() > 1)
                {
                    int step = (input.GetScrollDelta() > 0.0f) ? -1 : 1;
                    int count = static_cast<int>(entries.size());
                    int ordinal = editor.selectedPlacedOrdinal;
                    if (ordinal < 0 || ordinal >= count)
                        ordinal = 0;
                    ordinal = (ordinal + step + count) % count;
                    editor.selectedPlacedOrdinal = ordinal;
                    SyncFocusedTileSelection(sceneLoader.GetSceneData(), editor);
                    std::ostringstream ss;
                    ss << "[Editor] Selected placed object " << (ordinal + 1)
                       << "/" << count << " at [" << tx << "," << ty << "]\n";
                    OutputDebugStringA(ss.str().c_str());
                }
                else if (entries.size() == 1)
                {
                    editor.selectedPlacedOrdinal = 0;
                    SyncFocusedTileSelection(sceneLoader.GetSceneData(), editor);
                }
            }
        }
    }

    // --- Handle active selection hotkeys ---
    if (editor.selection == EditorSelection::Tile) {
        HandleTerrainHotkey(input, sceneLoader.GetSceneData(), editor,
            sceneLoader, occluders, grid);
        HandleItemHotkey(input, sceneLoader.GetSceneData(), editor);
    }

    // --- Update title bar ---
    EditorUpdateTitle(hWnd, editor);

    // --- Update overlay labels ---
    const vamp::SceneData* scenePtr = sceneLoader.IsLoaded() ? &sceneLoader.GetSceneData() : nullptr;
    EditorUpdateOverlay(editor, input, grid, scenePtr);

    // --- Render ---
    lights.Update(time);
    renderer.BeginFrame();

    renderQueue.Clear();

    // Submit textured ground tiles and placed objects as sprites
    if (sceneLoader.IsLoaded())
    {
        EditorSubmitGroundTiles(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);
        EditorSubmitObjects(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);
    }

    // Fog: fully visible in editor (no gameplay fog)
    fog.ClearVisible();
    fog.SetAllVisible();
    fog.UpdateExplored();

    bgPager.Update(camera, renderer.GetFrameIndex());

    sceneRenderer.RenderFrame(renderer, camera, bgPager,
        renderQueue, lights, occluders,
        fog, roofs, time);

    // Always draw grid + walls + selection in editor
    renderer.TransitionBackBufferToRT();

    sceneRenderer.DrawGridOverlay(renderer, camera, grid,
        1.0f, 1.0f, 1.0f, 0.15f);

    if (sceneLoader.IsLoaded())
        EditorDrawWalls(sceneRenderer, renderer, grid, sceneLoader.GetSceneData());

    EditorDrawSelection(sceneRenderer, renderer, grid, editor);

    // --- Render UI overlay ---
    uiSystem.Update();
    uiSystem.Render(renderer, sceneRenderer.GetPipelineStates());

    renderer.TransitionBackBufferToPresent();
    renderer.EndFrame();

    input.EndFrame();
}

// ---------------------------------------------------------------------------
// Load terrain texture on demand (called during render with active cmd list)
// ---------------------------------------------------------------------------
static void EnsureTerrainTexture(EditorState& editor,
    vamp::TerrainType terrain,
    engine::RendererD3D12& renderer) {
    int idx = static_cast<int>(terrain);
    if (editor.terrainTexturesLoaded[idx])
        return;

    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t sl = dir.find_last_of(L"\\/");
    if (sl != std::wstring::npos)
        dir = dir.substr(0, sl + 1);

    const char* imagePath = TerrainTexturePath(terrain);
    std::wstring widePath(imagePath, imagePath + std::strlen(imagePath));
    std::wstring fullPath = dir + widePath;

    if (editor.terrainTextures[idx].LoadFromPNG(
        renderer.GetDevice(), renderer.GetCommandList(),
        renderer.GetUploadManager(), renderer.GetSRVHeap(), fullPath))
    {
        editor.terrainTexturesLoaded[idx] = true;
    }
    else
    {
        editor.terrainTexturesLoaded[idx] = true;
    }
}

// ---------------------------------------------------------------------------
// Submit textured ground tiles as sprites
// ---------------------------------------------------------------------------
static void EditorSubmitGroundTiles(EditorState& editor,
    const vamp::SceneData& scene,
    const engine::Grid& grid,
    engine::RenderQueue& renderQueue,
    engine::RendererD3D12& renderer) {
    const int gridW = static_cast<int>(scene.header.gridWidth);
    const int gridH = static_cast<int>(scene.header.gridHeight);

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            const vamp::MapTile& tile = scene.tiles[y * gridW + x];
            EnsureTerrainTexture(editor, tile.terrain, renderer);

            const int texIdx = static_cast<int>(tile.terrain);
            if (!editor.terrainTexturesLoaded[texIdx] ||
                !editor.terrainTextures[texIdx].IsValid())
                continue;

            auto center = grid.TileToWorld(x, y);
            DirectX::XMFLOAT2 top, right, bottom, left;
            grid.TileDiamondVertices(x, y, top, right, bottom, left);

            engine::SpriteInstance inst;
            inst.position = center;
            inst.size = { right.x - left.x, bottom.y - top.y };
            inst.uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
            inst.color = { 1.0f, 1.0f, 1.0f, 1.0f };
            inst.rotation = 0.0f;
            inst.sortY = center.y;
            inst.textureIndex = editor.terrainTextures[texIdx].GetSRVIndex();
            inst.pad = 0;

            renderQueue.Submit(engine::RenderLayer::GroundTiles, inst.sortY, 0, 0, inst);
        }
    }
}

// ---------------------------------------------------------------------------
// Load object texture on demand (called during render with active cmd list)
// ---------------------------------------------------------------------------
static void EnsureObjectTexture(EditorState& editor,
    vamp::SceneObjectType type,
    const char* imagePath,
    engine::RendererD3D12& renderer) {
    int idx = static_cast<int>(type);
    if (editor.objectTexturesLoaded[idx])
        return;

    // Resolve path relative to executable directory
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t sl = dir.find_last_of(L"\\/");
    if (sl != std::wstring::npos)
        dir = dir.substr(0, sl + 1);

    std::wstring widePath(imagePath, imagePath + std::strlen(imagePath));
    std::wstring fullPath = dir + widePath;

    if (editor.objectTextures[idx].LoadFromPNG(
        renderer.GetDevice(), renderer.GetCommandList(),
        renderer.GetUploadManager(), renderer.GetSRVHeap(), fullPath)) {
        editor.objectTexturesLoaded[idx] = true;

        std::ostringstream ss;
        ss << "[Editor] Loaded object texture: " << imagePath << "\n";
        OutputDebugStringA(ss.str().c_str());
    }
    else {
        // PNG not found -- generate a placeholder texture so the object is visible
        const uint32_t kSize = 32;
        std::vector<uint8_t> pixels(kSize * kSize * 4);
        for (uint32_t i = 0; i < kSize * kSize; ++i)
        {
            pixels[i * 4 + 0] = 60;   // R
            pixels[i * 4 + 1] = 100;  // G
            pixels[i * 4 + 2] = 180;  // B
            pixels[i * 4 + 3] = 200;  // A
        }

        if (editor.objectTextures[idx].CreateFromRGBA(
                renderer.GetDevice(), renderer.GetCommandList(),
                renderer.GetUploadManager(), renderer.GetSRVHeap(),
                kSize, kSize, pixels.data()))
        {
            editor.objectTexturesLoaded[idx] = true;

            std::ostringstream ss;
            ss << "[Editor] PNG not found, using placeholder for: " << imagePath << "\n";
            OutputDebugStringA(ss.str().c_str());
        }
        else
        {
            // Mark as loaded to avoid retrying every frame
            editor.objectTexturesLoaded[idx] = true;

            std::ostringstream ss;
            ss << "[Editor] ERROR: Failed to create placeholder texture for: " << imagePath << "\n";
            OutputDebugStringA(ss.str().c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// Submit placed objects as sprites to the render queue
// ---------------------------------------------------------------------------
static void EditorSubmitObjects(EditorState& editor,
    const vamp::SceneData& scene,
    const engine::Grid& grid,
    engine::RenderQueue& renderQueue,
    engine::RendererD3D12& renderer) {
    for (const auto& obj : scene.objects) {
        int idx = static_cast<int>(obj.type);
        EnsureObjectTexture(editor, obj.type, obj.imagePath, renderer);

        if (!editor.objectTexturesLoaded[idx] ||
            !editor.objectTextures[idx].IsValid())
            continue;

        float minX = 1e30f;
        float maxX = -1e30f;
        float minY = 1e30f;
        float maxY = -1e30f;
        for (int ty = obj.y0; ty <= obj.y1; ++ty)
        {
            for (int tx = obj.x0; tx <= obj.x1; ++tx)
            {
                if (grid.IsIsometric())
                {
                    DirectX::XMFLOAT2 hex[6];
                    grid.TileHexVertices(tx, ty, hex);
                    for (int i = 0; i < 6; ++i)
                    {
                        minX = (std::min)(minX, hex[i].x);
                        maxX = (std::max)(maxX, hex[i].x);
                        minY = (std::min)(minY, hex[i].y);
                        maxY = (std::max)(maxY, hex[i].y);
                    }
                }
                else
                {
                    DirectX::XMFLOAT2 top, right, bottom, left;
                    grid.TileDiamondVertices(tx, ty, top, right, bottom, left);
                    const DirectX::XMFLOAT2 quad[4] = { top, right, bottom, left };
                    for (int i = 0; i < 4; ++i)
                    {
                        minX = (std::min)(minX, quad[i].x);
                        maxX = (std::max)(maxX, quad[i].x);
                        minY = (std::min)(minY, quad[i].y);
                        maxY = (std::max)(maxY, quad[i].y);
                    }
                }
            }
        }

        float spanX = maxX - minX;
        float spanY = maxY - minY;
        float cx = (minX + maxX) * 0.5f;

        float drawW = spanX;
        float drawH = spanY;
        const float texW = static_cast<float>(editor.objectTextures[idx].GetWidth());
        const float texH = static_cast<float>(editor.objectTextures[idx].GetHeight());
        if (texW > 0.0f && texH > 0.0f)
        {
            const float texAspect = texW / texH;
            drawH = drawW / texAspect;
        }

        float cy = (minY + maxY) * 0.5f;
        switch (obj.placement)
        {
        case vamp::SceneObjectPlacement::YMin:
            cy = minY + drawH * 0.5f;
            break;
        case vamp::SceneObjectPlacement::YMiddle:
            cy = (minY + maxY) * 0.5f;
            break;
        case vamp::SceneObjectPlacement::YMax:
        default:
            cy = maxY - drawH * 0.5f;
            break;
        }

        engine::SpriteInstance inst;
        inst.position = {cx, cy};
        inst.size = {drawW, drawH};
        inst.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
        inst.color = {1.0f, 1.0f, 1.0f, 0.85f};
        inst.rotation = 0.0f;
        inst.sortY = cy;
        inst.textureIndex = editor.objectTextures[idx].GetSRVIndex();
        inst.pad = 0;

        renderQueue.Submit(engine::RenderLayer::WallsProps, inst.sortY, 50, 0, inst);
    }
}
