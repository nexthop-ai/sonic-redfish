#include "dbus_exporter.hpp"
#include "logger.hpp"

namespace sonic::dbus_bridge
{

// D-Bus interface names (OpenBMC standard)
constexpr const char* IFACE_INVENTORY_CHASSIS = "xyz.openbmc_project.Inventory.Item.Chassis";
constexpr const char* IFACE_DECORATOR_ASSET = "xyz.openbmc_project.Inventory.Decorator.Asset";
constexpr const char* IFACE_DECORATOR_MODEL = "xyz.openbmc_project.Inventory.Decorator.Model";
constexpr const char* IFACE_STATE_CHASSIS = "xyz.openbmc_project.State.Chassis";

// D-Bus object paths
constexpr const char* OBJ_PATH_CHASSIS = "/xyz/openbmc_project/inventory/system/chassis";
constexpr const char* OBJ_PATH_SYSTEM = "/xyz/openbmc_project/inventory/system/system0";
constexpr const char* OBJ_PATH_CHASSIS_STATE = "/xyz/openbmc_project/state/chassis0";

DBusExporter::DBusExporter(sdbusplus::asio::object_server& inventoryServer)
    : inventoryServer_(inventoryServer)
{
}

bool DBusExporter::createObjects(const InventoryModel& model)
{
    LOG_INFO("Creating D-Bus objects...");

    if (!createChassisObject(model.chassis))
    {
        LOG_ERROR("Failed to create chassis object");
        return false;
    }

    if (!createSystemObject(model.system))
    {
        LOG_ERROR("Failed to create system object");
        return false;
    }

    if (!createChassisStateObject(model.chassisState))
    {
        LOG_ERROR("Failed to create chassis state object");
        return false;
    }

    // Note: Software objects are created by SoftwareMgr, not here

    currentModel_ = model;

    LOG_INFO("D-Bus objects created successfully");
    return true;
}

bool DBusExporter::updateObjects(const InventoryModel& model)
{
    // For now, just update the model
    // Property change signals would be emitted here
    currentModel_ = model;
    return true;
}

bool DBusExporter::createChassisObject(const ChassisInfo& chassis)
{
    // Store chassis data in currentModel_ for property getters
    currentModel_.chassis = chassis;

    // Item.Chassis interface (REQUIRED by bmcweb for chassis discovery!)
    auto chassisIface = inventoryServer_.add_interface(OBJ_PATH_CHASSIS, IFACE_INVENTORY_CHASSIS);
    chassisIface->register_property_r<std::string>(
        "Type", std::string(""),
        sdbusplus::vtable::property_::const_,
        [](const auto&) {
            return "xyz.openbmc_project.Inventory.Item.Chassis.ChassisType.RackMount";
        });
    chassisIface->initialize();
    interfaces_[std::string(OBJ_PATH_CHASSIS) + ":" + IFACE_INVENTORY_CHASSIS] = chassisIface;

    // Decorator.Asset interface
    auto assetIface = inventoryServer_.add_interface(OBJ_PATH_CHASSIS, IFACE_DECORATOR_ASSET);
    assetIface->register_property_r<std::string>(
        "SerialNumber", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.chassis.serialNumber; });
    assetIface->register_property_r<std::string>(
        "PartNumber", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.chassis.partNumber; });
    assetIface->register_property_r<std::string>(
        "Manufacturer", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.chassis.manufacturer; });
    assetIface->initialize();
    interfaces_[std::string(OBJ_PATH_CHASSIS) + ":" + IFACE_DECORATOR_ASSET] = assetIface;

    // Decorator.Model interface
    auto modelIface = inventoryServer_.add_interface(OBJ_PATH_CHASSIS, IFACE_DECORATOR_MODEL);
    modelIface->register_property_r<std::string>(
        "Model", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.chassis.model; });
    modelIface->initialize();
    interfaces_[std::string(OBJ_PATH_CHASSIS) + ":" + IFACE_DECORATOR_MODEL] = modelIface;

    LOG_INFO("Created chassis object at %s", OBJ_PATH_CHASSIS);
    return true;
}

bool DBusExporter::createSystemObject(const SystemInfo& system)
{
    // Store system data in currentModel_ for property getters
    currentModel_.system = system;

    // Decorator.Asset interface
    auto assetIface = inventoryServer_.add_interface(OBJ_PATH_SYSTEM, IFACE_DECORATOR_ASSET);
    assetIface->register_property_r<std::string>(
        "SerialNumber", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.system.serialNumber; });
    assetIface->register_property_r<std::string>(
        "Manufacturer", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.system.manufacturer; });
    assetIface->initialize();
    interfaces_[std::string(OBJ_PATH_SYSTEM) + ":" + IFACE_DECORATOR_ASSET] = assetIface;

    // Decorator.Model interface
    auto modelIface = inventoryServer_.add_interface(OBJ_PATH_SYSTEM, IFACE_DECORATOR_MODEL);
    modelIface->register_property_r<std::string>(
        "Model", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) { return currentModel_.system.model; });
    modelIface->initialize();
    interfaces_[std::string(OBJ_PATH_SYSTEM) + ":" + IFACE_DECORATOR_MODEL] = modelIface;

    LOG_INFO("Created system object at %s", OBJ_PATH_SYSTEM);
    return true;
}

bool DBusExporter::createChassisStateObject(const ChassisState& state)
{
    // Store state data in currentModel_ for property getters
    currentModel_.chassisState = state;

    // State.Chassis interface
    auto stateIface = inventoryServer_.add_interface(OBJ_PATH_CHASSIS_STATE, IFACE_STATE_CHASSIS);
    stateIface->register_property_r<std::string>(
        "CurrentPowerState", std::string(""),
        sdbusplus::vtable::property_::const_,
        [this](const auto&) {
            return (currentModel_.chassisState.powerState == "on")
                       ? "xyz.openbmc_project.State.Chassis.PowerState.On"
                       : "xyz.openbmc_project.State.Chassis.PowerState.Off";
        });
    stateIface->initialize();
    interfaces_[std::string(OBJ_PATH_CHASSIS_STATE) + ":" + IFACE_STATE_CHASSIS] = stateIface;

    LOG_INFO("Created chassis state object at %s", OBJ_PATH_CHASSIS_STATE);
    return true;
}

} // namespace sonic::dbus_bridge

