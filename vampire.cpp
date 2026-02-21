// vampire.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "vampire.h"
#include "engine/Engine.h"
#include "game/SceneLoader.h"
#include "game/GenerateTestScene.h"
#include <string>
#include <sstream>
#include <cmath>
#include <sys/stat.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND  g_hWnd = nullptr;

// Engine globals
static engine::RendererD3D12   g_renderer;
static engine::SceneRenderer   g_sceneRenderer;
static engine::Camera2D        g_camera;
static engine::Grid            g_grid;
static engine::BackgroundPager g_bgPager;
static engine::RenderQueue     g_renderQueue;
static engine::LightSystem     g_lights;
static engine::OccluderSet     g_occluders;
static engine::FogRenderer     g_fog;
static engine::RoofSystem      g_roofs;
static engine::InputSystem     g_input;
static bool                    g_engineInitialized = false;
static float                   g_time = 0.0f;

// Developer mode flag
static bool                    g_devMode = true;

// Scene loader
static vamp::SceneLoader       g_sceneLoader;

// High-resolution timer
static LARGE_INTEGER            g_timerFreq;
static LARGE_INTEGER            g_timerLast;

// Player indicator
static engine::Texture2D       g_playerIndicatorTex;
static bool                    g_playerIndicatorReady = false;
static float                   g_playerFacingAngle = 0.0f; // Radians, 0 = right/east
static float                   g_playerWorldX = 0.0f;
static float                   g_playerWorldY = 0.0f;

// Debug overlay toggles
static bool                    g_showGrid = false;
static bool                    g_showWalls = false;

// Forward declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
bool                InitEngine(HWND hwnd, uint32_t width, uint32_t height);
void                ShutdownEngine();
void                RenderFrame();
void                HandleTileClick(int tileX, int tileY);
void                OutputTileInspection(const vamp::TileInspection& info);
static void         CreatePlayerIndicator();
static void         SubmitPlayerIndicator();
static void         DrawWallOverlay();

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_VAMPIRE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_VAMPIRE));

    // Initialize high-resolution timer
    QueryPerformanceFrequency(&g_timerFreq);
    QueryPerformanceCounter(&g_timerLast);

    // Real-time message loop (PeekMessage instead of GetMessage)
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            // Render a frame when no messages pending
            if (g_engineInitialized)
                RenderFrame();
        }
    }

    ShutdownEngine();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_VAMPIRE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = nullptr; // No GDI background -- D3D12 paints everything
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_VAMPIRE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 1280, 720, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
        return FALSE;

    g_hWnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Get actual client area size
    RECT rc;
    GetClientRect(hWnd, &rc);
    uint32_t width  = static_cast<uint32_t>(rc.right - rc.left);
    uint32_t height = static_cast<uint32_t>(rc.bottom - rc.top);

    if (!InitEngine(hWnd, width, height))
    {
        MessageBoxW(hWnd, L"Failed to initialize rendering engine.", L"Error", MB_OK);
        return FALSE;
    }

    return TRUE;
}

