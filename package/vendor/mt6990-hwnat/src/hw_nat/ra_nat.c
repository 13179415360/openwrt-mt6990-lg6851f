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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/if_vlan.h>
#include <net/ipv6.h>
#include <net/ip.h>
#include <linux/if_pppox.h>
#include <linux/ppp_defs.h>
#include <linux/pci.h>
#include <linux/errno.h>
#include <linux/inetdevice.h>
#include <net/rtnetlink.h>
#include <net/netevent.h>
#include <linux/platform_device.h>
#include <linux/timer.h>
#include "ra_nat.h"
#include "foe_fdb.h"
#include "frame_engine.h"
#include "util.h"
#include "hnat_ioctl.h"
#include "hnat_define.h"
#include "hnat_config.h"
#include "hnat_dbg_proc.h"
#include "hnat_common.h"
#include "hnat_monitor.h"
#include "hnat_fast.h"
#include "hnat_power.h"

static struct platform_device *pdev_hnat;
struct timer_list hwnat_clear_entry_timer;
unsigned int hnat_chip_name;
EXPORT_SYMBOL(hnat_chip_name);
unsigned int fe_feature;
EXPORT_SYMBOL(fe_feature);
void __iomem *fe_base;
void __iomem *med_base;
void __iomem *netsys_base;

static dev_t dev_hnat;

extern u32 rndis_mod;
extern u32 rndis_bind_count;

void set_rx_if_idx_md(struct sk_buff *skb, u8 channel)
{
	/* channel: 0~19, bit7 is used for Ethernet PDU so we mask it */

	FOE_IF_IDX(skb) = (channel & 0x1f) + DP_CCMNI0;
}

void set_rx_if_idx(struct sk_buff *skb)
{
	u8 i, match;

	match = 0;
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == skb->dev) {
			FOE_IF_IDX(skb) = i;
			match = 1;
			//if (debug_level >= 7)
			//	pr_info("%s : Interface=%s, i=%x\n", __func__, skb->dev->name, i);
			break;
		}
	}
	if (match == 0)
		FOE_IF_IDX(skb) = INVALID_IFIDX;
}
#if(0)
static void hwnat_clear_entry(struct timer_list *t)
{
	pr_info("HW_NAT work normally\n");
	reg_modify_bits(PPE_FOE_CFG, FWD_CPU_BUILD_ENTRY, 4, 2);
	/* del_timer_sync(&hwnat_clear_entry_timer); */
}
#endif
#include "mcast_tbl.h"

/*#include "../../../drivers/net/raeth/ra_ioctl.h"*/

/* #define DSCP_REMARK_TEST */
/* #define PREBIND_TEST */
#define DD \
{\
pr_info("%s %d\n", __func__, __LINE__); \
}
#if(0)
static void hnat_reset_timestamp(unsigned long data)
{
	struct foe_entry *entry;
	int hash_index;

	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_TCP_AGE, 9, 0);
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_UDP_AGE, 10, 0);
	reg_write(FOE_TS, 0);
	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];
		if (entry->bfib1.state == BIND)
			entry->ipv4_hnapt.udib1.time_stamp = 0;
	}

	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_TCP_AGE, 9, 1);
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_UDP_AGE, 10, 1);
	mod_timer(&hnat_reset_timestamp_timer, jiffies + 14400 * HZ);
}

