# AGENTS.md - Vampire Project Reference

This document is the authoritative reference for AI coding agents working on this
project. It supersedes README.md where they disagree. README.md is for humans
browsing the repo; this file is for agents and is kept more complete and current.

## Overview

A 2D top-down vampire RPG built as a native Win32/D3D12 application in C++14. The
player is a human turned vampire navigating city underground, clan politics, and
tactical turn-based combat. The project is split into three layers:

- `engine/` - custom Direct3D 12 rendering engine (no third-party libraries)
- `game/`   - RPG systems and game logic (`vamp` namespace)
- `editor/` - in-game scene editor (no namespace; types live at global scope)
- `ui/`     - immediate-mode UI system (`ui` namespace)

## Build

- **IDE:** Visual Studio 2022 (v143 toolset)
- **Solution:** `vampire.sln` -> single project `vampire.vcxproj`
- **Configurations:** Debug/Release x Win32/x64 (primary target: Debug|x64)
- **C++ Standard:** C++14 (do not use C++17 or later features)
- **Platform:** Windows 10+, Windows SDK 10.0
- **Entry point:** `vampire.cpp` -> `wWinMain` (Win32 subsystem, Unicode)
- **Post-build:** Copies `engine/shaders/` -> `$(OutDir)shaders/` and `assets/` -> `$(OutDir)assets/`
- **Linked libraries:** d3d12, dxgi, d3dcompiler, windowscodecs (WIC for PNG loading).
  All linked via `#pragma comment(lib)` in source files - no manual linker settings needed.

### vcxproj is hand-maintained
**Important:** `vampire.vcxproj` and `vampire.vcxproj.filters` contain explicit
file lists. Adding or renaming a `.h` or `.cpp` file requires updating both files
or the project will not see the new file. Helper scripts `update_vcxproj.ps1`
and `update_filters.ps1` exist but are one-shot migration scripts, not generic
add-file tools. The simplest correct workflow is:

1. Add the file to disk under `engine/`, `game/`, `editor/`, or `ui/`.
2. Open the solution in Visual Studio and use *Add -> Existing Item* (this
   updates both `.vcxproj` and `.vcxproj.filters` automatically), or hand-edit
   both files following the existing patterns (`<ClInclude>` for headers,
   `<ClCompile>` for sources, plus a matching entry in the filters file).
3. Rebuild.

### Other build helpers
- `copyAssets.ps1` - copies `assets/` to `x64/Debug/assets/` if the post-build
  step did not run (e.g. after a manual asset change without a rebuild).
- `copyToUSB.ps1` / `copyFromUSB.ps1` - dev workflow scripts, not relevant to
  building or editing code.

## Project Structure