// ---------------------------------------------------------------------------
// Engine initialization
// ---------------------------------------------------------------------------
bool InitEngine(HWND hwnd, uint32_t width, uint32_t height)
{
    if (!g_renderer.Init(hwnd, width, height))
        return false;

    // Determine shader directory (next to executable)
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring shaderDir(exePath);
    size_t lastSlash = shaderDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
        shaderDir = shaderDir.substr(0, lastSlash);
    shaderDir += L"/shaders";

    engine::SceneRendererConfig config;
    config.SetFromFullRes(width, height);
    config.shaderDir = shaderDir;

    if (!g_sceneRenderer.Init(g_renderer, config))
        return false;

    // Set up camera centered at origin
    g_camera.SetViewport(width, height);
    g_camera.SetPosition(0.0f, 0.0f);
    g_camera.SetZoom(1.0f);

    // Try to load a scene file from scenes/ directory next to executable
    std::string exeDir;
    {
        char buf[MAX_PATH];
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        exeDir = buf;
        size_t sl = exeDir.find_last_of("\\/");
        if (sl != std::string::npos)
            exeDir = exeDir.substr(0, sl + 1);
    }

    std::string scenesDir = exeDir + "scenes\\";
    std::string scenePath = scenesDir + "test.vmp";

    // Create scenes directory if it doesn't exist
    CreateDirectoryA(scenesDir.c_str(), nullptr);

    // Auto-generate test scene if it doesn't exist
    {
        struct stat st;
        if (stat(scenePath.c_str(), &st) != 0)
        {
            OutputDebugStringA("[Vampire] Generating test scene: ");
            OutputDebugStringA(scenePath.c_str());
            OutputDebugStringA("\n");
            if (vamp::GenerateTestScene(scenePath))
                OutputDebugStringA("[Vampire] Test scene generated successfully.\n");
            else
                OutputDebugStringA("[Vampire] ERROR: Failed to generate test scene!\n");
        }
    }

    // Enable developer mode spawn override
    if (g_devMode)
        g_sceneLoader.SetDevModeSpawn(true, 15, 15); // Center of test map

    if (g_sceneLoader.LoadScene(scenePath, g_grid, g_occluders, g_lights,
                                 g_fog, g_roofs, g_camera))
    {
        OutputDebugStringA("[Vampire] Loaded scene: ");
        OutputDebugStringA(g_sceneLoader.GetHeader().sceneName);
        OutputDebugStringA("\n");

        // Set player position from camera (which was set to spawn point by LoadScene)
        g_playerWorldX = g_camera.GetWorldX();
        g_playerWorldY = g_camera.GetWorldY();

        if (g_devMode)
        {
            std::ostringstream ss;
            ss << "[Vampire] DEV MODE: Grid " << g_sceneLoader.GetHeader().gridWidth
               << "x" << g_sceneLoader.GetHeader().gridHeight
               << " | NPCs: " << g_sceneLoader.GetSceneData().npcs.size()
               << " | Lights: " << g_sceneLoader.GetSceneData().lights.size()
               << " | Items: " << g_sceneLoader.GetSceneData().groundItems.size()
               << " | Shops: " << g_sceneLoader.GetSceneData().shops.size()
               << "\n";
            OutputDebugStringA(ss.str().c_str());
        }
    }
    else
    {
        // No scene file -- set up a default empty grid for testing
        OutputDebugStringA("[Vampire] No scene found, using default empty grid.\n");
        g_grid.Init(32.0f, 64, 64, -1024.0f, -1024.0f);

        engine::SceneRendererConfig fogConfig;
        fogConfig.SetFromFullRes(width, height);
        g_fog.Init(fogConfig.fogResWidth, fogConfig.fogResHeight,
                   g_grid.GetWorldWidth(), g_grid.GetWorldHeight(),
                   g_grid.GetOriginX(), g_grid.GetOriginY());

        g_lights.AddLight(0.0f, 0.0f, 1.0f, 0.8f, 0.4f, 200.0f, 1.0f, 0.5f);

        g_playerWorldX = 0.0f;
        g_playerWorldY = 0.0f;
    }

    // Initialize input system
    g_input.Init();
    g_input.SetCameraPanSpeed(600.0f);
    g_input.SetZoomRange(0.25f, 4.0f);
    g_input.SetZoomSensitivity(0.15f);
    g_input.SetEdgeScrollEnabled(false);
    g_input.SetKeyboardPanEnabled(false);

    g_engineInitialized = true;
    return true;
}

void ShutdownEngine()
{
    if (g_engineInitialized)
    {
        g_sceneLoader.UnloadScene();
        g_renderer.WaitForGPU();
        if (g_playerIndicatorReady)
        {
            g_playerIndicatorTex.Shutdown(g_renderer.GetSRVHeap());
            g_playerIndicatorReady = false;
        }
        g_bgPager.Shutdown(g_renderer.GetSRVHeap());
        g_sceneRenderer.Shutdown(g_renderer);
        g_renderer.Shutdown();
        g_engineInitialized = false;
    }
}

// ---------------------------------------------------------------------------
// Tile inspection (debug output to OutputDebugString)
// ---------------------------------------------------------------------------
static const char* TerrainName(vamp::TerrainType t)
{
    switch (t)
    {
    case vamp::TerrainType::Floor:      return "Floor";
    case vamp::TerrainType::Street:     return "Street";
    case vamp::TerrainType::Rubble:     return "Rubble";
    case vamp::TerrainType::Water:      return "Water";
    case vamp::TerrainType::Wall:       return "Wall";
    case vamp::TerrainType::Door:       return "Door";
    case vamp::TerrainType::MetroTrack: return "MetroTrack";
    case vamp::TerrainType::Shadow:     return "Shadow";
    default:                            return "Unknown";
    }
}

