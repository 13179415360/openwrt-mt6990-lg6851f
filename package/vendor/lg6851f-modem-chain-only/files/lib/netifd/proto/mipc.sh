#!/bin/sh

[ -n "$INCLUDE_ONLY" ] || {
. /lib/functions.sh
. /lib/functions/network.sh
. ../netifd-proto.sh
. /usr/share/libubox/jshn.sh
init_proto "$@"
}

proto_mipc_init_config() {
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
    proto_config_add_int dataroaming
    proto_config_add_defaults
}

pdn_lock() {
    flock -n 1022 &> /dev/null
    if [ "$?" != "0" ]; then
        exec 1022>"/var/lock/mipc_apn_setup.lock"
        echo "check lock"
        flock 1022
        if [ "$?" != "0" ]; then
            logger "warning: procd flock for $service_name failed"
        fi
    fi
}

check_auto_apn_prov() {
    echo "Run apn provision by SIM"

    mipc_wan_cli --nw_radio_state_set 1
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

    uci set network.wan.proto='mipc'
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

    [ $vlan -gt 0 ] && eth_ifname="eth0."$vlan"0"
    uci delete network.@device[$device_id].ports
    uci add_list network.@device[$device_id].ports=$wan_device
    uci add_list network.@device[$device_id].ports=$eth_ifname
    uci commit network
}

# This is a reference design by Mediatek
proto_mipc_setup() {
    local interface="$1"
    local sim_state timeout retry imei

    sensitive_log=`uci get system.@system[0].enable_sensitive_log`

    echo "proto_mipc_setup with $interface"

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
    json_get_vars apn auth mtu username password roamingtype iptype staticip staticgw staticmask staticdns staticipv6 staticgwv6 staticipv6prefix staticdnsv6 iaapn iaiptype iaroamingtype rattype plmn vlan mode auto_conf retry bearer_bitmask iabearer_bitmask dataroaming
    [ -z "$timeout" ] && timeout="5"
    [ -z "$retry" ] && retry="5"
    [ -z "$vlan" ] && vlan="0"
    # The IP type is IPv4v6
    [ -z "$roamingtype" ] && roamingtype="3"
    [ -z "$iaroamingtype" ] && iaroamingtype=${roamingtype}
    [ -z "$iptype" ] && iptype="3"
    # NR + LTE + UMTS
    [ -z "$rattype" ] && rattype="21"
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
        proto_notify_error "$interface" NO_APN
        proto_block_restart "$interface"
        return 1
    fi

    if [ ${mode} -eq 2 ] ; then
        echo "Bridge mode is enabled"
        sysctl -w net.ipv6.conf.default.accept_ra=0
    fi

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
            proto_notify_error "$interface" SIM_NOT_INSERTED
            proto_block_restart "$interface"
            return 1
        elif [ "$check_sim_count" -lt "$timeout" ]; then
            let check_sim_count++
            echo "$sim_status with $check_sim_count"
            sleep 5;
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
    # Write message to /proc/bootprof for boot time calcurating
    [ -f /proc/bootprof ] && echo -n "modem camp on" > /proc/bootprof

    # Deactivate data call firstly by APN
    mipc_wan_cli --data_call_deact_apn "$apn"

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
    if [ -n "$v6_addr" ] ; then
        if [ ${v6_addr:0:2} == "fc" ] || [ ${v6_addr:0:2} == "fd" ] ; then
            uci set dhcp.lan.ra_default=1
            uci commit dhcp
        fi
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

        mipc_setup_bridge "$ifname" "$v4_addr" "$v4_mask" "$v4_gw" "$v4_dns1" "$v4_dns2" "v4_mtu" "$v6_addr" "$v6_dns1" "$v6_dns2" "$odu_addr" "$odu_dhcp_offset" "$vlan"
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

    return 0
}

proto_mipc_teardown() {

    echo "[MIPC] Stopping network $interface"

    # Remove all pending commands firstly
    killall mipc_wan_cli >/dev/null 2>&1

    json_load "$(ubus call network.interface.$interface status)"
    json_select data
    json_get_vars mode
    json_get_vars cid
    json_get_vars clatd
    json_get_vars vlan

    if [ -n "$cid" ] ; then
        echo "Deactivate data call with cid=""$cid"
        mipc_wan_cli --data_call_deact "${cid}"
    fi

    # Avoid pending command for data_call_deact
    killall mipc_wan_cli >/dev/null 2>&1

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

    [ -z "$mode"] && mode="$(uci -q get network.$interface.mode)"
    # Mode: 1 => Routing 2 => Bridge
    [ -z "$mode" ] && mode="1"
    if [ ${mode} -eq 2 ] ; then
        mipc_reset_bridge "$vlan"
    fi
}

mipc_reset_bridge() {
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

        [ $vlan -gt 0 ] && eth_ifname="eth0."$vlan"0"
        uci delete network.@device[$device_id].ports
        uci add_list network.@device[$device_id].ports=$eth_ifname
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

    ifup "$section"
    /etc/init.d/dnsmasq restart

    echo "clean bridge is done"
}

mipc_setup_bridge() {
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
        # uci add_list dhcp."$section".dhcp_option="26,$v4_mtu"
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

    ifup $section
    /etc/init.d/dnsmasq restart
}

[ -n "$INCLUDE_ONLY" ] || {
    add_protocol mipc
}