```
vampire.sln / vampire.vcxproj    Solution and project files (vcxproj is hand-maintained)
vampire.cpp                      Application entry point, game loop, engine wiring
framework.h                      Precompiled Windows includes (WIN32_LEAN_AND_MEAN)
vampire.h / vampire.rc           Win32 resource definitions
TODO.md                          Free-form scratch notes (currently empty)

engine/                          2D rendering engine (D3D12, namespace `engine`)
  Engine.h                       Master include for all engine headers
  EngineTypes.h                  Shared types: SpriteInstance, RenderLayer, PointLight2D, constants
  RendererD3D12.h/.cpp           Core device, swap chain, command lists, descriptor heaps, frame sync
  DescriptorAllocator.h/.cpp     Persistent (free-list) and linear (per-frame bump) descriptor allocators
  UploadManager.h/.cpp           Ring-buffer upload heap for CPU->GPU transfers
  Texture2D.h/.cpp               GPU texture wrapper: LoadFromDDS, LoadFromPNG (WIC), CreateFromRGBA
  RenderTarget.h/.cpp            Render target and depth/stencil wrappers
  PipelineStates.h/.cpp          All root signatures and PSOs (sprite, shadow, light, composite, grid)
  Camera2D.h                     Orthographic camera: world/screen transforms, zoom, culling
  Grid.h                         Tile-grid coordinate system (rectangular OR isometric flat-top hex)
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
    SpritePointPS.hlsl           Point-filtered sprite pixel shader
    ShadowVolumeVS.hlsl          Stencil-only shadow volume pass
    LightRadialVS.hlsl           Light quad vertex shader (world-space bounding box)
    LightRadialPS.hlsl           Radial falloff light pixel shader (additive blend, stencil test)
    CompositeVS.hlsl             Fullscreen triangle (3 vertices, no VB)
    CompositePS.hlsl             Final compositing: SceneColor * fog + LightAccum
    GridOverlayVS.hlsl           Debug grid/wall line overlay vertex shader
    GridOverlayPS.hlsl           Debug grid/wall line overlay pixel shader

game/                            RPG systems and game logic (namespace `vamp`)
  GameSystems.h                  Master include for all game headers
  SceneData.h                    Complete scene/level data structures for .vmp binary format
  SceneFile.h/.cpp               Binary serialization/deserialization of .vmp scene files
  SceneLoader.h/.cpp             Loads .vmp scenes into engine systems, tile inspection API
  GenerateTestScene.h/.cpp       Procedurally generates a 32x32 test scene with NPCs, items, lights
  MapTile.h                      Tile: terrain type, directional cover, LoS, movement cost
  Character.h/.cpp               Full character sheet: attributes, skills, inventory, status
  Attributes.h                   6 core attributes (STR/AGI/END/PER/INT/CHA), derived stats
  SkillDefs.h                    26 skill IDs with categories and metadata
  Skill.h/.cpp                   Skill checks (3d6 <= attr+rank+mods), opposed rolls
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

editor/                          In-game scene editor (no namespace; global types)
  EditorMode.h/.cpp              Tile painting, item/object placement, light placement,
                                 context-menu UI, searchable dropdowns, Ctrl+S save,
                                 configurable hotkeys via editor.ini

ui/                              Immediate-mode UI system (namespace `ui`)
  UI.h                           Master include header
  UITypes.h                      Core types: Rect, Color, Anchor, TextAlign
  BitmapFont.h/.cpp              Runtime font atlas baked from Windows GDI
  UIElement.h                    Base element with tree layout and hit testing
  UIPanel.h                      Solid-color panel with optional border
  UILabel.h                      Text element using BitmapFont
  UIDropdown.h                   Searchable dropdown selector with filter textbox
  UIRenderer.h/.cpp              Screen-space quad batching via the sprite PSO
  UISystem.h/.cpp                Root UI manager, element creation, char forwarding (WM_CHAR)

assets/                          Game assets (copied to output by post-build)
  hangar/hangar.png              Test background image
```

## Namespaces

- **`engine`** - All rendering/engine code. Types: `RendererD3D12`, `SceneRenderer`,
  `Camera2D`, `Grid`, `BackgroundPager`, `RenderQueue`, `LightSystem`, `OccluderSet`,
  `FogRenderer`, `RoofSystem`, `InputSystem`, `Texture2D`, `PipelineStates`,
  `PersistentDescriptorAllocator`, `LinearDescriptorAllocator`, `UploadManager`,
  `SpriteInstance`, `RenderLayer`, `PointLight2D`, etc.
- **`vamp`** - All game logic. Types: `SceneData`, `SceneFile`, `SceneLoader`,
  `Character`, `MapTile`, `CombatSystem`, `GameWorld`, `Discipline`, `SocialSystem`,
  `SleepSystem`, `WeaponData`, `ArmorData`, `TerrainType`, `Faction`, etc.
- **`ui`** - UI system. Types: `UISystem`, `UIPanel`, `UILabel`, `UIDropdown`,
  `BitmapFont`, `Anchor`, `Color`, `TextAlign`, `Rect`.
- **No namespace** - Editor types (`EditorState`, `EditorMode`, `TileCoord`,
  `EditorTerrainAsset`, `EditorContextMenuPage`, etc.) live at global scope.
  Functions like `EditorFrame`, `EditorInitUI` are also global. This is
  intentional - the editor is a thin layer that wires everything together and
  is not meant to be a reusable library.

## Dev Mode (`g_devMode`)

`g_devMode` is a `static bool` in `vampire.cpp`, currently hard-coded to `true`.
There is no command-line flag for it; flip it manually and recompile to disable.

