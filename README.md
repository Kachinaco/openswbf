# OpenSWBF

Open-source reimplementation of Star Wars: Battlefront (2004) targeting WebAssembly.

<!-- Badges -->
![Build Status](https://img.shields.io/github/actions/workflow/status/Kachinaco/openswbf/ci.yml?branch=main)
![License](https://img.shields.io/github/license/Kachinaco/openswbf)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue)

<!-- Screenshot placeholder — replace with actual screenshot when available -->
<!-- ![OpenSWBF Screenshot](docs/screenshot.png) -->

---

## What is this?

OpenSWBF is a clean-room reimplementation of the game engine behind Star Wars: Battlefront (2004). The goal is to let players run the original game in a modern web browser via WebAssembly, with no plugins, no emulation layers, and no proprietary code.

This project does not include any copyrighted game assets. Players supply their own legally-obtained copy of Star Wars: Battlefront (2004). OpenSWBF reads the original game files at runtime to reconstruct the experience.

## How it works

1. **Clean-room engine** -- The renderer, audio system, physics, scripting runtime, and game logic are being written from scratch in C++17 based on publicly available documentation of the original file formats.
2. **Asset loading** -- The engine parses SWBF's proprietary `.lvl` container files (UCFB format), extracting meshes, textures, terrain, scripts, and object definitions at runtime.
3. **WebAssembly target** -- The codebase compiles to both native (SDL2 + OpenGL) for development and WASM (Emscripten + WebGL) for browser deployment. The browser build serves a single HTML page that loads the WASM module and renders via WebGL.
4. **Lua scripting** -- The original game used Lua 4.0 mission scripts. OpenSWBF embeds Lua 5.4 with a compatibility shim to execute original mission scripts unmodified.

## Current Status

The project is in **Phase 0 -- Foundation**. Core infrastructure is being built.

- [x] CMake build system (native + Emscripten)
- [x] Module architecture (core, assets, renderer, audio, physics, input, scripting, platform, game)
- [ ] UCFB / .lvl file parser
- [ ] Window creation and WebGL context
- [ ] Sky dome rendering
- [ ] Terrain rendering (.ter)
- [ ] Free-fly camera

## Roadmap

| Phase | Name | Description |
|-------|------|-------------|
| 0 | Foundation | Build system, window, sky dome, terrain, camera |
| 1 | Static World | Full world loading, meshes, textures, lighting, water |
| 2 | Entities | Object spawning, animations, vehicles, Lua scripting |
| 3 | Gameplay | Weapons, health, command posts, conquest mode, AI |
| 4 | Multiplayer | WebRTC networking, lobby, full online play |

## Building

### Prerequisites

- CMake 3.20+
- A C++17 compiler (GCC 10+, Clang 12+, or MSVC 2019+)
- SDL2 and OpenAL development libraries (native builds only)
- Emscripten SDK (WASM builds only)

### Native (development)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
./build/app/openswbf/openswbf
```

### WebAssembly

```bash
source /path/to/emsdk/emsdk_env.sh
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j$(nproc)
# Serve web/ directory and open in browser
```

### Build options

| Option | Default | Description |
|--------|---------|-------------|
| `OPENSWBF_BUILD_TESTS` | OFF | Build unit and integration tests |
| `OPENSWBF_BUILD_TOOLS` | OFF | Build asset viewer and lvl_dump tools |

## Project Structure

```
openswbf/
  CMakeLists.txt          Root build configuration
  app/
    openswbf/             Main executable entry point
    asset_viewer/         Debug tool: browse loaded assets
    lvl_dump/             Debug tool: dump .lvl contents
  src/
    core/                 Logging, math, memory, file I/O
    assets/               UCFB parser, .lvl loader, .msh/.ter readers
    renderer/             WebGL/OpenGL abstraction, sky, terrain, mesh drawing
    audio/                OpenAL / Web Audio playback
    physics/              Collision detection, raycasting
    input/                Keyboard, mouse, gamepad abstraction
    scripting/            Lua 5.4 runtime with Lua 4.0 compat shim
    network/              WebRTC multiplayer (future)
    platform/             SDL2 / Emscripten platform layer
    game/                 Game state, command posts, conquest logic
  web/
    index.html            Browser shell page for WASM build
  tests/                  Unit and integration tests
  docs/                   File format references, design notes
  cmake/                  CMake helper modules
  libs/                   Third-party vendored headers (glm, stb, imgui)
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on code style, architecture, and how to submit patches.

## Legal

**This is a clean-room reimplementation.** No original game code or assets are included in this repository. No reverse engineering of executable code was performed. All format knowledge comes from the SWBF modding community's publicly available documentation.

Users must own a legitimate copy of Star Wars: Battlefront (2004) to supply the game files required at runtime.

Star Wars and all related marks are trademarks of Lucasfilm Ltd. and The Walt Disney Company. This project is not affiliated with, endorsed by, or sponsored by Lucasfilm, Disney, or Pandemic Studios.

## Credits

This project would not be possible without decades of work by the SWBF modding community:

- **SWBFSpy** -- Keeping multiplayer alive long after official servers shut down
- **PrismaticFlower** -- Extensive file format documentation and modding tools
- **LibSWBF2 / Ben1138** -- Open-source .lvl parsing library that informed format understanding
- **Schlechtwetterfront** -- Blender tools, format research, and community knowledge sharing
- **Gametoast** -- The central hub for SWBF modding knowledge and community discussion

## License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
