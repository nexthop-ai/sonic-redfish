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

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace sonic::dbus_bridge
{

/**
 * @brief Minimal implementation of xyz.openbmc_project.ObjectMapper
 *
 * This is a very small, in-process mapper that only knows about
 * objects exported by sonic-dbus-bridge itself. It is intentionally
 * limited to the subset of methods used by bmcweb:
 *   - GetObject
 *   - GetSubTree
 *   - GetSubTreePaths
 *   - GetAssociatedSubTreePaths (stub, returns empty set)
 */
class ObjectMapperService
{
  public:
    explicit ObjectMapperService(sdbusplus::asio::object_server& server);
    ~ObjectMapperService() = default;

    /**
     * @brief Register the ObjectMapper D-Bus object/vtable
     *
     * Must be called after the process has acquired the
     * xyz.openbmc_project.ObjectMapper bus name.
     */
    bool initialize();

    /**
     * @brief Register or update a D-Bus object in the mapper registry.
     *
     * @param path        Object path
     * @param interfaces  Interfaces implemented at this path
     * @param serviceName Service name that owns this object (optional,
     *                    defaults to inventoryServiceName_)
     */
    void registerObject(const std::string& path,
                        const std::vector<std::string>& interfaces,
                        const std::string& serviceName = "");

    /**
     * @brief Unregister a D-Bus object from the mapper registry.
     *
     * @param path Object path to remove
     */
    void unregisterObject(const std::string& path);

  private:
    sdbusplus::asio::object_server& server_;

    // Registry: object path -> {interfaces, serviceName}
    struct ObjectInfo
    {
        std::vector<std::string> interfaces;
        std::string serviceName;
    };
    std::map<std::string, ObjectInfo> objects_;

    // D-Bus interface for ObjectMapper
    std::shared_ptr<sdbusplus::asio::dbus_interface> mapperIface_;

    // Helpers
    static bool pathIsUnder(const std::string& root, const std::string& path);

    // Method implementations (return types match sdbusplus method registration)
    using GetObjectResult =
        std::map<std::string, std::vector<std::string>>;
    using GetSubTreeResult =
        std::map<std::string, std::map<std::string, std::vector<std::string>>>;
    using GetSubTreePathsResult = std::vector<std::string>;

    GetObjectResult getObject(const std::string& path,
                              const std::vector<std::string>& interfaces);

    GetSubTreeResult getSubTree(const std::string& subtree, int32_t depth,
                                const std::vector<std::string>& interfaces);

    GetSubTreePathsResult getSubTreePaths(const std::string& subtree,
                                          int32_t depth,
                                          const std::vector<std::string>& interfaces);

    GetSubTreePathsResult getAssociatedSubTreePaths(
        const std::string& associatedPath, const std::string& subtree,
        int32_t depth, const std::vector<std::string>& interfaces);
};

} // namespace sonic::dbus_bridge

