///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include "users.hpp"
#include "object_mapper.hpp"
#include "redis_adapter.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sonic
{
namespace user
{

using UserInfoMap = std::map<std::string, std::variant<std::string, std::vector<std::string>, bool>>;

class UserMgr
{
	  public:
    /** @brief Constructs UserMgr object.
     *
     *  @param[in] server        - sdbusplus asio object server
     *  @param[in] path          - D-Bus path
     *  @param[in] objectMapper  - ObjectMapper service for registration (optional)
     *  @param[in] redisAdapter  - CONFIG_DB access for the trusted client
     *                             certificate common names (optional)
     */
    UserMgr(sdbusplus::asio::object_server& server, const char* path,
            sonic::dbus_bridge::ObjectMapperService* objectMapper = nullptr,
            sonic::dbus_bridge::RedisAdapter* redisAdapter = nullptr);

    /** @brief Get reference to user objects map
     *
     *  @return const reference to usersList map
     */
    const std::unordered_map<std::string, std::unique_ptr<Users>>& getUsers() const
    {
        return usersList;
    }

    /** @brief get user info
     *  Returns user properties for the given user name
     *
     *  @param[in] userName - Name of the user
     *  @return - map of user properties
     */
    UserInfoMap getUserInfo(const std::string& userName);

  private:
    /** @brief sdbusplus asio object server */
    sdbusplus::asio::object_server& server;

    /** @brief object path */
    const std::string path;

    /** @brief ObjectMapper service for user registration */
    sonic::dbus_bridge::ObjectMapperService* objectMapper_;

    /** @brief CONFIG_DB access for the trusted common name list */
    sonic::dbus_bridge::RedisAdapter* redisAdapter_;

    /** @brief User.Manager D-Bus interface */
    std::shared_ptr<sdbusplus::asio::dbus_interface> userMgrIface;

    /** @brief privilege manager container */
    const std::vector<std::string> privMgr = {"priv-admin", "priv-operator",
                                              "priv-user"};

    /** @brief all groups that can be assigned to users */
    const std::vector<std::string> allGroups = {"redfish"};

    /** @brief map container to hold users object (only admin) */
    std::unordered_map<std::string, std::unique_ptr<Users>> usersList;

    /** @brief initialize the user manager objects
     *  Creates the D-Bus object for the virtual Redfish identity
     */
    void initUserObjects(void);

    /** @brief check the common name against the configured trusted list
     *
     *  Reads REDFISH|certs client_crt_cname on every call, so a change takes
     *  effect on the next request. An unset or empty list refuses every
     *  common name, as the SONiC REST API server does, and so does a failed
     *  read rather than dropping the check.
     *
     *  @param[in] commonName - common name taken from the client certificate
     *
     *  @return true if the common name is accepted
     */
    bool isCommonNameTrusted(const std::string& commonName);
};

} // namespace user
} // namespace sonic
