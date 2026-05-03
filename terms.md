# Terminology

This document is the source of truth for naming used throughout the engine,
editor, and game code. When code disagrees with this document, **the document
wins** — fix the code.

The terminology is organized from the data layer (logical, on-disk, in-memory)
down to the rendering layer (what the GPU draws each frame).

---

## 1. World structure

### Grid
A 2D array of cells. The grid is fixed-size per scene (`gridWidth × gridHeight`),
laid out as a flat-top hexagonal grid in isometric mode (the default for
gameplay maps) or a square grid in rectangular mode. Owned by `engine::Grid`.

### Tile
**A single cell of the grid.** "Tile" and "hex" refer to the same thing — one
addressable unit of the grid identified by `(tileX, tileY)`. We use **tile**
in code and prose; "hex" appears only when emphasizing the geometric shape
(e.g. `TileHexVertices`).

Tile data is stored in `vamp::MapTile` (one entry per grid cell). It carries:
- `TerrainType terrain` — the terrain category (see below)
- `CoverLevel` per direction
- `blocksLoS`, `blocksMove`, `isShadow`, `isLocked`, `moveCost`, `elevation`

### TerrainType
The category a tile falls into. Affects movement cost, line-of-sight, stealth,
and which **ground texture** the tile is painted with.

Values: `Floor`, `Street`, `Rubble`, `Water`, `Wall`, `Door`, `MetroTrack`,
`Shadow`.

> **Naming caveat:** `TerrainType::Wall` (a terrain category — a tile painted
> with `assets/floor/wall.png`) is **not the same thing** as a placed-object
> wall sprite (e.g. `assets/wall/wall_vert.png`, a `SceneObject`). The former
> is a property of a `MapTile`; the latter is an entry in
> `SceneData::objects`. Don't conflate them.

### Ground
The flat layer at the bottom of every scene. Every tile **is** ground —
there is no such thing as a "non-ground tile". The visible representation
of the ground is built from two render layers (see §4):

- **TileColorFill** — flat color underlay per tile, sized to the tile's
  bounding box. Acts as a backstop so that transparent corners of the
  textured ground sprite don't reveal whatever was behind.
- **GroundTextures** — the textured PNG per tile (`floor.png`, `wall.png`,
  `door.png`, etc., from `assets/floor/`), keyed by `TerrainType`.

> "Ground" is a *layer* concept; "tile" is a *cell* concept. One ground
> layer is composed of `gridWidth × gridHeight` tiles' worth of fills and
> textures.

### Grid lines
Thin world-space line segments drawn between tiles in the editor as a
visual aid. Generated procedurally per frame from the grid. Drawn between
TileColorFill and GroundTextures so that ground textures and placed objects
naturally cover them. Not a `RenderLayer` enum value — passed as a separate
`underlayGrid` parameter to `SceneRenderer::RenderFrame`.

---

## 2. Things that sit on tiles

### Placed object (`SceneObject`)
Free-standing art placed on a bounding box of tiles `(x0,y0)..(x1,y1)`.
Stored in `SceneData::objects`. Each object references a PNG by `imagePath`
and has a `placement` value (`YMin`, `YMiddle`, `YMax`) that controls where
on the tile bounding box the sprite anchors vertically.

Examples: walls (`assets/wall/wall_vert.png`), buildings (`assets/hangar/`),
props, fixtures.

> **A placed-object wall is not a wall-terrain tile.** A wall placed as a
> `SceneObject` is a sprite drawn on top of whatever ground texture the
> underlying tile has. A wall as `TerrainType::Wall` is a property baked
> into the tile itself.

### Ground item (`SceneGroundItem`)
A pickup that sits on a single tile. Carries an `ItemType` (`Weapon`,
`Consumable`, `Ammo`, `Armor`, `KeyItem`, `Misc`), a `templateId` into the
relevant item table, and a `quantity`. Player can walk over and pick up.

