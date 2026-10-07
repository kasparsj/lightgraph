#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

#include "lightgraph/integration.hpp"

using namespace lightgraph::integration;

namespace {
int failures = 0;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << " check failed: " #condition "\n"; \
    ++failures; } } while (false)
#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))

DrawingShape shape(DrawingShapeKind kind, std::uint8_t id, std::uint32_t rgb,
                   std::uint8_t opacity, std::initializer_list<float> params) {
    DrawingShape result;
    result.kind = kind;
    result.id = id;
    result.rgb = rgb;
    result.opacity = opacity;
    std::copy(params.begin(), params.end(), result.params.begin());
    return result;
}

std::uint16_t nearestPixel(const lightgraph::geometry::GeometryProvider& geometry, double x, double y) {
    const auto points = geometry.points();
    double best = std::numeric_limits<double>::max();
    std::uint16_t pixel = 0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const double px = (points[i].x + 1.0) / 2.0;
        const double py = (points[i].y + 1.0) / 2.0;
        const double distance = std::hypot(px - x, py - y);
        if (distance < best) { best = distance; pixel = static_cast<std::uint16_t>(i); }
    }
    return pixel;
}

void installStaticPixel(RuntimeState& state, std::uint16_t pixel, std::uint32_t rgb,
                        BlendMode blendMode = BLEND_NORMAL,
                        std::uint16_t behaviourFlags = 0) {
    LightList* list = new LightList();
    list->bindRuntimeContext(state.object.runtimeContext());
    list->behaviour = new Behaviour(behaviourFlags);
    list->model = state.object.getModel(0);
    list->blendMode = blendMode;
    list->setDuration(INFINITE_DURATION);
    list->setup(1);
    list->setPalette(Palette({static_cast<std::int64_t>(rgb)}, {0.0f}));
    list->lights[0]->setRenderedPixel(pixel);
    list->numEmitted = 1;
    state.lightLists[1] = list;
    state.totalLightLists++;
    state.totalLights++;
}

void checkDefaultsModesAndRevision() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    DrawingStatus initial = drawing.status();
    CHECK(initial.supported);
    CHECK_EQ(std::strlen(initial.generation.data()), 16u);
    CHECK_EQ(initial.revision, 0u);
    CHECK(initial.mode == DrawingMode::Palette);
    CHECK(initial.activeSource == DrawingSource::Scene);

    state.lightLists[0]->visible = false;
    CHECK_EQ(drawing.setFill(0xAA1020), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    CHECK(!state.lightLists[0]->visible);
    CHECK(drawing.status().mode == DrawingMode::Drawing);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::NoChange);
    const std::uint64_t revision = drawing.status().revision;
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::NoChange);
    CHECK_EQ(drawing.status().revision, revision);

    CHECK_EQ(drawing.layerColor(20).get(), 0xAA1020u);
    CHECK_EQ(drawing.setEnabled(false), DrawingResult::Applied);
    CHECK(drawing.mode() == DrawingMode::Palette);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(drawing.layerColor(20).get(), 0xAA1020u);
}

void checkDefaultBlackActivation() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    CHECK_EQ(drawing.setFill(0), DrawingResult::NoChange);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    for (std::uint16_t pixel : {std::uint16_t(0), std::uint16_t(127), std::uint16_t(918)}) {
        CHECK_EQ(drawing.layerColor(pixel).get(), 0u);
    }
}

void checkVisibilityMutationsAdvanceRevision() {
    Heptagon919 object;
    RuntimeState state(object);
    const std::uint64_t initial = state.drawing().status().revision;
    state.setOn(false);
    state.setOn(true);
    CHECK_EQ(state.drawing().status().revision, initial + 2);
}

