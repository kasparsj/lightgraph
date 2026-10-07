#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

#include "lightgraph/integration.hpp"

using namespace lightgraph::integration;

namespace {

int failures = 0;

#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << " check failed: " #condition "\n"; \
    ++failures; } } while (false)
#define CHECK_EQ(actual, expected) CHECK((actual) == (expected))

DrawingShape band(float center, float width, float angle = 0.0f) {
    DrawingShape value;
    value.kind = DrawingShapeKind::Band;
    value.id = 1;
    value.rgb = 0xFF0000;
    value.opacity = 255;
    value.params = {center, width, angle, 0.0f, 0.0f, 0.0f};
    return value;
}

std::pair<std::uint16_t, std::uint16_t> horizontalExtrema(
        const lightgraph::geometry::GeometryProvider& geometry) {
    const auto points = geometry.points();
    std::uint16_t minimum = 0;
    std::uint16_t maximum = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (points[i].x < points[minimum].x) minimum = static_cast<std::uint16_t>(i);
        if (points[i].x > points[maximum].x) maximum = static_cast<std::uint16_t>(i);
    }
    return {minimum, maximum};
}

template <typename Object>
void exerciseDrawing(Object& object, bool degenerateBand) {
    RuntimeState state(object);
    CHECK(state.ready());
    DrawingRuntime& drawing = state.drawing();

    CHECK_EQ(drawing.setFill(0x123456), DrawingResult::Applied);
    CHECK_EQ(drawing.setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(drawing.layerColor(0).get(), 0x123456u);
    const auto* geometry = drawing.geometry();
    CHECK(geometry != nullptr);
    if (!geometry) return;
    CHECK_EQ(geometry->points().size(), object.pixelCount);
    for (const auto& point : geometry->points()) {
        CHECK(std::isfinite(point.x));
        CHECK(std::isfinite(point.y));
    }

    const auto extrema = horizontalExtrema(*geometry);
    CHECK_EQ(drawing.setGradient(0.0f, 0.0f, 1.0f, 0.0f, 0x000000, 0xFFFFFF),
             DrawingResult::Applied);
    CHECK(drawing.layerColor(extrema.first).R < 32);
    CHECK(drawing.layerColor(extrema.second).R > 223);

    const std::uint8_t image[] = {
        255, 0, 0,   0, 255, 0,
        0, 0, 255,   255, 255, 255,
    };
    CHECK_EQ(drawing.applyImage(1, 2, 2, image, sizeof(image)), DrawingResult::Applied);
    CHECK(drawing.layerColor(extrema.first).G < 32);
    CHECK(drawing.layerColor(extrema.second).G > 223);

    DrawingScene scene;
    scene.rgb1 = 0;
    scene.shapeCount = 1;
    scene.shapes[0] = band(0.5f, 0.2f);
    CHECK_EQ(drawing.replaceScene(scene), DrawingResult::Applied);
    std::size_t selected = 0;
    for (std::uint16_t pixel = 0; pixel < object.pixelCount; ++pixel) {
        if (drawing.layerColor(pixel).get() == 0xFF0000u) ++selected;
    }
    CHECK(selected > 0);
    if (degenerateBand) CHECK_EQ(selected, object.pixelCount);
    else CHECK(selected < object.pixelCount);
}

void checkLineRotation() {
    Line object(300);
    RuntimeState state(object);
    DrawingScene scene;
    scene.rgb1 = 0;
    scene.shapeCount = 1;
    scene.shapes[0] = band(0.5f, 0.2f, 90.0f);
    CHECK_EQ(state.drawing().replaceScene(scene), DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().layerColor(0).get(), 0u);
    CHECK_EQ(state.drawing().layerColor(299).get(), 0u);
    CHECK_EQ(state.drawing().layerColor(149).get(), 0xFF0000u);
}

void checkCrossIntersection() {
    Cross object(288);
    RuntimeState state(object);
    CHECK_EQ(state.drawing().setGradient(0, 0, 1, 1, 0x000000, 0xFFFFFF),
             DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().layerColor(72).get(), state.drawing().layerColor(216).get());
}

void checkTriangleJoins() {
    Triangle object(900);
    RuntimeState state(object);
    CHECK_EQ(state.drawing().setGradient(0, 0, 1, 1, 0x102030, 0xE0D0C0),
             DrawingResult::Applied);
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK_EQ(state.drawing().layerColor(299).get(), state.drawing().layerColor(300).get());
    CHECK_EQ(state.drawing().layerColor(599).get(), state.drawing().layerColor(600).get());
}

class CustomGeometry final : public lightgraph::geometry::GeometryProvider {
public:
    explicit CustomGeometry(TopologyObject& object) : GeometryProvider(object) {}
    lightgraph::geometry::GeometryResult refresh() noexcept override {
        geometryRevision_ = sourceTopologyRevision();
        return lightgraph::geometry::GeometryResult::Ready;
    }
    bool compatible() const noexcept override { return true; }
    ArrayView<Point> points() const noexcept override { return {points_.data(), points_.size()}; }
    ArrayView<Point> outline() const noexcept override { return {points_.data(), points_.size()}; }
private:
    std::array<Point, 4> points_{{{-1, 0}, {0, -1}, {1, 0}, {0, 1}}};
};

class CustomLine final : public Line {
public:
    CustomLine() : Line(4) {}
    std::unique_ptr<lightgraph::geometry::GeometryProvider> createGeometry() override {
        setGeometryCreationResult(lightgraph::geometry::GeometryResult::Ready);
        return std::unique_ptr<lightgraph::geometry::GeometryProvider>(new CustomGeometry(*this));
    }
};

bool denyGeometry(lightgraph::memory::Operation operation,
                  const lightgraph::memory::Estimate&, void* user) noexcept {
    return operation != lightgraph::memory::Operation::Geometry ||
           !*static_cast<bool*>(user);
}

void checkLazyAdmissionRecovery() {
    Line object(300);
    RuntimeState state(object);
    CHECK(state.ready());
    const auto generation = state.drawing().status().generation;
    bool deny = true;
    object.runtimeContext().memoryAdmission = {denyGeometry, &deny};
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Busy);
    CHECK(std::strcmp(state.drawing().status().lastRejection.data(), "insufficient_heap") == 0);
    deny = false;
    CHECK_EQ(state.drawing().setEnabled(true), DrawingResult::Applied);
    CHECK(state.drawing().status().generation == generation);
    CHECK_EQ(state.drawing().layerColor(0).get(), 0u);
}

} // namespace

int main() {
    {
        Heptagon919 object;
        exerciseDrawing(object, false);
    }
    {
        Heptagon3024 object;
        exerciseDrawing(object, false);
    }
    {
        Line object(300);
        exerciseDrawing(object, true);
    }
    {
        Cross object(288);
        exerciseDrawing(object, false);
    }
    {
        Triangle object(900);
        exerciseDrawing(object, false);
    }
    {
        CustomLine object;
        exerciseDrawing(object, false);
    }
    checkLineRotation();
    checkCrossIntersection();
    checkTriangleJoins();
    checkLazyAdmissionRecovery();

    if (failures != 0) {
        std::cerr << failures << " built-in drawing test(s) failed\n";
        return 1;
    }
    std::cout << "All built-in drawing tests passed\n";
    return 0;
}
