/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#ifndef	__HW_NAT_IOCTL_H__
#define	__HW_NAT_IOCTL_H__

#include "hnat_config.h"

extern unsigned int hnat_chip_name;
extern unsigned int fe_feature;
extern struct net_device	*dst_port[64];
extern unsigned int debug_PPP;

#define HW_NAT_DUMP_CACHE_ENTRY         (0x24)
#define HW_NAT_ADD_ENTRY		(0x01)
#define HW_NAT_DEL_ENTRY		(0x38)
#define HW_NAT_DUMP_ENTRY		(0x03)
#define HW_NAT_GET_ALL_ENTRIES		(0x04)
#define HW_NAT_BIND_ENTRY		(0x05)
#define HW_NAT_UNBIND_ENTRY		(0x06)
#define HW_NAT_INVALID_ENTRY		(0x07)
#define HW_NAT_DEBUG			(0x08)

#define HW_NAT_DROP_ENTRY		(0x36)
#define HW_NAT_TBL_CLEAR		(0x37)

#define HW_NAT_GET_AC_CNT		(0x09)
#define HW_NAT_BIND_THRESHOLD		(0x16)
#define HW_NAT_MAX_ENTRY_LMT		(0x17)
#define HW_NAT_RULE_SIZE		(0x18)
#define HW_NAT_KA_INTERVAL		(0x19)
#define HW_NAT_UB_LIFETIME		(0x1A)
#define HW_NAT_BIND_LIFETIME		(0x1B)
#define HW_NAT_BIND_DIRECTION		(0x1C)
#define HW_NAT_VLAN_ID			(0x1D)
#define HW_NAT_MCAST_INS		(0x20)
#define HW_NAT_MCAST_DEL		(0x21)
#define HW_NAT_MCAST_DUMP		(0x22)
#define HW_NAT_MIB_DUMP			(0x23)
#define HW_NAT_MIB_DRAM_DUMP            (0x25)
#define HW_NAT_MIB_GET			(0x26)
#define HW_NAT_MIB_GET_ALL_IP	(0x27)
#define HW_NAT_IPI_CTRL_FROM_EXTIF			(0x50)
#define HW_NAT_IPI_CTRL_FROM_PPEHIT			(0x51)
#define HW_NAT_DPORT			(0x52)
#define HW_NAT_CLEAR_HOOK  		(0x53)
#define HW_NAT_RESTORE_HOOK		(0x54)


#define HW_NAT_DEVNAME			"hwnat0"
#define HW_NAT_MAJOR			(121)

extern struct mib_entry *ppe_mib_base;
enum hwnat_status {
	HWNAT_SUCCESS = 0,
	HWNAT_FAIL = 1,
	HWNAT_ENTRY_NOT_FOUND = 2
};

struct hwnat_tuple {
	unsigned short ppe_index;
	unsigned short hash_index;
	unsigned short rmt;
	unsigned short frag;
	unsigned short checksum;
	unsigned short ttl;
	unsigned short rxif_idx;
	unsigned short act_mode;
	unsigned short act_dp;
	unsigned int pkt_type;
	unsigned int set_idx;
	unsigned int   is_udp;
	unsigned int fport;
	unsigned int fqos;
	unsigned int qid;
	unsigned int dr_idx;
	/* egress layer2 */
	unsigned char dmac[6];
	unsigned char smac[6];
	unsigned short vlan1;
	unsigned short vlan2;
	unsigned short pppoe_id;

	/* ingress layer3 */
	unsigned int ing_sipv4;
	unsigned int ing_dipv4;

	unsigned int ing_sipv6_0;
	unsigned int ing_sipv6_1;
	unsigned int ing_sipv6_2;
	unsigned int ing_sipv6_3;

	unsigned int ing_dipv6_0;
	unsigned int ing_dipv6_1;
	unsigned int ing_dipv6_2;
	unsigned int ing_dipv6_3;

	/* egress layer3 */
	unsigned int eg_sipv4;
	unsigned int eg_dipv4;

	unsigned int eg_sipv6_0;
	unsigned int eg_sipv6_1;
	unsigned int eg_sipv6_2;
	unsigned int eg_sipv6_3;

	unsigned int eg_dipv6_0;
	unsigned int eg_dipv6_1;
	unsigned int eg_dipv6_2;
	unsigned int eg_dipv6_3;
	unsigned char prot;
	/*ingress layer4*/
	unsigned short ing_sp;
	unsigned short ing_dp;