void checkSceneRenderingAndRotation() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    auto geometry = object.createGeometry();
    CHECK_EQ(geometry->refresh(), lightgraph::geometry::GeometryResult::Ready);
    CHECK(geometry != nullptr && geometry->compatible());

    DrawingScene scene;
    scene.rgb1 = 0xFF0000;
    scene.shapeCount = 2;
    scene.shapes[0] = shape(DrawingShapeKind::Circle, 8, 0x0000FF, 128,
                            {0.5f, 0.5f, 1.0f});
    scene.shapes[1] = shape(DrawingShapeKind::Circle, 3, 0x00FF00, 255,
                            {0.5f, 0.5f, 1.0f});
    CHECK_EQ(drawing.replaceScene(scene), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    // IDs, rather than insertion order, define compositing. Blue ID 8 is last.
    const ColorRGB ordered = drawing.layerColor(nearestPixel(*geometry, 0.5, 0.5));
    CHECK(ordered.B >= 127 && ordered.G >= 126 && ordered.R == 0);

    DrawingScene asymmetric;
    asymmetric.rgb1 = 0;
    asymmetric.shapeCount = 1;
    asymmetric.shapes[0] = shape(DrawingShapeKind::Rectangle, 0, 0xFFFFFF, 255,
                                  {0.5f, 0.18f, 0.24f, 0.35f, 0.0f});
    CHECK_EQ(drawing.replaceScene(asymmetric), DrawingResult::Applied);
    const std::uint16_t top = nearestPixel(*geometry, 0.5, 0.18);
    const std::uint16_t left = nearestPixel(*geometry, 0.18, 0.5);
    CHECK_EQ(drawing.layerColor(top).get(), 0xFFFFFFu);
    CHECK_EQ(drawing.layerColor(left).get(), 0u);
    CHECK_EQ(drawing.setRotation(90.0f), DrawingResult::Applied);
    CHECK_EQ(drawing.layerColor(top).get(), 0u);
    CHECK_EQ(drawing.layerColor(left).get(), 0xFFFFFFu);

    DrawingScene primitives;
    primitives.rgb1 = 0;
    primitives.shapeCount = 4;
    primitives.shapes[0] = shape(DrawingShapeKind::Band, 0, 0x110000, 255,
                                  {0.5f, 0.2f, 0.0f});
    primitives.shapes[1] = shape(DrawingShapeKind::Ring, 1, 0x220000, 255,
                                  {0.5f, 0.5f, 0.25f, 0.1f});
    primitives.shapes[2] = shape(DrawingShapeKind::Line, 2, 0x330000, 255,
                                  {0.2f, 0.2f, 0.8f, 0.2f, 0.1f});
    primitives.shapes[3] = shape(DrawingShapeKind::Circle, 3, 0x440000, 255,
                                  {0.8f, 0.8f, 0.08f});
    CHECK_EQ(drawing.replaceScene(primitives), DrawingResult::Applied);
    std::array<unsigned, 4> primitiveHits{};
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel) {
        switch (drawing.layerColor(pixel).get()) {
            case 0x110000: ++primitiveHits[0]; break;
            case 0x220000: ++primitiveHits[1]; break;
            case 0x330000: ++primitiveHits[2]; break;
            case 0x440000: ++primitiveHits[3]; break;
        }
    }
    for (unsigned hits : primitiveHits) CHECK(hits > 0);
}

template <typename Heptagon>
void checkBandStripeThresholds(Heptagon& object) {
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    CHECK_EQ(drawing.setFill(0), DrawingResult::NoChange);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    const auto* geometry = drawing.geometry();
    CHECK(geometry != nullptr);

    DrawingScene scene;
    scene.rgb1 = 0x9E1B34;
    scene.shapeCount = 1;
    scene.shapes[0] = shape(DrawingShapeKind::Band, 0, 0xFFFFFF, 255,
                            {0.5f, 0.2f, 0.0f});
    CHECK_EQ(drawing.replaceScene(scene), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::NoChange);

    const auto points = geometry->points();
    double minY = std::numeric_limits<double>::max();
    double maxY = -minY;
    for (unsigned index = 0; index < 14; index += 2) {
        const double y = HeptagonGeometry::intersectionPosition(0, index).y;
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
    }
    unsigned whitePixels = 0;
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel) {
        const double height = (points[pixel].y - minY) / (maxY - minY);
        const bool white = height >= 0.4 && height < 0.6;
        CHECK_EQ(drawing.layerColor(pixel).get(), white ? 0xFFFFFFu : 0x9E1B34u);
        if (white) ++whitePixels;
    }
    CHECK(whitePixels > 0);
}

