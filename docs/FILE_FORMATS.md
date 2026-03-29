# SWBF File Format Reference

This document provides a brief overview of the file formats used by Star Wars: Battlefront (2004). All information here comes from publicly available community documentation.

## UCFB Container Format

UCFB ("Unnamed Chunked File Binary") is the container format used by nearly every SWBF data file. It is a recursive chunk-based binary format.

Each chunk consists of:
- **FourCC** (4 bytes) -- chunk type identifier (e.g., `ucfb`, `lvl_`, `tex_`)
- **Size** (4 bytes, little-endian) -- size of the chunk data (excluding the 8-byte header)
- **Data** (variable) -- chunk payload, which may contain nested chunks

Chunks are padded to 4-byte alignment. The parser reads chunks sequentially and recurses into container-type chunks.

## .lvl Files

Level files (`.lvl`) are UCFB containers that bundle all assets for a map or game module: textures, meshes, terrain, scripts, sound banks, localization strings, and object definitions.

The game loads multiple `.lvl` files at startup:
- `core.lvl` -- shared assets (UI, common textures, HUD elements)
- `shell.lvl` -- main menu
- `<mapname>/<mapname>.lvl` -- per-map assets
- `ingame.lvl` -- in-game HUD and common gameplay assets

Key top-level chunk types within `.lvl` files:
| Chunk FourCC | Content |
|-------------|---------|
| `tex_` | Texture (DDS data with metadata) |
| `skel` | Skeleton hierarchy |
| `modl` | Model (references mesh + skeleton) |
| `Lght` | Light definitions |
| `tern` | Terrain data |
| `scr_` | Lua script bytecode |
| `loc_` | Localization strings |
| `plan` | AI planning data |
| `PATH` | Pathfinding nodes |

## .msh Mesh Format

Mesh files define 3D geometry and are embedded within `.lvl` containers (not standalone files in the shipped game). The format stores:

- Vertex positions, normals, and UV coordinates
- Vertex colors and weights (for skinned meshes)
- Triangle strip or triangle list index data
- Material references (texture names, render flags)
- Segment/LOD hierarchy
- Collision primitives (bounding boxes, spheres, cylinders)

Meshes support multiple segments (sub-meshes) with independent materials, and up to 4 LOD levels.

## .ter Terrain Format

Terrain data is stored in the `tern` chunk within a map's `.lvl` file. It describes the ground surface as a heightmap grid with:

- Grid dimensions and cell size
- Height values (16-bit per grid point)
- Texture layers (up to 16) with blend weights per cell
- Terrain "cuts" (holes for building interiors, tunnels)
- Water plane height and extent

The terrain renderer splits the grid into chunks for frustum culling and LOD selection.

## ODF -- Object Definition Files

ODF files are text-based configuration files that define game entities (soldiers, vehicles, weapons, command posts, props). In the shipped game, they are compiled into binary chunks within `.lvl` files.

An ODF entry specifies:
- Base class (e.g., `soldier`, `hover`, `prop`, `commandpost`)
- Class hierarchy via `ClassParent` references
- Properties as key-value pairs (health, speed, weapon loadout, model references)
- Attached effects, sounds, and hardpoints

Example structure:
```
[GameObjectClass]
ClassLabel = hover
ClassParent = aatstank_base

[Properties]
MaxHealth = 3000.0
MaxSpeed = 12.0
GeometryName = "aat_geo"
```

## Community Resources

The following community projects and documents were invaluable references for understanding these formats:

- **ze_filetypes** -- PrismaticFlower's comprehensive SWBF file format documentation:
  https://github.com/PrismaticFlower/SWBF-unmunge/wiki

- **SWBF-unmunge** -- Tool for extracting and converting assets from .lvl files:
  https://github.com/PrismaticFlower/SWBF-unmunge

- **LibSWBF2** -- Open-source C++ library for reading SWBF2 file formats (shares lineage with SWBF1):
  https://github.com/Ben1138/LibSWBF2

- **Gametoast Modding Wiki** -- Community-maintained documentation on formats, scripting, and modding:
  https://gametoast.com/

- **Schlechtwetterfront's Blender tools** -- Import/export plugins with format parsing code:
  https://github.com/Schlechtwetterfront/swbf-unmunge-blender
