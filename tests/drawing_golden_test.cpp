#include <algorithm>
#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include "lightgraph/integration.hpp"

using namespace lightgraph::integration;

namespace {

int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            std::cerr << __FILE__ << ':' << __LINE__ << " check failed: " #condition "\n";         \
            ++failures;                                                                            \
        }                                                                                          \
    } while (false)
#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))

DrawingShape shape(DrawingShapeKind kind, std::uint8_t id, std::uint32_t rgb, std::uint8_t opacity,
                   std::initializer_list<float> params) {
    DrawingShape result;
    result.kind = kind;
    result.id = id;
    result.rgb = rgb;
    result.opacity = opacity;
    std::copy(params.begin(), params.end(), result.params.begin());
    return result;
}

void addByte(std::uint64_t& hash, std::uint8_t value) {
    hash ^= value;
    hash *= UINT64_C(1099511628211);
}

template <typename Sample> std::uint64_t pixelHash(std::uint16_t count, Sample sample) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (std::uint16_t pixel = 0; pixel < count; ++pixel) {
        const ColorRGB color = sample(pixel);
        addByte(hash, color.R);
        addByte(hash, color.G);
        addByte(hash, color.B);
    }
    return hash;
}

template <typename Heptagon> std::uint64_t sceneFixture(Heptagon& object) {
    RuntimeState state(object);
    DrawingScene scene;
    scene.baseKind = DrawingBaseKind::Gradient;
    scene.rgb1 = 0x18052A;
    scene.rgb2 = 0xE39A41;
    scene.x1 = 0.13f;
    scene.y1 = 0.91f;
    scene.x2 = 0.88f;
    scene.y2 = 0.07f;
    scene.rotationDegrees = 37.0f;
    scene.shapeCount = 5;
    scene.shapes[0] = shape(DrawingShapeKind::Band, 7, 0x36D7B7, 137, {0.42f, 0.19f, 23.0f});
    scene.shapes[1] = shape(DrawingShapeKind::Circle, 2, 0xE34264, 211, {0.28f, 0.31f, 0.22f});
    scene.shapes[2] = shape(DrawingShapeKind::Ring, 9, 0xF4F1BB, 173, {0.68f, 0.62f, 0.29f, 0.08f});
    scene.shapes[3] =
        shape(DrawingShapeKind::Line, 4, 0x4062BB, 231, {0.12f, 0.78f, 0.83f, 0.22f, 0.055f});
    scene.shapes[4] =
        shape(DrawingShapeKind::Rectangle, 12, 0x59C3C3, 189, {0.59f, 0.37f, 0.31f, 0.18f, -21.0f});
    CHECK_EQ(state.drawing().replaceScene(scene), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    return pixelHash(object.pixelCount,
                     [&](std::uint16_t pixel) { return state.drawing().sampleColor(pixel); });
}

template <typename Heptagon> std::uint64_t imageFixture(Heptagon& object) {
    RuntimeState state(object);
    std::array<std::uint8_t, 8 * 8 * 3> image{};
    for (std::uint8_t y = 0; y < 8; ++y) {
        for (std::uint8_t x = 0; x < 8; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * 8 + x) * 3;
            image[offset] = static_cast<std::uint8_t>(x * 29 + y * 3);
            image[offset + 1] = static_cast<std::uint8_t>(y * 31 + x * 2);
            image[offset + 2] = static_cast<std::uint8_t>((x * 17) ^ (y * 23));
        }
    }
    CHECK_EQ(state.drawing().applyImage(41, 8, 8, image.data(), image.size()),
             DrawingResult::Applied);
    CHECK_EQ(state.drawing().setRotation(90.0f), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    return pixelHash(object.pixelCount,
                     [&](std::uint16_t pixel) { return state.drawing().sampleColor(pixel); });
}

void installStaticPixel(RuntimeState& state, std::uint16_t pixel, std::uint32_t rgb,
                        std::uint16_t behaviourFlags = 0) {
    LightList* list = new LightList();
    list->bindRuntimeContext(state.object.runtimeContext());
    list->behaviour = new Behaviour(behaviourFlags);
    list->model = state.object.getModel(0);
    list->blendMode = BLEND_NORMAL;
    list->setDuration(INFINITE_DURATION);
    list->setup(1);
    list->setPalette(Palette({static_cast<std::int64_t>(rgb)}, {0.0f}));
    list->lights[0]->setRenderedPixel(pixel);
    list->numEmitted = 1;
    const std::uint8_t slot = state.totalLightLists;
    state.lightLists[slot] = list;
    ++state.totalLightLists;
    ++state.totalLights;
}

std::uint64_t compositionFixture() {
    Heptagon919 object;
    RuntimeState state(object);
    installStaticPixel(state, 25, 0x604020, B_MIRROR_ROTATE);
    installStaticPixel(state, 301, 0x203060);
    DrawingScene scene;
    scene.rgb1 = 0xC02080;
    scene.shapeCount = 1;
    scene.shapes[0] = shape(DrawingShapeKind::Circle, 3, 0x10E080, 191, {0.5f, 0.5f, 0.47f});
    CHECK_EQ(state.drawing().replaceScene(scene), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setComposite(DrawingPlacement::Overlay, BLEND_SCREEN, true),
             DrawingResult::Applied);
    state.update();
    return pixelHash(object.pixelCount, [&](std::uint16_t pixel) { return state.getPixel(pixel); });
}

template <typename Heptagon> std::uint64_t refreshedFixture(Heptagon& object) {
    RuntimeState state(object);
    CHECK_EQ(state.drawing().setGradient(0.14f, 0.21f, 0.91f, 0.72f, 0x123456, 0xFEDCBA),
             DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    TopologySnapshot snapshot;
    CHECK(object.exportSnapshot(snapshot));
    CHECK(object.importSnapshot(snapshot));
    (void)state.drawing().refreshGeometry();
    const std::uint64_t after = pixelHash(
        object.pixelCount, [&](std::uint16_t pixel) { return state.drawing().sampleColor(pixel); });
    return after;
}

void expectHash(const char* name, std::uint64_t actual, std::uint64_t expected) {
    if (actual != expected) {
        std::cerr << name << " hash: 0x" << std::hex << std::setw(16) << std::setfill('0') << actual
                  << std::dec << '\n';
        ++failures;
    }
}

} // namespace

int main() {
    Heptagon919 smallScene;
    Heptagon3024 largeScene;
    Heptagon919 smallImage;
    Heptagon3024 largeImage;
    Heptagon919 smallRefresh;
    Heptagon3024 largeRefresh;

    expectHash("heptagon919 scene", sceneFixture(smallScene), UINT64_C(0x24ae4b918c18c9fa));
    expectHash("heptagon3024 scene", sceneFixture(largeScene), UINT64_C(0x96df751333fa0b20));
    expectHash("heptagon919 image", imageFixture(smallImage), UINT64_C(0xe8ff7036e6eba857));
    expectHash("heptagon3024 image", imageFixture(largeImage), UINT64_C(0x8d3c82532099cfec));
    expectHash("heptagon919 composition", compositionFixture(), UINT64_C(0xa7daf2beecc54134));
    expectHash("heptagon919 refresh", refreshedFixture(smallRefresh), UINT64_C(0x6132683fc92b99ca));
    expectHash("heptagon3024 refresh", refreshedFixture(largeRefresh),
               UINT64_C(0xdc1dafdbb1ffb7e5));

    if (failures != 0) {
        std::cerr << failures << " drawing golden fixture(s) failed\n";
        return 1;
    }
    std::cout << "drawing golden fixtures passed\n";
    return 0;
}
