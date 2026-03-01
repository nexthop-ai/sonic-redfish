#!/bin/bash
# Debug script to check Redis connectivity

echo "=== Redis Diagnostic Script ==="
echo ""

echo "1. Checking Redis service status..."
systemctl status redis-server 2>/dev/null || systemctl status redis 2>/dev/null || echo "Redis service not found"
echo ""

echo "2. Checking for Redis processes..."
ps aux | grep redis | grep -v grep
echo ""

echo "3. Checking Redis socket/port..."
netstat -tlnp 2>/dev/null | grep 6379 || ss -tlnp 2>/dev/null | grep 6379 || echo "Port 6379 not listening"
echo ""

echo "4. Checking Unix socket..."
ls -la /var/run/redis* 2>/dev/null || echo "No Redis Unix sockets found"
ls -la /run/redis* 2>/dev/null || echo "No Redis Unix sockets found in /run"
echo ""

echo "5. Testing Redis connection (localhost:6379)..."
redis-cli -h localhost -p 6379 ping 2>&1 || echo "Failed to connect to localhost:6379"
echo ""

echo "6. Testing Redis connection (127.0.0.1:6379)..."
redis-cli -h 127.0.0.1 -p 6379 ping 2>&1 || echo "Failed to connect to 127.0.0.1:6379"
echo ""

echo "7. Testing Unix socket connection..."
redis-cli -s /var/run/redis/redis.sock ping 2>/dev/null || \
redis-cli -s /run/redis/redis.sock ping 2>/dev/null || \
redis-cli -s /var/run/redis.sock ping 2>/dev/null || \
echo "Failed to connect via Unix socket"
echo ""

echo "8. Checking Redis configuration..."
cat /etc/redis/redis.conf 2>/dev/null | grep -E "^bind|^port|^unixsocket" || echo "Redis config not found"
echo ""

echo "9. Checking SONiC database service..."
systemctl status database 2>/dev/null || echo "SONiC database service not found"
echo ""

echo "10. Checking for SONiC Redis instances..."
docker ps 2>/dev/null | grep database || echo "No database container found"
echo ""

echo "11. Testing connection to database container..."
if docker ps 2>/dev/null | grep -q database; then
    echo "Database container is running"
    docker exec database redis-cli ping 2>&1 || echo "Failed to ping Redis in container"
else
    echo "Database container not running"
fi
echo ""

echo "12. Checking if Redis is in a namespace..."
ip netns list 2>/dev/null || echo "No network namespaces"
echo ""

echo "=== End of Diagnostics ==="

