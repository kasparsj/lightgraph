#pragma once

#include "HashMap.h"
#include "topology/Owner.h"
#include "topology/Port.h"
#include "topology/Weight.h"
#include "topology/Model.h"
#include "topology/Intersection.h"
#include "topology/Connection.h"
#include "topology/TopologyObject.h"

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