void foe_clear_entry(struct neighbour *neigh)
{
	int hash_index, clear;
	struct foe_entry *entry;
	u32 *daddr = (u32 *)neigh->primary_key;
	const u8 *addrtmp;
	u8 mac0, mac1, mac2, mac3, mac4, mac5;
	u32 dip;

	dip = (u32)(*daddr);
	clear = 0;
	addrtmp = neigh->ha;
	mac0 = (u8)(*addrtmp);
	mac1 = (u8)(*(addrtmp + 1));
	mac2 = (u8)(*(addrtmp + 2));
	mac3 = (u8)(*(addrtmp + 3));
	mac4 = (u8)(*(addrtmp + 4));
	mac5 = (u8)(*(addrtmp + 5));

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];
		if (entry->bfib1.state == BIND) {
			/*pr_info("before old mac= %x:%x:%x:%x:%x:%x, new_dip=%x\n",*/
			/*	entry->ipv4_hnapt.dmac_hi[3],*/
			/*	entry->ipv4_hnapt.dmac_hi[2],*/
			/*	entry->ipv4_hnapt.dmac_hi[1],*/
			/*	entry->ipv4_hnapt.dmac_hi[0],*/
			/*	entry->ipv4_hnapt.dmac_lo[1],*/
			/*	entry->ipv4_hnapt.dmac_lo[0], entry->ipv4_hnapt.new_dip);*/
			if (entry->ipv4_hnapt.new_dip == ntohl(dip)) {
				if ((entry->ipv4_hnapt.dmac_hi[3] != mac0) ||
				    (entry->ipv4_hnapt.dmac_hi[2] != mac1) ||
				    (entry->ipv4_hnapt.dmac_hi[1] != mac2) ||
				    (entry->ipv4_hnapt.dmac_hi[0] != mac3) ||
				    (entry->ipv4_hnapt.dmac_lo[1] != mac4) ||
				    (entry->ipv4_hnapt.dmac_lo[0] != mac5)) {
					pr_info("%s: state=%d\n", __func__, neigh->nud_state);
					reg_modify_bits(PPE_FOE_CFG, ONLY_FWD_CPU, 4, 2);

					entry->ipv4_hnapt.udib1.state = INVALID;
					entry->ipv4_hnapt.udib1.time_stamp = reg_read(FOE_TS) & 0xFF;
					ppe_set_cache_ebl();
					mod_timer(&hwnat_clear_entry_timer, jiffies + 3 * HZ);

					pr_info("delete old entry: dip =%x\n", ntohl(dip));

					pr_info("old mac= %x:%x:%x:%x:%x:%x, dip=%x\n",
						entry->ipv4_hnapt.dmac_hi[3],
						entry->ipv4_hnapt.dmac_hi[2],
						entry->ipv4_hnapt.dmac_hi[1],
						entry->ipv4_hnapt.dmac_hi[0],
						entry->ipv4_hnapt.dmac_lo[1],
						entry->ipv4_hnapt.dmac_lo[0],
						ntohl(dip));
					pr_info("new mac= %x:%x:%x:%x:%x:%x, dip=%x\n",
						mac0, mac1, mac2, mac3, mac4, mac5, ntohl(dip));
				}
			}
		}
	}
}

