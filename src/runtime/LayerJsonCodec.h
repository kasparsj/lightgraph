#pragma once

#include <vector>

#include <ArduinoJson.h>

#include "Behaviour.h"
#include "State.h"
#include "../rendering/Palette.h"

namespace lightgraph_layer_json {

inline void applyLayerArray(JsonArrayConst layersArray, State& state) {
  for (JsonObjectConst layerObj : layersArray) {
    if (!layerObj.containsKey("index")) {
      continue;
    }

    const uint8_t index = layerObj["index"].as<uint8_t>();
    if (index >= MAX_LIGHT_LISTS) {
      continue;
    }
    if (state.lightLists[index] == nullptr) {
      state.setupBg(index);
    }

    if (layerObj.containsKey("visible")) {
      state.lightLists[index]->visible = layerObj["visible"].as<bool>();
    }

    if (layerObj.containsKey("brightness")) {
      const uint8_t brightness = layerObj["brightness"].as<uint8_t>();
      state.lightLists[index]->maxBri = brightness;
      if (state.lightLists[index]->minBri > brightness) {
        state.lightLists[index]->minBri = brightness;
      }
    }

    if (layerObj.containsKey("blendMode")) {
      const uint8_t blendMode = layerObj["blendMode"].as<uint8_t>();
      if (blendMode <= BLEND_PIN_LIGHT) {
        state.lightLists[index]->blendMode = static_cast<BlendMode>(blendMode);
      }
    }

    if (layerObj.containsKey("speed")) {
      const float speed = layerObj["speed"].as<float>();
      if (speed >= -10.0f && speed <= 10.0f) {
        state.lightLists[index]->speed = speed;
      }
    }

    if (layerObj.containsKey("ease")) {
      const uint8_t ease = layerObj["ease"].as<uint8_t>();
      if (ease <= EASE_ELASTIC_INOUT) {
        const float currentSpeed = state.lightLists[index]->speed;
        state.lightLists[index]->setSpeed(currentSpeed, ease);
      }
    }

    if (layerObj.containsKey("fadeSpeed")) {
      const uint8_t fadeSpeed = layerObj["fadeSpeed"].as<uint8_t>();
      const uint8_t currentFadeThresh = state.lightLists[index]->fadeThresh;
      const uint8_t currentFadeEase = state.lightLists[index]->fadeEaseIndex;
      state.lightLists[index]->setFade(fadeSpeed, currentFadeThresh, currentFadeEase);
    }

    if (layerObj.containsKey("behaviourFlags")) {
      const uint16_t behaviourFlags = layerObj["behaviourFlags"].as<uint16_t>();
      if (state.lightLists[index]->behaviour == nullptr) {
        state.lightLists[index]->behaviour = new Behaviour(behaviourFlags);
      } else {
        state.lightLists[index]->behaviour->flags = behaviourFlags;
      }
    }

    if (layerObj.containsKey("offset")) {
      state.lightLists[index]->setOffset(layerObj["offset"].as<float>());
    }

    if (!(layerObj.containsKey("colors") && layerObj["colors"].is<JsonArrayConst>())) {
      continue;
    }

    std::vector<int64_t> layerColors;
    std::vector<float> layerPositions;

    JsonArrayConst colorsArray = layerObj["colors"].as<JsonArrayConst>();
    for (JsonVariantConst color : colorsArray) {
      layerColors.push_back(color.as<int64_t>());
    }

    if (layerObj.containsKey("positions") && layerObj["positions"].is<JsonArrayConst>()) {
      JsonArrayConst positionsArray = layerObj["positions"].as<JsonArrayConst>();
      for (JsonVariantConst pos : positionsArray) {
        layerPositions.push_back(pos.as<float>());
      }
    }

    if (layerPositions.size() != layerColors.size()) {
      layerPositions.clear();
      for (size_t i = 0; i < layerColors.size(); i++) {
        const float pos = (layerColors.size() == 1)
                              ? 0.0f
                              : static_cast<float>(i) / static_cast<float>(layerColors.size() - 1);
        layerPositions.push_back(pos);
      }
    }

    if (layerColors.empty()) {
      continue;
    }

    Palette palette(layerColors, layerPositions);
    if (layerObj.containsKey("colorRule")) {
      palette.setColorRule(layerObj["colorRule"].as<int8_t>());
    }
    if (layerObj.containsKey("interMode")) {
      palette.setInterpolationMode(layerObj["interMode"].as<int8_t>());
    }
    if (layerObj.containsKey("wrapMode")) {
      palette.setWrapMode(layerObj["wrapMode"].as<int8_t>());
    }
    if (layerObj.containsKey("segmentation")) {
      palette.setSegmentation(layerObj["segmentation"].as<float>());
    }
    state.lightLists[index]->setPalette(palette);
  }
}

inline void serializeStateLayers(const State& state, JsonArray layersArray) {
  for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
    if (state.lightLists[i] == nullptr) {
      continue;
    }

    JsonObject layerObj = layersArray.createNestedObject();
    layerObj["index"] = i;
    layerObj["visible"] = state.lightLists[i]->visible;
    layerObj["brightness"] = state.lightLists[i]->maxBri;
    layerObj["blendMode"] = static_cast<uint8_t>(state.lightLists[i]->blendMode);
    layerObj["speed"] = state.lightLists[i]->speed;
    layerObj["ease"] = state.lightLists[i]->easeIndex;
    layerObj["fadeSpeed"] = state.lightLists[i]->fadeSpeed;
    layerObj["offset"] = state.lightLists[i]->getOffset();

    if (state.lightLists[i]->behaviour != nullptr) {
      layerObj["behaviourFlags"] = state.lightLists[i]->behaviour->flags;
    }

    if (!state.lightLists[i]->hasPalette()) {
      continue;
    }

    const Palette& palette = state.lightLists[i]->getPalette();
    JsonArray colorsArray = layerObj.createNestedArray("colors");
    const std::vector<int64_t>& colors = palette.getColors();
    for (const auto& color : colors) {
      colorsArray.add(color);
    }

    JsonArray positionsArray = layerObj.createNestedArray("positions");
    const std::vector<float>& positions = palette.getPositions();
    for (const auto& pos : positions) {
      positionsArray.add(pos);
    }

    layerObj["colorRule"] = palette.getColorRule();
    layerObj["interMode"] = palette.getInterpolationMode();
    layerObj["wrapMode"] = palette.getWrapMode();
    layerObj["segmentation"] = palette.getSegmentation();
  }
}

}  // namespace lightgraph_layer_json