void checkGradientImageAndSourcePreservation() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    auto geometry = object.createGeometry();
    CHECK_EQ(geometry->refresh(), lightgraph::geometry::GeometryResult::Ready);
    CHECK_EQ(drawing.setGradient(0, 0, 1, 0, 0x000000, 0xFFFFFF), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    const ColorRGB middle = drawing.layerColor(nearestPixel(*geometry, 0.5, 0.5));
    CHECK(std::abs(static_cast<int>(middle.R) - 128) <= 2);

    const std::uint8_t image[] = {
        255, 0, 0,   0, 255, 0,
        0, 0, 255,   255, 255, 255,
    };
    CHECK_EQ(drawing.applyImage(1, 2, 2, image, sizeof(image)), DrawingResult::Applied);
    CHECK(drawing.status().activeSource == DrawingSource::Image);
    const std::uint16_t samplePixel = nearestPixel(*geometry, 0.5, 0.5);
    const auto points = geometry->points();
    const double sampleX = (points[samplePixel].x + 1.0) / 2.0;
    const double sampleY = (points[samplePixel].y + 1.0) / 2.0;
    const int expectedR = static_cast<int>(std::lround(255.0 *
        ((1.0 - sampleX) * (1.0 - sampleY) + sampleX * sampleY)));
    const int expectedG = static_cast<int>(std::lround(255.0 * sampleX));
    const int expectedB = static_cast<int>(std::lround(255.0 * sampleY));
    const ColorRGB bilinear = drawing.layerColor(samplePixel);
    CHECK(std::abs(static_cast<int>(bilinear.R) - expectedR) <= 1);
    CHECK(std::abs(static_cast<int>(bilinear.G) - expectedG) <= 1);
    CHECK(std::abs(static_cast<int>(bilinear.B) - expectedB) <= 1);
    CHECK_EQ(drawing.setRotation(90), DrawingResult::Applied);
    CHECK(drawing.status().activeSource == DrawingSource::Image);
    CHECK_EQ(drawing.removeShape(15), DrawingResult::Applied);
    CHECK(drawing.status().activeSource == DrawingSource::Scene);
    CHECK_EQ(drawing.removeShape(15), DrawingResult::NoChange);
    CHECK_EQ(drawing.setFill(0x123456), DrawingResult::Applied);
    CHECK(drawing.status().activeSource == DrawingSource::Scene);
    CHECK_EQ(drawing.layerColor(0).get(), 0x123456u);
}

void checkCodecValidationAndAtomicity() {
    DrawingScene scene;
    scene.baseKind = DrawingBaseKind::Gradient;
    scene.rgb1 = 0x010203;
    scene.rgb2 = 0xA0B0C0;
    scene.x1 = -1; scene.y1 = 0.25f; scene.x2 = 2; scene.y2 = 0.75f;
    scene.rotationDegrees = 17;
    scene.shapeCount = 1;
    scene.shapes[0] = shape(DrawingShapeKind::Rectangle, 4, 0xABCDEF, 99,
                            {0.4f, 0.6f, 0.2f, 0.3f, -30.0f});
    std::array<std::uint8_t, 680> bytes{};
    std::size_t written = 0;
    CHECK(DrawingSceneCodec::encode(scene, bytes.data(), bytes.size(), written));
    CHECK_EQ(written, 80u);
    DrawingScene decoded;
    CHECK(DrawingSceneCodec::decode(bytes.data(), written, decoded));
    CHECK_EQ(decoded.shapeCount, 1u);
    CHECK_EQ(decoded.shapes[0].rgb, 0xABCDEFu);
    bytes[written - 1] = 1; // unused sixth parameter must be binary zero.
    CHECK(!DrawingSceneCodec::decode(bytes.data(), written, decoded));
    CHECK(DrawingSceneCodec::encode(scene, bytes.data(), bytes.size(), written));
    bytes[8] = 0; bytes[9] = 0; bytes[10] = 1; bytes[11] = 0; // base kind 256
    CHECK(!DrawingSceneCodec::decode(bytes.data(), written, decoded));

    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    CHECK_EQ(drawing.setFill(0x778899), DrawingResult::Applied);
    const DrawingScene before = drawing.scene();
    DrawingScene invalid = before;
    invalid.shapeCount = 1;
    invalid.shapes[0] = shape(DrawingShapeKind::Circle, 0, 0xFFFFFF, 255,
                              {0, 0, -1});
    CHECK_EQ(drawing.replaceScene(invalid), DrawingResult::Invalid);
    CHECK_EQ(drawing.scene().rgb1, before.rgb1);
}

