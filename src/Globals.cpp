#include "Globals.h"

FastNoise gPerlinNoise;
unsigned long gMillis = 0;

static LightgraphAllocationFailureObserver gLightgraphAllocationFailureObserver = nullptr;

void lightgraphSetAllocationFailureObserver(LightgraphAllocationFailureObserver observer) {
#if LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
  gLightgraphAllocationFailureObserver = observer;
#else
  (void) observer;
#endif
}

void lightgraphReportAllocationFailure(
    LightgraphAllocationFailureSite site,
    uint16_t detail0,
    uint16_t detail1) {
#if LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
  if (gLightgraphAllocationFailureObserver == nullptr) {
    return;
  }
  gLightgraphAllocationFailureObserver(site, detail0, detail1);
#else
  (void) site;
  (void) detail0;
  (void) detail1;
#endif
}
