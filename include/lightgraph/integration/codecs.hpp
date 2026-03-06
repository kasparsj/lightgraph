#pragma once

#include "src/runtime/LayerJsonCodec.h"
#include "src/topology/TopologyJsonCodec.h"

/**
 * @file codecs.hpp
 * @brief JSON codecs for topology and layer state used by source integrations.
 */

namespace lightgraph::integration {

using TopologySnapshot = ::TopologySnapshot;
using TopologyIntersectionSnapshot = ::TopologyIntersectionSnapshot;
using TopologyConnectionSnapshot = ::TopologyConnectionSnapshot;
using TopologyPortSnapshot = ::TopologyPortSnapshot;
using TopologyModelSnapshot = ::TopologyModelSnapshot;
using TopologyWeightSnapshot = ::TopologyPortWeightSnapshot;
using TopologyWeightConditionalSnapshot = ::TopologyWeightConditionalSnapshot;
using TopologyGapSnapshot = ::PixelGap;
using TopologyPortType = ::TopologyPortType;

namespace layer_json = ::lightgraph_layer_json;

} // namespace lightgraph::integration