void checkMailboxesAndPreconditions() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    CHECK(drawing.setGeneration(0x1234));
    DrawingScene first; first.rgb1 = 0x111111;
    DrawingScene second; second.rgb1 = 0x222222;
    DrawingHandle oldHandle;
    CHECK_EQ(drawing.queueScene(first, oldHandle), DrawingResult::Applied);
    drawing.releaseQueued(oldHandle);
    DrawingHandle current;
    CHECK_EQ(drawing.queueScene(second, current), DrawingResult::Applied);
    CHECK_EQ(drawing.applyQueued(oldHandle), DrawingResult::Stale);
    drawing.releaseQueued(oldHandle);
    CHECK_EQ(drawing.applyQueued(current), DrawingResult::Applied);
    CHECK_EQ(drawing.scene().rgb1, 0x222222u);

    DrawingStatus status = drawing.status();
    DrawingPrecondition stale;
    stale.required = true;
    stale.generation = status.generation;
    stale.revision = status.revision - 1;
    DrawingControl visible;
    visible.hasVisible = true;
    visible.visible = false;
    CHECK_EQ(drawing.control(visible, &stale), DrawingResult::Stale);
    CHECK(state.lightLists[0]->visible);
    DrawingPrecondition currentPrecondition = stale;
    currentPrecondition.revision = status.revision;
    CHECK_EQ(drawing.control(visible, &currentPrecondition), DrawingResult::Applied);

    TopologySnapshot original;
    CHECK(object.exportSnapshot(original));
    DrawingHandle doomed;
    CHECK_EQ(drawing.queueScene(first, doomed), DrawingResult::Applied);
    Line incompatible(300);
    TopologySnapshot incompatibleSnapshot;
    CHECK(incompatible.exportSnapshot(incompatibleSnapshot));
    CHECK(object.importSnapshot(incompatibleSnapshot));
    CHECK_EQ(drawing.applyQueued(doomed), DrawingResult::Unsupported);
    CHECK(object.importSnapshot(original));
    DrawingHandle slot0;
    DrawingHandle slot1;
    CHECK_EQ(drawing.queueScene(first, slot0), DrawingResult::Applied);
    CHECK_EQ(drawing.queueScene(second, slot1), DrawingResult::Applied);
    drawing.releaseQueued(slot0);
    drawing.releaseQueued(slot1);
}

void checkChunkTransfers() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    DrawingSession session(drawing);
    std::array<std::uint8_t, 3072> image{};
    for (std::size_t i = 0; i < image.size(); ++i) image[i] = static_cast<std::uint8_t>(i);
    DrawingHandle ready;
    CHECK_EQ(session.imageChunk(7, 10, 10, 32, 32, 2, image.data() + 1536, 768, &ready),
             DrawingResult::Applied);
    CHECK_EQ(session.imageCommit(7, 11, 10, &ready), DrawingResult::Applied);
    CHECK(ready.kind == DrawingHandleKind::None);
    CHECK_EQ(session.imageChunk(7, 12, 10, 32, 32, 0, image.data(), 768, &ready),
             DrawingResult::Applied);
    CHECK_EQ(session.imageChunk(7, 13, 10, 32, 32, 3, image.data() + 2304, 768, &ready),
             DrawingResult::Applied);
    CHECK_EQ(session.imageChunk(7, 14, 10, 32, 32, 1, image.data() + 768, 768, &ready),
             DrawingResult::Applied);
    CHECK(ready.kind == DrawingHandleKind::Image);
    CHECK_EQ(drawing.status().displayedFrameId, -1);
    CHECK_EQ(session.imageChunk(7, 15, 10, 32, 32, 1, image.data() + 768, 768),
             DrawingResult::NoChange);
    CHECK_EQ(session.imageCommit(7, 16, 10), DrawingResult::NoChange);
    CHECK_EQ(drawing.applyQueued(ready), DrawingResult::Applied);
    CHECK_EQ(drawing.status().displayedFrameId, 10);
    CHECK_EQ(session.status().transferChunksReceived, 0u);
    CHECK_EQ(session.status().transferChunksExpected, 0u);
    CHECK_EQ(session.imageChunk(7, 17, 10, 32, 32, 0, image.data(), 768),
             DrawingResult::NoChange);
    CHECK_EQ(session.imageCommit(7, 18, 10), DrawingResult::NoChange);

    CHECK_EQ(session.imageChunk(7, 20, 11, 1, 1, 0, image.data(), 3), DrawingResult::Applied);
    CHECK_EQ(session.imageChunk(7, 21, 11, 1, 1, 0, image.data(), 3), DrawingResult::NoChange);
    std::uint8_t conflict[3] = {9, 9, 9};
    CHECK_EQ(session.imageChunk(7, 22, 11, 1, 1, 0, conflict, 3), DrawingResult::Invalid);
    CHECK_EQ(session.imageCommit(7, 23, 11), DrawingResult::Invalid);
    CHECK_EQ(drawing.status().displayedFrameId, 10);

    CHECK_EQ(session.imageChunk(7, 30, 12, 32, 32, 0, image.data(), 768), DrawingResult::Applied);
    CHECK_EQ(drawing.status().frameHighWater, 12);
    session.expireTransfer(530);
    CHECK_EQ(session.imageCommit(7, 531, 12), DrawingResult::Invalid);
    CHECK_EQ(drawing.status().displayedFrameId, 10);
    CHECK_EQ(drawing.status().frameHighWater, 12);
    CHECK_EQ(session.imageChunk(7, 532, 12, 32, 32, 0, image.data(), 768),
             DrawingResult::Invalid);
}

