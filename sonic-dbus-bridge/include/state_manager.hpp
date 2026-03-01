#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <string>
#include <queue>
#include <chrono>
#include "redis_state_publisher.hpp"

namespace sonic::dbus_bridge
{

/**
 * @brief State manager for system state transitions
 * 
 * Handles system reset/reboot/power actions via D-Bus properties.
 * Implements OpenBMC xyz.openbmc_project.State.Host interface.
 * 
 * Flow:
 * 1. bmcweb writes to RequestedHostTransition property
 * 2. Property setter callback validates and queues action
 * 3. Async executor processes queue using Boost.Asio timer
 * 4. Publish to Redis STATE_DB via RedisStatePublisher
 * 5. Read back from Redis to verify write
 * 6. Update CurrentHostState property
 * 7. Emit D-Bus PropertiesChanged signal
 */
class StateManager
{
  public:
    /**
     * @brief Construct a new State Manager
     * 
     * @param server sdbusplus object server
     * @param io Boost ASIO io_context for async operations
     */
    StateManager(sdbusplus::asio::object_server& server,
                 boost::asio::io_context& io);

    /**
     * @brief Destructor - cleanup is automatic (RAII)
     */
    ~StateManager() = default;

    /**
     * @brief Create D-Bus state objects
     * 
     * Creates /xyz/openbmc_project/state/host0 with
     * xyz.openbmc_project.State.Host interface
     * 
     * @return true on success, false on error
     */
    bool createStateObjects();

  private:
    sdbusplus::asio::object_server& server_;
    boost::asio::io_context& io_;

    // D-Bus interface
    std::shared_ptr<sdbusplus::asio::dbus_interface> hostStateIface_;

    // State tracking
    std::string currentHostState_;
    std::string lastRequestedTransition_;

    // Redis publisher for state changes
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

    // Maximum queue size to prevent overflow
    static constexpr size_t MAX_QUEUE_SIZE = 10;

    /**
     * @brief Process next action in queue
     * 
     * Called when an action is queued or when previous action completes.
     * Non-blocking - schedules async execution via timer.
     */
    void processNextAction();

    /**
     * @brief Execute host transition action
     * 
     * @param transition D-Bus transition value
     */
    void executeHostTransition(const std::string& transition);

    /**
     * @brief Update host state and emit signal
     * 
     * @param newState New host state value
     */
    void updateHostState(const std::string& newState);

    /**
     * @brief Map D-Bus transition to script command
     * 
     * @param transition D-Bus transition value
     * @return Script command argument, or empty string if invalid
     */
    std::string transitionToScriptCommand(const std::string& transition);

    /**
     * @brief Validate transition value
     * 
     * @param transition D-Bus transition value
     * @return true if valid, false otherwise
     */
    bool isValidTransition(const std::string& transition);
};

} // namespace sonic::dbus_bridge

