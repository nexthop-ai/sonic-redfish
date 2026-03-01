// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024 SONiC Project

#pragma once

#include <hiredis/hiredis.h>
#include <map>
#include <optional>
#include <string>

namespace sonic::dbus_bridge
{

/**
 * @brief Redis client adapter for Switch CPU databases
 *
 * Connects to Switch CPU's Redis via USB network interface (usb0)
 * to read switch firmware version and other switch-specific data.
 *
 * BMC IP: 192.168.100.1 (usb0)
 * Switch IP: 192.168.100.2 (usb0)
 */
class SwitchRedisAdapter
{
  public:
    /**
     * @brief Construct a new Switch Redis Adapter
     *
     * @param host Switch Redis host (default: 192.168.100.2)
     * @param port Switch Redis port (default: 6379)
     */
    SwitchRedisAdapter(const std::string& host = "192.168.100.2",
                       int port = 6379);

    ~SwitchRedisAdapter();

    // Disable copy
    SwitchRedisAdapter(const SwitchRedisAdapter&) = delete;
    SwitchRedisAdapter& operator=(const SwitchRedisAdapter&) = delete;

    /**
     * @brief Connect to Switch Redis STATE_DB
     *
     * @return true if connection succeeded
     * @return false if connection failed
     */
    bool connect();

    /**
     * @brief Check if connected to Switch Redis
     */
    bool isConnected() const { return stateDbContext_ != nullptr; }

    /**
     * @brief Get switch SONiC version from STATE_DB
     *
     * Reads VERSIONS|sonic hash for build_version field
     *
     * @return Version string if available, nullopt otherwise
     */
    std::optional<std::string> getSwitchVersion();

    /**
     * @brief Get all version fields from STATE_DB
     *
     * Reads VERSIONS|sonic hash
     *
     * @return Map of field->value
     */
    std::map<std::string, std::string> getVersionInfo();

  private:
    std::string host_;
    int port_;
    redisContext* stateDbContext_{nullptr};

    /**
     * @brief Connect to a specific Redis database
     *
     * @param dbIndex Database index (6 for STATE_DB)
     * @return redisContext* on success, nullptr on failure
     */
    redisContext* connectToDb(int dbIndex);

    /**
     * @brief Get all fields from a Redis hash
     */
    std::map<std::string, std::string> hgetall(const std::string& key);

    /**
     * @brief Get a single field from a Redis hash
     */
    std::optional<std::string> hget(const std::string& key,
                                    const std::string& field);
};

} // namespace sonic::dbus_bridge

