///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <string>
#include <vector>

namespace sonic
{
namespace user
{

// Place where all user objects has to be created
constexpr auto usersObjPath = "/xyz/openbmc_project/user";

class UserMgr; // Forward declaration for UserMgr

/** @class Users
 *  @brief Lists User objects and its properties
 */
class Users
{
  public:
    Users() = delete;
    Users(const Users&) = delete;
    Users& operator=(const Users&) = delete;
    Users(Users&&) = delete;
    Users& operator=(Users&&) = delete;

    /** @brief Constructs Users object.
     *
     *  @param[in] server  - sdbusplus asio object server
     *  @param[in] path    - D-Bus path
     *  @param[in] groups  - users group list
     *  @param[in] priv    - users privilege
     *  @param[in] enabled - user enabled state
     *  @param[in] parent  - user manager - parent object
     */
    Users(sdbusplus::asio::object_server& server, const std::string& path,
          std::vector<std::string> groups, const std::string& priv,
          bool enabled, UserMgr& parent);

    /** @brief Destructor - removes D-Bus interfaces */
    ~Users();

    /** @brief Get user name
     *
     *  @return user name string
     */
    std::string getUserName() const
    {
        return userName;
    }

    /** @brief Get user privilege */
    const std::string& getUserPrivilege() const { return userPrivilege; }

    /** @brief Get user groups */
    const std::vector<std::string>& getUserGroups() const { return userGroups; }

    /** @brief Get user enabled state */
    bool getUserEnabled() const { return userEnabled; }

    /** @brief Get user locked for failed attempt state */
    bool getUserLockedForFailedAttempt() const { return userLockedForFailedAttempt; }

    /** @brief Get user password expired state */
    bool getUserPasswordExpired() const { return userPasswordExpired; }

  private:
    std::string userName;
    UserMgr& manager;
    sdbusplus::asio::object_server& server;

    // User attributes
    std::string userPrivilege;
    std::vector<std::string> userGroups;
    bool userEnabled;
    bool userLockedForFailedAttempt = false;
    bool userPasswordExpired = false;

    // D-Bus interface for User.Attributes
    std::shared_ptr<sdbusplus::asio::dbus_interface> userIface;
};

} // namespace user
} // namespace sonic
