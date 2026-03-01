#!/bin/bash
# SPDX-License-Identifier: Apache-2.0
# Test script for sonic-dbus-bridge D-Bus objects

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

SERVICE_NAME="com.sonic.BmcInventory"
CHASSIS_PATH="/xyz/openbmc_project/inventory/system/chassis"
SYSTEM_PATH="/xyz/openbmc_project/inventory/system/system0"
STATE_PATH="/xyz/openbmc_project/state/chassis0"

echo "========================================="
echo "SONiC D-Bus Bridge Test Suite"
echo "========================================="
echo ""

# Test 1: Check if service is running
echo -n "Test 1: Checking if sonic-dbus-bridge service is running... "
if systemctl is-active --quiet sonic-dbus-bridge; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    echo "Service is not running. Start it with: systemctl start sonic-dbus-bridge"
    exit 1
fi

# Test 2: Check if service owns D-Bus name
echo -n "Test 2: Checking if service owns D-Bus name... "
if busctl list | grep -q "$SERVICE_NAME"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    echo "Service does not own D-Bus name: $SERVICE_NAME"
    exit 1
fi

# Test 3: Check chassis object exists
echo -n "Test 3: Checking chassis object exists... "
if busctl tree xyz.openbmc_project.Inventory | grep -q "$CHASSIS_PATH"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    echo "Chassis object not found at: $CHASSIS_PATH"
    exit 1
fi

# Test 4: Check system object exists
echo -n "Test 4: Checking system object exists... "
if busctl tree xyz.openbmc_project.Inventory | grep -q "$SYSTEM_PATH"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    echo "System object not found at: $SYSTEM_PATH"
    exit 1
fi

# Test 5: Check chassis state object exists
echo -n "Test 5: Checking chassis state object exists... "
if busctl tree xyz.openbmc_project.Inventory | grep -q "$STATE_PATH"; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${YELLOW}WARN${NC}"
    echo "Chassis state object not found at: $STATE_PATH"
fi

# Test 6: Read chassis serial number
echo -n "Test 6: Reading chassis serial number... "
SERIAL=$(busctl get-property xyz.openbmc_project.Inventory \
    "$CHASSIS_PATH" \
    xyz.openbmc_project.Inventory.Decorator.Asset SerialNumber 2>/dev/null | awk '{print $2}' | tr -d '"')
if [ -n "$SERIAL" ]; then
    echo -e "${GREEN}PASS${NC} (Serial: $SERIAL)"
else
    echo -e "${YELLOW}WARN${NC} (Empty serial number)"
fi

# Test 7: Read chassis manufacturer
echo -n "Test 7: Reading chassis manufacturer... "
MANUFACTURER=$(busctl get-property xyz.openbmc_project.Inventory \
    "$CHASSIS_PATH" \
    xyz.openbmc_project.Inventory.Decorator.Asset Manufacturer 2>/dev/null | awk '{print $2}' | tr -d '"')
if [ -n "$MANUFACTURER" ]; then
    echo -e "${GREEN}PASS${NC} (Manufacturer: $MANUFACTURER)"
else
    echo -e "${YELLOW}WARN${NC} (Empty manufacturer)"
fi

# Test 8: Read chassis model
echo -n "Test 8: Reading chassis model... "
MODEL=$(busctl get-property xyz.openbmc_project.Inventory \
    "$CHASSIS_PATH" \
    xyz.openbmc_project.Inventory.Decorator.Model Model 2>/dev/null | awk '{print $2}' | tr -d '"')
if [ -n "$MODEL" ]; then
    echo -e "${GREEN}PASS${NC} (Model: $MODEL)"
else
    echo -e "${YELLOW}WARN${NC} (Empty model)"
fi

# Test 9: Read system hostname
echo -n "Test 9: Reading system pretty name... "
PRETTY_NAME=$(busctl get-property xyz.openbmc_project.Inventory \
    "$SYSTEM_PATH" \
    xyz.openbmc_project.Inventory.Item.System PrettyName 2>/dev/null | awk '{print $2}' | tr -d '"')
if [ -n "$PRETTY_NAME" ]; then
    echo -e "${GREEN}PASS${NC} (Name: $PRETTY_NAME)"
else
    echo -e "${YELLOW}WARN${NC} (Empty pretty name)"
fi

# Test 10: Check Redfish Chassis endpoint (if bmcweb is running)
echo -n "Test 10: Testing Redfish /Chassis endpoint... "
if command -v curl &> /dev/null; then
    if curl -k -s https://localhost/redfish/v1/Chassis | grep -q "Chassis"; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${YELLOW}WARN${NC} (bmcweb may not be running or configured)"
    fi
else
    echo -e "${YELLOW}SKIP${NC} (curl not available)"
fi

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo "All critical tests passed!"
echo ""
echo "To view all D-Bus objects:"
echo "  busctl tree xyz.openbmc_project.Inventory"
echo ""
echo "To monitor service logs:"
echo "  journalctl -u sonic-dbus-bridge -f"
echo ""