When `g_devMode` is `true`:

1. **Test scene auto-regen.** On startup, `scenes/test.vmp` is regenerated from
   `GenerateTestScene()`, even if the file already exists. This means changes
   to `SceneData` layout, `SceneFile` serialization, or the procedural generator
   take effect immediately without manually deleting old `.vmp` files.
   **Exception:** if `--editor` is also set, regen is suppressed so manual edits
   are preserved.
2. **Spawn override.** The player is force-spawned at tile (15, 15) regardless
   of what the scene file says.
3. **Verbose startup log.** Grid dimensions, NPC count, light count, item count,
   and shop count are printed to `OutputDebugString` after scene load.

When working on scene format changes, leave `g_devMode = true`. When testing
saved scenes from the editor, either run with `--editor` (which suppresses
regen) or set `g_devMode = false`.

## Where to Add Things

This section maps common tasks to the files you need to touch. Most additions
are data-table extensions, not new systems.

### A new weapon
1. Add a value to `enum class WeaponType` in `game/Weapon.h` (before `COUNT`).
2. Add a row to the `table[]` array in `GetWeaponData()` in the same file.
   Order must match the enum.
3. If the weapon should appear in the editor's item dropdown, also touch
   `editor/EditorMode.cpp` (search for the existing item list).
4. No `.cpp` changes needed for `Weapon.h` itself - it's header-only.

### A new armor type
1. Add a value to `enum class ArmorType` in `game/Armor.h`.
2. Add a row to the `GetArmorData()` table.
3. Header-only, no `.cpp` changes.

### A new skill
1. Add the ID to `enum class SkillId` in `game/SkillDefs.h`.
2. Add a row to the skill metadata table in the same file (name, governing
   attribute, category).
3. Update `Character::SkillEffective()` if the skill needs special handling.
4. No new file needed.

### A new vampire discipline
1. Add the discipline to `enum class DisciplineType` in `game/Discipline.h`.
2. Implement its effect in `game/Discipline.cpp`.
3. Update `CombatSystem::UseDiscipline` (or the equivalent dispatcher) if it
   has combat-time effects.
4. Update the clan specialty tables if relevant.

### A new terrain type
1. Add the value to `enum class TerrainType` in `game/SceneData.h` (or
   `MapTile.h`, wherever the enum currently lives - check both).
2. Update `MapTile::IsPassable()` and `MapTile::BlocksSight()` for the new tile.
3. Add a `TerrainName()` case in `vampire.cpp` (used by tile inspection debug
   output).
4. Add an `EditorTerrainAsset` entry in `editor/EditorMode.cpp` so the editor
   can paint it.
5. **Important:** changing `TerrainType` is a `.vmp` format change. Any
   existing scene files will deserialize incorrectly. With `g_devMode = true`
   and not in editor mode, the test scene regenerates automatically; otherwise
   delete `scenes/*.vmp` manually.

### A new status effect
1. Add to `enum class StatusType` in `game/StatusEffect.h`.
2. Implement application/tick logic in `game/StatusEffect.cpp` and the relevant
   `Character` methods (`GetHitModifier`, `GetAPCostMultiplier`, etc.).
3. Add a case to `CombatSystem::ApplyWoundEffects` if the effect should be
   applied automatically by hits.

### A new scene field (e.g. weather, time of day)
1. Add the field to `SceneData` (or `SceneHeader`) in `game/SceneData.h`.
2. Update `SceneFile::Save()` and `SceneFile::Load()` in
   `game/SceneFile.cpp` - **both sides must match exactly**, in the same order.
   The `.vmp` format has no versioning beyond the magic number.
3. If the field affects rendering or gameplay, propagate it through
   `SceneLoader::LoadScene` to the relevant subsystems.
4. Regenerate `scenes/test.vmp` (automatic in dev mode).

### A new render pass
1. Add the pass orchestration to `engine/SceneRenderer::RenderFrame()`.
2. If the pass needs new shaders, add `.hlsl` files to `engine/shaders/` and
   make sure they get copied by the post-build step (they should, since the
   step copies the whole directory).
3. If the pass needs a new PSO/root signature, add it to
   `engine/PipelineStates.cpp`.
