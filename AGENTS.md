# AGENTS.md — Vampire Project Reference

## Overview

A 2D top-down vampire RPG built as a native Win32/D3D12 application in C++14. The player is a human turned vampire navigating city underground, clan politics, and tactical turn-based combat. The project is split into two layers: a custom Direct3D 12 rendering engine (`engine/`) and game logic/RPG systems (`game/`).

## Build

- **IDE:** Visual Studio 2022 (v143 toolset)
- **Solution:** `vampire.sln` ? single project `vampire.vcxproj`
- **Configurations:** Debug/Release ? Win32/x64 (primary target: Debug|x64)
- **C++ Standard:** C++14 (do not use C++17 or later features)
- **Platform:** Windows 10+, Windows SDK 10.0
- **Entry point:** `vampire.cpp` ? `wWinMain` (Win32 subsystem, Unicode)
- **Post-build:** Copies `engine/shaders/` ? `$(OutDir)shaders/` and `assets/` ? `$(OutDir)assets/`
- **Linked libraries:** d3d12, dxgi, d3dcompiler, windowscodecs (WIC for PNG loading). All linked via `#pragma comment(lib)` in source files — no manual linker settings needed.

## Project Structure

```
vampire.sln / vampire.vcxproj    Solution and project files
vampire.cpp                      Application entry point, game loop, engine wiring
framework.h                      Precompiled Windows includes (WIN32_LEAN_AND_MEAN)
vampire.h / vampire.rc           Win32 resource definitions

engine/                          2D rendering engine (D3D12)
  Engine.h                       Master include for all engine headers
  EngineTypes.h                  Shared types: SpriteInstance, RenderLayer, PointLight2D, constants
  RendererD3D12.h/.cpp           Core device, swap chain, command lists, descriptor heaps, frame sync
  DescriptorAllocator.h/.cpp     Persistent (free-list) and linear (per-frame bump) descriptor allocators
  UploadManager.h/.cpp           Ring-buffer upload heap for CPU?GPU transfers
  Texture2D.h/.cpp               GPU texture wrapper: LoadFromDDS, LoadFromPNG (WIC), CreateFromRGBA
  RenderTarget.h/.cpp            Render target and depth/stencil wrappers
  PipelineStates.h/.cpp          All root signatures and PSOs (sprite, shadow, light, composite, grid)
  Camera2D.h                     Orthographic camera: world/screen transforms, zoom, culling
  Grid.h                         Tile-grid coordinate system (world?tile conversions)
  InputSystem.h                  Keyboard/mouse input, camera control, tile picking (header-only)
  BackgroundPager.h/.cpp         Paged DDS background streaming + single background image (PNG)
  RenderQueue.h                  Sortable render item queue with layer-based ranges (header-only)
  SceneRenderer.h/.cpp           Multi-pass renderer orchestrating all render passes
  LightSystem.h                  Dynamic point lights + shadow quad generation (header-only)
  OccluderSet.h                  Wall edge segments for LoS / shadow casting (header-only)
  FogRenderer.h/.cpp             CPU fog-of-war visibility rasterization + GPU texture upload
  RoofSystem.h                   Roof footprints with fade-on-enter logic (header-only)
  SpriteAtlas.h                  Sprite atlas stub (header-only, not yet implemented)

  shaders/                       HLSL shaders (compiled at runtime via D3DCompileFromFile, SM 5.1)
    SpriteVS.hlsl                Instanced sprite vertex shader (generates quad from SV_VertexID)
    SpritePS.hlsl                Bindless texture sampling: textures[instanceTexIndex].Sample()
    ShadowVolumeVS.hlsl          Stencil-only shadow volume pass
    LightRadialVS.hlsl           Light quad vertex shader (world-space bounding box)
    LightRadialPS.hlsl           Radial falloff light pixel shader (additive blend, stencil test)
    CompositeVS.hlsl             Fullscreen triangle (3 vertices, no VB)
    CompositePS.hlsl             Final compositing: SceneColor ? fog + LightAccum
    GridOverlayVS.hlsl           Debug grid/wall line overlay vertex shader
    GridOverlayPS.hlsl           Debug grid/wall line overlay pixel shader

game/                            RPG systems and game logic
  GameSystems.h                  Master include for all game headers
  SceneData.h                    Complete scene/level data structures for .vmp binary format
  SceneFile.h/.cpp               Binary serialization/deserialization of .vmp scene files
  SceneLoader.h/.cpp             Loads .vmp scenes into engine systems, tile inspection API
  GenerateTestScene.h/.cpp       Procedurally generates a 32?32 test scene with NPCs, items, lights
  MapTile.h                      Tile: terrain type, directional cover, LoS, movement cost
  Character.h/.cpp               Full character sheet: attributes, skills, inventory, status
  Attributes.h                   6 core attributes (STR/AGI/END/PER/INT/CHA), derived stats
  SkillDefs.h                    26 skill IDs with categories and metadata
  Skill.h/.cpp                   Skill checks (3d6 ? attr+rank+mods), opposed rolls
  Dice.h/.cpp                    3d6 rolls, random number utilities
  Weapon.h                       14 weapon types data table (header-only)
  Armor.h                        6 armor types data table (header-only)
  Inventory.h/.cpp               Equipment slots, backpack, consumables, money
  StatusEffect.h/.cpp            Status effects: Bleed, Stun, Pinned, Crippled, etc.
  CoverSystem.h/.cpp             Directional cover queries, Bresenham LoS, flanking detection
  GameWorld.h/.cpp               Grid map, territories, clans, heat system
  FogOfWar.h/.cpp                Game-level vision and fog-of-war mechanics
  CombatSystem.h/.cpp            Turn-based combat resolution with AP
  Discipline.h/.cpp              8 vampire blood disciplines
  SocialSystem.h/.cpp            Dialogue, persuasion, deception, empathy
  SleepSystem.h/.cpp             Safehouses, sleep restoration, ambush mechanics

assets/                          Game assets (copied to output by post-build)
  hangar/hangar.png              Test background image
```

