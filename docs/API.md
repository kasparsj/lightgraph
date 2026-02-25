# Lightgraph API Reference

This document describes the supported public API in `include/lightgraph/`.

Compatibility and deprecation guarantees for this API are defined in
`docs/API_POLICY.md`.

## 1) Umbrella Include

```cpp
#include <lightgraph/lightgraph.hpp>
```

The umbrella header re-exports stable installable API headers:

- `lightgraph/engine.hpp`
- `lightgraph/types.hpp`
- `lightgraph/status.hpp`
- `lightgraph/version.hpp`

## 2) High-Level Engine API

### `lightgraph::ObjectType`

Built-in object selection enum:

- `Heptagon919`
- `Heptagon3024`
- `Line`
- `Cross`
- `Triangle`

### `lightgraph::EngineConfig`

Engine configuration fields:

- `object_type`
- `pixel_count` (`0` uses object default)
- `auto_emit`

### `lightgraph::EmitCommand`

Value command for one emit request.

Key fields:

- `model`, `speed`, `length`, `trail`, `color`
- `note_id`, `min_brightness`, `max_brightness`
- `behaviour_flags`, `emit_groups`, `emit_offset`
- `duration_ms`, `from`, `linked`

### `lightgraph::ErrorCode`, `lightgraph::Status`, `lightgraph::Result<T>`

Typed error and result model used by `Engine`.

### `lightgraph/version.hpp`

Version and deprecation surface:

- `LIGHTGRAPH_VERSION_MAJOR`
- `LIGHTGRAPH_VERSION_MINOR`
- `LIGHTGRAPH_VERSION_PATCH`
- `LIGHTGRAPH_VERSION_STRING`
- `LIGHTGRAPH_DEPRECATED("message")`

### `lightgraph::Engine`

Thread-safe runtime facade:

- `Result<int8_t> emit(const EmitCommand&)`
- `void update(uint64_t millis)`
- `void tick(uint64_t delta_millis)`
- `void stopAll()`
- `bool isOn() const`, `void setOn(bool)`
- `bool autoEmitEnabled() const`, `void setAutoEmitEnabled(bool)`
- `uint16_t pixelCount() const`
- `Result<Color> pixel(uint16_t index, uint8_t max_brightness = 255) const`

## 3) Operational Guarantees

### Thread-safety

- `lightgraph::Engine` is safe for concurrent calls on the same instance.
- No additional external locking is required for `emit/update/tick/pixel/...` on one instance.
- Source-integration types (`lightgraph::integration::*`) are not thread-safe by default.

### Determinism

- With fixed inputs, deterministic command ordering, and a fixed `std::rand` seed
  (`std::srand(...)`), `lightgraph::Engine` emits deterministic output.
- Any call path that uses random defaults (for example omitted color/length in low-level
  integrations) inherits `std::rand` global-state behavior.

### Complexity (per call, approximate)

- `Engine::pixelCount()`: `O(1)`
- `Engine::pixel(index)`: `O(1)`
- `Engine::emit(...)`: `O(MAX_LIGHT_LISTS + G)` where `G` is grouped emitter lookup work.
- `Engine::update(...)` / `Engine::tick(...)`: `O(P + L)` where `P` is pixel count and
  `L` is active runtime light count.
- `Engine::stopAll()`: `O(MAX_LIGHT_LISTS)`

## 4) Source-Integration Module Headers

These headers expose broader topology/runtime/rendering integration types used by MeshLED.
They are source-level integration headers and are not part of the installable stable package contract.

Primary integration umbrella:

```cpp
#include <lightgraph/integration.hpp>
```

For CMake source-tree consumers, link the dedicated target:

```cmake
target_link_libraries(your_target PRIVATE lightgraph::integration)
```

### `lightgraph/integration/topology.hpp`

Namespace aliases:

- `lightgraph::integration::Object` (`TopologyObject`)
- `lightgraph::integration::Intersection`
- `lightgraph::integration::Connection`
- `lightgraph::integration::Model`
- `lightgraph::integration::Owner`
- `lightgraph::integration::Port`, `lightgraph::integration::InternalPort`, `lightgraph::integration::ExternalPort`
- `lightgraph::integration::Weight`

### `lightgraph/integration/runtime.hpp`

Namespace aliases:

- `lightgraph::integration::EmitParam`
- `lightgraph::integration::EmitParams`
- `lightgraph::integration::Behaviour`
- `lightgraph::integration::RuntimeLight`
- `lightgraph::integration::Light`
- `lightgraph::integration::LightList`
- `lightgraph::integration::BgLight`
- `lightgraph::integration::RuntimeState`

### `lightgraph/integration/rendering.hpp`

Namespace aliases/helpers:

- `lightgraph::integration::Palette`
- `lightgraph::integration::kWrapNoWrap`
- `lightgraph::integration::kWrapClampToEdge`
- `lightgraph::integration::kWrapRepeat`
- `lightgraph::integration::kWrapRepeatMirror`
- `lightgraph::integration::paletteCount()`
- `lightgraph::integration::paletteAt(index)`

### `lightgraph/integration/objects.hpp`

Namespace aliases/constants:

- `lightgraph::integration::Heptagon919`, `lightgraph::integration::Heptagon3024`, `lightgraph::integration::Line`, `lightgraph::integration::Cross`, `lightgraph::integration::Triangle`
- model enums for built-ins
- default pixel-count constants (`kLinePixelCount`, etc.)

### `lightgraph/integration/factory.hpp`

- `lightgraph::integration::BuiltinObjectType`
- `lightgraph::integration::makeObject(...)`

### `lightgraph/integration/debug.hpp`

- `lightgraph::integration::Debugger`

## 5) Invariants

- Stable engine API never returns raw owning pointers.
- `Engine::pixel(...)` returns `ErrorCode::OutOfRange` for invalid indices.
- `EmitCommand::max_brightness` must be `>= min_brightness` (`InvalidArgument` otherwise).
- Runtime update and output access in `Engine` are mutex-protected.

## 6) Internal Layout (Non-API)

- `src/topology`
- `src/runtime`
- `src/rendering`
- `src/objects`
- `src/debug`

Internal headers under `src/` are implementation details and may change.
