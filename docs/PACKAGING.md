# Packaging Guide

This document describes supported package-consumption paths for Lightgraph.

## CMake Package (Primary)

Install and consume as a CMake package:

```bash
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix /path/to/install
```

```cmake
find_package(lightgraph CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE lightgraph::lightgraph)
```

## Conan (Template Recipe)

`conanfile.py` is provided at repository root for Conan 2 workflows.

Example:

```bash
conan create . --build=missing
```

The recipe disables tests/examples/docs/benchmarks for package builds.

## vcpkg (Overlay Port Template)

Template files are provided in:

- `packaging/vcpkg/vcpkg.json`
- `packaging/vcpkg/portfile.cmake`

Use these in an overlay port setup:

```bash
vcpkg install lightgraph --overlay-ports=/path/to/lightgraph/packaging/vcpkg
```

## Consumer Example

The package consumer smoke test under `tests/package_smoke/` validates that an
installed Lightgraph package can be discovered with `find_package(lightgraph)`,
linked, and executed.

## Release Flow

See `docs/RELEASE.md` for the canonical release checklist (version bumps,
quality gates, tagging, and registry publication guidance).