## Namespaces

- **`engine`** — All rendering/engine code. Types: `RendererD3D12`, `SceneRenderer`, `Camera2D`, `Grid`, `BackgroundPager`, `RenderQueue`, `LightSystem`, `OccluderSet`, `FogRenderer`, `RoofSystem`, `InputSystem`, `Texture2D`, `PipelineStates`, `PersistentDescriptorAllocator`, `LinearDescriptorAllocator`, `UploadManager`, `SpriteInstance`, `RenderLayer`, `PointLight2D`, etc.
- **`vamp`** — All game logic. Types: `SceneData`, `SceneFile`, `SceneLoader`, `Character`, `MapTile`, `CombatSystem`, `GameWorld`, `Discipline`, `SocialSystem`, `SleepSystem`, etc.

## Rendering Pipeline

The renderer uses Direct3D 12 with a multi-pass architecture orchestrated by `SceneRenderer::RenderFrame()`:

1. **Background pass** ? SceneColor RT: Renders single background image (PNG) and/or paged DDS background tiles as instanced sprites.
2. **Base scene pass** ? SceneColor RT: Renders all game sprites (ground tiles, walls, actors, roofs) as instanced quads, Y-sorted within layers.
3. **Lighting pass** ? LightAccum RT (half-res R11G11B10_FLOAT): For each point light, renders stencil shadow volumes from occluder edges, then renders radial falloff quad with stencil test (additive blend).
4. **Composite pass** ? Backbuffer: Fullscreen triangle combines SceneColor ? fog-of-war darkening + LightAccum ? visibility.
5. **UI pass** ? Backbuffer: Screen-space icons/HUD as instanced sprites.
6. **Debug overlays** (optional) ? Backbuffer: Grid lines and wall edge highlights via line-draw PSO.

### Render Layers (draw order)
`BackgroundPages` ? `GroundTiles` ? `WallsProps` ? `Actors` ? `Roofs` ? `RoofActors` ? `ScreenSpaceIcons` ? `FogOverlay`

### Root Signatures
- **Main** (sprite, shadow, grid): CBV b0 (FrameConstants) + descriptor table (1024 SRVs at t0) + static sampler s0 (linear clamp)
- **Light**: CBV b0 (FrameConstants) + CBV b1 (LightConstants)
- **Composite**: CBV b0 (FrameConstants) + descriptor table (4 SRVs: SceneColor, LightAccum, FogVis, FogExp) + static samplers s0 (point), s1 (linear)

### Sprite Rendering
Sprites are rendered as instanced triangle strips (4 vertices per quad). Per-instance data (`SpriteInstance`, 64 bytes) is uploaded each frame from the CPU ring buffer. The vertex shader generates quad corners from `SV_VertexID` and applies rotation. The pixel shader does bindless texture lookup: `textures[instanceTexIndex].Sample()`.

