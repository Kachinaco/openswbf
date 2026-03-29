# Building OpenSWBF

## Prerequisites

| Tool              | Minimum Version | Notes                                      |
|-------------------|-----------------|--------------------------------------------|
| CMake             | 3.20            |                                            |
| C++17 compiler    | GCC 9+ / Clang 10+ | MSVC 2019+ should work but is untested |
| SDL2 (dev)        |                 | Native builds only                         |
| OpenAL Soft (dev) |                 | Native builds only                         |
| Emscripten SDK    | 3.1.56          | WASM builds only                           |
| Python 3          |                 | For the local dev server                   |

## Native build (Linux)

```bash
# Install system libraries
sudo apt-get install -y libsdl2-dev libopenal-dev

# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/app/openswbf/openswbf
```

### Optional CMake flags

| Flag                          | Default | Description                       |
|-------------------------------|---------|-----------------------------------|
| `OPENSWBF_BUILD_TESTS`       | OFF     | Build unit and integration tests  |
| `OPENSWBF_BUILD_TOOLS`       | OFF     | Build asset viewer and lvl_dump   |
| `OPENSWBF_WARNINGS_AS_ERRORS`| OFF     | Treat compiler warnings as errors |

## WebAssembly build (Emscripten)

```bash
# Install and activate Emscripten (one-time)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install 3.1.56
./emsdk activate 3.1.56
source ./emsdk_env.sh
cd ..

# Configure
emcmake cmake -B build-wasm -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build-wasm -j$(nproc)
```

The WASM artifacts land in `build-wasm/app/openswbf/`:

- `openswbf.html` -- Emscripten shell page
- `openswbf.js` -- JS glue code
- `openswbf.wasm` -- the WebAssembly binary
- `openswbf.data` -- preloaded filesystem (if any)

## Running locally

After building the WASM target, serve the output directory over HTTP:

```bash
cd build-wasm/app/openswbf/
python3 -m http.server 8080
```

Then open `http://localhost:8080/openswbf.html` in your browser.

### COOP / COEP headers for SharedArrayBuffer

If you later enable threading (`-sPTHREAD_POOL_SIZE`, `-sUSE_PTHREADS`), browsers
require Cross-Origin-Opener-Policy and Cross-Origin-Embedder-Policy headers.
Python's built-in server does not set these. Use the included service worker
(`web/sw.js`) or a custom server:

```bash
# Option A: service worker (no server changes needed)
# The web/ directory includes sw.js which patches response headers in the browser.

# Option B: custom Python server with COOP/COEP headers
python3 -c "
import http.server, functools

class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

http.server.HTTPServer(('', 8080), Handler).serve_forever()
"
```

## macOS

```bash
brew install sdl2 openal-soft cmake
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(sysctl -n hw.logicalcpu)
```

## Game data

OpenSWBF does **not** ship any copyrighted assets. You need a legitimate copy of
Star Wars: Battlefront (2004). Place (or symlink) the game's `GameData/` directory
so the engine can find `.lvl` files at runtime:

```bash
ln -s /path/to/SWBF/GameData data
```

The `data/` directory is in `.gitignore` and will never be committed.
