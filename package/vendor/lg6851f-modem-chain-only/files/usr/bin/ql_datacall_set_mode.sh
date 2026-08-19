#!/bin/sh

#This script is used after sample_datacall is executed
#Only applicable to API single datacall scenarios => (ccmni<->lan)

reset_rndis() {
    local section="lan"
    local member

    member=$(uci -q get network.@device[0].ports | grep rndis0)

    if [ -z "$member" ]; then
        echo "no need to reset rndis0"
        return 1
    fi

    ifconfig rndis0 down
    sleep 1
    ifconfig rndis0 up
}

reset_rtl8221() {
    local section="lan"
    local member

    member=$(uci -q get network.@device[0].ports | grep eth1)

    if [ -z "$member" ]; then
        echo "no need to reset rtl8221"
        return 1
    fi

    ifconfig eth1 down
    ifconfig eth1 up
}

reset_mt7531() {
    local section="lan"
    local member

    member=$(uci -q get network.@device[0].ports | grep eth0)

    if [ -z "$member" ]; then
        echo "no need to reset mt7531"
        return 1
    fi

    enable_vlan=$(uci -q get network.eth0.enable_vlan)
    if [ -z "$enable_vlan" ] || [ "$enable_vlan" -eq 0 ] ; then
        echo "MT7531 vlan disable, reset all port"
        for i in `seq 0 1 4`
        do
                switch phy cl22 w $i 0 0x1840
                switch phy cl22 w $i 0 0x1040
        done
    else
        echo "MT7531 vlan enable, check which port should be reset"

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

                echo "port$port need to be reset"
                switch phy cl22 w $port 0 0x1840
                switch phy cl22 w $port 0 0x1040
                let j++
            done

            let i++
        done
    fi
}

check_ccmni()
{
    read -p "please input the ccmniX (such as ccmni1): " ccmni

    # check ccmni
    if [ "${ccmni:0:5}" != "ccmni" ] ; then
        echo "please input correct parameter"
        exit
    fi
}

check_dev()
{
    read -p "please input the LAN device (such as eth0 and so on): " dev

    # check dev
    if [ "${dev:0:3}" != "eth" ] && [ "${dev:0:3}" != "ra0" ] && [ "${dev:0:4}" != "rai0" ]; then
        echo "please input correct parameter"
        exit
    fi
}

check_addr()
{
    read -p "please input the IPv4 public ip (If not, please enter to skip this step): " v4_addr

    if [ -n "$v4_addr" ] ; then
        read -p "please input the IPv4 netmask: " netmask
        [ -z "$netmask" ] && exit
    fi

    read -p "please input the IPv6 global addr (If not, please enter to skip this step): " v6_addr
}

reset()
{
    uci -q delete network.lan.gateway
    uci -q delete network.lan.dns
    uci -q delete network.lan.ip6addr
    uci -q delete network.lan.macaddr
    uci -q delete network.@alias[0]
    uci -q delete dhcp.lan.dhcp_option
    uci -q delete dhcp.lan.dns
    uci -q delete dhcp.lan.public_ip
    uci -q delete dhcp.@host[0]

    uci set network.lan.ipaddr="192.168.1.1"
    uci set network.lan.netmask="255.255.255.0"
    uci set network.lan.ip6assign=60
    uci set dhcp.lan.start=150
    uci set dhcp.lan.limit=100

    ccmni_dev=$(uci get network.@device[0].ports | grep -o 'ccmni[0-9]*')
    [ -n "$ccmni_dev" ] && uci del_list network.@device[0].ports=$ccmni_dev

    uci commit
}