Stored in `SceneData::groundItems`. Currently not rendered as a sprite
(only the editor's debug overlay shows them); future work may submit them
to a dedicated render layer.

### Quest item (`SceneQuestItem`)
A specialized ground item with quest metadata (`questId`, `collected`).
Stored in `SceneData::questItems`.

### NPC (`SceneNPC`)
A character placed in the scene with behavior (`Idle`, `Patrol`, `Guard`,
`Merchant`, `QuestGiver`, `Civilian`).

### Light (`SceneLight`)
A point light placed at a tile (or free world position). Has `radius`,
`intensity`, `intensityLevel`, color, optional flicker. Editor draws a
small marker sprite at each light position; gameplay renders the actual
light into the LightAccum buffer.

### Roof (`SceneRoof`)
A bounding box of tiles forming a building footprint. Draws an opaque roof
sprite that fades out when the player enters the box, revealing the
interior. Managed by `engine::RoofSystem`.

### Trigger / Transition / Shop / Safehouse / Territory
Non-rendering metadata zones. See `SceneData.h`.

---

## 3. Background

### Background image
A single PNG covering the entire scene's world bounds. Loaded via
`BackgroundPager::SetBackgroundImage`. Drawn beneath everything else.

### Background page
A streamed, paged background tile (DDS) for very large scenes that don't
fit a single image. The pager keeps an LRU GPU cache (`kMaxCachedPages`
slots). Background pages and the background image are mutually
non-exclusive (both can render). Both feed the **BackgroundPages** render
layer (see §4).

---

## 4. Render layers

`engine::RenderLayer` defines draw order from background to foreground.
The numeric depth values come from `DepthForLayer` in `SceneRenderer.cpp`.
Higher depth = farther back; lower depth = closer to camera (D3D
`LESS_EQUAL` test).

| Layer              | Depth | Carries                                   |
|--------------------|-------|-------------------------------------------|
| `BackgroundPages`  | 0.95  | Background image + paged background pages |
| `TileColorFill`    | 0.92  | Per-tile flat-color hex sprites           |
| `GridLines`        | 0.88  | World-space grid line overlay (not used by `RenderQueue`; passed via `underlayGrid` parameter) |
| `GroundTextures`   | 0.80  | Per-tile textured PNG (terrain art)       |
| `PlacedObjects`    | 0.60  | `SceneObject` sprites (walls, props, etc.) |
| `Actors`           | 0.45  | Characters, NPCs (Y-sorted)               |
| `Roofs`            | 0.30  | Roof sprites                              |
| `RoofActors`       | 0.20  | Characters on top of roofs                |
| `ScreenSpaceIcons` | 0.00  | Health bars, selection, quest markers     |
| `FogOverlay`       | 0.10  | Fog-of-war darkening (post-composite)     |

> **Renames applied** (from the older terminology):
> - `GroundTiles` → `GroundTextures`. The old name was redundant ("tiles
>   are ground"); the new name describes what the layer actually
>   carries — the textured sprite per tile, distinct from the
>   `TileColorFill` underlay.
> - `WallsProps` → `PlacedObjects`. The old name implied the layer was
>   for walls or props specifically; in fact every `SceneObject` (and
>   the editor's light markers) goes here regardless of archetype.

---

## 5. Pipeline states

`engine::PipelineStates` owns every PSO. Each render layer / pass uses one.

| PSO                       | Blend                          | Used by                                                                                  |
|---------------------------|--------------------------------|------------------------------------------------------------------------------------------|
| `SpritePSO`               | Alpha (`SRC_ALPHA / INV_SRC_ALPHA`) | `BackgroundPages`, `GroundTextures`, `Actors`, `Roofs`, `RoofActors`                  |
| `SpriteCutoutPSO`         | None (opaque)                  | `TileColorFill`, `PlacedObjects`. Pixels with `alpha < 0.001` are clipped in the PS. |
| `SpriteScreenPSO`         | Alpha                          | UI sprites on the backbuffer (linear UNORM target, not sRGB)                          |
| `SpriteScreenPointPSO`    | Alpha + point sampling         | UI text glyphs                                                                            |
| `ShadowVolumePSO`         | Stencil-only, no color write   | Shadow volume extrusion in `PassLighting`                                                 |
| `LightRadialPSO`          | Additive (`ONE / ONE`)         | Per-light radial gradient into LightAccum, gated by stencil                               |
| `CompositePSO`            | None (overwrites backbuffer)   | Fullscreen composite of SceneColor + LightAccum + Fog → backbuffer                        |
| `GridOverlayPSO`          | Alpha, line topology           | Backbuffer-targeted line overlays (selection rectangles, debug lines)                     |
| `GridOverlayScenePSO`     | Alpha, line topology           | SceneColor-targeted grid line underlay (drawn inside `PassBaseScene`)                     |

---

## 6. Render targets

| Target           | Format                  | Resolution | Notes                                              |
|------------------|-------------------------|------------|----------------------------------------------------|
| **SceneColor**   | `R8G8B8A8_UNORM_SRGB`   | Full       | Receives BG, ground, placed objects, actors, roofs. Cleared to `(0,0,0,0)`. |
| **LightAccum**   | `R11G11B10_FLOAT`       | Half       | Additive accumulator for point lights.             |
| **DepthStencil** | `D24_UNORM_S8_UINT`     | Full       | Depth for layer ordering; stencil for shadows.     |
| **FogVisible**   | `R8_UNORM`              | Quarter    | Currently visible mask.                            |
| **FogExplored**  | `R8_UNORM`              | Quarter    | Persistent explored mask.                          |
| **Backbuffer**   | `R8G8B8A8_UNORM`        | Window     | Final present target.                              |

---

## 7. Per-entity rendering details

### Tile color fill
- **Submitted by:** `EditorSubmitTileColorFills` (editor only — gameplay does not currently submit fills)
- **Layer:** `TileColorFill`
- **PSO:** `SpriteCutoutPSO` (opaque)
- **Texture:** 1×1 white pixel, tinted via `SpriteInstance::color`
- **Sizing:** quad covers the tile's bounding box (from `TileDiamondVertices`)
- **Purpose:** color backstop so transparent corners of ground textures don't reveal a black SceneColor clear

### Grid lines
- **Submitted by:** `EditorFrame` via the `underlayGrid` parameter to `SceneRenderer::RenderFrame`
- **Layer:** drawn at the `GridLines` slot in `PassBaseScene` (between `TileColorFill` and `GroundTextures`)
- **PSO:** `GridOverlayScenePSO` (line topology, alpha-blended)
- **Geometry:** per-tile hex edges (isometric) or row/column lines (rectangular)
- **Purpose:** editor-only visual aid; gameplay does not pass an `underlayGrid`

### Ground textures
- **Submitted by:** `EditorSubmitGroundTextures` (editor) — gameplay does not currently submit per-tile textured ground
- **Layer:** `GroundTextures`
- **PSO:** `SpritePSO` (alpha-blended)
- **Texture:** the PNG keyed by the tile's `TerrainType` (e.g. `assets/floor/wall.png` for `TerrainType::Wall`)
- **Sizing:** scaled to fit the tile's bounding box (currently capped to native texture size — see open issues)
- **Caveats:** texture art is hex-shaped with transparent corners. Without the `TileColorFill` underlay, the corners reveal whatever is behind in SceneColor. Per-tile depth is offset by hex 3-coloring to avoid z-fighting at sprite edges where adjacent tiles' bounding boxes overlap.

### Placed objects
- **Submitted by:** `EditorSubmitObjects` (editor); gameplay submission TBD
- **Layer:** `PlacedObjects`
- **PSO:** `SpriteCutoutPSO` (opaque, with `clip(alpha)` for hard edges)
- **Texture:** `SceneObject::imagePath` PNG
- **Sizing:** width = tile bounding-box span; height = preserves texture aspect ratio
- **Anchor:** `YMin` (top of bbox), `YMiddle` (center), or `YMax` (bottom of bbox)

### Light markers (editor only)
- **Submitted by:** `EditorSubmitLightMarkers`
- **Layer:** `PlacedObjects` (shares the layer with real objects; this is intentional — they need to draw above ground but below actors)
- **PSO:** `SpriteCutoutPSO`
- **Texture:** procedurally generated 16×16 yellow-dot sprite
- **Purpose:** debug visual showing where a `SceneLight` is placed

### Actors / NPCs
- **Layer:** `Actors`
- **PSO:** `SpritePSO` (alpha-blended)
- **Sort:** Y-sorted within the layer so closer actors draw on top
- **Submission:** `vampire.cpp` (gameplay) submits the player; editor has no actor submission

### Roofs
- **Layer:** `Roofs`
- **PSO:** `SpritePSO`
- **Alpha:** driven by `RoofSystem` based on whether player is inside the roof's footprint (fades out when inside)

### Lights (radial pass)
- **Layer:** none — drawn into LightAccum in `PassLighting`, not into SceneColor
- **PSO:** `LightRadialPSO` (additive blend), gated by stencil from `ShadowVolumePSO`
- **Composite:** added to SceneColor in `PassComposite`

### Fog of war
- **Layer:** none — applied during composite
- **Targets:** `FogVisible`, `FogExplored` (R8 textures)
- **Composite:** `lerp(fogColor, sceneColor, visibleFactor)` then darken by ambient

### UI / screen-space sprites
- **Layer:** `ScreenSpaceIcons`
- **PSO:** `SpriteScreenPSO` (or `SpriteScreenPointPSO` for text)
- **Coordinates:** screen-space (not world-space), drawn after composite

---

## 8. Glossary of misleading-but-currently-used terms

| Term in code        | What it actually means                                              |
|---------------------|---------------------------------------------------------------------|
| `WallsProps`        | **Renamed to `PlacedObjects`.** Old name suggested walls/props specifically; was used for any `SceneObject`. |
| `GroundTiles`       | **Renamed to `GroundTextures`.** Old name was redundant; new name distinguishes the textured layer from `TileColorFill`. |
| `wall` (image stem) | When in `assets/floor/wall.png` → a *terrain texture* for `TerrainType::Wall`. When in `assets/wall/wall_vert.png` → a *placed-object archetype* (a `SceneObject`). The file paths disambiguate. |
| `Diamond vertices`  | The AABB of a hex (top, right, bottom, left of bounding box). Despite the name, it's not a true diamond — it's the bounding rectangle expressed as four cardinal points. |
