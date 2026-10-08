///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2026 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// Author: Chinmoy Dey <chinmoy@nexthop.ai>
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include <sdbusplus/bus/match.hpp>

namespace redfish
{

class DbusHostStateMonitor
{
  public:
    DbusHostStateMonitor();
    sdbusplus::bus::match_t hostStateMonitor;
};

} // namespace redfish
