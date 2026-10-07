#pragma once

#include "lightgraph/internal/Globals.h"
#include "lightgraph/internal/topology.hpp"

/**
 * @file observability.hpp
 * @brief Allocation-failure observer hooks and diagnostics globals.
 */

namespace lightgraph::integration {

using AllocationFailureSite = ::LightgraphAllocationFailureSite;
using AllocationFailureObserver = ::LightgraphAllocationFailureObserver;
using MemoryAdmissionPolicy = lightgraph::memory::Policy;

inline void setAllocationFailureObserver(AllocationFailureObserver observer) {
  ::lightgraphSetAllocationFailureObserver(observer);
}

inline void setAllocationFailureObserver(::TopologyObject& object, AllocationFailureObserver observer) {
  ::lightgraphSetAllocationFailureObserver(object.runtimeContext(), observer);
}

inline void setMemoryAdmissionPolicy(::TopologyObject& object, MemoryAdmissionPolicy policy) {
  object.runtimeContext().memoryAdmission = policy;
}

inline void reportAllocationFailure(AllocationFailureSite site, uint16_t detail0 = 0, uint16_t detail1 = 0) {
  ::lightgraphReportAllocationFailure(site, detail0, detail1);
}

inline void reportAllocationFailure(::TopologyObject& object,
                                    AllocationFailureSite site,
                                    uint16_t detail0 = 0,
                                    uint16_t detail1 = 0) {
  ::lightgraphReportAllocationFailure(object.runtimeContext(), site, detail0, detail1);
}

} // namespace lightgraph::integration
