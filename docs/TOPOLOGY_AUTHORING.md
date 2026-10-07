# Topology Authoring

## Built-in Topology Objects

The codebase currently defines these topology objects:

- `Heptagon919`: 919-pixel heptagon/star layout with multiple routing models.
- `Heptagon3024`: 3024-pixel heptagon/star layout with explicit gap mapping for non-LED spans.
- `Line`: a single-loop line topology with default and bounce routing models.
- `Cross`: intersecting horizontal/vertical paths with directional model variants.
- `Triangle`: three-sided topology with clockwise/counter-clockwise behavior models.
- `HeptagonStar`: shared base topology used by the `Heptagon919` and `Heptagon3024` specializations (source-integration layer).

For the stable high-level `lightgraph::Engine` API, currently supported `ObjectType` values are:
`Heptagon919`, `Heptagon3024`, `Line`, `Cross`, and `Triangle`.

## Defining New Topologies

To add a new object topology:

1. Create `src/objects/<YourObject>.h` and `.cpp` with a class that inherits `TopologyObject`.
2. In the constructor, pass `pixelCount` to `TopologyObject(pixelCount)` and call `setup()`.
3. In `setup()`, define structure with `addIntersection(...)`, `addConnection(...)`, and optional `addBridge(...)`.
4. Add routing behavior with one or more `Model` instances and set per-path `Weight`s via `model->put(...)`.
5. Optionally override helpers like `getModelParams(...)`, `getParams(...)`, and mirror behavior methods.
6. Optionally provide drawing coordinates by overriding `supportsGeometry()` and
   `createGeometry()` with a `lightgraph::geometry::GeometryProvider`.
7. Expose the object through integration and/or stable API surfaces:
   - shared registry: update `include/lightgraph/internal/object_factory.hpp`
   - integration aliases: update `include/lightgraph/integration/objects.hpp`
   - stable engine enum: update `include/lightgraph/types.hpp` (`ObjectType`)

Minimal scaffold:

```cpp
class MyObject : public TopologyObject {
  public:
    explicit MyObject(uint16_t pixelCount) : TopologyObject(pixelCount) { setup(); }

    EmitParams getModelParams(int model) const override {
        return EmitParams(model, Random::randomSpeed());
    }

  private:
    void setup() {
        Model::maxWeights = 2;
        Model* base = addModel(new Model(0, 10, GROUP1));

        Connection* bridge = addBridge(pixelCount - 1, 0, GROUP1);
        Connection* segment =
            addConnection(new Connection(bridge->to, bridge->from, GROUP1, pixelCount - 3));

        base->put(bridge, 0);
        base->put(segment, 10);
    }
};
```

## Drawing Geometry

Drawing consumes a topology-neutral `GeometryProvider`; it does not inspect
connections, strip boundaries, or concrete object classes. A provider supplies:

- one finite `(x, y)` coordinate for every logical pixel;
- outline vertices used to derive projected band extents;
- compatibility and transactional `refresh()` behavior;
- the topology revision represented by its current published coordinates.

Coordinates use the normalized `[-1, 1]` front view, with positive Y pointing
down. A projected outline with zero width maps every pixel to normalized band
position `0.5` on that axis.

`refresh()` must build replacement storage before publishing it. If a topology
mutation is incompatible, or admission/allocation fails, retain the previous
valid mapping. Return `Unsupported`, `AdmissionDenied`, or `AllocationFailed`
as appropriate so drawing can retry a temporary resource failure later.

Built-in conventions are:

- `Line`: pixel order runs left-to-right from `(-1, 0)` to `(1, 0)`.
- `Cross`: the horizontal strip runs left-to-right and the vertical strip runs
  top-to-bottom; their configured crossing indices map to `(0, 0)`.
- `Triangle`: segment order traces `(0, -1)`, `(sqrt(3)/2, 0.5)`,
  `(-sqrt(3)/2, 0.5)`, then returns to the first vertex.
- The heptagon providers preserve their established front-view mapping,
  physical-strip gaps, orphan assignments, and seven-vertex outline.

Geometry is acquired lazily by drawing. `supportsGeometry()` means the topology
can provide it; temporary memory pressure should therefore report `Busy` to a
drawing mutation and remain retryable, rather than permanently marking the
topology unsupported.

### Edited built-in layouts

Line endpoints, Cross endpoints/crossing, and Triangle corners retain their
object-owned role IDs from construction. Refresh resolves those IDs from the
current topology, including same-ID snapshot imports, rather than retaining
intersection pointers. Logical pixel count stays fixed; Cross retains its two
physical strip domains.

Edited endpoints and segment joins become interpolation anchors at the existing
outline vertices. Cross crossing indices remain at `(0, 0)`. Pixels outside a
span clamp to its endpoint; uncovered pixels use the nearest anchor within the
same strip or perimeter order, with the lower logical index breaking a tie.
Missing roles, invalid ordering/ranges, incompatible connectivity, and conflicting
anchor assignments return `Unsupported`. Refresh admission/allocation failures
retain previous coordinates and retry later. Unchanged layouts retain their
established coordinates.

Canonical anchor roles stay in `GROUP1`. Port capacity may grow for external
attachments, but must retain the role minimum (Line endpoints 2, Cross endpoints
3, Cross center 4, Triangle corners 2). Endpoint roles have no second pixel;
Cross center and Triangle corners retain their paired pixel anchors. Role checks
run even when a port-only edit leaves the topology revision unchanged. A
single-pixel Line retains its midpoint coordinate `(0, 0)`.

Geometry connectivity is defined by ordered anchor indices. Moving an
intersection through the topology mutation API removes its attached routing
connections; geometry does not require those deleted edges to be recreated.
Routing changes remain the caller's responsibility.
