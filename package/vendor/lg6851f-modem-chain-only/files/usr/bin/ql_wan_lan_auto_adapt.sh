#!/bin/sh

. /usr/share/libubox/jshn.sh

set_wan_mipc()
{
    uci del network.@device[0].ports

    res=$(cat /proc/mtketh/sgmii_phy_id | grep 0x181d)
    if [ -n "$res" ]; then
        uci add_list network.@device[0].ports=eth0.1
        uci add_list network.@device[0].ports=eth0.2
        uci add_list network.@device[0].ports=eth0.3
        uci add_list network.@device[0].ports=eth0.4
    else
        uci add_list network.@device[0].ports=eth0
    fi
    uci add_list network.@device[0].ports=eth1
    uci commit network
    ifup lan

    proto="$(uci -q get network.wan.proto)"
    case "$proto" in ql_datacall|ql_mipc) exit 1;; esac

    json_load "$(ql_datacall --apn_provision_by_sim)"

    json_get_var apn apn
    json_get_var auth_type auth_type
    json_get_var user user
    json_get_var password password
    json_get_var protocol protocol
    json_get_var iaapn iaapn
    json_get_var rattype rattype
    json_get_var plmn plmn

    [ -z "$apn" ] && apn=""

    uci set network.wan.proto='ql_mipc'
    uci set network.wan.device='ccmni'
    uci set network.wan6.device='ccmni'
    uci set network.wan.apn="$apn"

    [ -z "$auth_type" ] || uci set network.wan.auth="$auth_type"
    [ -z "$user" ] || uci set network.wan.username="$user"
    [ -z "$password" ] || uci set network.wan.password="$password"
    [ -z "$protocol" ] || uci set network.wan.iptype="$protocol"
    [ -z "$iapan" ] || uci set network.wan.iaapn="$iaapn"
    [ -z "$rattype" ] || uci set network.wan.rattype="$rattype"
    [ -z "$plmn" ] || uci set network.wan.plmn="$plmn"

    uci commit network

    ifup wan
}

set_wan_eth()
{
    uci del network.wan
    uci set network.wan='interface'
    uci set network.wan.proto=dhcp
    uci set network.wan.device="$1"
    uci set network.wan6.device="$1"

    uci del network.@device[0].ports

    if [ $1 == "eth1" ]; then
        res=$(cat /proc/mtketh/sgmii_phy_id | grep 0x181d)
        if [ -n "$res" ]; then
            uci add_list network.@device[0].ports=eth0.1
            uci add_list network.@device[0].ports=eth0.2
            uci add_list network.@device[0].ports=eth0.3
            uci add_list network.@device[0].ports=eth0.4
        else
            uci add_list network.@device[0].ports=eth0
        fi
    elif [ $1 == "eth0" ]; then
        uci add_list network.@device[0].ports="eth1"
    else
        for i in `seq 1 4`
        do
            if [ $1 != "eth0.$i" ]; then
                uci add_list network.@device[0].ports="eth0.$i"
            fi
        done
        uci add_list network.@device[0].ports="eth1"
    fi

    uci commit network
    ifup lan
    ifup wan
}

set_wan()
{
    echo "set_wan $1"
    if [ $1 != "ccmni" ] && [ $1 != "mipc" ] &&\
       [ $1 != "eth1" ] && [ $1 != "eth0.1" ] &&\
       [ $1 != "eth0.2" ] && [ $1 != "eth0.3" ] &&\
       [ $1 != "eth0.4" ] && [ $1 != "eth0" ]; then
        echo "invaild parameter"
        exit
    fi

    uci set network.wan1.device="backup"
    uci commit network
    ifup wan1

    if [ $1 == "ccmni" ]; then
        set_wan_mipc
    else
        set_wan_eth $1
    fi

    /etc/init.d/dnsmasq restart >/dev/null 2>&1

    # restart firewall, Flushing conntrack
    fw3 restart >/dev/null 2>&1
}

init()
{
    brctl delif br-lan $1
    uci set network.wan1.proto='dhcp'
    uci set network.wan1.device=$1
    uci commit network
    ifup wan1

    # wait for interface set up
    sleep 1
}

deinit()
{
    uci set network.wan1.device="backup"
    uci set network.wan1.proto=dhcp
    uci commit network
    ifup wan1

    brctl addif br-lan $1
}

case $1 in
    init)
        init $2
        ;;
    deinit)
        deinit $2
        ;;
    set)
        set_wan $2
        ;;
    *)
        echo "commond error"
        ;;
esac