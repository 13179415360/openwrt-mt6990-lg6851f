#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
. /lib/functions.sh
. /lib/functions/network.sh
. ../netifd-proto.sh
. /usr/share/libubox/jshn.sh
init_proto "$@"
}

proto_ql_mipc_init_config() {
    available=1
    no_device=1
    teardown_on_l3_link_down=1

    proto_config_add_string apn
    proto_config_add_string pincode
    proto_config_add_int auth
    proto_config_add_int mtu
    proto_config_add_string username
    proto_config_add_string password
    proto_config_add_string bearer_bitmask
    proto_config_add_string iabearer_bitmask
    proto_config_add_int roamingtype
    proto_config_add_int iptype
    proto_config_add_string staticip
    proto_config_add_string staticgw
    proto_config_add_string staticmask
    proto_config_add_string staticdns
    proto_config_add_string staticipv6
    proto_config_add_string staticgwv6
    proto_config_add_string staticipv6prefix
    proto_config_add_string staticdnsv6
    proto_config_add_string iaapn
    proto_config_add_int iaiptype
    proto_config_add_int iaroamingtype
    proto_config_add_int rattype
    proto_config_add_string plmn
    proto_config_add_int vlan
    proto_config_add_int mode
    proto_config_add_int auto_conf
    proto_config_add_int retry
    proto_config_add_int ql_conf
    proto_config_add_int ql_monitor
    proto_config_add_int dataroaming
    proto_config_add_defaults
}

pdn_lock() {
    flock -n 1022 &> /dev/null
    if [ "$?" != "0" ]; then
        exec 1022>"/var/lock/ql_mipc.lock"
        echo "check lock"
        flock 1022
        if [ "$?" != "0" ]; then
            logger "warning: procd flock for $service_name failed"
        fi
    fi
}

check_auto_apn_prov() {
    echo "Run apn provision by SIM"

    apn_data=`(mipc_wan_cli --apn_provision_by_sim)`
    [ -z "$apn_data" ] && return
    json_load "$apn_data"
    json_get_var db_apn apn
    echo "apn is $db_apn"
    [ -z "$db_apn" ] && return

    json_get_var db_auth_type auth_type
    json_get_var db_user username
    json_get_var db_password password
    json_get_var db_protocol protocol
    json_get_var db_roaming_protocol roaming_protocol
    json_get_var db_iaapn iaapn
    json_get_var db_iaprotocol iaprotocol
    json_get_var db_iaroaming_protocol iaroaming_protocol
    json_get_var db_plmn plmn

    echo "Config:$1 & SIM:$db_plmn"
    [ "$db_plmn" = $1 ] && return

    uci set network.wan.proto='ql_mipc'
    uci set network.wan.device="ccmni"
    uci set network.wan6.device="ccmni"
    uci set network.wan.apn="$db_apn"
    uci set network.wan.auth="$db_auth_type"
    uci set network.wan.username="$db_user"
    uci set network.wan.password="$db_password"
    uci set network.wan.roamingtype="$db_roaming_protocol"
    uci set network.wan.iaroamingtype="$db_iaroaming_protocol"
    uci set network.wan.iptype="$db_protocol"
    uci set network.wan.iaapn="$db_iaapn"
    uci set network.wan.iaiptype="$db_iaprotocol"
    uci set network.wan.plmn="$db_plmn"
    uci set network.wan.dataroaming="$db_data_roaming"
    uci commit network
    ifup wan

    echo "APN provision done for $2"
    proto_notify_error "$2" APN_CHANGE
    exit 1
}

# get_device_idx_by_name device_name
get_device_idx_by_name() {
    local i=0

    [ -z "$1" ] && return 1

    echo "get_device_idx_by_name $1"

    while true; do
        result=$(uci -q get network.@device[$i].name)
        if [[ -z "$result" ]]; then
            echo "get_device_idx_by_name: No result"
            return 1
        fi

        if [[ $result == $1 ]]; then
            let device_id=$i
            echo "get_device_idx_by_name: device_id=$i"
            return 0
        fi

        let i++
    done
}

#fixup_br_ifname vlan mode
#Remove ccmni interface from bridge
fixup_br_ifname() {
    local section="lan"
    local eth_ifname="eth0"
    local device_name=""
    local result=1
    local vlan=$1
    local mode=$2

    echo "Check bridge ifname:vlan $1 mode $2"
    [ $1 -gt 0 ] && section="lan"$1
    echo "section=$section"

    device_name=$(uci -q get network.$section.device)
    echo "device_name: $device_name"

    get_device_idx_by_name $device_name
    result=$?
    [ $result -eq 1 ] && return 1

    echo "device_id=$device_id result=$result"

    ccmni_dev=$(uci get network.@device[$device_id].ports | grep -o 'ccmni[0-9]*')

    [ -z "$ccmni_dev" ] && return 1

    echo "fixup_br_ifname ports has ccmni: $ccmni_dev"

    # Reset bridge interface ifname
    uci del_list network.@device[$device_id].ports=$ccmni_dev
    uci commit network

    # Bringup bridge interface if routed mode. 
    if [ "$mode" -eq "1" ]
    then
        echo "fixup_br_ifname mode $mode"
        ifup "$section"
    fi
}

