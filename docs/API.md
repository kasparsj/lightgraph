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
- `length_mode`, `visible_length`

`LengthMode::Legacy` preserves the existing moving-list behavior.
`LengthMode::Centered` reserves the explicit `length` as one contiguous list and
uses `visible_length` as its centered, fractional visible span. A centered emit
requires a non-zero `note_id`, an explicit non-zero `length`, linked lights, and
behavior flags compatible with fixed contiguous allocation.

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
- `Status setListLengths(const ListLengthUpdate* updates, size_t count)`
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
- Drawing mutation, topology mutation, transfer-session work, and rendering must be
  serialized on the same owner thread or by the same recursive lock.

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

`RuntimeState::initializationResult` distinguishes `Ready`, `AdmissionDenied`,
and `AllocationFailed`. A state that is not ready is permanently inert: it
rejects emissions, ignores updates, and returns black pixels. Construct a fresh
candidate to recover, and publish it only after `ready()` succeeds.

### Drawing and geometry source integration

- `lightgraph/integration/drawing.hpp` re-exports core-owned drawing scene,
  control, status, handle, result, and runtime types.
- `lightgraph/integration/geometry.hpp` exposes `GeometryProvider`,
  `GeometryResult`, coordinate/view types, and the heptagon provider.
- `lightgraph/integration/drawing_codec.hpp` exposes `DrawingSceneCodec` for the
  existing binary scene representation.
- `lightgraph/integration/drawing_session.hpp` owns producer arbitration,
  chunk assembly, timeout handling, frame ordering, and transfer counters. It
  uses the drawing runtime's inactive image slot rather than allocating a third
  full image buffer.

Drawing and geometry are source-integration APIs. They are compiled into the
library for in-tree consumers but are not installed as stable package headers.
All five built-in topology objects provide drawing geometry. Geometry uses one
finite coordinate per logical pixel in `[-1, 1]`, with positive Y downward.

`lightgraph/integration/drawing_presentation.hpp` exposes
`PendingDrawingPresentation` for owner-loop consumers. Bind it to the runtime,
enqueue accepted handles, and call `poll()` before processing input each iteration.
Enqueue attempts immediate presentation until a handle returns `Busy`, preserving
ordering with subsequent drawing commands in the normal path. It retains FIFO
ownership on `Busy`, holds at most three handles, and releases terminal failures.
`DrawingRuntime::validQueued()` checks handle ownership without presenting it;
the helper uses it to discard retired slots before capacity checks.
Binding a replacement runtime or changing the drawing generation clears old
ownership through the lifetime guard. It allocates no scene or image storage.

`DrawingStatus::lastRejection` owns a null-terminated `std::array<char, 64>`;
use `.data()` for C-string serialization. Status copies survive runtime
replacement and destruction. `reject(nullptr)` records `"none"`; longer reasons
are truncated to 63 bytes. Existing transport rejection names are unchanged.

Custom geometry factory `std::bad_alloc` failures report `Busy` and can be retried;
unrelated exceptions propagate in exception-enabled builds. Incompatible geometry
switches the entire next composed frame to the palette and retires queued drawing
submissions. Resource failures retain the last valid frame and mapping.

Contiguous allocation callbacks form a pair: supply both allocator and
deallocator, or leave both null for `malloc/free`. An incomplete pair rejects the
allocation before invoking either callback. Each allocation retains its original
deallocator and user context even when the runtime policy later changes.

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

### `lightgraph/integration/platform.hpp`

- imports platform logging and utility macros (`LG_LOG*`, `LG_RANDOM`, `LG_STRING`)

### `lightgraph/integration/palette_names.hpp`

- `lightgraph::integration::kPredefinedPaletteNames`
- `lightgraph::integration::predefinedPaletteNameCount()`
- `lightgraph::integration::predefinedPaletteNameAt(index)`

### `lightgraph/integration/observability.hpp`

- `lightgraph::integration::AllocationFailureSite`
- `lightgraph::integration::AllocationFailureObserver`
- `lightgraph::integration::setAllocationFailureObserver(...)`
- `lightgraph::integration::reportAllocationFailure(...)`

### `lightgraph/integration/remote_snapshot.hpp`

- namespace alias `lightgraph::integration::remote_snapshot` for remote snapshot descriptors/builders

### `lightgraph/integration/codecs.hpp`

- topology snapshot codecs (`parseTopologySnapshotFromJson`, `serializeTopologySnapshotToJson`)
- layer snapshot codecs via `lightgraph::integration::layer_json::*`
- note: codec APIs currently depend on Arduino JSON types

## 5) Invariants

- Stable engine API never returns raw owning pointers.
- `Engine::pixel(...)` returns `ErrorCode::OutOfRange` for invalid indices.
- `EmitCommand::max_brightness` must be `>= min_brightness` (`InvalidArgument` otherwise).
- New fields remain appended to `EmitCommand`, preserving existing aggregate
  initialization order.
- Centered-list update batches contain at most 19 unique, non-zero note IDs and
  finite non-negative spans. Validation is atomic: one invalid live target
  rejects the whole batch. Missing or expired notes are successful no-ops.
- A centered visible span cannot exceed that list's reserved length.
- Runtime allocation/admission failure maps to `ErrorCode::ResourceUnavailable`;
  calls against an incompletely initialized runtime map to `ErrorCode::NotReady`.
- Runtime update and output access in `Engine` are mutex-protected.

## 6) Internal Layout (Non-API)

- `src/topology`
- `src/runtime`
- `src/rendering`
- `src/drawing`
- `src/geometry`
- `src/objects`
- `src/debug`

Internal headers under `src/` are implementation details and may change.