void checkBothLayoutsAndUnsupportedTopology() {
    Heptagon3024 large;
    RuntimeState largeState(large);
    CHECK(largeState.drawing().status().supported);
    CHECK_EQ(largeState.drawing().setFill(0x010203), DrawingResult::Applied);
    CHECK_EQ(largeState.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(largeState.drawing().layerColor(3000).get(), 0x010203u);

    class UnsupportedLine : public Line {
    public:
        UnsupportedLine() : Line(300) {}
        bool supportsGeometry() const override { return false; }
    } line;
    RuntimeState unsupported(line);
    CHECK(!unsupported.drawing().status().supported);
    CHECK_EQ(unsupported.drawing().setEnabled(true), DrawingResult::Unsupported);
    const std::uint8_t black[] = {0, 0, 0};
    CHECK_EQ(unsupported.drawing().applyImage(1, 1, 1, black, sizeof(black)),
             DrawingResult::Unsupported);
    CHECK_EQ(unsupported.drawing().setVisible(false), DrawingResult::Applied);
    CHECK_EQ(unsupported.drawing().clear(), DrawingResult::NoChange);
}

void checkBackgroundReplacementInvalidatesRuntime() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingScene scene;
    scene.rgb1 = 0x123456;
    DrawingHandle queued;
    CHECK_EQ(state.drawing().queueScene(scene, queued), DrawingResult::Applied);
    const auto oldGeneration = state.drawing().status().generation;
    CHECK(state.setupBgChecked(0));
    DrawingRuntime& replacement = state.drawing();
    CHECK(replacement.status().generation != oldGeneration);
    CHECK_EQ(replacement.scene().rgb1, 0u);
    CHECK_EQ(replacement.applyQueued(queued), DrawingResult::Stale);
    CHECK(state.setupBgChecked(0));
    CHECK(state.drawing().status().generation != oldGeneration);
}

void checkClearSurvivesIncompatibleTopology() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    const std::uint8_t image[] = {0xAA, 0xBB, 0xCC};
    CHECK_EQ(drawing.applyImage(1, 1, 1, image, sizeof(image)), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().layerColor(0).get(), 0xAABBCCu);

    TopologySnapshot original;
    CHECK(object.exportSnapshot(original));
    Line line(300);
    TopologySnapshot incompatible;
    CHECK(line.exportSnapshot(incompatible));
    CHECK(object.importSnapshot(incompatible));

    DrawingControl atomicClearEnable;
    atomicClearEnable.clear = true;
    atomicClearEnable.hasEnabled = true;
    atomicClearEnable.enabled = true;
    CHECK_EQ(drawing.control(atomicClearEnable), DrawingResult::Unsupported);
    const DrawingStatus rejected = drawing.status();
    CHECK(rejected.activeSource == DrawingSource::Image);
    CHECK_EQ(rejected.displayedFrameId, 1);
    CHECK_EQ(rejected.imageWidth, 1u);
    CHECK_EQ(rejected.imageHeight, 1u);

    CHECK_EQ(drawing.clear(), DrawingResult::Applied);
    const DrawingStatus cleared = drawing.status();
    CHECK(cleared.activeSource == DrawingSource::Scene);
    CHECK_EQ(cleared.displayedFrameId, -1);
    CHECK_EQ(cleared.imageWidth, 0u);
    CHECK_EQ(cleared.imageHeight, 0u);
    CHECK_EQ(cleared.rotationDegrees, 0.0f);
    CHECK_EQ(cleared.shapeCount, 0u);
    CHECK(cleared.mode == DrawingMode::Palette);

    CHECK(object.importSnapshot(original));
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().layerColor(0).get(), 0u);
}

void checkDrawingOwnsSharedGeometry() {
    Heptagon919 object;
    RuntimeState state(object);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    const auto* geometry = state.drawing().geometry();
    CHECK(geometry != nullptr);
    CHECK_EQ(geometry->points().size(), object.pixelCount);
    CHECK_EQ(state.drawing().setFill(0x123456), DrawingResult::Applied);
    CHECK(state.drawing().geometry() == geometry);
}