static int wh2_netevent_handler(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct net_device *dev = NULL;
	struct neighbour *neigh = NULL;

	switch (event) {
	case NETEVENT_NEIGH_UPDATE:
		neigh = ptr;
		dev = neigh->dev;
		if (dev)
			foe_clear_entry(neigh);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block hnat_netevent_nb __read_mostly = {
	.notifier_call = wh2_netevent_handler,
};
#endif
#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
int32_t ppe_tx_modem_handler(struct sk_buff *skb, u32 net_type, u32 channel_id)
{
	struct foe_entry *entry, entry_input;
	int ret;
	int eth_pdu = 0;

	FOE_MINFO_NTYPE(skb) = net_type;
	FOE_MINFO_CHID(skb) = channel_id;
	if ((channel_id & 0x80) == 0x80)
		eth_pdu = 1;

	ret = check_whitelist(skb);

	if (ret)
		return 1;

	ret = check_entry_region(skb);

	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_modem(skb, &entry_input, MDMA_PSE_PORT, eth_pdu);

	return ret;
}

int32_t ppe_rx_modem_thread_handler(struct sk_buff *skb)
{
	return rx_cpu_handler_modem_thread(skb);
}

extern void __iomem *medmcu_hnat_info_host_base_virt;
int32_t ppe_rx_modem_handler(struct sk_buff *skb, u8 drop, u8 channel)
{
	int ret;
	struct MED_HNAT_INFO_HOST *med_dmad;
	unsigned int foe_entry_num, cpu_reason, wdix, rdix;
	//void *cache_start;

	rdix = reg_read(MEDHW_SSR1_DST_RB0_RIDX) & 0x3ffff;
	wdix = reg_read(MEDHW_SSR1_DST_RB0_WIDX) & 0x3ffff;
#if(0)
	cache_start = medmcu_hnat_info_host_base_virt + (sizeof(struct MED_HNAT_INFO_HOST) * rdix);

	__inval_dcache_area(cache_start, sizeof(struct MED_HNAT_INFO_HOST));
#endif
	med_dmad = &med_info_base[rdix];

	if((rdix == wdix) && (wdix == 0)) {
		pr_info("HNAT HOST INFO index error wdix = %x, rdix = %x\n", wdix, rdix);
	}



	foe_entry_num = med_dmad->dmad_info1.PPE_ENTRY;
	cpu_reason = med_dmad->dmad_info1.CRSN;

	if (debug_level >= 11) {
		pr_info("MD RX : cpu_reason = %x, foe_entry_num = %x, rdix = %x, wdix = %x, channel= %u\n",
			cpu_reason, foe_entry_num, rdix, wdix, channel);
	}

	reg_write(MEDHW_SSR1_DST_RB0_DEC, ((0x1 << 31) | 0x1));

	//if (debug_level >= 6) {
	//	while (reg_read(MEDHW_SSR1_DST_RB0_DEC) & (0x1 << 31));
	//	rdix = reg_read(MEDHW_SSR1_DST_RB0_RIDX) & 0x3ffff;
	//	pr_info("MD RX : new ridx = %x\n", rdix);
	//}

	if (drop == 1) {
		if (debug_level >= 6)
			pr_info("hook drop\n");
		return 1;
	}

	if (IS_SPACE_AVAILABLE_HEAD(skb)) {
		FOE_ENTRY_NUM(skb) = foe_entry_num;
		FOE_AI(skb) = cpu_reason;
		FOE_MAGIC_TAG(skb) = FOE_MAGIC_MED;
	} else {
		pr_info("header room size not available = %u\n", skb_headroom(skb));
	}

	foe_format_create(skb);
	rx_debug_log(skb);
	FOE_ALG(skb) = 0;
	set_rx_if_idx_md(skb, channel);
	ret = rx_cpu_handler_modem(skb);

	return ret;
}
#endif
int32_t ppe_tx_rndis_handler(struct sk_buff *skb)
{
	struct foe_entry *entry, entry_input;
	int ret;

	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_AI(skb), FOE_SP(skb));
	ret = check_whitelist(skb);

	if (ret)
		return 1;

	ret = check_entry_region(skb);

	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_rndis(skb, &entry_input);

	return ret;
}

int32_t ppe_rx_rndis_handler(struct sk_buff *skb)
{
	int ret;

	set_rx_if_idx(skb);
	FOE_ALG(skb) = 0;
	ret = rx_cpu_handler_rndis(skb);
	return ret;
}



int32_t ppe_tx_wifi_handler(struct sk_buff *skb, int gmac_no)
{
	struct foe_entry *entry, entry_input;
	int ret;

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
	if (gmac_no == ADMA_PSE_PORT) {
		gmac_no = EDMA0_PSE_PORT;

		if (debug_level >= 10)
			pr_info("%s, wifi download bind use edma0 rx instead of adma\n", __func__);
	}
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT */

	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d, gmac_no:%d\n", __func__, FOE_AI(skb), FOE_SP(skb), gmac_no);
	ret = check_whitelist(skb);

	if (ret)
		return 1;

	ret = check_entry_region(skb);

	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_wifi(skb, &entry_input, gmac_no);

	if(debug_level >= 10)
		pr_info("%s end, FOE_AI(skb):0x%x, FOE_SP(skb):%d, gmac_no:%d\n", __func__, FOE_AI(skb), FOE_SP(skb), gmac_no);


	return ret;
}

int32_t ppe_rx_wifi_handler(struct sk_buff *skb)
{
	int ret;

	foe_format_create(skb);
	rx_debug_log(skb);
	set_rx_if_idx(skb);
	FOE_ALG(skb) = 0;
	ret = rx_cpu_handler_wifi(skb);
	return ret;
}

int32_t ppe_tx_eth_handler(struct sk_buff *skb, int gmac_no)
{
	struct foe_entry *entry, entry_input;
	int ret;

	ret = check_whitelist(skb);

	if (ret)
		return 1;
	ret = check_entry_region(skb);

	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_eth(skb, &entry_input, gmac_no);

	if (debug_level >= 10)
		pr_notice("%s, end, ret:%d\n", __func__, ret);

	return ret;
}

int32_t ppe_rx_eth_handler(struct sk_buff *skb)
{
	int ret;

	foe_format_create(skb);
	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_AI(skb), FOE_SP(skb));

	//FOE_INFO_DUMP(skb);
	FOE_ALG(skb) = 0;
	rx_debug_log(skb);
	set_rx_if_idx(skb);
	ret = rx_cpu_handler_eth(skb);

	if (debug_level >= 10)
		pr_info("%s end, ret:%d\n", __func__, ret);

	return ret;
}