4. If the pass needs a new render target, add it to `engine/SceneRenderer`
   alongside `SceneColor` and `LightAccum`.
5. **The vcxproj must list new shader files** if you want them to appear in
   the Solution Explorer, but compilation is at runtime so missing them from
   the vcxproj does not break the build - they only need to be in the output
   directory.

### A new render layer
1. Add the value to `enum class RenderLayer` in `engine/EngineTypes.h`.
2. Update the layer order comment in this file's "Render Layers" section.
3. Sprites submitted to the new layer will be drawn between the existing
   layers based on enum order.

### A new UI element type
1. Create `ui/UIYourElement.h` next to `UIPanel.h`/`UILabel.h`.
2. Inherit from `ui::UIElement`. Implement `Render()` (and `OnChar()` if it
   accepts text input).
3. Add a factory method on `UISystem` (e.g. `CreateYourElement(...)`).
4. Include the header from `ui/UI.h`.
5. Update the vcxproj.

### A new editor tool / context menu page
1. Add a value to `EditorContextMenuPage` in `editor/EditorMode.h`.
2. Implement the page in the appropriate menu-build function in
   `editor/EditorMode.cpp` (search for existing pages).
3. Add hotkey bindings to `EditorState::shortcutBindings` and the
   `editor.ini` template if you want a default key.

### Procedural test scene contents
Edit `game/GenerateTestScene.cpp`. Anything you add there appears in
`scenes/test.vmp` on next dev-mode startup.

## Rendering Pipeline

The renderer uses Direct3D 12 with a multi-pass architecture orchestrated by
`SceneRenderer::RenderFrame()`:

1. **Background pass** -> SceneColor RT: Renders single background image (PNG)
   and/or paged DDS background tiles as instanced sprites.
2. **Base scene pass** -> SceneColor RT: Renders all game sprites (ground tiles,
   walls, actors, roofs) as instanced quads, Y-sorted within layers.
3. **Lighting pass** -> LightAccum RT (half-res R11G11B10_FLOAT): For each point
   light, renders stencil shadow volumes from occluder edges, then renders
   radial falloff quad with stencil test (additive blend).
4. **Composite pass** -> Backbuffer: Fullscreen triangle combines
   SceneColor * fog-of-war darkening + LightAccum * visibility.
5. **UI pass** -> Backbuffer: Screen-space panels/labels/dropdowns as instanced
   sprites via the same sprite PSO with an orthographic projection.
6. **Debug overlays** (optional) -> Backbuffer: Grid lines and wall edge
   highlights via line-draw PSO.

### Render Layers (draw order)
`BackgroundPages` -> `GroundTiles` -> `WallsProps` -> `Actors` -> `Roofs` ->
`RoofActors` -> `ScreenSpaceIcons` -> `FogOverlay`

### Root Signatures
- **Main** (sprite, shadow, grid): CBV b0 (FrameConstants) + descriptor table
  (1024 SRVs at t0) + static sampler s0 (linear clamp)
- **Light**: CBV b0 (FrameConstants) + CBV b1 (LightConstants)
- **Composite**: CBV b0 (FrameConstants) + descriptor table (4 SRVs:
  SceneColor, LightAccum, FogVis, FogExp) + static samplers s0 (point), s1 (linear)

### Sprite Rendering
Sprites are rendered as instanced triangle strips (4 vertices per quad).
Per-instance data (`SpriteInstance`, 64 bytes) is uploaded each frame from the
CPU ring buffer. The vertex shader generates quad corners from `SV_VertexID`
and applies rotation. The pixel shader does bindless texture lookup:
`textures[instanceTexIndex].Sample()`.

### GPU Resource Management
- **Descriptor heaps:** One persistent shader-visible SRV heap (1024 slots,
  free-list), one RTV heap, one DSV heap. Per-frame linear allocators for
  transient descriptors.
- **Upload ring buffer:** Single persistent upload heap (`UploadManager`).
  Frame markers track reuse safety via fence values.
- **Double buffering:** 2 frames in flight (`kMaxFramesInFlight = 2`),
  fence-based CPU/GPU sync.

## Scene File Format (.vmp)

