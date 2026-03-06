#pragma once

#include <cstdint>
#include <memory>

#include <lightgraph/internal/objects.hpp>
#include <lightgraph/types.hpp>

namespace lightgraph::integration {

enum class BuiltinObjectType {
    Heptagon919,
    Heptagon3024,
    Line,
    Cross,
    Triangle,
};

} // namespace lightgraph::integration

namespace lightgraph::internal {

inline constexpr uint16_t defaultPixelCount(integration::BuiltinObjectType type) {
    switch (type) {
    case integration::BuiltinObjectType::Heptagon919:
        return HEPTAGON919_PIXEL_COUNT;
    case integration::BuiltinObjectType::Heptagon3024:
        return HEPTAGON3024_PIXEL_COUNT;
    case integration::BuiltinObjectType::Line:
        return LINE_PIXEL_COUNT;
    case integration::BuiltinObjectType::Cross:
        return CROSS_PIXEL_COUNT;
    case integration::BuiltinObjectType::Triangle:
        return TRIANGLE_PIXEL_COUNT;
    }

    return LINE_PIXEL_COUNT;
}

inline constexpr integration::BuiltinObjectType toBuiltinObjectType(ObjectType type) {
    switch (type) {
    case ObjectType::Heptagon919:
        return integration::BuiltinObjectType::Heptagon919;
    case ObjectType::Heptagon3024:
        return integration::BuiltinObjectType::Heptagon3024;
    case ObjectType::Line:
        return integration::BuiltinObjectType::Line;
    case ObjectType::Cross:
        return integration::BuiltinObjectType::Cross;
    case ObjectType::Triangle:
        return integration::BuiltinObjectType::Triangle;
    }

    return integration::BuiltinObjectType::Line;
}

inline constexpr uint16_t resolvePixelCount(integration::BuiltinObjectType type, uint16_t pixelCount) {
    return pixelCount > 0 ? pixelCount : defaultPixelCount(type);
}

inline std::unique_ptr<TopologyObject> makeBuiltinObject(integration::BuiltinObjectType type,
                                                         uint16_t pixelCount = 0) {
    const uint16_t resolvedPixelCount = resolvePixelCount(type, pixelCount);
    switch (type) {
    case integration::BuiltinObjectType::Heptagon919:
        return std::unique_ptr<TopologyObject>(new Heptagon919());
    case integration::BuiltinObjectType::Heptagon3024:
        return std::unique_ptr<TopologyObject>(new Heptagon3024());
    case integration::BuiltinObjectType::Line:
        return std::unique_ptr<TopologyObject>(new Line(resolvedPixelCount));
    case integration::BuiltinObjectType::Cross:
        return std::unique_ptr<TopologyObject>(new Cross(resolvedPixelCount));
    case integration::BuiltinObjectType::Triangle:
        return std::unique_ptr<TopologyObject>(new Triangle(resolvedPixelCount));
    }

    return std::unique_ptr<TopologyObject>(new Line(resolvedPixelCount));
}

inline std::unique_ptr<TopologyObject> makeBuiltinObject(ObjectType type, uint16_t pixelCount = 0) {
    return makeBuiltinObject(toBuiltinObjectType(type), pixelCount);
}

} // namespace lightgraph::internal
