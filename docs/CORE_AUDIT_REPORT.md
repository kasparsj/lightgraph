# Lightpath Core Audit Report

Date: 2026-02-23
Scope: `src` and core-adjacent runtime adapters. Downstream integration references to MeshLED (`apps/simulator`, `firmware/esp`) are included where relevant.

Canonical location: `lightpath/docs/CORE_AUDIT_REPORT.md` in the Lightpath repository.

## 1) Executive Summary

### What the core appears to do

- Implements a graph-based light propagation engine over `Intersection` and `Connection` topology objects, with per-path weighted routing via `Model`/`Weight`.
- Maintains runtime animation layers (`LightList`) that emit and update light particles (`LPLight`/`Light`) every frame.
- Applies color/palette interpolation (including `ofxColorTheory` color rules) and per-layer blend modes into shared pixel accumulators in `State`.
- Supports behavior flags for movement/brightness/mirroring/segment rendering (`BehaviourFlags` in `Config.h`) that alter propagation and rendering semantics.
- Is reused in two surfaces: desktop simulator (openFrameworks + OSC input) and ESP firmware (Arduino + OSC/HTTP control), with firmware compiling core through a symlink (`firmware/esp/src -> ../../packages/lightpath/src`).

### Readiness score (0-10)

| Area | Score | Rationale |
|---|---:|---|
| Buildability | 7 | Core now compiles via standalone host CMake target and firmware path (`pio run -e esp32dev -t compiledb`), while simulator still depends on external OF layout.
| Reliability | 5 | Core teardown safety improved and sanitizer lanes are passing, but macro-heavy abstractions and known warnings still create risk.
| Docs | 6 | Core build docs and reproducible host workflow exist, but architecture and troubleshooting depth can still improve.
| Tests | 6 | Host smoke/regression tests run in CI with ASan and UBSan, though rendering/long-run coverage is still limited.
| Portability | 4 | Dual Arduino/openFrameworks abstraction exists, but host build is OF-coupled and firmware integration relies on symlink behavior.
| Maintainability | 5 | Build/test ergonomics improved, but centralized state and raw-pointer ownership still increase change risk.

### 5 highest-impact wins

- Add a standalone core build target (host static lib + minimal test harness) to decouple from simulator/firmware scaffolding.
- Fix correctness bugs in `LightList`/`State`/object model selection and add guardrails for null/div-by-zero paths.
- Replace fixed pixel buffer assumptions in `LPLight` segment/link rendering with bounded storage (or explicit clipping).
- Add baseline tests for topology routing, emit/update lifecycle, and palette interpolation behavior.
- Publish core architecture + integration contract docs (engine loop, ownership, protocol mapping from OSC/HTTP to `EmitParams`).

## 2) Codebase Map

### Directory/module map

- `packages/lightpath/src`
- Purpose: shared native engine used by firmware and simulator.
- Notable files:
- `State.*`: frame update, emission, blending, layer lifecycle.
- `LightList.*`: layer config, light allocation, emitting, per-list state.
- `LPLight.*` and `Light.*`: particle data and per-frame updates.
- `LPObject.*`, `Intersection.*`, `Connection.*`, `Port.*`, `Model.*`, `Weight.*`: graph topology and weighted traversal.
- `EmitParams.*`, `Behaviour.*`, `Config.h`: parameter schema and behavior flags.
- `Palette.*`, `Palettes.*`: palette generation/interpolation and preset palettes.
- `objects/*`: concrete topologies (`Heptagon919`, `Heptagon3024`, `Line`, `Cross`, `Triangle`).
- `apps/simulator/src`
- Purpose: openFrameworks adapter around core.
- `main.cpp`: app entrypoint.
- `ofApp.*`: OSC receiver, command parsing, core updates, visualization.

### Key classes/modules and responsibilities

- `State` (`packages/lightpath/src/runtime/State.h`): owns active `LightList`s and pixel accumulators; orchestrates `autoEmit`, `emit`, `update`, and final pixel sampling.
- `LightList` (`packages/lightpath/src/runtime/LightList.h`): per-layer configuration/state (speed, easing, fade, palette, behavior, blend mode), light allocation/reset/update.
- `LPLight`/`Light` (`packages/lightpath/src/runtime/LPLight.h`, `packages/lightpath/src/runtime/Light.h`): per-light progression on topology and color/brightness computation.
- `LPObject` (`packages/lightpath/src/topology/LPObject.h`): topology container for intersections/connections/models and command presets.
- `Intersection`/`Connection`/`Port`: route lights across graph edges and optionally out to external sinks.
- `Model`/`Weight`: weighted routing table per model.
- `Palette`/`Palettes`: color handling, interpolation and presets, optional color-rule generation through `ofxColorTheory`.

