# Lightgraph Migration Guide

This guide covers migration to the current single-layer header layout.

## Summary

The `lightgraph/legacy*` compatibility layer was removed.
Former compatibility headers were promoted to top-level public headers.

## Breaking Changes

1. Removed:
   - `lightgraph/legacy.hpp`
   - `lightgraph/legacy/*.hpp`
2. Include paths changed:
   - `#include <lightgraph/legacy.hpp>` -> `#include <lightgraph/lightgraph.hpp>`
   - `#include <lightgraph/legacy/rendering.hpp>` -> `#include <lightgraph/integration/rendering.hpp>`
   - `#include <lightgraph/legacy/debug.hpp>` -> `#include <lightgraph/integration/debug.hpp>`
3. Legacy install option removed:
   - `LIGHTGRAPH_CORE_INSTALL_LEGACY_HEADERS`

## New Public Header Layout

Integration headers are now explicitly separated under `lightgraph/integration*`:

- `lightgraph/integration.hpp`
- `lightgraph/integration/topology.hpp`
- `lightgraph/integration/runtime.hpp`
- `lightgraph/integration/rendering.hpp`
- `lightgraph/integration/objects.hpp`
- `lightgraph/integration/factory.hpp`
- `lightgraph/integration/debug.hpp`

High-level typed stable facade:

- `lightgraph/engine.hpp`
- `lightgraph/types.hpp`
- `lightgraph/status.hpp`

## Typical Migration

### Before

```cpp
#include <lightgraph/legacy.hpp>
```

### After

```cpp
#include <lightgraph/lightgraph.hpp>
```

If you previously depended on legacy topology/runtime internals, include modules directly:

```cpp
#include <lightgraph/integration/runtime.hpp>
#include <lightgraph/integration/topology.hpp>
```

## Parent Migration Notes (MeshLED)

Updated in the same change set:

1. `/Users/kasparsj/Work2/meshled/apps/simulator/src/ofApp.h`
   - `lightgraph/legacy.hpp` -> `lightgraph/integration.hpp`
2. `/Users/kasparsj/Work2/meshled/firmware/esp/LightGraph.h`
   - `lightgraph/legacy.hpp` -> `lightgraph/integration.hpp`
3. `/Users/kasparsj/Work2/meshled/firmware/esp/WebServerLayers.h`
   - `lightgraph/legacy/rendering.hpp` -> `lightgraph/integration/rendering.hpp`
4. `/Users/kasparsj/Work2/meshled/firmware/esp/homo_deus.ino`
   - `lightgraph/legacy/debug.hpp` -> `lightgraph/integration/debug.hpp`

## Notes

- Existing source-integration aliases now live under `lightgraph::integration::*`.
- `lightgraph::Engine` (typed facade) remains the recommended entry point for new host integrations.
- Install/export package consumers should prefer the stable umbrella (`lightgraph/lightgraph.hpp`).
- CMake source-tree integrations should link `lightgraph::integration` when using
  `lightgraph/integration*.hpp` headers.
