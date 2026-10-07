#include "DrawingRuntimeInternal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include "../geometry/GeometryProvider.h"

namespace lightgraph::drawing::detail {
namespace {

constexpr double kPi = 3.1415926535897932384626433832795;

bool sameShape(const DrawingShape& a, const DrawingShape& b) {
    if (a.kind != b.kind || a.id != b.id || a.rgb != b.rgb || a.opacity != b.opacity) {
        return false;
    }
    for (std::size_t i = 0; i < a.params.size(); ++i) {
        if (!sameFloat(a.params[i], b.params[i]))
            return false;
    }
    return true;
}

} // namespace

bool finite(float value) { return std::isfinite(static_cast<double>(value)); }

bool validRgb(std::uint32_t rgb) { return rgb <= 0xFFFFFFu; }

float normalizedDegrees(float degrees) {
    const float value = std::fmod(degrees, 360.0f);
    return value == -0.0f ? 0.0f : value;
}

bool sameFloat(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

bool sameScene(const DrawingScene& a, const DrawingScene& b) {
    if (a.baseKind != b.baseKind || a.rgb1 != b.rgb1 || a.rgb2 != b.rgb2 ||
        !sameFloat(a.x1, b.x1) || !sameFloat(a.y1, b.y1) || !sameFloat(a.x2, b.x2) ||
        !sameFloat(a.y2, b.y2) || !sameFloat(a.rotationDegrees, b.rotationDegrees) ||
        a.shapeCount != b.shapeCount) {
        return false;
    }
    for (std::size_t i = 0; i < a.shapeCount; ++i) {
        if (!sameShape(a.shapes[i], b.shapes[i]))
            return false;
    }
    return true;
}

bool canonicalizeScene(const DrawingScene& value, DrawingScene& canonical) {
    if (!DrawingRuntime::validateScene(value))
        return false;
    canonical = value;
    canonical.rotationDegrees = normalizedDegrees(canonical.rotationDegrees);
    std::sort(canonical.shapes.begin(), canonical.shapes.begin() + canonical.shapeCount,
              [](const DrawingShape& a, const DrawingShape& b) { return a.id < b.id; });
    return true;
}

Vec2 rotate(Vec2 point, double degrees) {
    const double radians = degrees * kPi / 180.0;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return {c * point.x + s * point.y, -s * point.x + c * point.y};
}

Rgb unpack(std::uint32_t rgb) {
    return {static_cast<std::uint8_t>(rgb >> 16), static_cast<std::uint8_t>(rgb >> 8),
            static_cast<std::uint8_t>(rgb)};
}

Rgb composite(Rgb destination, Rgb source, std::uint8_t opacity) {
    const unsigned inverse = 255u - opacity;
    return {
        static_cast<std::uint8_t>((source.r * opacity + destination.r * inverse + 127u) / 255u),
        static_cast<std::uint8_t>((source.g * opacity + destination.g * inverse + 127u) / 255u),
        static_cast<std::uint8_t>((source.b * opacity + destination.b * inverse + 127u) / 255u),
    };
}

double distanceToSegment(Vec2 p, Vec2 a, Vec2 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared == 0.0)
        return std::hypot(p.x - a.x, p.y - a.y);
    const double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSquared, 0.0, 1.0);
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
}

} // namespace lightgraph::drawing::detail