#setup_br_ports vlan wan_device
#setup member ports of bridge
setup_br_ifname() {

    local section="lan"
    local eth_ifname="eth0"
    local device_name=""
    local result=1
    local vlan=$1
    local wan_device=$2
    local cnt=1

    echo "Setup bridge ifname:vlan $1 wan_device $2"
    [ $1 -gt 0 ] && section="lan"$1
    echo "section=$section"

    device_name=$(uci -q get network.$section.device)
    [ -z "$device_name" ] && return 1
    echo "device_name: $device_name"

    get_device_idx_by_name $device_name
    result=$?
    [ $result -eq 1 ] && return 1

    echo "device_id=$device_id result=$result"

    # Retain the original device. If no device exists, use the default value
    uci get network.@device[$device_id].ports
    if [ "$?" != "0" ] ; then
        [ $vlan -gt 0 ] && eth_ifname="eth0."$vlan"0"
        uci add_list network.@device[$device_id].ports=$eth_ifname
    fi

    uci add_list network.@device[$device_id].ports=$wan_device

    uci commit network
}

#ql_update_mtu mtu vlan
#update to dhcp based on network mtu
ql_update_mtu() {
    local section="lan"
    local mtu="$1"
    local cnt=1
    local has_mtu=0

    [ $2 -gt 0 ] && section="lan"$2

    while true;
    do
        result=$(uci get dhcp."$section".dhcp_option | awk -F" " '{print $'$cnt'}')
        [ -z $result ] && break

        option=$(echo $result | awk -F, '{print $1}')
        if [ $option -eq 26 ] ; then
            value=$(echo $result | awk -F, '{print $2}')
            if [ $value -eq $mtu ] ; then
                has_mtu=1
                echo "mtu has no change!!!"
            else
                uci del_list dhcp."$section".dhcp_option="$result"
            fi

            break
        fi

        let cnt++
    done

    echo "ql_update_mtu $mtu $has_mtu"
    if [ $has_mtu -eq 0 ] ; then
        uci add_list dhcp."$section".dhcp_option="26,$mtu"
        uci commit dhcp
        /etc/init.d/dnsmasq reload

        # Reset all links
        if [ $2 -eq 0 ] ; then
            for i in `seq 0 1 4`
            do
                switch phy cl22 w $i 0 0x1840
                switch phy cl22 w $i 0 0x1040
            done

            ethtool -r eth1
        else
            ql_reset_nic.sh $section
        fi
    fi
}

#ql_reset_mtu vlan
#reset default mtu
ql_reset_mtu() {
    local section="lan"
    local cnt=1

    [ $1 -gt 0 ] && section="lan"$1

    while true;
    do
        result=$(uci get dhcp."$section".dhcp_option | awk -F" " '{print $'$cnt'}')
        [ -z $result ] && break

        option=$(echo $result | awk -F, '{print $1}')
        if [ $option -eq 26 ] ; then
            uci del_list dhcp."$section".dhcp_option="$result"
            uci commit dhcp
            /etc/init.d/dnsmasq reload
            break
        fi

        let cnt++
    done
}

#ql_data_monitor interface ifname
#monitor data transmission
ql_data_monitor() {
    local interface="$1"
    local ifname="$2"
    local cnt=0

    start_tx_packets=$(cat /sys/class/net/"$ifname"/statistics/tx_packets)
    start_rx_packets=$(cat /sys/class/net/"$ifname"/statistics/rx_packets)

    while true;
    do
        if [ $cnt -eq 60 ] ; then
            end_tx_packets=$(cat /sys/class/net/"$ifname"/statistics/tx_packets)
            end_rx_packets=$(cat /sys/class/net/"$ifname"/statistics/rx_packets)

            during_tx_packets=$(($end_tx_packets-$start_tx_packets))
            during_rx_packets=$(($end_rx_packets-$start_rx_packets))

            echo "$ifname during_tx_packets:$during_tx_packets  during_rx_packets:$during_rx_packets"

            if [ $during_tx_packets -gt 10 ] && [ $during_rx_packets -eq 0 ] ; then
                pdp_reset=$(uci -q get network."$interface".pdp_reset)
                [ -z "$pdp_reset" ] && pdp_reset=0

                reattach=$(uci -q get network."$interface".reattach)
                [ -z "$reattach" ] && reattach=0

                echo "The PDN is failure!!!!"
                if [ $pdp_reset -eq 0 ] ; then
                    echo "Start to reset PDP......"
                    uci set network."$interface".pdp_reset=1
                    uci commit network
                    ifup $interface
                elif [ $reattach -eq 0 ] ; then
                    echo "Start to PS re-attach......"
                    uci set network."$interface".reattach=1
                    uci commit network
                    mipc_wan_cli --at_cmd AT+EGREA=1
                    mipc_wan_cli --at_cmd AT+EGTYPE=0,1
                    mipc_wan_cli --at_cmd AT+EGREA=0
                    mipc_wan_cli --at_cmd AT+EGTYPE=4
                else
                    echo "Start to restart radio......"
                    uci set network."$interface".pdp_reset=0
                    uci set network."$interface".reattach=0
                    uci commit network
                    mipc_wan_cli --at_cmd AT+EFUN=0
                    mipc_wan_cli --at_cmd AT+EFUN=1
                fi
            else
                uci set network."$interface".pdp_reset=0
                uci set network."$interface".reattach=0
                uci commit network
                cnt=0
                start_tx_packets=$end_tx_packets
                start_rx_packets=$end_rx_packets
            fi
        fi

        sleep 1
        let cnt++
    done
}

# get_alias_idx_by_interface section
get_alias_idx_by_interface() {
    local i=0

    [ -z "$1" ] && return 1

    echo "get_alias_idx_by_interface $1"

    while true; do
        result=$(uci -q get network.@alias[$i].interface)
        if [[ -z "$result" ]]; then
            echo "get_alias_idx_by_interface: No result"
            return 1
        fi

        if [[ $result == $1 ]]; then
            let alias_id=$i
            echo "get_alias_idx_by_interface: alias_id=$i"
            return 0
        fi

        let i++
    done
}

