# Changelog

## Unreleased

### API

- Added a public namespaced header surface under `include/lightpath/`.
- Added umbrella include `lightpath/lightpath.hpp`.
- Added object factory helper `lightpath::makeObject(...)`.
- Added RAII runtime facade `lightpath::Engine`.
- Breaking change: `LPObject::getParams(char)` now returns `std::optional<EmitParams>` and `LPObject::getModelParams(int)` now returns `EmitParams` by value (both `const`).
- Added topology editing helpers `LPObject::removeConnection(uint8_t,size_t)` and `LPObject::removeConnection(Connection*)`.
- Breaking change: `Intersection::ports` moved from raw array pointer to `std::vector<Port*>`.

### Build

- Added target alias `lightpath::lightpath`.
- Added `LIGHTPATH_CORE_BUILD_EXAMPLES` option and minimal example target.
- Added `LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` option to export `src/` as a public include root for internal module headers.
- Changed `LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS` default to `OFF` (BC break for consumers that include internal headers directly).

### Refactor

- Refactored `src/runtime/EmitParams.h` to value semantics (`std::optional<uint16_t>` for length) and removed manual heap ownership.
- Refactored `src/runtime/State.h`/`src/runtime/State.cpp` pixel accumulation buffers to RAII vectors.
- Reduced unnecessary coupling in object headers by removing direct `State.h` includes.
- Made `LightList` copy/move assignment explicitly deleted to avoid accidental unsafe ownership operations.
- Reorganized `src/` into module folders (`topology`, `runtime`, `rendering`, `debug`) aligned with public API headers.
- Removed flat forwarding headers in `src/*.h` (intentional source-level BC break for internal includes).
- Moved `Weight.cpp` to `src/topology/Weight.cpp` to co-locate topology implementation files.
- Refactored `Intersection` port storage to RAII vector-backed slots.
- Fixed connection teardown lifecycle by detaching ports from endpoint intersections in `Port::~Port`.
- Hardened debugger initialization against null/removed ports in edited topologies.

### Tests

- Added `tests/public_api_test.cpp` for public API coverage.
- Added example execution to CTest when tests are enabled.
- Extended regression coverage for optional/value command-parameter API behavior.
- Added regression coverage for connection removal + port-slot detachment/reconnect behavior.

### Docs

- Reworked `README.md` with public API quickstart and integration guidance.
- Added API reference at `docs/API.md`.
- Added migration guide at `MIGRATION.md`.
