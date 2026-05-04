// EditorMode.cpp - In-game tile/item editor implementation

#include "EditorMode.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <fstream>
#include <unordered_map>
#include <cstdio>
#include <cstdlib>

// Forward declarations
static void EnsureTerrainTexture(EditorState& editor,
                                 vamp::TerrainType terrain,
                                 engine::RendererD3D12& renderer);
static void EditorSubmitGroundTextures(EditorState& editor,
                                       const vamp::SceneData& scene,
                                       const engine::Grid& grid,
                                       engine::RenderQueue& renderQueue,
                                       engine::RendererD3D12& renderer);
static engine::Texture2D* EnsureObjectTexture(EditorState& editor,
                                              const char* imagePath,
                                              engine::RendererD3D12& renderer);
static void EditorSubmitObjects(EditorState& editor,
                                const vamp::SceneData& scene,
                                const engine::Grid& grid,
                                engine::RenderQueue& renderQueue,
                                engine::RendererD3D12& renderer);
static void EnsureLightMarkerTexture(EditorState& editor,
                                     engine::RendererD3D12& renderer);
static void EditorSubmitLightMarkers(EditorState& editor,
                                     const vamp::SceneData& scene,
                                     const engine::Grid& grid,
                                     engine::RenderQueue& renderQueue,
                                     engine::RendererD3D12& renderer);
static void EnsureTileFillTexture(EditorState& editor,
                                  engine::RendererD3D12& renderer);
static void EditorSubmitTileColorFills(EditorState& editor,
                                       const vamp::SceneData& scene,
                                       const engine::Grid& grid,
                                       engine::RenderQueue& renderQueue,
                                       engine::RendererD3D12& renderer);
static std::string SceneObjectDisplayName(const EditorState& editor, const vamp::SceneObject& obj);
static const char* SceneLightDisplayName(const vamp::SceneLight& light);
static const char* SceneObjectPlacementName(vamp::SceneObjectPlacement placement);
static bool PlaceSelectedObject(vamp::SceneData& scene,
                                EditorState& editor,
                                const EditorObjectAsset& asset,
                                const char* sourceLabel);
static bool PlaceSelectedLight(vamp::SceneData& scene,
                               EditorState& editor,
                               const engine::Grid& grid,
                               const char* sourceLabel);
static int FindTopmostObjectAtTile(const vamp::SceneData& scene, int tx, int ty);
static void RefreshContextMenuItems(EditorState& editor, const vamp::SceneData& scene);
static bool GetPrimarySelectedTile(const EditorState& editor, int& tx, int& ty);
static std::vector<int> GetGroundItemIndicesAtTile(const vamp::SceneData& scene, int tx, int ty);
static std::vector<int> GetLightIndicesAtTile(const vamp::SceneData& scene, int tx, int ty);
struct TilePlacedEntry
{
    enum class Kind
    {
        GroundItem,
        Object,
        Light,
    } kind = Kind::GroundItem;
    int index = -1;
};
static std::vector<TilePlacedEntry> GetPlacedEntriesAtTile(const vamp::SceneData& scene, int tx, int ty);
static void SyncFocusedTileSelection(const vamp::SceneData& scene, EditorState& editor);
static const char* GroundItemName(const vamp::SceneGroundItem& gi);
static const char* ContextMenuPageTitle(EditorContextMenuPage page);
static int GetShortcutForAction(const EditorState& editor, uint64_t actionId);
static void SetShortcutForAction(EditorState& editor, uint64_t actionId, int vk);
static std::string ShortcutKeyName(int vk);
static int ParseShortcutKeyName(const std::string& text);
static std::string ContextMenuActionLabel(const EditorState& editor, uint64_t actionId, const char* label);
static bool IsAssignableShortcutAction(uint64_t actionId);
static bool IsAssignableShortcutKey(int vk);
static int GetFirstPressedAssignableShortcutKey(const engine::InputSystem& input);
static void SaveShortcutBindings(const EditorState& editor);
static void LoadShortcutBindings(EditorState& editor);
static bool ExecuteAssignedShortcut(const engine::InputSystem& input,
                                    vamp::SceneData& scene,
                                    EditorState& editor,
                                    vamp::SceneLoader& sceneLoader,
                                    engine::OccluderSet& occluders,
                                    const engine::Grid& grid);
static void ApplyTerrainFromDropdown(uint64_t terrainId,
                                     vamp::SceneData& scene,
                                     EditorState& editor,
                                     vamp::SceneLoader& sceneLoader,
                                     engine::OccluderSet& occluders,
                                     const engine::Grid& grid);
static void DiscoverEditorAssets(EditorState& editor);
static void RebuildEditorLights(const vamp::SceneData& scene,
                                engine::LightSystem& lights,
                                const engine::Grid& grid);
static int TileDistanceToBox(int tx, int ty, int x0, int y0, int x1, int y1);

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

static const uint64_t kActionBack                 = 0xFFFFFFFFFFFFFFF0ull;
static const uint64_t kActionRootGround           = 0xFFFFFFFFFFFFFFE0ull;
static const uint64_t kActionRootPlaceItem        = 0xFFFFFFFFFFFFFFE1ull;
static const uint64_t kActionRootPlaceObject      = 0xFFFFFFFFFFFFFFE2ull;
static const uint64_t kActionRootSelectedItem     = 0xFFFFFFFFFFFFFFE3ull;
static const uint64_t kActionRootSelectedObject   = 0xFFFFFFFFFFFFFFE4ull;
static const uint64_t kActionRootSelectedLight    = 0xFFFFFFFFFFFFFFE5ull;
static const uint64_t kActionDeleteFirst          = 0xFFFFFFFFFFFFFFD0ull;
static const uint64_t kActionCopySelectedItem     = 0xFFFFFFFFFFFFFFD1ull;
static const uint64_t kActionDeleteSelectedItem   = 0xFFFFFFFFFFFFFFD2ull;
static const uint64_t kActionPlaceLight           = 0xFFFFFFFFFFFFFFD3ull;
static const uint64_t kActionCopySelectedLight    = 0xFFFFFFFFFFFFFFD4ull;
static const uint64_t kActionDeleteSelectedLight  = 0xFFFFFFFFFFFFFFD5ull;
static const uint64_t kActionLightIntensity1      = 0xFFFFFFFFFFFFFFB1ull;
static const uint64_t kActionLightIntensity2      = 0xFFFFFFFFFFFFFFB2ull;
static const uint64_t kActionLightIntensity3      = 0xFFFFFFFFFFFFFFB3ull;
static const uint64_t kActionLightIntensity4      = 0xFFFFFFFFFFFFFFB4ull;
static const uint64_t kActionLightIntensity5      = 0xFFFFFFFFFFFFFFB5ull;
static const uint64_t kActionPlacementYMin        = 0xFFFFFFFFFFFFFFC0ull;
static const uint64_t kActionPlacementYMiddle     = 0xFFFFFFFFFFFFFFC1ull;
static const uint64_t kActionPlacementYMax        = 0xFFFFFFFFFFFFFFC2ull;
static const uint64_t kActionDeleteSelectedObject = 0xFFFFFFFFFFFFFFC3ull;
static const uint64_t kActionConsumableBase       = 0xFFFFFFFFFFFFF000ull;

static uint64_t HashAssetName(const std::string& text)
{
    const uint64_t kOffset = 14695981039346656037ull;
    const uint64_t kPrime  = 1099511628211ull;

    uint64_t hash = kOffset;
    for (size_t i = 0; i < text.size(); ++i)
    {
        hash ^= static_cast<uint8_t>(text[i]);
        hash *= kPrime;
    }
    return hash;
}

static std::string ToLowerCopy(const std::string& text)
{
    std::string lowered = text;
    for (size_t i = 0; i < lowered.size(); ++i)
    {
        lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
    }
    return lowered;
}

static std::string WideToNarrow(const std::wstring& text)
{
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        wchar_t ch = text[i];
        out.push_back(ch >= 0 && ch <= 127 ? static_cast<char>(ch) : '?');
    }
    return out;
}

