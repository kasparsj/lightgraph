# Lightgraph

## What It Is

Lightgraph is a graph-native lighting engine for installations that are not just straight LED strips.

It treats a lighting piece as a connected structure and lets light move through that structure as if it were a spatial system. You can think in terms of flow, branching, and movement across a sculpture, facade, or 3D object, instead of programming each strip as an isolated line.

It solves a common problem in advanced LED work: once a project has intersections, loops, branches, and non-linear geometry, traditional strip-first tooling becomes hard to manage. Lightgraph gives you a single abstraction for those layouts.

Unlike WLED and most strip-first systems, the primary model here is not a set of channels, but a connected light network. What makes that conceptually interesting is that physical form is no longer separate from animation logic: the shape of the installation directly influences how light behaves.

## Core Concept

The core idea is simple:

- Your LED installation is modeled as a graph (a mesh of connected segments).
- Intersections are nodes.
- LED segments between intersections are edges.
- Light events travel through this graph over time.

This means animation is no longer limited to left-to-right strip playback. Instead, motion can route through arbitrary topology, making the installation itself part of the composition logic.

Conceptually, Lightgraph is interesting because the physical structure is not just where pixels live, it becomes part of how behavior is generated.

## Why It's Different

WLED and most traditional LED strip workflows are optimized for linear channels and fixture maps. They are excellent for many practical systems, but the mental model is still largely strip-based.

Lightgraph is topology-based.

- In strip-first systems, geometry is often something you work around.
- In Lightgraph, geometry is the primary input.
- In strip-first systems, complex routing often means custom glue code.
- In Lightgraph, routing through branches and intersections is built into the model.

So instead of coordinating many strips manually, you program one connected light space.

## Example Use Cases

- LED sculpture: route light through branching metal or acrylic structures where direction and junction behavior matter.
- 3D installations: drive volumetric pieces where motion should move through depth, not just across a flat map.
- Architectural lighting: treat corridors, columns, and facade segments as a connected lighting network.
- Generative light art: build systems where the topology itself influences emergent visual behavior.

## How It Works (High Level)

At a high level, Lightgraph does four things:

1. Defines a connected lighting topology.
2. Accepts light events (speed, color, length, behavior).
3. Advances those events through the topology over time.
4. Produces per-pixel RGB output for your host runtime.

The engine is host-agnostic, so it can sit behind firmware, desktop tools, simulators, or custom control software.

## Future Vision

Lightgraph is intended to evolve in three directions at once:

- As a digital art tool: a medium for composing movement through space-aware light structures.
- As a hardware lighting engine: a reusable core for products and embedded lighting systems.
- As programmable light infrastructure: a foundation for building interoperable, topology-aware lighting stacks.

The long-term goal is to make complex spatial lighting as programmable and portable as software graphics workflows.

---

## Technical Reference

### API Layout

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

### Build and Test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### CI Presets

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

### Static Analysis

Lightweight static analysis helpers are included:

```bash
./scripts/check-clang-format.sh
cmake -S . -B build/static-analysis -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=OFF -DLIGHTGRAPH_CORE_BUILD_BENCHMARKS=ON
./scripts/run-clang-tidy.sh build/static-analysis
./scripts/check-benchmark.sh build/static-analysis/lightgraph_core_benchmark
```

### Coverage Reporting

```bash
cmake --preset coverage
cmake --build --preset coverage --parallel
ctest --preset coverage
./scripts/generate-coverage.sh build/preset-coverage
```

### Quickstart (Stable API)

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

### CMake Integration

#### `add_subdirectory`

```cmake
add_subdirectory(external/lightgraph)
target_link_libraries(your_target PRIVATE lightgraph::lightgraph)
```

#### `add_subdirectory` (source integration layer)

```cmake
add_subdirectory(external/lightgraph)
target_link_libraries(your_target PRIVATE lightgraph::integration)
```

Use `lightgraph::integration` only for source-tree integrations that need
topology/runtime internals (`lightgraph/integration*.hpp`).

#### Install + `find_package`

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

### Build Options

- `LIGHTGRAPH_CORE_BUILD_TESTS` (default: `ON`)
- `LIGHTGRAPH_CORE_BUILD_EXAMPLES` (default: `ON`)
- `LIGHTGRAPH_CORE_BUILD_BENCHMARKS` (default: `OFF`)
- `LIGHTGRAPH_CORE_BUILD_DOCS` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_ASAN` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_UBSAN` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_COVERAGE` (default: `OFF`)
- `LIGHTGRAPH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` (default: `OFF`)

### Benchmarks

Micro-benchmark target:

```bash
cmake -S . -B build-bench -DLIGHTGRAPH_CORE_BUILD_BENCHMARKS=ON -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF -DLIGHTGRAPH_CORE_BUILD_EXAMPLES=OFF
cmake --build build-bench --parallel
./build-bench/lightgraph_core_benchmark
```

### Package Distribution

In addition to CMake install/export:

- Conan recipe: `conanfile.py`
- vcpkg overlay templates: `packaging/vcpkg/`

### Source Layout

- `include/lightgraph/` stable facade + source-integration module headers
- `src/topology/` graph objects and routing
- `src/runtime/` state update and animation
- `src/rendering/` palette/blend implementation
- `src/objects/` built-in topology definitions
- `src/debug/` debug helpers
- `src/core/` shared constants/types/platform macros

### Docs

- API reference: `docs/API.md`
- API compatibility policy: `docs/API_POLICY.md`
- Packaging guide: `docs/PACKAGING.md`
- Release process: `docs/RELEASE.md`
- Migration notes: `MIGRATION.md`
- Changelog: `CHANGELOG.md`

#### Generated API Reference (GitHub Pages)

The repository publishes Doxygen API docs to GitHub Pages through GitHub Actions.

Local generation:

```bash
doxygen Doxyfile
```

Generated HTML output:

- `build/docs/html/index.html`