static const char* CoverName(vamp::CoverLevel c)
{
    switch (c)
    {
    case vamp::CoverLevel::None: return "None";
    case vamp::CoverLevel::Half: return "Half";
    case vamp::CoverLevel::Full: return "Full";
    default:                     return "?";
    }
}

static const char* FactionName(vamp::Faction f)
{
    switch (f)
    {
    case vamp::Faction::Player:              return "Player";
    case vamp::Faction::VampireClanNosferatu:return "Nosferatu";
    case vamp::Faction::VampireClanTremere:  return "Tremere";
    case vamp::Faction::VampireClanBrujah:   return "Brujah";
    case vamp::Faction::VampireClanVentrue:  return "Ventrue";
    case vamp::Faction::VampireClanMalkavian:return "Malkavian";
    case vamp::Faction::Police:              return "Police";
    case vamp::Faction::Mercenary:           return "Mercenary";
    case vamp::Faction::Civilian:            return "Civilian";
    default:                                 return "Unknown";
    }
}

static const char* NPCBehaviorName(vamp::NPCBehavior b)
{
    switch (b)
    {
    case vamp::NPCBehavior::Idle:       return "Idle";
    case vamp::NPCBehavior::Patrol:     return "Patrol";
    case vamp::NPCBehavior::Guard:      return "Guard";
    case vamp::NPCBehavior::Merchant:   return "Merchant";
    case vamp::NPCBehavior::QuestGiver: return "QuestGiver";
    case vamp::NPCBehavior::Civilian:   return "Civilian";
    default:                            return "Unknown";
    }
}

static const char* TriggerTypeName(vamp::TriggerType t)
{
    switch (t)
    {
    case vamp::TriggerType::Script:     return "Script";
    case vamp::TriggerType::Trap:       return "Trap";
    case vamp::TriggerType::Dialogue:   return "Dialogue";
    case vamp::TriggerType::Ambush:     return "Ambush";
    case vamp::TriggerType::LorePickup: return "LorePickup";
    case vamp::TriggerType::Alarm:      return "Alarm";
    default:                            return "Unknown";
    }
}

void OutputTileInspection(const vamp::TileInspection& info)
{
    std::ostringstream ss;
    ss << "\n=== TILE [" << info.tileX << ", " << info.tileY << "] ===\n";

    if (info.tile)
    {
        ss << "  Terrain: " << TerrainName(info.tile->terrain)
           << " | MoveCost: " << info.tile->moveCost
           << " | Walkable: " << (info.tile->IsPassable() ? "Yes" : "No")
           << " | BlocksLoS: " << (info.tile->BlocksSight() ? "Yes" : "No")
           << "\n";
        ss << "  Cover N:" << CoverName(info.tile->coverNorth)
           << " S:" << CoverName(info.tile->coverSouth)
           << " E:" << CoverName(info.tile->coverEast)
           << " W:" << CoverName(info.tile->coverWest)
           << "\n";
        if (info.tile->isShadow)
            ss << "  [Shadow zone - stealth bonus]\n";
        if (info.tile->isLocked)
            ss << "  [Locked door]\n";
    }
    else
    {
        ss << "  (out of bounds)\n";
    }

    if (info.isPlayerSpawn)
        ss << "  ** PLAYER SPAWN **\n";

    for (const auto* npc : info.npcs)
    {
        ss << "  NPC: \"" << npc->character.name << "\""
           << " | Faction: " << FactionName(npc->character.faction)
           << " | Behavior: " << NPCBehaviorName(npc->behavior)
           << " | HP: " << npc->character.currentHP
           << " | Vampire: " << (npc->character.isVampire ? "Yes" : "No")
           << " | Hostile: " << (npc->isHostile ? "Yes" : "No")
           << " | Essential: " << (npc->isEssential ? "Yes" : "No");
        if (npc->tag[0])
            ss << " | Tag: " << npc->tag;
        ss << "\n";

        // Show weapon
        const auto& weap = vamp::GetWeaponData(npc->character.inventory.equipment.primaryWeapon);
        ss << "    Weapon: " << weap.name
           << " | Armor: " << vamp::GetArmorData(npc->character.inventory.equipment.armor).name
           << "\n";
    }

    for (const auto* gi : info.groundItems)
    {
        ss << "  Item: type=" << static_cast<int>(gi->type)
           << " templateId=" << gi->templateId
           << " qty=" << gi->quantity << "\n";
    }

    for (const auto* qi : info.questItems)
    {
        ss << "  QuestItem: questId=" << qi->questId
           << " templateId=" << qi->templateId
           << " collected=" << (qi->collected ? "Yes" : "No");
        if (qi->tag[0])
            ss << " tag=" << qi->tag;
        ss << "\n";
    }

    for (const auto* trg : info.triggers)
    {
        ss << "  Trigger: " << TriggerTypeName(trg->type)
           << " [" << trg->x0 << "," << trg->y0 << "]-["
           << trg->x1 << "," << trg->y1 << "]"
           << " oneShot=" << (trg->oneShot ? "Y" : "N")
           << " enabled=" << (trg->enabled ? "Y" : "N");
        if (trg->tag[0])
            ss << " tag=" << trg->tag;
        ss << "\n";
    }

    for (const auto* trans : info.transitions)
    {
        ss << "  Transition -> \"" << trans->targetScene << "\""
           << " spawn=[" << trans->targetSpawnX << "," << trans->targetSpawnY << "]";
        if (trans->requiresKey)
            ss << " [LOCKED, keyId=" << trans->keyItemId << "]";
        if (trans->tag[0])
            ss << " tag=" << trans->tag;
        ss << "\n";
    }

    if (info.shop)
    {
        ss << "  Shop: \"" << info.shop->shopName << "\""
           << " faction=" << FactionName(info.shop->faction);
        if (info.shop->tag[0])
            ss << " tag=" << info.shop->tag;
        ss << "\n";
    }

    if (info.safehouse)
    {
        ss << "  Safehouse: \"" << info.safehouse->name << "\""
           << " security=" << info.safehouse->securityRating
           << " cost=" << info.safehouse->accessCost
           << " discovered=" << (info.safehouse->isDiscovered ? "Y" : "N")
           << "\n";
    }

    if (info.territory)
    {
        ss << "  Territory: \"" << info.territory->name << "\""
           << " controller=" << FactionName(info.territory->controllingFaction)
           << " heat=" << info.territory->heatLevel
           << " danger=" << info.territory->dangerRating
           << "\n";
    }

    OutputDebugStringA(ss.str().c_str());
}

