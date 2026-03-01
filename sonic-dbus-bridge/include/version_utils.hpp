// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 SONiC Project

#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <syslog.h>

namespace sonic::dbus_bridge
{

/**
 * @brief SONiC version information from sonic_version.yml
 */
struct SonicVersion
{
    std::string buildVersion;    // build_version field
    std::string debianVersion;   // debian_version field
    std::string kernelVersion;   // kernel_version field
    std::string asicType;        // asic_type field
    std::string commitId;        // commit_id field
    std::string buildDate;       // build_date field
    std::string builtBy;         // built_by field
};

/**
 * @brief Parse a simple YAML key-value line
 * 
 * Handles format: key: 'value' or key: "value" or key: value
 * 
 * @param line The line to parse
 * @param key Output key
 * @param value Output value
 * @return true if successfully parsed
 */
inline bool parseYamlLine(const std::string& line, std::string& key,
                          std::string& value)
{
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#')
    {
        return false;
    }

    // Find colon separator
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos)
    {
        return false;
    }

    // Extract key (trim whitespace)
    key = line.substr(0, colonPos);
    size_t keyStart = key.find_first_not_of(" \t");
    size_t keyEnd = key.find_last_not_of(" \t");
    if (keyStart == std::string::npos)
    {
        return false;
    }
    key = key.substr(keyStart, keyEnd - keyStart + 1);

    // Extract value (trim whitespace and quotes)
    value = line.substr(colonPos + 1);
    size_t valStart = value.find_first_not_of(" \t");
    if (valStart == std::string::npos)
    {
        value = "";
        return true;
    }
    size_t valEnd = value.find_last_not_of(" \t\r\n");
    value = value.substr(valStart, valEnd - valStart + 1);

    // Remove surrounding quotes if present
    if (value.size() >= 2)
    {
        char first = value.front();
        char last = value.back();
        if ((first == '\'' && last == '\'') || (first == '"' && last == '"'))
        {
            value = value.substr(1, value.size() - 2);
        }
    }

    return true;
}

/**
 * @brief Read SONiC version from sonic_version.yml file
 * 
 * @param path Path to sonic_version.yml (default: /etc/sonic/sonic_version.yml)
 * @return Optional SonicVersion, nullopt on failure
 */
inline std::optional<SonicVersion> readSonicVersion(
    const std::string& path = "/etc/sonic/sonic_version.yml")
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        syslog(LOG_WARNING, "Failed to open %s", path.c_str());
        return std::nullopt;
    }

    SonicVersion version;
    std::string line;

    while (std::getline(file, line))
    {
        std::string key, value;
        if (!parseYamlLine(line, key, value))
        {
            continue;
        }

        if (key == "build_version")
        {
            version.buildVersion = value;
        }
        else if (key == "debian_version")
        {
            version.debianVersion = value;
        }
        else if (key == "kernel_version")
        {
            version.kernelVersion = value;
        }
        else if (key == "asic_type")
        {
            version.asicType = value;
        }
        else if (key == "commit_id")
        {
            version.commitId = value;
        }
        else if (key == "build_date")
        {
            version.buildDate = value;
        }
        else if (key == "built_by")
        {
            version.builtBy = value;
        }
    }

    if (version.buildVersion.empty())
    {
        syslog(LOG_WARNING, "No build_version found in %s", path.c_str());
        return std::nullopt;
    }

    syslog(LOG_INFO, "Read SONiC version: %s from %s",
           version.buildVersion.c_str(), path.c_str());
    return version;
}

} // namespace sonic::dbus_bridge

