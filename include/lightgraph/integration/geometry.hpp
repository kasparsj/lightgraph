#pragma once

#include "lightgraph/internal/geometry.hpp"

/**
 * @file geometry.hpp
 * @brief Geometry helpers shared by source-integrated visualizers.
 */

namespace lightgraph::integration {

using GeometryProvider = geometry::GeometryProvider;
using GeometryResult = geometry::GeometryResult;
using GeometryPoint = geometry::GeometryPoint;
template <typename T>
using GeometryArrayView = geometry::GeometryArrayView<T>;
using HeptagonGeometry = ::HeptagonGeometry;

} // namespace lightgraph::integration