void HandleTileClick(int tileX, int tileY)
{
    if (!g_sceneLoader.IsLoaded())
    {
        std::ostringstream ss;
        ss << "[Click] Tile [" << tileX << ", " << tileY << "] (no scene loaded)\n";
        OutputDebugStringA(ss.str().c_str());
        return;
    }

    auto info = g_sceneLoader.InspectTile(tileX, tileY);
    OutputTileInspection(info);
}

// ---------------------------------------------------------------------------
// Per-frame rendering
// ---------------------------------------------------------------------------
void RenderFrame()
{
    // Compute delta time with high-resolution timer
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = static_cast<float>(now.QuadPart - g_timerLast.QuadPart)
                    / static_cast<float>(g_timerFreq.QuadPart);
    g_timerLast = now;

    // Clamp delta to avoid spiral-of-death on breakpoints / alt-tab
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;

    g_time += deltaTime;

    // Process input (drives camera via RMB drag/zoom, resolves tile hover/click)
    g_input.Update(deltaTime, g_camera, g_grid);

    // WASD / arrows move the player character, not the camera
    {
        float moveSpeed = 150.0f * deltaTime; // world units per second
        if (g_input.IsKeyDown('W') || g_input.IsKeyDown(VK_UP))    g_playerWorldY -= moveSpeed;
        if (g_input.IsKeyDown('S') || g_input.IsKeyDown(VK_DOWN))  g_playerWorldY += moveSpeed;
        if (g_input.IsKeyDown('A') || g_input.IsKeyDown(VK_LEFT))  g_playerWorldX -= moveSpeed;
        if (g_input.IsKeyDown('D') || g_input.IsKeyDown(VK_RIGHT)) g_playerWorldX += moveSpeed;
    }

    // Update player facing angle toward mouse cursor
    {
        float dx = g_input.GetMouseWorldX() - g_playerWorldX;
        float dy = g_input.GetMouseWorldY() - g_playerWorldY;
        if (dx * dx + dy * dy > 1.0f)
        {
            // atan2 gives angle from +X axis; the triangle texture points up (-Y),
            // so we add PI/2 to rotate from "up" to the atan2 result.
            g_playerFacingAngle = std::atan2(dy, dx) + 1.5707963f;
        }
    }

    // Handle tile clicks -- inspect what's on the clicked tile
    if (g_input.WasTileClicked())
        HandleTileClick(g_input.GetClickTileX(), g_input.GetClickTileY());

    if (g_input.WasTileRightClicked())
    {
        // Right-click: same inspection for now (game would use this for context menu)
        HandleTileClick(g_input.GetRightClickTileX(), g_input.GetRightClickTileY());
    }

    // Toggle debug overlays
    if (g_input.IsKeyPressed(VK_F2))
        g_showWalls = !g_showWalls;
    if (g_input.IsKeyPressed(VK_F3))
        g_showGrid = !g_showGrid;

    g_lights.Update(g_time);

    g_renderer.BeginFrame();

    // Create player indicator texture on first frame (needs active command list)
    if (!g_playerIndicatorReady)
        CreatePlayerIndicator();

    g_renderQueue.Clear();

    // Submit player indicator to render queue
    SubmitPlayerIndicator();

    // Update fog visibility
    g_fog.ClearVisible();
    g_fog.ComputeVisibility(g_playerWorldX, g_playerWorldY,
                             300.0f, g_occluders);
    g_fog.UpdateExplored();

    // Update roofs
    int playerTileX, playerTileY;
    g_grid.WorldToTile(g_playerWorldX, g_playerWorldY,
                        playerTileX, playerTileY);
    g_roofs.Update(deltaTime, playerTileX, playerTileY);

    // Update background pager
    g_bgPager.Update(g_camera, g_renderer.GetFrameIndex());

    // Render everything
    g_sceneRenderer.RenderFrame(g_renderer, g_camera, g_bgPager,
                                 g_renderQueue, g_lights, g_occluders,
                                 g_fog, g_roofs, g_time);

    // Draw debug overlays (backbuffer is in PRESENT state after RenderFrame,
    // so transition back to RT for overlays)
    if (g_showGrid || g_showWalls)
    {
        g_renderer.TransitionBackBufferToRT();

        if (g_showGrid)
            g_sceneRenderer.DrawGridOverlay(g_renderer, g_camera, g_grid,
                                             1.0f, 1.0f, 1.0f, 0.15f);

        if (g_showWalls)
            DrawWallOverlay();

        g_renderer.TransitionBackBufferToPresent();
    }

    g_renderer.EndFrame();

    // Snapshot input state for next frame's edge detection
    g_input.EndFrame();
}

