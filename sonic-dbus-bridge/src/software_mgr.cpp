// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 SONiC Project

#include "software_mgr.hpp"
#include "switch_redis_adapter.hpp"
#include "config.h"

#include <syslog.h>

namespace sonic::dbus_bridge
{

// D-Bus interface name
constexpr auto IFACE_SOFTWARE_VERSION = "xyz.openbmc_project.Software.Version";

// D-Bus object path prefix
constexpr auto OBJ_PATH_SOFTWARE_PREFIX = "/xyz/openbmc_project/software/";

SoftwareMgr::SoftwareMgr(sdbusplus::asio::object_server& server,
                         ObjectMapperService* objectMapper) :
    server_(server),
    objectMapper_(objectMapper)
{
    syslog(LOG_INFO, "SoftwareMgr created");
}

bool SoftwareMgr::initialize()
{
    syslog(LOG_INFO, "Initializing software manager...");

    // Build software inventory from data sources
    buildSoftwareInventory();

    if (softwareList_.empty())
    {
        syslog(LOG_WARNING, "No software items found");
        return true; // Not a fatal error
    }

    // Create D-Bus objects for each software item
    for (const auto& sw : softwareList_)
    {
        if (!createSoftwareObject(sw))
        {
            syslog(LOG_ERR, "Failed to create software object for %s",
                   sw.id.c_str());
            return false;
        }

        // Register with ObjectMapper
        std::string objPath = std::string(OBJ_PATH_SOFTWARE_PREFIX) + sw.id;
        registerWithObjectMapper(objPath);
    }

    syslog(LOG_INFO, "Software manager initialized with %zu items",
           softwareList_.size());
    return true;
}

void SoftwareMgr::buildSoftwareInventory()
{
    syslog(LOG_INFO, "Building software inventory...");

    // Read BMC version from /etc/sonic/sonic_version.yml
    auto bmcVersion = readSonicVersion();
    if (bmcVersion.has_value())
    {
        SoftwareInfo bmcSw;
        bmcSw.id = "bmc";
        bmcSw.version = bmcVersion->buildVersion;
        bmcSw.purpose = SoftwarePurpose::BMC;
        bmcSw.description = "BMC SONiC image";

        softwareList_.push_back(bmcSw);
        syslog(LOG_INFO, "Added BMC software: version=%s", bmcSw.version.c_str());
    }
    else
    {
        syslog(LOG_WARNING, "Could not read BMC version from sonic_version.yml");
    }

    // Read Switch CPU version from Switch Redis via USB network
    SwitchRedisAdapter switchRedis;
    std::string switchVersion = "switch-version-placeholder";

    if (switchRedis.connect())
    {
        auto version = switchRedis.getSwitchVersion();
        if (version.has_value())
        {
            switchVersion = version.value();
            syslog(LOG_INFO, "Got Switch version from Redis: %s",
                   switchVersion.c_str());
        }
        else
        {
            syslog(LOG_INFO,
                   "Switch version not in Redis, using placeholder");
        }
    }
    else
    {
        syslog(LOG_INFO,
               "Could not connect to Switch Redis, using placeholder");
    }

    SoftwareInfo switchSw;
    switchSw.id = "switch";
    switchSw.version = switchVersion;
    switchSw.purpose = SoftwarePurpose::Host;
    switchSw.description = "Switch SONiC image";

    softwareList_.push_back(switchSw);
    syslog(LOG_INFO, "Added Switch software: version=%s",
           switchSw.version.c_str());
}

bool SoftwareMgr::createSoftwareObject(const SoftwareInfo& sw)
{
    std::string objPath = std::string(OBJ_PATH_SOFTWARE_PREFIX) + sw.id;

    // Convert purpose enum to D-Bus string
    std::string purposeStr = "xyz.openbmc_project.Software.Version.VersionPurpose.";
    switch (sw.purpose)
    {
        case SoftwarePurpose::BMC:
            purposeStr += "BMC";
            break;
        case SoftwarePurpose::Host:
            purposeStr += "Host";
            break;
        default:
            purposeStr += "Other";
            break;
    }

    // Create Software.Version interface
    auto versionIface = server_.add_interface(objPath, IFACE_SOFTWARE_VERSION);

    // Store values for lambda capture (copy by value to avoid dangling refs)
    std::string version = sw.version;
    std::string purpose = purposeStr;

    versionIface->register_property_r<std::string>(
        "Version", std::string(""),
        sdbusplus::vtable::property_::const_,
        [version](const auto&) { return version; });

    versionIface->register_property_r<std::string>(
        "Purpose", std::string(""),
        sdbusplus::vtable::property_::const_,
        [purpose](const auto&) { return purpose; });

    versionIface->initialize();
    interfaces_.push_back(versionIface);

    syslog(LOG_INFO, "Created software object at %s (version=%s, purpose=%s)",
           objPath.c_str(), version.c_str(), purpose.c_str());
    return true;
}

void SoftwareMgr::registerWithObjectMapper(const std::string& objPath)
{
    if (!objectMapper_)
    {
        return;
    }

    objectMapper_->registerObject(
        objPath,
        {IFACE_SOFTWARE_VERSION},
        SOFTWARE_MANAGER_BUSNAME);

    syslog(LOG_INFO, "Registered software object with ObjectMapper: %s (service: %s)",
           objPath.c_str(), SOFTWARE_MANAGER_BUSNAME);
}

} // namespace sonic::dbus_bridge

