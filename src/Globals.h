#pragma once

#include "FastNoise.h"
#include <cstdint>

#ifndef LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
#ifdef MESHLED_OOM_TELEMETRY_ENABLED
#define LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED MESHLED_OOM_TELEMETRY_ENABLED
#else
#define LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED 1
#endif
#endif

extern FastNoise gPerlinNoise;
extern unsigned long gMillis;

enum class LightgraphAllocationFailureSite : uint8_t {
  Unknown = 0,
  StateBehaviourAllocation = 1,
  StateListAllocation = 2,
  StateSetupException = 3,
  SetupBgAllocation = 4,
  LightListArrayAllocation = 5,
  LightListLightAllocation = 6,
  RemoteBehaviourAllocation = 7,
  RemoteListAllocation = 8,
  RemoteLightAllocation = 9,
};

using LightgraphAllocationFailureObserver =
    void (*)(LightgraphAllocationFailureSite site, uint16_t detail0, uint16_t detail1);

void lightgraphSetAllocationFailureObserver(LightgraphAllocationFailureObserver observer);
void lightgraphReportAllocationFailure(
    LightgraphAllocationFailureSite site,
    uint16_t detail0 = 0,
    uint16_t detail1 = 0);
