#pragma once

#include "types.hpp"
#include <sdbusplus/asio/object_server.hpp>
#include <memory>
#include <map>
#include <string>

namespace sonic::dbus_bridge
{

/**
 * @brief D-Bus object exporter
 *
 * Creates and manages D-Bus objects that bmcweb expects:
 * - /xyz/openbmc_project/inventory/system/chassis
 * - /xyz/openbmc_project/inventory/system/system0
 * - /xyz/openbmc_project/state/chassis0
 *
 * Note: Software objects are handled by SoftwareMgr
 */
class DBusExporter
{
  public:
    /**
     * @brief Construct a new DBus Exporter
     *
     * @param inventoryServer Object server for inventory objects (chassis, system, state)
     */
    explicit DBusExporter(sdbusplus::asio::object_server& inventoryServer);

    /**
     * @brief Destructor - cleanup is automatic (RAII)
     */
    ~DBusExporter() = default;

    /**
     * @brief Create all D-Bus objects from inventory model
     *
     * @param model Complete inventory model
     * @return true on success, false on error
     */
    bool createObjects(const InventoryModel& model);

    /**
     * @brief Update D-Bus objects with new model
     *
     * Only updates properties that have changed
     *
     * @param model New inventory model
     * @return true on success, false on error
     */
    bool updateObjects(const InventoryModel& model);

  private:
    sdbusplus::asio::object_server& inventoryServer_;

    // Current model (for change detection)
    InventoryModel currentModel_;

    // D-Bus interfaces (managed by shared_ptr, cleanup is automatic)
    std::map<std::string, std::shared_ptr<sdbusplus::asio::dbus_interface>> interfaces_;

    /**
     * @brief Create chassis inventory object
     */
    bool createChassisObject(const ChassisInfo& chassis);

    /**
     * @brief Create system inventory object
     */
    bool createSystemObject(const SystemInfo& system);

    /**
     * @brief Create chassis state object
     */
    bool createChassisStateObject(const ChassisState& state);
};

} // namespace sonic::dbus_bridge

