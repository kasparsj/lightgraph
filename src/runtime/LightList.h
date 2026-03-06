#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../Globals.h"
#include "../../vendor/ofxEasing/ofxEasing.h"
#include "Behaviour.h"
#include "RuntimeLight.h"
#include <cstddef>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>
#include "../rendering/Palette.h"

class Model;
class Light;
class Owner;

class LightList {

  public:

    static uint16_t nextId;
    
    uint16_t id;
    uint16_t noteId = 0;
    float speed = DEFAULT_SPEED;
    ofxeasing::function ease = ofxeasing::linear::easeNone;
    uint8_t easeIndex = EASE_NONE; // Store which easing function is being used
    uint8_t fadeSpeed = 0;
    uint8_t fadeThresh = 0;
    uint8_t minBri = 0;
    uint8_t maxBri = 255;
    ofxeasing::function fadeEase = ofxeasing::linear::easeNone;
    uint8_t fadeEaseIndex = EASE_NONE; // Store which fade easing function is being used
    uint32_t lifeMillis = 0;
    ListOrder order = LIST_ORDER_SEQUENTIAL;
    ListHead head = LIST_HEAD_FRONT;
    bool linked = true;
    Model *model = 0;
    Behaviour *behaviour = 0;
    uint16_t length = 0;
    uint16_t numLights = 0;
    uint16_t lead = 0;
    uint16_t trail = 0;
    RuntimeLight **lights = 0;
    Owner *emitter = 0;
    uint16_t numEmitted = 0;
    uint8_t numSplits = 0;
    std::vector<ColorRGB> colors; // Vector of colors used for the actual lights
    Palette palette; // Palette to manage colors and their positions
    bool visible = true;
    bool editable = false;
    BlendMode blendMode = BLEND_NORMAL;
    uint32_t duration = 1000;
    uint8_t emitOffset = 0;
    bool compensateHiddenIngressContinuity = false;
    bool externalBatchForwarded = false;
    uint8_t externalBatchDevice[6] = {0};
    uint8_t externalBatchTargetId = 0;

    LightList() {
      this->id = nextId++;
    }
    
    // Copy constructor
    LightList(const LightList& other) {
      // Assign a new ID
      this->id = nextId++;
      
      // Copy basic properties
      noteId = other.noteId;
      speed = other.speed;
      ease = other.ease;
      easeIndex = other.easeIndex;
      fadeSpeed = other.fadeSpeed;
      fadeThresh = other.fadeThresh;
      minBri = other.minBri;
      fadeEase = other.fadeEase;
      fadeEaseIndex = other.fadeEaseIndex;
      lifeMillis = other.lifeMillis;
      order = other.order;
      head = other.head;
      linked = other.linked;
      model = other.model; // Shallow copy, model is shared
      lead = other.lead;
      trail = other.trail;
      emitter = other.emitter; // Shallow copy, emitter is shared
      numEmitted = other.numEmitted;
      numSplits = other.numSplits;
      visible = other.visible;
      editable = other.editable;
      blendMode = other.blendMode;
      emitOffset = other.emitOffset;
      compensateHiddenIngressContinuity = false;
      
      // Deep copy behaviour if it exists
      if (other.behaviour != NULL) {
        behaviour = new (std::nothrow) Behaviour(other.behaviour->flags, other.behaviour->colorChangeGroups);
        if (behaviour == nullptr) {
          lightgraphReportAllocationFailure(
              runtimeContext(),
              LightgraphAllocationFailureSite::StateBehaviourAllocation,
              other.behaviour->flags,
              other.behaviour->colorChangeGroups);
        }
      } else {
        behaviour = NULL;
      }
      
        numLights = other.numLights > 0 ? other.numLights : other.length;
        maxBri = other.maxBri;
      duration = other.duration;
      palette = other.palette;
      runtimeContext_ = other.runtimeContext_;
      reset();
    }

    LightList& operator=(const LightList&) = delete;
    LightList(LightList&&) = delete;
    LightList& operator=(LightList&&) = delete;

    virtual ~LightList();
    
    virtual void init(uint16_t numLights);
    virtual void setup(uint16_t numLights, uint8_t brightness = 255);
    virtual void reset();

    float getBriMult(uint16_t i);

    RuntimeLight* operator [] (uint16_t i) const {
      return lights[i];
    }

    RuntimeLight*& operator [] (uint16_t i) {
      return lights[i];
    }

