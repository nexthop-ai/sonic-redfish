#pragma once

#include "types.hpp"
#include <optional>

namespace sonic::dbus_bridge
{

/**
 * @brief Inventory model builder and normalizer
 * 
 * Merges data from multiple sources (FRU, Redis, platform.json)
 * with precedence: FRU > CONFIG_DB > platform.json > defaults
 */
class InventoryModelBuilder
{
  public:
    /**
     * @brief Build inventory model from all sources
     * 
     * @param fruInfo FRU EEPROM data (optional)
     * @param deviceMetadata CONFIG_DB metadata (optional)
     * @param platformDesc platform.json description (optional)
     * @param chassisState STATE_DB chassis state (optional)
     * @return Complete InventoryModel
     */
    static InventoryModel build(
        const std::optional<FruInfo>& fruInfo,
        const std::optional<DeviceMetadata>& deviceMetadata,
        const std::optional<PlatformDescription>& platformDesc,
        const std::optional<ChassisState>& chassisState);

  private:
    /**
     * @brief Build chassis info with fallback priority
     */
    static ChassisInfo buildChassisInfo(
        const std::optional<FruInfo>& fruInfo,
        const std::optional<DeviceMetadata>& deviceMetadata,
        const std::optional<PlatformDescription>& platformDesc);

    /**
     * @brief Build system info with fallback priority
     */
    static SystemInfo buildSystemInfo(
        const std::optional<FruInfo>& fruInfo,
        const std::optional<DeviceMetadata>& deviceMetadata);

    /**
     * @brief Build PSU list from platform.json
     */
    static std::vector<PsuInfo> buildPsuList(
        const std::optional<PlatformDescription>& platformDesc);

    /**
     * @brief Build fan list from platform.json
     */
    static std::vector<FanInfo> buildFanList(
        const std::optional<PlatformDescription>& platformDesc);

    /**
     * @brief Get value with fallback chain
     * 
     * Returns first non-empty value from the list
     */
    template<typename... Args>
    static std::string getWithFallback(const std::string& defaultValue,
                                       Args&&... args)
    {
        std::string result = defaultValue;
        ((args && !args->empty() ? (result = *args, true) : false) || ...);
        return result;
    }
};

/**
 * @brief Compare two inventory models for changes
 * 
 * @param oldModel Previous model
 * @param newModel New model
 * @return true if models differ
 */
bool hasChanged(const InventoryModel& oldModel, const InventoryModel& newModel);

} // namespace sonic::dbus_bridge

