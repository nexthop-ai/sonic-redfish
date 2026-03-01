///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include "types.hpp"
#include <string>
#include <optional>

namespace sonic::dbus_bridge
{

/**
 * @brief Adapter for reading platform.json
 * 
 * Parses /usr/share/sonic/device/<platform>/platform.json
 * to extract hardware topology information.
 */
class PlatformJsonAdapter
{
  public:
    /**
     * @brief Construct a new Platform Json Adapter
     * 
     * @param platformJsonPath Path to platform.json (can contain ${PLATFORM})
     */
    explicit PlatformJsonAdapter(const std::string& platformJsonPath);

    /**
     * @brief Load and parse platform.json
     * 
     * @return true if file loaded and parsed successfully
     * @return false if file not found or parse error
     */
    bool load();

    /**
     * @brief Check if platform.json was loaded successfully
     */
    bool isLoaded() const { return loaded_; }

    /**
     * @brief Get platform description
     * 
     * @return PlatformDescription with chassis/fan/PSU/thermal info
     */
    PlatformDescription getPlatformDescription() const;

    /**
     * @brief Get chassis name from platform.json
     */
    std::optional<std::string> getChassisName() const;

    /**
     * @brief Get chassis part number from platform.json
     */
    std::optional<std::string> getChassisPartNumber() const;

    /**
     * @brief Get chassis hardware version from platform.json
     */
    std::optional<std::string> getChassisHardwareVersion() const;

  private:
    std::string platformJsonPath_;
    bool loaded_{false};
    PlatformDescription description_;

    /**
     * @brief Expand environment variables in path
     * 
     * Replaces ${PLATFORM} with value from environment
     */
    std::string expandPath(const std::string& path) const;

    /**
     * @brief Parse JSON file
     * 
     * @param path File path
     * @return true on success
     */
    bool parseJson(const std::string& path);
};

} // namespace sonic::dbus_bridge