// ---------------------------------------------------------------------------
// Player indicator (procedural red triangle texture)
// ---------------------------------------------------------------------------
static void GenerateTriangleTexture(uint8_t* pixels, uint32_t size)
{
    // Generate an upward-pointing filled triangle with anti-aliased edges.
    // The triangle points upward (toward -Y) within the texture, which
    // corresponds to the "default" facing direction. Rotation is applied
    // via the SpriteInstance rotation field.
    //
    // Triangle vertices (in normalized [0,1] coords):
    //   Top:          (0.5, 0.1)
    //   Bottom-left:  (0.15, 0.85)
    //   Bottom-right: (0.85, 0.85)

    float ax = 0.5f,  ay = 0.1f;
    float bx = 0.15f, by = 0.85f;
    float cx = 0.85f, cy = 0.85f;

    for (uint32_t y = 0; y < size; ++y)
    {
        for (uint32_t x = 0; x < size; ++x)
        {
            float px = (x + 0.5f) / size;
            float py = (y + 0.5f) / size;

            // Barycentric / edge function test
            float w0 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            float w1 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
            float w2 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);

            bool inside = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) ||
                          (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);

            // Soft edge: compute signed distance to nearest edge for anti-aliasing
            float alpha = 0.0f;
            if (inside)
            {
                // Distance to each edge (approximate, using minimum edge function value)
                float minEdge = (std::min)({ std::fabs(w0), std::fabs(w1), std::fabs(w2) });
                float edgeDist = minEdge * size * 0.5f; // Scale to pixels
                alpha = (std::min)(edgeDist * 2.0f, 1.0f); // Soft edge over ~0.5 pixels
            }

            uint32_t idx = (y * size + x) * 4;
            pixels[idx + 0] = 220;  // R
            pixels[idx + 1] = 40;   // G
            pixels[idx + 2] = 40;   // B
            pixels[idx + 3] = static_cast<uint8_t>(alpha * 255.0f);
        }
    }
}

