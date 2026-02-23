# Lightpath

Lightpath is a standalone C++ light-graph engine extracted from MeshLED.
It models LED topology (intersections/connections), runs animation state updates,
and produces deterministic per-pixel color output.

## Highlights

- Topology graph engine (`LPObject`, `Intersection`, `Connection`, `Model`)
- Runtime animation/state system (`State`, `LightList`, `LPLight`)
- Palette + blend-mode rendering (`Palette`, built-in palettes, blend operators)
- Public namespaced API surface under `include/lightpath/`
- Module-aligned internal sources under `src/`

## Build and Test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTPATH_CORE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Quickstart (Public API)

```cpp
#include <lightpath/lightpath.hpp>

int main() {
    auto object = lightpath::makeObject(lightpath::BuiltinObjectType::Line);
    lightpath::Engine engine(std::move(object));

    lightpath::EmitParams params(0, 1.0f, 0x33CC99);
    params.setLength(6);
    engine.state().emit(params);

    lightpath::millis() += 16;
    engine.update(lightpath::millis());

    lightpath::Color p0 = engine.state().getPixel(0);
    return (p0.R || p0.G || p0.B) ? 0 : 1;
}
```

A complete compiling sample is available at `examples/minimal_usage.cpp`.

## Core Concepts

- Topology:
  - `LPObject` owns grouped `Intersection` and `Connection` graphs plus `Model` weights.
  - Built-in shapes are available via `lightpath::makeObject(...)`.
- Runtime State:
  - `State` owns layered `LightList` instances and computes frame-by-frame pixel output.
  - `emit(...)` adds or reuses active light lists.
- Rendering:
  - `Palette` controls color interpolation and wrap behavior.
  - Blend modes combine active layers into final pixel color.

## CMake Integration

### Option 1: `add_subdirectory`

```cmake
add_subdirectory(external/lightpath)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

### Option 2: `FetchContent`

```cmake
include(FetchContent)
FetchContent_Declare(
  lightpath
  GIT_REPOSITORY https://github.com/kasparsj/lightpath.git
  GIT_TAG main
)
FetchContent_MakeAvailable(lightpath)

target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

## Public Headers

- `lightpath/lightpath.hpp` (umbrella)
- `lightpath/types.hpp`
- `lightpath/topology.hpp`
- `lightpath/runtime.hpp`
- `lightpath/rendering.hpp`
- `lightpath/objects.hpp`
- `lightpath/factory.hpp`
- `lightpath/debug.hpp`

## Source Layout

`src/` is now organized by module to mirror the public API:

- `src/topology/` graph and routing primitives (`LPObject`, `Intersection`, `Connection`, `Model`, `Port`)
- `src/runtime/` animation/state machinery (`State`, `LightList`, `LPLight`, `EmitParams`, `Behaviour`)
- `src/rendering/` palette and blend support (`Palette`, built-in palettes)
- `src/debug/` debugger helpers (`LPDebugger`)
- `src/objects/` built-in topology definitions
- `src/core/` shared non-platform primitives (`Types.h`, `Limits.h`) and platform adapter macros (`Platform.h`)

## Compatibility

Flat compatibility headers at `src/*.h` were removed. If you include internals
directly, use module paths (`src/topology/...`, `src/runtime/...`, etc.).

`LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` controls whether `src/` is exported
as a public include directory for transitional builds that include module
headers via `topology/...`, `runtime/...`, `rendering/...`, `debug/...`.

Source-level API break: command parameter APIs are now value-based:

- `LPObject::getParams(char)` -> `std::optional<EmitParams>`
- `LPObject::getModelParams(int)` -> `EmitParams` (by value)

This removes manual `new`/`delete` ownership patterns for command dispatch.

Public namespace cleanup:

- Canonical names are now `lightpath::Object`, `lightpath::RuntimeState`, `lightpath::RuntimeLight`, `lightpath::Debugger`.
- `lightpath::LP*` compatibility aliases were removed from public headers.

## Additional Docs

- API reference: `docs/API.md`
- Migration notes: `MIGRATION.md`
- Audit report: `docs/CORE_AUDIT_REPORT.md`

## Build Profiles

Address/undefined sanitizers and strict warnings are supported:

```bash
CC=clang CXX=clang++ cmake -S . -B build-asan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_ASAN=ON
CC=clang CXX=clang++ cmake -S . -B build-ubsan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_UBSAN=ON
CC=clang CXX=clang++ cmake -S . -B build-warnings -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_STRICT_WARNINGS=ON
```
