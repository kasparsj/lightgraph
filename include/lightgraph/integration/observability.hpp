#pragma once

#include "src/Globals.h"

/**
 * @file observability.hpp
 * @brief Allocation-failure observer hooks and diagnostics globals.
 */

namespace lightgraph::integration {

using AllocationFailureSite = ::LightgraphAllocationFailureSite;
using AllocationFailureObserver = ::LightgraphAllocationFailureObserver;

inline void setAllocationFailureObserver(AllocationFailureObserver observer) {
  ::lightgraphSetAllocationFailureObserver(observer);
}

inline void reportAllocationFailure(AllocationFailureSite site, uint16_t detail0 = 0, uint16_t detail1 = 0) {
  ::lightgraphReportAllocationFailure(site, detail0, detail1);
}

} // namespace lightgraph::integration