	/*egress layer4*/
	unsigned short eg_sp;
	unsigned short eg_dp;

	unsigned char	ipv6_flowlabel;
	unsigned char   pppoe_act;
	unsigned int    vlan_layer;
	unsigned char	dst_port;
	unsigned int    dscp;

	unsigned long long packets;
	unsigned long long bytes;

	enum hwnat_status result;
};

struct hwnat_args {
	enum hwnat_status result;
	unsigned int entry_num;
	unsigned int num_of_entries;
	unsigned int debug;
	unsigned int entry_state; /* invalid=0, unbind=1, bind=2, fin=3 */
	struct hwnat_tuple entries[0];
};

/*hnat qos*/
struct hwnat_qos_args {
	unsigned int enable:1;
	unsigned int up:3;
	unsigned int weight:3;	/*UP resolution */
	unsigned int dscp:6;
	unsigned int dscp_set:3;
	unsigned int vpri:3;
	unsigned int ac:2;
	unsigned int mode:2;
	unsigned int weight0:4;	/*WRR 4 queue weight */
	unsigned int weight1:4;
	unsigned int weight2:4;
	unsigned int weight3:4;
	enum hwnat_status result;
};

/*hnat config*/
struct hwnat_config_args {
	unsigned int bind_threshold:16;
	unsigned int foe_full_lmt:14;
	unsigned int foe_half_lmt:14;
	unsigned int foe_qut_lmt:14;
	unsigned int pre_acl:9;
	unsigned int pre_meter:9;
	unsigned int pre_ac:9;
	unsigned int post_meter:9;
	unsigned int post_ac:9;
	unsigned int foe_tcp_ka:8;	/*unit 4 sec */
	unsigned int foe_udp_ka:8;	/*unit 4 sec */
	unsigned int foe_unb_dlta:8;	/*unit 1 sec */
	unsigned int foe_tcp_dlta:16;	/*unit 1 sec */
	unsigned int foe_udp_dlta:16;	/*unit 1 sec */
	unsigned int foe_fin_dlta:16;	/*unit 1 sec */
	unsigned int wan_vid:16;
	unsigned int lan_vid:16;
	unsigned int bind_dir:2;	/* 0=upstream, 1=downstream, 2=bi-direction */
	enum hwnat_status result;
};

struct hwnat_ac_args {
	unsigned int ag_index;
	unsigned long long ag_byte_cnt;
	unsigned long long ag_pkt_cnt;
	enum hwnat_status result;
};

struct hwnat_mcast_args {
	unsigned int    mc_vid:16;
	unsigned int    mc_px_en:4;
	unsigned int    valid:1;
	unsigned int    rev2:3;
	unsigned int    mc_px_qos_en:4;
	unsigned int    mc_qos_qid:4;
	unsigned char	dst_mac[6];
};

struct hwnat_mib_args {
	unsigned int    entry_num:16;
};

struct hwnat_ipi_args {
	unsigned int hnat_ipi_enable;
	unsigned int drop_pkt;
	unsigned int queue_thresh;
	unsigned int ipi_cnt_mod;
};

struct hwnat_mib_ip {
	union {
		unsigned int ipv4_addr;
		unsigned int ipv6_addr[4];
		}ip;
	unsigned int is_ipv4;
	unsigned long long tx_bytes;
	unsigned long long rx_bytes;
	unsigned long long tx_packets;
	unsigned long long rx_packets;
};

#define HWNAT_MAX_MIB_IP_ENTRY_NUM (512)
struct hwnat_mib_all_ip_args {
	unsigned int entry_num;
	struct hwnat_mib_ip entries[HWNAT_MAX_MIB_IP_ENTRY_NUM];
};
int get_ppe_mib_ip(struct hwnat_mib_all_ip_args *all_ip);
int ppe_get_agcnt(struct hwnat_ac_args *opt3);
int reply_entry_idx(struct hwnat_tuple *opt, unsigned int entry_num);
void ppe_mib_dump(unsigned int entry_num);
void ppe_mib_dram_dump(unsigned int entry_num);
int get_ppe_mib(struct hwnat_tuple *opt);
int ppe_tbl_clear(void);
void dump_dport(void);
int ppe_set_ka_interval(unsigned char tcp_ka, unsigned char udp_ka);
int ppe_set_464_enable(int enable);
#endif
