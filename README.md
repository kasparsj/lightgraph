# Lightpath

Lightpath is a standalone C++ light-graph engine extracted from Lightgraph.

## Features

- Topology-based light routing (`Intersection`, `Connection`, `Port`)
- Layered runtime animation (`State`, `LightList`, `LPLight`)
- Palette and blend-mode rendering
- Host CMake build with smoke/regression tests

## Prerequisites

- CMake 3.20+
- C++17 compiler
- Git submodules (for `vendor/ofxColorTheory`)

## Build and test

```bash
git submodule update --init --recursive
cmake -S . -B build -DLIGHTGRAPH_CORE_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Sanitizer / warnings profiles

```bash
CC=clang CXX=clang++ cmake -S . -B build-asan -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_ENABLE_ASAN=ON
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build-asan --output-on-failure

CC=clang CXX=clang++ cmake -S . -B build-ubsan -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_ENABLE_UBSAN=ON
cmake --build build-ubsan
ctest --test-dir build-ubsan --output-on-failure

CC=clang CXX=clang++ cmake -S . -B build-warnings -DLIGHTGRAPH_CORE_BUILD_TESTS=ON -DLIGHTGRAPH_CORE_ENABLE_STRICT_WARNINGS=ON
cmake --build build-warnings
ctest --test-dir build-warnings --output-on-failure
```
