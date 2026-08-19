#!/bin/sh

# netifd's packet_steering service prefers this platform hook over the generic
# ucode policy.  The generic policy sees the single RX queue exposed by the
# current MT6990 Ethernet driver and repeatedly restores a one-CPU RPS mask.

[ "$(cat /tmp/sysinfo/board_name 2>/dev/null)" = "fiberhome,lg6851f" ] || exit 0

exec /usr/sbin/lg6851f-net-affinity
