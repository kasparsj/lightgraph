# Lightpath API Reference

This document describes the supported public API in `include/lightpath/`.

Compatibility and deprecation guarantees for this API are defined in
`docs/API_POLICY.md`.

## 1) Umbrella Include

```cpp
#include <lightpath/lightpath.hpp>
```

The umbrella header re-exports stable installable API headers:

- `lightpath/engine.hpp`
- `lightpath/types.hpp`
- `lightpath/status.hpp`
- `lightpath/version.hpp`

## 2) High-Level Engine API

### `lightpath::ObjectType`

Built-in object selection enum:

- `Heptagon919`
- `Heptagon3024`
- `Line`
- `Cross`
- `Triangle`

### `lightpath::EngineConfig`

Engine configuration fields:

- `object_type`
- `pixel_count` (`0` uses object default)
- `auto_emit`

### `lightpath::EmitCommand`

Value command for one emit request.

Key fields:

- `model`, `speed`, `length`, `trail`, `color`
- `note_id`, `min_brightness`, `max_brightness`
- `behaviour_flags`, `emit_groups`, `emit_offset`
- `duration_ms`, `from`, `linked`

### `lightpath::ErrorCode`, `lightpath::Status`, `lightpath::Result<T>`

Typed error and result model used by `Engine`.

### `lightpath/version.hpp`

Version and deprecation surface:

- `LIGHTPATH_VERSION_MAJOR`
- `LIGHTPATH_VERSION_MINOR`
- `LIGHTPATH_VERSION_PATCH`
- `LIGHTPATH_VERSION_STRING`
- `LIGHTPATH_DEPRECATED("message")`

### `lightpath::Engine`

Thread-safe runtime facade:

- `Result<int8_t> emit(const EmitCommand&)`
- `void update(uint64_t millis)`
- `void tick(uint64_t delta_millis)`
- `void stopAll()`
- `bool isOn() const`, `void setOn(bool)`
- `bool autoEmitEnabled() const`, `void setAutoEmitEnabled(bool)`
- `uint16_t pixelCount() const`
- `Result<Color> pixel(uint16_t index, uint8_t max_brightness = 255) const`

## 3) Source-Integration Module Headers

These headers expose broader topology/runtime/rendering integration types used by MeshLED.
They are source-level integration headers and are not part of the installable stable package contract.

Primary integration umbrella:

```cpp
#include <lightpath/integration.hpp>
```

### `lightpath/integration/topology.hpp`

Namespace aliases:

- `lightpath::integration::Object` (`LPObject`)
- `lightpath::integration::Intersection`
- `lightpath::integration::Connection`
- `lightpath::integration::Model`
- `lightpath::integration::Owner`
- `lightpath::integration::Port`, `lightpath::integration::InternalPort`, `lightpath::integration::ExternalPort`
- `lightpath::integration::Weight`

### `lightpath/integration/runtime.hpp`

Namespace aliases:

- `lightpath::integration::EmitParam`
- `lightpath::integration::EmitParams`
- `lightpath::integration::Behaviour`
- `lightpath::integration::RuntimeLight`
- `lightpath::integration::Light`
- `lightpath::integration::LightList`
- `lightpath::integration::BgLight`
- `lightpath::integration::RuntimeState`

### `lightpath/integration/rendering.hpp`

Namespace aliases/helpers:

- `lightpath::integration::Palette`
- `lightpath::integration::kWrapNoWrap`
- `lightpath::integration::kWrapClampToEdge`
- `lightpath::integration::kWrapRepeat`
- `lightpath::integration::kWrapRepeatMirror`
- `lightpath::integration::paletteCount()`
- `lightpath::integration::paletteAt(index)`

### `lightpath/integration/objects.hpp`

Namespace aliases/constants:

- `lightpath::integration::Heptagon919`, `lightpath::integration::Heptagon3024`, `lightpath::integration::Line`, `lightpath::integration::Cross`, `lightpath::integration::Triangle`
- model enums for built-ins
- default pixel-count constants (`kLinePixelCount`, etc.)

### `lightpath/integration/factory.hpp`

- `lightpath::integration::BuiltinObjectType`
- `lightpath::integration::makeObject(...)`

### `lightpath/integration/debug.hpp`

- `lightpath::integration::Debugger`

## 4) Invariants

- Stable engine API never returns raw owning pointers.
- `Engine::pixel(...)` returns `ErrorCode::OutOfRange` for invalid indices.
- `EmitCommand::max_brightness` must be `>= min_brightness` (`InvalidArgument` otherwise).
- Runtime update and output access in `Engine` are mutex-protected.

## 5) Internal Layout (Non-API)

- `src/topology`
- `src/runtime`
- `src/rendering`
- `src/objects`
- `src/debug`

Internal headers under `src/` are implementation details and may change.