### Data flow diagram (text)

```text
Inputs
  -> Simulator: UDP OSC messages (/emit, /note_on, /command, ...)
  -> Firmware: OSC + HTTP handlers mapped to EmitParams/layer mutations

Parse/Map
  -> EmitParams + direct layer/state changes

Core Engine (per frame)
  -> State::autoEmit()
  -> State::emit() -> get/create LightList -> choose emitter (intersection/connection)
  -> State::update()
     -> each LightList::update()
     -> each LPLight owner update (Intersection/Connection traversal)
     -> brightness/color resolution + blend into pixel accumulators

Outputs
  -> Firmware: LED drivers consume State::getPixel()
  -> Simulator: ofApp::draw() renders graph circles/segments from State::getPixel()
  -> Optional external propagation via ExternalPort + sendLightViaESPNow callback
```

### Addons/plugins and third-party integration

- `ofxColorTheory` submodule (`packages/lightpath/vendor/ofxColorTheory`) is used directly in `Palette.h` and `LightList.cpp` for color rules/schemes.
- `ofxOsc` is used in simulator (`apps/simulator/addons.make`, `apps/simulator/src/ofApp.h`) for OSC input.
- `ofxEasing.h` is vendored in core and used heavily for timing/easing.
- `FastNoise.*` is vendored in core for noise-driven behavior.

## 3) Build & Runtime

### Build systems detected

- Standalone host build for core: CMake (`packages/lightpath/CMakeLists.txt`).
- openFrameworks simulator build surfaces:
- GNU Make (`apps/simulator/Makefile` + `config.make` + `addons.make`).
- QBS (`apps/simulator/simulator.qbs`).
- Xcode project (`apps/simulator/simulator.xcodeproj`).
- Firmware build surface: PlatformIO (`firmware/esp/platformio.ini`), compiling core via symlinked source dir.

### Supported platforms (inferred)

- Confirmed targets in repo tooling:
- ESP32, ESP32-S3 via PlatformIO environments.
- macOS explicitly supported by simulator Xcode config.
- Linux/Windows simulator support is plausible through openFrameworks make/qbs but not confirmed in this audit.

### Dependencies

- Required external:
- openFrameworks checkout for simulator.
- `packages/lightpath/vendor/ofxColorTheory` submodule.
- PlatformIO + Arduino framework for firmware integration build path.
- Core-local/vendored:
- `ofxEasing.h`
- `FastNoise.*`

### Entrypoints and runtime model

- Simulator entrypoint: `apps/simulator/src/main.cpp` -> `ofRunApp(new ofApp())`.
- Simulator loop:
- `ofApp::update()` -> OSC polling -> `State::autoEmit()` -> `State::update()`.
- `ofApp::draw()` visualizes topology/pixels.
- Firmware integration entrypoint:
- `homo_deus.ino` setup/loop calls `setupState()`, `updateLEDs()`, `drawLEDs()`.
- Core runtime reused identically with Arduino timing/logging macros.

### Current build verification from this audit

- `cmake -S packages/lightpath -B packages/lightpath/build -DLIGHTPATH_CORE_BUILD_TESTS=ON` succeeded.
- `cmake --build packages/lightpath/build` succeeded (builds core static library and host test executables).
- `ctest --test-dir packages/lightpath/build --output-on-failure` succeeded (2/2 passing).
- `CC=clang CXX=clang++ cmake -S packages/lightpath -B packages/lightpath/build-asan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_ASAN=ON` succeeded.
- `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir packages/lightpath/build-asan --output-on-failure` succeeded (2/2 passing).
- `CC=clang CXX=clang++ cmake -S packages/lightpath -B packages/lightpath/build-ubsan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_UBSAN=ON` succeeded.
- `ctest --test-dir packages/lightpath/build-ubsan --output-on-failure` succeeded (2/2 passing).
- `pio run -e esp32dev -t compiledb` succeeded (core sources compile under firmware toolchain).
- `make -n` in `apps/simulator` failed locally due missing openFrameworks path at expected default (`../../../../openframeworks`).

