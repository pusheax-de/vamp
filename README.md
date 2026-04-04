# Vampire -- 2D Turn-Based RPG

A city-based vampire RPG with a GURPS-inspired rule system, built in C++14 as a native Win32/Direct3D 12 application. No third-party libraries -- rendering, UI, font rasterization, and asset loading are all built from scratch.

## Premise

You play as a human who gets infected by a vampire in a modern city. Dive into the underground metro tunnels, navigate vampire clan politics, fight police and mercenaries, and master blood disciplines -- all in a turn-based, cover-based tactical system.

## Current State

The project has a working **custom D3D12 rendering engine**, a complete set of **RPG game systems**, and an **in-game scene editor**. The rendering pipeline supports isometric tile maps, dynamic point lights with stencil shadow volumes, fog-of-war, roof fade, and a batched UI overlay. A procedurally generated 32x32 test scene exercises all engine and data systems.

### What Works
- Direct3D 12 renderer with multi-pass pipeline (sprites, shadow volumes, radial lights, fog composite)
- 2:1 diamond isometric tile grid with world/screen coordinate transforms
- Binary scene format (`.vmp`) with full serialization of tiles, NPCs, items, lights, triggers, transitions, shops, safehouses, territories
- Procedural test scene generation (32x32 map with walls, NPCs, items, lights, patrol routes)
- Player character movement (WASD) with mouse-facing indicator
- Camera controls (MMB/RMB drag pan, scroll zoom)
- Fog-of-war with CPU raycasting against occluder edges
- Dynamic point lights with flickering, shadow casting via stencil volumes
- Roof system with fade-on-enter when player enters a building
- Background image loading (PNG via WIC) mapped to world-space grid
- Tile click inspection with full debug output (terrain, NPCs, items, triggers, etc.)
- Complete RPG character sheet (attributes, 26 skills, equipment, status effects)
- Combat system with AP, directional cover, overwatch, suppression, wound effects
- 8 vampire disciplines powered by Blood Reserve
- Social system (persuasion, deception, intimidation, empathy, streetwise)
- Sleep/safehouse system with ambush mechanics
- Runtime bitmap font atlas generation from Windows GDI
- Immediate-mode UI system with panels, labels, and searchable dropdowns
- **In-game editor** (`--editor` flag) with tile painting, item placement, object placement, and Ctrl+S save
- Editor dropdown selectors with search/filter for terrain and item types

### What's Not Done Yet
- Actual gameplay loop (turns, enemy AI, win/lose conditions)
- Sprite art and animations (currently procedural placeholder textures)
- Dialogue system UI (data structures exist, no in-game display)
- Inventory UI and equipment management
- Sound and music
- Multiple scenes / scene transitions at runtime
- Save/load game state

## RPG System Overview

### Core Attributes (1-12)
| Attribute | Governs |
|-----------|---------|
| **STR** | Melee damage, carry, grapples |
| **AGI** | Accuracy, dodge, stealth, AP scaling |
| **END** | HP, resist bleed/poison, sprint |
| **PER** | Vision range, overwatch, detecting hidden enemies |
| **INT** | Hacking, crafting, medicine, ritual complexity |
| **CHA** | Dialogue, prices, leadership, mental resistance |

### Derived Stats
- **HP** = 8 + END + floor(STR/2)
- **AP per turn** = 6 + floor(AGI/2)
- **Initiative** = AGI + PER (tie-break 1d6)
- **Vision radius** = 6 + floor(PER/2) tiles

### Skill Check
All checks: **3d6 <= (Attribute + SkillRank + modifiers)**

### Skills (26 total, ranked 0-5)
- **Combat:** Firearms, Melee, Athletics, Tactics, Explosives, Medicine
- **Thief:** Stealth, Lockpicking, Pickpocket, Hacking, Traps, Disguise
- **Social:** Talking, Persuasion, Deception, Intimidation, Empathy, Streetwise
- **Vampire Disciplines:** Blink, Telekinesis, Hemocraft, Blood Mark, Auspex, Domination, Obfuscate, Celerity

### Combat
- Turn-based with Action Points
- Directional cover (half: -2, full: -4 to attacker)
- Overwatch, suppression, burst fire
- Wound effects: Bleeding, Stunned, Crippled, Pinned

### Vampire Disciplines
Powered by **Blood Reserve** (max 6 + Blood Potency), replenished by sleep or risky feeding.

