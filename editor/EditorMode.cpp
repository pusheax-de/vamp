// EditorMode.cpp - In-game tile/item editor implementation

#include "EditorMode.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>

// Forward declarations
static void EnsureObjectTexture(EditorState& editor,
                                vamp::SceneObjectType type,
                                const char* imagePath,
                                engine::RendererD3D12& renderer);
static void EditorSubmitObjects(EditorState& editor,
                                const vamp::SceneData& scene,
                                const engine::Grid& grid,
                                engine::RenderQueue& renderQueue,
                                engine::RendererD3D12& renderer);

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
    // Delete: remove first ground item at each selected tile
    if (input.IsKeyPressed(VK_DELETE))
    {
        int deleted = 0;
        for (const auto& tc : editor.selectedTiles)
        {
            // Delete ground items
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
        }

        // Delete objects whose bounding box overlaps any selected tile
        {
            auto& objs = scene.objects;
            for (const auto& tc : editor.selectedTiles)
            {
                auto it = std::find_if(objs.begin(), objs.end(),
                    [&tc](const vamp::SceneObject& obj) {
                        return tc.x >= obj.x0 && tc.x <= obj.x1 &&
                               tc.y >= obj.y0 && tc.y <= obj.y1;
                    });
                if (it != objs.end())
                {
                    objs.erase(it);
                    ++deleted;
                }
            }
        }

        if (deleted > 0)
        {
            editor.dirty = true;
            std::ostringstream ss;
            ss << "[Editor] Deleted " << deleted << " item(s)/object(s)\n";
            OutputDebugStringA(ss.str().c_str());
        }
        else
        {
            OutputDebugStringA("[Editor] No items/objects to delete at selection.\n");
        }
        editor.ClearSelection();
        return true;
    }

    // Object placement: 1 = Hangar
    if (input.IsKeyPressed('1') && !editor.selectedTiles.empty())
    {
        // Compute bounding box of all selected tiles
        int minX = editor.selectedTiles[0].x, maxX = minX;
        int minY = editor.selectedTiles[0].y, maxY = minY;
        for (const auto& tc : editor.selectedTiles)
        {
            if (tc.x < minX) minX = tc.x;
            if (tc.x > maxX) maxX = tc.x;
            if (tc.y < minY) minY = tc.y;
            if (tc.y > maxY) maxY = tc.y;
        }

        vamp::SceneObject obj;
        obj.type = vamp::SceneObjectType::Hangar;
        obj.x0   = minX;
        obj.y0   = minY;
        obj.x1   = maxX;
        obj.y1   = maxY;
        {
            std::memset(obj.imagePath, 0, sizeof(obj.imagePath));
            const char* path = "assets\\hangar\\hangar.png";
            size_t len = std::strlen(path);
            if (len >= sizeof(obj.imagePath)) len = sizeof(obj.imagePath) - 1;
            std::memcpy(obj.imagePath, path, len);
        }
        std::memset(obj.tag, 0, sizeof(obj.tag));
        scene.objects.push_back(obj);

        editor.dirty = true;
        std::ostringstream ss;
        ss << "[Editor] Placed Hangar at [" << minX << "," << minY
           << "]-[" << maxX << "," << maxY << "]\n";
        OutputDebugStringA(ss.str().c_str());

        editor.ClearSelection();
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
// Draw selection highlight (diamond outline for each selected tile)
// ---------------------------------------------------------------------------
static void EditorDrawSelection(engine::SceneRenderer& sceneRenderer,
                                engine::RendererD3D12& renderer,
                                engine::Grid& grid,
                                const EditorState& editor)
{
    if (editor.selection == EditorSelection::None || editor.selectedTiles.empty())
        return;

    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(editor.selectedTiles.size() * 8);

    for (const auto& tc : editor.selectedTiles)
    {
        if (!grid.InBounds(tc.x, tc.y))
            continue;

        DirectX::XMFLOAT2 top, right, bottom, left;
        grid.TileDiamondVertices(tc.x, tc.y, top, right, bottom, left);

        verts.push_back(top);    verts.push_back(right);
        verts.push_back(right);  verts.push_back(bottom);
        verts.push_back(bottom); verts.push_back(left);
        verts.push_back(left);   verts.push_back(top);
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

    // Line 2: Tile terrain + items + objects
    {
        std::ostringstream ss;
        if (scene && input.IsHoverTileValid())
        {
            int hx = input.GetHoverTileX();
            int hy = input.GetHoverTileY();
            if (scene->InBounds(hx, hy))
            {
                ss << EditorTerrainName(scene->TileAt(hx, hy).terrain);

                for (const auto& gi : scene->groundItems)
                {
                    if (gi.tileX == hx && gi.tileY == hy)
                    {
                        ss << ", " << GroundItemName(gi);
                        if (gi.quantity > 1)
                            ss << " x" << gi.quantity;
                    }
                }

                for (const auto& obj : scene->objects)
                {
                    if (hx >= obj.x0 && hx <= obj.x1 &&
                        hy >= obj.y0 && hy <= obj.y1)
                    {
                        ss << ", Hangar";
                    }
                }
            }
        }
        if (editor.uiTileInfoLabel)
            editor.uiTileInfoLabel->SetText(ss.str());
    }

    // Line 3: Selection mode + hotkey hints
    {
        std::ostringstream ss;
        switch (editor.selection)
        {
        case EditorSelection::Tile:
            ss << "TILE (" << editor.selectedTiles.size() << " sel) "
               << "F=Floor T=Street R=Rubble W=Water L=Wall D=Door M=Metro H=Shadow";
            break;
        case EditorSelection::Item:
            ss << "ITEM (" << editor.selectedTiles.size() << " sel) "
               << "1=Hangar B=Bandage S=Stim A=Antidote F=Flash V=Vial DEL=Remove";
            break;
        default:
            ss << "LMB=Select Tile  RMB=Select Item  Ctrl+S=Save";
            break;
        }
        if (editor.selection != EditorSelection::None)
            ss << "  Shift+LMB=Add  ESC=Cancel";
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

            DirectX::XMFLOAT2 top, right, bottom, left;
            grid.TileDiamondVertices(x, y, top, right, bottom, left);

            if (y == 0 || scene.tiles[(y - 1) * gridW + x].terrain != vamp::TerrainType::Wall)
            { wallVerts.push_back(top); wallVerts.push_back(right); }
            if (x == gridW - 1 || scene.tiles[y * gridW + (x + 1)].terrain != vamp::TerrainType::Wall)
            { wallVerts.push_back(right); wallVerts.push_back(bottom); }
            if (y == gridH - 1 || scene.tiles[(y + 1) * gridW + x].terrain != vamp::TerrainType::Wall)
            { wallVerts.push_back(bottom); wallVerts.push_back(left); }
            if (x == 0 || scene.tiles[y * gridW + (x - 1)].terrain != vamp::TerrainType::Wall)
            { wallVerts.push_back(left); wallVerts.push_back(top); }
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
    // Disable RMB camera drag so RMB is free for tile selection.
    // Camera panning uses WASD + middle-mouse in editor mode.
    input.SetRMBDragEnabled(false);
    input.Update(deltaTime, camera, grid);

    // --- Esc: cancel current selection ---
    if (input.IsKeyPressed(VK_ESCAPE)) {
        if (editor.selection != EditorSelection::None) {
            editor.ClearSelection();
            OutputDebugStringA("[Editor] Selection cancelled.\n");
        }
    }

    // --- Ctrl+S: save ---
    bool ctrlHeld = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
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

    // --- LMB: select tile for terrain editing ---
    if (input.WasTileClicked() && input.IsHoverTileValid()) {
        int tx = input.GetClickTileX();
        int ty = input.GetClickTileY();

        if (shiftHeld && editor.selection == EditorSelection::Tile) {
            // Shift+LMB: toggle tile in existing Tile selection
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
            // Plain LMB: start fresh selection
            editor.SetSingleTile(tx, ty);
            editor.selection = EditorSelection::Tile;
        }

        std::ostringstream ss;
        ss << "[Editor] Tile selection: " << editor.selectedTiles.size() << " tile(s)\n";
        OutputDebugStringA(ss.str().c_str());
    }
    // --- RMB: select tile for item/object editing ---
    else if (input.WasTileRightClicked() && input.IsHoverTileValid()) {
        int tx = input.GetRightClickTileX();
        int ty = input.GetRightClickTileY();

        if (shiftHeld && editor.selection == EditorSelection::Item) {
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
            editor.selection = EditorSelection::Item;
        }

        std::ostringstream ss;
        ss << "[Editor] Item selection: " << editor.selectedTiles.size() << " tile(s)\n";
        OutputDebugStringA(ss.str().c_str());
    }

    // --- Handle active selection hotkeys ---
    if (editor.selection == EditorSelection::Tile) {
        HandleTerrainHotkey(input, sceneLoader.GetSceneData(), editor,
            sceneLoader, occluders, grid);
    }
    else if (editor.selection == EditorSelection::Item) {
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

    // Submit placed objects as sprites
    if (sceneLoader.IsLoaded())
        EditorSubmitObjects(editor, sceneLoader.GetSceneData(), grid, renderQueue, renderer);

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

        // Compute world-space center and size from tile AABB
        auto topLeft = grid.TileToWorld(obj.x0, obj.y0);
        auto botRight = grid.TileToWorld(obj.x1, obj.y1);
        float cx = (topLeft.x + botRight.x) * 0.5f;
        float cy = (topLeft.y + botRight.y) * 0.5f;

        // Size: span the tile centers plus one full tile of padding
        float tileW = grid.GetTileSize() * 0.5f;   // half diamond width
        float tileH = grid.GetTileSize() * 0.25f;  // half diamond height
        float spanX = std::fabs(botRight.x - topLeft.x) + tileW * 2.0f;
        float spanY = std::fabs(botRight.y - topLeft.y) + tileH * 2.0f;

        engine::SpriteInstance inst;
        inst.position = {cx, cy};
        inst.size = {spanX, spanY};
        inst.uvRect = {0.0f, 0.0f, 1.0f, 1.0f};
        inst.color = {1.0f, 1.0f, 1.0f, 0.85f};
        inst.rotation = 0.0f;
        inst.sortY = cy;
        inst.textureIndex = editor.objectTextures[idx].GetSRVIndex();
        inst.pad = 0;

        renderQueue.Submit(engine::RenderLayer::WallsProps, inst.sortY, 50, 0, inst);
    }
}