### Typical failure modes and debugging

- Missing OF checkout or wrong `OF_ROOT` causes simulator make failure.
- Missing/uninitialized submodule (`packages/lightpath/vendor/ofxColorTheory`) causes compile failures in core palette code.
- Firmware-core path depends on symlink (`firmware/esp/src`) which can be problematic on environments that do not preserve symlinks.
- Debug visibility:
- `LP_LOG*` macros route to `Serial` (Arduino) or `ofLog` (OF).
- `LPDebugger` can visualize intersections/connections/model weights in simulator and firmware (when enabled).

## 4) Architecture & Implementation Analysis

### Update/render pipeline

- Frame-based pipeline centered in `State::update()`:
- clears RGB/divisor accumulators.
- updates each active `LightList`.
- advances each light via owner (`Intersection`/`Connection`).
- resolves color + blend mode per pixel.
- Consumers read final pixels through `State::getPixel()`.

### Timing model

- Frame-driven, variable-step timing using global `gMillis`.
- Duration defaults assume `EmitParams::DURATION_FPS = 62.5` for frame-to-ms conversions.
- No fixed-step integrator; behavior depends on caller loop cadence.

### State management model

- Centralized mutable state object (`State`) with global statics (`gMillis`, `gPerlinNoise`).
- Layer-centric model (`LightList`) plus topology model (`LPObject` + graph components).
- No ECS/scene-graph abstraction; direct object graph with pointer ownership.

### Concurrency model

- Single-threaded by design in both simulator and firmware loop usage.
- No internal locking; static pools/globals are not thread-safe.

### Memory/resource management

- Predominantly raw `new`/`delete` ownership.
- Ownership boundaries are implicit and in places incomplete (see risks in section 6).
- `State` destructor does not release `lightLists` (`packages/lightpath/src/runtime/State.h:36`), implying leaks on teardown/object switches.

### Config/parameter system

- `EmitParams` is the core command schema (model/speed/fade/palette/behavior/emit groups, etc.).
- `Config.h` defines behavior/easing/blend enums and compile-time limits.
- Firmware/simulator parse OSC/HTTP into `EmitParams` and mutate layer fields directly.

## 5) Networking / Device Control

### Protocols used (where core is integrated)

- Core itself does not open sockets or host endpoints.
- Simulator adapter uses OSC/UDP receive (`ofxOscReceiver`, port 54321 default).
- Firmware adapter exposes OSC + HTTP (and mDNS/ESP-NOW in firmware layer) and maps those controls to core state.

### Discovery mechanism

- Not in core; discovery (mDNS, etc.) exists in firmware layer.

### Message formats and key integration points

- OSC pair/triplet param contract maps to `EmitParam` IDs and `EmitParams` fields.
- HTTP control panel endpoints (e.g. `/get_layers`, `/update_palette`, `/update_behaviour_flags`) mutate `state->lightLists[...]` fields directly in firmware handlers.
- Core supports external propagation hook via `ExternalPort` + `sendLightViaESPNow` function pointer (`packages/lightpath/src/topology/Port.h:62`).

### Reliability considerations

- No delivery/ack/retry semantics in core external-port callback path.
- Emission path assumes non-empty intersection/connection sets; zero-count cases can lead to invalid modulo/division behavior in `State::getEmitter`.
- Input validation is mostly at adapter layer; core trusts many incoming values.

### Security notes (practical scope)

- Core has no authn/authz layer; trust boundary is firmware/simulator ingress.
- Firmware OSC/HTTP flows are plaintext/unauthenticated by default; core executes mapped commands directly.

## 6) Quality Signals

### Style and consistency

- Mixed modern C++ and C-style patterns (raw arrays/macros/manual memory).
- Naming and layering are mostly understandable but not uniformly encapsulated.

### Error handling patterns

- Primarily log-and-return patterns (`LP_LOGF`, `-1` returns).
- Limited defensive checks in key paths.

### Logging/telemetry

- Basic logging available through compile-time macros.
- No structured telemetry or persistent runtime diagnostics in core itself.

### Testability assessment

