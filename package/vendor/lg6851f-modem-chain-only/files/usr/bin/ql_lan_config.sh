#!/bin/sh
# Copyright :
# Copyright (c) 2023, Quectel Wireless Solutions Co., Ltd. All rights reserved.
# Quectel Wireless Solutions Proprietary and Confidential.

. /usr/share/libubox/jshn.sh

# Usage: ql_lan_config.sh add vlan_id device_type
# Usage: ql_lan_config.sh del vlan_id
# Usage: ql_lan_config.sh show
# Usage: ql_lan_config.sh set_lanip vlan_id ip_address
# Usage: ql_lan_config.sh set_dhcpip vlan_id start limit
# Usage: ql_lan_config.sh set_mode vlan_id mode ifname v4_addr v4_mask v4_gw v4_dns1 v4_dns2 v4_mtu v6_addr v6_dns1 v6_dns2 odu_addr odu_dhcp_offset v6_prefix v6_prefix_len
# Usage: ql_lan_config.sh set_mtu vlan_id mtu
# Usage: ql_lan_config.sh reset_mtu vlan_id
# Usage: ql_lan_config.sh reset_nic vlan_id

# function to add VLAN
function add_vlan() {
    local vlan_id=$1
    local device_type=$2
    local section="lan"$vlan_id
    #local device_name=$device_type.$vlan_id
    local ipv4_addr

    logger -t ql_lan_config "add_vlan() entry"

    # check vlan_id
    if [ "$vlan_id" -le 0 ] || [ "$vlan_id" -gt 255 ] || [ -z "$vlan_id" ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    # TBD: only support eth0.x and eth1.x
    # check device_type
    if [ "$device_type" -ne 1 ] && [ "$device_type" -ne 2 ]; then
        echo "FAIL: Invalid device_type"
        return 1
    fi

    if [ "$device_type" -eq 1 ]; then
        device_name="eth0".$vlan_id
    elif [ "$device_type" -eq 2 ]; then
        device_name="eth1".$vlan_id
    fi

    result=$(uci -q get network.$section)
    if [ -n "$result" ]; then
        echo "FAIL: VLAN$vlan_id already exist"
        return 1
    fi

    if [ "$vlan_id" -eq 1 ]; then
        ipv4_addr="192.168.0.1"
    else
        ipv4_addr="192.168.$vlan_id.1"
    fi

    # add network.lanX
    uci set network.$section=interface
    uci set network.$section.device="br-$section"
    uci set network.$section.proto="static"
    uci set network.$section.ipaddr="$ipv4_addr"
    uci set network.$section.netmask="255.255.255.0"
    uci set network.$section.ip6assign="60"

    # add network.ql_lanX
    uci set network.ql_$section=device
    uci set network.ql_$section.name="br-$section"
    uci set network.ql_$section.type="bridge"
    uci add_list network.ql_$section.ports="$device_name"

    # add dhcp.lanX_dnsmasq
    section=lan"$vlan_id"_dnsmasq
    uci set dhcp.$section=dnsmasq
    uci set dhcp.$section.rebind_localhost='1'
    uci set dhcp.$section.local='/lan'$vlan_id'/'
    uci set dhcp.$section.domain='lan'$vlan_id''
    uci set dhcp.$section.readethers='1'
    #uci set dhcp.$section.leasefile='/tmp/dhcp.leases.lan'$vlan_id''
    uci set dhcp.$section.leasefile='/tmp/dhcp.leases'
    uci set dhcp.$section.resolvfile='/tmp/resolv.conf.d/resolv.conf.auto'

    # add dhcp.lanX
    section="lan"$vlan_id
    uci set dhcp.$section=dhcp
    uci set dhcp.$section.interface="$section"
    uci set dhcp.$section.leasetime="12h"
    uci set dhcp.$section.dhcpv6="server"
    uci set dhcp.$section.ra="server"
    uci set dhcp.$section.start="150"
    uci set dhcp.$section.limit="100"
    uci set dhcp.$section.instance=''$section'_dnsmasq'

    # set firewall.@zone[0]
    uci add_list firewall.@zone[0].network="lan$vlan_id"

    # save config
    uci commit network
    uci commit dhcp
    uci commit firewall

    ifup "lan"$vlan_id
    /etc/init.d/dnsmasq start lan"$vlan_id"_dnsmasq >/dev/null 2>&1
    /etc/init.d/firewall reload >/dev/null 2>&1
    /etc/init.d/odhcpd restart >/dev/null 2>&1

    logger -t ql_lan_config "add_vlan() exit"
}

# function to del VLAN
function del_vlan() {
    local vlan_id=$1
    local section="lan"$vlan_id

    logger -t ql_lan_config "del_vlan() entry"

    # check vlan_id
    if [ "$vlan_id" -le 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    ifdown "lan"$vlan_id
    /etc/init.d/dnsmasq stop lan"$vlan_id"_dnsmasq

    # del network.lanX
    uci del network.$section

    # del network.ql_lanX
    uci del network.ql_$section

    # del dhcp.lanX_dnsmasq
    uci del dhcp.lan"$vlan_id"_dnsmasq

    # del dhcp.lanX
    uci del dhcp.$section

    # del dhcp.ql_lanX
    uci del dhcp.ql_$section

    # del firewall.@zone[0]
    uci del_list firewall.@zone[0].network="lan$vlan_id"

    # save config
    uci commit network
    uci commit dhcp
    uci commit firewall

    # if enable ippt auto adapt function
    auto_adapt=$(uci -q get system.@system[0].ql_ippt_auto_adapt)
    if [ "$auto_adapt" -eq 1 ]; then
        /etc/init.d/ql_ippt reload &
    fi

    /etc/init.d/firewall reload >/dev/null 2>&1

    logger -t ql_lan_config "del_vlan() exit"
}

# function to show all VLANs
function show_vlan() {
    local section="lan"

    logger -t ql_lan_config "show_vlan() entry"

    echo "List all VLANs:"
    for i in `seq 0 1 255`
    do
        [ "$i" -gt 0 ] && section="lan"$i
        res=$(uci -q get network.$section)
        [ -z "$res" ] && continue

        json_load "$(ubus call network.interface.$section status)"
        json_get_vars up
        [ -z "$up" ] && up=0

        [ "$i" -gt 0 ] && device_type=$(uci -q get network.ql_$section.ports)

        if [ "$i" -eq 0 ]; then
            echo "VLAN:$i status:$up"
        else
            echo "VLAN:$i status:$up device_type:${device_type%.*}"
        fi
    done

    logger -t ql_lan_config "show_vlan() exit"
}

# function to set VLAN IP
function set_vlan_ip() {
    local vlan_id=$1
    local ipv4_addr=$2
    local section="lan"
    local wan_section="wan"

    logger -t ql_lan_config "set_vlan_ip() entry"

    # check vlan_id
    if [ "$vlan_id" -lt 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    # check ipv4_addr available
    result=$(echo $ipv4_addr|awk -F. '$1<=255&&$2<=255&&$3<=255&&$4<=255{print "yes"}')
    if echo $ipv4_addr|grep -E "^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$">/dev/null; then
        if [ ${result:-no} != "yes" ]; then
            echo "FAIL: $ipv4_addr is not available!"
            return 1
        fi
    else
        echo "FAIL: IP format error!"
        return 1
    fi

    # check ipv4_addr duplicate
    result=$(uci show network | grep $ipv4_addr)
    if [ -n "$result" ]; then
        echo "FAIL: ipv4_addr duplicate"
        return 1
    fi

    # find mapping relationship
    for i in `seq 1 1 16`
    do
        [ "$i" -gt 1 ] && wan_section="wan$i"

        uci_vlan=$(uci -q get network.$wan_section.vlan)
        [ -z "$uci_vlan" ] && uci_vlan=0

        [ "$uci_vlan" != "$vlan_id" ] && continue

        uci_mode=$(uci -q get network.$wan_section.mode)
        [ -z "$uci_mode" ] && uci_mode=1
        if [ "$uci_mode" -eq 2 ]; then
            echo "FAIL: the mode is ippt without nat, the vlan can't be set"
            return 1
        else
            wan_id=$i
            break
        fi
    done

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    # set network.lanX.ipaddr
    uci set network.$section.ipaddr=$ipv4_addr

    # save config
    uci commit network

    ifup $section
    if [ "$vlan_id" -eq 0 ]; then
        /etc/init.d/dnsmasq restart >/dev/null 2>&1
    else
        /etc/init.d/dnsmasq stop "$section"_dnsmasq >/dev/null 2>&1
        /etc/init.d/dnsmasq start "$section"_dnsmasq >/dev/null 2>&1

        ubus call network.interface.$wan_section status >/dev/null 2>&1
        if [ "$?" = "0" ]; then
            json_load "$(ubus call network.interface.$wan_section status)"
            json_select ipv6-prefix
            json_select 1
            json_get_vars address
            if [ -n "$address" ] ; then
                ip -6 addr add "$address"1/64 dev br-$section
            fi
            ql_vlan_pdn_map.sh add_map "$vlan_id" "$wan_id"
        fi
    fi

    /etc/init.d/odhcpd restart >/dev/null 2>&1

    logger -t ql_lan_config "set_vlan_ip() exit"
}

# function to set VLAN IP Range
function set_dhcp_ip() {
    local vlan_id="$1"
    local start="$2"
    local limit="$3"
    local section="lan"

    logger -t ql_lan_config "set_dhcp_ip() entry"

    # check vlan_id
    if [ "$vlan_id" -lt 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    # check start
    if [ "$start" -lt 2 ] || [ "$start" -gt 254 ]; then
        echo "FAIL: Invalid start"
        return 1
    fi

    # check limit
    if [ "$limit" -lt 1 ] || [ "$limit" -gt 253 ]; then
        echo "FAIL: Invalid limit"
        return 1
    fi

    # set dhcp.lanX
    uci set dhcp.$section.start=$start
    uci set dhcp.$section.limit=$limit

    # save config
    uci commit dhcp

    if [ "$vlan_id" -eq 0 ]; then
        /etc/init.d/dnsmasq restart >/dev/null 2>&1
    else
        /etc/init.d/dnsmasq stop "$section"_dnsmasq >/dev/null 2>&1
        /etc/init.d/dnsmasq start "$section"_dnsmasq >/dev/null 2>&1
    fi

    logger -t ql_lan_config "set_dhcp_ip() exit"
}

reset_rndis() {
    local vlan_id=$1
    local section="lan"
    local member

    logger -t ql_lan_config "reset_rndis() entry"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    if [ "$vlan_id" -eq 0 ]; then
        member=$(uci -q get network.@device[0].ports | grep rndis0)
    else
        member=$(uci -q get network.ql_$section.ports | grep rndis0)
    fi

    if [ -z "$member" ]; then
        logger -t ql_lan_config "no need to reset rndis0"
        return 1
    fi

    ifconfig rndis0 down
    sleep 1
    ifconfig rndis0 up

    logger -t ql_lan_config "reset_rndis() exit"
}

reset_rtl8221() {
    local vlan_id=$1
    local section="lan"
    local member

    logger -t ql_lan_config "reset_rtl8221() entry"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    if [ "$vlan_id" -eq 0 ]; then
        member=$(uci -q get network.@device[0].ports | grep eth1)
    else
        member=$(uci -q get network.ql_$section.ports | grep eth1)
    fi

    if [ -z "$member" ]; then
        logger -t ql_lan_config "no need to reset rtl8221"
        return 1
    fi

    ifconfig eth1 down
    sleep 3
    ifconfig eth1 up

    logger -t ql_lan_config "reset_rtl8221() exit"
}

#reset_mt7531 vlan
#reset switch port to update IP
reset_mt7531() {
    local vlan_id=$1
    local section="lan"
    local member

    logger -t ql_lan_config "reset_mt7531() entry"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    if [ "$vlan_id" -eq 0 ]; then
        member=$(uci -q get network.@device[0].ports | grep eth0)
    else
        member=$(uci -q get network.ql_$section.ports | grep eth0)
    fi

    if [ -z "$member" ]; then
        logger -t ql_lan_config "no need to reset mt7531"
        return 1
    fi

    enable_vlan=$(uci -q get network.eth0.enable_vlan)
    if [ -z "$enable_vlan" ] || [ "$enable_vlan" -eq 0 ] ; then
        logger -t ql_lan_config "MT7531 vlan disable, reset all port"
        for i in `seq 0 1 4`
        do
                switch phy cl22 w $i 0 0x1840
                switch phy cl22 w $i 0 0x1040
        done
    else
        logger -t ql_lan_config "MT7531 vlan enable, check which port should be reset"

        i=1
        while true ;
        do
            dev=$(echo $member | awk '{ print $'$i' }')
            [ -z $dev ] && break
            result=$(echo $dev | grep eth0)
            if [ -z $result ] ; then
                let i++
                continue
            fi

            vlan=$(echo ${dev#*.})
            tmp=$(uci show network | grep -vn "enable_vlan" | grep "vlan='$vlan'" | awk -F. '{ print $2 }')
            j=1
            while true ;
            do
                port=$(uci -q get network.$tmp.ports | awk '{ print $'$j' }')
                [ -z $port ] && break
                [ "$port" == "5t" ] && break

                logger -t ql_lan_config "port$port need to be reset"
                switch phy cl22 w $port 0 0x1840
                switch phy cl22 w $port 0 0x1040
                let j++
            done

            let i++
        done
    fi

    logger -t ql_lan_config "reset_mt7531() exit"
}

set_route() {
    local vlan_id="$1"
    local section="lan"
    local v4_addr="192.168.1.1"

    logger -t ql_lan_config "set_route() entry, VLAN$vlan_id"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    if [ "$vlan_id" -eq 1 ]; then
        v4_addr="192.168.0.1"
    elif [ "$vlan_id" -gt 1 ]; then
        v4_addr="192.168.$vlan_id.1"
    fi

    if [ "$vlan_id" -eq 0 ]; then
        ccmni_dev=$(uci get network.@device[0].ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.@device[0].ports=$ccmni_dev
    else
        ccmni_dev=$(uci get network.ql_$section.ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.ql_$section.ports=$ccmni_dev
    fi

    # It is already in routing mode
    [ -z "$ccmni_dev" ] && return 1

    public_ip=$(uci -q get dhcp.$section.public_ip)
    if [ -n "$ccmni_dev" ] && [ -n "$public_ip" ]; then
        iptables -t nat -D POSTROUTING -o $ccmni_dev -j SNAT --to $public_ip
    fi

    # reset network.lanX
    uci del network.$section
    uci del network.ql_"$section"_alias 2>/dev/null

    # set network.lanX
    uci set network.$section=interface
    uci set network.$section.device="br-$section"
    uci set network.$section.proto="static"
    uci set network.$section.ipaddr=$v4_addr
    uci set network.$section.netmask="255.255.255.0"
    uci set network.$section.ip6assign="60"

    # reset dhcp.lanX
    uci del dhcp.$section.dhcp_option
    uci del dhcp.$section.dns
    uci del dhcp.$section.public_ip
    uci del dhcp.ql_$section 2>/dev/null

    # set dhcp.lanX
    uci set dhcp.$section=dhcp
    uci set dhcp.$section.interface="$section"
    uci set dhcp.$section.leasetime="12h"
    uci set dhcp.$section.dhcpv6="server"
    uci set dhcp.$section.ra="server"
    uci set dhcp.$section.start="150"
    uci set dhcp.$section.limit="100"
    [ "$vlan_id" -gt 0 ] && uci set dhcp.$section.instance=''$section'_dnsmasq'

    # save config
    uci commit network
    uci commit dhcp

    # if enable ippt auto adapt function
    auto_adapt=$(uci -q get system.@system[0].ql_ippt_auto_adapt)
    if [ "$auto_adapt" -eq 1 ]; then
        /etc/init.d/ql_ippt reload
    fi

    # stop service
    if [ "$vlan_id" -gt 0 ] ; then
        /etc/init.d/dnsmasq stop "$section"_dnsmasq
        echo > "/tmp/dhcp.leases.$section"
    else
        /etc/init.d/dnsmasq stop
        echo > "/tmp/dhcp.leases"
    fi

    ifdown "$section"
    sleep 3
    ifup "$section"

    # start service
    if [ "$vlan_id" -gt 0 ] ; then
        /etc/init.d/dnsmasq start "$section"_dnsmasq 2>/dev/null
    else
        /etc/init.d/dnsmasq start 2>/dev/null
    fi

    reset_mt7531 $vlan_id

    logger -t ql_lan_config "set_route() exit, VLAN$vlan_id"
}

set_ippt_without_nat() {
    local vlan_id="$1"
    local ifname="$2"
    local v4_addr="$3"
    local v4_prefix="$4"
    local v4_gw="$5"
    local v4_dns1="$6"
    local v4_dns2="$7"
    local v4_mtu="$8"
    local v6_addr="$9"
    local v6_dns1="${10}"
    local v6_dns2="${11}"
    local odu_addr="${12}"
    local odu_dhcp_offset="${13}"
    local section="lan"

    logger -t ql_lan_config "set_ippt_without_nat() entry, VLAN$vlan_id"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    # reset network.ql_lanX
    if [ "$vlan_id" -eq 0 ]; then
        ccmni_dev=$(uci get network.@device[0].ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.@device[0].ports=$ccmni_dev
    else
        ccmni_dev=$(uci get network.ql_$section.ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.ql_$section.ports=$ccmni_dev
    fi

    # reset dhcp.lanX
    uci del dhcp.$section.dhcp_option
    uci del dhcp.$section.dns
    uci del dhcp.$section.public_ip
    uci del dhcp.ql_$section 2>/dev/null

    macaddr="$(cat /sys/class/net/$ifname/address)" 2>/dev/null
    uci set network."$section".macaddr="$macaddr"
    uci -q delete network."$section".ip6assign

    if [ "$vlan_id" -eq 0 ]; then
        uci add_list network.@device[0].ports=$ifname
    else
        uci add_list network.ql_$section.ports=$ifname
    fi

    if [ "$v4_addr" != "" ] ; then
        local v4_netmask
        eval "$(ipcalc.sh "$v4_addr/$v4_prefix")";v4_netmask=$NETMASK

        # workaround
        if [ $odu_dhcp_offset -eq 0 ] ; then
            logger -t ql_lan_config "special addr:$v4_addr"

            v4_netmask="255.0.0.0"

            v4_addr2=$(echo "$v4_addr" | awk -F. '{print $2}')
            v4_addr3=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=65536*v4_addr2+256*v4_addr3

            odu_addr=${v4_addr%.*}.2
            v4_gw=${v4_addr%.*}.1
        elif [ $odu_dhcp_offset -eq 255 ] ; then
            logger -t ql_lan_config "special addr:$v4_addr"

            v4_netmask="255.255.0.0"
            segment=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=segment*256+255
        else
            logger -t ql_lan_config "No special treatment is required"
        fi

        logger -t ql_lan_config "new ip:$odu_addr"
        uci set network."$section".ipaddr="$odu_addr"
        uci set network."$section".netmask="$v4_netmask"
        uci set network."$section".gateway="$v4_gw"
        uci set network."$section".dns="$v4_dns1"

        uci set dhcp."$section".start="$odu_dhcp_offset"
        uci set dhcp."$section".limit=1
        uci add_list dhcp."$section".dhcp_option="1,$v4_netmask"
        uci add_list dhcp."$section".dhcp_option="3,$v4_gw"
        uci add_list dhcp."$section".dhcp_option="6,$v4_dns1"
        uci add_list dhcp."$section".dhcp_option="26,$v4_mtu"

        # if enable ippt auto adapt function
        auto_adapt=$(uci -q get system.@system[0].ql_ippt_auto_adapt)
        if [ "$auto_adapt" -eq 1 ]; then
            uci set dhcp.ql_"$section"=host
            uci set dhcp.ql_"$section".ip="$v4_addr"
            uci set dhcp.ql_"$section".name="$section"
            [ "$vlan_id" -gt 0 ] && uci set dhcp.ql_"$section".instance=''$section'_dnsmasq'

            ippt_method=$(uci -q get dhcp."$section".ippt_method)
            if [ -n "$ippt_method" -a "$ippt_method" -eq 1 ]; then
                ippt_mac=$(uci -q get dhcp."$section".ippt_mac)
                if [ -z "$ippt_mac" ]; then
                    uci set dhcp.ql_"$section".mac="ff:ff:ff:ff:ff:ff"
                else
                    uci set dhcp.ql_"$section".mac="$ippt_mac"
                fi
            else
                uci set dhcp.ql_"$section".mac="ff:ff:ff:ff:ff:ff"
            fi
        fi
    fi

    if [ "$v6_addr" != "" ] ; then
        sysctl -w net.ipv6.conf.default.router_solicitations=3
        sysctl -w net.ipv6.conf.default.accept_ra=2
        uci set network."$section".ip6addr="$v6_addr"

        if [ -n "$v6_dns1" ] ; then
            uci add_list dhcp."$section".dns="$v6_dns1"
        fi
        if [ -n "$v6_dns2" ] ; then
            uci add_list dhcp."$section".dns="$v6_dns2"
        fi
    else
        sysctl -w net.ipv6.conf.default.router_solicitations=-1
        sysctl -w net.ipv6.conf.default.accept_ra=0
    fi

    # save config
    uci commit network
    uci commit dhcp

    # stop service
    if [ "$vlan_id" -gt 0 ] ; then
        /etc/init.d/dnsmasq stop "$section"_dnsmasq
    else
        /etc/init.d/dnsmasq stop
    fi

    ifdown "$section"
    sleep 3
    ifup "$section"
    ip addr flush dev $ifname
    brctl addif br-$section $ifname

    if [ "$auto_adapt" -eq 1 ]; then
        /etc/init.d/ql_ippt reload
    else
        # start service
        if [ "$vlan_id" -gt 0 ] ; then
            /etc/init.d/dnsmasq start "$section"_dnsmasq 2>/dev/null
            ubus -t 30 wait_for dnsmasq."$section"_dnsmasq
        else
            /etc/init.d/dnsmasq start 2>/dev/null
            ubus -t 30 wait_for dnsmasq
        fi
    fi

    reset_mt7531 "$vlan_id"

    logger -t ql_lan_config "set_ippt_without_nat() exit, VLAN$vlan_id"
}

ql_wait_for_device_access() {
    local section="lan"
    local vlan_id="$1"
    local v4_addr="$2"
    local line_num
    local mac
    local exit_flag=0

    logger -t ql_lan_config "ql_wait_for_device_access() entry"

    [ $vlan_id -gt 0 ] && section="lan"$vlan_id
    ip neigh flush dev br-$section

    while true;
    do
        line_num=$(ip -4 neigh show dev br-$section | wc -l)
        if [ "$line_num" = 0 ] ; then
            sleep 1
            continue
        fi

        for i in `seq 1 1 $line_num`
        do
            mac=$(ip -4 neigh show dev br-$section | awk 'NR=='$i'' | awk '{ print $3 }')
            [ -z "$mac" ] && continue

            [ "$mac" = "06:16:26:36:46:56" ] && continue

            echo "$mac"
            exit_flag=1
            break
        done

        [ "$exit_flag" -eq 1 ] && break
    done

    uci set dhcp.ql_"$section"=host
    uci set dhcp.ql_"$section".ip="$v4_addr"
    uci set dhcp.ql_"$section".mac="$mac"
    uci set dhcp.ql_"$section".name="$section"
    uci commit dhcp

    if [ $vlan_id -gt 0 ] ; then
        /etc/init.d/dnsmasq reload "$section"_dnsmasq
        echo > "/tmp/dhcp.leases.$section"
    else
        /etc/init.d/dnsmasq reload
        echo > "/tmp/dhcp.leases"
    fi

    reset_mt7531 "$vlan_id"
    reset_rtl8221 "$vlan_id"
    reset_rndis "$vlan_id"

    logger -t ql_lan_config "MAC: $mac use the $v4_addr"
    logger -t ql_lan_config "ql_wait_for_device_access() exit"
}

set_ippt_with_nat() {
    local vlan_id="$1"
    local ifname="$2"
    local v4_addr="$3"
    local v4_prefix="$4"
    local v4_gw="$5"
    local v4_dns1="$6"
    local v4_dns2="$7"
    local v4_mtu="$8"
    local v6_addr="$9"
    local v6_dns1="${10}"
    local v6_dns2="${11}"
    local odu_addr="${12}"
    local odu_dhcp_offset="${13}"
    local v6_prefix="${14}"
    local v6_prefix_len="${15}"
    local section="lan"

    local lan_addr
    local lan_v6addr

    logger -t ql_lan_config "set_ippt_with_nat() entry, VLAN$vlan_id"

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    # reset network.ql_lanX
    if [ "$vlan_id" -eq 0 ]; then
        ccmni_dev=$(uci get network.@device[0].ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.@device[0].ports=$ccmni_dev
    else
        ccmni_dev=$(uci get network.ql_$section.ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.ql_$section.ports=$ccmni_dev
    fi

    # reset dhcp.lanX
    uci del dhcp.$section.dhcp_option
    uci del dhcp.$section.dns
    uci del dhcp.$section.public_ip
    uci del dhcp.ql_$section 2>/dev/null

    lan_addr=$(uci -q get network.$section.ipaddr)

    macaddr="$(cat /sys/class/net/$ifname/address)" 2>/dev/null
    uci set network."$section".macaddr="$macaddr"
    uci -q delete network."$section".ip6assign

    if [ "$vlan_id" -eq 0 ]; then
        uci add_list network.@device[0].ports=$ifname
    else
        uci add_list network.ql_$section.ports=$ifname
    fi

    uci set network.ql_"$section"_alias=alias
    uci set network.ql_"$section"_alias.interface="$section"
    uci set network.ql_"$section"_alias.proto="static"

    uci -q delete dhcp."$section".dhcp_option
    uci -q delete dhcp."$section".dns

    if [ "$v4_addr" != "" ] ; then
        local v4_netmask
        eval "$(ipcalc.sh "$v4_addr/$v4_prefix")";v4_netmask=$NETMASK

        # workaround
        if [ $odu_dhcp_offset -eq 0 ] ; then
            logger -t ql_lan_config "special addr:$v4_addr"

            v4_netmask="255.0.0.0"

            v4_addr2=$(echo "$v4_addr" | awk -F. '{print $2}')
            v4_addr3=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=65536*v4_addr2+256*v4_addr3

            odu_addr=${v4_addr%.*}.2
            v4_gw=${v4_addr%.*}.1
        elif [ $odu_dhcp_offset -eq 255 ] ; then
            logger -t ql_lan_config "special addr:$v4_addr"

            v4_netmask="255.255.0.0"
            segment=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=segment*256+255
        else
            logger -t ql_lan_config "No special treatment is required"
        fi

        logger -t ql_lan_config "add new ip:$odu_addr"
        uci set network.ql_"$section"_alias.ipaddr="$odu_addr"
        uci set network.ql_"$section"_alias.netmask="$v4_netmask"

        uci add_list dhcp."$section".dhcp_option="1,255.255.255.0"
        uci add_list dhcp."$section".dhcp_option="3,$lan_addr"
        uci add_list dhcp."$section".dhcp_option="6,$v4_dns1"
        uci add_list dhcp."$section".dhcp_option="26,$v4_mtu"
        uci set dhcp."$section".public_ip="$v4_addr"

        # if enable ippt auto adapt function
        auto_adapt=$(uci -q get system.@system[0].ql_ippt_auto_adapt)
        if [ "$auto_adapt" -eq 1 ]; then
            uci set dhcp.ql_"$section"=host
            uci set dhcp.ql_"$section".ip="$v4_addr"
            uci set dhcp.ql_"$section".name="$section"
            [ "$vlan_id" -gt 0 ] && uci set dhcp.ql_"$section".instance=''$section'_dnsmasq'

            ippt_method=$(uci -q get dhcp."$section".ippt_method)
            if [ -n "$ippt_method" -a "$ippt_method" -eq 1 ]; then
                ippt_mac=$(uci -q get dhcp."$section".ippt_mac)
                if [ -z "$ippt_mac" ]; then
                    uci set dhcp.ql_"$section".mac="ff:ff:ff:ff:ff:ff"
                else
                    uci set dhcp.ql_"$section".mac="$ippt_mac"
                fi
            else
                uci set dhcp.ql_"$section".mac="ff:ff:ff:ff:ff:ff"
            fi
        fi
    fi

    if [ "$v6_addr" != "" ] ; then
        lan_v6addr="$v6_prefix"1/$v6_prefix_len
        uci set network.ql_"$section"_alias.ip6addr="$lan_v6addr"

        if [ -n "$v6_dns1" ] ; then
            uci add_list dhcp."$section".dns="$v6_dns1"
        fi

        if [ -n "$v6_dns2" ] ; then
            uci add_list dhcp."$section".dns="$v6_dns2"
        fi
    else
        sysctl -w net.ipv6.conf.default.router_solicitations=-1
        sysctl -w net.ipv6.conf.default.accept_ra=0
    fi

    # save config
    uci commit network
    uci commit dhcp

    ebtables -t nat -D POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_addr/24 --arp-opcode Reply -j DROP >/dev/null 2>&1
    ebtables -t nat -I POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_addr/24 --arp-opcode Reply -j DROP

    # stop service
    if [ "$vlan_id" -gt 0 ] ; then
        /etc/init.d/dnsmasq stop "$section"_dnsmasq
    else
        /etc/init.d/dnsmasq stop
    fi

    ifdown "$section"
    sleep 3
    ifup "$section"
    ip addr flush dev $ifname
    brctl addif br-$section $ifname

    if [ "$auto_adapt" -eq 1 ]; then
        /etc/init.d/ql_ippt reload
    else
        # start service
        if [ "$vlan_id" -gt 0 ] ; then
            /etc/init.d/dnsmasq start "$section"_dnsmasq 2>/dev/null
            ubus -t 30 wait_for dnsmasq."$section"_dnsmasq
        else
            /etc/init.d/dnsmasq start 2>/dev/null
            ubus -t 30 wait_for dnsmasq
        fi
    fi

    reset_mt7531 "$vlan_id"

    # TBD: set rule & route
    if [ "$v4_addr" != "" ] ; then
        if [ $vlan_id -eq 0 ] ; then
            ifconfig $ifname 169.254.2.1
            ip route add default dev $ifname
            ip route add $v4_addr dev br-$section
        else
            ifconfig $ifname 169.254.2.$vlan_id
            ip route add default dev $ifname table $vlan_id
            ip route add $v4_addr dev br-$section table $vlan_id
            ip route add 192.168."$vlan_id".0/24 dev br-$section table $vlan_id

            # set rule
            iptables -D PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
            iptables -I PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan_id -w
            ip rule del fwmark 30$vlan_id table $vlan_id >/dev/null 2>&1
            ip rule add fwmark 30$vlan_id table $vlan_id
        fi

        iptables -t nat -D POSTROUTING -o $ifname -j SNAT --to $v4_addr >/dev/null 2>&1
        iptables -t nat -I POSTROUTING -o $ifname -j SNAT --to $v4_addr
    fi

    if [ "$v6_addr" != "" ] ; then
        ip -6 addr del $v6_addr/64 dev $ifname
        ip -6 addr add $v6_addr/128 dev $ifname
        if [ $vlan_id -gt 0 ] ; then
            ip -6 route add default dev $ifname table $vlan_id
            ip -6 route add $lan_v6addr dev br-$section table $vlan_id

            # set rule
            ip6tables -D PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan_id -w >/dev/null 2>&1
            ip6tables -I PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan_id -w
            ip -6 rule del fwmark 30$vlan_id table $vlan_id >/dev/null 2>&1
            ip -6 rule add fwmark 30$vlan_id table $vlan_id
        fi
    fi

    ifconfig br-$section promisc

    # TBD: Using scripts to monitor device access
    if [ "$auto_adapt" -eq 0 -a "$v4_addr" != "" ]; then
        ql_wait_for_device_access "$vlan_id" "$v4_addr"
    fi

    logger -t ql_lan_config "set_ippt_with_nat() exit, VLAN$vlan_id"
}

# function to set mode for VLAN
function set_mode() {
    local vlan_id=$1
    local mode=$2
    local section="lan"

    logger -t ql_lan_config "set_mode() entry"

    # check vlan_id
    if [ "$vlan_id" -lt 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    # check mode
    if [ "$mode" -lt 1 ] || [ "$mode" -gt 3 ]; then
        echo "FAIL: Invalid mode"
        return 1
    fi

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id
    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    if [ "$mode" -eq 1 ]; then
        set_route "$vlan_id"
    elif [ "$mode" -eq 2 ]; then
        set_ippt_without_nat "$vlan_id" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}" "${14}"
    else
        set_ippt_with_nat "$vlan_id" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}" "${14}" "${15}" "${16}"
    fi

    logger -t ql_lan_config "set_mode() exit"
}

# function to set mtu for VLAN
function set_mtu() {
    local vlan_id="$1"
    local mtu="$2"
    local section="lan"
    local cnt="1"
    local update="1"

    logger -t ql_lan_config "set_mtu() entry"

    # check vlan_id
    if [ "$vlan_id" -lt 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    [ -z "$mtu" ] && return

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    while true;
    do
        result=$(uci -q get dhcp."$section".dhcp_option | awk -F" " '{print $'$cnt'}')
        [ -z $result ] && break

        option=$(echo $result | awk -F, '{print $1}')
        if [ $option -eq 26 ] ; then
            value=$(echo $result | awk -F, '{print $2}')
            if [ "$value" -eq "$mtu" ] ; then
                update=0
                logger -t ql_lan_config "mtu has no change!!!"
            else
                update=1
                uci del_list dhcp."$section".dhcp_option="$result"
            fi

            break
        fi

        let cnt++
    done

    logger -t ql_lan_config "ql_update_mtu $mtu $update"
    if [ "$update" -eq 1 ] ; then
        uci add_list dhcp."$section".dhcp_option="26,$mtu"
        uci commit dhcp

        if [ "$vlan_id" -eq 0 ]; then
            /etc/init.d/dnsmasq restart >/dev/null 2>&1
        else
            /etc/init.d/dnsmasq stop "$section"_dnsmasq >/dev/null 2>&1
            /etc/init.d/dnsmasq start "$section"_dnsmasq >/dev/null 2>&1
        fi

        reset_mt7531 "$vlan_id"
    fi

    logger -t ql_lan_config "set_mtu() exit"
}

# function to reset mtu for VLAN
function reset_mtu() {
    local vlan_id="$1"
    local section="lan"
    local cnt="1"

    logger -t ql_lan_config "reset_mtu() entry"

    # check vlan_id
    if [ "$vlan_id" -lt 0 ]; then
        echo "FAIL: Invalid vlan_id"
        return 1
    fi

    [ "$vlan_id" -gt 0 ] && section="lan"$vlan_id

    result=$(uci -q get network.$section)
    if [ -z "$result" ]; then
        echo "FAIL: VLAN$vlan_id doesn't exist"
        return 1
    fi

    while true;
    do
        result=$(uci get dhcp."$section".dhcp_option | awk -F" " '{print $'$cnt'}')
        [ -z $result ] && break

        option=$(echo $result | awk -F, '{print $1}')
        if [ $option -eq 26 ] ; then
            uci del_list dhcp."$section".dhcp_option="$result"
            uci commit dhcp
            if [ "$vlan_id" -eq 0 ]; then
                /etc/init.d/dnsmasq restart >/dev/null 2>&1
            else
                /etc/init.d/dnsmasq stop "$section"_dnsmasq >/dev/null 2>&1
                /etc/init.d/dnsmasq start "$section"_dnsmasq >/dev/null 2>&1
            fi

            break
        fi

        let cnt++
    done

    logger -t ql_lan_config "reset_mtu() exit"
}

case $1 in
    add)
       logger -t ql_lan_config "Adding VLAN$2"
       add_vlan $2 $3
       break
       ;;

    del)
       logger -t ql_lan_config "Delete VLAN$2"
       del_vlan $2
       break
       ;;

    show)
       logger -t ql_lan_config "Show all VLANs"
       show_vlan
       break
       ;;

    set_lanip)
       logger -t ql_lan_config "Set VLAN$2 IP"
       set_vlan_ip $2 $3
       break
       ;;

    set_dhcpip)
       logger -t ql_lan_config "Set VLAN$2 IP Range"
       set_dhcp_ip $2 $3 $4
       break
       ;;

    set_mode)
       logger -t ql_lan_config "Set mode for VLAN$2"
       set_mode "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}" "${14}" "${15}" "${16}" "${17}"
       break
       ;;

    set_mtu)
       logger -t ql_lan_config "Set mtu for VLAN$2"
       set_mtu "$2" "$3"
       break
       ;;

    reset_mtu)
       logger -t ql_lan_config "Reset mtu for VLAN$2"
       reset_mtu "$2"
       break
       ;;

    reset_nic)
       logger -t ql_lan_config "Reset nic for VLAN$2"
       if [ "$2" -eq 0 ]; then
            ifup lan
       else
            ifup "lan"$2
       fi

       reset_mt7531 "$2"
       reset_rtl8221 "$2"
       break
       ;;

     *)
       echo -e "Usage: ql_lan_config.sh [OPTION] [PARAM]\n\
       ql_lan_config.sh add vlan_id device_type\n\
       ql_lan_config.sh del vlan_id\n\
       ql_lan_config.sh set_lanip vlan_id ip_address\n\
       ql_lan_config.sh set_dhcpip vlan_id start limit\n\
       ql_lan_config.sh set_mode vlan_id mode ifname v4_addr v4_mask v4_gw v4_dns1 v4_dns2 v4_mtu v6_addr v6_dns1 v6_dns2 odu_addr odu_dhcp_offset v6_prefix v6_prefix_len\n\
       ql_lan_config.sh set_mtu vlan_id mtu\n\
       ql_lan_config.sh reset_mtu vlan_id\n\
       ql_lan_config.sh reset_nic vlan_id\n"
       ;;

esac