| Discipline | Effect |
|------------|--------|
| **Blink** | Teleport up to 3-6 tiles |
| **Telekinesis** | Push/pull enemies, disarm weapons |
| **Hemocraft** | Blood armor, blood spike, seal doors |
| **Blood Mark** | Track target through fog-of-war + debuff |
| **Auspex** | Reveal pulse, pierce stealth and obfuscation |
| **Domination** | Stun or command weak-willed humans |
| **Obfuscate** | Supernatural invisibility |
| **Celerity** | Spend BR to gain bonus AP |

### Vampire Clans
| Clan | Specialty |
|------|-----------|
| **Nosferatu** | Obfuscate, Auspex -- metro dwellers, masters of stealth |
| **Tremere** | Hemocraft, Blood Mark -- blood sorcerers |
| **Brujah** | Celerity, Blink -- street fighters, anarchists |
| **Ventrue** | Domination, Telekinesis -- aristocrats, mental control |
| **Malkavian** | Auspex, Domination -- seers and madmen |

### Sleep & Safehouses
Sleep at limited safehouses to fully restore Blood Reserve. Each has a security rating, access cost, and territory alignment. Ambush chance = base 10% + heat - security.

## Rendering Pipeline

The renderer uses Direct3D 12 with a multi-pass architecture:

1. **Background pass** (SceneColor RT): Single background image (PNG) and/or paged DDS tiles as instanced sprites.
2. **Base scene pass** (SceneColor RT): All game sprites (ground, walls, actors, roofs) as instanced quads, Y-sorted within layers.
3. **Lighting pass** (LightAccum RT, half-res R11G11B10_FLOAT): Stencil shadow volumes from occluder edges, then radial falloff quads with stencil test (additive blend). Up to 32 dynamic point lights.
4. **Composite pass** (Backbuffer): Fullscreen triangle combines scene color with fog darkening and light accumulation.
5. **UI pass** (Backbuffer): Screen-space quad batching (panels, text, dropdowns) via orthographic sprite PSO.
6. **Debug overlays** (optional, Backbuffer): Grid diamond wireframes and wall edge highlights.

Sprites are rendered as instanced triangle strips (4 vertices per quad, 64-byte `SpriteInstance`). The vertex shader generates corners from `SV_VertexID`; the pixel shader does bindless texture lookup via `textures[instanceTexIndex].Sample()` with a 1024-slot SRV heap.

## Scene Editor

Launch with `--editor` command line flag. The editor provides:

- **LMB** selects tiles for terrain painting (Shift+LMB to multi-select)
- **RMB** selects tiles for item/object placement
- **Searchable dropdown menus** appear on selection for choosing terrain types or items (type to filter)
- **Keyboard hotkeys** still work alongside dropdowns (F=Floor, L=Wall, etc.)
- **Ctrl+S** saves the scene to the `.vmp` file
- **ESC** cancels the current selection
- Grid, wall edges, and selection diamond outlines are always visible
- Object textures loaded on demand with placeholder fallback

### Editor Terrain Types
Floor, Street, Rubble, Water, Wall, Door, Metro Track, Shadow

### Editor Item Types
Hangar (object), Bandage, Stimpack, Antidote, Flashbang, Blood Vial, Delete

## Project Structure

