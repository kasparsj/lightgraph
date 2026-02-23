# Changelog

## Unreleased

### API

- Added stable high-level public API:
  - `include/lightpath/lightpath.hpp`
  - `include/lightpath/engine.hpp`
  - `include/lightpath/types.hpp`
  - `include/lightpath/status.hpp`
- Moved historical broad API to explicit compatibility layer:
  - `include/lightpath/legacy.hpp`
  - `include/lightpath/legacy/*.hpp`
- Added typed status/result error model (`ErrorCode`, `Status`, `Result<T>`).

### Refactor

- Added `src/api/Engine.cpp` facade over legacy runtime/state internals.
- Improved `LPObject` ownership handling using internal `std::unique_ptr` containers.
- Hardened runtime pixel access against out-of-range reads/writes in `State`.
- Fixed `LightList` reallocation teardown to delete using allocated size.
- Fixed undefined behavior in `Connection::render` index conversion/clamping.

### Build

- Added install/export/package-config support (`lightpathConfig.cmake`).
- Added install option for compatibility headers:
  - `LIGHTPATH_CORE_INSTALL_LEGACY_HEADERS`
- Added CI-friendly `CMakePresets.json` profiles:
  - `default`, `warnings`, `asan`, `ubsan`

### Tests

- Added stable API coverage (`tests/public_api_test.cpp`).
- Added API fuzz lane (`tests/api_fuzz_test.cpp`).
- Added mutation edge coverage (`tests/core_mutation_edge_test.cpp`).
- Added sanitizer-driven regressions for runtime memory/UB fixes.

### Docs

- Reworked `README.md` to match stable + legacy API split.
- Rewrote `docs/API.md` for the current public surface.
- Updated `MIGRATION.md` with breaking changes and parent migration notes.