int32_t ppe_tx_ext_handler(struct sk_buff *skb, int gmac_no)
{
	struct foe_entry *entry, entry_input;
	int ret;

	ret = check_whitelist(skb);

	if (ret)
		return 1;

	ret = check_entry_region(skb);

	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_ext(skb, &entry_input, gmac_no);

	return ret;

}

int32_t ppe_rx_ext_handler(struct sk_buff *skb)
{
	int ret;

	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):%x\n", __func__, FOE_AI(skb));
	set_rx_if_idx(skb);
	ret = rx_cpu_handler_ext(skb);
	return ret;
}

int32_t ppe_tx_snps_handler(struct sk_buff *skb)
{
	struct foe_entry *entry, entry_input;
	int ret;

	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_AI(skb), FOE_SP(skb));

	ret = check_whitelist(skb);
	if (ret)
		return 1;

	ret = check_entry_region(skb);
	if (ret)
		return 1;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	entry_input = *entry;

	ret = tx_cpu_handler_snps(skb, &entry_input);

	if (debug_level >= 10)
		pr_info("%s end, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_AI(skb), FOE_SP(skb));

	return ret;

}

int32_t ppe_rx_snps_handler(struct sk_buff *skb)
{
	int ret;

	set_rx_if_idx(skb);
	FOE_ALG(skb) = 0;
	ret = rx_cpu_handler_snps(skb);

	if (debug_level >= 10)
		pr_info("%s end, FOE_AI(skb):0x%x\n", __func__, FOE_AI(skb));

	return ret;
}

void ppe_init_hook(bool init) {

	/* In manual mode, PPE always reports UN-HIT CPU reason, so we don't need to process it */
	/* Register RX/TX hook point */
	if (!(fe_feature & MANUAL_MODE)) {
		ppe_hook_rx_wifi = (init ? ppe_rx_wifi_handler : NULL);
		ppe_hook_tx_wifi = (init ? ppe_tx_wifi_handler : NULL);
		ra_sw_nat_hook_rx = (init ? ppe_rx_wifi_handler : NULL);
		ra_sw_nat_hook_tx = (init ? ppe_tx_wifi_handler : NULL);
#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
#ifdef CONFIG_TUNNEL_FAST_PATH
		ppe_hook_rx_modem_thread = (init ? ppe_rx_modem_thread_handler : NULL);
#endif /* CONFIG_TUNNEL_FAST_PATH */
		ppe_hook_rx_modem = (init ? ppe_rx_modem_handler : NULL);
		ppe_hook_tx_modem = (init ? ppe_tx_modem_handler : NULL);
#endif

#ifdef	CONFIG_RAETH_EDMA
		ppe_hook_rx_eth = NULL;
		ppe_hook_tx_eth = NULL;
#else
		ppe_hook_rx_eth = (init ? ppe_rx_eth_handler : NULL);
		ppe_hook_tx_eth = (init ? ppe_tx_eth_handler : NULL);
#endif
		ppe_hook_rx_ext = (init ? ppe_rx_ext_handler : NULL);
		/* EDIA TX fast path is not ready */
		ppe_hook_tx_ext = NULL;

		if (fe_feature & AUTO_HNAT) {
			ppe_hook_rx_snps = (init ? ppe_rx_snps_handler : NULL);
			ppe_hook_tx_snps = (init ? ppe_tx_snps_handler : NULL);
		}

		ppe_get_dev_stats = (init ? ppe_get_dev_stats_handler : NULL);
		ppe_get_conn_stats = (init ? ppe_get_conn_stats_handler : NULL);

		/* For wifi driver */
		ppe_del_entry_by_mac = (init ? foe_del_entry_by_mac : NULL);

		ppe_hook_eth_tx_fport = (init ? ppe_eth_tx_fport_handler : NULL);

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
		ppe_hook_rx_rndis = (init ? ppe_rx_rndis_handler : NULL);
		ppe_hook_tx_rndis = (init ? ppe_tx_rndis_handler : NULL);

		ppe_usb_set_cb_hook = (init ? ppe_usb_set_cb : NULL);
		ppe_usb_tx_done_hook = (init ? ppe_usb_tx_done : NULL);
		ppe_usb_rx_send_hook = (init ? ppe_usb_rx_send : NULL);
		ppe_usb_tx_link_num_hook = (init ? ppe_usb_tx_link_num : NULL);
		ppe_usb_resume_queue_hook = (init ? ppe_usb_resume_queue : NULL);
		ppe_usb_suspend_queue_hook = (init ? ppe_usb_suspend_queue : NULL);
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT */

	}

	ppe_dev_register_hook = (init ? ppe_dev_reg_handler : NULL);
	ppe_dev_unregister_hook = (init ? ppe_dev_unreg_handler : NULL);
}

