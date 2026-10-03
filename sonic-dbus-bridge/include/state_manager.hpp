///////////////////////////////////////
// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Nexthop AI
// Copyright (C) 2024 SONiC Project
// Author: Nexthop AI
// Author: SONiC Project
// Author: Chinmoy Dey <chinmoy@nexthop.ai>
// License file: sonic-redfish/LICENSE
///////////////////////////////////////

#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <string>
#include <queue>
#include <chrono>
#include <vector>
#include "redis_adapter.hpp"
#include "redis_state_publisher.hpp"
#include "types.hpp"

namespace sonic::dbus_bridge
{

/**
 * @brief State manager for Switch-Host power state and transitions
 *
 * Implements OpenBMC xyz.openbmc_project.State.Host (host0) and
 * xyz.openbmc_project.State.Chassis (chassis0) on D-Bus for bmcweb.
 *
 * Source of truth for CurrentHostState / CurrentPowerState is the
 * HOST_STATE|switch-host hash in STATE_DB, owned by bmcctld. The bridge
 * never updates host state optimistically.
 *
 * Request flow:
 * 1. bmcweb writes RequestedHostTransition / RequestedPowerTransition
 * 2. Setter validates (host_state_mapping.hpp) and queues the action
 * 3. Action is published to RACK_MANAGER_COMMAND|<id> (status PENDING)
 * 4. bmcctld executes it, updates HOST_STATE|switch-host and finally
 *    sets the command status to DONE / FAILED
 * 5. UpdateEngine forwards both STATE_DB changes here:
 *    - onHostStateChanged()     -> CurrentHostState + PropertiesChanged
 *    - onCommandStatusChanged() -> logs result, releases the action slot
 */
class StateManager
{
  public:
    /**
     * @brief Construct a new State Manager
     *
     * @param server sdbusplus object server (State.Host / State.Chassis connection)
     * @param io Boost ASIO io_context for async operations
     * @param redisAdapter STATE_DB reader used to seed the initial host state
     */
    StateManager(sdbusplus::asio::object_server& server,
                 boost::asio::io_context& io,
                 std::shared_ptr<RedisAdapter> redisAdapter);

    /**
     * @brief Destructor - cleanup is automatic (RAII)
     */
    ~StateManager() = default;

    /**
     * @brief Create D-Bus state objects
     *
     * Creates /xyz/openbmc_project/state/host0 (State.Host) and
     * /xyz/openbmc_project/state/chassis0 (State.Chassis). The initial
     * state is read from HOST_STATE|switch-host.
     *
     * @return true on success, false on error
     */
    bool createStateObjects();

    /**
     * @brief HOST_STATE|switch-host changed in STATE_DB
     *
     * Thread-safe: work is posted to the io_context.
     */
    void onHostStateChanged(const HostStateInfo& info);

    /**
     * @brief RACK_MANAGER_COMMAND|<id> changed in STATE_DB
     *
     * Thread-safe: work is posted to the io_context.
     */
    void onCommandStatusChanged(const RackManagerCommandInfo& info);

    /// Current xyz.openbmc_project.State.Host.HostState value
    const std::string& currentHostState() const { return currentHostState_; }

  private:
    sdbusplus::asio::object_server& server_;
    boost::asio::io_context& io_;
    std::shared_ptr<RedisAdapter> redisAdapter_;

    // D-Bus interfaces
    std::shared_ptr<sdbusplus::asio::dbus_interface> hostStateIface_;
    std::shared_ptr<sdbusplus::asio::dbus_interface> chassisStateIface_;

    // State tracking (mirrors HOST_STATE|switch-host)
    std::string currentHostState_;
    std::string currentChassisPowerState_;
    std::string lastRequestedHostTransition_;
    std::string lastRequestedChassisTransition_;

    // Redis publisher for RACK_MANAGER_COMMAND
    std::unique_ptr<RedisStatePublisher> redisPublisher_;

    // Action queue for async processing
    struct ActionRequest
    {
        std::string transition;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::queue<ActionRequest> actionQueue_;
    std::unique_ptr<boost::asio::steady_timer> actionTimer_;
    bool actionInProgress_{false};

    // Command published to bmcctld and awaiting DONE / FAILED
    std::string pendingCommandId_;
    std::string pendingCommand_;
    std::unique_ptr<boost::asio::steady_timer> commandTimer_;

    // Maximum queue size to prevent overflow
    static constexpr size_t MAX_QUEUE_SIZE = 10;

    /**
     * @brief Seed currentHostState_ from HOST_STATE|switch-host
     */
    void initializeFromDb();

    /**
     * @brief Validate and enqueue a transition requested over D-Bus
     *
     * @param transition D-Bus Host or Chassis transition value
     * @throws std::invalid_argument / std::runtime_error (mapped to a D-Bus error)
     */
    void queueTransition(const std::string& transition);

    /**
     * @brief Process next action in queue
     *
     * Called when an action is queued or when the previous command reaches
     * a terminal status. Non-blocking - schedules async execution via timer.
     */
    void processNextAction();

    /**
     * @brief Publish a transition as a RACK_MANAGER_COMMAND
     *
     * @param transition D-Bus transition value
     * @return true if the command was published and is now pending
     */
    bool executeHostTransition(const std::string& transition);

    /**
     * @brief Apply a HOST_STATE|switch-host snapshot (io_context thread)
     */
    void applyHostState(const HostStateInfo& info);

    /**
     * @brief Handle a RACK_MANAGER_COMMAND update (io_context thread)
     */
    void handleCommandStatus(const RackManagerCommandInfo& info);

    /**
     * @brief Pending command did not reach DONE / FAILED in time
     */
    void onCommandTimeout(const std::string& commandId);

    /**
     * @brief Release the action slot and continue with the queue
     */
    void completePendingCommand();

    /**
     * @brief Update host / chassis state and emit PropertiesChanged
     *
     * @param newState New xyz.openbmc_project.State.Host.HostState value
     */
    void updateHostState(const std::string& newState);
};

} // namespace sonic::dbus_bridge