# get_host_idx_by_name section
get_host_idx_by_name() {
    local i=0

    [ -z "$1" ] && return 1

    echo "get_host_idx_by_name $1"

    while true; do
        result=$(uci -q get dhcp.@host[$i].name)
        if [[ -z "$result" ]]; then
            echo "get_host_idx_by_name: No result"
            return 1
        fi

        if [[ $result == $1 ]]; then
            let host_id=$i
            echo "get_host_idx_by_name: host_id=$i"
            return 0
        fi

        let i++
    done
}

# ql_wait_for_device_access vlan v4_addr v4_dns1 v4_dns2 v6_dns1 v6_dns2
ql_wait_for_device_access() {
    local section="lan"
    local v4_addr="$2"
    local line_num
    local mac
    local exit_flag=0

    [ $1 -gt 0 ] && section="lan"$1
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

    uci add dhcp host
    uci set dhcp.@host[-1].ip="$v4_addr"
    uci set dhcp.@host[-1].mac="$mac"
    uci set dhcp.@host[-1].name="$section"
    uci commit dhcp

    /etc/init.d/dnsmasq reload
}

# This is a reference design by Mediatek
proto_ql_mipc_setup() {
    local interface="$1"
    local sim_state timeout retry imei

    sensitive_log=`uci get system.@system[0].enable_sensitive_log`

    echo "proto_ql_mipc_setup with $interface"

    # Meta mode handling
    bootmode=$(hexdump -v /proc/device-tree/chosen/atag,boot | awk '{print $6}')
    echo  "bootmode:${bootmode}" > /tmp/boot.txt
    if [ ${bootmode} = "0001" ] ;
    then
        proto_notify_error "$interface" META_MODE
        proto_block_restart "$interface"
        return 1
    fi

    # Modem exception handling with retry for 12 * 30 seconds
    modem_try=12
    while [ "$modem_try" -gt 0 ]
    do
        modem_status=$(cat /sys/kernel/ccci/boot | cut -c 5)
        if [ ${modem_status} -eq 5 ] ; then
            let modem_try--
            if [ "$modem_try" -eq 0 ] ; then
                proto_notify_error "$interface" MODEM_EXCEPTION
                proto_block_restart "$interface"
                return 1
            fi
            sleep 30
            echo "Modem exception retry: $modem_try times to left"
        else
            break
        fi
    done

    # Read from /etc/config/network
    json_get_vars apn auth mtu username password roamingtype iptype staticip staticgw staticmask staticdns staticipv6 staticgwv6 staticipv6prefix staticdnsv6 iaapn iaiptype iaroamingtype rattype plmn vlan mode auto_conf retry bearer_bitmask iabearer_bitmask dataroaming ql_conf ql_monitor
    [ -z "$timeout" ] && timeout="5"
    [ -z "$retry" ] && retry="2"
    [ -z "$vlan" ] && vlan="0"
    # The IP type is IPv4v6
    [ -z "$roamingtype" ] && roamingtype="3"
    [ -z "$iaroamingtype" ] && iaroamingtype=${roamingtype}
    [ -z "$iptype" ] && iptype="3"
    # NR + LTE + UMTS
    [ -z "$rattype" ] && rattype="0"
    # Mode: 1 => Routing 2 => Bridge
    [ -z "$mode" ] && mode="1"
    [ -z "$mtu" ] && mtu="1500"
    [ -z "$auto_conf" ] && auto_conf="1"
    [ -z "$plmn" ] && plmn=""
    [ -z "$iaapn" ] && iaapn=${apn}
    [ -z "$iaiptype" ] && iaiptype=${iptype}
    [ -z "$auth" ] && auth="0"
    [ -z "$bearer_bitmask" ] && bearer_bitmask="0xfffdffff"
    [ -z "$iabearer_bitmask" ] && iabearer_bitmask=${bearer_bitmask}
    [ -z "$dataroaming" ] && dataroaming="0"

    [ -z "$ql_conf" ] && ql_conf="0"
    [ -z "$ql_monitor" ] && ql_monitor="0"

    # Setup PDN in sequence
    if [ $vlan -gt 0 ] ; then
        echo "Wait for multiple PDN lock"
        pdn_lock
        auto_conf="0"
    fi

    # Check APN provision from SIM
    if [ "$auto_conf" = "1" ] ; then
        check_auto_apn_prov $plmn $interface
    fi

    # UI configuration issue, no APN
    if [ -z "$apn" ] ; then
        echo "APN is empty"
        apn="0"
        iaapn="0"
    fi

    if [ ${mode} -eq 2 ] || [ ${mode} -eq 3 ] ; then
        echo "Bridge mode is enabled"
        sysctl -w net.ipv6.conf.default.accept_ra=0
        echo 1 > /proc/ccmni/bridge_mode_control
    fi

    # Check CFUN State
    cfun_retry=5
    while [ "$cfun_retry" -gt 0 ]
    do
        cfun_state=$(mipc_wan_cli --nw_radio_state_get 0)

        if [ "$cfun_state" != "Get SW radio state:MIPC_NW_RADIO_STATE_ON" ] ; then
            echo "radio state get failed or state is off"
            let cfun_retry--
            if [ "$cfun_retry" -eq 0 ] ; then
                proto_notify_error "$interface" CFUN_IS_OFF
                proto_block_restart "$interface"
                return 1
            fi
            sleep 3
        else
            break
        fi
    done

    echo "Radio on ia:[${iaapn} ${rattype} ${iaiptype} ${iaroamingtype} ${dataroaming}]"
    mipc_wan_cli --check_ia ${iaapn} ${rattype} ${iaiptype} ${iaroamingtype} "${username}" "${password}" ${auth} ${iabearer_bitmask} ${dataroaming}

    imei=$(cat /tmp/imei) >/dev/null 2>&1
    [ -z "$imei" ] && imei=$(mipc_wan_cli --get_imei)
    if [ -z "$imei" ] ; then
        echo "no imei & stop to setup data call"
        proto_notify_error "$interface" NO_IMEI
        proto_block_restart "$interface"
        return 1
    fi
    echo "$imei" > /tmp/imei

    net_srv=$(ubus call service list | grep mtk_netagent)
    if [ -z "$net_srv" ] ; then
       echo "Wait for mtk_netagnet service ready"
       sleep 1
       return 1
    fi

    check_sim_count=0
    sim_status=$(mipc_wan_cli --show_sim_status)
    while [ "$sim_status" != "MIPC_SIM_STATUS_COMPLETE_READY" ]
    do
        if [ ${sim_status} == "MIPC_SIM_STATUS_SIM_PIN" ]; then
            echo "SIM PIN is enabled"
            proto_notify_error "$interface" SIM_PIN_LOCKED
            proto_block_restart "$interface"
            return 1
        elif [ ${sim_status} == "MIPC_SIM_STATUS_SIM_PUK" ]; then
            echo "SIM PUK is enabled"
            proto_notify_error "$interface" SIM_PUK_LOCKED
            proto_block_restart "$interface"
            return 1
        elif [ ${sim_status} == "MIPC_SIM_STATUS_NOT_INSERT" ]; then
            echo "SIM is not inserted"
        fi

        if [ "$check_sim_count" -lt "$timeout" ]; then
            let check_sim_count++
            echo "$sim_status with $check_sim_count"
            sleep 2;
        else
            echo "SIM not initialized"
            proto_notify_error "$interface" SIM_NOT_INITIALIZED
            proto_block_restart "$interface"
            return 1
        fi
        sim_status=$(mipc_wan_cli --show_sim_status)
    done

    # Wait network registartion status
    echo "Check Network status"
    nw_status=$(mipc_wan_cli --check_nw_status)
    echo -n "modem camp on" > /proc/bootprof

    if [ $vlan -eq 0 ] ; then
        # Deactivate data call firstly by APN
        mipc_wan_cli --data_call_deact_apn "$apn"
    fi

    echo "starting connection with apn '${apn}' in '${nw_status}' mtu:${mtu}"
    json_init
    json_add_string "apn" "$apn"
    json_add_int    "ip_type" "$iptype"
    json_add_int    "roaming_type" "$roamingtype"
    json_add_int    "auth_type" "$auth"
    json_add_string "username" "$username"
    json_add_string "password" "$password"
    json_add_string "bearer_bitmask" "$bearer_bitmask"
    json_add_int "mtu" "$mtu"
    json_add_int "mode" "$mode"
    cmd_args=`json_dump`

    while [ "$retry" -gt 0 ]
    do
        data_call_result=`(mipc_wan_cli --data_call_act "${cmd_args}")`
        json_load "$data_call_result"
        json_get_var result result
        echo "data call result:${result}"
        if [ $result -eq 0 ] ; then
            echo "Connect OK"
            break;
        else
            sleep 3
            mipc_wan_cli --check_ia ${iaapn} ${rattype} ${iaiptype} ${iaroamingtype} "${username}" "${password}" ${auth} ${iabearer_bitmask} ${dataromaing}
            nw_status=$(mipc_wan_cli --check_nw_status)

            let retry--
            if [ "$retry" -eq 0 ] ; then
                echo "Unable to connect"
                proto_notify_error "$interface" CONNECT_FAILED_RETRY_LATER
                proto_block_restart "$interface"
                return 1
            fi
            echo "Data call retry: $retry times to left"
        fi
    done

    json_get_var cid cid
    json_get_var ifname ifname
    json_get_var v4_addr v4_addr
    json_get_var v4_mask v4_mask
    json_get_var v4_gw   v4_gw
    json_get_var odu_addr   odu_addr
    json_get_var odu_dhcp_offset   odu_dhcp_offset
    json_get_var v4_dns1 v4_dns1
    json_get_var v4_dns2 v4_dns2
    json_get_var v4_mtu v4_mtu

    json_get_var v6_addr v6_addr
    json_get_var v6_prefix v6_prefix
    json_get_var v6_gw v6_gw
    json_get_var v6_dns1 v6_dns1
    json_get_var v6_dns2 v6_dns2
    json_get_var v6_mtu v6_mtu
    json_get_var clatd clatd

    # Check ifname is up before notify OpenWRT
    # Because of PDN may be activated & deactivated soon by modem during IPv6 global address checking
    if_check=$(ip link show $ifname up)
    if [ -z "$if_check" ] ; then
        echo "$ifname is down & re-connect"
        echo `ip link show $ifname`
        return 1
    fi

    # Correct bridge ifname if it has ccmni interface
    fixup_br_ifname $vlan $mode

    # ULA workaround: set dhcp.lan.ra_default=1 to force annoucing default route
    if [ ${v6_addr:0:2} == "fc" ] || [ ${v6_addr:0:2} == "fd" ] ; then
        uci set dhcp.lan.ra_default=1
        uci commit dhcp
    fi

    if [ ${mode} -eq "2" ] ; then
        proto_init_update "$ifname" 1 1
        proto_set_keep 1

        proto_add_data
        json_add_string "cid" "$cid"
        json_add_string "mode" "$mode"
        json_add_string "clatd" "$clatd"
        json_add_string "vlan" "$vlan"
        proto_close_data
        proto_send_update "$interface"

        ql_mipc_setup_bridge "$ifname" "$v4_addr" "$v4_mask" "$v4_gw" "$v4_dns1" "$v4_dns2" "$v4_mtu" "$v6_addr" "$v6_dns1" "$v6_dns2" "$odu_addr" "$odu_dhcp_offset" "$vlan"
    elif [ ${mode} -eq "3" ] ; then
        proto_init_update "$ifname" 1 1
        proto_set_keep 1

        if [ -n "$v4_dns1" ] ; then
            proto_add_dns_server "${v4_dns1}"
        fi
        if [ -n "$v4_dns2" ] ; then
            proto_add_dns_server "${v4_dns2}"
        fi

        if [ -n "$v6_dns1" ] ; then
            proto_add_dns_server "${v6_dns1}"
        fi
        if [ -n "$v6_dns2" ] ; then
            proto_add_dns_server "${v6_dns2}"
        fi

        proto_add_data
        json_add_string "cid" "$cid"
        json_add_string "mode" "$mode"
        json_add_string "clatd" "$clatd"
        json_add_string "vlan" "$vlan"
        proto_close_data
        proto_send_update "$interface"

        ql_mipc_setup_bridge_with_nat "$ifname" "$v4_addr" "$v4_mask" "$v4_gw" "$v4_dns1" "$v4_dns2" "$v4_mtu" "$v6_addr" "$v6_dns1" "$v6_dns2" "$odu_addr" "$odu_dhcp_offset" "$vlan"
    else
        echo "Setting up $ifname & update connection info."
        proto_init_update "$ifname" 1
        proto_set_keep 1

        if [ -n "$staticip" ] && [ -n "$staticmask" ] && [ -n "$staticgw" ]; then
            proto_add_ipv4_address "${staticip}" "${staticmask}"
            [ $sensitive_log == '1' ] && echo "adding default IPv4 route via ${staticgw}"
            if [ ${mode} -ne 2 ] ; then
                proto_add_ipv4_route "0.0.0.0" "0" "${staticgw}" "${staticip}"
            fi
            if [ -n "$staticdns" ] ; then
                proto_add_dns_server "${staticdns}"
            fi
        else
            if [ -n "$v4_addr" ] ; then
                proto_add_ipv4_address "${v4_addr}" "${v4_mask}"
            fi
            if [ -n "$v4_gw" ] ; then
                [ $sensitive_log == '1' ] && echo "adding default IPv4 route via ${v4_gw}"
                if [ ${mode} -ne 2 ] ; then
                    proto_add_ipv4_route "0.0.0.0" "0" "${v4_gw}" "${v4_addr}"
                fi
            fi
            if [ -n "$v4_dns1" ] ; then
                proto_add_dns_server "${v4_dns1}"
            fi
            if [ -n "$v4_dns2" ] ; then
                proto_add_dns_server "${v4_dns2}"
            fi
        fi
        if [ -n "$staticipv6" ] && [ -n "$staticipv6prefix" ] && [ -n "$staticgwv6" ]; then
            proto_add_ipv6_address "${staticipv6}" "128"
            proto_add_ipv6_prefix "${staticipv6:0:20}:/${staticipv6prefix}"
            proto_add_ipv6_route "::0" 0 "$staticgwv6"
            if [ -n "$staticdnsv6" ] ; then
                proto_add_dns_server "${staticdnsv6}"
            fi
        else
            if [ -n "$v6_addr" ] ; then
                proto_add_ipv6_address "${v6_addr}" "128"
            fi
            if [ -n "$v6_prefix" ] ; then
                proto_add_ipv6_prefix "${v6_addr:0:20}:/${v6_prefix}"
            fi
            if [ -n "$v6_gw" ] ; then
                proto_add_ipv6_route "::0" 0 "$v6_gw"
            fi
            if [ -n "$v6_dns1" ] ; then
                proto_add_dns_server "${v6_dns1}"
            fi
            if [ -n "$v6_dns2" ] ; then
                proto_add_dns_server "${v6_dns2}"
            fi
        fi

        # Enable MTU clamping for PDN for both directions
        uci set firewall.@zone[0].mtu_fix='1'
        uci set firewall.@zone[1].mtu_fix='1'
        uci commit firewall

        if [ -n "$clatd" ] ; then
            clad_if=`uci get network.clatd`
            if [ -z $clad_if ] ; then
                echo "Setup clatd interface"
                uci set network.clatd=interface
                uci set network.clatd.proto='464xlat'
                # Disable ip6prefix setting & perform DNS query with ipv4only.arpa
                # to confirm operator supports clatd function
                # uci set network.clatd.ip6prefix='64:ff9b::/96'
                uci set network.clatd.tunlink='wan'
                uci commit network

                uci set firewall.@zone[1].network='wan wan6 clatd'
                uci set firewall.@zone[1].forward='ACCEPT'
                uci commit firewall
            fi
            ifup clatd
        fi

        proto_add_data
        json_add_string "cid" "$cid"
        json_add_string "mode" "$mode"
        json_add_string "clatd" "$clatd"
        json_add_string "vlan" "$vlan"
        proto_close_data
        proto_send_update "$interface"

        ql_update_mtu "$v4_mtu" "$vlan"

        if [ $vlan -gt 0 ] && [ $ql_conf -eq 1 ] ; then
            if [ -n "$v4_addr" ] ; then
                # set route
                ip route add default dev $ifname table $vlan
                ipaddr=$(uci -q get network."lan$vlan".ipaddr)
                ipaddr=${ipaddr%.*}.0/24
                ip route add $ipaddr dev br-lan$vlan table $vlan

                # set rule
                iptables -D PREROUTING -t mangle -i br-lan$vlan -j MARK --set-mark 30$vlan -w
                iptables -I PREROUTING -t mangle -i br-lan$vlan -j MARK --set-mark 30$vlan -w
                ip rule del fwmark 30$vlan table $vlan
                ip rule add fwmark 30$vlan table $vlan
            fi

            if [ -n "$v6_addr" ] ; then
                ip6addr=${v6_addr:0:20}:1/64
                ip -6 addr add $ip6addr dev br-lan$vlan

                # set route
                ip -6 route add default dev $ifname table $vlan
                ip -6 route add $ip6addr dev br-lan$vlan table $vlan

                # set rule
                ip6tables -D PREROUTING -t mangle -i br-lan$vlan -j MARK --set-mark 30$vlan -w
                ip6tables -I PREROUTING -t mangle -i br-lan$vlan -j MARK --set-mark 30$vlan -w
                ip -6 rule del fwmark 30$vlan table $vlan
                ip -6 rule add fwmark 30$vlan table $vlan
            fi
        fi
    fi

    # Work-around of ntpclient hotplug hanging issue: kill hanging ntpclient hotplug.
    # Note that ntpclient config may specify multiple servers, may need to kill multiple times.
    kill_try=`uci show ntpclient | grep "ntpclient.@ntpserver\[.*\]=ntpserver" | wc | awk '{print $1}'`
    re='^[0-9]+$'
    if ! [[ $kill_try =~ $re ]] ; then
        # not a number, do not proceed
        return 0
    fi

    while [ "$kill_try" -gt 0 ]
    do
       ntpclient_hotplug=`ps -w | grep "/usr/sbin/ntpclient -c 1 -p .* -i 2" | grep -v "grep" | awk '{print $1}'`
       if [ -z "$ntpclient_hotplug" ] ; then
          break;
       else
          echo "Found hanging ntpclient hotplug, process id $ntpclient_hotplug, kill the process ..."
          kill -9 $ntpclient_hotplug
          let kill_try--
          sleep 1
       fi
    done

    if [ $ql_monitor -eq 1 ] ; then
        [ $vlan -gt 0 ] && flock -u 1022
        ql_data_monitor "$interface" "$ifname"
    fi

    return 0
}

