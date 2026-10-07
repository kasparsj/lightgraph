# Changelog

## Unreleased

### Recovery fixes

- Retain queued presentation handles across temporary `Busy` results in firmware
  and simulator, without adding scene/image buffers.
- Publish ready firmware topology candidates transactionally with memory admission
  and telemetry installed before runtime construction.
- Adapt built-in geometry to edited endpoints, crossings, and segment joins;
  reconcile incompatible geometry before composing a complete palette frame.
- Convert custom geometry factory allocation failures into retryable diagnostics,
  reject incomplete contiguous allocator pairs, and make rejection snapshots own
  their fixed-size text storage.

### API

- Added stable high-level public API:
  - `include/lightgraph/lightgraph.hpp`
  - `include/lightgraph/engine.hpp`
  - `include/lightgraph/types.hpp`
  - `include/lightgraph/status.hpp`
- Removed `include/lightgraph/legacy*` compatibility layer.
- Moved source-integration module headers under `include/lightgraph/integration/` and `include/lightgraph/integration.hpp`.
- Moved source-integration aliases into `lightgraph::integration::*` namespace.
- Narrowed `lightgraph/lightgraph.hpp` to stable installable API headers only (`engine.hpp`, `types.hpp`, `status.hpp`).
- Added typed status/result error model (`ErrorCode`, `Status`, `Result<T>`).
- Added explicit source-integration CMake target (`lightgraph::integration`) for
  non-installable `lightgraph/integration*.hpp` usage.
- Added stable centered-list controls through appended `EmitCommand` fields,
  `LengthMode`, `ListLengthUpdate`, and atomic `Engine::setListLengths(...)`.
- Added stable `ResourceUnavailable` and `NotReady` error results without
  changing the values of existing error codes.
- Added source-integration drawing, generic geometry, binary scene codec, and
  transfer-session APIs. These remain outside the installed stable package.

### Refactor

- Removed the unused standalone `gPerlinNoise` definition and declaration,
  saving 1,088 bytes of static RAM in the ESP32 build. Noise effects continue
  using `LightgraphRuntimeContext::perlinNoise`. Source integrations that
  referenced the internal global must use the relevant runtime context instead;
  the stable installed API is unchanged.
- Added `src/api/Engine.cpp` facade over legacy runtime/state internals.
- Improved `TopologyObject` ownership handling using internal `std::unique_ptr` containers.
- Hardened runtime pixel access against out-of-range reads/writes in `State`.
- Fixed `LightList` reallocation teardown to delete using allocated size.
- Fixed undefined behavior in `Connection::render` index conversion/clamping.
- Replaced recursive source globs with explicit CMake source lists.
- Split third-party color-theory compilation into a dedicated internal target.
- Made failed runtime initialization explicitly queryable and permanently inert;
  callers can reject a candidate without exposing partial frame storage.
- Made runtime emission admission authoritative, including same-note centered
  replacement at full capacity.
- Replaced heptagon-only drawing geometry with revisioned generic providers for
  all five built-in topologies while preserving heptagon output.
- Separated core drawing/rendering state from binary encoding and image transfer
  arbitration. Transfer assembly now leases the runtime's inactive image slot.
- Moved drawing placement, clipping, and geometry ownership out of `BgLight` and
  into drawing/runtime composition.
- Added runtime-scoped paired allocation/deallocation hooks and portable bit
  counting for MSVC.

### Build

- Added install/export/package-config support (`lightgraphConfig.cmake`).
- Added CI-friendly `CMakePresets.json` profiles:
  - `default`, `warnings`, `asan`, `ubsan`, `coverage`
- Added CI coverage job and gcovr artifact generation.
- Added benchmark guardrail check in CI static-analysis lane.
- Added a Windows MSVC warnings-as-errors build/test lane.

### Tests

- Added stable API coverage (`tests/public_api_test.cpp`).
- Added API fuzz lane (`tests/api_fuzz_test.cpp`).
- Added mutation edge coverage (`tests/core_mutation_edge_test.cpp`).
- Added sanitizer-driven regressions for runtime memory/UB fixes.
- Added deterministic heptagon drawing fixtures covering scenes, images,
  rotation, blending, clipping, mirrors, and topology refresh.
- Added geometry-provider coverage for the five built-ins and custom providers,
  plus initialization, allocation-recovery, centered-replacement, and aggregate
  compatibility regressions.

### Docs

- Reworked `README.md` to document stable-vs-source-integration header tiers.
- Rewrote `docs/API.md` for the current public surface.
- Updated `MIGRATION.md` with breaking changes and parent migration notes.
- Added thread-safety, determinism, and complexity guarantees to API docs.
- Added a host-loop integration example with multi-object + custom palette strategy.
- Added `docs/RELEASE.md` with tag/release/package publication workflow.
- Documented centered-list validation/no-op rules, runtime readiness, generic
  geometry conventions, drawing/transport ownership, and retry behavior.
