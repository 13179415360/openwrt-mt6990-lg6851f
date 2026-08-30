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

#ifndef _FOE_DEFINE_WANTED
#define _FOE_DEFINE_WANTED

#include "frame_engine.h"
extern int (*ra_sw_nat_hook_rx)(struct sk_buff *skb);
extern int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_no);
extern void (*ppe_dev_register_hook)(struct net_device *dev);
extern void (*ppe_dev_unregister_hook)(struct net_device *dev);
extern u8 bind_dir;
extern u16 wan_vid;
extern u16 lan_vid;
#if defined(CONFIG_RAETH_QDMA)
extern unsigned int M2Q_table[64];
extern unsigned int lan_wan_separate;
#endif
extern u32 debug_level;
extern struct net_device *dst_port[MAX_IF_NUM];

extern struct ps_entry *ppe_ps_base;
/*extern struct pkt_parse_result ppe_parse_result;*/
extern int dbg_cpu_reason;
#ifdef CONFIG_SUPPORT_OPENWRT
#define DEV_NAME_HNAT_LAN	"eth0"
#define DEV_NAME_HNAT_WAN	"eth1"
#else
#define DEV_NAME_HNAT_LAN	"eth2"
#define DEV_NAME_HNAT_WAN	"eth3"
#endif

#ifdef CONFIG_RA_HW_NAT_PACKET_SAMPLING
static inline void hwnat_set_packet_sampling(struct foe_entry *entry)
{
	entry->ipv4_hnapt.bfib1.ps = 1;
}
#else
static inline void hwnat_set_packet_sampling(struct foe_entry *entry)
{
}
#endif

#if !defined(CONFIG_RALINK_MT7621)
static inline void hwnat_set_6rd_id(struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	reg_modify_bits(PPE_HASH_SEED, ntohs(ppe_parse_result->iph.id), 0, 16);
	entry->ipv6_6rd.per_flow_6rd_id = 1;
}
#else
static inline void hwnat_set_6rd_id(struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
}
#endif

#endif