namespace lightgraph::drawing {

detail::Rgb DrawingRuntime::Impl::baseColor(const DrawingScene& value, detail::Vec2 point) const {
    if (value.baseKind == DrawingBaseKind::Solid)
        return detail::unpack(value.rgb1);
    const double dx = static_cast<double>(value.x2) - static_cast<double>(value.x1);
    const double dy = static_cast<double>(value.y2) - static_cast<double>(value.y1);
    const double denom = dx * dx + dy * dy;
    const double t =
        std::clamp(((point.x - value.x1) * dx + (point.y - value.y1) * dy) / denom, 0.0, 1.0);
    const detail::Rgb a = detail::unpack(value.rgb1);
    const detail::Rgb b = detail::unpack(value.rgb2);
    return {
        static_cast<std::uint8_t>(std::lround(a.r + (b.r - a.r) * t)),
        static_cast<std::uint8_t>(std::lround(a.g + (b.g - a.g) * t)),
        static_cast<std::uint8_t>(std::lround(a.b + (b.b - a.b) * t)),
    };
}

bool DrawingRuntime::Impl::shapeContains(const DrawingShape& shape, detail::Vec2 point,
                                         detail::Vec2 original, float sceneRotation) const {
    const auto& p = shape.params;
    switch (shape.kind) {
    case DrawingShapeKind::Band: {
        const double angle = -(static_cast<double>(sceneRotation) + p[2]);
        const detail::Vec2 projected =
            detail::rotate({original.x * 2.0 - 1.0, original.y * 2.0 - 1.0}, angle);
        double minY = std::numeric_limits<double>::max();
        double maxY = -minY;
        for (const auto outer : geometry->outline()) {
            const detail::Vec2 vertex = detail::rotate({outer.x, outer.y}, angle);
            minY = std::min(minY, vertex.y);
            maxY = std::max(maxY, vertex.y);
        }
        const double h = maxY == minY ? 0.5 : (projected.y - minY) / (maxY - minY);
        const double lower = p[0] - p[1] / 2.0;
        const double upper = p[0] + p[1] / 2.0;
        return h >= lower && h < upper;
    }
    case DrawingShapeKind::Circle:
        return std::hypot(point.x - p[0], point.y - p[1]) <= p[2];
    case DrawingShapeKind::Ring: {
        const double distance = std::hypot(point.x - p[0], point.y - p[1]);
        const double inner = std::max(0.0, p[2] - p[3] / 2.0);
        return distance >= inner && distance <= p[2] + p[3] / 2.0;
    }
    case DrawingShapeKind::Line:
        return detail::distanceToSegment(point, {p[0], p[1]}, {p[2], p[3]}) <= p[4] / 2.0;
    case DrawingShapeKind::Rectangle: {
        detail::Vec2 local = detail::rotate({point.x - p[0], point.y - p[1]}, -p[4]);
        return std::abs(local.x) <= p[2] / 2.0 && std::abs(local.y) <= p[3] / 2.0;
    }
    }
    return false;
}

bool DrawingRuntime::Impl::rasterizeScene(const DrawingScene& value) {
    if (!prepareRaster())
        return false;
    const auto points = geometry->points();
    std::uint8_t* target = cache.get();
    for (std::size_t i = 0; i < ledCount; ++i) {
        const detail::Vec2 original{(points[i].x + 1.0) / 2.0, (points[i].y + 1.0) / 2.0};
        const detail::Vec2 centered{original.x - 0.5, original.y - 0.5};
        const detail::Vec2 inverse = detail::rotate(centered, -value.rotationDegrees);
        const detail::Vec2 sample{inverse.x + 0.5, inverse.y + 0.5};
        detail::Rgb color = baseColor(value, sample);
        for (std::size_t shapeIndex = 0; shapeIndex < value.shapeCount; ++shapeIndex) {
            const DrawingShape& shape = value.shapes[shapeIndex];
            if (shapeContains(shape, sample, original, value.rotationDegrees)) {
                color = detail::composite(color, detail::unpack(shape.rgb), shape.opacity);
            }
        }
        target[i * 3] = color.r;
        target[i * 3 + 1] = color.g;
        target[i * 3 + 2] = color.b;
    }
    cacheGeometryRevision = geometry->topologyRevision();
    deferredGeometryRevision = 0;
    return true;
}

bool DrawingRuntime::validateScene(const DrawingScene& value) {
    if (value.shapeCount > DRAWING_MAX_SHAPES || !detail::validRgb(value.rgb1) ||
        !detail::validRgb(value.rgb2) || !detail::finite(value.rotationDegrees))
        return false;
    if (value.baseKind == DrawingBaseKind::Gradient) {
        if (!detail::finite(value.x1) || !detail::finite(value.y1) || !detail::finite(value.x2) ||
            !detail::finite(value.y2) || (value.x1 == value.x2 && value.y1 == value.y2))
            return false;
    } else if (value.baseKind != DrawingBaseKind::Solid || value.rgb2 != 0 || value.x1 != 0 ||
               value.y1 != 0 || value.x2 != 0 || value.y2 != 0) {
        return false;
    }
    std::uint16_t ids = 0;
    for (std::size_t index = 0; index < value.shapeCount; ++index) {
        const DrawingShape& shape = value.shapes[index];
        if (shape.id >= DRAWING_MAX_SHAPES || !detail::validRgb(shape.rgb) ||
            (ids & (1u << shape.id)) != 0)
            return false;
        ids |= static_cast<std::uint16_t>(1u << shape.id);
        for (float param : shape.params)
            if (!detail::finite(param))
                return false;
        std::size_t used = 0;
        switch (shape.kind) {
        case DrawingShapeKind::Band:
            used = 3;
            if (!(shape.params[1] > 0))
                return false;
            break;
        case DrawingShapeKind::Circle:
            used = 3;
            if (!(shape.params[2] > 0))
                return false;
            break;
        case DrawingShapeKind::Ring:
            used = 4;
            if (!(shape.params[2] > 0 && shape.params[3] > 0))
                return false;
            break;
        case DrawingShapeKind::Line:
            used = 5;
            if (!(shape.params[4] > 0))
                return false;
            break;
        case DrawingShapeKind::Rectangle:
            used = 5;
            if (!(shape.params[2] > 0 && shape.params[3] > 0))
                return false;
            break;
        default:
            return false;
        }
        for (std::size_t i = used; i < shape.params.size(); ++i)
            if (shape.params[i] != 0)
                return false;
    }
    return true;
}

} // namespace lightgraph::drawing
