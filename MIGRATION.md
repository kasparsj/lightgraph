# Lightpath Migration Guide

This guide covers migration to the current refactored Lightpath layout.

## What Changed

1. Introduced a stable high-level API:
   - `lightpath/lightpath.hpp`
   - `lightpath/engine.hpp`
   - `lightpath/types.hpp`
   - `lightpath/status.hpp`
2. Moved historical broad API into a compatibility namespace/header set:
   - `lightpath/legacy.hpp`
   - `lightpath/legacy/*.hpp`
3. Added CMake install/export support:
   - installable target: `lightpath::lightpath`
   - generated package config: `lightpathConfig.cmake`
4. Added CI-friendly preset profiles (`CMakePresets.json`).
5. Fixed runtime memory-safety issues found by fuzz/sanitizer tests:
   - pixel write/read bounds hardening in `State`
   - `LightList` reallocation/delete size mismatch
   - connection render index UB under UBSAN

## Breaking Changes

1. Top-level historical headers are no longer under `include/lightpath/*.hpp`:
   - `topology.hpp`, `runtime.hpp`, `rendering.hpp`, `objects.hpp`,
     `factory.hpp`, `debug.hpp` moved to `include/lightpath/legacy/`.
2. Consumers of the old broad API must now include:
   - `#include <lightpath/legacy.hpp>`
   - or individual `lightpath/legacy/*.hpp` headers.
3. Stable API is intentionally smaller and engine-oriented; it does not expose
   direct topology/runtime internals.

## Migration Paths

### Path A: Stay on Legacy API (fastest)

Use:

```cpp
#include <lightpath/legacy.hpp>
```

No semantic changes are required for most existing MeshLED-style code.

### Path B: Migrate to Stable API (recommended for new integrations)

Use:

```cpp
#include <lightpath/lightpath.hpp>
```

Then:

1. Replace direct `State`/`LPObject` ownership with `lightpath::Engine`.
2. Replace imperative parameter mutation with `lightpath::EmitCommand`.
3. Replace raw return/error conventions with `lightpath::Result<T>`.

## CMake Migration

### As a subdirectory

```cmake
add_subdirectory(external/lightpath)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

### As an installed package

```cmake
find_package(lightpath CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE lightpath::lightpath)
```

Legacy headers are installed only when:

```cmake
-DLIGHTPATH_CORE_INSTALL_LEGACY_HEADERS=ON
```

## Parent Migration Notes (MeshLED)

Parent code was updated in the same change set:

1. Simulator now includes:
   - `apps/simulator/src/ofApp.h` -> `#include "lightpath/legacy.hpp"`
2. Firmware now includes:
   - `firmware/esp/LightPath.h` -> `#include <lightpath/legacy.hpp>`
   - `firmware/esp/WebServerLayers.h` -> `#include <lightpath/legacy/rendering.hpp>`
   - `firmware/esp/homo_deus.ino` (debug mode) -> `#include <lightpath/legacy/debug.hpp>`
3. Existing include paths for `packages/lightpath/include` remain valid.

## Compatibility Notes

- Legacy API remains source-compatible where possible but is now explicitly
  marked as compatibility surface.
- Stable API and legacy API can evolve independently.
- Internal `src/` headers remain non-API and may continue to change.
