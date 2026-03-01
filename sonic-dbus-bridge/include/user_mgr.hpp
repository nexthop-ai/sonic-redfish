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
     */
    UserMgr(sdbusplus::asio::object_server& server, const char* path,
            sonic::dbus_bridge::ObjectMapperService* objectMapper = nullptr);

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
     *  Creates D-Bus object only for the admin user
     */
    void initUserObjects(void);

    /** @brief check if user is enabled
     *
     *  @param[in] userName - name of the user
     *
     *  @return true if enabled, false otherwise
     */
    bool isUserEnabled(const std::string& userName);

    /** @brief check if user exists in usersList
     *
     *  @param[in] userName - name of the user
     *
     *  @return true if user exists, false otherwise
     */
    bool isUserExist(const std::string& userName);
};

} // namespace user
} // namespace sonic
