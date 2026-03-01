#pragma once

#include "types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace sonic::dbus_bridge
{

/**
 * @brief Adapter for reading FRU EEPROMs
 * 
 * Reads FRU data from sysfs I2C EEPROM devices and parses
 * ONIE TLV format to extract serial number, part number, etc.
 */
class FruAdapter
{
  public:
    /**
     * @brief Construct a new FRU Adapter
     * 
     * @param eepromPaths List of possible EEPROM paths to try
     */
    explicit FruAdapter(const std::vector<std::string>& eepromPaths);

    /**
     * @brief Scan and read FRU EEPROM
     * 
     * Tries each configured path until one succeeds
     * 
     * @return true if FRU data read successfully
     * @return false if all paths failed
     */
    bool scanAndLoad();

    /**
     * @brief Check if FRU data was loaded successfully
     */
    bool isLoaded() const { return loaded_; }

    /**
     * @brief Get FRU information
     * 
     * @return FruInfo structure (fields may be empty if not in EEPROM)
     */
    FruInfo getFruInfo() const { return fruInfo_; }

  private:
    std::vector<std::string> eepromPaths_;
    bool loaded_{false};
    FruInfo fruInfo_;

    /**
     * @brief Read FRU EEPROM from a specific path
     * 
     * @param path EEPROM device path
     * @return true if read and parsed successfully
     */
    bool readEeprom(const std::string& path);

    /**
     * @brief Parse ONIE TLV format
     * 
     * @param data Raw EEPROM data
     * @return FruInfo structure
     */
    FruInfo parseTlv(const std::vector<uint8_t>& data);

    /**
     * @brief Validate TLV CRC
     * 
     * @param data Raw EEPROM data
     * @return true if CRC is valid
     */
    bool validateCrc(const std::vector<uint8_t>& data);
};

} // namespace sonic::dbus_bridge