### Key GPU Resource Management
- **Descriptor heaps:** One persistent shader-visible SRV heap (1024 slots, free-list), one RTV heap, one DSV heap. Per-frame linear allocators for transient descriptors.
- **Upload ring buffer:** Single persistent upload heap (`UploadManager`). Frame markers track reuse safety via fence values.
- **Double buffering:** 2 frames in flight (`kMaxFramesInFlight = 2`), fence-based CPU/GPU sync.

## Scene File Format (.vmp)

Binary format with magic `"VMP1"` (version 1). Layout:
```
[SceneHeader]                    magic, version, grid dims, tile size, origin, scene name
[uint32 tileCount]               followed by MapTile ? count
[uint32 bgPageCount]             followed by SceneBackgroundPage ? count
[string backgroundImagePath]     length-prefixed UTF-8 string (relative path to PNG)
[int32 playerSpawnX/Y]
[NPCs with full Character blobs]
[Patrol routes]
[Ground items, Quest items]
[Triggers, Transitions]
[Shops, Safehouses]
[Lights, Roofs]
[Territories]
[int32 globalHeat, dayCount]
```

Strings are serialized as `[uint32 length][char ? length]`. Fixed-size char arrays (names, tags) are written raw. The `SceneFile` class handles all serialization. `GenerateTestScene` creates a test .vmp procedurally.

**Important:** In dev mode (`g_devMode = true`), the test scene is always regenerated on startup. This means changes to `SceneData` or the binary format take effect immediately without manually deleting old scene files.

## Background Image System

Scenes can specify a background image via `SceneData::backgroundImagePath` (relative to exe directory, e.g., `"assets\\hangar\\hangar.png"`). At runtime, `BackgroundPager::SetBackgroundImage()` loads the PNG via `Texture2D::LoadFromPNG()` (using Windows Imaging Component / WIC) and maps it to cover the entire grid world area. The image is rendered as a single sprite in the background pass before any paged DDS tiles.

## Game Loop (vampire.cpp)

`wWinMain` ? `InitInstance` ? `InitEngine` ? real-time `PeekMessage` loop ? `RenderFrame()` each idle frame.

### InitEngine flow:
1. Create D3D12 renderer
2. Create SceneRenderer with shader directory
3. Set up camera at origin
4. Generate test scene if needed (always in dev mode)
5. Load .vmp scene ? populate Grid, OccluderSet, LightSystem, FogRenderer, RoofSystem, Camera
6. Queue background image load if scene specifies one
7. Initialize InputSystem (WASD moves player, RMB drags camera, scroll zooms)

### RenderFrame flow:
1. Compute delta time (high-res timer, clamped to 100ms)
2. Process input ? update camera, player movement (WASD at 150 units/sec), player facing (toward mouse)
3. Handle tile clicks ? `SceneLoader::InspectTile()` ? `OutputDebugStringA`
4. Toggle debug overlays (F2 = walls, F3 = grid)
5. `BeginFrame` ? create player indicator texture (first frame) ? load background image (first frame)
6. Submit player indicator to render queue
7. Update fog visibility, roofs, background pager
8. `SceneRenderer::RenderFrame()` ? all render passes
9. Draw debug overlays if enabled
10. `EndFrame` ? present

### Player Indicator
A 32?32 procedural red triangle texture generated on first frame. Submitted as a sprite in the `Actors` layer. Rotation tracks the mouse cursor via `atan2`.

## RPG System

### Attributes (1–12)
STR, AGI, END, PER, INT, CHA. Derived: HP = 8+END+?STR/2?, AP/turn = 6+?AGI/2?, Vision = 6+?PER/2? tiles.

### Skill Checks
All checks: `3d6 ? (Attribute + SkillRank + modifiers)`. 26 skills across Combat, Thief, Social, and Vampire Discipline categories.

### Combat
Turn-based with Action Points. Directional cover (half: ?2, full: ?4). Features: overwatch, suppression, burst fire, wound effects (Bleeding, Stunned, Crippled, Pinned).

### Vampire Disciplines
8 disciplines powered by Blood Reserve (max 6 + Blood Potency): Blink, Telekinesis, Hemocraft, Blood Mark, Auspex, Domination, Obfuscate, Celerity.

### Factions
Player, 5 vampire clans (Nosferatu, Tremere, Brujah, Ventrue, Malkavian), Police, Mercenary, Civilian.

## Coding Conventions

