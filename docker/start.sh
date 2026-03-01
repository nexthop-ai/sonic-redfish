#!/bin/bash

# Sync passwd/shadow/group from host (/host/etc) to container's /etc
# This ensures the container sees the current host users at startup
echo "Syncing user files from host..."
cat /host/etc/passwd > /etc/passwd 2>/dev/null || echo "Warning: Failed to sync passwd"
cat /host/etc/shadow > /etc/shadow 2>/dev/null || echo "Warning: Failed to sync shadow"
cat /host/etc/group > /etc/group 2>/dev/null || echo "Warning: Failed to sync group"

# Start supervisord
exec /usr/local/bin/supervisord

