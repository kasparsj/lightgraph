# Lightpath

Lightpath is a standalone C++17 light-graph engine extracted from MeshLED.
It builds topology, runs animation/runtime state, and produces per-pixel RGB output.

## API Layout

Lightpath currently ships two API layers:

- Stable high-level API (`include/lightpath/`):
  - `lightpath/lightpath.hpp`
  - `lightpath/engine.hpp`
  - `lightpath/types.hpp`
  - `lightpath/status.hpp`
- Legacy compatibility API (`include/lightpath/legacy.hpp`, `include/lightpath/legacy/*.hpp`):
  - Exposes historical topology/runtime/rendering types used by MeshLED integrations.
  - Backed by internal headers in `src/`.

## Build and Test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CI Presets

`CMakePresets.json` includes CI-friendly profiles:

- `default`: normal build + tests + example
- `warnings`: warnings as errors
- `asan`: AddressSanitizer
- `ubsan`: UndefinedBehaviorSanitizer

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Quickstart (Stable API)

```cpp
#include <lightpath/lightpath.hpp>

int main() {
    lightpath::EngineConfig config;
    config.object_type = lightpath::ObjectType::Line;
    config.pixel_count = 64;

    lightpath::Engine engine(config);

    lightpath::EmitCommand emit;
    emit.model = 0;
    emit.length = 8;
    emit.speed = 1.0f;
    emit.color = 0x33CC99;

    const auto emitted = engine.emit(emit);
    if (!emitted) {
        return 1;
    }

    engine.tick(16);

    const auto p0 = engine.pixel(0);
    if (!p0) {
        return 1;
    }

    const lightpath::Color color = p0.value();
    return (color.r || color.g || color.b) ? 0 : 1;
}
```

A compiling example is provided in `examples/minimal_usage.cpp`.

## CMake Integration

### `add_subdirectory`

```cmake
add_subdirectory(external/lightpath)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

### Install + `find_package`

Install:

```bash
cmake -S . -B build -DLIGHTPATH_CORE_BUILD_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix /path/to/install
```

Consume:

```cmake
find_package(lightpath CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

## Build Options

- `LIGHTPATH_CORE_BUILD_TESTS` (default: `ON`)
- `LIGHTPATH_CORE_BUILD_EXAMPLES` (default: `ON`)
- `LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_ASAN` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_UBSAN` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` (default: `OFF`)
- `LIGHTPATH_CORE_INSTALL_LEGACY_HEADERS` (default: `OFF`)

## Source Layout

- `include/lightpath/` stable public API
- `include/lightpath/legacy/` compatibility surface
- `src/topology/` graph objects and routing
- `src/runtime/` state update and animation
- `src/rendering/` palette/blend implementation
- `src/objects/` built-in topology definitions
- `src/debug/` debug helpers
- `src/core/` shared constants/types/platform macros

## Docs

- API reference: `docs/API.md`
- Migration notes: `MIGRATION.md`
- Changelog: `CHANGELOG.md`