static std::wstring GetExecutableDirectory()
{
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    size_t slash = dir.find_last_of(L"\\/");
    if (slash != std::wstring::npos)
        dir = dir.substr(0, slash + 1);
    return dir;
}

static std::string GetFileStem(const std::string& path)
{
    size_t slash = path.find_last_of("\\/");
    size_t start = (slash == std::string::npos) ? 0 : (slash + 1);
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot < start)
        dot = path.size();
    return path.substr(start, dot - start);
}

static bool TryParseTerrainAssetName(const std::string& name, vamp::TerrainType& terrain)
{
    const std::string lowered = ToLowerCopy(name);
    if (lowered == "floor")      { terrain = vamp::TerrainType::Floor; return true; }
    if (lowered == "street")     { terrain = vamp::TerrainType::Street; return true; }
    if (lowered == "rubble")     { terrain = vamp::TerrainType::Rubble; return true; }
    if (lowered == "water")      { terrain = vamp::TerrainType::Water; return true; }
    if (lowered == "wall")       { terrain = vamp::TerrainType::Wall; return true; }
    if (lowered == "door")       { terrain = vamp::TerrainType::Door; return true; }
    if (lowered == "metrotrack") { terrain = vamp::TerrainType::MetroTrack; return true; }
    if (lowered == "shadow")     { terrain = vamp::TerrainType::Shadow; return true; }
    return false;
}

static std::string SceneObjectDisplayName(const EditorState& editor, const vamp::SceneObject& obj)
{
    if (obj.typeName[0] != '\0')
        return obj.typeName;

    std::unordered_map<uint64_t, size_t>::const_iterator it =
        editor.objectAssetIndexByTypeId.find(obj.typeId);
    if (it != editor.objectAssetIndexByTypeId.end())
        return editor.objectAssets[it->second].name;

    if (obj.imagePath[0] != '\0')
        return GetFileStem(obj.imagePath);

    std::ostringstream ss;
    ss << "object_" << obj.typeId;
    return ss.str();
}

static float LightIntensityFromLevel(uint8_t level)
{
    switch (level)
    {
    case 1: return 0.4f;
    case 2: return 0.7f;
    case 3: return 1.0f;
    case 4: return 1.3f;
    case 5: return 1.6f;
    default: return 1.0f;
    }
}

static uint8_t LightLevelFromIntensity(float intensity)
{
    struct Mapping { uint8_t level; float value; };
    static const Mapping mappings[] = {
        { 1, 0.4f }, { 2, 0.7f }, { 3, 1.0f }, { 4, 1.3f }, { 5, 1.6f }
    };

    uint8_t bestLevel = 3;
    float bestDist = 1000000.0f;
    for (size_t i = 0; i < sizeof(mappings) / sizeof(mappings[0]); ++i)
    {
        const float dist = std::fabs(intensity - mappings[i].value);
        if (dist < bestDist)
        {
            bestDist = dist;
            bestLevel = mappings[i].level;
        }
    }
    return bestLevel;
}

static uint8_t NormalizeLightLevel(const vamp::SceneLight& light)
{
    if (light.intensityLevel >= 1 && light.intensityLevel <= 5)
        return light.intensityLevel;
    return LightLevelFromIntensity(light.intensity);
}

