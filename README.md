# Lightpath

Lightpath is a standalone C++17 light-graph engine extracted from [MeshLED](https://github.com/kasparsj/meshled).
It builds topology, runs animation/runtime state, and produces per-pixel RGB output.

## API Layout

Lightpath provides two header tiers:

- Stable installable API:
  - `lightpath/lightpath.hpp`
  - `lightpath/engine.hpp`
  - `lightpath/types.hpp`
  - `lightpath/status.hpp`
  - `lightpath/version.hpp`
- Source-integration module headers (for in-repo integrations like [MeshLED](https://github.com/kasparsj/meshled)):
  - `lightpath/integration.hpp`
  - `lightpath/integration/topology.hpp`
  - `lightpath/integration/runtime.hpp`
  - `lightpath/integration/rendering.hpp`
  - `lightpath/integration/objects.hpp`
  - `lightpath/integration/factory.hpp`
  - `lightpath/integration/debug.hpp`

The stable install/export package installs only the stable API headers.

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
- `static-analysis`: compile commands + benchmark target for analysis tooling
- `docs`: Doxygen docs target

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Static Analysis

Lightweight static analysis helpers are included:

```bash
./scripts/check-clang-format.sh
cmake -S . -B build/static-analysis -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLIGHTPATH_CORE_BUILD_TESTS=OFF -DLIGHTPATH_CORE_BUILD_EXAMPLES=OFF -DLIGHTPATH_CORE_BUILD_BENCHMARKS=ON
./scripts/run-clang-tidy.sh build/static-analysis
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
- `LIGHTPATH_CORE_BUILD_BENCHMARKS` (default: `OFF`)
- `LIGHTPATH_CORE_BUILD_DOCS` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_ASAN` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_UBSAN` (default: `OFF`)
- `LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` (default: `OFF`)

## Benchmarks

Micro-benchmark target:

```bash
cmake -S . -B build-bench -DLIGHTPATH_CORE_BUILD_BENCHMARKS=ON -DLIGHTPATH_CORE_BUILD_TESTS=OFF -DLIGHTPATH_CORE_BUILD_EXAMPLES=OFF
cmake --build build-bench --parallel
./build-bench/lightpath_core_benchmark
```

## Package Distribution

In addition to CMake install/export:

- Conan recipe: `conanfile.py`
- vcpkg overlay templates: `packaging/vcpkg/`

## Source Layout

- `include/lightpath/` stable facade + source-integration module headers
- `src/topology/` graph objects and routing
- `src/runtime/` state update and animation
- `src/rendering/` palette/blend implementation
- `src/objects/` built-in topology definitions
- `src/debug/` debug helpers
- `src/core/` shared constants/types/platform macros

## Docs

- API reference: `docs/API.md`
- API compatibility policy: `docs/API_POLICY.md`
- Packaging guide: `docs/PACKAGING.md`
- Migration notes: `MIGRATION.md`
- Changelog: `CHANGELOG.md`

### Generated API Reference (GitHub Pages)

The repository publishes Doxygen API docs to GitHub Pages through GitHub Actions.

Local generation:

```bash
doxygen Doxyfile
```

Generated HTML output:

- `build/docs/html/index.html`