- Medium testability in current form:
- strong coupling to globals/macros.
- host build target now exists (`packages/lightpath/CMakeLists.txt`) and can run in CI.
- topology setup code is deterministic and testable once harness exists.

### Existing tests and coverage gaps

- Added baseline smoke tests in `packages/lightpath/tests/core_smoke_test.cpp`:
- `HashMap` const lookup behavior.
- `LightList::setDuration` assignment correctness.
- `State::emit` invalid model guard.
- `State::emit` zero-emitter guard via dummy object.
- Added regression tests in `packages/lightpath/tests/core_regression_test.cpp`:
- model-selection wrap behavior for `Line`/`Cross`/`Triangle`.
- palette repeat-wrap behavior.
- emit/update/stop lifecycle checks on `Line`, including `stopNote`, `stopAll`, and `findListById` checks.
- same-note list reuse behavior.
- deterministic blend-mode checks across all documented blend modes (`BLEND_NORMAL` through `BLEND_PIN_LIGHT`) via stacked `BgLight` layers.
- Coverage now includes a bounded long-run lifecycle stress check on `Line` (repeated emit/stop cycles + post-`stopAll` drain), but broader multi-topology stress coverage is still limited.

### Likely performance hotspots

- `State::update()` per-frame full pixel clear + blend across all active lights.
- Blend-mode switch in `State::setPixel()` can be heavy under large layer counts.
- Palette interpolation and color-rule generation can allocate/work significantly when reconfigured frequently.

### High-signal code risks identified

Resolved in Phase 1:
- Missing return in `LightList::addLightFromMsg`.
- Wrong assignment in `LightList::setDuration`.
- `LightList::doEmit` `uint8_t` truncation risk on larger lists.
- Segment/link pixel packing overflow risk in `LPLight`.
- Null dereference path in `Intersection::update`.
- Divide/modulo-by-zero risk in `State::getEmitter`.
- Recursive const `HashMap::operator[]`.
- Off-by-one model modulo in `Line`, `Cross`, `Triangle`.

Remaining high-signal risks:
- Core still relies on macro-heavy platform indirection (`Config.h` + `ofMain`/Arduino shims).
- Warning-gated CI is now in place for core host builds, but it currently relies on targeted suppression for legacy `ofxEasing` sequencing diagnostics.
- Long-run lifecycle coverage is still narrow (currently focused on `Line`); complex-topology long-run invariants are not yet covered.

## 7) Open Source Readiness Assessment

### Missing for “build core from scratch”

- Host target now exists (`packages/lightpath/CMakeLists.txt`) and is CI-validated.
- Core-specific quickstart now exists (`docs/core-build.md`) but still lacks a platform/version support matrix.
- Simulator build prerequisites depend on user-managed openFrameworks path/version without explicit pin.

### Docs needed

- `Core Quickstart`: build matrix (firmware-integrated build vs simulator), prerequisites, smoke checks.
- `Core Architecture`: object graph, update loop, ownership/lifecycle.
- `Core Troubleshooting`: submodule/symlink/OF_ROOT issues and common runtime assertions.
- `Core API reference`: `EmitParams`, behavior flags, blend modes, object model command mappings.

### Licensing concerns to verify

- `ofxColorTheory` submodule has LICENSE, but root docs should explicitly call out third-party license obligations.
- `FastNoise.*` includes MIT header, good.
- `ofxEasing.h` has no visible license header in-file; provenance/license should be documented.
- Palette set in `Palettes.*` says derived from FastLED/cpt-city; attribution and license compatibility should be documented explicitly.

### Dev tooling suggestions (without overengineering)

- Add `clang-format` config for `packages/lightpath/src` + basic format check in CI.
- Add a minimal core host test job (even one executable) once standalone target exists.
- Add optional static analysis pass (`clang-tidy` or compiler warnings-as-errors for core only).

## 8) Risks and Unknowns

### Assumptions made

- `packages/lightpath/src` is canonical core source used by both firmware and simulator.
- Firmware symlink (`firmware/esp/src`) remains the intended integration mechanism.
- Simulator is intended as the primary host runtime for visual verification of core behavior.

### Top unknowns blocking confidence and fastest confirmations