static void CreatePlayerIndicator()
{
    constexpr uint32_t kTexSize = 32;
    std::vector<uint8_t> pixels(kTexSize * kTexSize * 4, 0);
    GenerateTriangleTexture(pixels.data(), kTexSize);

    auto* device  = g_renderer.GetDevice();
    auto* cmdList = g_renderer.GetCommandList();

    g_playerIndicatorTex.CreateFromRGBA(device, cmdList,
                                         g_renderer.GetUploadManager(),
                                         g_renderer.GetSRVHeap(),
                                         kTexSize, kTexSize, pixels.data());
    g_playerIndicatorReady = true;
}

static void SubmitPlayerIndicator()
{
    if (!g_playerIndicatorReady || !g_playerIndicatorTex.IsValid())
        return;

    engine::SpriteInstance inst;
    inst.position     = { g_playerWorldX, g_playerWorldY };
    inst.size         = { 24.0f, 24.0f };
    inst.uvRect       = { 0.0f, 0.0f, 1.0f, 1.0f };
    inst.color        = { 1.0f, 1.0f, 1.0f, 0.9f };
    inst.rotation     = g_playerFacingAngle;
    inst.sortY        = g_playerWorldY;
    inst.textureIndex = g_playerIndicatorTex.GetSRVIndex();
    inst.pad          = 0;

    g_renderQueue.Submit(engine::RenderLayer::Actors, inst.sortY, 100, 0, inst);
}

static void DrawWallOverlay()
{
    if (!g_sceneLoader.IsLoaded())
        return;

    const auto& scene = g_sceneLoader.GetSceneData();
    int gridW = scene.header.gridWidth;
    int gridH = scene.header.gridHeight;
    float ts  = scene.header.tileSize;
    float ox  = scene.header.originX;
    float oy  = scene.header.originY;

    // Build line segments for wall tile edges
    std::vector<DirectX::XMFLOAT2> verts;
    verts.reserve(gridW * gridH); // rough estimate

    for (int y = 0; y < gridH; ++y)
    {
        for (int x = 0; x < gridW; ++x)
        {
            const auto& tile = scene.tiles[y * gridW + x];
            if (tile.terrain != vamp::TerrainType::Wall)
                continue;

            float left   = ox + x * ts;
            float top    = oy + y * ts;
            float right  = left + ts;
            float bottom = top + ts;

            // Draw edges where the wall tile borders a non-wall tile
            // Top edge
            if (y == 0 || scene.tiles[(y - 1) * gridW + x].terrain != vamp::TerrainType::Wall)
            {
                verts.push_back({ left,  top });
                verts.push_back({ right, top });
            }
            // Bottom edge
            if (y == gridH - 1 || scene.tiles[(y + 1) * gridW + x].terrain != vamp::TerrainType::Wall)
            {
                verts.push_back({ left,  bottom });
                verts.push_back({ right, bottom });
            }
            // Left edge
            if (x == 0 || scene.tiles[y * gridW + (x - 1)].terrain != vamp::TerrainType::Wall)
            {
                verts.push_back({ left, top });
                verts.push_back({ left, bottom });
            }
            // Right edge
            if (x == gridW - 1 || scene.tiles[y * gridW + (x + 1)].terrain != vamp::TerrainType::Wall)
            {
                verts.push_back({ right, top });
                verts.push_back({ right, bottom });
            }
        }
    }

    if (!verts.empty())
    {
        g_sceneRenderer.DrawLineOverlay(g_renderer, verts.data(), verts.size(),
                                         0.45f, 0.25f, 0.1f, 0.8f);
    }
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Forward input messages to the input system first
    if (g_engineInitialized)
    {
        if (g_input.HandleMessage(message, wParam, lParam))
        {
            switch (message)
            {
            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
                SetCapture(hWnd);
                return 0;
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
                ReleaseCapture();
                return 0;
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_MOUSEMOVE:
            case WM_MOUSEWHEEL:
                return 0;
            default:
                break;
            }
        }
    }

    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }
        break;

    case WM_SIZE:
        if (g_engineInitialized && wParam != SIZE_MINIMIZED)
        {
            uint32_t w = LOWORD(lParam);
            uint32_t h = HIWORD(lParam);
            if (w > 0 && h > 0)
            {
                g_renderer.Resize(w, h);
                g_sceneRenderer.Resize(g_renderer, w, h);
                g_camera.SetViewport(w, h);
            }
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
