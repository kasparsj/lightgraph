# Lightpath Migration Guide

## Summary

Lightpath now publishes a stable public API under `include/lightpath/` and
introduces a source-level API break for command parameter access to remove
manual heap ownership.

## What Changed

1. Added public namespaced API headers:
   - `lightpath/lightpath.hpp` (umbrella)
   - `lightpath/types.hpp`, `topology.hpp`, `runtime.hpp`, `rendering.hpp`, `objects.hpp`, `factory.hpp`, `debug.hpp`
2. Added CMake target alias:
   - `lightpath::lightpath` (aliases existing `lightpath_core`)
3. Added `lightpath::Engine` facade and `lightpath::makeObject(...)` factory helpers.
4. Added example target and public API test coverage.
5. Updated parent integrations (simulator + firmware) to include the new public headers.
6. Source-level API break:
   - `LPObject::getParams(char)` now returns `std::optional<EmitParams>` and is `const`.
   - `LPObject::getModelParams(int)` now returns `EmitParams` by value and is `const`.
   - Derived object overrides must match the new signatures.
7. Topology editing API updates:
   - `LPObject` now exposes `removeConnection(uint8_t groupIndex, size_t index)` and `removeConnection(Connection*)`.
   - `Intersection::ports` is now `std::vector<Port*>` (no manual array allocation/deallocation).
8. `src/` was reorganized to mirror public API modules:
   - `src/topology/`, `src/runtime/`, `src/rendering/`, `src/debug/`, plus existing `src/objects/`.

## Why

- Provide a professional, discoverable API boundary for external users.
- Decouple public usage from internal file layout under `src/`.
- Remove error-prone ownership transfer (`new`/`delete`) from command parameter lookups.
- Keep runtime behavior stable while modernizing API safety.

## Migration Steps

1. Prefer replacing direct `src/*` includes with:
   - `#include <lightpath/lightpath.hpp>`
   - or module-specific headers under `lightpath/`.
2. In CMake consumers, link `lightpath::lightpath`.
3. Optionally migrate to facade-based startup:
   - `auto object = lightpath::makeObject(...);`
   - `lightpath::Engine engine(std::move(object));`
4. Update command parameter usage from owning pointers to values:

```cpp
// Before
EmitParams* params = object->getParams(command);
if (params != nullptr) {
  doEmit(*params);
}
delete params;

// After
if (std::optional<EmitParams> params = object->getParams(command)) {
  doEmit(*params);
}
```
5. Update custom object overrides:

```cpp
// Before
EmitParams* getModelParams(int model) override;
EmitParams* getParams(char command) override;

// After
EmitParams getModelParams(int model) const override;
std::optional<EmitParams> getParams(char command) const override;
```
6. If you mutate topology at runtime, replace direct connection delete/erase with:

```cpp
// Before
delete object->conn[group][index];
object->conn[group].erase(object->conn[group].begin() + index);

// After
object->removeConnection(group, index);
```
7. If you include private/internal headers directly, prefer module paths under `src/`:
   - `src/topology/...`, `src/runtime/...`, `src/rendering/...`, `src/debug/...`

## Compatibility / Deprecation Notes

- Flat compatibility headers in `src/*.h` were removed.
- Internal includes must use module paths under `src/`:
  - `src/topology/...`, `src/runtime/...`, `src/rendering/...`, `src/debug/...`
- `LIGHTPATH_CORE_ENABLE_LEGACY_INCLUDE_PATHS=ON` only exports `src/` as a
  public include root for module-prefixed internal headers.
- No runtime behavior changes are intended in this migration.
- There is no pointer-based compatibility shim for `getParams`/`getModelParams`; callers and overrides must migrate to value semantics.
- Port-slot lifetime is now managed automatically during connection teardown; manual `Intersection` port cleanup is no longer needed.

## Parent Migration Notes (MeshLED)

The parent repository was migrated in the same change set:

1. Firmware headers now consume public API headers:
   - `firmware/esp/LightPath.h` -> `<lightpath/lightpath.hpp>`
   - `firmware/esp/WebServerLayers.h` -> `<lightpath/rendering.hpp>`
   - `firmware/esp/homo_deus.ino` (debug path) -> `<lightpath/debug.hpp>`
2. PlatformIO now exports the public include directory:
   - `firmware/esp/platformio.ini` adds `-I../../packages/lightpath/include`
3. Simulator now includes the umbrella API header:
   - `apps/simulator/src/ofApp.h` -> `\"lightpath/lightpath.hpp\"`
   - `apps/simulator/config.make` adds include path `-I../../packages/lightpath/include`
4. Parent command dispatchers migrated to optional/value API:
   - `apps/simulator/src/ofApp.cpp` now consumes `std::optional<EmitParams>`
   - `firmware/esp/LEDLib.h` now consumes `std::optional<EmitParams>`
5. Parent firmware topology editor now uses `LPObject::removeConnection(...)` and no longer manually deletes from connection vectors.
