#ifndef PACKAGES_LIGHTGRAPH_SRC_RUNTIME_STATE_H_
#define PACKAGES_LIGHTGRAPH_SRC_RUNTIME_STATE_H_

#pragma once

#include <cstddef>
#include <vector>
#include "../drawing/DrawingRuntime.h"
#include "lightgraph/types.hpp"

#include "../Globals.h"
#include "../core/Types.h"
#include "../core/Limits.h"

class EmitParams;
class TopologyObject;
class LightList;
class Model;
class Behaviour;
class Owner;
class RuntimeLight;

class State {

  public:

    enum class InitializationResult : uint8_t {
      Ready = 0,
      AdmissionDenied = 1,
      AllocationFailed = 2,
    };

    enum class EmitFailure : uint8_t {
      None = 0,
      InvalidArgument,
      InvalidModel,
      NoFreeLightList,
      NoEmitterAvailable,
      CapacityExceeded,
      AdmissionDenied,
      AllocationFailed,
      NotReady,
    };

    enum class MemoryRejectionReason : uint8_t {
      None = 0,
      Admission = 1,
      AllocationFailure = 2,
    };

    struct MemoryRejection {
      lightgraph::memory::Operation operation = lightgraph::memory::Operation::Emit;
      size_t peakBytes = 0;
      size_t largestBlock = 0;
      MemoryRejectionReason reason = MemoryRejectionReason::None;
    };

    static EmitParams autoParams;
    // Optional diagnostics observer; called before constructor buffer allocations.
    using AllocationCheckpoint = void (*)(const char* stage, size_t requestedBytes);
    static AllocationCheckpoint allocationCheckpoint;

    TopologyObject &object;
    bool initializationAdmitted = true;
    InitializationResult initializationResult = InitializationResult::AllocationFailed;
    EmitFailure lastEmitFailure = EmitFailure::None;
    LightList *lightLists[MAX_LIGHT_LISTS] = {0};
    uint16_t totalLights = 0;
    uint8_t totalLightLists = 0;
    unsigned long nextEmit = 0;
    std::vector<uint16_t> pixelValuesR;
    std::vector<uint16_t> pixelValuesG;
    std::vector<uint16_t> pixelValuesB;
    std::vector<uint8_t> pixelDiv;
    std::vector<uint8_t> geometricOccupancy;
    std::vector<uint16_t> renderPixelScratch;
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    static_assert(FULL_BRIGHTNESS <= UINT8_MAX,
                  "Per-list RGB scratch requires an 8-bit brightness ceiling");
    // Each per-list contribution saturates before storage; frame sums stay wide.
    std::vector<uint8_t> listPixelValuesR;
    std::vector<uint8_t> listPixelValuesG;
    std::vector<uint8_t> listPixelValuesB;
    std::vector<uint16_t> listTouchedPixels;
#endif
    bool autoEnabled = false;
    uint8_t currentPalette = 0;
    bool showIntersections = false;
    bool showConnections = false;
    uint8_t reservedTailSlots = 0;
    uint32_t emitMemoryRejections = 0;
    MemoryRejection lastEmitRejection{};

    explicit State(TopologyObject &obj);
    ~State();

    uint8_t randomModel();
    ColorRGB paletteColor(uint8_t index, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void autoEmit(unsigned long millis);
    int8_t emit(EmitParams &params);
    void emit(LightList& lightList);
    int8_t getOrCreateList(EmitParams &params);
    LightList* setupListFrom(uint8_t i, EmitParams &params);
    Owner* getEmitter(Model* model, Behaviour* behaviour, EmitParams& params);
    void update();
    void updateLight(RuntimeLight* light, uint16_t listSlot = 0);
    void colorAll();
    void splitAll();
    void stopAll();
    int8_t findList(uint16_t noteId) const;
    LightList* findListById(uint16_t id);
    void stopNote(uint16_t noteId);
    bool setListLengths(const lightgraph::ListLengthUpdate* updates, size_t count);
    ColorRGB getPixel(uint16_t i, uint8_t maxBrightness = FULL_BRIGHTNESS);
    void debug();
    bool isOn();
    void setOn(bool newState);
    void setupBg(uint8_t i);
    bool setupBgChecked(uint8_t i);
    bool hasRequiredFrameBuffers() const;
    bool ready() const noexcept { return initializationResult == InitializationResult::Ready && hasRequiredFrameBuffers(); }
    lightgraph::drawing::DrawingRuntime& drawing();
    const lightgraph::drawing::DrawingRuntime& drawing() const;
    void activateList(Owner* from, LightList *lightList, uint8_t emitOffset = 0, bool countTotals = true);
    void doEmit(Owner* from, LightList *lightList, uint8_t emitOffset = 0);
    void setReservedTailSlots(uint8_t slots);
    uint8_t getReservedTailSlots() const;
    uint8_t getLocalSlotEndExclusive() const;
    bool clearListSlot(uint8_t slot);
    bool replaceListSlot(uint8_t slot, LightList* replacement);
    void recordEmitAllocationFailure(
        lightgraph::memory::Operation operation,
        const lightgraph::memory::Estimate& estimate = {}) noexcept;
    bool admitMemory(lightgraph::memory::Operation operation,
                     const lightgraph::memory::Estimate& estimate);

  private:
    mutable lightgraph::drawing::DrawingRuntime drawingRuntime_;
    void failInitializationAllocation() noexcept;
    bool constructBackground(uint8_t slot);
    int8_t emitResolved(EmitParams& params);
    void recordMemoryRejection(lightgraph::memory::Operation operation,
                               const lightgraph::memory::Estimate& estimate,
                               MemoryRejectionReason reason) noexcept;
    void doEmit(Owner* from, LightList *lightList, EmitParams& params);
    void updatePass(bool renderStep);
    void setPixelsWeighted(uint16_t pixel, const ColorRGB& color, const LightList* const lightList, uint8_t weight);
    void setPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setPixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setFramePixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setFramePixel(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void markOccupancy(uint16_t pixel, const LightList* lightList);
    bool isOccupied(uint16_t pixel) const;
    void renderDrawingOverlay();
#if LIGHTGRAPH_FRACTIONAL_RENDERING
    void beginListRender(const LightList* lightList);
    void endListRender(const LightList* lightList);
    void setListPixels(uint16_t pixel, ColorRGB &color, const LightList* const lightList);
    void setListPixel(uint16_t pixel, ColorRGB &color);
    const LightList* renderingList = nullptr;
#endif

};

#endif  // PACKAGES_LIGHTGRAPH_SRC_RUNTIME_STATE_H_
