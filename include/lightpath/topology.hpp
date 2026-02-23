#pragma once

#include "../../src/HashMap.h"
#include "../../src/topology/LPOwner.h"
#include "../../src/topology/Port.h"
#include "../../src/topology/Weight.h"
#include "../../src/topology/Model.h"
#include "../../src/topology/Intersection.h"
#include "../../src/topology/Connection.h"
#include "../../src/topology/LPObject.h"

/**
 * @file topology.hpp
 * @brief Public topology and routing graph types.
 */

namespace lightpath {

using Owner = ::LPOwner;
using LPOwner = ::LPOwner;  // compatibility alias
using Port = ::Port;
using InternalPort = ::InternalPort;
using ExternalPort = ::ExternalPort;
using Weight = ::Weight;
using Model = ::Model;
using Intersection = ::Intersection;
using Connection = ::Connection;
using Object = ::LPObject;
using LPObject = ::LPObject;  // compatibility alias
using PixelGap = ::PixelGap;

}  // namespace lightpath
