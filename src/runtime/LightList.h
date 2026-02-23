#pragma once

#include "../core/Types.h"
#include "../core/Limits.h"
#include "../ofxEasing.h"
#include "Behaviour.h"
#include "LPLight.h"
#include <stdexcept>
#include <vector>
#include "../rendering/Palette.h"

class Model;
class Light;
class LPOwner;

typedef struct {
  uint8_t messageType;
  uint8_t portId;
  uint16_t listId;
  uint16_t lightIdx;
  uint8_t brightness;
  uint8_t colorR;
  uint8_t colorG;
  uint8_t colorB;
  float speed;
  uint32_t life;
} LightMessage;

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
    LPLight **lights = 0;
    LPOwner *emitter = 0;
    uint16_t numEmitted = 0;
    uint8_t numSplits = 0;
    std::vector<ColorRGB> colors; // Vector of colors used for the actual lights
    Palette palette; // Palette to manage colors and their positions
    bool visible = true;
    bool editable = false;
    BlendMode blendMode = BLEND_NORMAL;
    uint32_t duration = 1000;

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
      
      // Deep copy behaviour if it exists
      if (other.behaviour != NULL) {
        behaviour = new Behaviour(other.behaviour->flags, other.behaviour->colorChangeGroups);
      } else {
        behaviour = NULL;
      }
      
        numLights = other.numLights > 0 ? other.numLights : other.length;
        maxBri = other.maxBri;
        duration = other.duration;
        palette = other.palette;
        reset();
    }

    LightList& operator=(const LightList&) = delete;
    LightList(LightList&&) = delete;
    LightList& operator=(LightList&&) = delete;

    virtual ~LightList() {
      if (lights != NULL) {
        for (uint16_t i = 0; i < allocatedLights; i++) {
          delete lights[i];
        }
        delete[] lights;
      }
      
      if (behaviour != NULL) {
        delete behaviour;
      }
    }
    
    virtual void init(uint16_t numLights);
    virtual void setup(uint16_t numLights, uint8_t brightness = 255);
    virtual void reset();

    float getBriMult(uint16_t i);

    LPLight* operator [] (uint16_t i) const {
      return lights[i];
    }

    LPLight*& operator [] (uint16_t i) {
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
    float getPosition(LPLight* const light) const;
    uint16_t getBri(const LPLight* light) const;
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
    
    LPLight* addLightFromMsg(const LightMessage* lightMsg);

  private:

    LPLight* createLight(uint16_t i, uint8_t brightness);
    void initPosition(uint16_t i, LPLight* const light) const;
    void initBri(uint16_t i, LPLight* const light) const;
    void initLife(uint16_t i, LPLight* const light) const;
    void doEmit();
    void setLightColors();
    inline uint16_t body() {
        return numLights - lead - trail;
    }
    uint16_t allocatedLights = 0;

};
