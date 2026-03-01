#!/bin/bash
# Test Redfish API endpoints

BMC_IP="${1:-localhost}"
PORT="${2:-18080}"
USER="${3:-root}"
PASS="${4:-0penBmc}"

BASE_URL="https://${BMC_IP}:${PORT}"

echo "=== Redfish API Test ==="
echo "Target: $BASE_URL"
echo "User: $USER"
echo ""

# Test if jq is available
if ! command -v jq &> /dev/null; then
    echo "Warning: jq not found, output will not be formatted"
    JQ="cat"
else
    JQ="jq ."
fi

echo "1. Testing Service Root..."
curl -k -u "$USER:$PASS" "$BASE_URL/redfish/v1" 2>/dev/null | $JQ
echo ""

echo "2. Testing Chassis Collection..."
curl -k -u "$USER:$PASS" "$BASE_URL/redfish/v1/Chassis" 2>/dev/null | $JQ
echo ""

echo "3. Testing Chassis Details..."
curl -k -u "$USER:$PASS" "$BASE_URL/redfish/v1/Chassis/chassis" 2>/dev/null | $JQ
echo ""

echo "4. Testing Systems Collection..."
curl -k -u "$USER:$PASS" "$BASE_URL/redfish/v1/Systems" 2>/dev/null | $JQ
echo ""

echo "5. Testing System Details..."
curl -k -u "$USER:$PASS" "$BASE_URL/redfish/v1/Systems/system" 2>/dev/null | $JQ
echo ""

echo "=== Test Complete ==="