void ppe_modify_hook(bool clear, unsigned char hook_id, int dir) {

	pr_info("ppe_modify_hook, clear = %d, hook_id = %d, dir=%d\n", clear, hook_id, dir);

	if (hook_id == HWNAT_HOOK_ID_ETH) {

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX)
			ppe_hook_rx_eth = clear ? NULL : ppe_rx_eth_handler;

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_TX)
			ppe_hook_tx_eth = clear ? NULL : ppe_tx_eth_handler;

	} else if (hook_id == HWNAT_HOOK_ID_WIFI) {

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX) {
			ppe_hook_rx_wifi = clear ? NULL : ppe_rx_wifi_handler;
			ra_sw_nat_hook_rx = clear ? NULL : ppe_rx_wifi_handler;
		}

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_TX) {
			ppe_hook_tx_wifi = clear ? NULL : ppe_tx_wifi_handler;
			ra_sw_nat_hook_tx = clear ? NULL : ppe_tx_wifi_handler;
		}
	} else if (hook_id == HWNAT_HOOK_ID_MODEM) {

	#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX)
			ppe_hook_rx_modem = clear ? NULL : ppe_rx_modem_handler;

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_TX)
			ppe_hook_tx_modem = clear ? NULL : ppe_tx_modem_handler;

	#endif /* CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT */

	} else if (hook_id == HWNAT_HOOK_ID_RNDIS) {

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX) {
			ppe_hook_rx_rndis = clear ? NULL : ppe_rx_rndis_handler;
			ppe_usb_rx_send_hook = clear ? NULL : ppe_usb_rx_send;
		}
		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_TX)
			ppe_hook_tx_rndis = clear ? NULL : ppe_tx_rndis_handler;
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT */

	} else if (hook_id == HWNAT_HOOK_ID_EXT) {

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX)
			ppe_hook_rx_ext = clear ? NULL : ppe_rx_ext_handler;

		ppe_hook_tx_ext = NULL;

	} else if (hook_id == HWNAT_HOOK_ID_SNPS) {

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_RX)
			ppe_hook_rx_snps = clear ? NULL : ppe_rx_snps_handler;

		if (dir == HWNAT_DIR_ID_ALL || dir == HWNAT_DIR_ID_TX)
			ppe_hook_tx_snps = clear ? NULL : ppe_tx_snps_handler;
	}

	pr_notice("****************************************\n");
	pr_notice("[0] ETH_RX is %p, ETH_TX is %p\n",
			ppe_hook_rx_eth, ppe_hook_tx_eth);
	pr_notice("[1] MD_RX is %p, MD_TX is %p\n",
			ppe_hook_rx_modem, ppe_hook_tx_modem);
	pr_notice("[2] WIFI_RX is %p, WIFI_TX is %p\n",
			ra_sw_nat_hook_rx, ra_sw_nat_hook_tx);
	pr_notice("[2] WIFI_RX is %p, WIFI_TX is %p\n",
			ppe_hook_rx_wifi, ppe_hook_tx_wifi);
	pr_notice("[3] RNDIS_RX is %p, RNDIS_TX is %p\n",
			ppe_hook_rx_rndis, ppe_hook_tx_rndis);
	pr_notice("[4] EXT_RX is %p, EXT_TX is %p\n",
			ppe_hook_rx_ext, ppe_hook_tx_ext);
	pr_notice("[3] RNDIS_RX is %p, RNDIS_TX is %p\n",
			ppe_usb_rx_send_hook, ppe_hook_tx_rndis);
	if (fe_feature & AUTO_HNAT)
		pr_notice("[5] SNPS_RX is %p, SNPS_TX is %p\n",
			ppe_hook_rx_snps, ppe_hook_tx_snps);
	pr_notice("****************************************\n");

}

