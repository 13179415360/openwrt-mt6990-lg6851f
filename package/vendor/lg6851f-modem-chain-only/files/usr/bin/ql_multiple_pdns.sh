#!/bin/sh
# Copyright :
# Copyright (c) 2023, Quectel Wireless Solutions Co., Ltd. All rights reserved.
# Quectel Wireless Solutions Proprietary and Confidential.

. /usr/share/libubox/jshn.sh

reset_configuration()
{
    local id
    for id in `seq 2 1 15`
    do
        ql_wwan_config.sh del $id >/dev/null 2>&1
        ql_lan_config.sh del $id >/dev/null 2>&1
    done
}

set_default_for_auto_datacall()
{
    local id
    local retry=10

    echo "-----------------------------------------"
    read -p "please input the num of PDN (valid: 2-15): " num
    if [ $num -lt 2 ] || [ $num -gt 15 ] ; then
        echo "invalid param"
        exit;
    fi

    echo "please choose the mode: "
    echo "1) route"
    echo "2) ip passthrough without nat"
    echo "3) ip passthrough with nat"
    read mode

    # check mode
    if [ "$mode" -lt 1 ] || [ "$mode" -gt 3 ]; then
        echo "FAIL: Invalid mode"
        return 1
    fi

    echo "Please wait patiently......."

    # ifdown interface
    ifdown -a

    # reset
    reset_configuration

    ifup lan

    # set wan1
    ql_wwan_config.sh add 1 0 1 "$mode" 1

    # set multiple LANs and WANs
    for id in `seq 2 1 $num`
    do
        ql_lan_config.sh add "$id" 1
        ql_wwan_config.sh add "$id" "$id" 1 "$mode" 1
    done

    # check the last wan
    while [ "$retry" -gt 0 ]
    do
        json_load "$(ubus call network.interface.wan"$id" status)"
        json_get_vars up
        if [ -n "$up" ] && [ "$up" -eq 1 ]; then
            break
        fi

        let retry--
        sleep 2
    done

    ifup loopback
}

set_wan_by_manual()
{
    local wan_id
    local vlan_id
    local choice
    local exit_flag=0
    local delete=0

    while [ $exit_flag -eq 0 ]
    do
        echo "0) add/update"
        echo "1) connect"
        echo "2) disconnect"
        echo "3) delete"
        echo "4) exit"
        read choice
        case $choice in
            0)
                read -p "please input the wan id (range:1-15): " wan_id

                read -p "please input the vlan id: " vlan_id

                read -p "please choose the method of configuration (0: manual configure  1: auto configure): " auto_conf

                read -p "please input the mode (1: route  2: ippt without nat  3:ippt with nat): " mode

                read -p "please choose the method of data call (0: mannual data call  1: auto data call): " auto
                if [ "$auto_conf" -eq 1 ]; then
                    ql_wwan_config.sh add "$wan_id" "$vlan_id" "1" "$mode" "$auto"
                else
                    read -p "please input the apn: " apn
                    read -p "please input the iptype (1: IPv4  2: IPv6  3:IPv4v6): " iptype
                    read -p "please input the auth (0: none  1: PAP  2: CHAP  3:PAP and CHAP): " auth
                    read -p "please input the username: " username
                    read -p "please input the password: " password
                    ql_wwan_config.sh add "$wan_id" "$vlan_id" "0" "$mode" "$auto" "$apn" "$iptype" "$auth" "$username" "$password"
                fi
                ;;
            1)
                read -p "please input the wan id (range:1-15): " wan_id
                if [ "$wan_id" -eq 1 ]; then
                    ifup wan
                else
                    ifup wan$wan_id
                fi
                ;;
            2)
                read -p "please input the wan id (range:1-15): " wan_id
                if [ "$wan_id" -eq 1 ]; then
                    ifdown wan
                else
                    ifdown wan$wan_id
                fi
                ;;
            3)
                read -p "please input the wan id (range:2-15): " wan_id
                ql_wwan_config.sh del "$wan_id"
                ;;
            *)
                exit_flag=1
                ;;
        esac
    done
}

set_lan_by_manual()
{
    local choice
    local exit_flag=0
    local delete=0

    while [ $exit_flag -eq 0 ]
    do
        echo "0) add"
        echo "1) set IPv4 addr"
        echo "2) delete"
        echo "3) exit"
        read choice
        case $choice in
            0)
                read -p "please input the vlan id (range:1-255): " vlan
                read -p "please input the device type (1: eth0  2: eth1): " type
                ql_lan_config.sh add "$vlan" "$type"
                ;;
            1)
                read -p "please input the vlan id: " vlan
                read -p "please input the ipv4 addr: " ipv4_addr
                ql_lan_config.sh set_lanip "$vlan" "$ipv4_addr"
                ;;
            2)
                read -p "please input the vlan id: " vlan
                ql_lan_config.sh del "$vlan"
                ;;
            *)
                exit_flag=1
                ;;
        esac
    done
}

reset()
{
    echo "Please wait patiently......."

    ifdown -a

    # reset
    reset_configuration

    uci del network.wan
    uci set network.wan=interface
    uci set network.wan.device=ccmni
    uci set network.wan.proto=ql_mipc

    uci commit network

    ifup wan
    ifup lan
    ifup loopback
}

echo "please choose the configuration: "
echo "0) all by default"
echo "1) wan"
echo "2) lan"
echo "3) reset"
read conf
echo "-----------------------------------------"

case $conf in
    0)
        set_default_for_auto_datacall
        ;;
    1)
        set_wan_by_manual
        ;;
    2)
        set_lan_by_manual
        ;;
    3)
        reset
        ;;
    *)
        echo "commond error"
        ;;
esac