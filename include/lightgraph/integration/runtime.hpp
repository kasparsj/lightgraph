#pragma once

#include "runtime/Behaviour.h"
#include "runtime/EmitParams.h"
#include "runtime/RuntimeLight.h"
#include "runtime/Light.h"
#include "runtime/LightList.h"
#include "runtime/BgLight.h"
#include "runtime/State.h"
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
