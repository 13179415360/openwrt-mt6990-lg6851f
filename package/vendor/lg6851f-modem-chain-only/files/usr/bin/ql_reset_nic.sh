#!/bin/sh

section=$1
device=$(uci -q get network.$section.device)
device_id=0
phy_flag=0
switch_flag=0

[ -z "$device" ] && exit 1

switch clear

t=5
while [ $t -gt 0 ] ;
do
    line_num=$(switch dump | wc -l)
    [ "$line_num" -gt 3 ] && break

    let t--
    sleep 1
done

line_num=$(ip -4 neigh show dev $device | wc -l)
echo "line_num=$line_num"
[ "$line_num" = 0 ] && exit 1


while true; do
    result=$(uci -q get network.@device[$device_id].name)
    [ -z "$result" ] && exit 1

    if [[ $result == $device ]]; then
        let device_id=$device_id
        break
    fi

    let device_id++
done

has_eth1=$(uci get network.@device[$device_id].ports | grep eth1)

for i in `seq 1 1 $line_num`
do
    # find connected mac
    state=$(ip -4 neigh show dev $device | awk 'NR=='$i'' | awk '{ print $4 }')
    echo "state=$state"
    [ -z "$state" ] && continue
    [ "$state" = FAILED ] && continue

    mac=$(ip -4 neigh show dev $device | awk 'NR=='$i'' | awk '{ print $3 }')
    mac=${mac//:}

    [ "$mac" = 061626364656 ] && continue

    # find switch
    switch_flag=0
    str=$(switch dump | grep $mac | awk '{print $2}')
    if [ -n "$str" ] ; then
        for i in `seq 0 1 4`
        do
            tmp=${str:$i:1}
            if [ "$tmp" = 1 ] ; then
                echo "port: $i"
                switch phy cl22 w $i 0 0x1840
                switch phy cl22 w $i 0 0x1040
                switch_flag=1
                break
            fi
        done
    fi

    [ $switch_flag -eq 1 ] && continue

    if [ -n "$has_eth1" ] && [ $phy_flag -eq 0 ] ; then
        echo "reset phy"
        ethtool -r eth1
        phy_flag=1
    fi
done

