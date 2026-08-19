#!/bin/sh
# Copyright :
# Copyright (c) 2023, Quectel Wireless Solutions Co., Ltd. All rights reserved.
# Quectel Wireless Solutions Proprietary and Confidential.

. /usr/share/libubox/jshn.sh

# Usage: ql_wwan_config.sh add wan_id vlan_id auto_conf mode auto apn iptype auth username password
# Usage: ql_wwan_config.sh del wan_id
# Usage: ql_wwan_config.sh show

# function to add/update WAN
add_wan() {
    local wan_id="$1"
    local vlan_id="$2"
    local auto_conf="$3"
    local mode="$4"
    local auto="$5"
    local apn="$6"
    local iptype="$7"
    local auth="$8"
    local username="$9"
    local password="${10}"
    local section="wan"
    local tmp_section="wan"

    logger -t ql_wwan_config "add_wan() entry"

    if [ $# -lt 5 ]; then
        echo "FAIL: Invalid parameter"
        return 1
    elif [ $# -lt 10 ] && [ "$auto_conf" -eq 0 ]; then
        echo "FAIL: Insufficient parameters"
        return 1
    fi

    # check wan_id
    if [ "$wan_id" -le 0 ] || [ "$wan_id" -gt 15 ]; then
        echo "FAIL: Invalid wan_id"
        return 1
    fi

    # check vlan_id
    if [ "$vlan_id" -lt 0 ] || [ -z "$vlan_id" ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    # check vlan_id duplicate
    for i in `seq 1 1 15`
    do
        [ "$i" -eq "$wan_id" ] && continue

        [ "$i" -gt 1 ] && tmp_section="wan"$i
        result=$(uci -q get network.$tmp_section)
        [ -z "$result" ] && continue

        result=$(uci -q get network.$tmp_section.proto)
        case "$result" in ql_datacall|ql_mipc) ;; *) continue;; esac

        result=$(uci -q get network.$tmp_section.vlan | grep "$vlan_id")
        if [ -n "$result" ] || [ -z "$result" -a "$vlan_id" -eq 0 ]; then
            echo "FAIL: vlan_id duplicate"
            return 1
        fi
    done

    # check auto_config
    if [ "$auto_conf" -ne 0 ] && [ "$auto_conf" -ne 1 ]; then
        echo "FAIL: Invalid auto_config"
        return 1
    fi

    if [ "$auto_conf" -eq 1 ]; then
        [ -z "$iptype" ] && iptype="3"
        [ -z "$auth" ] && auth="0"
        if [ "$wan_id" -eq 1 ]; then
            apn="internet"
            uci -q del network.wan.plmn
        else
            apn="quectel$wan_id"
            auto_conf=0
        fi
    fi

    [ "$apn" == "0" ] && apn=""

    # check mode
    if [ "$mode" -lt 1 ] || [ "$mode" -gt 3 ]; then
        echo "FAIL: Invalid mode"
        return 1
    fi

    # check auto
    if [ "$auto" -ne 0 ] && [ "$auto" -ne 1 ]; then
        echo "FAIL: Invalid auto"
        return 1
    fi

    # check iptype
    if [ "$iptype" -lt 1 ] || [ "$iptype" -gt 3 ]; then
        echo "FAIL: Invalid iptype"
        return 1
    fi

    # check auth
    if [ "$auth" -lt 0 ] || [ "$auth" -gt 3 ]; then
        echo "FAIL: Invalid auth"
        return 1
    fi

    [ "$wan_id" -gt 1 ] && section="wan"$wan_id

    # add network.wanX
    uci set network."$section"="interface"
    uci set network."$section".proto="ql_mipc"
    uci set network."$section".device="ccmni"
    uci set network."$section".apn="$apn"
    uci set network."$section".iptype="$iptype"
    uci set network."$section".vlan="$vlan_id"
    uci set network."$section".mode="$mode"
    uci set network."$section".auth="$auth"
    uci set network."$section".username="$username"
    uci set network."$section".password="$password"
    uci set network."$section".auto="$auto"
    uci set network."$section".retry='5'
    uci set network."$section".ql_conf='1'
    uci set network."$section".auto_conf="$auto_conf"

    if [ "$vlan_id" -eq 0 ]; then
        uci set network."$section".defaultroute='1'
    else
        uci set network."$section".defaultroute='0'
    fi

    # set firewall.@zone[1]
    uci del_list firewall.@zone[1].network="$section" >/dev/null 2>&1
    uci add_list firewall.@zone[1].network="$section"

    # save config
    uci commit network
    uci commit firewall

    [ "$auto" -eq 1 ] && ifup $section

    /etc/init.d/firewall reload >/dev/null 2>&1

    logger -t ql_wwan_config "add_wan() exit"
}

# function to delete WAN
# Can't delete wan_id 1
del_wan() {
    local wan_id=$1
    local section="wan"$wan_id

    logger -t ql_wwan_config "del_wan() entry"

    # check wan_id
    if [ "$wan_id" -le 1 ] || [ "$wan_id" -gt 8 ]; then
        echo "FAIL: Invalid wan_id"
        return 1
    fi

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: WAN$wan_id doesn't exist"
        return 1
    fi

    json_load "$(ubus call network.interface.$section status)"
    json_get_vars up
    if [ -n "$up" ] && [ "$up" -eq 1 ]; then
        echo "FAIL: Stop first before deleting WAN"
        return 1
    fi

    # del network.wanX
    uci del network.$section

    # del firewall.@zone[1]
    uci del_list firewall.@zone[1].network="$section"

    /etc/init.d/firewall reload >/dev/null 2>&1

    logger -t ql_wwan_config "del_wan() exit"
}

# function to show all WANs
show_wan() {
    local section="wan"
    local count=0
    local v4address=""
    local v6address=""

    logger -t ql_wwan_config "show_wan() entry"

    echo "List all WANs:"
    for i in `seq 0 1 15`
    do
        [ "$i" -gt 0 ] && section="wan"$i
        res=$(uci -q get network."$section")
        [ -z "$res" ] && continue

        proto=$(uci -q get network."$section".proto)
        device=$(uci -q get network."$section".device)
        if { [ "$proto" = "ql_datacall" ] || [ "$proto" = "ql_mipc" ]; } && [ "$device" = "ccmni" ] ; then
            let count++
        else
            continue
        fi

        ubus call network.interface.$section status >/dev/null 2>&1
        if [ "$?" -eq 0 ] ; then
            json_load "$(ubus call network.interface.$section status)" >/dev/null 2>&1
            json_get_vars up
            [ -z "$up" ] && up=0

            if [ "$up" -eq 1 ] ; then
                json_select ipv4-address
                json_select 1
                json_get_vars address
                v4address=$address
                json_load "$(ubus call network.interface.$section status)"
                json_select ipv6-address
                json_select 1
                json_get_vars address
                v6address=$address
            else
                v4address=""
                v6address=""
            fi
        else
            up=0
            v4address=""
            v6address=""
        fi

        if [ "$i" -eq 0 ]; then
            echo "WAN: $up $v4address $v6address"
        else
            echo "WAN$i: $up $v4address $v6address"
        fi

    done

    echo "Number of WANs:$count"

    logger -t ql_wwan_config "show_wan() exit"
}

case $1 in
    add)
        logger -t ql_wwan_config "Adding WAN$2"
        add_wan "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}"
        break
        ;;

    del)
        logger -t ql_wwan_config "Delete WAN$2"
        del_wan "$2"
        break
        ;;

    show)
       logger -t ql_wwan_config "Show all WAN"
       show_wan
       break
       ;;

    *)
        echo -e "Usage: ql_wwan_config.sh [OPTION] [PARAM]\n\
        ql_wwan_config.sh add wan_id vlan_id auto_conf mode auto apn iptype auth username password\n\
        ql_wwan_config.sh del wan_id\n\
        ql_wwan_config.sh show\n"
        ;;

esac