- Exact openFrameworks version compatibility:
- Fast check: pin and document a known-good OF version; run simulator build in CI with that image/cache.
- Whether simulator GNU Make path currently compiles core sources (not only headers):
- Fast check: perform full `make` with real OF checkout and inspect linked objects.
- Intended bounds for segment/link rendering buffers:
- Fast check: add assertions/tests for max connection length vs `CONNECTION_MAX_LEDS`.
- Intended model-selection semantics for non-heptagon objects:
- Fast check: run command-model mapping tests for `Line/Cross/Triangle` and validate expected model IDs.
- Ownership model for `Connection`/`Port` lifecycle:
- Fast check: run ASan-enabled host harness and inspect leaks/use-after-free across create/destroy cycles.

## 9) Roadmap (Phased, Concrete, Prioritized)

## Phase 0: documentation + reproducible build (low risk)

### Task 0.1
Goal: document core build surfaces and prerequisites clearly.
Files/modules: `docs/CORE_AUDIT_REPORT.md`, `docs/build.md` (and optional new `docs/core-build.md`).
Acceptance criteria: contributor can follow documented steps to verify core through firmware-compiledb and simulator prerequisites without guesswork.
Complexity: S.
Dependencies: none.

### Task 0.2
Goal: codify integration invariants for shared core wiring.
Files/modules: CI workflow notes, docs.
Acceptance criteria: docs explicitly state symlink requirement and submodule requirement with verification commands.
Complexity: S.
Dependencies: 0.1.

### Task 0.3
Goal: add architecture primer for newcomers.
Files/modules: docs.
Acceptance criteria: one concise architecture document covering update loop, topology, and ingress mapping to `EmitParams`.
Complexity: S.
Dependencies: 0.1.

## Phase 1: safe cleanup and modularization (low/medium risk)

### Task 1.1
Goal: fix low-risk correctness bugs without changing architecture.
Files/modules: `LightList.cpp`, `State.cpp`, `objects/Line.h`, `objects/Cross.h`, `objects/Triangle.h`, `Intersection.cpp`.
Acceptance criteria: missing-return/assignment/modulo/null checks fixed; firmware build and simulator compile still pass.
Complexity: M.
Dependencies: 0.1.

### Task 1.2
Goal: enforce safe bounds for pixel packing paths.
Files/modules: `LPLight.h`, `LPLight.cpp`, `Config.h`.
Acceptance criteria: no out-of-bounds writes in segment/link mode for worst-case connections; behavior defined when over limit.
Complexity: M.
Dependencies: 1.1.

### Task 1.3
Goal: clarify and harden ownership semantics.
Files/modules: `State.h/.cpp`, `Connection.h/.cpp`, `Model.h`.
Acceptance criteria: deterministic cleanup path documented and implemented for core-owned resources.
Complexity: M.
Dependencies: 1.1.

## Phase 2: tests, CI, reliability improvements (medium)

### Task 2.1
Goal: add standalone core host target for tests.
Files/modules: new `packages/lightpath` build files (e.g., CMake or simple host make), minimal harness.
Acceptance criteria: core compiles outside OF/Arduino adapters on CI host.
Complexity: M.
Dependencies: 1.1.
Status: completed in this phase (`packages/lightpath/CMakeLists.txt` + `packages/lightpath/host/include/ofMain.h`).

### Task 2.2
Goal: add regression tests for routing and lifecycle.
Files/modules: test sources around `State`, `LPObject`, `LightList`, `Palette`.
Acceptance criteria: tests cover emit/update expiry, model routing, palette interpolation, and edge cases (empty emit groups).
Complexity: M.
Dependencies: 2.1.
Status: complete baseline (regression tests added for lifecycle/model-wrap/palette wrap + edge-case guards, deterministic full blend-mode matrix coverage, and bounded long-run lifecycle stress coverage on `Line`).

### Task 2.3
Goal: add sanitizer/strict-warning CI lane for core host target.
Files/modules: CI workflow.
Acceptance criteria: ASan/UBSan pass on core test suite; warnings tracked as actionable.
Complexity: M.
Dependencies: 2.1, 2.2.
Status: completed (ASan + UBSan + strict warning lane added; strict lane uses `LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS=ON` with targeted suppression for legacy `ofxEasing` sequencing warnings).

## Phase 3: architectural refactors (optional/high risk)