proto_ql_mipc_teardown() {

    echo "[MIPC] Stopping network $interface"

    json_load "$(ubus call network.interface.$interface status)"
    json_select data
    json_get_vars mode
    json_get_vars cid
    json_get_vars clatd
    json_get_vars vlan

    # Stop PDN in sequence
    if [ $vlan -gt 0 ] ; then
        echo "Wait for multiple PDN lock"
        pdn_lock
    fi

    [ -z "$mode" ] && mode="$(uci -q get network.$interface.mode)"
    # Mode: 1 => Routing 2 => Bridge
    [ -z "$mode" ] && mode="1"
    if [ ${mode} -eq 1 ] ; then
        ql_reset_mtu "$vlan"
    fi

    if [ -n "$cid" ] ; then
        echo "Deactivate data call with cid=""$cid"
        mipc_wan_cli --data_call_deact "${cid}"

        # Avoid pending command for data_call_deact
        pid=$(ps | grep "mipc_wan_cli --data_call_deact ${cid}" | awk '{ print $1}' | head -1)
        kill -9 $pid
    fi

    # Handle AT+ERAT command
    # rat=`(mipc_wan_cli --nw_get_rat) | head -n 1 | tr -dc '0-9'`
    # rattype="$(uci -q get network.$interface.rattype)"
    # if [ "$rat" -gt 0 ] 2>/dev/null && [ "$rat" != "$rattype" ]; then
    #     echo "Commit new RAT type:$rat"
    #     uci set network."$interface".rattype="$rat"
    #     uci commit network
    #     ifup "$interface"
    # fi

    proto_init_update "*" 0
    proto_send_update "$interface"

    # Handle AT+EFUN=0 command
    radio_status=`(mipc_wan_cli --nw_radio_state_get)`
    if [[ "$radio_status" == "*MIPC_NW_RADIO_STATE_OFF*" ]]; then
        proto_notify_error "$interface" RADIO_OFF
        proto_block_restart "$interface"
    else
        proto_notify_error "$interface" STOPPED
    fi

    if [ ${mode} -eq 2 ] ; then
        ql_mipc_reset_bridge "$vlan"
    elif [ ${mode} -eq 3 ] ; then
        ql_mipc_reset_bridge_with_nat "$vlan"
    fi
}

