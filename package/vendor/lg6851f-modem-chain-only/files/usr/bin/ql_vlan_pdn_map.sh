#!/bin/sh
# Copyright :
# Copyright (c) 2023, Quectel Wireless Solutions Co., Ltd. All rights reserved.
# Quectel Wireless Solutions Proprietary and Confidential.

. /usr/share/libubox/jshn.sh

# Usage: ql_vlan_pdn_map.sh add_map vlan_id wan_id
# Usage: ql_vlan_pdn_map.sh del_map vlan_id wan_id

# function to map vlan to pdn
function map_vlan_to_pdn() {
    local vlan_id=$1
    local pdn_id=$2
    local ipaddr
    local lan_section="lan"
    local wan_section="wan"

    logger -t ql_vlan_pdn_map "map_vlan_to_pdn() entry"

    # check vlan_id
    if [ "$vlan_id" -le 0 ] || [ -z "$vlan_id" ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    # check pdn_id
    if [ "$pdn_id" -lt 1 ] || [ "$pdn_id" -gt 15 ]; then
        echo "FAIL: Invalid wan_id"
        return 1
    fi

    [ "$vlan_id" -gt 1 ] && lan_section="lan"$vlan_id
    result=$(uci -q get network.$lan_section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    [ "$pdn_id" -gt 1 ] && wan_section="wan"$pdn_id
    result=$(uci -q get network.$wan_section)
    if [ -z "$result" ]; then
        echo "FAIL: WAN$pdn_id doesn't exist"
        return 1
    fi

    # add to ipv4 route table pdn_id & set rule
    ipaddr=$(uci -q get network."lan$vlan_id".ipaddr)
    ipaddr=${ipaddr%.*}.0/24
    ip route add $ipaddr dev br-lan$vlan_id table $pdn_id >/dev/null 2>&1
    iptables -D PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
    iptables -I PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w
    ip rule del fwmark 30$vlan_id table $pdn_id >/dev/null 2>&1
    ip rule add fwmark 30$vlan_id table $pdn_id

    # add to ipv6 route table pdn_id & set rule
    json_load "$(ubus call network.interface.$wan_section status)"
    json_select ipv6-prefix
    json_select 1
    json_get_vars address
    if [ -n "$address" ] ; then
        ip -6 route add "$address"1/64 dev br-lan$vlan_id table $pdn_id >/dev/null 2>&1
        ip6tables -D PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
        ip6tables -I PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w
        ip -6 rule del fwmark 30$vlan_id table $pdn_id >/dev/null 2>&1
        ip -6 rule add fwmark 30$vlan_id table $pdn_id
    fi

    logger -t ql_vlan_pdn_map "map_vlan_to_pdn() exit"
}

# function to delete vlan to pdn mapping
function del_vlan_to_pdn_map() {
    local vlan_id=$1
    local pdn_id=$2
    local wan_section="wan"

    logger -t ql_vlan_pdn_map "del_vlan_to_pdn_map() entry"

    # check vlan_id
    if [ "$vlan_id" -le 0 ] || [ -z "$vlan_id" ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    # check pdn_id
    if [ "$pdn_id" -lt 0 ] || [ "$pdn_id" -gt 15 ]; then
        echo "FAIL: Invalid wan_id"
        return 1
    fi

    # chear ipv4 route table pdn_id & del rule
    # TBD: one vlan corresponds to one pdn
    str=$(ip route show table $pdn_id | grep br-lan$vlan_id)
    [ -n "$str" ] && ip route del $str table $pdn_id
    iptables -D PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
    ip rule del fwmark 30$vlan_id table $pdn_id >/dev/null 2>&1

    # chear ipv6 route table pdn_id & del rule
    # TBD: one vlan corresponds to one pdn
    str=$(ip -6 route show table $pdn_id | grep br-lan$vlan_id)
    [ -n "$str" ] && ip -6 route del $str table $pdn_id
    ip6tables -D PREROUTING -t mangle -i br-lan$vlan_id -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
    ip -6 rule del fwmark 30$vlan_id table $pdn_id >/dev/null 2>&1

    logger -t ql_vlan_pdn_map "del_vlan_to_pdn_map() exit"
}

case $1 in
    add_map)
        logger -t ql_vlan_pdn_map "Mapping vlan$2 to pdn$3"
        map_vlan_to_pdn $2 $3
        ;;
    del_map)
        logger -t ql_vlan_pdn_map "Deleting vlan$2 to pdn$3 mapping"
        del_vlan_to_pdn_map $2 $3
        ;;
    *)
        echo "Invalid option"
        ;;
esac