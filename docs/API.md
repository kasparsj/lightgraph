# Lightpath API Reference

This document describes the currently supported public API in `include/lightpath/`.

## 1) Stable API

Primary include:

```cpp
#include <lightpath/lightpath.hpp>
```

The umbrella header re-exports:

- `lightpath/engine.hpp`
- `lightpath/types.hpp`
- `lightpath/status.hpp`

### `lightpath::Color`

Simple RGB value type:

- `uint8_t r`
- `uint8_t g`
- `uint8_t b`

### `lightpath::ObjectType`

Built-in object selection enum:

- `Heptagon919`
- `Heptagon3024`
- `Line`
- `Cross`
- `Triangle`

### `lightpath::EngineConfig`

Engine construction config:

- `object_type`
- `pixel_count` (`0` = object default)
- `auto_emit`

### `lightpath::EmitCommand`

Value command for one emit call:

- required-ish fields: `model`, `speed`
- optional controls: `length`, `color`, `duration_ms`, `from`
- behavior tuning: `trail`, `behaviour_flags`, `emit_groups`, `emit_offset`
- brightness bounds: `min_brightness`, `max_brightness`
- list-reuse/linking: `note_id`, `linked`

### `lightpath::ErrorCode`

Typed error codes:

- `Ok`
- `InvalidArgument`
- `InvalidModel`
- `NoFreeLightList`
- `NoEmitterAvailable`
- `CapacityExceeded`
- `OutOfRange`
- `InternalError`

### `lightpath::Status`

Status object with:

- `ok()`
- `code()`
- `message()`
- static constructors `success()` and `error(...)`

### `lightpath::Result<T>`

Value + status container:

- `ok()`
- `value()`
- `status()`
- static `error(...)`

### `lightpath::Engine`

Thread-safe runtime facade around legacy internals:

- lifecycle:
  - `Engine(const EngineConfig&)`
- runtime actions:
  - `Result<int8_t> emit(const EmitCommand&)`
  - `void update(uint64_t millis)`
  - `void tick(uint64_t delta_millis)`
  - `void stopAll()`
- runtime toggles:
  - `bool isOn() const`, `void setOn(bool)`
  - `bool autoEmitEnabled() const`, `void setAutoEmitEnabled(bool)`
- output:
  - `uint16_t pixelCount() const`
  - `Result<Color> pixel(uint16_t index, uint8_t max_brightness = 255) const`

## 2) Legacy Compatibility API

For existing MeshLED-style integrations:

```cpp
#include <lightpath/legacy.hpp>
```

Legacy headers under `include/lightpath/legacy/*.hpp` expose topology/runtime/rendering/debug aliases backed by `src/` internals:

- `legacy/types.hpp`
- `legacy/topology.hpp`
- `legacy/runtime.hpp`
- `legacy/rendering.hpp`
- `legacy/objects.hpp`
- `legacy/factory.hpp`
- `legacy/debug.hpp`

Notes:

- This layer is intentionally compatibility-oriented and broader than the stable API.
- For installed packages, legacy headers are installed only when
  `LIGHTPATH_CORE_INSTALL_LEGACY_HEADERS=ON`.

## 3) Behavior and Invariants

- Stable API never returns raw owning pointers.
- `Engine::pixel(...)` returns `ErrorCode::OutOfRange` for invalid indices.
- `EmitCommand::max_brightness` must be `>= min_brightness` (`InvalidArgument` otherwise).
- `emit(...)` validates model availability and emitter availability before dispatch.
- Runtime update and render access are mutex-protected in the stable engine facade.

## 4) Internal Layout (Non-API)

- `src/topology`: graph representation and traversal
- `src/runtime`: list/light lifecycle and blending inputs
- `src/rendering`: palette and color-theory helpers
- `src/objects`: concrete topologies
- `src/debug`: debugger utilities

These are implementation details and may change independently of the stable API.
