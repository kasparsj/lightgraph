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
6. Expose the object through integration and/or stable API surfaces:
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
