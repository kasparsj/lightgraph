#pragma once

#include "../../src/HashMap.h"
#include "../../src/LPOwner.h"
#include "../../src/Port.h"
#include "../../src/Weight.h"
#include "../../src/Model.h"
#include "../../src/Intersection.h"
#include "../../src/Connection.h"
#include "../../src/LPObject.h"

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