### Task 3.1
Goal: decouple platform abstractions from `Config.h` include model.
Files/modules: `Config.h` and platform adapter headers.
Acceptance criteria: host/firmware builds select platform adapters explicitly, reducing macro-driven coupling.
Complexity: L.
Dependencies: 2.1.

### Task 3.2
Goal: modernize ownership model.
Files/modules: core graph/layer classes.
Acceptance criteria: replace fragile raw ownership with RAII containers/smart pointers where ownership is clear.
Complexity: L.
Dependencies: 3.1.

### Task 3.3
Goal: split engine from transport/control contracts.
Files/modules: engine modules vs adapter modules.
Acceptance criteria: clean boundaries where OSC/HTTP/ESP-NOW mappings live outside engine internals.
Complexity: L.
Dependencies: 3.1.

---

## Changes made

### Phase 0 docs-only

- Added `docs/core-build.md` with explicit shared-core verification steps:
- submodule init
- firmware symlink check
- firmware `compiledb` smoke command
- simulator `OF_ROOT` dry-run guidance
- Added `docs/core-architecture.md` with a concise module map, runtime flow, ownership/lifecycle notes, and adapter boundary contract.
- Added `docs/third-party-licenses.md` to track confirmed licenses and unresolved provenance follow-ups (`ofxEasing`, palette attribution).
- Added `scripts/build-core.sh` (`default|asan|ubsan|warnings|all`) for reproducible host core verification from repo root.
- Extracted core into standalone repo `git@github.com:kasparsj/lightpath.git` and reconnected monorepo `packages/lightpath` as a git submodule.
- Updated `docs/build.md` to point to `docs/core-build.md`.
- Updated `README.md` key docs list to include `docs/core-build.md`, `docs/core-architecture.md`, and `docs/third-party-licenses.md`.

### Phase 1 safe runtime fixes (implemented)

- Fixed `LightList::addLightFromMsg` missing return and `LightList::setDuration` wrong self-assignment.
- Removed `uint8_t` truncation risk in `LightList::doEmit` loop counters.
- Added emit-time guard for invalid model IDs in `State::emit`.
- Fixed `LP_OSC_REPLY` index variable usage (`index` instead of undefined `i`).
- Prevented `Behaviour` leak on early max-light rejection in `State::setupListFrom`.
- Added zero-candidate guards in `State::getEmitter` to avoid modulo/division by zero.
- Fixed null-dereference path in `Intersection::update` when no port is chosen.
- Added bounds clipping in `LPLight` segment/link pixel packing to prevent writes past `pixels`.
- Fixed off-by-one model modulo in `Line`, `Cross`, `Triangle` `getModelParams`.
- Fixed recursive const access bug in `HashMap::operator[]` and made lookup helpers const.

Validation after Phase 1:
- `pio run -e esp32dev -t compiledb` succeeded in `firmware/esp`.
- `make -n` in `apps/simulator` still fails only due missing local openFrameworks path (`../../../../openframeworks`), unchanged from pre-fix state.

### Phase 2 build/test expansion (implemented)

