# Lightgraph

Lightgraph is a standalone C++17 light-graph engine extracted from [MeshLED](https://github.com/kasparsj/meshled).
It builds topology, runs animation/runtime state, and produces per-pixel RGB output.

## API Layout

Lightgraph provides two header tiers:

- Stable installable API:
  - `lightgraph/lightgraph.hpp`
  - `lightgraph/engine.hpp`
  - `lightgraph/types.hpp`
  - `lightgraph/status.hpp`
  - `lightgraph/version.hpp`
- Source-integration module headers (for in-repo integrations like [MeshLED](https://github.com/kasparsj/meshled)):
  - `lightgraph/integration.hpp`
  - `lightgraph/integration/topology.hpp`
  - `lightgraph/integration/runtime.hpp`
  - `lightgraph/integration/rendering.hpp`
  - `lightgraph/integration/objects.hpp`
  - `lightgraph/integration/factory.hpp`
  - `lightgraph/integration/debug.hpp`

The stable install/export package installs only the stable API headers.
Source-integration headers are build-tree only and are intentionally exposed
through a separate CMake target: `lightgraph::integration`.

## Build and Test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=ON
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
- `coverage`: gcov/llvm-cov instrumentation profile for report generation

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Static Analysis

Lightweight static analysis helpers are included:

```bash
./scripts/check-clang-format.sh
cmake -S . -B build/static-analysis -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=OFF -DLIGHTGRAPH_CORE_BUILD_BENCHMARKS=ON
./scripts/run-clang-tidy.sh build/static-analysis
./scripts/check-benchmark.sh build/static-analysis/lightgraph_core_benchmark
```

## Coverage Reporting

```bash
cmake --preset coverage
cmake --build --preset coverage --parallel
ctest --preset coverage
./scripts/generate-coverage.sh build/preset-coverage
```

## Quickstart (Stable API)

```cpp
#include <lightgraph/lightgraph.hpp>

int main() {
    lightgraph::EngineConfig config;
    config.object_type = lightgraph::ObjectType::Line;
    config.pixel_count = 64;

    lightgraph::Engine engine(config);

    lightgraph::EmitCommand emit;
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

    const lightgraph::Color color = p0.value();
    return (color.r || color.g || color.b) ? 0 : 1;
}
```

A compiling example is provided in `examples/minimal_usage.cpp`.
For source-level topology/runtime integration, see `examples/integration_host_loop.cpp`.

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
topology/runtime internals (`lightgraph/integration*.hpp`).

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
- `LIGHTGRAPH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` (default: `OFF`)

## Benchmarks

Micro-benchmark target:

```bash
cmake -S . -B build-bench -DLIGHTGRAPH_CORE_BUILD_BENCHMARKS=ON -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=OFF
cmake --build build-bench --parallel
./build-bench/lightgraph_core_benchmark
```

## Package Distribution

In addition to CMake install/export:

- Conan recipe: `conanfile.py`
- vcpkg overlay templates: `packaging/vcpkg/`

## Source Layout

- `include/lightgraph/` stable facade + source-integration module headers
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
- Release process: `docs/RELEASE.md`
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