void checkOverlayCompositionAndOccupancy() {
    struct BlendExpectation {
        BlendMode mode;
        std::uint32_t expected;
    };
    const std::array<BlendExpectation, 16> expectations{{
        {BLEND_NORMAL, 0x964B32}, {BLEND_ADD, 0xFF9664},
        {BLEND_MULTIPLY, 0x4E1300}, {BLEND_SCREEN, 0xDD8263},
        {BLEND_OVERLAY, 0x9C2700}, {BLEND_REPLACE, 0xC83200},
        {BLEND_SUBTRACT, 0x003264}, {BLEND_DIFFERENCE, 0x643264},
        {BLEND_EXCLUSION, 0x8F6E64}, {BLEND_DODGE, 0xFF7C64},
        {BLEND_BURN, 0x390000}, {BLEND_HARD_LIGHT, 0xBC2700},
        {BLEND_SOFT_LIGHT, 0x853F27}, {BLEND_LINEAR_LIGHT, 0xF40000},
        {BLEND_VIVID_LIGHT, 0xE70000}, {BLEND_PIN_LIGHT, 0x916400},
    }};

    for (const auto& expectation : expectations) {
        Heptagon919 object;
        RuntimeState state(object);
        installStaticPixel(state, 10, 0x646464);
        DrawingRuntime& drawing = state.drawing();
        CHECK_EQ(drawing.setFill(0xC83200), DrawingResult::Applied);
        CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
        CHECK_EQ(drawing.setVisible(true), DrawingResult::NoChange);
        CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay,
                                      static_cast<std::uint8_t>(expectation.mode), true),
                 DrawingResult::Applied);
        state.update();
        const ColorRGB actual = state.getPixel(10);
        const ColorRGB expected(expectation.expected);
        CHECK(std::abs(static_cast<int>(actual.R) - expected.R) <= 2);
        CHECK(std::abs(static_cast<int>(actual.G) - expected.G) <= 2);
        CHECK(std::abs(static_cast<int>(actual.B) - expected.B) <= 2);
        CHECK_EQ(state.getPixel(11).get(), 0u);
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        installStaticPixel(state, 15, 0x000000, BLEND_ADD);
        DrawingRuntime& drawing = state.drawing();
        CHECK_EQ(drawing.setFill(0x00AA00), DrawingResult::Applied);
        CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
        CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay, BLEND_REPLACE, true),
                 DrawingResult::Applied);
        state.update();
        CHECK_EQ(state.getPixel(15).get(), 0x00AA00u);
        CHECK_EQ(state.getPixel(16).get(), 0u);
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        installStaticPixel(state, 20, 0xFFFFFF);
        DrawingRuntime& drawing = state.drawing();
        CHECK_EQ(drawing.setFill(0x000000), DrawingResult::NoChange);
        CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
        CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay, BLEND_MULTIPLY, true),
                 DrawingResult::Applied);
        state.update();
        CHECK_EQ(state.getPixel(20).get(), 0u);
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        const std::uint16_t source = 25;
        const std::uint16_t* mirrors = object.getMirroredPixels(source, nullptr, true);
        CHECK(mirrors != nullptr && mirrors[0] > 0);
        const std::uint16_t mirror = mirrors != nullptr && mirrors[0] > 0 ? mirrors[1] : source;
        installStaticPixel(state, source, 0x000000, BLEND_NORMAL, B_MIRROR_ROTATE);
        DrawingRuntime& drawing = state.drawing();
        CHECK_EQ(drawing.setFill(0xAA00AA), DrawingResult::Applied);
        CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
        CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay, BLEND_REPLACE, true),
                 DrawingResult::Applied);
        state.update();
        CHECK_EQ(state.getPixel(source).get(), 0xAA00AAu);
        CHECK_EQ(state.getPixel(mirror).get(), 0xAA00AAu);
    }

    {
        Heptagon919 object;
        RuntimeState state(object);
        BgLight* blackLayer = new BgLight();
        blackLayer->bindRuntimeContext(object.runtimeContext());
        blackLayer->model = object.getModel(0);
        blackLayer->setDuration(INFINITE_DURATION);
        blackLayer->setup(object.pixelCount);
        blackLayer->setPalette(Palette({0x000000}, {0.0f}));
        state.lightLists[1] = blackLayer;
        state.totalLightLists++;
        DrawingRuntime& drawing = state.drawing();
        CHECK_EQ(drawing.setFill(0x0088CC), DrawingResult::Applied);
        CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
        CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay, BLEND_REPLACE, true),
                 DrawingResult::Applied);
        state.update();
        CHECK_EQ(state.getPixel(100).get(), 0x0088CCu);
    }
}

