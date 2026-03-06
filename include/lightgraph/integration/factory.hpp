#pragma once

#include <cstdint>
#include <memory>

#include "lightgraph/internal/object_factory.hpp"
#include "objects.hpp"

/**
 * @file factory.hpp
 * @brief Convenience constructors for built-in topology objects.
 */

namespace lightgraph::integration {

/**
 * @brief Create one of the built-in topology objects.
 * @param type Built-in object kind.
 * @param pixelCount Optional pixel count override for line/cross/triangle.
 */
inline std::unique_ptr<Object> makeObject(BuiltinObjectType type, uint16_t pixelCount = 0) {
    return internal::makeBuiltinObject(type, pixelCount);
}

} // namespace lightgraph::integration
