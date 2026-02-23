# Packaging Guide

This document describes supported package-consumption paths for Lightpath.

## CMake Package (Primary)

Install and consume as a CMake package:

```bash
cmake -S . -B build -DLIGHTPATH_CORE_BUILD_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix /path/to/install
```

```cmake
find_package(lightpath CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
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
vcpkg install lightpath --overlay-ports=/path/to/lightpath/packaging/vcpkg
```

## Consumer Example

The package consumer smoke test under `tests/package_smoke/` validates that an
installed Lightpath package can be discovered with `find_package(lightpath)`,
linked, and executed.
