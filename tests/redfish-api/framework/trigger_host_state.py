#!/usr/bin/env python3
#######################################
# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 Nexthop AI
# Copyright (C) 2024 SONiC Project
# Author: Nexthop AI
# Author: SONiC Project
# Author: Chinmoy Dey <chinmoy@nexthop.ai>
# License file: sonic-redfish/LICENSE
#######################################

"""Inject a Switch-Host power state change into STATE_DB (db 6).

Usage:
    trigger_host_state.py <state>      # state: On | Off | PoweringOn | PoweringOff

Writes the HOST_STATE|switch-host fields the way bmcctld does.
The sonic-dbus-bridge maps device_power_state/device_status to the D-Bus
CurrentHostState, and bmcweb's host state monitor turns the change into a
ResourceEvent (ResourcePoweredOn / ResourcePoweredOff / ...).
"""

import sys
import time

import redis

STATE_DB = 6
KEY = "HOST_STATE|switch-host"

# Redfish-ish state -> bmcctld-style HOST_STATE|switch-host fields
STATE_TO_FIELDS = {
    "On": {"device_power_state": "POWERED_ON", "device_status": "ONLINE"},
    "Off": {"device_power_state": "POWERED_OFF", "device_status": "OFFLINE"},
    "PoweringOn": {"device_power_state": "POWERING_ON",
                   "device_status": "OFFLINE"},
    "PoweringOff": {"device_power_state": "POWERING_OFF",
                    "device_status": "ONLINE"},
}


def set_host_state(client, state: str) -> None:
    """Write HOST_STATE|switch-host power fields into STATE_DB."""
    fields = dict(STATE_TO_FIELDS[state])
    fields["last_change_timestamp"] = str(int(time.time()))
    client.hset(KEY, mapping=fields)


def main(argv) -> int:
    if len(argv) != 2:
        print(f"usage: {argv[0]} <state>", file=sys.stderr)
        return 2
    state = argv[1]
    if state not in STATE_TO_FIELDS:
        print(f"invalid state {state!r}; expected one of "
              f"{', '.join(STATE_TO_FIELDS)}", file=sys.stderr)
        return 2
    client = redis.StrictRedis(host="localhost", port=6379, db=STATE_DB,
                               decode_responses=True)
    set_host_state(client, state)
    print(f"{KEY} -> {state}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