ippt_without_nat()
{
    # input & check
    check_ccmni

    check_dev

    check_addr

    read -p "please input the DNS (If not, please enter to skip this step): " all_dns

    # reset bridge
    reset

    # enable bridge_mode contrl
    echo 1 > /proc/ccmni/bridge_mode_control

    # set network
    uci -q delete network.lan.ip6assign
    uci -q delete network.@device[0].ports

    cnt=1
    while true;
    do
        ports=$(echo $dev | awk -F" " '{print $'$cnt'}')
        [ -z $ports ] && break

        if [ "${dev:0:3}" != "ra0" ] && [ "${dev:0:4}" != "rai0" ]; then
            uci add_list network.@device[0].ports="$ports"
        fi

        let cnt++
    done

    uci add_list network.@device[0].ports="$ccmni"

    macaddr="$(cat /sys/class/net/$ccmni/address)" 2>/dev/null
    uci set network.lan.macaddr="$macaddr"

    if [ -n "$v4_addr" ] ; then
        # Calculate lan address and gateway
        dhcp_offset=$(echo "$v4_addr" | awk -F. '{print $4}')
        base_offset=8
        while [ $base_offset -le 256 ]
        do
            tmp=$(( $dhcp_offset % $base_offset ))
            if [ $tmp -eq 0 ] || [ $tmp -eq $(( $base_offset - 1 )) ] ; then
                base_offset=$(( $base_offset * 2 ))
                let base_offset*=2
                continue
            fi

            if [ $(( $tmp + 2 )) -lt $base_offset ] ; then
                gw_addr3=$(($dhcp_offset+1))
                lan_addr3=$(($dhcp_offset+2))
            else
                gw_addr3=$(($dhcp_offset-1))
                lan_addr3=$(($dhcp_offset-2))
            fi
            break
        done

        lan_addr=${v4_addr%.*}.$lan_addr3
        gw_addr=${v4_addr%.*}.$gw_addr3

        # set network
        uci set network.lan.ipaddr="$lan_addr"
        uci set network.lan.gateway=$gw_addr
        uci set network.lan.netmask=$netmask

        # set dhcp
        uci set dhcp.lan.start="$dhcp_offset"
        uci set dhcp.lan.limit=1
        uci add_list dhcp.lan.dhcp_option="1,$netmask"
        uci add_list dhcp.lan.dhcp_option="3,$gw_addr"
    fi

    if [  -n "$v6_addr" ]; then
        sysctl -w net.ipv6.conf.default.router_solicitations=3
        sysctl -w net.ipv6.conf.default.accept_ra=2
        uci set network.lan.ip6addr="$v6_addr"
    fi

    if [  -n "$all_dns" ]; then
        cnt=1
        while true;
        do
            dns=$(echo $all_dns | awk -F" " '{print $'$cnt'}')
            [ -z $dns ] && break

            # set dns
            len=$(echo "$dns" | awk '{print length($0)}')
            if [ $len -gt 0 ] && [ $len -lt 16 ] ; then
                uci add_list dhcp.lan.dhcp_option="6,$dns"
            elif [ $len -gt 16 ] && [ $len -lt 46 ] ; then
                uci add_list dhcp.lan.dns="$dns"
            fi

            let cnt++
        done
    fi

    # save config
    uci commit network
    uci commit dhcp

    # stop service
    /etc/init.d/dnsmasq stop

    ifdown lan
    sleep 3
    ifup lan
    ip addr flush dev $ccmni
    brctl addif br-lan $ccmni

    cnt=1
    while true;
    do
        ports=$(echo $dev | awk -F" " '{print $'$cnt'}')
        [ -z $ports ] && break

        if [ "${dev:0:3}" = "ra0" ] || [ "${dev:0:4}" = "rai0" ]; then
            brctl addif br-lan $ports
        fi

        let cnt++
    done

    # start service
    /etc/init.d/dnsmasq start 2>/dev/null
    ubus -t 30 wait_for dnsmasq

    # down up link
    reset_mt7531
    reset_rtl8221
    reset_rndis
}