- **C++14 only.** No `std::optional`, `std::string_view`, structured bindings, `if constexpr`, or other C++17+ features.
- **`(std::min)` / `(std::max)` with parentheses** to avoid Windows macro conflicts.
- Header-only classes are common in `engine/` for simple systems (Grid, Camera2D, InputSystem, RenderQueue, LightSystem, OccluderSet, RoofSystem).
- Engine types use `DirectX::XMFLOAT2/4` for GPU-facing data, plain `float` for CPU logic.
- No third-party libraries. PNG loading uses Windows Imaging Component (WIC). DDS loading is hand-rolled. Shaders are compiled at runtime via `D3DCompileFromFile`.
- Win32 API for windowing, timing (`QueryPerformanceCounter`), file I/O.
- `OutputDebugStringA` for all debug logging (visible in VS Output window).
- Game data types use fixed-size char arrays (e.g., `char tag[32]`, `char sceneName[64]`) for binary serialization, `std::string` for variable-length data.
- Master include headers: `engine/Engine.h` and `game/GameSystems.h`.
- Global engine state lives as `static` variables in `vampire.cpp` (no singleton pattern).

## Key Constants (EngineTypes.h)

| Constant | Value | Purpose |
|---|---|---|
| `kMaxFramesInFlight` | 2 | Double-buffered rendering |
| `kMaxLights` | 32 | Maximum dynamic point lights |
| `kMaxSpriteInstances` | 4096 | Max sprites per frame |
| `kMaxPersistentSRVs` | 1024 | SRV heap size / bindless texture limit |
| `kPageSizePixels` | 512 | Background page tile size |

## Important Warnings

- The .vmp binary format has no backward compatibility — any change to `SceneData` structures or serialization order requires regenerating all scene files. Dev mode handles this automatically for the test scene.
- Shaders are compiled from source HLSL files at startup (`D3DCompileFromFile`). They must be present in `$(OutDir)shaders/`. If shaders fail to compile, the application will fail to initialize.
- The upload ring buffer has a fixed size. Very large textures (e.g., 8K+ background images) may exceed the ring buffer capacity and fail to upload.
- `Texture2D::LoadFromPNG` calls `CoInitializeEx` if COM is not already initialized. This is safe for single-threaded use but should be noted.

## Grid System (Isometric)

The `Grid` class (`engine/Grid.h`) uses a 2:1 diamond isometric projection. The internal data model (tile grid, NPCs, items, etc.) is always a rectangular array — only the world-space projection is isometric.

### Coordinate System
- `tileSize` = diamond width in pixels; diamond height = `tileSize / 2`
- `halfW = tileSize / 2`, `halfH = tileSize / 4`
- `(originX, originY)` = **top-left corner** of the grid's world-space bounding box (same position used by the background image)
- `GetWorldWidth()` = `(gridWidth + gridHeight) * tileSize / 2`
- `GetWorldHeight()` = `(gridWidth + gridHeight) * tileSize / 4`

### Tile ? World Transforms
- `TileToWorld(x, y)` ? tile center at:
  - `wx = originX + (gridHeight + x - y) * halfW`
  - `wy = originY + (x + y) * halfH + halfH`
- `WorldToTile(wx, wy)` ? inverse:
  - `rx = (wx - originX) / halfW - gridHeight`
  - `ry = (wy - originY) / halfH - 1`
  - `tileX = floor((rx + ry) / 2)`, `tileY = floor((ry - rx) / 2)`
- `TileDiamondVertices(x, y)` ? 4 diamond corners: top, right, bottom, left

### Layout
Tile (0,0) top vertex is at the **top-center** of the bounding box. Increasing tileX moves right-and-down; increasing tileY moves left-and-down. The full diamond grid fits exactly within the `(originX, originY, worldWidth, worldHeight)` bounding box.

### Occluders
`OccluderSet::BuildFromTileGridIsometric()` generates diamond-shaped wall edge segments using the grid's `TileDiamondVertices()`. Each diamond edge between a wall tile and a non-wall tile becomes an occluder segment.

### Debug Hotkeys

| Key | Action |
|-----|--------|
| **F2** | Toggle wall edge overlay (diamond edges in isometric) |
| **F3** | Toggle grid overlay (diamond wireframes) |
| **WASD** | Move player character |
| **Mouse wheel** | Zoom camera |
| **RMB drag** | Pan camera |
| **LMB click** | Inspect tile (output to debug log) |
