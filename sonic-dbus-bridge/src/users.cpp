///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#include "users.hpp"
#include "user_mgr.hpp"
#include "logger.hpp"


namespace sonic
{
namespace user
{

/** @brief Constructs Users object.
 *
 *  @param[in] server  - sdbusplus asio object server
 *  @param[in] path    - D-Bus path
 *  @param[in] groups  - users group list
 *  @param[in] priv    - user privilege
 *  @param[in] enabled - user enabled state
 *  @param[in] parent  - user manager - parent object
 */
Users::Users(sdbusplus::asio::object_server& server, const std::string& path,
             std::vector<std::string> groups, const std::string& priv,
             bool enabled, UserMgr& parent) :
    userName(sdbusplus::message::object_path(path).filename()),
    manager(parent),
    server(server),
    userPrivilege(priv),
    userGroups(std::move(groups)),
    userEnabled(enabled)
{
    // Create D-Bus interface for User.Attributes
    userIface = server.add_interface(path, "xyz.openbmc_project.User.Attributes");

    // Register UserPrivilege property (read-only)
    userIface->register_property("UserPrivilege", userPrivilege);

    // Register UserGroups property (read-only)
    userIface->register_property("UserGroups", userGroups);

    // Register UserEnabled property (read-only)
    userIface->register_property("UserEnabled", userEnabled);

    userIface->register_property("UserLockedForFailedAttempt", false);
    userIface->register_property("UserPasswordExpired", false);

    userIface->initialize();

    LOG_INFO("User object created for: %s", userName.c_str());
}

Users::~Users()
{
    LOG_INFO("Removing D-Bus interfaces for user: %s", userName.c_str());

    // Remove interface from D-Bus
    server.remove_interface(userIface);
}

} // namespace user
} // namespace sonic