ql_wait_for_device_access() {
    local line_num
    local mac
    local exit_flag=0

    ip neigh flush dev br-lan

    while true;
    do
        echo "wait for device access..."
        line_num=$(ip -4 neigh show dev br-lan | wc -l)
        if [ "$line_num" = 0 ] ; then
            sleep 1
            continue
        fi

        for i in `seq 1 1 $line_num`
        do
            mac=$(ip -4 neigh show dev br-lan | awk 'NR=='$i'' | awk '{ print $3 }')
            [ -z "$mac" ] && continue

            [ "$mac" = "06:16:26:36:46:56" ] && continue

            echo "$mac"
            exit_flag=1
            break
        done

        [ "$exit_flag" -eq 1 ] && break
    done

    uci add dhcp host
    uci set dhcp.@host[-1].ip="$v4_addr"
    uci set dhcp.@host[-1].mac="$mac"
    uci set dhcp.@host[-1].name="lan"
    uci commit dhcp

    /etc/init.d/dnsmasq reload
    echo > "/tmp/dhcp.leases"

    # down up link
    reset_mt7531
    reset_rtl8221
    reset_rndis

    if [  -n "$all_dns" ]; then
        echo > /etc/resolv.conf
        echo search lan >> /etc/resolv.conf
        echo nameserver 127.0.0.1 >> /etc/resolv.conf
        echo nameserver ::1 >> /etc/resolv.conf
        echo > /tmp/resolv.conf.d/resolv.conf.auto

        cnt=1
        while true;
        do
            dns=$(echo $all_dns | awk -F" " '{print $'$cnt'}')
            [ -z $dns ] && break

            # set dns
            echo nameserver $dns >> /tmp/resolv.conf.d/resolv.conf.auto

            let cnt++
        done
    fi
}

