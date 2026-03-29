# Contributing to OpenSWBF

Thank you for your interest in contributing. This document covers how to get started, the code style we follow, and where help is most needed.

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/<your-username>/openswbf.git
   cd openswbf
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/description-of-change
   ```
4. **Build and test** your changes (see README.md for build instructions).
5. **Submit a pull request** against the `main` branch with a clear description of what you changed and why.

## Code Style

### Language

- C++17. Do not use C++20 features -- the Emscripten toolchain may lag behind.
- All source files use the `swbf` namespace (nested namespaces for modules: `swbf::assets`, `swbf::renderer`, etc.).

### Formatting

- 4-space indentation. No tabs.
- Opening braces on the same line for functions, classes, and control flow.
- `#pragma once` for header guards (no `#ifndef` guards).
- Headers use `.h` extension, source files use `.cpp`.

### Naming

| Element | Convention | Example |
|---------|-----------|---------|
| Classes / Structs | PascalCase | `TerrainChunk` |
| Functions / Methods | camelCase | `loadLevel()` |
| Member variables | m_ prefix + camelCase | `m_chunkSize` |
| Local variables | camelCase | `vertexCount` |
| Constants | k prefix + PascalCase | `kMaxPlayers` |
| Enums | PascalCase type, PascalCase values | `enum class RenderPass { Opaque, Transparent }` |
| Namespaces | lowercase | `swbf::renderer` |

### General

- Prefer value semantics and `std::unique_ptr` over raw `new`/`delete`.
- Use `std::span`, `std::string_view`, and references for non-owning access.
- Keep includes minimal. Forward-declare where possible.
- No exceptions in hot paths. Use return codes or `std::optional` for expected failures.
- Document non-obvious decisions with brief comments explaining *why*, not *what*.

## Architecture

The codebase is split into modules under `src/`. Each module is a static library with its own `CMakeLists.txt`.

```
core        Low-level utilities (logging, math, file I/O, memory)
assets      File format parsers (UCFB, .lvl, .msh, .ter, ODF)
renderer    Graphics abstraction (OpenGL / WebGL), drawing
audio       Sound playback (OpenAL / Web Audio)
physics     Collision, raycasting
input       Keyboard, mouse, gamepad
scripting   Lua runtime and game script bindings
network     Multiplayer networking (future)
platform    SDL2 / Emscripten platform layer
game        Game state, conquest logic, command posts
```

### Dependency Rules

Dependencies flow downward. Lower-level modules must not depend on higher-level ones.

```
game
  |-- scripting, renderer, audio, physics, input, network
        |-- assets, platform
              |-- core
```

- `core` depends on nothing (except the standard library and GLM).
- `assets` depends on `core`.
- `renderer`, `audio`, `physics`, `input` depend on `core` and `assets`.
- `game` depends on everything above.

Do not introduce circular dependencies between modules.

## Areas Needing Help

Contributions are welcome across all areas. The following list is roughly ordered by current priority:

### Phase 0 -- Foundation (current)
- UCFB container parser and .lvl file loader
- Terrain (.ter) parsing and rendering
- Sky dome rendering
- Free-fly debug camera
- WebGL context setup and render loop

### Phase 1 -- Static World
- Mesh (.msh) parsing and rendering
- Texture loading (DDS, TGA from .lvl chunks)
- World lighting and fog
- Water plane rendering
- Prop and foliage placement

### Phase 2 -- Entities
- ODF (Object Definition File) parser
- Entity spawn system
- Skeletal animation playback
- Vehicle physics
- Lua 4.0 compatibility shim for mission scripts

### Phase 3 -- Gameplay
- Weapon systems (projectiles, spread, damage)
- Health, stamina, and respawn
- Command post capture logic
- Conquest game mode
- Bot AI and pathfinding

### Phase 4 -- Multiplayer
- WebRTC data channel networking
- Lobby and matchmaking
- State synchronization
- Latency compensation

## Reporting Issues

When filing a bug report, please include:
- Steps to reproduce
- Expected vs. actual behavior
- Build configuration (native or WASM, compiler, OS)
- Relevant log output

## Legal Note

All contributions must be original work or properly attributed under a compatible license. Do not submit code derived from decompilation or disassembly of the original game. This is a clean-room project -- all format knowledge comes from the modding community's publicly available documentation.

By submitting a pull request, you agree to license your contribution under the project's GPL-3.0 license.
