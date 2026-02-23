# Lightpath API Reference

This document describes the public API exposed under `include/lightpath/`.

## Entry Points

- Umbrella include: `#include <lightpath/lightpath.hpp>`
- Modular includes:
  - `lightpath/types.hpp`
  - `lightpath/topology.hpp`
  - `lightpath/runtime.hpp`
  - `lightpath/rendering.hpp`
  - `lightpath/objects.hpp`
  - `lightpath/factory.hpp`
  - `lightpath/debug.hpp`

## Core Types

### `lightpath::Color`

Alias of legacy `ColorRGB`.

Invariants:

- Channels are 8-bit RGB (`R`, `G`, `B`)
- `get()` returns packed `0xRRGGBB`

### `lightpath::EmitParams`

Defines one emit request.

Invariants:

- `model` must reference a valid model for the active object (`State::emit` returns `-1` when invalid)
- `setLength(...)` controls requested list length (or random when omitted)
- `EmitParams` is a value type; command lookup APIs do not transfer heap ownership

## Topology Module (`topology.hpp`)

- `lightpath::Object`
- `lightpath::Intersection`
- `lightpath::Connection`
- `lightpath::Model`
- `lightpath::Port`, `lightpath::InternalPort`, `lightpath::ExternalPort`

Invariants:

- Grouped topology arrays are bounded by `lightpath::kMaxGroups`
- `Connection` LED count controls light traversal granularity
- `Intersection::ports` is a fixed-size slot vector (`std::vector<Port*>` sized by `numPorts`)
- Removing a `Connection` detaches its endpoint ports from intersections

### Topology Mutation Helpers

`LPObject` exposes runtime-safe connection removal helpers:

- `bool removeConnection(uint8_t groupIndex, size_t index)`
- `bool removeConnection(Connection* connection)`

Rules:

- Prefer these helpers over manual `delete` + `erase` on `conn[group]`.
- Successful removal guarantees endpoint intersections no longer retain dangling port pointers.

### Command Parameter API

`LPObject` command dispatch entry points:

- `std::optional<EmitParams> getParams(char command) const`
- `EmitParams getModelParams(int model) const`

Rules:

- `getParams(...)` returns `std::nullopt` when a command does not map to an emit action.
- Returned `EmitParams` values are stack/value-managed; callers must not `delete` anything.
- Custom objects overriding these APIs must use the exact `const` signatures above.

## Runtime Module (`runtime.hpp`)

- `lightpath::RuntimeState`
- `lightpath::LightList`, `lightpath::BgLight`
- `lightpath::RuntimeLight`, `lightpath::Light`
- `lightpath::Behaviour`
- `lightpath::Engine` facade

### `lightpath::Engine`

RAII wrapper owning both topology object and runtime state.

Behavior:

- `Engine::update(millis)` updates global runtime time and advances state.
- `Engine::state()` exposes full `State` API.

## Rendering Module (`rendering.hpp`)

- `lightpath::Palette`
- `lightpath::paletteCount()`
- `lightpath::paletteAt(index)`

Invariants:

- Palette interpolation is deterministic for fixed inputs.
- Wrap behavior is controlled via `kWrapNoWrap`, `kWrapClampToEdge`, `kWrapRepeat`, `kWrapRepeatMirror`.

## Built-In Objects (`objects.hpp` + `factory.hpp`)

Built-ins:

- `Heptagon919`
- `Heptagon3024`
- `Line`
- `Cross`
- `Triangle`

Factory:

- `lightpath::makeObject(lightpath::BuiltinObjectType type, uint16_t pixelCount = 0)`

Default pixel counts for line/cross/triangle are exposed as:

- `lightpath::kLinePixelCount`
- `lightpath::kCrossPixelCount`
- `lightpath::kTrianglePixelCount`

## Debug Module (`debug.hpp`)

- `lightpath::Debugger`

Provides inspection helpers for intersections, connections, and weighted model coverage.

## Compatibility Notes

- Flat compatibility headers at `src/*.h` were removed.
- Legacy `lightpath::LP*` compatibility aliases were removed from the public namespace.
- Pointer-based `EmitParams*` command lookup signatures are no longer supported.
- `Intersection::ports` is no longer a raw array pointer and should not be deleted manually.
- Internal source modules live under `src/topology`, `src/runtime`, `src/rendering`, and `src/debug`.