int ppe_start(void) {

	pr_info("%s\n", __func__);

	/* Set PPE FOE Table */
	ppe_set_foe_table();

	ppe_eng_init();

	/* Set PSE reserve queue */
	pse_set_reserve_q();

	/* Set GMAC fowrards packet to PPE */
	set_gdma_fwd(1);

	/* acquire FE driver & init EDMA0, EDMA1 */
	ppe_fe_dma_init();

	return 0;
}

void ppe_stop(void) {

	pr_info("%s\n", __func__);

	/* Restore PPE related register */
	ppe_eng_stop();

	/* release FE driver & deinit EDMA0, EDMA1 */
	ppe_fe_dma_deinit();
}

static int32_t ppe_init_mod(void){

	hwnat_config_setting();
	fe_feature_setting();

	pr_notice("%s, hnat_chip_name = 0x%x, fe_feature = 0x%x\n", __func__, hnat_chip_name, fe_feature);

	fe_base = ioremap(MTK_FE_BASE, MTK_FE_RANGE);
	med_base = ioremap(MTK_MED_BASE, MTK_FE_RANGE);
	netsys_base = ioremap(MTK_ETHDMA_BASE, 0x1000);

	pdev_hnat = platform_device_alloc("HW_NAT", PLATFORM_DEVID_AUTO);
	if (!pdev_hnat)
		return -ENOMEM;

	pdev_hnat->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	pdev_hnat->dev.dma_mask = &pdev_hnat->dev.coherent_dma_mask;
	hwnat_setup_dma_ops(&pdev_hnat->dev, FALSE);

	/* Allocate FOE table base */
	if (!foe_alloc_tbl(FOE_4TB_SIZ, &pdev_hnat->dev)){
		pr_info("foe table allocation failed\n");
		platform_device_put(pdev_hnat);
		return -ENOMEM; /* memory allocation failed */
	}

	ppe_cache_lock_init();

	/* fe probe */
	ppe_fe_dma_probe();

	/* ppe start */
	ppe_start();

	/* Get net_device structure of Dest Port */
	ppe_set_dst_port(1);

	/* Register ioctl handler */
	ppe_reg_ioctl_handler(dev_hnat);

	ppe_init_hook(true);

	/*if (fe_feature & HNAT_IPI)*/
	/*	HnatIPIInit();*/


	hnat_debug_proc_init();

	/* Set DVFS OPP */
	if (fe_feature & SW_DVFS)
		sw_dvfs_init();

	hw_dvfs_init();

	ppe_init_mib_counter();

	hnat_monitor_enable();

	/* set wakelock during hwnat */
	hnat_wakelock_register();
	hnat_work_init(1);

#if defined(CONFIG_HNAT_V2)
	hnat_driver_register();
#endif /* CONFIG_HNAT_V2 */

	return 0;
}

static void ppe_cleanup_mod(void)
{
	NAT_PRINT("Ralink HW NAT Module Disabled\n");

	/* Set GMAC fowrards packet to CPU */
	set_gdma_fwd(0);

	/* Unregister RX/TX hook point */
	ppe_init_hook(false);

	/* release wakelock during hwnat */
	hnat_wakelock_unregister();
	hnat_work_init(0);

	hnat_monitor_disable();

	/* ppe stop */
	ppe_stop();

	/* fe cleanup */
	ppe_fe_dma_cleanup();

	/* Free FOE table base */
	foe_free_tbl(FOE_4TB_SIZ, &pdev_hnat->dev);

	/* Unregister ioctl handler */
	ppe_unreg_ioctl_handler(dev_hnat);
	//if ((fe_feature & HNAT_QDMA) && (fe_feature & HNAT_MCAST))
		//foe_mcast_entry_del_all();

	/* Release net_device structure of Dest Port */
	ppe_set_dst_port(0);

/*	if(fe_feature & HNAT_IPI)*/
/*		HnatIPIDeInit();*/

	/* Set DVFS OPP */
	if (fe_feature & SW_DVFS)
		sw_dvfs_fini();

	hw_dvfs_fini();

	hnat_debug_proc_exit();
	iounmap(fe_base);
	iounmap(med_base);
	iounmap(netsys_base);

#if defined(CONFIG_HNAT_V2)
	hnat_driver_unregister();
#endif /* CONFIG_HNAT_V2 */

}



module_init(ppe_init_mod);
module_exit(ppe_cleanup_mod);

MODULE_AUTHOR("Steven Liu/Kurtis Ke");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("MediaTek MT6990 hardware NAT and PPE provider");
