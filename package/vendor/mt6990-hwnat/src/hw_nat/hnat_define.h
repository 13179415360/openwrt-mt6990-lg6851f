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
#include "ra_nat.h"

extern unsigned int hnat_chip_name;
extern unsigned int fe_feature;

extern struct MED_HNAT_INFO_HOST *med_info_base;

#if defined(CONFIG_RA_HW_NAT_PACKET_SAMPLING)
extern struct ps_entry *ppe_ps_base;
#endif

extern u8 USE_3T_UDP_FRAG;
extern u32 DP_GMAC1;
extern u32 DP_GMAC2;

extern unsigned int hnat_chip_name;
extern unsigned int fe_feature;

extern int (*ra_sw_nat_hook_rx)(struct sk_buff *skb);
extern int (*ra_sw_nat_hook_tx)(struct sk_buff *skb, int gmac_no);
extern int (*ppe_hook_rx_wifi)(struct sk_buff *skb);
extern int (*ppe_hook_tx_wifi)(struct sk_buff *skb, int gmac_no);
extern int (*ppe_hook_rx_modem_thread)(struct sk_buff *skb);
extern int (*ppe_hook_rx_modem)(struct sk_buff *skb, u8 drop, u8 channel);
extern int (*ppe_hook_tx_modem)(struct sk_buff *skb, u32 net_type, u32 channel_id);
extern int (*ppe_hook_rx_rndis)(struct sk_buff *skb);
extern int (*ppe_hook_tx_rndis)(struct sk_buff *skb);
extern int (*ppe_hook_rx_eth)(struct sk_buff *skb);
extern int (*ppe_hook_tx_eth)(struct sk_buff *skb, int gmac_no);
extern int (*ppe_hook_eth_tx_fport)(struct sk_buff *skb);
extern int (*ppe_hook_rx_ext)(struct sk_buff *skb);
extern int (*ppe_hook_tx_ext)(struct sk_buff *skb, int gmac_no);
extern int (*ppe_hook_rx_snps)(struct sk_buff *skb);
extern int (*ppe_hook_tx_snps)(struct sk_buff *skb);
extern void (*ppe_dev_register_hook)(struct net_device *dev);
extern void (*ppe_dev_unregister_hook)(struct net_device *dev);
extern int (*ppe_get_dev_stats)(struct net_device *dev, struct rtnl_link_stats64 *storage);
extern int (*ppe_get_conn_stats)(struct hnat_flow_tuple *tuple, u64 *pkt_cnt, u64 *byte_cnt);
extern int (*ppe_del_entry_by_mac)(unsigned char *mac);
extern int (*ppe_usb_set_cb_hook)(
	int (*ppe_tx_send)(struct net_device *net, char *buff, int len, int dma_id, int queue_id),
	void (*ppe_rx_done)(void *context));
extern void (*ppe_usb_tx_done_hook)(char *buff);
extern int  (*ppe_usb_rx_send_hook)(
	struct net_device *net, void *context, char *buff, int len);
extern int (*ppe_usb_tx_link_num_hook)(void);
extern void (*ppe_usb_resume_queue_hook)(struct net_device *net, int dma_id, int queue_id);
extern void (*ppe_usb_suspend_queue_hook)(struct net_device *net, int dma_id, int queue_id);

/* HOOK ID */
#define HWNAT_HOOK_ID_ETH 0
#define HWNAT_HOOK_ID_MODEM 1
#define HWNAT_HOOK_ID_WIFI 2
#define HWNAT_HOOK_ID_RNDIS 3
#define HWNAT_HOOK_ID_EXT 4
#define HWNAT_HOOK_ID_SNPS 5

/* DIR ID */
#define HWNAT_DIR_ID_ALL 0
#define HWNAT_DIR_ID_RX 1
#define HWNAT_DIR_ID_TX 2

/* ACT MODE */
#define HWNAT_NAPT 0
#define HWNAT_NAT 1
#define HWNAT_BRIDGE 2
#define HWNAT_ROUTE 3

/* PATH TYPE */
enum {
	HW_PATH = 0,
	SW_PATH,
};

extern u8 bind_dir;
extern u16 wan_vid;
extern u16 lan_vid;
#if defined(CONFIG_RAETH_QDMA)
extern unsigned int M2Q_table[64];
extern unsigned int lan_wan_separate;
#endif
extern u32 debug_level;
extern u16 set_fqos;
extern u8 eth_qos_enable;
extern u8 wifi_qos_enable;
extern u8 md_qos_enable;
extern u8 xlat_enable;
extern u8 forbid_bind_ecn;

extern struct net_device *dst_port[MAX_IF_NUM];
extern u8 dst_port_type[MAX_IF_NUM];
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

#if !defined(CONFIG_MTK_SGMII_NETSYS)
#define DEV_NAME_HNAT_SNPS	"eth0"
#elif defined(CONFIG_MTK_SGMII_SNPS)
#define DEV_NAME_HNAT_SNPS	"eth1"
#else
#define DEV_NAME_HNAT_SNPS	"eth2"
#endif

#define DEV_NAME_HNAT_EDMA0	"edma0"
#define DEV_NAME_HNAT_EDMA1	"edma1"
#define DEV_NAME_HNAT_CCCI0	"ccmni0"
#define DEV_NAME_HNAT_CCCI1	"ccmni1"
#define DEV_NAME_HNAT_CCCI2	"ccmni2"
#define DEV_NAME_HNAT_CCCI3	"ccmni3"
#define DEV_NAME_HNAT_CCCI4	"ccmni4"
#define DEV_NAME_HNAT_CCCI5	"ccmni5"
#define DEV_NAME_HNAT_CCCI6	"ccmni6"
#define DEV_NAME_HNAT_CCCI7	"ccmni7"
#define DEV_NAME_HNAT_RA0	"ra0"
#define DEV_NAME_HNAT_RAI0	"rai0"
#define DEV_NAME_HNAT_RAX0	"rax0"
#define DEV_NAME_HNAT_APCLI0	"apcli0"
#define DEV_NAME_HNAT_APCLI1	"apcli1"
#define DEV_NAME_HNAT_RNDIS0	"rndis0"

//HNAT DSA Special Tag support
#define DEV_NAME_HNAT_LAN0	"lan0"
#define DEV_NAME_HNAT_LAN1	"lan1"
#define DEV_NAME_HNAT_LAN2	"lan2"
#define DEV_NAME_HNAT_LAN3	"lan3"
#define IS_HNAT_DEV_LAN(dev) (!strncmp(dev->name, "lan", 3))

//EDMA
#define DEV_NAME        DEV_NAME_HNAT_EDMA0
#define DEV2_NAME       DEV_NAME_HNAT_EDMA1

#define AQR_DEV_NAME        "aqr0"
#define AQR_DEV2_NAME       "aqr1"

#endif