```
vampire.sln / vampire.vcxproj   Solution and project files
vampire.cpp                      Win32 entry point, game loop, engine wiring
framework.h                      Precompiled Windows includes

engine/                          Custom 2D rendering engine (D3D12)
  Engine.h                       Master include for all engine headers
  EngineTypes.h                  Shared types: SpriteInstance, RenderLayer, PointLight2D
  RendererD3D12.h / .cpp         Core D3D12 device, swap chain, command lists, frame sync
  DescriptorAllocator.h / .cpp   Persistent free-list and per-frame bump descriptor allocators
  UploadManager.h / .cpp         Ring-buffer upload heap for CPU-to-GPU transfers
  Texture2D.h / .cpp             GPU textures: DDS, PNG (WIC), and procedural RGBA creation
  RenderTarget.h / .cpp          Render target and depth/stencil wrappers
  PipelineStates.h / .cpp        All root signatures and PSOs
  Camera2D.h                     Orthographic camera with world/screen transforms
  Grid.h                         Isometric tile-grid coordinate system
  InputSystem.h                  Keyboard/mouse input, camera control, tile picking, WM_CHAR
  BackgroundPager.h / .cpp       Paged DDS background streaming + single PNG background
  RenderQueue.h                  Sortable sprite queue with layer-based draw order
  SceneRenderer.h / .cpp         Multi-pass renderer orchestrating all render passes
  LightSystem.h                  Dynamic point lights + shadow quad generation
  OccluderSet.h                  Wall edge segments for LoS / shadow casting
  FogRenderer.h / .cpp           CPU fog-of-war raycasting + GPU texture upload
  RoofSystem.h                   Roof footprints with fade-on-enter logic
  SpriteAtlas.h                  Sprite atlas stub (not yet implemented)

  shaders/                       HLSL shaders (compiled at runtime, SM 5.1)
    SpriteVS.hlsl                Instanced sprite vertex shader
    SpritePS.hlsl                Bindless texture sampling pixel shader
    SpritePointPS.hlsl           Point-filtered sprite pixel shader
    ShadowVolumeVS.hlsl          Stencil shadow volume pass
    LightRadialVS/PS.hlsl        Radial light with stencil test
    CompositeVS/PS.hlsl          Final scene + fog + light compositing
    GridOverlayVS/PS.hlsl        Debug grid and wall line overlays

game/                            RPG systems and game logic
  GameSystems.h                  Master include for all game headers
  SceneData.h                    Complete scene data structures for .vmp format
  SceneFile.h / .cpp             Binary serialization of .vmp scene files
  SceneLoader.h / .cpp           Loads scenes into engine systems, tile inspection API
  GenerateTestScene.h / .cpp     Procedural 32x32 test scene with NPCs, items, lights
  MapTile.h                      Tile: terrain, directional cover, LoS, movement cost
  Character.h / .cpp             Full character sheet with attributes, skills, inventory
  Attributes.h                   6 core attributes + derived stats
  SkillDefs.h                    26 skill IDs with categories and metadata
  Skill.h / .cpp                 Skill checks (3d6 <= attr+rank+mods), opposed rolls
  Dice.h / .cpp                  3d6 rolls, random utilities
  Weapon.h                       14 weapon types data table
  Armor.h                        6 armor types data table
  Inventory.h / .cpp             Equipment slots, backpack, consumables, money
  StatusEffect.h / .cpp          Status effects: Bleed, Stun, Pinned, Crippled, etc.
  CoverSystem.h / .cpp           Directional cover queries, Bresenham LoS, flanking
  GameWorld.h / .cpp             Grid map, territories, clans, heat system
  FogOfWar.h / .cpp              Game-level vision and fog-of-war mechanics
  CombatSystem.h / .cpp          Turn-based combat with AP
  Discipline.h / .cpp            8 vampire blood disciplines
  SocialSystem.h / .cpp          Dialogue, persuasion, deception, empathy
  SleepSystem.h / .cpp           Safehouses, sleep, ambush, feeding

editor/                          In-game scene editor
  EditorMode.h / .cpp            Tile painting, item/object placement, dropdown UI, save

ui/                              Immediate-mode UI system
  UI.h                           Master include header
  UITypes.h                      Core types: Rect, Color, Anchor, TextAlign
  BitmapFont.h / .cpp            Runtime font atlas from Windows GDI
  UIElement.h                    Base element with tree layout and hit testing
  UIPanel.h                      Solid-color panel with optional border
  UILabel.h                      Text element using BitmapFont
  UIDropdown.h                   Searchable dropdown selector with filter textbox
  UIRenderer.h / .cpp            Screen-space quad batching via sprite PSO
  UISystem.h / .cpp              Root UI manager, element creation, char forwarding

assets/                          Game assets (copied to output by post-build)
  hangar/hangar.png              Test background image
```

## Building

Open `vampire.sln` in Visual Studio 2022 and build (Debug/x64). Requires:
- Visual Studio 2022 with C++ desktop workload (v143 toolset)
- Windows SDK 10.0
- No external dependencies -- all libraries are linked via `#pragma comment(lib)` (d3d12, dxgi, d3dcompiler, windowscodecs)

The post-build step copies `engine/shaders/` and `assets/` to the output directory. Shaders are compiled from HLSL source at runtime.

### Running
- **Game mode:** Run `vampire.exe` normally
- **Editor mode:** Run with `--editor` command line flag

## Debug Hotkeys (Game Mode)

| Key | Action |
|-----|--------|
| **WASD / Arrows** | Move player character |
| **Mouse wheel** | Zoom camera |
| **RMB drag** | Pan camera |
| **LMB click** | Inspect tile (debug output) |
| **F2** | Toggle wall edge overlay |
| **F3** | Toggle grid overlay |

## License

Private project -- all rights reserved.
