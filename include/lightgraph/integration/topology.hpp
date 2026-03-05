#pragma once

#include "../../../src/HashMap.h"
#include "../../../src/topology/Owner.h"
#include "../../../src/topology/Port.h"
#include "../../../src/topology/Weight.h"
#include "../../../src/topology/Model.h"
#include "../../../src/topology/Intersection.h"
#include "../../../src/topology/Connection.h"
#include "../../../src/topology/TopologyObject.h"

/**
 * @file topology.hpp
 * @brief Public topology and routing graph types.
 */

namespace lightgraph::integration {

using Owner = ::Owner;
using Port = ::Port;
using InternalPort = ::InternalPort;
using ExternalPort = ::ExternalPort;
using Weight = ::Weight;
using Model = ::Model;
using Intersection = ::Intersection;
using Connection = ::Connection;
using Object = ::TopologyObject;
using PixelGap = ::PixelGap;

} // namespace lightgraph::integration
