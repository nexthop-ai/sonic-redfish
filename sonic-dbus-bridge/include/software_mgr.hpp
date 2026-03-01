// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 SONiC Project

#pragma once

#include "object_mapper.hpp"
#include "types.hpp"
#include "version_utils.hpp"

#include <sdbusplus/asio/object_server.hpp>

#include <memory>
#include <string>
#include <vector>

namespace sonic::dbus_bridge
{

/**
 * @brief Software Manager for FirmwareInventory
 *
 * Manages software/firmware version objects on D-Bus.
 * Creates D-Bus objects at /xyz/openbmc_project/software/<id>
 * with xyz.openbmc_project.Software.Version interface.
 *
 * Follows the same pattern as UserMgr - self-contained class that:
 * 1. Reads data sources (sonic_version.yml)
 * 2. Creates D-Bus objects
 * 3. Registers with ObjectMapper
 */
class SoftwareMgr
{
  public:
    /**
     * @brief Construct a new Software Mgr object
     *
     * @param server sdbusplus object server for software D-Bus service
     * @param objectMapper ObjectMapper service for registration (optional)
     */
    SoftwareMgr(sdbusplus::asio::object_server& server,
                ObjectMapperService* objectMapper = nullptr);

    /**
     * @brief Initialize software manager
     *
     * Reads software versions and creates D-Bus objects.
     *
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Get the software info list
     *
     * @return const reference to software info vector
     */
    const std::vector<SoftwareInfo>& getSoftwareList() const
    {
        return softwareList_;
    }

  private:
    /** @brief sdbusplus object server for software service */
    sdbusplus::asio::object_server& server_;

    /** @brief ObjectMapper service for registration */
    ObjectMapperService* objectMapper_;

    /** @brief List of software items */
    std::vector<SoftwareInfo> softwareList_;

    /** @brief D-Bus interfaces (kept alive) */
    std::vector<std::shared_ptr<sdbusplus::asio::dbus_interface>> interfaces_;

    /**
     * @brief Build software inventory by reading data sources
     *
     * Reads /etc/sonic/sonic_version.yml and populates softwareList_
     */
    void buildSoftwareInventory();

    /**
     * @brief Create D-Bus object for a software item
     *
     * @param sw Software info
     * @return true on success
     */
    bool createSoftwareObject(const SoftwareInfo& sw);

    /**
     * @brief Register software object with ObjectMapper
     *
     * @param objPath D-Bus object path
     */
    void registerWithObjectMapper(const std::string& objPath);
};

} // namespace sonic::dbus_bridge

