#include "Globals.h"

#include <algorithm>
#include <cmath>

namespace {

LightgraphRuntimeContext gLightgraphDefaultRuntimeContext;

} // namespace

unsigned long gMillis = 0;

LightgraphRuntimeContext& lightgraphDefaultRuntimeContext() {
  return gLightgraphDefaultRuntimeContext;
}

void lightgraphSetNowMillis(LightgraphRuntimeContext& context, unsigned long nowMillis) {
  context.nowMillis = nowMillis;
  context.hasExplicitNowMillis = true;
}

void lightgraphSetNowMillis(unsigned long nowMillis) {
  gMillis = nowMillis;
  lightgraphSetNowMillis(lightgraphDefaultRuntimeContext(), nowMillis);
}

void lightgraphSetAllocationFailureObserver(
    LightgraphRuntimeContext& context,
    LightgraphAllocationFailureObserver observer) {
#if LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
  context.allocationFailureObserver = observer;
#else
  (void) context;
  (void) observer;
#endif
}

void lightgraphSetAllocationFailureObserver(LightgraphAllocationFailureObserver observer) {
  lightgraphSetAllocationFailureObserver(lightgraphDefaultRuntimeContext(), observer);
}

void lightgraphReportAllocationFailure(
    const LightgraphRuntimeContext& context,
    LightgraphAllocationFailureSite site,
    uint16_t detail0,
    uint16_t detail1) {
#if LIGHTGRAPH_ALLOCATION_FAILURE_HOOK_ENABLED
  if (context.allocationFailureObserver == nullptr) {
    return;
  }
  context.allocationFailureObserver(site, detail0, detail1);
#else
  (void) context;
  (void) site;
  (void) detail0;
  (void) detail1;
#endif
}

void lightgraphReportAllocationFailure(
    LightgraphAllocationFailureSite site,
    uint16_t detail0,
    uint16_t detail1) {
  lightgraphReportAllocationFailure(lightgraphDefaultRuntimeContext(), site, detail0, detail1);
}

void lightgraphAdvanceFrameTiming(LightgraphRuntimeContext& context, unsigned long nowMillis) {
#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
  if (!context.hasFrameTiming || nowMillis < context.lastFrameMillis) {
    context.hasFrameTiming = true;
    context.lastFrameMillis = nowMillis;
    context.frameElapsedMillis = static_cast<float>(EmitParams::frameMs());
    context.currentStepMillis = context.frameElapsedMillis;
    return;
  }

  context.frameElapsedMillis = static_cast<float>(nowMillis - context.lastFrameMillis);
  context.lastFrameMillis = nowMillis;
  context.currentStepMillis = context.frameElapsedMillis;
#else
  (void) context;
  (void) nowMillis;
#endif
}

void lightgraphAdvanceFrameTiming(unsigned long nowMillis) {
  lightgraphAdvanceFrameTiming(lightgraphDefaultRuntimeContext(), nowMillis);
}

void lightgraphResetFrameTiming(LightgraphRuntimeContext& context) {
  context.hasFrameTiming = false;
  context.lastFrameMillis = 0;
  context.frameElapsedMillis = static_cast<float>(EmitParams::frameMs());
  context.currentStepMillis = context.frameElapsedMillis;
}

void lightgraphResetFrameTiming() {
  lightgraphResetFrameTiming(lightgraphDefaultRuntimeContext());
}

uint8_t lightgraphSimulationSubsteps(const LightgraphRuntimeContext& context) {
#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
  const float referenceFrameMillis = static_cast<float>(EmitParams::frameMs());
  if (referenceFrameMillis <= 0.0f) {
    return 1;
  }

  const float desiredSteps = std::ceil(context.frameElapsedMillis / referenceFrameMillis);
  const uint8_t steps = static_cast<uint8_t>(std::max(1.0f, desiredSteps));
  return std::min<uint8_t>(steps, LIGHTGRAPH_MAX_SIMULATION_SUBSTEPS);
#else
  (void) context;
  return 1;
#endif
}

uint8_t lightgraphSimulationSubsteps() {
  return lightgraphSimulationSubsteps(lightgraphDefaultRuntimeContext());
}

void lightgraphSetSimulationSubstep(LightgraphRuntimeContext& context, uint8_t stepCount) {
#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
  if (stepCount == 0) {
    context.currentStepMillis = context.frameElapsedMillis;
    return;
  }
  context.currentStepMillis =
      context.frameElapsedMillis / static_cast<float>(std::max<uint8_t>(stepCount, 1));
#else
  (void) context;
  (void) stepCount;
#endif
}

void lightgraphSetSimulationSubstep(uint8_t stepCount) {
  lightgraphSetSimulationSubstep(lightgraphDefaultRuntimeContext(), stepCount);
}

float lightgraphConfiguredSpeedPixelsPerSecond(const LightgraphRuntimeContext&, float speed) {
#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
  return speed * EmitParams::DURATION_FPS;
#else
  return speed;
#endif
}

float lightgraphConfiguredSpeedPixelsPerSecond(float speed) {
  return lightgraphConfiguredSpeedPixelsPerSecond(lightgraphDefaultRuntimeContext(), speed);
}

float lightgraphMotionDistance(const LightgraphRuntimeContext& context, float speed) {
#if LIGHTGRAPH_FPS_INDEPENDENT_SPEED
  return lightgraphConfiguredSpeedPixelsPerSecond(context, speed) *
         (context.currentStepMillis / 1000.0f);
#else
  (void) context;
  return speed;
#endif
}

float lightgraphMotionDistance(float speed) {
  return lightgraphMotionDistance(lightgraphDefaultRuntimeContext(), speed);
}
