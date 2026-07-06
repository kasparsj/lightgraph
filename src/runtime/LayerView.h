#ifndef PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LAYERVIEW_H_
#define PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LAYERVIEW_H_

#pragma once

#include <vector>

#include "State.h"
#include "../rendering/Palette.h"

struct PaletteView {
    std::vector<int64_t> colors;
    std::vector<float> positions;
    int8_t colorRule = -1;
    int8_t interpolationMode = 1;
    int8_t wrapMode = 0;
    float segmentation = 0.0f;
};

struct LayerView {
    uint8_t index = 0;
    bool editable = false;
    bool visible = false;
    uint8_t brightness = 0;
    float speed = 0.0f;
    uint8_t fadeSpeed = 0;
    uint8_t ease = 0;
    uint8_t blendMode = 0;
    uint16_t behaviourFlags = 0;
    float offset = 0.0f;
    PaletteView palette;
};

inline Palette makePaletteFromView(const PaletteView& view);

inline PaletteView makePaletteView(const Palette& palette) {
    PaletteView view;
    view.colors = palette.getColors();
    view.positions = palette.getPositions();
    view.colorRule = palette.getColorRule();
    view.interpolationMode = palette.getInterpolationMode();
    view.wrapMode = palette.getWrapMode();
    view.segmentation = palette.getSegmentation();
    return view;
}

inline PaletteView normalizePaletteView(const PaletteView& view) {
    return makePaletteView(makePaletteFromView(view));
}

inline Palette makePaletteFromView(const PaletteView& view) {
    Palette palette(view.colors, view.positions);
    palette.setColorRule(view.colorRule);
    palette.setInterpolationMode(view.interpolationMode);
    palette.setWrapMode(view.wrapMode);
    palette.setSegmentation(view.segmentation);
    return palette;
}

inline std::vector<LayerView> snapshotLayers(const State& state, bool editableOnly = false) {
    std::vector<LayerView> layers;
    layers.reserve(MAX_LIGHT_LISTS);
    for (uint8_t i = 0; i < MAX_LIGHT_LISTS; i++) {
        const LightList* list = state.lightLists[i];
        if (list == nullptr) {
            continue;
        }
        if (editableOnly && !list->editable) {
            continue;
        }

        LayerView view;
        view.index = i;
        view.editable = list->editable;
        view.visible = list->visible;
        view.brightness = list->maxBri;
        view.speed = list->speed;
        view.fadeSpeed = list->fadeSpeed;
        view.ease = list->easeIndex;
        view.blendMode = static_cast<uint8_t>(list->blendMode);
        view.behaviourFlags = (list->behaviour != nullptr) ? list->behaviour->flags : 0;
        view.offset = list->getOffset();
        view.palette = makePaletteView(list->getPalette());
        layers.push_back(std::move(view));
    }
    return layers;
}

#endif  // PACKAGES_LIGHTGRAPH_SRC_RUNTIME_LAYERVIEW_H_
