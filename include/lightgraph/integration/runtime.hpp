#pragma once

#include "../../../src/runtime/Behaviour.h"
#include "../../../src/runtime/EmitParams.h"
#include "../../../src/runtime/RuntimeLight.h"
#include "../../../src/runtime/Light.h"
#include "../../../src/runtime/LightList.h"
#include "../../../src/runtime/BgLight.h"
#include "../../../src/runtime/State.h"
#include "topology.hpp"

/**
 * @file runtime.hpp
 * @brief Public runtime/state and animation types.
 */

namespace lightgraph::integration {

using EmitParam = ::EmitParam;
using EmitParams = ::EmitParams;
using Behaviour = ::Behaviour;
using RuntimeLight = ::RuntimeLight;
using Light = ::Light;
using LightList = ::LightList;
using BgLight = ::BgLight;
using RuntimeState = ::State;

} // namespace lightgraph::integration