- Added standalone host build target:
- `packages/lightpath/CMakeLists.txt`
- Added host compatibility shim for core-only CMake builds:
- `packages/lightpath/host/include/ofMain.h`
- Added baseline smoke tests:
- `packages/lightpath/tests/core_smoke_test.cpp`
- Added regression tests:
- `packages/lightpath/tests/core_regression_test.cpp`
- Expanded regression coverage:
- deterministic blend-mode compositing checks across all documented blend modes (`BLEND_NORMAL` through `BLEND_PIN_LIGHT`) using stacked `BgLight` layers.
- lifecycle assertions for `stopNote` and `findListById` behavior.
- Added CI job for core host build + tests:
- `.github/workflows/ci.yml` (`Core (host build + tests)`)
- Added ASan lane for host core tests:
- `.github/workflows/ci.yml` (`Core (ASan)`)
- Added ASan toggle in host build:
- `LIGHTPATH_CORE_ENABLE_ASAN` in `packages/lightpath/CMakeLists.txt`
- Added UBSan lane for host core tests:
- `.github/workflows/ci.yml` (`Core (UBSan)`)
- Added UBSan toggle in host build:
- `LIGHTPATH_CORE_ENABLE_UBSAN` in `packages/lightpath/CMakeLists.txt`
- Added strict warning lane for host core tests:
- `.github/workflows/ci.yml` (`Core (Warnings)`)
- Added strict warning toggle in host build:
- `LIGHTPATH_CORE_ENABLE_STRICT_WARNINGS` in `packages/lightpath/CMakeLists.txt`
- Warning cleanup and annotation improvements:
- `LPLight` and `LightList` now have virtual destructors, `LPLight` ctor init order was fixed, object overrides were annotated, and warning-only footguns were cleaned up in `LPObject`/`LightList`.
- Improved teardown cleanup in core:
- `State` now deletes owned `LightList` instances on destruction.
- `Model` now deletes owned `Weight` instances on destruction.
- `Connection` now deletes owned `Port` instances, and `LPObject` now destroys connections before intersections to keep port cleanup order safe.
- Added long-run lifecycle regression coverage:
- `packages/lightpath/tests/core_regression_test.cpp` now verifies bounded repeated emit/update/stop cycles and post-`stopAll` drain behavior over a full `Line` traversal window, including background-slot (`lightLists[0]`) and `totalLightLists` invariants.
- Added teardown regression check:
- `packages/lightpath/tests/core_regression_test.cpp` now asserts the global `Port` pool is empty after scoped object teardown.
- Fixed list counter underflow during repeated expired background updates:
- `State::update` now keeps slot `0` allocated/non-visible without decrementing `totalLightLists` on every frame.
- Updated docs:
- `README.md`
- `docs/core-build.md`
- `docs/build.md`

Validation after Phase 2 expansion:
- `cmake -S packages/lightpath -B packages/lightpath/build -DLIGHTPATH_CORE_BUILD_TESTS=ON` succeeded.
- `cmake --build packages/lightpath/build` succeeded.
- `ctest --test-dir packages/lightpath/build --output-on-failure` succeeded (2/2).
- `CC=clang CXX=clang++ cmake -S packages/lightpath -B packages/lightpath/build-asan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_ASAN=ON` succeeded.
- `ASAN_OPTIONS=detect_leaks=0 ctest --test-dir packages/lightpath/build-asan --output-on-failure` succeeded (2/2).
- `CC=clang CXX=clang++ cmake -S packages/lightpath -B packages/lightpath/build-ubsan -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_UBSAN=ON` succeeded.
- `ctest --test-dir packages/lightpath/build-ubsan --output-on-failure` succeeded (2/2).
- `CC=clang CXX=clang++ cmake -S packages/lightpath -B packages/lightpath/build-warnings -DLIGHTPATH_CORE_BUILD_TESTS=ON -DLIGHTPATH_CORE_ENABLE_STRICT_WARNINGS=ON` succeeded.
- `ctest --test-dir packages/lightpath/build-warnings --output-on-failure` succeeded (2/2).
- `pio run -e esp32dev -t compiledb` still succeeds.
- `apps/simulator make -n` status unchanged without local openFrameworks checkout.

## Appendix A: Search-driven findings

Keyword scans performed across `packages/lightpath` and `apps/simulator`:

- Networking terms (`udp|osc|websocket|http|mdns|ssdp|espnow|tcp`): simulator OSC usage and core `ExternalPort` callback hook were found; no core-native socket stack.
- Parameter/config terms (`param|settings|json|palette|EmitParam|behaviourFlags`): `EmitParams`, palette system, behavior flags, and simulator parser paths confirmed.
- Timing terms (`frame|millis|fps|update`): frame-based timing found in `State`, `LightList`, `LPDebugger`, simulator `ofApp`.
- Framework/addon terms (`ofx|openFrameworks|glm|ofMain|Arduino`): dual-target abstraction in `Config.h`, OF integration in simulator, direct `ofxColorTheory` integration in core.

## Appendix B: Known TODO/FIXME markers

- `packages/lightpath/src/State.cpp`: `todo` on max-light overflow strategy and pixel retrieval note.
- `packages/lightpath/src/LightList.cpp`: `todo` on behavior gate and split trail handling.
- `packages/lightpath/src/objects/Heptagon919.cpp`: `todo` maxLength for all models.
- `packages/lightpath/src/objects/Heptagon3024.cpp`: `todo` maxLength for all models.
- `packages/lightpath/src/objects/HeptagonStar.h`: `todo` implement `getXYZ`.
