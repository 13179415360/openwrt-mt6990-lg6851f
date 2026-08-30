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
#ifndef _RA_COMMON_WANTED
#define _RA_COMMON_WANTED

#include <linux/ip.h>
#include <linux/ipv6.h>
#include "foe_fdb.h"

#ifndef NEXTHDR_IPIP
#define NEXTHDR_IPIP 4
#endif

void ppe_set_foe_table(void);
void pse_set_reserve_q(void);
int foe_alloc_tbl(u32 num_of_entry, struct device *dev);
void foe_free_tbl(uint32_t num_of_entry, struct device *dev);
void ppe_set_dst_port(uint32_t ebl);
void ppe_set_ip_prot(void);
void foe_ac_update_ebl(int ebl);
int32_t ppe_eng_start(void);
void ppe_dev_reg_handler(struct net_device *dev);
void ppe_dev_unreg_handler(struct net_device *dev);
uint32_t hnat_cpu_reason_cnt(struct sk_buff *skb);
uint32_t foe_dump_pkt(struct sk_buff *skb, struct foe_entry *entry);
int hitbind_force_to_cpu_handler(struct sk_buff *skb, struct foe_entry *entry);
uint16_t remove_vlan_tag(struct sk_buff *skb);
uint32_t ppe_extif_rx_prepare(struct sk_buff *skb);
uint32_t ppe_extif_rx_send(struct sk_buff *skb);
uint32_t ppe_extif_rx_handler(struct sk_buff *skb);
int32_t ppe_eth_tx_fport_handler(struct sk_buff *skb);
uint32_t ppe_extif_pingpong_handler(struct sk_buff *skb);
uint32_t keep_alive_handler(struct sk_buff *skb, struct foe_entry *entry);
uint32_t keep_alive_old_pkt_handler(struct sk_buff *skb);
int hitbind_force_to_cpu_handler(struct sk_buff *skb, struct foe_entry *entry);
int32_t ppe_parse_layer_info(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result);
int get_skb_interface(struct sk_buff *skb);
uint16_t tx_decide_which_region(struct sk_buff *skb);
int bridge_lan_subnet(struct sk_buff *skb);
int get_done_bit(struct sk_buff *skb, struct foe_entry *entry);
int hitbind_force_mcast_to_wifi_handler(struct sk_buff *skb);
uint16_t is_if_pcie_wlan_rx(struct sk_buff *skb);
uint16_t is_magic_tag_protect_valid(struct sk_buff *skb);
int32_t setforce_port_qdmatx_qdmarx(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int32_t setforce_port_qdmatx_pdmarx(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int32_t setforce_port_pdmatx_pdmarx(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
uint32_t ppe_set_ext_if_num(struct sk_buff *skb, struct foe_entry *entry);
void set_ppe_table_done(struct foe_entry *entry);
uint32_t set_gdma_fwd(uint32_t ebl);
unsigned char *FOE_INFO_START_ADDR(struct sk_buff *skb);
int ppe_fill_table(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result);
int set_force_port_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int check_use_UDP_3T(struct sk_buff *skb, struct foe_entry *entry);
int check_entry_region(struct sk_buff *skb);
void clear_mib_count(struct sk_buff *skb, int pse_port);
int ppe_common_part(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int ppe_common_eth(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int ppe_common_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result);
int ppe_common_wifi(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
					struct pkt_parse_result *ppe_parse_result);
int ppe_common_modem(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
					struct pkt_parse_result *ppe_parse_result, int eth_pdu);
int ppe_common_rndis(struct sk_buff *skb, struct foe_entry *entry,
				    struct pkt_parse_result *ppe_parse_result);
int ppe_modem_fill_dummy_info(struct sk_buff *skb, int rx);
int check_magic_tag_valid(struct sk_buff *skb);
int set_pre_bind(struct sk_buff *skb,struct foe_entry *entry);
int tx_cpu_handler_wifi(struct sk_buff *skb, struct foe_entry *entry, int gmac_no);
int tx_cpu_handler_eth(struct sk_buff *skb, struct foe_entry *entry, int gmac_no);
int tx_cpu_handler_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no);
int tx_cpu_handler_modem(struct sk_buff *skb, struct foe_entry *entry, int gmac_no, int eth_pdu);
int tx_cpu_handler_rndis(struct sk_buff *skb, struct foe_entry *entry);
int tx_cpu_handler_snps(struct sk_buff *skb, struct foe_entry *entry);
int rx_cpu_handler_eth(struct sk_buff *skb);
int rx_cpu_handler_wifi(struct sk_buff *skb);
int rx_cpu_handler_modem(struct sk_buff *skb);
int rx_cpu_handler_modem_thread(struct sk_buff *skb);
int rx_cpu_handler_rndis(struct sk_buff *skb);
int rx_cpu_handler_snps(struct sk_buff *skb);
void foe_format_create(struct sk_buff *skb);
void rx_debug_log(struct sk_buff *skb);
void ppe_eng_init(void);
int check_whitelist(struct sk_buff *skb);
int check_hnat_type(struct sk_buff *skb);
void set_fast_path_info_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result);
struct foe_entry  *decide_which_ppe(struct sk_buff *skb);
int rx_cpu_handler_ext(struct sk_buff *skb);
void hwnat_config_setting(void);
void fe_feature_setting(void);
void FOE_INFO_DUMP(struct sk_buff *skb);
void ppe_eng_stop(void);
void hnat_work_init(int enable);
void ppe_cache_lock_init(void);

#ifdef CONFIG_MTK_ETHERNET_SOC
/* Ethernet export API */
extern u32 mtk_eth_search_port_by_mac_table(const u8 *ptr_mac, u32 vid, int *port);
#endif /* CONFIG_MTK_ETHERNET_SOC */


#endif