ippt_with_nat()
{
    # input & check
    check_ccmni

    check_dev

    check_addr

    read -p "please input the DNS (If not, please enter to skip this step): " all_dns

    # reset bridge
    reset

    # enable bridge_mode contrl
    echo 1 > /proc/ccmni/bridge_mode_control

    # set network
    uci -q delete network.lan.ip6assign
    uci -q delete network.@device[0].ports

    cnt=1
    while true;
    do
        ports=$(echo $dev | awk -F" " '{print $'$cnt'}')
        [ -z $ports ] && break

        if [ "${dev:0:3}" != "ra0" ] && [ "${dev:0:4}" != "rai0" ]; then
            uci add_list network.@device[0].ports="$ports"
        fi

        let cnt++
    done

    uci add_list network.@device[0].ports="$ccmni"

    macaddr="$(cat /sys/class/net/$ccmni/address)" 2>/dev/null
    uci set network.lan.macaddr="$macaddr"

    uci add network alias
    uci set network.@alias[-1].interface="lan"
    uci set network.@alias[-1].proto="static"

    if [ -n "$v4_addr" ] ; then
        # Calculate lan address and gateway
        dhcp_offset=$(echo "$v4_addr" | awk -F. '{print $4}')
        base_offset=8
        while [ $base_offset -le 256 ]
        do
            tmp=$(( $dhcp_offset % $base_offset ))
            if [ $tmp -eq 0 ] || [ $tmp -eq $(( $base_offset - 1 )) ] ; then
                base_offset=$(( $base_offset * 2 ))
                let base_offset*=2
                continue
            fi

            if [ $(( $tmp + 2 )) -lt $base_offset ] ; then
                gw_addr3=$(($dhcp_offset+1))
                lan_addr3=$(($dhcp_offset+2))
            else
                gw_addr3=$(($dhcp_offset-1))
                lan_addr3=$(($dhcp_offset-2))
            fi
            break
        done

        lan_addr=${v4_addr%.*}.$lan_addr3
        gw_addr=${v4_addr%.*}.$gw_addr3

        # set network
        uci set network.@alias[-1].ipaddr="$lan_addr"
        uci set network.@alias[-1].netmask=$netmask

        # set dhcp
        uci add_list dhcp.lan.dhcp_option="1,255.255.255.0"

        lan_private_ip=$(uci -q get network.lan.ipaddr)
        uci add_list dhcp.lan.dhcp_option="3,$lan_private_ip"
        uci set dhcp.lan.public_ip="$v4_addr"
    fi

    if [  -n "$v6_addr" ]; then
        ipv6_prefix1=$(echo "$v6_addr" | awk -F: '{print $1}')
        ipv6_prefix2=$(echo "$v6_addr" | awk -F: '{print $2}')
        ipv6_prefix3=$(echo "$v6_addr" | awk -F: '{print $3}')
        ipv6_prefix4=$(echo "$v6_addr" | awk -F: '{print $4}')
        lan_v6addr="$ipv6_prefix1:$ipv6_prefix2:$ipv6_prefix3:$ipv6_prefix4::1/64"
        uci set network.@alias[-1].ip6addr="$lan_v6addr"

        /etc/init.d/odhcpd reload >/dev/null 2>&1
        ip -6 rule del pref 1000
    fi

    if [  -n "$all_dns" ]; then
        cnt=1
        while true;
        do
            dns=$(echo $all_dns | awk -F" " '{print $'$cnt'}')
            [ -z $dns ] && break

            # set dns
            len=$(echo "$dns" | awk '{print length($0)}')
            if [ $len -gt 0 ] && [ $len -lt 16 ] ; then
                uci add_list dhcp.lan.dhcp_option="6,$dns"
            elif [ $len -gt 16 ] && [ $len -lt 46 ] ; then
                uci add_list dhcp.lan.dns="$dns"
            fi

            let cnt++
        done
    fi

    # save config
    uci commit network
    uci commit dhcp

    # stop service
    /etc/init.d/dnsmasq stop

    ifdown lan
    sleep 3
    ifup lan
    ip addr flush dev $ccmni
    brctl addif br-lan $ccmni

    cnt=1
    while true;
    do
        ports=$(echo $dev | awk -F" " '{print $'$cnt'}')
        [ -z $ports ] && break

        if [ "${dev:0:3}" = "ra0" ] || [ "${dev:0:4}" = "rai0" ]; then
            brctl addif br-lan $ports
        fi

        let cnt++
    done

    # start service
    /etc/init.d/dnsmasq start 2>/dev/null
    ubus -t 30 wait_for dnsmasq

    reset_mt7531

    if [  -n "$v4_addr" ]; then
        ifconfig $ccmni 169.254.2.1
        ip route add default dev $ccmni
        ip route add $v4_addr dev br-lan

        ebtables -t nat -D POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_private_ip/24 --arp-opcode Reply -j DROP >/dev/null 2>&1
        ebtables -t nat -I POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_private_ip/24 --arp-opcode Reply -j DROP

        iptables -D FORWARD -t filter -i br-lan -j ACCEPT >/dev/null 2>&1
        iptables -I FORWARD -t filter -i br-lan -j ACCEPT

        iptables -t nat -D POSTROUTING -o $ccmni -j SNAT --to $v4_addr >/dev/null 2>&1
        iptables -t nat -I POSTROUTING -o $ccmni -j SNAT --to $v4_addr
    fi

    if [  -n "$v6_addr" ]; then
        /etc/init.d/odhcpd reload >/dev/null 2>&1
        ip -6 rule del pref 1000

        echo 0 > proc/sys/net/ipv6/conf/$ccmni/disable_ipv6
        ip -6 addr add $v6_addr/128 dev $ccmni
        ip -6 route add default dev $ccmni

        ip6tables -D FORWARD -t filter -i br-lan -j ACCEPT >/dev/null 2>&1
        ip6tables -I FORWARD -t filter -i br-lan -j ACCEPT
    fi

    ifconfig br-lan promisc

    if [  -n "$all_dns" ]; then
        echo > /etc/resolv.conf
        echo search lan >> /etc/resolv.conf
        echo nameserver 127.0.0.1 >> /etc/resolv.conf
        echo nameserver ::1 >> /etc/resolv.conf
        echo > /tmp/resolv.conf.d/resolv.conf.auto

        cnt=1
        while true;
        do
            dns=$(echo $all_dns | awk -F" " '{print $'$cnt'}')
            [ -z $dns ] && break

            # set dns
            echo nameserver $dns >> /tmp/resolv.conf.d/resolv.conf.auto

            let cnt++
        done
    fi

    ql_wait_for_device_access
}

