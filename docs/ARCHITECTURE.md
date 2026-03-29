# Architecture Overview

## Module dependency graph

```
                        +-------------+
                        |    game     |  Game logic, modes, AI, HUD
                        +------+------+
                               |
          +----------+---------+---------+----------+
          |          |         |         |          |
    +-----v--+ +----v---+ +--v-----+ +-v-------+ +v--------+
    |renderer| | audio  | |physics | |scripting| | input   |
    +-----+--+ +----+---+ +--+-----+ +-+-------+ ++--------+
          |          |         |         |          |
          +----------+---------+---------+----------+
                               |
                        +------v------+
                        |   assets    |  UCFB chunks, ODF, LVL, terrain
                        +------+------+
                               |
                        +------v------+
                        |    core     |  Types, logging, CRC, config, memory, math
                        +------+------+
                               |
                        +------v------+
                        |  platform   |  SDL2 / Emscripten abstraction
                        +-------------+

    Arrows point from dependent -> dependency.
    All modules link openswbf_core.
    The app/openswbf executable links everything.
```

## Key design decisions

### Language: C++17

C++17 gives us `std::optional`, `std::variant`, `std::string_view`, structured
bindings, and `if constexpr` -- enough modern tooling without requiring C++20
module support (which Emscripten lags on).

### Graphics: OpenGL ES 3.0 / WebGL 2

The original game used DirectX 9. We target OpenGL ES 3.0, which maps directly
to WebGL 2 on the browser and runs natively on Linux/macOS via SDL2. This means:

- No compute shaders (ES 3.0 limitation) -- use CPU fallbacks.
- Instanced rendering available (good for foliage, clones, droids).
- MRT (multiple render targets) for deferred lighting if needed later.

### Physics: Bullet Physics (planned)

SWBF's physics are simple (AABB terrain collision, ragdolls, vehicles). Bullet
compiles to WASM and provides sufficient fidelity.

### Audio: OpenAL

Emscripten ships an OpenAL port backed by Web Audio API. On native platforms we
use OpenAL Soft. This gives a single audio API across all targets.

### Scripting: Lua 5.4 with 4.0 compatibility shim

The original game used Lua 4.0 for mission scripts. We embed Lua 5.4 (smaller,
faster, compiles to WASM) and will provide a compatibility layer that exposes
the Lua 4.0 globals and calling conventions the original scripts expect.

### Asset pipeline

SWBF ships assets in `.lvl` files using the UCFB chunked binary format. The
`assets` module reads these at runtime:

- `ucfb/chunk_reader` -- generic chunk parsing
- `odf/odf_parser` -- Object Definition Files
- `lvl/lvl_loader` -- level container loader
- `terrain/terrain_data` -- heightmap and texture layers

### Third-party libraries

| Library | Purpose               | Source         |
|---------|-----------------------|----------------|
| GLM     | Vector/matrix math    | FetchContent   |
| Lua 5.4 | Scripting runtime     | FetchContent   |
| stb     | Image/audio decoding  | FetchContent   |
| SDL2    | Windowing, input, GL  | System / Emscripten port |
| OpenAL  | 3D audio              | System / Emscripten port |
| Bullet  | Physics (planned)     | FetchContent   |

## Phased roadmap

### Phase 1 -- Foundation (current)

- Project skeleton with CMake build system
- Core utilities (logging, CRC, config, memory)
- UCFB chunk reader and ODF parser
- OpenGL ES 3.0 context creation (native + WASM)
- CI pipelines (native Linux + WASM)

### Phase 2 -- Asset loading and rendering

- LVL file loader (meshes, textures, terrain)
- Terrain renderer with texture splatting
- Static mesh rendering
- Sky dome / skybox
- Camera system (free-fly, then third-person)

### Phase 3 -- Gameplay prototype

- Entity system with ODF-driven spawning
- Input mapping (keyboard/mouse + gamepad)
- Basic soldier movement and animation
- Weapon firing and projectile physics
- Lua mission scripting with 4.0 compat shim

### Phase 4 -- Multiplayer and polish

- Networked multiplayer (WebRTC data channels for browser)
- Command posts and ticket system
- Vehicle physics and mounting
- HUD and menu screens
- Audio: ambient, effects, music

## Further reading

- `docs/BUILDING.md` -- build instructions for native and WASM targets
- `cmake/EmscriptenToolchain.cmake` -- Emscripten-specific compiler/linker flags
- `src/assets/ucfb/chunk_types.h` -- UCFB chunk type definitions