Binary format with magic `"VMP1"` (version 1). Layout:

```
[SceneHeader]                    magic, version, grid dims, tile size, origin, scene name
[uint32 tileCount]               followed by MapTile * count
[uint32 bgPageCount]             followed by SceneBackgroundPage * count
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

Strings are serialized as `[uint32 length][char * length]`. Fixed-size char
arrays (names, tags) are written raw. The `SceneFile` class handles all
serialization. `GenerateTestScene` creates a test `.vmp` procedurally.

**Format compatibility:** The `.vmp` binary format has no backward
compatibility - any change to `SceneData` structures or serialization order
requires regenerating all scene files. With `g_devMode = true` (and not
`--editor`), the test scene regenerates automatically. Manual edits made via
the editor are not portable across format changes.

## Background Image System

Scenes can specify a background image via `SceneData::backgroundImagePath`
(relative to exe directory, e.g. `"assets\\hangar\\hangar.png"`). At runtime,
`BackgroundPager::SetBackgroundImage()` loads the PNG via
`Texture2D::LoadFromPNG()` (using Windows Imaging Component / WIC) and maps it
to cover the entire grid world area. The image is rendered as a single sprite
in the background pass before any paged DDS tiles.

## Game Loop (vampire.cpp)

`wWinMain` -> `InitInstance` -> `InitEngine` -> real-time `PeekMessage` loop ->
`RenderFrame()` (or `EditorFrame()` if `--editor`) each idle frame.

### InitEngine flow
1. Create D3D12 renderer
2. Create SceneRenderer with shader directory
3. Set up camera at origin
4. Generate test scene if needed (always in dev mode unless `--editor`)
5. Load `.vmp` scene -> populate Grid, OccluderSet, LightSystem, FogRenderer,
   RoofSystem, Camera
6. Queue background image load if scene specifies one
7. Initialize InputSystem (WASD moves player, RMB drags camera, scroll zooms)
8. Initialize UISystem and either gameplay HUD or editor overlay

### RenderFrame flow (gameplay mode)
1. Compute delta time (high-res timer, clamped to 100ms)
2. Process input -> update camera, player movement (WASD at 150 units/sec),
   player facing (toward mouse)
3. Handle tile clicks -> `SceneLoader::InspectTile()` -> `OutputDebugStringA`
4. Toggle debug overlays (F2 = walls, F3 = grid)
5. `BeginFrame` -> create player indicator texture (first frame) ->
   load background image (first frame)
6. Submit player indicator to render queue
7. Update fog visibility, roofs, background pager
8. `SceneRenderer::RenderFrame()` -> all render passes
9. UI pass via `UISystem::Render`
10. Draw debug overlays if enabled
11. `EndFrame` -> present

### EditorFrame flow (editor mode)
Lives in `editor/EditorMode.cpp`. Same outline as `RenderFrame` but with
selection handling, context-menu spawning, dropdown filtering, and Ctrl+S save
in place of normal gameplay input.

### Player Indicator
A 32x32 procedural red triangle texture generated on first frame. Submitted as
a sprite in the `Actors` layer. Rotation tracks the mouse cursor via `atan2`.

## RPG System

### Attributes (1-12)
STR, AGI, END, PER, INT, CHA. Derived: HP = 8+END+floor(STR/2),
AP/turn = 6+floor(AGI/2), Vision = 6+floor(PER/2) tiles.

### Skill Checks
All checks: `3d6 <= (Attribute + SkillRank + modifiers)`. 26 skills across
Combat, Thief, Social, and Vampire Discipline categories.

### Combat
Turn-based with Action Points. Directional cover (half: -2, full: -4).
Features: overwatch, suppression, burst fire, wound effects (Bleeding,
Stunned, Crippled, Pinned). See `fighting.md` for the full doctrine and
balance reasoning.

### Vampire Disciplines
8 disciplines powered by Blood Reserve (max 6 + Blood Potency): Blink,
Telekinesis, Hemocraft, Blood Mark, Auspex, Domination, Obfuscate, Celerity.

### Factions
Player, 5 vampire clans (Nosferatu, Tremere, Brujah, Ventrue, Malkavian),
Police, Mercenary, Civilian.

## Working With These Files (For Agents)

These rules exist so agents do not waste tokens regenerating large files.

1. **Write directly to the project path on the first attempt.** When asked to
   change a file in `d:\src\vampire\`, generate the content with the final
   destination path on the very first tool call. Do not first write to a
   sandbox/scratch path and then re-emit the same content to the project
   path - that doubles the token cost.
2. **Prefer `Filesystem:edit_file` over `Filesystem:write_file` for changes
   to existing files.** `edit_file` takes a list of `{oldText, newText}`
   pairs and only emits the changed regions. Use `write_file` only when
   creating a new file or when the rewrite is so extensive that nearly
   every line changes.
3. **For multi-section edits, batch them into one `edit_file` call** with
   multiple entries in the `edits` array, rather than one call per section.
4. **Read before editing.** `edit_file` requires exact-match `oldText`.
   Read the relevant range with `view` or `read_text_file` first to get the
   current text verbatim, including whitespace.
5. **Do not stage outputs in `/home/claude/` or `/mnt/user-data/outputs/`
   when the user has filesystem write access to the project.** Those paths
   are for files the user will download through the chat interface. For
   project files, write to `d:\src\vampire\...` directly.
6. **`present_files` is for sharing files via the chat UI, not for
   delivering project edits.** If the user has filesystem write access,
   skip `present_files` entirely.

## Coding Conventions

- **C++14 only.** No `std::optional`, `std::string_view`, structured bindings,
  `if constexpr`, or other C++17+ features.
- **`(std::min)` / `(std::max)` with parentheses** to avoid Windows macro
  conflicts.
- Header-only classes are common in `engine/` for simple systems (Grid,
  Camera2D, InputSystem, RenderQueue, LightSystem, OccluderSet, RoofSystem).
- Engine types use `DirectX::XMFLOAT2/4` for GPU-facing data, plain `float`
  for CPU logic.
- No third-party libraries. PNG loading uses WIC. DDS loading is hand-rolled.
  Shaders are compiled at runtime via `D3DCompileFromFile`.
- Win32 API for windowing, timing (`QueryPerformanceCounter`), file I/O.
- `OutputDebugStringA` for all debug logging (visible in VS Output window).
  There is no logging framework. Tag messages with a prefix like `[Vampire]`
  or `[Editor]` for filtering.
- Game data types use fixed-size char arrays (e.g. `char tag[32]`,
  `char sceneName[64]`) for binary serialization, `std::string` for
  variable-length data.
- Master include headers: `engine/Engine.h`, `game/GameSystems.h`, `ui/UI.h`.
- Global engine state lives as `static` variables in `vampire.cpp` (no
  singleton pattern).
- ASCII only in source files - no em-dashes, smart quotes, or other non-ASCII
  punctuation. (This file follows that rule too; if you see mojibake, the
  file's encoding is wrong.)

## Key Constants (EngineTypes.h)

| Constant | Value | Purpose |
|---|---|---|
| `kMaxFramesInFlight` | 2 | Double-buffered rendering |
| `kMaxLights` | 32 | Maximum dynamic point lights |
| `kMaxSpriteInstances` | 4096 | Max sprites per frame |
| `kMaxPersistentSRVs` | 1024 | SRV heap size / bindless texture limit |
| `kPageSizePixels` | 512 | Background page tile size |

## Important Warnings

- The `.vmp` binary format has no backward compatibility. Any change to
  `SceneData` structures or serialization order requires regenerating all
  scene files. Dev mode handles this automatically for the test scene.
- Shaders are compiled from source HLSL files at startup
  (`D3DCompileFromFile`). They must be present in `$(OutDir)shaders/`. If
  shaders fail to compile, the application will fail to initialize. Check the
  Output window for compile errors.
- The upload ring buffer has a fixed size. Very large textures (e.g. 8K+
  background images) may exceed the ring buffer capacity and fail to upload.
- `Texture2D::LoadFromPNG` calls `CoInitializeEx` if COM is not already
  initialized. This is safe for single-threaded use but should be noted.
- Adding a new `.h`/`.cpp` requires updating `vampire.vcxproj` AND
  `vampire.vcxproj.filters`. The post-build step copies output but does not
  add files to the project.

## Grid System (Isometric Hex)

The `Grid` class (`engine/Grid.h`) supports two layouts toggled by
`SetIsometric(bool)`:

- **Rectangular** (`m_isometric = false`): square tiles, simple `worldX/Y =
  origin + tile*tileSize`. Used by some test paths.
- **Isometric flat-top hex** (`m_isometric = true`): the gameplay default.
  The internal data model is still a rectangular array of tiles - only the
  world-space projection is hex-on-isometric.

The internal data model (tile grid, NPCs, items, etc.) is always a rectangular
array; only world-space projection differs.

### Isometric Hex Layout
- Logical hexes are flat-top, offset-column layout (odd columns are shifted
  down by half a hex height).
- Logical hex coordinates are projected to world space by the linear isometric
  transform `(x, y) -> (x - y, (x + y) / 2)`. This makes the hex appear as a
  squashed Fallout-style hex footprint rather than a top-down hex.
- `tileSize` is chosen so the projected hex width equals `tileSize` and the
  projected hex height equals `tileSize / 2`. The internal logical hex radius
  is `tileSize / (1 + sqrt(3))`.
- `(originX, originY)` = top-left corner of the grid's projected bounding box.
- `GetWorldWidth()` / `GetWorldHeight()` return the projected bounding box
  size (computed once in `UpdateDerivedMetrics`).

### Tile <-> World Transforms
- `TileToWorld(x, y)` -> projected center of the hex at logical (x, y).
- `WorldToTile(wx, wy)` -> the hex that contains the world-space point (uses
  point-in-hex test against all hexes; falls back to nearest center if no
  hex contains the point).
- `TileHexVertices(x, y, out[6])` -> 6 projected hex corners in clockwise order.
- `TileDiamondVertices(x, y, top, right, bottom, left)` -> legacy 4-vertex
  axis-aligned bounding diamond (computes hex bounds, does not return a true
  diamond). Use `TileHexVertices` for new code.

### Hex Neighbors
`Grid::HexNeighbor(x, y, edgeIndex 0..5, &nx, &ny)` returns the neighbor
across the given hex edge. The offset table differs for even and odd columns
(this is a flat-top offset-column layout).

### Occluders
`OccluderSet::BuildFromTileGridIsometric()` generates hex edge segments using
`TileHexVertices()`. Each hex edge between a wall tile and a non-wall tile
becomes an occluder segment for stencil shadow volumes and LoS.

## Running and Testing

There is no automated test suite, no asserts framework, and no headless mode.
Verification is manual:

- **Smoke test:** Build, run `vampire.exe`. The test scene should load, the
  camera should center on tile (15, 15), the red triangle player indicator
  should be visible, and `OutputDebugString` should print scene stats. If any
  of those fail, check the VS Output window for shader-compile or
  asset-missing errors.
- **Editor smoke test:** Run with `--editor`. LMB on a tile should pop a
  context menu. Ctrl+S should save (watch the Output window for the save
  confirmation).
- **After scene-format changes:** Verify dev-mode regen works (delete
  `x64/Debug/scenes/test.vmp` first if uncertain), then run normally and
  inspect a few tiles via LMB to confirm the new fields print correctly.
- **After combat/RPG changes:** No in-game combat loop exists yet (see
  `fighting.md` for the design). Add a temporary harness in `vampire.cpp` or
  a dedicated test file with `int main()` if you need to exercise
  `CombatSystem` directly.
- **GPU validation:** Build in Debug; the D3D12 debug layer is enabled and
  any resource-state or root-signature mismatches will print to the Output
  window.

### Debug Hotkeys

| Key | Action |
|-----|--------|
| **F2** | Toggle wall edge overlay (hex edges in isometric) |
| **F3** | Toggle grid overlay (hex wireframes) |
| **WASD** | Move player character (gameplay mode) / pan camera (editor mode) |
| **Mouse wheel** | Zoom camera |
| **RMB drag** | Pan camera |
| **LMB click** | Inspect tile (gameplay) / select tile (editor) |
| **Ctrl+S** | Save scene (editor mode only) |
| **Esc** | Cancel current selection (editor mode only) |