ql_mipc_reset_bridge() {
    local section="lan"
    local eth_ifname="eth0"

    # For single PDN case
    #[ -z "$1" ] && vlan="0"

    [ $vlan -gt 0 ] && section="lan"$vlan
    [ $vlan -gt 0 ] && eth_ifname="eth0."$vlan"0"

    echo "Clear bridge $eth_ifname for $section $1 $vlan"

    sysctl -w net.ipv6.conf.default.router_solicitations=-1
    sysctl -w net.ipv6.conf.default.accept_ra=0

    device_name=$(uci -q get network.$section.device)
    echo "device_name: $device_name"

    get_device_idx_by_name $device_name
    result=$?
    if [ $result -eq 0 ] ; then
        ccmni_dev=$(uci get network.@device[$device_id].ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.@device[$device_id].ports=$ccmni_dev
    fi

    if [ $vlan -eq 0 ] ; then
        uci set network."$section".ipaddr="192.168.1.1"
    else
        uci set network."$section".ipaddr="192.168."$vlan".1"
    fi

    uci set network."$section".netmask="255.255.255.0"
    uci set network."$section".ip6assign=60
    uci -q delete network."$section".gateway
    uci -q delete network."$section".dns
    uci -q delete network."$section".ip6addr
    uci commit network

    # Wait for br-lan ready to use
    uci set dhcp."$section".start=150
    uci set dhcp."$section".limit=100
    uci -q delete dhcp."$section".dhcp_option
    uci -q delete dhcp."$section".dns
    uci commit dhcp

    ql_reset_nic.sh $section

    ifup "$section"

    echo "clean bridge is done"
}

ql_mipc_setup_bridge() {
    local ifname="$1"
    local v4_addr="$2"
    local v4_prefix="$3"
    local v4_gw="$4"
    local v4_dns1="$5"
    local v4_dns2="$6"
    local v4_mtu="$7"
    local v6_addr="$8"
    local v6_dns1="$9"
    local v6_dns2="${10}"
    local odu_addr="${11}"
    local odu_dhcp_offset="${12}"
    local vlan="${13}"
    local section="lan"
    local vid

    [ ${13} -gt 0 ] && section="lan"${13}
    [ ${13} -gt 0 ] && vid=""${13}"0"

    echo "Update bridge with vid $vid for $section"

    macaddr="$(cat /sys/class/net/$ifname/address)" 2>/dev/null
    uci set network."$section".macaddr="$macaddr"
    uci -q delete network."$section".ip6assign
    setup_br_ifname $vlan $ifname

    uci -q delete dhcp."$section".dhcp_option
    uci -q delete dhcp."$section".dns
    uci set dhcp."$section".leasetime="1m"

    if [ "$v4_addr" != "" ] ; then
        local v4_netmask
        eval "$(ipcalc.sh "$v4_addr/$v4_prefix")";v4_netmask=$NETMASK

        if [ $odu_dhcp_offset -eq 0 ] ; then
            echo "pdn ip is x.x.x.0"
            odu_dhcp_offset=256
        elif [ $odu_dhcp_offset -eq 255 ] ; then
            echo "pdn ip is x.x.x.255"
            v4_netmask="255.255.0.0"
            segment=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=segment*256+255
        else
            echo "No special treatment is required"
        fi

        echo "new ip:$odu_addr"
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
    uci commit network

    uci commit dhcp

    ql_reset_nic.sh $section

    ip addr flush dev $ifname

    ifup $section

    brctl addif br-$section $ifname
}

ql_mipc_reset_bridge_with_nat() {
    local section="lan"
    local eth_ifname="eth0"
    local v4_addr
    local vlan="$1"

    # For single PDN case
    [ -z "$vlan" ] && vlan="0"

    [ $vlan -gt 0 ] && section="lan"$vlan
    #[ $vlan -gt 0 ] && eth_ifname="eth0."$vlan"0"

    v4_addr=$(uci -q get dhcp.$section.public_ip)

    echo "Clear bridge $eth_ifname for $section"

    sysctl -w net.ipv6.conf.default.router_solicitations=-1
    sysctl -w net.ipv6.conf.default.accept_ra=0

    device_name=$(uci -q get network.$section.device)
    echo "device_name: $device_name"

    get_device_idx_by_name $device_name
    result=$?
    if [ $result -eq 0 ] ; then
        ccmni_dev=$(uci get network.@device[$device_id].ports | grep -o 'ccmni[0-9]*')
        [ -n "$ccmni_dev" ] && uci del_list network.@device[$device_id].ports=$ccmni_dev
    fi

    if [ $vlan -eq 0 ] ; then
        uci set network."$section".ipaddr="192.168.1.1"
        ebtables -t nat -D POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=192.168.1.1/24 --arp-opcode Reply -j DROP
    else
        uci set network."$section".ipaddr="192.168."$vlan".1"
        ebtables -t nat -D POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=192.168."$vlan".1/24 --arp-opcode Reply -j DROP
    fi

    uci set network."$section".netmask="255.255.255.0"
    uci set network."$section".ip6assign=60
    uci -q delete network."$section".macaddr

    get_alias_idx_by_interface "$section"
    [ $? -eq 0 ] && uci del network.@alias[$alias_id]

    uci commit network

    # Wait for br-lan ready to use
    uci set dhcp."$section".start=150
    uci set dhcp."$section".limit=100
    uci set dhcp."$section".leasetime=12h
    uci -q delete dhcp."$section".dhcp_option
    uci -q delete dhcp."$section".dns
    uci -q delete dhcp."$section".public_ip

    get_host_idx_by_name "$section"
    [ $? -eq 0 ] && uci del dhcp.@host[$host_id]

    uci commit dhcp

    iptables -t nat -D POSTROUTING -o $ccmni_dev -j SNAT --to $v4_addr

    ql_reset_nic.sh $section
    ifup "$section"

    echo "clean bridge is done"
}

ql_mipc_setup_bridge_with_nat() {
    local ifname="$1"
    local v4_addr="$2"
    local v4_prefix="$3"
    local v4_gw="$4"
    local v4_dns1="$5"
    local v4_dns2="$6"
    local v4_mtu="$7"
    local v6_addr="$8"
    local v6_dns1="$9"
    local v6_dns2="${10}"
    local odu_addr="${11}"
    local odu_dhcp_offset="${12}"
    local vlan="${13}"
    local section="lan"
    local vid
    local lan_addr
    local lan_v6addr

    [ ${13} -gt 0 ] && section="lan"${13}
    [ ${13} -gt 0 ] && vid=""${13}"0"

    if [ $vlan -eq 0 ] ; then
        lan_addr="192.168.1.1"
    else
        lan_addr="192.168."$vlan".1"
    fi

    echo "IPPT with nat, Update v4 bridge with $vid for $section"

    macaddr="$(cat /sys/class/net/$ifname/address)" 2>/dev/null
    uci set network."$section".macaddr="$macaddr"
    uci -q delete network."$section".ip6assign
    setup_br_ifname $vlan $ifname

    uci -q delete dhcp."$section".dhcp_option
    uci -q delete dhcp."$section".dns

    uci add network alias
    uci set network.@alias[-1].interface="$section"
    uci set network.@alias[-1].proto="static"

    if [ "$v4_addr" != "" ] ; then
        local v4_netmask
        eval "$(ipcalc.sh "$v4_addr/$v4_prefix")";v4_netmask=$NETMASK

        if [ $odu_dhcp_offset -eq 0 ] ; then
            echo "pdn ip is x.x.x.0"
            odu_dhcp_offset=256
        elif [ $odu_dhcp_offset -eq 255 ] ; then
            echo "pdn ip is x.x.x.255"
            v4_netmask="255.255.0.0"
            segment=$(echo "$v4_addr" | awk -F. '{print $3}')
            let odu_dhcp_offset=segment*256+255
        else
            echo "No special treatment is required"
        fi

        echo "add new ip:$odu_addr"
        uci set network.@alias[-1].ipaddr="$odu_addr"
        uci set network.@alias[-1].netmask="$v4_netmask"

        uci add_list dhcp."$section".dhcp_option="1,255.255.255.0"
        uci add_list dhcp."$section".dhcp_option="3,$lan_addr"
        uci add_list dhcp."$section".dhcp_option="6,$v4_dns1"
        uci add_list dhcp."$section".dhcp_option="26,$v4_mtu"
        uci set dhcp."$section".public_ip="$v4_addr"
    fi

    if [ "$v6_addr" != "" ] ; then
        ipv6_prefix1=$(echo "$v6_addr" | awk -F: '{print $1}')
        ipv6_prefix2=$(echo "$v6_addr" | awk -F: '{print $2}')
        ipv6_prefix3=$(echo "$v6_addr" | awk -F: '{print $3}')
        ipv6_prefix4=$(echo "$v6_addr" | awk -F: '{print $4}')
        lan_v6addr="$ipv6_prefix1:$ipv6_prefix2:$ipv6_prefix3:$ipv6_prefix4::1/64"
        uci set network.@alias[-1].ip6addr="$lan_v6addr"

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
    uci commit network
    uci commit dhcp

    ebtables -t nat -D POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_addr/24 --arp-opcode Reply -j DROP >/dev/null 2>&1
    ebtables -t nat -I POSTROUTING -p arp --src 06:16:26:36:46:56 --arp-ip-src=$lan_addr/24 --arp-opcode Reply -j DROP

    ql_reset_nic.sh $section

    ifup $section

    brctl addif br-$section $ifname

    if [ "$v4_addr" != "" ] ; then
        if [ $vlan -eq 0 ] ; then
            ifconfig $ifname 169.254.2.1
            ip route add default dev $ifname
            ip route add $v4_addr dev br-$section
        else
            ifconfig $ifname 169.254.2.$vlan
            ip route add default dev $ifname table $vlan
            ip route add $v4_addr dev br-$section table $vlan

            # set rule
            iptables -D PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan -w >/dev/null 2>&1
            iptables -I PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan -w
            ip rule del fwmark 30$vlan table $vlan >/dev/null 2>&1
            ip rule add fwmark 30$vlan table $vlan

            if [ $vlan -eq 1 ] ; then
                ip rule del oif lo table $vlan >/dev/null 2>&1
                ip rule del iif lo table $vlan >/dev/null 2>&1
                ip rule add oif lo table $vlan
                ip rule add iif lo table $vlan
            fi
        fi

        iptables -t nat -D POSTROUTING -o $ifname -j SNAT --to $v4_addr >/dev/null 2>&1
        iptables -t nat -I POSTROUTING -o $ifname -j SNAT --to $v4_addr
    fi

    if [ "$v6_addr" != "" ] ; then
        ip -6 addr del $v6_addr/64 dev $ifname
        ip -6 addr add $v6_addr/128 dev $ifname
        if [ $vlan -gt 0 ] ; then
            ip -6 route add default dev $ifname table $vlan
            ip -6 route add $lan_v6addr dev br-$section table $vlan

            # set rule
            ip6tables -D PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan -w >/dev/null 2>&1
            ip6tables -I PREROUTING -t mangle -i br-$section -j MARK --set-mark 30$vlan -w
            ip -6 rule del fwmark 30$vlan table $vlan >/dev/null 2>&1
            ip -6 rule add fwmark 30$vlan table $vlan

            if [ $vlan -eq 1 ] ; then
                ip -6 rule del oif lo table $vlan >/dev/null 2>&1
                ip -6 rule del iif lo table $vlan >/dev/null 2>&1
                ip -6 rule add oif lo table $vlan
                ip -6 rule add iif lo table $vlan
            fi
        fi
    fi

    ifconfig br-$section promisc

    flock -u 1022
    ql_wait_for_device_access "$vlan" "$v4_addr" "$v4_dns1" "$v4_dns2" "$v6_dns1" "$v6_dns2"
}

[ -n "$INCLUDE_ONLY" ] || {
    add_protocol ql_mipc
}