route_mode()
{
    # input & check
    check_ccmni

    read -p "please input the IPv6 global addr (If not, please enter to skip this step): " v6_addr

    read -p "please input the DNS (If not, please enter to skip this step): " all_dns

    # reset configuration
    reset
    ifup lan

    # down up link
    reset_mt7531
    reset_rtl8221
    reset_rndis

    for wifi in $(iwpriv | grep Available | grep -Eo '^[a-zA-Z0-9]+');
    do
        if [ $(ifconfig | grep -c $wifi ) == 1 ] ; then
            brctl addif br-lan $wifi
        fi
    done

    fw3 restart >/dev/null 2>&1

    if [ -n "$v6_addr" ] ; then
        ip -6 addr del ${v6_addr}/64 dev $ccmni >/dev/null 2>&1
        ip -6 addr add ${v6_addr}/128 dev $ccmni >/dev/null 2>&1

        # get & set ipv6 prefix
        ipv6_prefix1=$(echo "$v6_addr" | awk -F: '{print $1}')
        ipv6_prefix2=$(echo "$v6_addr" | awk -F: '{print $2}')
        ipv6_prefix3=$(echo "$v6_addr" | awk -F: '{print $3}')
        ipv6_prefix4=$(echo "$v6_addr" | awk -F: '{print $4}')
        lan_ip6addr="$ipv6_prefix1:$ipv6_prefix2:$ipv6_prefix3:$ipv6_prefix4::1/64"
        ip -6 addr add $lan_ip6addr dev br-lan
        /etc/init.d/odhcpd reload >/dev/null 2>&1

        ip -6 rule del pref 1000
    fi

    if [  -n "$all_dns" ]; then
        echo > /etc/resolv.conf
        echo search lan >> /etc/resolv.conf
        echo nameserver 127.0.0.1 >> /etc/resolv.conf
        echo nameserver ::1 >> /etc/resolv.conf
        echo > /tmp/resolv.conf.d/resolv.conf.auto

        cnt=1
        while true;
        do
            dns=$(echo $all_dns | awk -F" " '{print $'$cnt'}')
            [ -z $dns ] && break

            # set dns
            echo nameserver $dns >> /tmp/resolv.conf.d/resolv.conf.auto

            let cnt++
        done
    fi

    iptables -t nat -A POSTROUTING -o $ccmni -j MASQUERADE
    iptables -I FORWARD -t filter -i br-lan -j ACCEPT
    ip6tables -I FORWARD -t filter -i br-lan -j ACCEPT
}

reset_to_default()
{
    # reset configuration
    reset

    uci -q delete network.@device[0].ports
    uci add_list network.@device[0].ports="eth0"
    uci add_list network.@device[0].ports="eth1"
    uci commit network
    ifup lan

    for wifi in $(iwpriv | grep Available | grep -Eo '^[a-zA-Z0-9]+');
    do
        if [ $(ifconfig | grep -c $wifi ) == 1 ] ; then
            brctl addif br-lan $wifi
        fi
    done

    fw3 restart >/dev/null 2>&1

    # down up link
    reset_mt7531
    reset_rtl8221
    reset_rndis
}

echo "please choose mode: "
echo "0) route"
echo "1) ippt without nat"
echo "2) ippt with nat"
echo "3) reset to default configuration"
read mode
echo "-----------------------------------------"

case $mode in
    0)
        route_mode
        ;;
    1)
        ippt_without_nat
        ;;
    2)
        ippt_with_nat
        ;;
    3)
        reset_to_default
        ;;
    *)
        echo "commond error"
        ;;
esac