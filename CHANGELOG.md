# Changelog

## Unreleased

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

### Refactor

- Added `src/api/Engine.cpp` facade over legacy runtime/state internals.
- Improved `TopologyObject` ownership handling using internal `std::unique_ptr` containers.
- Hardened runtime pixel access against out-of-range reads/writes in `State`.
- Fixed `LightList` reallocation teardown to delete using allocated size.
- Fixed undefined behavior in `Connection::render` index conversion/clamping.
- Replaced recursive source globs with explicit CMake source lists.
- Split third-party color-theory compilation into a dedicated internal target.

### Build

- Added install/export/package-config support (`lightgraphConfig.cmake`).
- Added CI-friendly `CMakePresets.json` profiles:
  - `default`, `warnings`, `asan`, `ubsan`, `coverage`
- Added CI coverage job and gcovr artifact generation.
- Added benchmark guardrail check in CI static-analysis lane.

### Tests

- Added stable API coverage (`tests/public_api_test.cpp`).
- Added API fuzz lane (`tests/api_fuzz_test.cpp`).
- Added mutation edge coverage (`tests/core_mutation_edge_test.cpp`).
- Added sanitizer-driven regressions for runtime memory/UB fixes.

### Docs

- Reworked `README.md` to document stable-vs-source-integration header tiers.
- Rewrote `docs/API.md` for the current public surface.
- Updated `MIGRATION.md` with breaking changes and parent migration notes.
- Added thread-safety, determinism, and complexity guarantees to API docs.
- Added a host-loop integration example with multi-object + custom palette strategy.
- Added `docs/RELEASE.md` with tag/release/package publication workflow.