    void setSpeed(float speed, uint8_t ease = 0) {
        this->speed = speed;
        this->easeIndex = ease; // Store the ease index
        this->ease = ease == EASE_NONE ?
            ofxeasing::linear::easeNone :
            ofxeasing::easing(static_cast<ofxeasing::Function>((ease - 1) / 3), static_cast<ofxeasing::Type>((ease - 1) % 3));
    }
    void setFade(uint8_t fadeSpeed, uint8_t fadeThresh = 0, uint8_t fadeEase = 0) {
        this->fadeSpeed = fadeSpeed;
        this->fadeThresh = fadeThresh;
        this->fadeEaseIndex = fadeEase; // Store the fade ease index
        this->fadeEase = fadeEase == EASE_NONE ?
            ofxeasing::linear::easeNone :
            ofxeasing::easing(static_cast<ofxeasing::Function>((fadeEase - 1) / 3), static_cast<ofxeasing::Type>((fadeEase - 1) % 3));
    }
    void setLeadTrail(uint16_t trail);
    void setDuration(uint32_t durMillis);
    
    virtual void setPalette(const Palette& newPalette) {
        palette = newPalette;        
        colors = palette.interpolate(numLights);
        setLightColors();
    }
    void setupFrom(const EmitParams &params);
    void initEmit(uint8_t posOffset = 0);
    virtual bool update();
    void split();
    float getPosition(RuntimeLight* const light) const;
    uint16_t getBri(const RuntimeLight* light) const;
    virtual ColorRGB getColor(int16_t /*pixel*/ = -1) const {
        throw std::runtime_error("LightList::getColor not implemented");
    }
    virtual const ColorRGB& getLightColor(uint32_t i) const {
        return Palette::wrapColors(i, numLights, colors, palette.getWrapMode(), palette.getSegmentation());
    }
    
    bool hasPalette() const {
        return palette.size() > 0;
    }
    
    const Palette& getPalette() const {
        return palette;
    }
    
    // Get the position of the light list (position of the first light)
    virtual float getOffset() const;
    
    // Set the position of the light list (moves all lights by the offset)
    virtual void setOffset(float newPosition);
    
    RuntimeLight* createAutoLight(uint16_t slot, uint8_t brightness);
    void releaseOwnedLight(RuntimeLight*& light);
    bool initContiguousLights(uint16_t numLights);
    Light* createContiguousLight(uint16_t slot, float speed, uint32_t lifeMillis,
                                 uint16_t idx = 0, uint8_t maxBri = 255);
    void clearExternalBatchForwardState() {
      externalBatchForwarded = false;
      externalBatchTargetId = 0;
      std::memset(externalBatchDevice, 0, sizeof(externalBatchDevice));
    }
    void markExternalBatchForwarded(const uint8_t device[6], uint8_t targetId) {
      if (device == nullptr) {
        clearExternalBatchForwardState();
        return;
      }
      std::memcpy(externalBatchDevice, device, sizeof(externalBatchDevice));
      externalBatchTargetId = targetId;
      externalBatchForwarded = true;
    }
    bool hasExternalBatchForwardedTo(const uint8_t device[6], uint8_t targetId) const {
      return externalBatchForwarded && device != nullptr && externalBatchTargetId == targetId &&
             std::memcmp(externalBatchDevice, device, sizeof(externalBatchDevice)) == 0;
    }
    void bindRuntimeContext(LightgraphRuntimeContext& context) {
      runtimeContext_ = &context;
    }
    LightgraphRuntimeContext& runtimeContext() {
      return (runtimeContext_ != nullptr) ? *runtimeContext_ : lightgraphDefaultRuntimeContext();
    }
    const LightgraphRuntimeContext& runtimeContext() const {
      return (runtimeContext_ != nullptr) ? *runtimeContext_ : lightgraphDefaultRuntimeContext();
    }

  private:

    RuntimeLight* createLight(uint16_t i, uint8_t brightness);
    void clearAllocatedLights();
    bool ownsContiguousLight(const RuntimeLight* light) const;
    void initPosition(uint16_t i, RuntimeLight* const light) const;
    void initBri(uint16_t i, RuntimeLight* const light) const;
    void initLife(uint16_t i, RuntimeLight* const light) const;
    void doEmit();
    void setLightColors();
    inline uint16_t body() {
        return numLights - lead - trail;
    }
    uint16_t allocatedLights = 0;
    void* contiguousLightStorage = nullptr;
    size_t contiguousLightStrideBytes = 0;
    LightgraphRuntimeContext* runtimeContext_ = nullptr;

};