void checkCompositionStatusAndRevision() {
    Heptagon919 object;
    RuntimeState state(object);
    DrawingRuntime& drawing = state.drawing();
    const DrawingStatus initial = drawing.status();
    CHECK(initial.compositingSupported);
    CHECK(initial.placement == DrawingPlacement::Background);
    CHECK_EQ(initial.blendMode, BLEND_NORMAL);
    CHECK(!initial.clipToContent);
    CHECK_EQ(initial.brightness, 255u);
    CHECK_EQ(drawing.setComposite(DrawingPlacement::Overlay, BLEND_MULTIPLY, true),
             DrawingResult::Applied);
    const DrawingStatus composite = drawing.status();
    CHECK(composite.placement == DrawingPlacement::Overlay);
    CHECK_EQ(composite.blendMode, BLEND_MULTIPLY);
    CHECK(composite.clipToContent);
    CHECK_EQ(composite.revision, initial.revision + 1);

    BgLight* background = state.lightLists[0]->asBgLight();
    background->maxBri = 127;
    background->blendMode = BLEND_SCREEN;
    const DrawingStatus reconciled = drawing.status();
    CHECK_EQ(reconciled.brightness, 127u);
    CHECK_EQ(reconciled.blendMode, BLEND_SCREEN);
    CHECK_EQ(reconciled.revision, composite.revision + 1);
}

void checkUnsupportedRefreshFallsBackBeforeComposition() {
    Heptagon919 object;
    RuntimeState state(object);
    BgLight* background = state.lightLists[0]->asBgLight();
    background->setPalette(Palette({0x00FF00}, {0.0f}));

    DrawingRuntime& drawing = state.drawing();
    CHECK_EQ(drawing.setFill(0xFF0000), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    DrawingHandle queued;
    CHECK_EQ(drawing.queueScene(DrawingScene{}, queued), DrawingResult::Applied);
    CHECK(drawing.validQueued(queued));
    DrawingRuntime::ImageLease lease;
    CHECK_EQ(drawing.leaseImage(1, 1, 1, lease), DrawingResult::Applied);
    lease.bytes[0] = 0x11;
    lease.bytes[1] = 0x22;
    lease.bytes[2] = 0x33;
    DrawingHandle queuedImage;
    CHECK_EQ(drawing.submitImage(lease, &queuedImage), DrawingResult::Applied);
    CHECK(drawing.validQueued(queuedImage));
    const std::uint64_t revision = drawing.status().revision;

    CHECK(object.addIntersection(new Intersection(2, 10, -1, GROUP1)) != nullptr);
    state.update();

    const DrawingStatus status = drawing.status();
    CHECK(status.mode == DrawingMode::Palette);
    CHECK_EQ(status.revision, revision + 1);
    CHECK(std::strcmp(status.lastRejection.data(), "unsupported_geometry") == 0);
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel) {
        CHECK_EQ(state.getPixel(pixel).get(), 0x00FF00u);
    }
    CHECK_EQ(drawing.applyQueued(queued), DrawingResult::Stale);
    CHECK(!drawing.validQueued(queued));
    CHECK(!drawing.queuedImage());
    CHECK(!drawing.validQueued(queuedImage));
    CHECK_EQ(drawing.applyQueued(queuedImage), DrawingResult::Stale);
}