static const char* SceneLightDisplayName(const vamp::SceneLight& light)
{
    return light.tag[0] != '\0' ? light.tag : "Light";
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

static void CollectPngFilesRecursive(const std::wstring& dir, std::vector<std::wstring>& files)
{
    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW((dir + L"\\*").c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return;

    do
    {
        const wchar_t* name = findData.cFileName;
        if (wcscmp(name, L".") == 0 || wcscmp(name, L"..") == 0)
            continue;

        const std::wstring fullPath = dir + L"\\" + name;
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            CollectPngFilesRecursive(fullPath, files);
            continue;
        }

        const wchar_t* ext = wcsrchr(name, L'.');
        if (ext != nullptr && _wcsicmp(ext, L".png") == 0)
            files.push_back(fullPath);
    }
    while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
}

static void DiscoverEditorAssets(EditorState& editor)
{
    editor.terrainAssets.clear();
    editor.objectAssets.clear();
    editor.terrainAssetIndexByActionId.clear();
    editor.objectAssetIndexByTypeId.clear();
    for (int i = 0; i < static_cast<int>(vamp::TerrainType::COUNT); ++i)
    {
        editor.terrainTexturePaths[i].clear();
        editor.terrainTexturesLoaded[i] = false;
    }

    const std::wstring exeDir = GetExecutableDirectory();
    const std::wstring assetsDir = exeDir + L"assets";
    std::vector<std::wstring> pngFiles;
    CollectPngFilesRecursive(assetsDir, pngFiles);
    std::sort(pngFiles.begin(), pngFiles.end());

    std::unordered_map<uint64_t, std::string> claimedNames;
    const std::wstring floorPrefix = assetsDir + L"\\floor\\";

    for (size_t i = 0; i < pngFiles.size(); ++i)
    {
        const std::wstring& fullPath = pngFiles[i];
        if (fullPath.size() <= exeDir.size())
            continue;

        std::wstring relativeWide = fullPath.substr(exeDir.size());
        std::replace(relativeWide.begin(), relativeWide.end(), L'/', L'\\');
        const std::string relativePath = WideToNarrow(relativeWide);
        const std::string assetName = ToLowerCopy(GetFileStem(relativePath));
        const uint64_t hash = HashAssetName(assetName);

        std::unordered_map<uint64_t, std::string>::const_iterator collision = claimedNames.find(hash);
        if (collision != claimedNames.end())
        {
            std::fprintf(stderr,
                "[Editor] WARNING: asset hash collision for '%s' and '%s'. Skipping '%s'.\n",
                collision->second.c_str(), assetName.c_str(), relativePath.c_str());
            continue;
        }

        if (fullPath.compare(0, floorPrefix.size(), floorPrefix) == 0)
        {
            vamp::TerrainType terrain = vamp::TerrainType::Floor;
            if (!TryParseTerrainAssetName(assetName, terrain))
            {
                std::fprintf(stderr,
                    "[Editor] WARNING: unknown floor asset '%s'. Skipping.\n",
                    relativePath.c_str());
                continue;
            }

            claimedNames[hash] = assetName;
            EditorTerrainAsset asset;
            asset.actionId = hash;
            asset.terrain = terrain;
            asset.name = assetName;
            asset.imagePath = relativePath;
            editor.terrainAssetIndexByActionId[asset.actionId] = editor.terrainAssets.size();
            editor.terrainAssets.push_back(asset);
            editor.terrainTexturePaths[static_cast<int>(terrain)] = relativePath;
        }
        else
        {
            claimedNames[hash] = assetName;
            EditorObjectAsset asset;
            asset.typeId = hash;
            asset.name = assetName;
            asset.imagePath = relativePath;
            editor.objectAssetIndexByTypeId[asset.typeId] = editor.objectAssets.size();
            editor.objectAssets.push_back(asset);
        }
    }
}

static bool PlaceSelectedObject(vamp::SceneData& scene,
                                EditorState& editor,
                                const EditorObjectAsset& asset,
                                const char* sourceLabel)
{
    if (editor.selectedTiles.empty())
        return false;

    int placed = 0;
    for (const auto& tc : editor.selectedTiles)
    {
        vamp::SceneObject obj;
        obj.typeId = asset.typeId;
        obj.x0   = tc.x;
        obj.y0   = tc.y;
        obj.x1   = tc.x;
        obj.y1   = tc.y;
        obj.placement = vamp::SceneObjectPlacement::YMax;
        std::memset(obj.typeName, 0, sizeof(obj.typeName));
        size_t nameLen = (std::min)(asset.name.size(), sizeof(obj.typeName) - 1);
        std::memcpy(obj.typeName, asset.name.data(), nameLen);
        std::memset(obj.imagePath, 0, sizeof(obj.imagePath));
        size_t pathLen = (std::min)(asset.imagePath.size(), sizeof(obj.imagePath) - 1);
        std::memcpy(obj.imagePath, asset.imagePath.data(), pathLen);
        std::memset(obj.tag, 0, sizeof(obj.tag));
        scene.objects.push_back(obj);
        ++placed;
    }

    editor.dirty = true;
    std::ostringstream ss;
    ss << "[Editor] Placed " << asset.name
       << " on " << placed << " tile(s)";
    if (sourceLabel && sourceLabel[0])
        ss << " " << sourceLabel;
    ss << "\n";
    OutputDebugStringA(ss.str().c_str());
    return true;
}

static bool PlaceSelectedLight(vamp::SceneData& scene,
                               EditorState& editor,
                               const engine::Grid& grid,
                               const char* sourceLabel)
{
    if (editor.selectedTiles.empty())
        return false;

    int placed = 0;
    for (const auto& tc : editor.selectedTiles)
    {
        vamp::SceneLight light;
        light.tileX = tc.x;
        light.tileY = tc.y;
        const auto center = grid.TileToWorld(tc.x, tc.y);
        light.worldX = center.x;
        light.worldY = center.y;
        light.r = 1.0f;
        light.g = 0.9f;
        light.b = 0.5f;
        light.radius = 140.0f;
        light.intensityLevel = 3;
        light.intensity = LightIntensityFromLevel(light.intensityLevel);
        light.flickerPhase = 0.0f;
        std::memset(light.tag, 0, sizeof(light.tag));
        scene.lights.push_back(light);
        ++placed;
    }

    editor.dirty = true;
    std::ostringstream ss;
    ss << "[Editor] Placed light on " << placed << " tile(s)";
    if (sourceLabel && sourceLabel[0])
        ss << " " << sourceLabel;
    ss << "\n";
    OutputDebugStringA(ss.str().c_str());
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

static std::vector<int> GetLightIndicesAtTile(const vamp::SceneData& scene, int tx, int ty)
{
    std::vector<int> indices;
    for (int i = 0; i < static_cast<int>(scene.lights.size()); ++i)
    {
        if (scene.lights[i].tileX == tx && scene.lights[i].tileY == ty)
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
            entries.push_back({ TilePlacedEntry::Kind::GroundItem, i });
    }

    for (int i = 0; i < static_cast<int>(scene.objects.size()); ++i)
    {
        const vamp::SceneObject& obj = scene.objects[i];
        if (tx >= obj.x0 && tx <= obj.x1 &&
            ty >= obj.y0 && ty <= obj.y1)
        {
            entries.push_back({ TilePlacedEntry::Kind::Object, i });
        }
    }

    for (int i = 0; i < static_cast<int>(scene.lights.size()); ++i)
    {
        if (scene.lights[i].tileX == tx && scene.lights[i].tileY == ty)
            entries.push_back({ TilePlacedEntry::Kind::Light, i });
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
    case EditorContextMenuPage::SelectedLight:  return "Selected Light";
    default:                                    return "Tile Actions";
    }
}

static int GetShortcutForAction(const EditorState& editor, uint64_t actionId)
{
    for (const auto& binding : editor.shortcutBindings)
    {
        if (binding.actionId == actionId)
            return binding.vk;
    }
    return 0;
}

static void SetShortcutForAction(EditorState& editor, uint64_t actionId, int vk)
{
    editor.shortcutBindings.erase(
        std::remove_if(editor.shortcutBindings.begin(), editor.shortcutBindings.end(),
            [actionId, vk](const EditorShortcutBinding& binding)
            {
                return binding.actionId == actionId || binding.vk == vk;
            }),
        editor.shortcutBindings.end());

    if (actionId != 0 && vk != 0)
        editor.shortcutBindings.push_back({ actionId, vk });
}

static std::string ShortcutKeyName(int vk)
{
    if (vk >= '0' && vk <= '9')
        return std::string(1, static_cast<char>(vk));
    if (vk >= 'A' && vk <= 'Z')
        return std::string(1, static_cast<char>(vk));
    if (vk >= VK_F1 && vk <= VK_F12)
    {
        std::ostringstream ss;
        ss << "F" << (vk - VK_F1 + 1);
        return ss.str();
    }

    switch (vk)
    {
    case VK_DELETE: return "Del";
    case VK_INSERT: return "Ins";
    case VK_HOME:   return "Home";
    case VK_END:    return "End";
    case VK_PRIOR:  return "PgUp";
    case VK_NEXT:   return "PgDn";
    case VK_SPACE:  return "Space";
    default:        return "?";
    }
}

static int ParseShortcutKeyName(const std::string& text)
{
    if (text.size() == 1)
    {
        char ch = text[0];
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
        if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z'))
            return static_cast<int>(ch);
    }

    if (text.size() >= 2 &&
        (text[0] == 'F' || text[0] == 'f'))
    {
        int fn = std::atoi(text.c_str() + 1);
        if (fn >= 1 && fn <= 12)
            return VK_F1 + fn - 1;
    }

    if (text == "Del" || text == "Delete")
        return VK_DELETE;
    if (text == "Ins" || text == "Insert")
        return VK_INSERT;
    if (text == "Home")
        return VK_HOME;
    if (text == "End")
        return VK_END;
    if (text == "PgUp")
        return VK_PRIOR;
    if (text == "PgDn")
        return VK_NEXT;
    if (text == "Space")
        return VK_SPACE;

    return 0;
}

static std::string ContextMenuActionLabel(const EditorState& editor, uint64_t actionId, const char* label)
{
    std::string result = label ? label : "";
    const int shortcut = GetShortcutForAction(editor, actionId);
    if (shortcut != 0)
    {
        result += " [";
        result += ShortcutKeyName(shortcut);
        result += "]";
    }
    return result;
}

static bool IsAssignableShortcutAction(uint64_t actionId)
{
    return actionId != 0 && actionId != kActionBack;
}

static bool IsAssignableShortcutKey(int vk)
{
    if (vk >= '0' && vk <= '9')
        return true;
    if (vk >= 'A' && vk <= 'Z')
        return true;
    if (vk >= VK_F1 && vk <= VK_F12)
        return true;

    switch (vk)
    {
    case VK_DELETE:
    case VK_INSERT:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
        return true;
    default:
        return false;
    }
}

static int GetFirstPressedAssignableShortcutKey(const engine::InputSystem& input)
{
    for (int vk = 0; vk < 256; ++vk)
    {
        if (IsAssignableShortcutKey(vk) && input.IsKeyPressed(vk))
            return vk;
    }
    return 0;
}

static void SaveShortcutBindings(const EditorState& editor)
{
    if (editor.shortcutConfigPath.empty())
        return;

    std::ofstream out(editor.shortcutConfigPath.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open())
        return;

    out << "[shortcuts]\n";
    for (const auto& binding : editor.shortcutBindings)
        out << binding.actionId << "=" << ShortcutKeyName(binding.vk) << "\n";
}

static void LoadShortcutBindings(EditorState& editor)
{
    editor.shortcutBindings.clear();
    if (editor.shortcutConfigPath.empty())
        return;

    std::ifstream in(editor.shortcutConfigPath.c_str());
    if (!in.is_open())
        return;

    std::string line;
    bool inShortcutsSection = false;
    while (std::getline(in, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        if (line[0] == '[')
        {
            inShortcutsSection = (line == "[shortcuts]");
            continue;
        }

        if (!inShortcutsSection)
            continue;

        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        const uint64_t actionId = static_cast<uint64_t>(std::strtoull(line.substr(0, eq).c_str(), nullptr, 10));
        const int vk = ParseShortcutKeyName(line.substr(eq + 1));
        if (actionId != 0 && vk != 0)
            SetShortcutForAction(editor, actionId, vk);
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
        editor.selectedLightIndex = -1;
        return;
    }

    std::vector<TilePlacedEntry> entries = GetPlacedEntriesAtTile(scene, tx, ty);
    if (entries.empty())
    {
        editor.selectedPlacedOrdinal = -1;
        editor.selectedGroundItemOrdinal = -1;
        editor.selectedObjectIndex = -1;
        editor.selectedLightIndex = -1;
    }
    else
    {
        if (editor.selectedPlacedOrdinal < 0)
            editor.selectedPlacedOrdinal = 0;
        editor.selectedPlacedOrdinal %= static_cast<int>(entries.size());

        const TilePlacedEntry& selected = entries[editor.selectedPlacedOrdinal];
        if (selected.kind == TilePlacedEntry::Kind::GroundItem)
        {
            int groundOrdinal = -1;
            for (int i = 0; i <= editor.selectedPlacedOrdinal; ++i)
            {
                if (entries[i].kind == TilePlacedEntry::Kind::GroundItem)
                    ++groundOrdinal;
            }
            editor.selectedGroundItemOrdinal = groundOrdinal;
            editor.selectedObjectIndex = -1;
            editor.selectedLightIndex = -1;
        }
        else if (selected.kind == TilePlacedEntry::Kind::Light)
        {
            editor.selectedGroundItemOrdinal = -1;
            editor.selectedObjectIndex = -1;
            editor.selectedLightIndex = selected.index;
        }
        else
        {
            editor.selectedGroundItemOrdinal = -1;
            editor.selectedObjectIndex = selected.index;
            editor.selectedLightIndex = -1;
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
    if (selected.kind == TilePlacedEntry::Kind::GroundItem)
    {
        const vamp::SceneGroundItem& gi = scene.groundItems[selected.index];
        ss << GroundItemName(gi);
        if (gi.quantity > 1)
            ss << " x" << gi.quantity;
    }
    else if (selected.kind == TilePlacedEntry::Kind::Light)
    {
        ss << SceneLightDisplayName(scene.lights[selected.index])
           << " (Intensity " << static_cast<int>(NormalizeLightLevel(scene.lights[selected.index])) << ")";
    }
    else
    {
        const vamp::SceneObject& obj = scene.objects[selected.index];
        ss << SceneObjectDisplayName(editor, obj)
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
    std::vector<int> lightsAtTile = hasPrimaryTile
        ? GetLightIndicesAtTile(scene, primaryX, primaryY)
        : std::vector<int>();

    switch (editor.contextMenuPage)
    {
    case EditorContextMenuPage::Root:
        itemItems.push_back({ kActionRootGround, ContextMenuActionLabel(editor, kActionRootGround, "Ground >") });
        itemItems.push_back({ kActionRootPlaceItem, ContextMenuActionLabel(editor, kActionRootPlaceItem, "Place Item >") });
        itemItems.push_back({ kActionRootPlaceObject, ContextMenuActionLabel(editor, kActionRootPlaceObject, "Place Object >") });
        if (editor.selectedGroundItemOrdinal >= 0 &&
            editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
            itemItems.push_back({ kActionRootSelectedItem, ContextMenuActionLabel(editor, kActionRootSelectedItem, "Selected Item >") });
        if (editor.selectedObjectIndex >= 0 &&
            editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
            itemItems.push_back({ kActionRootSelectedObject, ContextMenuActionLabel(editor, kActionRootSelectedObject, "Selected Object >") });
        if (editor.selectedLightIndex >= 0 &&
            editor.selectedLightIndex < static_cast<int>(scene.lights.size()))
            itemItems.push_back({ kActionRootSelectedLight, ContextMenuActionLabel(editor, kActionRootSelectedLight, "Selected Light >") });
        itemItems.push_back({ kActionDeleteFirst, ContextMenuActionLabel(editor, kActionDeleteFirst, "[Delete First Item/Object At Tile]") });
        break;

    case EditorContextMenuPage::Ground:
        itemItems.push_back({ kActionBack, "< Back" });
        for (size_t i = 0; i < editor.terrainAssets.size(); ++i)
        {
            const EditorTerrainAsset& asset = editor.terrainAssets[i];
            itemItems.push_back({ asset.actionId,
                ContextMenuActionLabel(editor, asset.actionId, asset.name.c_str()) });
        }
        break;

    case EditorContextMenuPage::PlaceItem:
        itemItems.push_back({ kActionBack, "< Back" });
        itemItems.push_back({ kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Bandage),   ContextMenuActionLabel(editor, kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Bandage), "Bandage") });
        itemItems.push_back({ kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Stimpack),  ContextMenuActionLabel(editor, kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Stimpack), "Stimpack") });
        itemItems.push_back({ kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Antidote),  ContextMenuActionLabel(editor, kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Antidote), "Antidote") });
        itemItems.push_back({ kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Flashbang), ContextMenuActionLabel(editor, kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::Flashbang), "Flashbang") });
        itemItems.push_back({ kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::BloodVial), ContextMenuActionLabel(editor, kActionConsumableBase + static_cast<uint64_t>(vamp::ConsumableType::BloodVial), "Blood Vial") });
        break;

    case EditorContextMenuPage::PlaceObject:
        itemItems.push_back({ kActionBack, "< Back" });
        itemItems.push_back({ kActionPlaceLight, ContextMenuActionLabel(editor, kActionPlaceLight, "Light") });
        for (size_t i = 0; i < editor.objectAssets.size(); ++i)
        {
            const EditorObjectAsset& asset = editor.objectAssets[i];
            itemItems.push_back({ asset.typeId,
                ContextMenuActionLabel(editor, asset.typeId, asset.name.c_str()) });
        }
        break;

    case EditorContextMenuPage::SelectedItem:
        itemItems.push_back({ kActionBack, "< Back" });
        if (editor.selectedGroundItemOrdinal >= 0 &&
            editor.selectedGroundItemOrdinal < static_cast<int>(groundItems.size()))
        {
            itemItems.push_back({ kActionCopySelectedItem, ContextMenuActionLabel(editor, kActionCopySelectedItem, "Place On Selection") });
            itemItems.push_back({ kActionDeleteSelectedItem, ContextMenuActionLabel(editor, kActionDeleteSelectedItem, "[Delete Selected Ground Item]") });
        }
        break;

    case EditorContextMenuPage::SelectedObject:
        itemItems.push_back({ kActionBack, "< Back" });
        if (editor.selectedObjectIndex >= 0 &&
            editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
        {
            itemItems.push_back({ kActionPlacementYMin, ContextMenuActionLabel(editor, kActionPlacementYMin, "Placement: Y Min") });
            itemItems.push_back({ kActionPlacementYMiddle, ContextMenuActionLabel(editor, kActionPlacementYMiddle, "Placement: Y Middle") });
            itemItems.push_back({ kActionPlacementYMax, ContextMenuActionLabel(editor, kActionPlacementYMax, "Placement: Y Max") });
            itemItems.push_back({ kActionDeleteSelectedObject, ContextMenuActionLabel(editor, kActionDeleteSelectedObject, "[Delete Selected Object]") });
        }
        break;

    case EditorContextMenuPage::SelectedLight:
        itemItems.push_back({ kActionBack, "< Back" });
    if (editor.selectedLightIndex >= 0 &&
        editor.selectedLightIndex < static_cast<int>(scene.lights.size()) &&
        !lightsAtTile.empty())
    {
            itemItems.push_back({ kActionLightIntensity1, ContextMenuActionLabel(editor, kActionLightIntensity1, "Intensity: 1") });
            itemItems.push_back({ kActionLightIntensity2, ContextMenuActionLabel(editor, kActionLightIntensity2, "Intensity: 2") });
            itemItems.push_back({ kActionLightIntensity3, ContextMenuActionLabel(editor, kActionLightIntensity3, "Intensity: 3") });
            itemItems.push_back({ kActionLightIntensity4, ContextMenuActionLabel(editor, kActionLightIntensity4, "Intensity: 4") });
            itemItems.push_back({ kActionLightIntensity5, ContextMenuActionLabel(editor, kActionLightIntensity5, "Intensity: 5") });
            itemItems.push_back({ kActionCopySelectedLight, ContextMenuActionLabel(editor, kActionCopySelectedLight, "Place On Selection") });
            itemItems.push_back({ kActionDeleteSelectedLight, ContextMenuActionLabel(editor, kActionDeleteSelectedLight, "[Delete Selected Light]") });
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

static void RebuildEditorLights(const vamp::SceneData& scene,
                                engine::LightSystem& lights,
                                const engine::Grid& grid)
{
    lights.Clear();
    for (const auto& light : scene.lights)
    {
        float lightX = light.worldX;
        float lightY = light.worldY;
        if (scene.InBounds(light.tileX, light.tileY))
        {
            const auto center = grid.TileToWorld(light.tileX, light.tileY);
            lightX = center.x;
            lightY = center.y;
        }

        const float intensity = LightIntensityFromLevel(NormalizeLightLevel(light));
        lights.AddLight(lightX, lightY, light.r, light.g, light.b,
            light.radius, intensity, light.flickerPhase);
    }
}

static int TileDistanceToBox(int tx, int ty, int x0, int y0, int x1, int y1)
{
    int dx = 0;
    if (tx < x0) dx = x0 - tx;
    else if (tx > x1) dx = tx - x1;

    int dy = 0;
    if (ty < y0) dy = y0 - ty;
    else if (ty > y1) dy = ty - y1;

    return (std::max)(dx, dy);
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
                             EditorState& editor,
                             const engine::Grid& grid)
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

            if (editor.selectedLightIndex >= 0 &&
                editor.selectedLightIndex < static_cast<int>(scene.lights.size()))
            {
                const vamp::SceneLight source = scene.lights[editor.selectedLightIndex];
                int placed = 0;
                for (const auto& tc : editor.selectedTiles)
                {
                    vamp::SceneLight light = source;
                    light.tileX = tc.x;
                    light.tileY = tc.y;
                    const auto center = grid.TileToWorld(tc.x, tc.y);
                    light.worldX = center.x;
                    light.worldY = center.y;
                    scene.lights.push_back(light);
                    ++placed;
                }

                if (placed > 0)
                {
                    editor.dirty = true;
                    std::ostringstream ss;
                    ss << "[Editor] Copied selected light to " << placed << " tile(s)\n";
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
        else if (editor.selectedLightIndex >= 0)
        {
            scene.lights.erase(scene.lights.begin() + editor.selectedLightIndex);
            editor.selectedLightIndex = -1;
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

    // Object placement: 1/2 place the first discovered object assets.
    if (input.IsKeyPressed('1') && !editor.selectedTiles.empty() &&
        !editor.objectAssets.empty())
    {
        PlaceSelectedObject(scene, editor, editor.objectAssets[0], "");
        return true;
    }
    if (input.IsKeyPressed('2') && !editor.selectedTiles.empty() &&
        editor.objectAssets.size() > 1)
    {
        PlaceSelectedObject(scene, editor, editor.objectAssets[1], "");
        return true;
    }
    if (input.IsKeyPressed('3') && !editor.selectedTiles.empty())
    {
        PlaceSelectedLight(scene, editor, grid, "");
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
                ss << " | Object: " << SceneObjectDisplayName(editor, obj)
                   << " (" << SceneObjectPlacementName(obj.placement) << ")";
            }

            std::vector<int> tileLights = GetLightIndicesAtTile(*scene, tx, ty);
            if (!tileLights.empty())
            {
                if (editor.selectedLightIndex >= 0 &&
                    editor.selectedLightIndex < static_cast<int>(scene->lights.size()) &&
                    scene->lights[editor.selectedLightIndex].tileX == tx &&
                    scene->lights[editor.selectedLightIndex].tileY == ty)
                {
                    ss << " | Light: " << SceneLightDisplayName(scene->lights[editor.selectedLightIndex]);
                }
                else
                {
                    ss << " | Lights: " << tileLights.size();
                }
            }
        }
        if (editor.uiTileInfoLabel)
            editor.uiTileInfoLabel->SetText(ss.str());
    }

    // Line 3: Common shortcuts
    {
        std::ostringstream ss;
        ss << "LMB=Select Tile  Shift+LMB=Add  RMB=Context Menu  Wheel=Cycle Tile Items  "
           << "P=Copy Selected Item  1/2=First Objects  3=Light  B/S/A/F/V=Consumables  "
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
    DiscoverEditorAssets(editor);
    LoadShortcutBindings(editor);

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
        for (size_t i = 0; i < editor.terrainAssets.size(); ++i)
        {
            const EditorTerrainAsset& asset = editor.terrainAssets[i];
            terrainItems.push_back({ asset.actionId, asset.name });
        }

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
static void ApplyTerrainFromDropdown(uint64_t terrainId,
                                      vamp::SceneData& scene,
                                      EditorState& editor,
                                      vamp::SceneLoader& sceneLoader,
                                      engine::OccluderSet& occluders,
                                      const engine::Grid& grid)
{
    std::unordered_map<uint64_t, size_t>::const_iterator it =
        editor.terrainAssetIndexByActionId.find(terrainId);
    if (it == editor.terrainAssetIndexByActionId.end())
        return;

    const EditorTerrainAsset& asset = editor.terrainAssets[it->second];
    vamp::TerrainType type = asset.terrain;
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
           << asset.name << " (dropdown)\n";
        OutputDebugStringA(ss.str().c_str());
    }
    SyncFocusedTileSelection(scene, editor);
}

// ---------------------------------------------------------------------------
// Apply item placement from dropdown selection
// ---------------------------------------------------------------------------
static void ApplyItemFromDropdown(uint64_t itemId,
                                   vamp::SceneData& scene,
                                   EditorState& editor,
                                   vamp::SceneLoader& sceneLoader,
                                   engine::OccluderSet& occluders,
                                    const engine::Grid& grid)
{
    SyncFocusedTileSelection(scene, editor);

    if (itemId == kActionBack)
    {
        editor.contextMenuPage = EditorContextMenuPage::Root;
        return;
    }

    if (itemId == kActionRootGround) { editor.contextMenuPage = EditorContextMenuPage::Ground; return; }
    if (itemId == kActionRootPlaceItem) { editor.contextMenuPage = EditorContextMenuPage::PlaceItem; return; }
    if (itemId == kActionRootPlaceObject) { editor.contextMenuPage = EditorContextMenuPage::PlaceObject; return; }
    if (itemId == kActionRootSelectedItem) { editor.contextMenuPage = EditorContextMenuPage::SelectedItem; return; }
    if (itemId == kActionRootSelectedObject) { editor.contextMenuPage = EditorContextMenuPage::SelectedObject; return; }
    if (itemId == kActionRootSelectedLight) { editor.contextMenuPage = EditorContextMenuPage::SelectedLight; return; }

    if (editor.terrainAssetIndexByActionId.find(itemId) != editor.terrainAssetIndexByActionId.end())
    {
        ApplyTerrainFromDropdown(itemId, scene, editor, sceneLoader, occluders, grid);
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::Ground;
        return;
    }

    if (itemId == kActionCopySelectedItem)
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

    if (itemId == kActionDeleteSelectedItem)
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

    if (itemId == kActionCopySelectedLight)
    {
        if (editor.selectedLightIndex >= 0 &&
            editor.selectedLightIndex < static_cast<int>(scene.lights.size()))
        {
            const vamp::SceneLight source = scene.lights[editor.selectedLightIndex];
            int placed = 0;
            for (const auto& tc : editor.selectedTiles)
            {
                vamp::SceneLight light = source;
                light.tileX = tc.x;
                light.tileY = tc.y;
                const auto center = grid.TileToWorld(tc.x, tc.y);
                light.worldX = center.x;
                light.worldY = center.y;
                scene.lights.push_back(light);
                ++placed;
            }

            if (placed > 0)
            {
                editor.dirty = true;
                std::ostringstream ss;
                ss << "[Editor] Copied selected light to " << placed << " tile(s) (context)\n";
                OutputDebugStringA(ss.str().c_str());
            }
        }
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::SelectedLight;
        return;
    }

    if ((itemId >= kActionLightIntensity1 && itemId <= kActionLightIntensity5) &&
        editor.selectedLightIndex >= 0 &&
        editor.selectedLightIndex < static_cast<int>(scene.lights.size()))
    {
        uint8_t level = 3;
        switch (itemId)
        {
        case kActionLightIntensity1: level = 1; break;
        case kActionLightIntensity2: level = 2; break;
        case kActionLightIntensity3: level = 3; break;
        case kActionLightIntensity4: level = 4; break;
        case kActionLightIntensity5: level = 5; break;
        default: break;
        }

        vamp::SceneLight& light = scene.lights[editor.selectedLightIndex];
        light.intensityLevel = level;
        light.intensity = LightIntensityFromLevel(level);
        editor.dirty = true;

        std::ostringstream ss;
        ss << "[Editor] Light intensity -> " << static_cast<int>(level) << " (context)\n";
        OutputDebugStringA(ss.str().c_str());
        editor.contextMenuPage = EditorContextMenuPage::SelectedLight;
        return;
    }

    if (itemId == kActionDeleteSelectedLight)
    {
        if (editor.selectedLightIndex >= 0 &&
            editor.selectedLightIndex < static_cast<int>(scene.lights.size()))
        {
            scene.lights.erase(scene.lights.begin() + editor.selectedLightIndex);
            editor.selectedLightIndex = -1;
            editor.dirty = true;
            OutputDebugStringA("[Editor] Deleted selected light (context)\n");
        }
        SyncFocusedTileSelection(scene, editor);
        editor.contextMenuPage = EditorContextMenuPage::SelectedLight;
        return;
    }

    if ((itemId == kActionPlacementYMin ||
         itemId == kActionPlacementYMiddle ||
         itemId == kActionPlacementYMax) &&
        editor.selectedObjectIndex >= 0 &&
        editor.selectedObjectIndex < static_cast<int>(scene.objects.size()))
    {
        vamp::SceneObjectPlacement placement = vamp::SceneObjectPlacement::YMax;
        if (itemId == kActionPlacementYMin)
            placement = vamp::SceneObjectPlacement::YMin;
        else if (itemId == kActionPlacementYMiddle)
            placement = vamp::SceneObjectPlacement::YMiddle;
        scene.objects[editor.selectedObjectIndex].placement = placement;
        editor.dirty = true;

        std::ostringstream ss;
        ss << "[Editor] " << SceneObjectDisplayName(editor, scene.objects[editor.selectedObjectIndex])
           << " placement -> " << SceneObjectPlacementName(placement) << " (dropdown)\n";
        OutputDebugStringA(ss.str().c_str());
        editor.contextMenuPage = EditorContextMenuPage::SelectedObject;
        return;
    }

    if (itemId == kActionDeleteSelectedObject &&
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
    if (itemId == kActionDeleteFirst)
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

            auto& tileLights = scene.lights;
            auto lit = std::find_if(tileLights.begin(), tileLights.end(),
                [&tc](const vamp::SceneLight& light) {
                    return light.tileX == tc.x && light.tileY == tc.y;
                });
            if (lit != tileLights.end())
            {
                tileLights.erase(lit);
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

    if (!editor.selectedTiles.empty())
    {
        if (itemId == kActionPlaceLight)
        {
            PlaceSelectedLight(scene, editor, grid, "(dropdown)");
            editor.contextMenuPage = EditorContextMenuPage::Root;
            return;
        }

        std::unordered_map<uint64_t, size_t>::const_iterator objectIt =
            editor.objectAssetIndexByTypeId.find(itemId);
        if (objectIt != editor.objectAssetIndexByTypeId.end())
        {
            PlaceSelectedObject(scene, editor, editor.objectAssets[objectIt->second], "(dropdown)");
            editor.contextMenuPage = EditorContextMenuPage::Root;
            return;
        }
    }

    // Consumable placement
    if (itemId >= kActionConsumableBase &&
        itemId < kActionConsumableBase + 1000)
    {
        vamp::ConsumableType ctype = static_cast<vamp::ConsumableType>(itemId - kActionConsumableBase);
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

static bool ExecuteAssignedShortcut(const engine::InputSystem& input,
                                    vamp::SceneData& scene,
                                    EditorState& editor,
                                    vamp::SceneLoader& sceneLoader,
                                    engine::OccluderSet& occluders,
                                    const engine::Grid& grid)
{
    if (editor.selectedTiles.empty())
        return false;

    for (const auto& binding : editor.shortcutBindings)
    {
        if (!input.IsKeyPressed(binding.vk))
            continue;

        ApplyItemFromDropdown(binding.actionId, scene, editor, sceneLoader, occluders, grid);
        RefreshContextMenuItems(editor, scene);
        return true;
    }

    return false;
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

    bool shortcutAssignedThisFrame = false;

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

        if (editor.itemDropdown && editor.itemDropdown->IsOpen())
        {
            const uint64_t hoveredActionId = editor.itemDropdown->GetHoveredItemId();
            const int pressedShortcut = GetFirstPressedAssignableShortcutKey(input);
            if (hoveredActionId != 0 &&
                pressedShortcut != 0 &&
                IsAssignableShortcutAction(hoveredActionId))
            {
                SetShortcutForAction(editor, hoveredActionId, pressedShortcut);
                SaveShortcutBindings(editor);
                RefreshContextMenuItems(editor, sceneLoader.GetSceneData());
                shortcutAssignedThisFrame = true;

                std::ostringstream ss;
                ss << "[Editor] Shortcut "
                   << ShortcutKeyName(pressedShortcut)
                   << " -> action " << hoveredActionId << "\n";
                OutputDebugStringA(ss.str().c_str());
            }
        }

        // Forward character input to open dropdowns
        if (anyDropdownOpen && !shortcutAssignedThisFrame && input.HasChar())
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
            static uint64_t s_pendingItemId = 0;

            // Set up one-shot callbacks
            if (editor.itemDropdown && editor.itemDropdown->IsVisible())
            {
                editor.itemDropdown->SetOnSelect(
                    [](uint64_t id, const std::string&) { s_pendingItemId = id; });
                if (editor.itemDropdown->HandleClick(mx, my))
                {
                    if (s_pendingItemId != 0 && sceneLoader.IsLoaded())
                    {
                        const bool reopenForNavigation =
                            (s_pendingItemId == kActionBack) ||
                            (s_pendingItemId == kActionRootGround) ||
                            (s_pendingItemId == kActionRootPlaceItem) ||
                            (s_pendingItemId == kActionRootPlaceObject) ||
                            (s_pendingItemId == kActionRootSelectedItem) ||
                            (s_pendingItemId == kActionRootSelectedObject) ||
                            (s_pendingItemId == kActionRootSelectedLight);
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
                    s_pendingItemId = 0;
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
        if (!shortcutAssignedThisFrame &&
            !ExecuteAssignedShortcut(input, sceneLoader.GetSceneData(), editor,
            sceneLoader, occluders, grid))
        {
            HandleTerrainHotkey(input, sceneLoader.GetSceneData(), editor,
                sceneLoader, occluders, grid);
            HandleItemHotkey(input, sceneLoader.GetSceneData(), editor, grid);
        }
    }

    // --- Update title bar ---
    EditorUpdateTitle(hWnd, editor);

    // --- Update overlay labels ---
    const vamp::SceneData* scenePtr = sceneLoader.IsLoaded() ? &sceneLoader.GetSceneData() : nullptr;
    EditorUpdateOverlay(editor, input, grid, scenePtr);

    // --- Render ---
    if (sceneLoader.IsLoaded())
        RebuildEditorLights(sceneLoader.GetSceneData(), lights, grid);

    lights.Update(time);
    renderer.BeginFrame();

    renderQueue.Clear();

    // Submit textured ground tiles and placed objects as sprites
    if (sceneLoader.IsLoaded())
    {
        //EditorSubmitTileColorFills(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);
        EditorSubmitGroundTextures(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);
        EditorSubmitObjects(editor, sceneLoader.GetSceneData(), grid,
            renderQueue, renderer);
        EditorSubmitLightMarkers(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);
    }

    // Fog: fully visible in editor (no gameplay fog)
    fog.ClearVisible();
    fog.SetAllVisible();
    fog.UpdateExplored();

    bgPager.Update(camera, renderer.GetFrameIndex());
    const DirectX::XMFLOAT4 editorGridColor = { 1.0f, 1.0f, 1.0f, 0.15f };

    sceneRenderer.RenderFrame(renderer, camera, bgPager,
        renderQueue, lights, occluders,
        fog, roofs, time, &grid, &editorGridColor);

    // Grid is drawn as an underlay during RenderFrame. Keep only selection as a
    // post-frame overlay so placed objects can appear above wall terrain.
    renderer.TransitionBackBufferToRT();

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

    if (editor.terrainTexturePaths[idx].empty())
    {
        std::ostringstream ss;
        ss << "[Editor] WARNING: no PNG path for TerrainType " << idx
           << " (" << EditorTerrainName(terrain) << "). Tile will fall back to color fill only.\n";
        OutputDebugStringA(ss.str().c_str());
        editor.terrainTexturesLoaded[idx] = true;
        return;
    }

    const std::wstring dir = GetExecutableDirectory();
    const std::string& imagePath = editor.terrainTexturePaths[idx];
    std::wstring widePath(imagePath.begin(), imagePath.end());
    std::wstring fullPath = dir + widePath;

    if (editor.terrainTextures[idx].LoadFromPNG(
        renderer.GetDevice(), renderer.GetCommandList(),
        renderer.GetUploadManager(), renderer.GetSRVHeap(), fullPath))
    {
        editor.terrainTexturesLoaded[idx] = true;
        std::ostringstream ss;
        ss << "[Editor] Loaded terrain texture: " << imagePath
           << " (" << EditorTerrainName(terrain) << ", "
           << editor.terrainTextures[idx].GetWidth() << "x"
           << editor.terrainTextures[idx].GetHeight() << ")\n";
        OutputDebugStringA(ss.str().c_str());
    }
    else
    {
        editor.terrainTexturesLoaded[idx] = true;
        std::ostringstream ss;
        ss << "[Editor] ERROR: failed to load terrain PNG: " << imagePath
           << " (" << EditorTerrainName(terrain)
           << "). Tile will fall back to color fill only.\n";
        OutputDebugStringA(ss.str().c_str());
    }
}

// ---------------------------------------------------------------------------
// Submit textured ground tiles as sprites (one textured PNG per tile,
// keyed by TerrainType).
// ---------------------------------------------------------------------------
static void EditorSubmitGroundTextures(EditorState& editor,
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
            float drawW = static_cast<float>(editor.terrainTextures[texIdx].GetWidth());
            float drawH = static_cast<float>(editor.terrainTextures[texIdx].GetHeight());

            if (drawW <= 0.0f || drawH <= 0.0f)
                continue;

            DirectX::XMFLOAT2 top, right, bottom, left;
            grid.TileDiamondVertices(x, y, top, right, bottom, left);
            const float maxW = right.x - left.x;
            const float maxH = bottom.y - top.y;
            float scale = 1.0f;
            if (drawW > maxW || drawH > maxH)
            {
                scale = (std::min)(maxW / drawW, maxH / drawH);
                drawW *= scale;
                drawH *= scale;
            }

            const float tileBottomY = bottom.y;
            const uint16_t tileOrder = static_cast<uint16_t>(((y & 0xFF) << 8) | (x & 0xFF));

            // Hex grids are 3-colorable: no two adjacent tiles share the
            // same color.  For flat-top, odd-shift-down offset coordinates
            // the following gives a valid 3-coloring:
            //   color = (x + 2*y + (x&1)) mod 3
            // This guarantees every pair of hex neighbors gets a different
            // depth, eliminating z-fighting flicker on overlapping sprites.
            const int hexColor = ((x % 3) + 2 * (y % 3) + (x & 1)) % 3;

            engine::SpriteInstance inst;
            inst.position = center;
            inst.size = { drawW, drawH };
            inst.uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
            inst.color = { 1.0f, 1.0f, 1.0f, 1.0f };
            inst.rotation = 0.0f;
            inst.sortY = tileBottomY;
            inst.textureIndex = editor.terrainTextures[texIdx].GetSRVIndex();
            inst.depthZ = 0.80f + 0.02f * static_cast<float>(hexColor);

            renderQueue.Submit(engine::RenderLayer::GroundTextures, inst.sortY, 0, tileOrder, inst);
        }
    }
}

// ---------------------------------------------------------------------------
// Load object texture on demand (called during render with active cmd list)
// ---------------------------------------------------------------------------
static engine::Texture2D* EnsureObjectTexture(EditorState& editor,
    const char* imagePath,
    engine::RendererD3D12& renderer) {
    if (imagePath == nullptr || imagePath[0] == '\0')
        return nullptr;

    EditorObjectTextureCacheEntry* cache = nullptr;
    for (size_t i = 0; i < editor.objectTextureCache.size(); ++i)
    {
        EditorObjectTextureCacheEntry& entry = editor.objectTextureCache[i];
        if (entry.imagePath == imagePath)
        {
            cache = &entry;
            if (entry.loaded)
                return entry.texture.IsValid() ? &entry.texture : nullptr;
            break;
        }
    }

    if (cache == nullptr)
    {
        editor.objectTextureCache.push_back(EditorObjectTextureCacheEntry());
        cache = &editor.objectTextureCache.back();
        cache->imagePath = imagePath;
    }

    // Resolve path relative to executable directory
    const std::wstring dir = GetExecutableDirectory();

    std::wstring widePath(imagePath, imagePath + std::strlen(imagePath));
    std::wstring fullPath = dir + widePath;

    if (cache->texture.LoadFromPNG(
        renderer.GetDevice(), renderer.GetCommandList(),
        renderer.GetUploadManager(), renderer.GetSRVHeap(), fullPath)) {
        cache->loaded = true;

        std::ostringstream ss;
        ss << "[Editor] Loaded object texture: " << imagePath << "\n";
        OutputDebugStringA(ss.str().c_str());
        return &cache->texture;
    }

    // PNG not found -- generate a placeholder texture so the object is visible
    const uint32_t kSize = 32;
    std::vector<uint8_t> pixels(kSize * kSize * 4);
    for (uint32_t i = 0; i < kSize * kSize; ++i)
    {
        pixels[i * 4 + 0] = 60;
        pixels[i * 4 + 1] = 100;
        pixels[i * 4 + 2] = 180;
        pixels[i * 4 + 3] = 255;
    }

    if (cache->texture.CreateFromRGBA(
            renderer.GetDevice(), renderer.GetCommandList(),
            renderer.GetUploadManager(), renderer.GetSRVHeap(),
            kSize, kSize, pixels.data()))
    {
        cache->loaded = true;

        std::ostringstream ss;
        ss << "[Editor] PNG not found, using placeholder for: " << imagePath << "\n";
        OutputDebugStringA(ss.str().c_str());
        return &cache->texture;
    }

    cache->loaded = true;

    std::ostringstream ss;
    ss << "[Editor] ERROR: Failed to create placeholder texture for: " << imagePath << "\n";
    OutputDebugStringA(ss.str().c_str());
    return nullptr;
}

// ---------------------------------------------------------------------------
// Submit placed objects as sprites to the render queue
// ---------------------------------------------------------------------------
static void EditorSubmitObjects(EditorState& editor,
    const vamp::SceneData& scene,
    const engine::Grid& grid,
    engine::RenderQueue& renderQueue,
    engine::RendererD3D12& renderer) {
    for (size_t objIndex = 0; objIndex < scene.objects.size(); ++objIndex) {
        const auto& obj = scene.objects[objIndex];
        engine::Texture2D* objectTexture = EnsureObjectTexture(editor, obj.imagePath, renderer);
        if (objectTexture == nullptr || !objectTexture->IsValid())
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
        const float texW = static_cast<float>(objectTexture->GetWidth());
        const float texH = static_cast<float>(objectTexture->GetHeight());
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
        inst.color = {1.0f, 1.0f, 1.0f, 1.0f};
        inst.rotation = 0.0f;
        inst.sortY = cy;
        inst.textureIndex = objectTexture->GetSRVIndex();
        inst.depthZ = 0.55f;

        // Placed objects use the cutout sprite path so wall/prop/fixture
        // pixels render solid; depth keeps them in front of terrain.
        renderQueue.Submit(engine::RenderLayer::PlacedObjects,
                           inst.sortY, 50,
                           static_cast<uint16_t>(objIndex & 0xFFFF), inst);
    }
}

static void EnsureLightMarkerTexture(EditorState& editor,
    engine::RendererD3D12& renderer)
{
    if (editor.lightMarkerTextureLoaded)
        return;

    const uint32_t kSize = 16;
    std::vector<uint8_t> pixels(kSize * kSize * 4, 0);
    const float center = 7.5f;
    for (uint32_t y = 0; y < kSize; ++y)
    {
        for (uint32_t x = 0; x < kSize; ++x)
        {
            const float dx = static_cast<float>(x) - center;
            const float dy = static_cast<float>(y) - center;
            const float dist2 = dx * dx + dy * dy;
            const size_t idx = (y * kSize + x) * 4;

            if (dist2 <= 49.0f)
            {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 220;
                pixels[idx + 2] = 96;
                pixels[idx + 3] = 220;
            }
            if (dist2 <= 16.0f)
            {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 250;
                pixels[idx + 2] = 180;
                pixels[idx + 3] = 255;
            }
        }
    }

    if (editor.lightMarkerTexture.CreateFromRGBA(
            renderer.GetDevice(), renderer.GetCommandList(),
            renderer.GetUploadManager(), renderer.GetSRVHeap(),
            kSize, kSize, pixels.data()))
    {
        editor.lightMarkerTextureLoaded = true;
    }
    else
    {
        editor.lightMarkerTextureLoaded = true;
        OutputDebugStringA("[Editor] ERROR: failed to create light marker texture (16x16 procedural). Light markers will not appear.\n");
    }
}

static void EditorSubmitLightMarkers(EditorState& editor,
    const vamp::SceneData& scene,
    const engine::Grid& grid,
    engine::RenderQueue& renderQueue,
    engine::RendererD3D12& renderer)
{
    EnsureLightMarkerTexture(editor, renderer);
    if (!editor.lightMarkerTextureLoaded || !editor.lightMarkerTexture.IsValid())
        return;

    for (size_t lightIndex = 0; lightIndex < scene.lights.size(); ++lightIndex)
    {
        const vamp::SceneLight& light = scene.lights[lightIndex];
        if (!scene.InBounds(light.tileX, light.tileY))
            continue;

        const auto center = grid.TileToWorld(light.tileX, light.tileY);
        engine::SpriteInstance inst;
        inst.position = { center.x, center.y - 6.0f };
        inst.size = { 12.0f, 12.0f };
        inst.uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
        inst.color = { 1.0f, 1.0f, 1.0f, 0.95f };
        inst.rotation = 0.0f;
        inst.sortY = center.y - 6.0f;
        inst.textureIndex = editor.lightMarkerTexture.GetSRVIndex();
        inst.depthZ = 0.5f;

        renderQueue.Submit(engine::RenderLayer::Actors, inst.sortY, 75,
            static_cast<uint16_t>(lightIndex & 0xFFFF), inst);
    }
}

// ---------------------------------------------------------------------------
// Tile color-fill underlay: 1 hex sprite per tile, tinted by terrain type.
// Drawn on the TileColorFill layer so it sits below grid lines, ground tile
// textures, walls and actors -- this gives the editor a visible "tile-type
// heatmap" without obscuring placed content.
// ---------------------------------------------------------------------------
static void EnsureTileFillTexture(EditorState& editor,
    engine::RendererD3D12& renderer)
{
    if (editor.tileFillTextureLoaded)
        return;

    // Single white pixel; SpritePS multiplies texColor * inst.color, so this
    // produces a flat color quad tinted by the per-instance color.
    const uint8_t pixel[4] = { 255, 255, 255, 255 };
    if (editor.tileFillTexture.CreateFromRGBA(
            renderer.GetDevice(), renderer.GetCommandList(),
            renderer.GetUploadManager(), renderer.GetSRVHeap(),
            1, 1, pixel))
    {
        editor.tileFillTextureLoaded = true;
    }
    else
    {
        editor.tileFillTextureLoaded = true;
        OutputDebugStringA("[Editor] ERROR: failed to create tile fill texture (1x1 white). Tile color fills will not appear.\n");
    }
}

static DirectX::XMFLOAT4 TerrainFillColor(vamp::TerrainType t)
{
    // Colors below are sampled from the actual terrain PNGs in assets/floor/
    // (average of opaque pixels). Keeping the fill close to the texture color
    // means the alpha-soft hex edges of the texture blend cleanly with the
    // underlay instead of producing a contrasting halo. Alpha=1 because the
    // cutout PSO writes opaquely.
    switch (t)
    {
    case vamp::TerrainType::Floor:      return { 0.23f, 0.23f, 0.23f, 1.0f }; // dark gray  (floor.png)
    case vamp::TerrainType::Street:     return { 0.24f, 0.23f, 0.01f, 1.0f }; // dark olive (street.png)
    case vamp::TerrainType::Rubble:     return { 0.56f, 0.36f, 0.01f, 1.0f }; // amber      (rubble.png)
    case vamp::TerrainType::Water:      return { 0.04f, 0.40f, 0.68f, 1.0f }; // blue       (water.png)
    case vamp::TerrainType::Wall:       return { 0.60f, 0.00f, 0.60f, 1.0f }; // magenta    (wall.png)
    case vamp::TerrainType::Door:       return { 0.41f, 0.31f, 0.00f, 1.0f }; // dark olive (door.png)
    case vamp::TerrainType::MetroTrack: return { 0.30f, 0.20f, 0.11f, 1.0f }; // brown      (metrotrack.png)
    case vamp::TerrainType::Shadow:     return { 0.00f, 0.00f, 0.00f, 1.0f }; // black      (shadow.png)
    default:                            return { 0.30f, 0.30f, 0.30f, 1.0f };
    }
}

static void EditorSubmitTileColorFills(EditorState& editor,
    const vamp::SceneData& scene,
    const engine::Grid& grid,
    engine::RenderQueue& renderQueue,
    engine::RendererD3D12& renderer)
{
    EnsureTileFillTexture(editor, renderer);
    if (!editor.tileFillTextureLoaded || !editor.tileFillTexture.IsValid())
        return;

    const int gridW = static_cast<int>(scene.header.gridWidth);
    const int gridH = static_cast<int>(scene.header.gridHeight);

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            const vamp::MapTile& tile = scene.tiles[y * gridW + x];

            // Size the quad to fully cover the hex / diamond bounding box.
            DirectX::XMFLOAT2 top, right, bottom, left;
            grid.TileDiamondVertices(x, y, top, right, bottom, left);
            const float drawW = right.x - left.x;
            const float drawH = bottom.y - top.y;
            if (drawW <= 0.0f || drawH <= 0.0f)
                continue;

            const auto center = grid.TileToWorld(x, y);
            const float tileBottomY = bottom.y;
            const uint16_t tileOrder = static_cast<uint16_t>(((y & 0xFF) << 8) | (x & 0xFF));

            engine::SpriteInstance inst;
            inst.position     = center;
            inst.size         = { drawW, drawH };
            inst.uvRect       = { 0.0f, 0.0f, 1.0f, 1.0f };
            inst.color        = TerrainFillColor(tile.terrain);
            inst.rotation     = 0.0f;
            inst.sortY        = tileBottomY;
            inst.textureIndex = editor.tileFillTexture.GetSRVIndex();
            inst.depthZ       = 0.92f; // matches DepthForLayer(TileColorFill)

            renderQueue.Submit(engine::RenderLayer::TileColorFill,
                               inst.sortY, 0, tileOrder, inst);
        }
    }
}
