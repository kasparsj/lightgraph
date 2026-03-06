# Build And Integration

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
- `LIGHTGRAPH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` (default: `OFF`)

Non-CMake integrations can disable the same feature by defining
`LIGHTGRAPH_FRACTIONAL_RENDERING=0` when compiling Lightgraph sources.

## Package Distribution

In addition to CMake install/export:

- Conan recipe: `conanfile.py`
- vcpkg overlay templates: `packaging/vcpkg/`
