#pragma once

#include <memory>

#include "../../src/runtime/Behaviour.h"
#include "../../src/runtime/EmitParams.h"
#include "../../src/runtime/LPLight.h"
#include "../../src/runtime/Light.h"
#include "../../src/runtime/LightList.h"
#include "../../src/runtime/BgLight.h"
#include "../../src/runtime/State.h"
#include "types.hpp"
#include "topology.hpp"

/**
 * @file runtime.hpp
 * @brief Public runtime/state and animation types.
 */

namespace lightpath {

using EmitParam = ::EmitParam;
using EmitParams = ::EmitParams;
using Behaviour = ::Behaviour;
using RuntimeLight = ::LPLight;
using Light = ::Light;
using LightList = ::LightList;
using BgLight = ::BgLight;
using RuntimeState = ::State;

/**
 * @brief RAII facade owning a topology object and runtime state.
 *
 * This provides a stable entry point for host integrations while preserving
 * the underlying legacy engine behavior.
 */
class Engine {
  public:
    explicit Engine(std::unique_ptr<Object> object)
        : object_(std::move(object)), state_(*object_) {
    }

    Object& object() noexcept {
        return *object_;
    }

    const Object& object() const noexcept {
        return *object_;
    }

    RuntimeState& state() noexcept {
        return state_;
    }

    const RuntimeState& state() const noexcept {
        return state_;
    }

    /**
     * @brief Advance runtime by one frame at the provided timestamp.
     */
    void update(unsigned long millis) {
        lightpath::millis() = millis;
        state_.autoEmit(millis);
        state_.update();
    }

  private:
    std::unique_ptr<Object> object_;
    RuntimeState state_;
};

}  // namespace lightpath
