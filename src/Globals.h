#ifndef PACKAGES_LIGHTGRAPH_SRC_GLOBALS_H_
#define PACKAGES_LIGHTGRAPH_SRC_GLOBALS_H_

#pragma once

#include <cstdint>
#include "FastNoise.h"
#include "runtime/EmitParams.h"

#ifndef LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
#ifdef MESHLED_OOM_TELEMETRY_ENABLED
#define LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED MESHLED_OOM_TELEMETRY_ENABLED
#else
#define LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED 1
#endif
#endif

#ifndef LIGHTGRAPH_FPS_INDEPENDENT_SPEED
#define LIGHTGRAPH_FPS_INDEPENDENT_SPEED 1
#endif

#ifndef LIGHTGRAPH_FRACTIONAL_RENDERING
#define LIGHTGRAPH_FRACTIONAL_RENDERING 1
#endif

#ifndef LIGHTGRAPH_MAX_SIMULATION_SUBSTEPS
#define LIGHTGRAPH_MAX_SIMULATION_SUBSTEPS 8
#endif

class RuntimeLight;

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

using LightgraphExternalSendHook =
    bool (*)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList);

struct LightgraphRuntimeContext {
  FastNoise perlinNoise;
  unsigned long nowMillis = 0;
  bool hasExplicitNowMillis = false;
  LightgraphAllocationFailureObserver allocationFailureObserver = nullptr;
  bool hasFrameTiming = false;
  unsigned long lastFrameMillis = 0;
  float frameElapsedMillis = static_cast<float>(EmitParams::frameMs());
  float currentStepMillis = static_cast<float>(EmitParams::frameMs());
  LightgraphExternalSendHook externalSendHook = nullptr;
};

extern FastNoise gPerlinNoise;
extern unsigned long gMillis;

LightgraphRuntimeContext& lightgraphDefaultRuntimeContext();

void lightgraphSetAllocationFailureObserver(LightgraphAllocationFailureObserver observer);
void lightgraphSetAllocationFailureObserver(
    LightgraphRuntimeContext& context,
    LightgraphAllocationFailureObserver observer);
void lightgraphReportAllocationFailure(
    LightgraphAllocationFailureSite site,
    uint16_t detail0 = 0,
    uint16_t detail1 = 0);
void lightgraphReportAllocationFailure(
    const LightgraphRuntimeContext& context,
    LightgraphAllocationFailureSite site,
    uint16_t detail0 = 0,
    uint16_t detail1 = 0);

void lightgraphAdvanceFrameTiming(unsigned long nowMillis);
void lightgraphAdvanceFrameTiming(LightgraphRuntimeContext& context, unsigned long nowMillis);
void lightgraphResetFrameTiming();
void lightgraphResetFrameTiming(LightgraphRuntimeContext& context);
uint8_t lightgraphSimulationSubsteps();
uint8_t lightgraphSimulationSubsteps(const LightgraphRuntimeContext& context);
void lightgraphSetSimulationSubstep(uint8_t stepCount);
void lightgraphSetSimulationSubstep(LightgraphRuntimeContext& context, uint8_t stepCount);
float lightgraphConfiguredSpeedPixelsPerSecond(float speed);
float lightgraphConfiguredSpeedPixelsPerSecond(const LightgraphRuntimeContext& context, float speed);
float lightgraphMotionDistance(float speed);
float lightgraphMotionDistance(const LightgraphRuntimeContext& context, float speed);
void lightgraphSetNowMillis(unsigned long nowMillis);
void lightgraphSetNowMillis(LightgraphRuntimeContext& context, unsigned long nowMillis);

#endif  // PACKAGES_LIGHTGRAPH_SRC_GLOBALS_H_
