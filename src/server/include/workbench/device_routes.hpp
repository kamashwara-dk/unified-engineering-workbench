#pragma once
// device_routes.hpp -- HTTP routes for simulated device adapters.
// All payloads include "simulated": true.
#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable: 4127 4244 4456 4702 4996)
#endif
#include "httplib.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif
#include "workbench/devices.hpp"
namespace workbench {
void register_device_routes(httplib::Server& svr, DeviceRegistry& reg);
}  // namespace workbench