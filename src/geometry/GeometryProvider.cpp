#include "GeometryProvider.h"

#include "../topology/TopologyObject.h"

namespace lightgraph::geometry {

uint32_t GeometryProvider::sourceTopologyRevision() const noexcept {
    return object_.topologyRevision();
}

} // namespace lightgraph::geometry