void checkCanonicalizationAndQueuedHandlePrecedence() {
    Heptagon919 object;
    RuntimeState state(object);
    auto& drawing = state.drawing();
    DrawingScene scene;
    scene.rotationDegrees = -360.0f;
    scene.shapeCount = 2;
    scene.shapes[0] = shape(DrawingShapeKind::Circle, 8, 0xFF0000, 255, {0.5f, 0.5f, 0.2f});
    scene.shapes[1] = shape(DrawingShapeKind::Circle, 2, 0x00FF00, 255, {0.5f, 0.5f, 0.1f});
    CHECK_EQ(drawing.replaceScene(scene), DrawingResult::Applied);
    DrawingScene equivalent = scene;
    equivalent.rotationDegrees = 360.0f;
    std::swap(equivalent.shapes[0], equivalent.shapes[1]);
    CHECK_EQ(drawing.replaceScene(equivalent), DrawingResult::NoChange);
    DrawingHandle first, second;
    CHECK_EQ(drawing.queueScene(equivalent, first), DrawingResult::Applied);
    CHECK_EQ(drawing.applyQueued(first), DrawingResult::NoChange);
    equivalent.rotationDegrees = -0.0f;
    CHECK_EQ(drawing.queueScene(equivalent, first), DrawingResult::Applied);
    CHECK_EQ(drawing.queueScene(scene, second), DrawingResult::Applied);
    DrawingScene invalid = scene;
    invalid.shapeCount = static_cast<std::uint8_t>(invalid.shapes.size() + 1);
    DrawingHandle invalidHandle;
    CHECK_EQ(drawing.queueScene(invalid, invalidHandle), DrawingResult::Invalid);
    CHECK(std::strcmp(drawing.status().lastRejection.data(), "invalid_scene") == 0);
    DrawingHandle unknown = first;
    unknown.kind = static_cast<DrawingHandleKind>(255);
    CHECK_EQ(drawing.applyQueued(unknown), DrawingResult::Invalid);
    CHECK(!drawing.validQueued(unknown));
    drawing.releaseQueued(unknown);
    CHECK(drawing.validQueued(first));
    ++unknown.runtimeGeneration;
    CHECK_EQ(drawing.applyQueued(unknown), DrawingResult::Stale);
    for (DrawingHandle malformed : {first, second}) {
        malformed.slot = 255;
        CHECK_EQ(drawing.applyQueued(malformed), DrawingResult::Stale);
        drawing.releaseQueued(malformed);
        CHECK(drawing.validQueued(first));
        CHECK(drawing.validQueued(second));
    }
    DrawingHandle stale = first;
    ++stale.slotGeneration;
    CHECK_EQ(drawing.applyQueued(stale), DrawingResult::Stale);
    drawing.releaseQueued(stale);
    CHECK(drawing.validQueued(first));
    drawing.releaseQueued(first);
    drawing.releaseQueued(second);

    DrawingRuntime::ImageLease lease;
    CHECK_EQ(drawing.leaseImage(1, 1, 1, lease), DrawingResult::Applied);
    std::fill_n(lease.bytes, lease.size, 0x42);
    DrawingHandle image;
    CHECK_EQ(drawing.submitImage(lease, &image), DrawingResult::Applied);
    stale = image;
    ++stale.slotGeneration;
    CHECK_EQ(drawing.applyQueued(stale), DrawingResult::Stale);
    drawing.releaseQueued(stale);
    CHECK(drawing.validQueued(image));
    stale = image;
    stale.slot = 255;
    CHECK_EQ(drawing.applyQueued(stale), DrawingResult::Stale);
    drawing.releaseQueued(stale);
    CHECK(drawing.validQueued(image));
    drawing.releaseQueued(image);
    CHECK_EQ(drawing.leaseImage(2, 1, 1, lease), DrawingResult::Applied);
    std::fill_n(lease.bytes, lease.size, 0x43);
    DrawingHandle replacement;
    CHECK_EQ(drawing.submitImage(lease, &replacement), DrawingResult::Applied);
    drawing.releaseQueued(image);
    CHECK(drawing.validQueued(replacement));
    CHECK_EQ(drawing.applyQueued(replacement), DrawingResult::Applied);
}

} // namespace

int main() {
    checkCanonicalizationAndQueuedHandlePrecedence();
    checkDefaultsModesAndRevision();
    checkDefaultBlackActivation();
    checkVisibilityMutationsAdvanceRevision();
    checkSceneRenderingAndRotation();
    {
        Heptagon919 object;
        checkBandStripeThresholds(object);
    }
    {
        Heptagon3024 object;
        checkBandStripeThresholds(object);
    }
    checkGradientImageAndSourcePreservation();
    checkCodecValidationAndAtomicity();
    checkMailboxesAndPreconditions();
    checkChunkTransfers();
    checkBothLayoutsAndUnsupportedTopology();
    checkBackgroundReplacementInvalidatesRuntime();
    checkClearSurvivesIncompatibleTopology();
    checkDrawingOwnsSharedGeometry();
    checkOverlayCompositionAndOccupancy();
    checkCompositionStatusAndRevision();
    checkUnsupportedRefreshFallsBackBeforeComposition();
    if (failures != 0) {
        std::cerr << failures << " drawing runtime test(s) failed\n";
        return 1;
    }
    std::cout << "All drawing runtime tests passed\n";
    return 0;
}
