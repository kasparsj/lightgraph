#ifndef PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_BUILTINGEOMETRY_H_
#define PACKAGES_LIGHTGRAPH_SRC_GEOMETRY_BUILTINGEOMETRY_H_

#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "GeometryProvider.h"

namespace lightgraph::geometry {

enum class BuiltinGeometryKind : uint8_t { Line, Cross, Triangle };

struct BuiltinGeometryRoles {
    std::array<uint8_t, 5> intersectionIds{};
    uint8_t count = 0;
};

std::unique_ptr<GeometryProvider> createBuiltinGeometry(TopologyObject& object,
                                                        BuiltinGeometryKind kind,
                                                        uint16_t compatiblePixelCount,
                                                        const BuiltinGeometryRoles& roles,
                                                        GeometryResult& result) noexcept;

} // namespace lightgraph::geometry

#endif
