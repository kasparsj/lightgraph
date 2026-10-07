# Build And Integration

## API Layout

Lightgraph provides two header tiers:

- Stable installable API:
  - `lightgraph/lightgraph.hpp`
  - `lightgraph/engine.hpp`
  - `lightgraph/types.hpp`
  - `lightgraph/status.hpp`
  - `lightgraph/version.hpp`
- Build-only source-integration headers (for in-repo integrations like [MeshLED](https://github.com/kasparsj/meshled)):
  - `lightgraph/integration.hpp`
  - `lightgraph/integration/topology.hpp`
  - `lightgraph/integration/runtime.hpp`
  - `lightgraph/integration/rendering.hpp`
  - `lightgraph/integration/objects.hpp`
  - `lightgraph/integration/factory.hpp`
  - `lightgraph/integration/debug.hpp`
  - `lightgraph/integration/drawing.hpp`
  - `lightgraph/integration/geometry.hpp`
  - `lightgraph/integration/drawing_codec.hpp`
  - `lightgraph/integration/drawing_session.hpp`

There is also a build-only internal namespace for shared non-stable types used by
in-repo adapters and tests:

- `lightgraph/internal/*.h`
- `lightgraph/internal/*.hpp`
- `lightgraph/internal/runtime/*`
- `lightgraph/internal/topology/*`

The stable install/export package installs only the stable API headers under
`lightgraph/*.hpp`. Neither `lightgraph/integration/*` nor `lightgraph/internal/*`
is installed.

Source-integration headers are intentionally exposed only through the separate
CMake target `lightgraph::integration`. These headers are self-contained and use
installed-style include paths; consumers should not add `src/` as a public include
root or include Lightgraph implementation headers directly.

Drawing scene/rendering state and geometry providers are owned by core source
modules. `lightgraph/integration/drawing.hpp` and `geometry.hpp` re-export those
types for source consumers. Protocol adapters should keep wire parsing outside
the renderer, use `DrawingSceneCodec` for the binary scene format, and bind one
`DrawingSession` to `state.drawing()` for image producer/chunk handling and a
merged rendering/transfer status.

`DrawingSession` owns transfer metadata only. Image assembly leases the inactive
one of the drawing runtime's two image slots and submits it atomically, so an
adapter must keep the session alive until its queued handle is applied or
released. Serialize session calls with drawing, topology, and frame rendering.

### MeshLED HTTP firmware prerequisite

The full MeshLED HTTP drawing profile requires an Arduino WebServer library
variant that exposes the raw request-body API used by the adapter, including
`HTTPRaw` and `RequestHandler::canRaw()`. The standard pinned framework
package `3.20006` (Arduino core 2.0.6) does not provide that API. The separately
installed `3.20017` package (Arduino core 2.0.17) provides the raw interface.
No framework version change is included in this refactor.

Select or patch an SDK with that raw API before building the full HTTP profile.
The OSC-only profile does not use this HTTP request path. Passing the Lightgraph
host/CMake suites or the OSC-only firmware build therefore does not establish
that the default full HTTP profile builds against the standard pinned SDK.

## Build And Test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CMake Integration

### `add_subdirectory`

```cmake
add_subdirectory(external/lightgraph)
target_link_libraries(your_target PRIVATE lightgraph::lightgraph)
```

### `add_subdirectory` (source integration layer)

```cmake
add_subdirectory(external/lightgraph)
target_link_libraries(your_target PRIVATE lightgraph::integration)
```

Use `lightgraph::integration` only for source-tree integrations that need
internal topology/runtime headers (`lightgraph/integration*.hpp`).

### Install + `find_package`

Install:

```bash
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix /path/to/install
```

Consume:

```cmake
find_package(lightgraph CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE lightgraph::lightgraph)
```

## Build Options

- `LIGHTGRAPH_CORE_BUILD_TESTS` (default: `ON`)
- `LIGHTGRAPH_CORE_BUILD_EXAMPLES` (default: `ON`)
- `LIGHTGRAPH_CORE_BUILD_BENCHMARKS` (default: `OFF`)
- `LIGHTGRAPH_CORE_BUILD_DOCS` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_ASAN` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_UBSAN` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_COVERAGE` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_FRACTIONAL_RENDERING` (default: `ON`)

Non-CMake integrations can disable the same feature by defining
`LIGHTGRAPH_FRACTIONAL_RENDERING=0` when compiling Lightgraph sources.

The CI matrix builds and tests GCC, Clang, Apple Clang, and MSVC. The warnings
preset treats supported compiler warnings as errors; ASAN and UBSAN remain
Clang/GCC-only profiles.

## Package Distribution

In addition to CMake install/export:

- Conan recipe: `conanfile.py`
- vcpkg overlay templates: `packaging/vcpkg/`

### Firmware candidate publication

HTTP imports and persisted topology loading use the same candidate construction
and publication service, also available in OSC-only builds. Install firmware
memory admission and allocation telemetry before constructing `State`; publish
only when `ready()` succeeds. Failed replacement preserves the old runtime,
topology, settings, generation, and debug resources. Successful publication
establishes a fresh drawing generation and retires the previous runtime before
rebuilding optional debug resources. If persisted loading fails, retain the ready
default runtime. Initial startup with no ready runtime retains restart handling.
