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
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include "ra_nat.h"
#include "frame_engine.h"
#include "foe_fdb.h"
#include "hnat_ioctl.h"
#include "util.h"
#include "hnat_config.h"
#include "hnat_define.h"
#include "hnat_common.h"
#include "hnat_dbg_proc.h"
#ifdef CONFIG_TUNNEL_FAST_PATH
#include "hnat_tnl.h"
#endif /* CONFIG_TUNNEL_FAST_PATH */
#ifdef CONFIG_MTK_NAT46_UTIL
#include "nat46_util.h"
#endif /* CONFIG_MTK_NAT46_UTIL */

struct pkt_rx_parse_result ppe_parse_rx_result;

extern struct foe_entry *ppe_foe_base;
extern struct foe_entry *ppe1_foe_base;
extern struct mib_entry *ppe_mib_base;
extern struct mib_entry *ppe1_mib_base;

#define PPE_MIB_TIMEOUT (DFL_FOE_TCP_DLTA - 1)
static struct timer_list ppe_mib_timer;
static spinlock_t ppe_mib_timer_lock;
static bool ppe_mib_timer_start = false;
extern bool ppe_mib_counter_en;

#ifdef CONFIG_TUNNEL_FAST_PATH
static uint64_t last_hnat_tnl_swnat_cnt_ul = 0;
static uint64_t last_hnat_tnl_swnat_cnt_dl = 0;
static uint64_t last_hnat_tnl_swnat_byte_cnt_ul = 0;
static uint64_t last_hnat_tnl_swnat_byte_cnt_dl = 0;

static uint64_t current_hnat_tnl_swnat_cnt_ul = 0;
static uint64_t current_hnat_tnl_swnat_cnt_dl = 0;
static uint64_t current_hnat_tnl_swnat_byte_cnt_ul = 0;
static uint64_t current_hnat_tnl_swnat_byte_cnt_dl = 0;
#endif

bool check_flow_match(struct hnat_flow_tuple *tuple, struct foe_entry *entry)
{


	if (tuple->pkt_type == IPV4_5T &&
	    (entry->bfib1.pkt_type == IPV4_HNAPT || entry->bfib1.pkt_type == IPV4_DSLITE) &&
	    entry->bfib1.udp == tuple->is_udp &&
	    entry->ipv4_hnapt.sip == tuple->sipv4 &&
	    entry->ipv4_hnapt.dip == tuple->dipv4 &&
	    entry->ipv4_hnapt.sport == tuple->sp &&
	    entry->ipv4_hnapt.dport == tuple->dp)
		return true;
	else if (tuple->pkt_type == IPV6_5T &&
		 (entry->bfib1.pkt_type == IPV6_5T_ROUTE ||  entry->bfib1.pkt_type == IPV6_6RD) &&
		 entry->bfib1.udp == tuple->is_udp &&
		 entry->ipv6_5t_route.ipv6_sip0 == tuple->sipv6_0 &&
		 entry->ipv6_5t_route.ipv6_sip1 == tuple->sipv6_1 &&
		 entry->ipv6_5t_route.ipv6_sip2 == tuple->sipv6_2 &&
		 entry->ipv6_5t_route.ipv6_sip3 == tuple->sipv6_3 &&
		 entry->ipv6_5t_route.ipv6_dip0 == tuple->dipv6_0 &&
		 entry->ipv6_5t_route.ipv6_dip1 == tuple->dipv6_1 &&
		 entry->ipv6_5t_route.ipv6_dip2 == tuple->dipv6_2 &&
		 entry->ipv6_5t_route.ipv6_dip3 == tuple->dipv6_3 &&
		 entry->ipv6_5t_route.sport == tuple->sp &&
		 entry->ipv6_5t_route.dport == tuple->dp)
		return true;

	return false;
}

bool ppe_entry_check_state(struct foe_entry *entry, int state)
{
	return (entry->bfib1.state == state);
}

uint32_t get_vlan1(struct foe_entry *entry)
{
	if (IS_IPV4_GRP(entry))
		return entry->ipv4_hnapt.vlan1;
	else if (IS_IPV6_GRP(entry))
		return entry->ipv6_5t_route.vlan1;

	return 0;
}


uint32_t get_rxif_idx(struct foe_entry *entry) {

	if (IS_IPV4_HNAT(entry)) {
		return entry->ipv4_hnapt.rxif_idx;

	} else if (IS_IPV4_HNAPT(entry)) {
		return entry->ipv4_hnapt.rxif_idx;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_1T_ROUTE(entry)) {
			return entry->ipv6_1t_route.rxif_idx;

		} else if (IS_IPV4_DSLITE(entry)) {

			return entry->ipv4_dslite.rxif_idx;

		} else if (IS_IPV6_3T_ROUTE(entry)) {

			return entry->ipv6_3t_route.rxif_idx;

		} else if (IS_IPV6_5T_ROUTE(entry)) {

			return entry->ipv6_5t_route.rxif_idx;

		} else if (IS_IPV6_6RD(entry)) {

			return entry->ipv6_6rd.rxif_idx;
		}
	}

	return entry->ipv4_hnapt.rxif_idx;

}

void set_rxif_idx(struct foe_entry *entry, u16 value) {

	if (IS_IPV4_HNAT(entry)) {
		entry->ipv4_hnapt.rxif_idx = value;

	} else if (IS_IPV4_HNAPT(entry)) {
		entry->ipv4_hnapt.rxif_idx = value;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_1T_ROUTE(entry)) {
			entry->ipv6_1t_route.rxif_idx = value;

		} else if (IS_IPV4_DSLITE(entry)) {

			entry->ipv4_dslite.rxif_idx = value;

		} else if (IS_IPV6_3T_ROUTE(entry)) {

			entry->ipv6_3t_route.rxif_idx = value;

		} else if (IS_IPV6_5T_ROUTE(entry)) {

			entry->ipv6_5t_route.rxif_idx = value;

		} else if (IS_IPV6_6RD(entry)) {

			entry->ipv6_6rd.rxif_idx = value;
		}
	}
}

uint32_t get_act_mode(struct foe_entry *entry) {

	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		return entry->ipv4_hnapt.act_mode;

	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_1T_ROUTE(entry)) {
			return entry->ipv6_1t_route.act_mode;

		} else if (IS_IPV4_DSLITE(entry)) {

			return entry->ipv4_dslite.act_mode;

		} else if (IS_IPV6_3T_ROUTE(entry)) {

			return entry->ipv6_3t_route.act_mode;

		} else if (IS_IPV6_5T_ROUTE(entry)) {

			return entry->ipv6_5t_route.act_mode;

		} else if (IS_IPV6_6RD(entry)) {

			return entry->ipv6_6rd.act_mode;
		}
	}

	return entry->ipv4_hnapt.act_mode;

}

uint32_t get_act_dp(struct foe_entry *entry) {

	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		return entry->ipv4_hnapt.act_dp;

	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_1T_ROUTE(entry)) {
			return entry->ipv6_1t_route.act_dp;

		} else if (IS_IPV4_DSLITE(entry)) {

			return entry->ipv4_dslite.act_dp;

		} else if (IS_IPV6_3T_ROUTE(entry)) {

			return entry->ipv6_3t_route.act_dp;

		} else if (IS_IPV6_5T_ROUTE(entry)) {

			return entry->ipv6_5t_route.act_dp;

		} else if (IS_IPV6_6RD(entry)) {

			return entry->ipv6_6rd.act_dp;
		}
	}

	return entry->ipv4_hnapt.act_dp;

}

int is_foe_mcast_entry(struct foe_entry *entry) {

#if defined(CONFIG_ODU_MCAST_SUPPORT)

	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry)) {
		if (entry->ipv4_hnapt.dmac_hi[3] == 0x01)
			return 1;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_1T_ROUTE(entry)) {
			if (entry->ipv6_1t_route.dmac_hi[3] == 0x33)
				return 1;
		} else if (IS_IPV4_DSLITE(entry)) {
			if (entry->ipv4_dslite.dmac_hi[3] == 0x33)
				return 1;
		} else if (IS_IPV6_3T_ROUTE(entry)) {
			if (entry->ipv6_3t_route.dmac_hi[3] == 0x33)
				return 1;
		} else if (IS_IPV6_5T_ROUTE(entry)) {
			if (entry->ipv6_5t_route.dmac_hi[3] == 0x33)
				return 1;
		} else if (IS_IPV6_6RD(entry)) {
			if (entry->ipv6_6rd.dmac_hi[3] == 0x33)
				return 1;
		}
	}
#endif /* CONFIG_ODU_MCAST_SUPPORT */
	return 0;
}



void ppe_mib_dump_ppe0(unsigned int entry_num, u64 *pkt_cnt, u64 *byte_cnt)
{
	u32 byt_l = 0;
	u64 byt_h = 0;
	u32 pkt_l = 0;
	u64 pkt_h = 0;

	reg_write(MIB_SER_CR, entry_num | (1 << 16));
	while (1) {
		if (!((reg_read(MIB_SER_CR) & 0x10000) >> 16))
			break;
	}
	/*make sure write dram correct*/
	wmb();
#if defined(CONFIG_HNAT_V1)
	byt_l = reg_read(MIB_SER_R0);	/* byte cnt bit31~ bit0 */
	byt_h = reg_read(MIB_SER_R1) & 0xffff;	/* byte cnt bit47 ~ bit0 */
	pkt_l = (reg_read(MIB_SER_R1) & 0xffff0000) >> 16;
	pkt_h = reg_read(MIB_SER_R2) & 0xffffff;	/* packet cnt bit39 ~ bit16 */

	*pkt_cnt = (pkt_h << 16) + pkt_l;
	*byte_cnt = (byt_h << 32) + byt_l;
#elif defined(CONFIG_HNAT_V2)

	byt_l = reg_read(MIB_SER_R0);	/* byte cnt bit31~ bit0 */
	byt_h = reg_read(MIB_SER_R1);	/* byte cnt bit63 ~ bit32 */
	pkt_l = reg_read(MIB_SER_R2);	/* packet cnt bit31~ bit0 */
	pkt_h = reg_read(MIB_SER_R3);	/* packet cnt bit63 ~ bit32 */

	*pkt_cnt = (pkt_h << 32) + pkt_l;
	*byte_cnt = (byt_h << 32) + byt_l;

#endif /* CONFIG_HNAT_V2 */
}

void ppe_mib_dump_ppe1(unsigned int entry_num, u64 *pkt_cnt, u64 *byte_cnt)
{
	u32 byt_l = 0;
	u64 byt_h = 0;
	u32 pkt_l = 0;
	u64 pkt_h = 0;
	reg_write(MIB_SER_CR_PPE1, entry_num | (1 << 16));
	while (1) {
		if (!((reg_read(MIB_SER_CR_PPE1) & 0x10000) >> 16))
			break;
	}
	/*make sure write dram correct*/
	wmb();
#if defined(CONFIG_HNAT_V1)
	byt_l = reg_read(MIB_SER_R0_PPE1);	/* byte cnt bit31~ bit0 */
	byt_h = reg_read(MIB_SER_R1_PPE1) & 0xffff;	/* byte cnt bit47 ~ bit0 */
	pkt_l = (reg_read(MIB_SER_R1_PPE1) & 0xffff0000) >> 16;
	pkt_h = reg_read(MIB_SER_R2_PPE1) & 0xffffff;	/* packet cnt bit39 ~ bit16 */

	*pkt_cnt = (pkt_h << 16) + pkt_l;
	*byte_cnt = (byt_h << 32) + byt_l;
#elif defined(CONFIG_HNAT_V2)

	byt_l = reg_read(MIB_SER_R0_PPE1);	/* byte cnt bit31~ bit0 */
	byt_h = reg_read(MIB_SER_R1_PPE1);	/* byte cnt bit63 ~ bit32 */
	pkt_l = reg_read(MIB_SER_R2_PPE1);	/* packet cnt bit31~ bit0 */
	pkt_h = reg_read(MIB_SER_R3_PPE1);	/* packet cnt bit63 ~ bit32 */

	*pkt_cnt = (pkt_h << 32) + pkt_l;
	*byte_cnt = (byt_h << 32) + byt_l;

#endif /* CONFIG_HNAT_V2 */
}

static void ppe_get_mib_by_entry(struct foe_entry *entry, int hash_index, int ppe_index){

	u8 sport, fport, mcast;
	u64 pkt_cnt = 0, byte_cnt = 0;
	struct hnat_mib_struct *mib_entry_p = NULL;


	fport = get_act_dp(entry);
	sport = get_rxif_idx(entry);
	mcast = is_foe_mcast_entry(entry);

	if (ppe_index == 0) {
		ppe_mib_dump_ppe0(hash_index, &pkt_cnt, &byte_cnt);

		if (fe_feature & PPE_MIB)
			mib_entry_p = &hnat_entry_mib[hash_index];

	} else {
		ppe_mib_dump_ppe1(hash_index, &pkt_cnt, &byte_cnt);

		if (fe_feature & PPE_MIB)
			mib_entry_p = &hnat_entry1_mib[hash_index];
	}

	if (fe_feature & PPE_MIB) {
		mib_entry_p->pkt_count += pkt_cnt;
		mib_entry_p->byte_count += byte_cnt;
	}

	if (fport < MAX_IF_NUM) {
		hnat_if[fport].tx_byte_cnt += byte_cnt;
		hnat_if[fport].tx_pkt_cnt += pkt_cnt;
	}

	if (sport < MAX_IF_NUM) {
		hnat_if[sport].rx_byte_cnt += byte_cnt;
		hnat_if[sport].rx_pkt_cnt += pkt_cnt;

		if (mcast)
			hnat_if[sport].rx_mcast_cnt += pkt_cnt;
	}

	if (debug_level >= 5 && sport < MAX_IF_NUM && fport < MAX_IF_NUM) {

		if (fe_feature & PPE_MIB)
			pr_notice("%s(%d)(%d), sport(%d), fport(%d): rx_byte:%llu, rx_pkt:%llu, tx_byte=%llu, tx_pkt=%llu, mcast=%llu, entry_pkt_cnt=%llu, entry_byte_cnt=%llu\n",
				__func__, ppe_index, hash_index, sport, fport, hnat_if[sport].rx_byte_cnt, hnat_if[sport].rx_pkt_cnt,
				hnat_if[fport].tx_byte_cnt, hnat_if[fport].tx_pkt_cnt, hnat_if[sport].rx_mcast_cnt,
				mib_entry_p->pkt_count, mib_entry_p->byte_count);
		else

			pr_notice("%s(%d)(%d), sport(%d), fport(%d): rx_byte:%llu, rx_pkt:%llu, tx_byte=%llu, tx_pkt=%llu, mcast=%llu\n",
				__func__, ppe_index, hash_index, sport, fport, hnat_if[sport].rx_byte_cnt, hnat_if[sport].rx_pkt_cnt,
				hnat_if[fport].tx_byte_cnt, hnat_if[fport].tx_pkt_cnt, hnat_if[sport].rx_mcast_cnt);
	}
}

int ppe_mib_update(void) {


	int hash_index;
	struct foe_entry *entry, *entry1;
	int bind_count = 0;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {

		entry = &ppe_foe_base[hash_index];

		if (entry->bfib1.state == BIND)
			bind_count ++;

		ppe_get_mib_by_entry(entry, hash_index, 0);

		entry1 = &ppe1_foe_base[hash_index];

		if (entry1->bfib1.state == BIND)
			bind_count ++;

		ppe_get_mib_by_entry(entry1, hash_index, 1);

    #ifdef CONFIG_TUNNEL_FAST_PATH
		/* SW age entry when SWNAT */
		hnat_tnl_sw_age_entry(hash_index);
    #endif /* CONFIG_TUNNEL_FAST_PATH */
	}

	return bind_count;
}


static void ppe_mib_update_cycle(struct timer_list *t){


	int bind_count = ppe_mib_update();

	spin_lock(&ppe_mib_timer_lock);
	/* setup next timer */
	if (bind_count > 0) {
		mod_timer(&ppe_mib_timer, jiffies + HZ * PPE_MIB_TIMEOUT);
		ppe_mib_timer_start = true;

		if (debug_level == 10)
 			pr_info("%s, start timer\n", __func__);
	} else {
		ppe_mib_timer_start = false;
		pr_info("%s, timer is stopped\n", __func__);
	}
	spin_unlock(&ppe_mib_timer_lock);

}


void ppe_init_mib_counter(void) {

	/* setup mib timer */
	timer_setup(&ppe_mib_timer, ppe_mib_update_cycle, 0);
	spin_lock_init(&ppe_mib_timer_lock);
}


void ppe_start_mib_timer(void) {

	spin_lock(&ppe_mib_timer_lock);
	if (ppe_mib_counter_en && !ppe_mib_timer_start) {

		/* start the timer to update ccmni mib */
		mod_timer(&ppe_mib_timer, jiffies + HZ * PPE_MIB_TIMEOUT);
		ppe_mib_timer_start = true;

		if (debug_level == 10)
 			pr_info("%s, start timer\n", __func__);
	}
	spin_unlock(&ppe_mib_timer_lock);
}

void ppe_reset_mib_entry(struct hnat_mib_struct *mib_entry_p)
{
	if (fe_feature & PPE_MIB)
		mib_entry_p->pkt_count = mib_entry_p->byte_count = 0;
}

void ppe_reset_dev_mib(struct net_device *dev) {
	int i;

	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			// clear
			hnat_if[i].rx_byte_cnt = 0;
			hnat_if[i].tx_byte_cnt = 0;

			hnat_if[i].rx_pkt_cnt = 0;
			hnat_if[i].tx_pkt_cnt= 0;
			hnat_if[i].rx_mcast_cnt = 0;
			break;
		}
	}
}

void ppe_reset_entry_mib(struct foe_entry *entry) {

	int idx;

	/* no need to reset */
	if (!(fe_feature & PPE_MIB))
		return;

	for (idx = 0; idx < FOE_4TB_SIZ; idx++) {

		if (entry == &ppe_foe_base[idx]) {

			hnat_entry_mib[idx].byte_count = 0;
			hnat_entry_mib[idx].pkt_count = 0;
			break;

		} else if (entry == &ppe1_foe_base[idx]) {

			hnat_entry1_mib[idx].byte_count = 0;
			hnat_entry1_mib[idx].pkt_count = 0;
			break;

		}
	}

}



void ppe_get_mib_by_dev(u32 dev_idx) {

	u8 sport, fport, sport1, fport1;
	int hash_index;
	int mcast, mcast1;
	struct foe_entry *entry, *entry1;
	struct hnat_mib_struct *mib_entry_p;

	u64 pkt_cnt = 0, byte_cnt = 0, pkt_cnt1 = 0, byte_cnt1 = 0;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {

		entry = &ppe_foe_base[hash_index];
		entry1 = &ppe1_foe_base[hash_index];

		fport = get_act_dp(entry);
		sport = get_rxif_idx(entry);
		mcast = is_foe_mcast_entry(entry);

		if (dev_idx == fport || dev_idx == sport) {

			ppe_mib_dump_ppe0(hash_index, &pkt_cnt, &byte_cnt);

			if (dev_idx == fport) {
				hnat_if[dev_idx].tx_pkt_cnt += pkt_cnt;
				hnat_if[dev_idx].tx_byte_cnt += byte_cnt;
			}

			if (dev_idx == sport) {

				hnat_if[dev_idx].rx_pkt_cnt += pkt_cnt;
				hnat_if[dev_idx].rx_byte_cnt += byte_cnt;
				if (mcast)
					hnat_if[dev_idx].rx_mcast_cnt += pkt_cnt;
			}

			if (fe_feature & PPE_MIB) {

				mib_entry_p = &hnat_entry_mib[hash_index];

				mib_entry_p->pkt_count += pkt_cnt;
				mib_entry_p->byte_count += byte_cnt;
			}
		}

		fport1 = get_act_dp(entry1);
		sport1 = get_rxif_idx(entry1);
		mcast1 = is_foe_mcast_entry(entry1);

		if (dev_idx == fport1 || dev_idx == sport1) {

			ppe_mib_dump_ppe1(hash_index, &pkt_cnt1, &byte_cnt1);

			if (dev_idx == fport1) {

				hnat_if[dev_idx].tx_pkt_cnt += pkt_cnt1;
				hnat_if[dev_idx].tx_byte_cnt += byte_cnt1;
			}

			if (dev_idx == sport1) {

				hnat_if[dev_idx].rx_pkt_cnt += pkt_cnt1;
				hnat_if[dev_idx].rx_byte_cnt += byte_cnt1;

				if (mcast1)
					hnat_if[dev_idx].rx_mcast_cnt += pkt_cnt1;
			}

			if (fe_feature & PPE_MIB) {

				mib_entry_p = &hnat_entry1_mib[hash_index];

				mib_entry_p->pkt_count += pkt_cnt1;
				mib_entry_p->byte_count += byte_cnt1;
			}
		}

	}
}


int ppe_get_dev_stats_handler(struct net_device *dev, struct rtnl_link_stats64 *stats) {


	u8 i, dev_idx, match;

	if (debug_level == 1)
		pr_notice("%s, dev = %s\n", __func__, dev->name);

#ifdef CONFIG_TUNNEL_FAST_PATH
	if(strcmp(dev->name, TUNNEL_INTERFACE_NAME) == 0) {
		/* UL: teb tx */
		/* UL pkt cnt */
		current_hnat_tnl_swnat_cnt_ul = get_hnat_tnl_swnat_cnt_ul();
		stats->tx_packets += current_hnat_tnl_swnat_cnt_ul - last_hnat_tnl_swnat_cnt_ul;
		if (debug_level == 1)
			pr_notice("%s, tx_packets += %llu\n", __func__, current_hnat_tnl_swnat_cnt_ul - last_hnat_tnl_swnat_cnt_ul);
		last_hnat_tnl_swnat_cnt_ul = current_hnat_tnl_swnat_cnt_ul;
		/* UL byte cnt */
		current_hnat_tnl_swnat_byte_cnt_ul = get_hnat_tnl_swnat_byte_cnt_ul();
		stats->tx_bytes += current_hnat_tnl_swnat_byte_cnt_ul - last_hnat_tnl_swnat_byte_cnt_ul;
		if (debug_level == 1)
			pr_notice("%s, tx_bytes += %llu\n", __func__, current_hnat_tnl_swnat_byte_cnt_ul - last_hnat_tnl_swnat_byte_cnt_ul);
		last_hnat_tnl_swnat_byte_cnt_ul = current_hnat_tnl_swnat_byte_cnt_ul;

		/* DL: teb rx */
		/* DL pkt cnt */
		current_hnat_tnl_swnat_cnt_dl = get_hnat_tnl_swnat_cnt_dl();
		stats->rx_packets += current_hnat_tnl_swnat_cnt_dl - last_hnat_tnl_swnat_cnt_dl;
		if (debug_level == 1)
			pr_notice("%s, rx_packets += %llu\n", __func__, current_hnat_tnl_swnat_cnt_dl - last_hnat_tnl_swnat_cnt_dl);
		last_hnat_tnl_swnat_cnt_dl = current_hnat_tnl_swnat_cnt_dl;
		/* DL byte cnt */
		current_hnat_tnl_swnat_byte_cnt_dl = get_hnat_tnl_swnat_byte_cnt_dl();
		stats->rx_bytes += current_hnat_tnl_swnat_byte_cnt_dl - last_hnat_tnl_swnat_byte_cnt_dl;
		if (debug_level == 1)
			pr_notice("%s, rx_bytes += %llu\n", __func__, current_hnat_tnl_swnat_byte_cnt_dl - last_hnat_tnl_swnat_byte_cnt_dl);
		last_hnat_tnl_swnat_byte_cnt_dl = current_hnat_tnl_swnat_byte_cnt_dl;

		return 1; /* succeed */
	}
#endif /* CONFIG_TUNNEL_FAST_PATH */

	dev_idx = 1;
	match = 0;
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			dev_idx = i;
			match = 1;
			break;
		}

	}
	if (match == 0) {
		stats->rx_packets = 0;
		stats->tx_packets = 0;
		stats->rx_bytes = 0;
		stats->tx_bytes = 0;
		stats->multicast = 0;
		return 1; /* succeed */
	}

	ppe_get_mib_by_dev(dev_idx);

	stats->rx_packets += hnat_if[dev_idx].rx_pkt_cnt;
	stats->tx_packets += hnat_if[dev_idx].tx_pkt_cnt;
	stats->rx_bytes += hnat_if[dev_idx].rx_byte_cnt;
	stats->tx_bytes += hnat_if[dev_idx].tx_byte_cnt;
	stats->multicast += hnat_if[dev_idx].rx_mcast_cnt;

	//reset counter
	hnat_if[dev_idx].rx_pkt_cnt = 0;
	hnat_if[dev_idx].tx_pkt_cnt = 0;
	hnat_if[dev_idx].rx_byte_cnt = 0;
	hnat_if[dev_idx].tx_byte_cnt = 0;
	hnat_if[dev_idx].rx_mcast_cnt = 0;

	if (debug_level >= 5)
		pr_notice("%s, if(%u):%s: rx_pkt:%llu, tx_pkt:%llu, rx_byte=%llu, tx_byte=%llu, mcast:%llu\n",
			__func__, dev_idx, dev->name, stats->rx_packets, stats->tx_packets,
			stats->rx_bytes, stats->tx_bytes, stats->multicast);

	return 1; /* succeed */

}

int ppe_get_conn_stats_handler(struct hnat_flow_tuple *tuple, u64 *pkt_cnt, u64 *byte_cnt) {

	u32 hash_index = -1;
	int idx;
	struct foe_entry *entry = NULL;
	struct hnat_mib_struct *mib_entry_p = NULL;

	if ((fe_feature & PPE_MIB) == 0){
		pr_notice("%s, PPE_MIB isn't enabled in fe_feature:%d\n", __func__, fe_feature);
		return 0; /* fail */
	}

#ifdef CONFIG_MTK_NAT46_UTIL
	if (convert_addr_4to6(tuple->sipv4,  tuple->dipv4,
		&tuple->sipv6_0, &tuple->sipv6_1, &tuple->sipv6_2, &tuple->sipv6_3,
		&tuple->dipv6_0, &tuple->dipv6_1, &tuple->dipv6_2, &tuple->dipv6_3)) {
		tuple->pkt_type = IPV6_5T;
		if (debug_level >= 5)
			pr_notice("%s, convert src=%x, dst=%x to src=%x:%x:%x:%x, dst=%x:%x:%x:%x\n",
					__func__,
					tuple->sipv4, tuple->dipv4,
					tuple->sipv6_0, tuple->sipv6_1, tuple->sipv6_2, tuple->sipv6_3,
					tuple->dipv6_0, tuple->dipv6_1, tuple->dipv6_2, tuple->dipv6_3);
	}
#endif /* CONFIG_MTK_NAT46_UTIL */

	/* get hash index */
	if (tuple->pkt_type == IPV4_5T)
		hash_index = get_hash_ipv4(tuple->sipv4, tuple->dipv4, tuple->sp, tuple->dp);
	else if (tuple->pkt_type == IPV6_5T)
		hash_index = get_hash_ipv6(tuple->sipv6_0, tuple->sipv6_1,
			tuple->sipv6_2, tuple->sipv6_3, tuple->dipv6_0,
			tuple->dipv6_1, tuple->dipv6_2, tuple->dipv6_3,
			tuple->sp, tuple->dp);
	else
		return 0; /* fail */

	if (hash_index >= FOE_4TB_SIZ)
		return 0; /* fail */


	for (idx = 0; idx < 2; idx ++) {

		if (idx == 0) {
			entry = &ppe_foe_base[hash_index];
			mib_entry_p =  &hnat_entry_mib[hash_index];
		} else {
			entry = &ppe1_foe_base[hash_index];
			mib_entry_p =  &hnat_entry1_mib[hash_index];
		}

		if (check_flow_match(tuple, entry)) {

			ppe_get_mib_by_entry(entry, hash_index, idx);

			/* nf_conntrack needs IP frame size, not ETH frame size */
			*byte_cnt = mib_entry_p->byte_count - ETH_HLEN * mib_entry_p->pkt_count;
			*pkt_cnt = mib_entry_p->pkt_count;

			return 1; /* succeed */
		}
	}

	if (debug_level >= 5)
		pr_notice("%s, no match flow\n", __func__);

	return 0; /* fail */
}

#define DD \
{\
pr_notice("%s %d\n", __func__, __LINE__); \
}

/* 4          2         0 */
/* +----------+---------+ */
/* |      DMAC[47:16]   | */
/* +--------------------+ */
/* |DMAC[15:0]| 2nd VID | */
/* +----------+---------+ */
/* 4          2         0 */
/* +----------+---------+ */
/* |      SMAC[47:16]   | */
/* +--------------------+ */
/* |SMAC[15:0]| PPPOE ID| */
/* +----------+---------+ */
/* Ex: */
/* Mac=01:22:33:44:55:66 */
/* 4          2         0 */
/* +----------+---------+ */
/* |     01:22:33:44    | */
/* +--------------------+ */
/* |  55:66   | PPPOE ID| */
/* +----------+---------+ */
#if defined(CONFIG_RA_HW_NAT_IPV6)

u32 get_hash_ipv6(u32 sip0, u32 sip1, u32 sip2, u32 sip3,
	u32 dip0, u32 dip1, u32 dip2, u32 dip3, u32 sport, u32 dport) {

	u32 t_hvt_31, t_hvt_63, t_hvt_95, t_hvt_sd;
	u32 t_hvt_sd_23, t_hvt_sd_31_24, t_hash_32, t_hashs_16, t_ha16k, hash_index;
	u32 ipv6_sip_127_96, ipv6_sip_95_64, ipv6_sip_63_32, ipv6_sip_31_0;
	u32 ipv6_dip_127_96, ipv6_dip_95_64, ipv6_dip_63_32, ipv6_dip_31_0;

	ipv6_sip_127_96 = sip0;
	ipv6_sip_95_64 = sip1;
	ipv6_sip_63_32 = sip2;
	ipv6_sip_31_0 = sip3;
	ipv6_dip_127_96 = dip0;
	ipv6_dip_95_64 = dip1;
	ipv6_dip_63_32 = dip2;
	ipv6_dip_31_0 = dip3;

	t_hvt_31 = ipv6_sip_31_0 ^ ipv6_dip_31_0 ^ (sport << 16 | dport);
	t_hvt_63 = ipv6_sip_63_32 ^ ipv6_dip_63_32 ^ ipv6_dip_127_96;
	t_hvt_95 = ipv6_sip_95_64 ^ ipv6_dip_95_64 ^ ipv6_sip_127_96;
	if (DFL_FOE_HASH_MODE == 1)	/* hash mode 1 */
		t_hvt_sd = (t_hvt_31 & t_hvt_63) | ((~t_hvt_31) & t_hvt_95);
	else                            /* hash mode 2 */
		t_hvt_sd = t_hvt_63 ^ (t_hvt_31 & (~t_hvt_95));

	t_hvt_sd_23 = t_hvt_sd & 0xffffff;
	t_hvt_sd_31_24 = t_hvt_sd & 0xff000000;
	t_hash_32 = t_hvt_31 ^ t_hvt_63 ^ t_hvt_95 ^ ((t_hvt_sd_23 << 8) | (t_hvt_sd_31_24 >> 24));
	t_hashs_16 = ((t_hash_32 & 0xffff0000) >> 16) ^ (t_hash_32 & 0xfffff);

	if (FOE_4TB_SIZ == 32768 || FOE_4TB_SIZ == 16384)
		t_ha16k = t_hashs_16 & 0x1fff;  /* FOE_16k */
	else if (FOE_4TB_SIZ == 8192)
		t_ha16k = t_hashs_16 & 0xfff;  /* FOE_8k */
	else if (FOE_4TB_SIZ == 4096)
		t_ha16k = t_hashs_16 & 0x7ff;  /* FOE_4k */
	else if (FOE_4TB_SIZ == 2048)
		t_ha16k = t_hashs_16 & 0x3ff;  /* FOE_2k */
	else
		t_ha16k = t_hashs_16 & 0x1ff;  /* FOE_1k */
	hash_index = (u32)t_ha16k * 4;

	if (debug_level >= 5)
		pr_info("%s, saddr = %x:%x:%x:%x, daddr=%x:%x:%x:%x, sport=%d, dport=%d, hash_index=%d\n",
			__func__, sip0, sip1, sip2, sip3, dip0, dip1, dip2, dip3,
			sport, dport, hash_index);

	return hash_index;
}


int hash_ipv6(struct foe_pri_key *key, struct foe_entry *entry, int del)
{
	u32 hash_index;
	u32 ppe_saddr_127_96, ppe_saddr_95_64, ppe_saddr_63_32, ppe_saddr_31_0;
	u32 ppe_daddr_127_96, ppe_daddr_95_64, ppe_daddr_63_32, ppe_daddr_31_0;
	u32 ipv6_sip_127_96, ipv6_sip_95_64, ipv6_sip_63_32, ipv6_sip_31_0;
	u32 ipv6_dip_127_96, ipv6_dip_95_64, ipv6_dip_63_32, ipv6_dip_31_0;
	u32 sport, dport, ppe_sportv6, ppe_dportv6;

	ipv6_sip_127_96 = key->ipv6_routing.sip0;
	ipv6_sip_95_64 = key->ipv6_routing.sip1;
	ipv6_sip_63_32 = key->ipv6_routing.sip2;
	ipv6_sip_31_0 = key->ipv6_routing.sip3;
	ipv6_dip_127_96 = key->ipv6_routing.dip0;
	ipv6_dip_95_64 = key->ipv6_routing.dip1;
	ipv6_dip_63_32 = key->ipv6_routing.dip2;
	ipv6_dip_31_0 = key->ipv6_routing.dip3;
	sport = key->ipv6_routing.sport;
	dport = key->ipv6_routing.dport;

	hash_index = get_hash_ipv6(
		key->ipv6_routing.sip0, key->ipv6_routing.sip1,
		key->ipv6_routing.sip2, key->ipv6_routing.sip3,
		key->ipv6_routing.dip0, key->ipv6_routing.dip1,
		key->ipv6_routing.dip2, key->ipv6_routing.dip3, sport, dport);

	entry = &ppe_foe_base[hash_index];
	ppe_saddr_127_96 = entry->ipv6_5t_route.ipv6_sip0;
	ppe_saddr_95_64 = entry->ipv6_5t_route.ipv6_sip1;
	ppe_saddr_63_32 = entry->ipv6_5t_route.ipv6_sip2;
	ppe_saddr_31_0 = entry->ipv6_5t_route.ipv6_sip3;

	ppe_daddr_127_96 = entry->ipv6_5t_route.ipv6_dip0;
	ppe_daddr_95_64 = entry->ipv6_5t_route.ipv6_dip1;
	ppe_daddr_63_32 = entry->ipv6_5t_route.ipv6_dip2;
	ppe_daddr_31_0 = entry->ipv6_5t_route.ipv6_dip3;

	ppe_sportv6 = entry->ipv6_5t_route.sport;
	ppe_dportv6 = entry->ipv6_5t_route.dport;
	if (del != 1) {
		if (entry->ipv6_5t_route.bfib1.state == BIND) {
			pr_notice("IPV6 Hash collision, hash index +1\n");
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
		}
		if (entry->ipv6_5t_route.bfib1.state == BIND) {
			pr_notice("IPV6 Hash collision can not bind\n");
			return -1;
		}
	} else if (del == 1) {
		if ((ipv6_sip_127_96 == ppe_saddr_127_96) && (ipv6_sip_95_64 == ppe_saddr_95_64) &&
		    (ipv6_sip_63_32 == ppe_saddr_63_32) && (ipv6_sip_31_0 == ppe_saddr_31_0) &&
		    (ipv6_dip_127_96 == ppe_daddr_127_96) && (ipv6_dip_95_64 == ppe_daddr_95_64) &&
		    (ipv6_dip_63_32 == ppe_daddr_63_32) && (ipv6_dip_31_0 == ppe_daddr_31_0) &&
		    (sport == ppe_sportv6) && (dport == ppe_dportv6)) {
		} else {
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
			ppe_saddr_127_96 = entry->ipv6_5t_route.ipv6_sip0;
			ppe_saddr_95_64 = entry->ipv6_5t_route.ipv6_sip1;
			ppe_saddr_63_32 = entry->ipv6_5t_route.ipv6_sip2;
			ppe_saddr_31_0 = entry->ipv6_5t_route.ipv6_sip3;

			ppe_daddr_127_96 = entry->ipv6_5t_route.ipv6_dip0;
			ppe_daddr_95_64 = entry->ipv6_5t_route.ipv6_dip1;
			ppe_daddr_63_32 = entry->ipv6_5t_route.ipv6_dip2;
			ppe_daddr_31_0 = entry->ipv6_5t_route.ipv6_dip3;

			ppe_sportv6 = entry->ipv6_5t_route.sport;
			ppe_dportv6 = entry->ipv6_5t_route.dport;
			if ((ipv6_sip_127_96 == ppe_saddr_127_96) && (ipv6_sip_95_64 == ppe_saddr_95_64) &&
			    (ipv6_sip_63_32 == ppe_saddr_63_32) && (ipv6_sip_31_0 == ppe_saddr_31_0) &&
			    (ipv6_dip_127_96 == ppe_daddr_127_96) && (ipv6_dip_95_64 == ppe_daddr_95_64) &&
			    (ipv6_dip_63_32 == ppe_daddr_63_32) && (ipv6_dip_31_0 == ppe_daddr_31_0) &&
			    (sport == ppe_sportv6) && (dport == ppe_dportv6)) {
			} else {
				if (fe_feature & SEMI_AUTO_MODE)
					pr_notice("Ipv6 Entry delete : Entry Not found\n");
				else if (fe_feature & MANUAL_MODE)
					pr_notice("Ipv6 hash collision hwnat can not found\n");
				return -1;
			}
		}
	}
	return hash_index;
}

int hash_mib_ipv6(struct foe_pri_key *key, struct foe_entry *entry)
{
	u32 hash_index;
	u32 ppe_saddr_127_96, ppe_saddr_95_64, ppe_saddr_63_32, ppe_saddr_31_0;
	u32 ppe_daddr_127_96, ppe_daddr_95_64, ppe_daddr_63_32, ppe_daddr_31_0;
	u32 ipv6_sip_127_96, ipv6_sip_95_64, ipv6_sip_63_32, ipv6_sip_31_0;
	u32 ipv6_dip_127_96, ipv6_dip_95_64, ipv6_dip_63_32, ipv6_dip_31_0;
	u32 sport, dport, ppe_sportv6, ppe_dportv6;

	ipv6_sip_127_96 = key->ipv6_routing.sip0;
	ipv6_sip_95_64 = key->ipv6_routing.sip1;
	ipv6_sip_63_32 = key->ipv6_routing.sip2;
	ipv6_sip_31_0 = key->ipv6_routing.sip3;
	ipv6_dip_127_96 = key->ipv6_routing.dip0;
	ipv6_dip_95_64 = key->ipv6_routing.dip1;
	ipv6_dip_63_32 = key->ipv6_routing.dip2;
	ipv6_dip_31_0 = key->ipv6_routing.dip3;
	sport = key->ipv6_routing.sport;
	dport = key->ipv6_routing.dport;

	hash_index = get_hash_ipv6(
		key->ipv6_routing.sip0, key->ipv6_routing.sip1,
		key->ipv6_routing.sip2, key->ipv6_routing.sip3,
		key->ipv6_routing.dip0, key->ipv6_routing.dip1,
		key->ipv6_routing.dip2, key->ipv6_routing.dip3, sport, dport);

	entry = &ppe_foe_base[hash_index];
	ppe_saddr_127_96 = entry->ipv6_5t_route.ipv6_sip0;
	ppe_saddr_95_64 = entry->ipv6_5t_route.ipv6_sip1;
	ppe_saddr_63_32 = entry->ipv6_5t_route.ipv6_sip2;
	ppe_saddr_31_0 = entry->ipv6_5t_route.ipv6_sip3;

	ppe_daddr_127_96 = entry->ipv6_5t_route.ipv6_dip0;
	ppe_daddr_95_64 = entry->ipv6_5t_route.ipv6_dip1;
	ppe_daddr_63_32 = entry->ipv6_5t_route.ipv6_dip2;
	ppe_daddr_31_0 = entry->ipv6_5t_route.ipv6_dip3;

	ppe_sportv6 = entry->ipv6_5t_route.sport;
	ppe_dportv6 = entry->ipv6_5t_route.dport;

	if ((ipv6_sip_127_96 == ppe_saddr_127_96) && (ipv6_sip_95_64 == ppe_saddr_95_64) &&
	    (ipv6_sip_63_32 == ppe_saddr_63_32) && (ipv6_sip_31_0 == ppe_saddr_31_0) &&
	    (ipv6_dip_127_96 == ppe_daddr_127_96) && (ipv6_dip_95_64 == ppe_daddr_95_64) &&
	    (ipv6_dip_63_32 == ppe_daddr_63_32) && (ipv6_dip_31_0 == ppe_daddr_31_0) &&
	    (sport == ppe_sportv6) && (dport == ppe_dportv6)) {
		if (debug_level >= 1)
			pr_notice("mib: ipv6 entry found entry idx = %d\n", hash_index);
	} else {
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
			ppe_saddr_127_96 = entry->ipv6_5t_route.ipv6_sip0;
			ppe_saddr_95_64 = entry->ipv6_5t_route.ipv6_sip1;
			ppe_saddr_63_32 = entry->ipv6_5t_route.ipv6_sip2;
			ppe_saddr_31_0 = entry->ipv6_5t_route.ipv6_sip3;

			ppe_daddr_127_96 = entry->ipv6_5t_route.ipv6_dip0;
			ppe_daddr_95_64 = entry->ipv6_5t_route.ipv6_dip1;
			ppe_daddr_63_32 = entry->ipv6_5t_route.ipv6_dip2;
			ppe_daddr_31_0 = entry->ipv6_5t_route.ipv6_dip3;

			ppe_sportv6 = entry->ipv6_5t_route.sport;
			ppe_dportv6 = entry->ipv6_5t_route.dport;
			if ((ipv6_sip_127_96 == ppe_saddr_127_96) && (ipv6_sip_95_64 == ppe_saddr_95_64) &&
			    (ipv6_sip_63_32 == ppe_saddr_63_32) && (ipv6_sip_31_0 == ppe_saddr_31_0) &&
			    (ipv6_dip_127_96 == ppe_daddr_127_96) && (ipv6_dip_95_64 == ppe_daddr_95_64) &&
			    (ipv6_dip_63_32 == ppe_daddr_63_32) && (ipv6_dip_31_0 == ppe_daddr_31_0) &&
			    (sport == ppe_sportv6) && (dport == ppe_dportv6)) {
				if (debug_level >= 1)
					pr_notice("mib: ipv6 entry found entry idx = %d\n", hash_index);
			} else {
				if (debug_level >= 1)
					pr_notice("mib: ipv6 entry not found\n");
				return -1;
			}
	}

	return hash_index;
}
#endif

u32 get_hash_ipv4(u32 saddr, u32 daddr, u32 sport, u32 dport) {

	u32 t_hvt_31;
	u32 t_hvt_63;
	u32 t_hvt_95;
	u32 t_hvt_sd;

	u32 t_hvt_sd_23;
	u32 t_hvt_sd_31_24;
	u32 t_hash_32;
	u32 t_hashs_16;
	u32 t_ha16k;

	u32 hash_index;

	t_hvt_31 = sport << 16 | dport;
	t_hvt_63 = daddr;
	t_hvt_95 = saddr;

	if (DFL_FOE_HASH_MODE == 1)	/* hash mode 1 */
		t_hvt_sd = (t_hvt_31 & t_hvt_63) | ((~t_hvt_31) & t_hvt_95);
	else                            /* hash mode 2 */
		t_hvt_sd = t_hvt_63 ^ (t_hvt_31 & (~t_hvt_95));

	t_hvt_sd_23 = t_hvt_sd & 0xffffff;
	t_hvt_sd_31_24 = t_hvt_sd & 0xff000000;
	t_hash_32 = t_hvt_31 ^ t_hvt_63 ^ t_hvt_95 ^ ((t_hvt_sd_23 << 8) | (t_hvt_sd_31_24 >> 24));
	t_hashs_16 = ((t_hash_32 & 0xffff0000) >> 16) ^ (t_hash_32 & 0xfffff);

	if (FOE_4TB_SIZ == 32768 || FOE_4TB_SIZ == 16384)
		t_ha16k = t_hashs_16 & 0x1fff;  /* FOE_16k */
	else if (FOE_4TB_SIZ == 8192)
		t_ha16k = t_hashs_16 & 0xfff;  /* FOE_8k */
	else if (FOE_4TB_SIZ == 4096)
		t_ha16k = t_hashs_16 & 0x7ff;  /* FOE_4k */
	else if (FOE_4TB_SIZ == 2048)
		t_ha16k = t_hashs_16 & 0x3ff;  /* FOE_2k */
	else
		t_ha16k = t_hashs_16 & 0x1ff;  /* FOE_1k */

	hash_index = (u32)t_ha16k * 4;


	if (debug_level >= 5)
		pr_info("%s, saddr = %pI4h, daddr=%pI4h, sport=%d, dport=%d, hash_index=%d\n",
			__func__, &saddr, &daddr, sport, dport, hash_index);

	return hash_index;
}

int hash_ipv4(struct foe_pri_key *key, struct foe_entry *entry, int del)
{
	u32 hash_index;
	u32 ppe_saddr, ppe_daddr, ppe_sport, ppe_dport, saddr, daddr, sport, dport;

	saddr = key->ipv4_hnapt.sip;
	daddr = key->ipv4_hnapt.dip;
	sport = key->ipv4_hnapt.sport;
	dport = key->ipv4_hnapt.dport;

	hash_index = get_hash_ipv4(saddr, daddr, sport, dport);

	entry = &ppe_foe_base[hash_index];
	ppe_saddr = entry->ipv4_hnapt.sip;
	ppe_daddr = entry->ipv4_hnapt.dip;
	ppe_sport = entry->ipv4_hnapt.sport;
	ppe_dport = entry->ipv4_hnapt.dport;

	if (del != 1) {
		if (entry->ipv4_hnapt.bfib1.state == BIND) {
			pr_notice("Hash collision, hash index +1\n");
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
		}
		if (entry->ipv4_hnapt.bfib1.state == BIND) {
			pr_notice("Hash collision can not bind\n");
			return -1;
		}
	} else if (del == 1) {
		if ((saddr == ppe_saddr) && (daddr == ppe_daddr) &&
		    (sport == ppe_sport) && (dport == ppe_dport)) {
		} else {
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
			ppe_saddr = entry->ipv4_hnapt.sip;
			ppe_daddr = entry->ipv4_hnapt.dip;
			ppe_sport = entry->ipv4_hnapt.sport;
			ppe_dport = entry->ipv4_hnapt.dport;
			if ((saddr == ppe_saddr) && (daddr == ppe_daddr) &&
			    (sport == ppe_sport) && (dport == ppe_dport)) {
			} else {
				if (fe_feature & SEMI_AUTO_MODE)
					pr_notice("hash collision hwnat can not found\n");
				else if (fe_feature & MANUAL_MODE)
					pr_notice("Entry delete : Entry Not found\n");

				return -1;
			}
		}
	}
	return hash_index;
}

int hash_mib_ipv4(struct foe_pri_key *key, struct foe_entry *entry)
{
	u32 hash_index;
	u32 ppe_saddr, ppe_daddr, ppe_sport, ppe_dport, saddr, daddr, sport, dport;

	saddr = key->ipv4_hnapt.sip;
	daddr = key->ipv4_hnapt.dip;
	sport = key->ipv4_hnapt.sport;
	dport = key->ipv4_hnapt.dport;

	hash_index = get_hash_ipv4(saddr, daddr, sport, dport);

	entry = &ppe_foe_base[hash_index];
	ppe_saddr = entry->ipv4_hnapt.sip;
	ppe_daddr = entry->ipv4_hnapt.dip;
	ppe_sport = entry->ipv4_hnapt.sport;
	ppe_dport = entry->ipv4_hnapt.dport;

	if ((saddr == ppe_saddr) && (daddr == ppe_daddr) &&
	    (sport == ppe_sport) && (dport == ppe_dport)) {
		if (debug_level >= 1)
			pr_notice("mib: ipv4 entry entry : %d\n", hash_index);
	} else {
			hash_index = hash_index + 1;
			entry = &ppe_foe_base[hash_index];
			ppe_saddr = entry->ipv4_hnapt.sip;
			ppe_daddr = entry->ipv4_hnapt.dip;
			ppe_sport = entry->ipv4_hnapt.sport;
			ppe_dport = entry->ipv4_hnapt.dport;
			if ((saddr == ppe_saddr) && (daddr == ppe_daddr) &&
			    (sport == ppe_sport) && (dport == ppe_dport)) {
				if (debug_level >= 1)
					pr_notice("mib: ipv4 entry entry : %d\n", hash_index);
			} else {
				if (debug_level >= 1)
					pr_notice("mib: ipv4 entry not found\n");
				return -1;
			}
			return hash_index;
	}

	return hash_index;
}

int get_ppe_entry_idx(struct foe_pri_key *key, struct foe_entry *entry, int del)
{
	if ((key->pkt_type) == IPV4_NAPT)
		return hash_ipv4(key, entry, del);
#if defined(CONFIG_RA_HW_NAT_IPV6)
	else if ((key->pkt_type) == IPV6_ROUTING)
		return hash_ipv6(key, entry, del);
#endif
	else
		return -1;
}

int get_mib_entry_idx(struct foe_pri_key *key, struct foe_entry *entry)
{
	if ((key->pkt_type) == IPV4_NAPT)
		return hash_mib_ipv4(key, entry);
#if defined(CONFIG_RA_HW_NAT_IPV6)
	else if ((key->pkt_type) == IPV6_ROUTING)
		return hash_mib_ipv6(key, entry);
#endif
	else
		return -1;
}
EXPORT_SYMBOL(get_mib_entry_idx);

void foe_set_mac_hi_info(u8 *dst, uint8_t *src)
{
	dst[3] = src[0];
	dst[2] = src[1];
	dst[1] = src[2];
	dst[0] = src[3];
}

void foe_set_mac_lo_info(u8 *dst, uint8_t *src)
{
	dst[1] = src[4];
	dst[0] = src[5];
}

void foe_revert_mac_hi_info(u8 *dst, uint8_t *src)
{
	dst[0] = src[3];
	dst[1] = src[2];
	dst[2] = src[1];
	dst[3] = src[0];
}

void foe_revert_mac_lo_info(u8 *dst, uint8_t *src)
{
	dst[4] = src[1];
	dst[5] = src[0];
}


static int is_request_done(void)
{
	int count = 1000;

	/* waiting for 1sec to make sure action was finished */
	do {
		if (((reg_read(CAH_CTRL) >> 8) & 0x1) == 0)
			return 1;
		usleep_range(1000, 1100);
	} while (--count);

	return 0;
}

#if defined(CONFIG_HNAT_V1)
#define MAX_CACHE_LINE_NUM		32
#elif defined(CONFIG_HNAT_V2)
#define MAX_CACHE_LINE_NUM		128
#endif /* CONFIG_HNAT_V2 */

int foe_dump_cache_entry(void)
{
	int line = 0;
	int state = 0;
	int tag = 0;
	int cah_en = 0;
	int i = 0;

	pr_notice("foe_dump_cache_entry!!!!\n");
	cah_en = reg_read(CAH_CTRL) & 0x1;

	if (!cah_en) {
		pr_notice("Cache is not enabled\n");
		return 0;
	}

	/* disable scan mode */
	reg_modify_bits(PPE_FOE_CFG, 0, 16, 2);

	/* cache disable */
	reg_modify_bits(CAH_CTRL, 0, 0, 1);

	pr_notice(" No--|---State---|----Tag-----\n");
	pr_notice("-----+-----------+------------\n");
	for (line = 0; line < MAX_CACHE_LINE_NUM; line++) {
		/* set line number */
		reg_modify_bits(CAH_LINE_RW, line, 0, 15);

		/* OFFSET_RW = 0x1F (Get Entry Number) */
		reg_modify_bits(CAH_LINE_RW, 0x1F, 16, 8);

		/* software access cache command = read */
		reg_modify_bits(CAH_CTRL, 2, 12, 2);

		/* trigger software access cache request */
		reg_modify_bits(CAH_CTRL, 1, 8, 1);

		if (is_request_done()) {
			tag = (reg_read(CAH_RDATA) & 0xFFFF);
			state = ((reg_read(CAH_RDATA) >> 16) & 0x3);
			pr_notice("%04d | %s   | %05d\n", line,
				 (state == 3) ? " Lock  " :
				 (state == 2) ? " Dirty " :
				 (state == 1) ? " Valid " : "Invalid", tag);
		} else {
			pr_notice("%s is timeout (%d)\n", __func__, line);
		}

		/* software access cache command = read */
		reg_modify_bits(CAH_CTRL, 3, 12, 2);

		reg_write(CAH_WDATA, 0);

		/* trigger software access cache request */
		reg_modify_bits(CAH_CTRL, 1, 8, 1);

		if (!is_request_done())
			pr_notice("%s is timeout (%d)\n", __func__, line);
		/* dump first 16B for each foe entry */
		pr_notice("==========<Flow Table Entry=%d >===============\n", tag);
		for (i = 0; i < 16; i++) {
			reg_modify_bits(CAH_LINE_RW, i, 16, 8);

			/* software access cache command = read */
			reg_modify_bits(CAH_CTRL, 2, 12, 2);

			/* trigger software access cache request */
			reg_modify_bits(CAH_CTRL, 1, 8, 1);

			if (is_request_done())
				pr_notice("%02d  %08X\n", i, reg_read(CAH_RDATA));
			else
				pr_notice("%s is timeout (%d)\n", __func__, line);

			/* software access cache command = write */
			reg_modify_bits(CAH_CTRL, 3, 12, 2);

			reg_write(CAH_WDATA, 0);

			/* trigger software access cache request */
			reg_modify_bits(CAH_CTRL, 1, 8, 1);

			if (!is_request_done())
				pr_notice("%s is timeout (%d)\n", __func__, line);
		}
		pr_notice("=========================================\n");
	}

	/* clear cache table before enabling cache */
	reg_modify_bits(CAH_CTRL, 1, 9, 1);
	reg_modify_bits(CAH_CTRL, 0, 9, 1);

	/* cache enable */
	reg_modify_bits(CAH_CTRL, 1, 0, 1);

	/* enable scan mode */
	reg_modify_bits(PPE_FOE_CFG, 2, 16, 2);

	return 1;
}

int ppe_force_port(struct foe_entry *entry)
{
	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry)) {
#if defined(CONFIG_HNAT_V1)
		return (entry->ipv4_hnapt.info_blk2 >> 5 & 0x7) + (((entry->ipv4_hnapt.info_blk2 >> 14) & 0x1) << 3);
#elif defined (CONFIG_HNAT_V2)
		return (entry->ipv4_hnapt.info_blk2 >> 9 & 0xf);
#else
		return (entry->ipv4_hnapt.info_blk2 >> 5 & 0x7);
#endif
	} else {
#if defined(CONFIG_HNAT_V1)
		return (entry->ipv6_5t_route.info_blk2 >> 5 & 0x7) + (((entry->ipv6_5t_route.info_blk2 >> 14) & 0x1) << 3);
#elif defined (CONFIG_HNAT_V2)
		return (entry->ipv6_5t_route.info_blk2 >> 9 & 0xf);
#else
		return (entry->ipv6_5t_route.info_blk2 >> 5 & 0x7);
#endif
	}
}

int ppe_qid(struct foe_entry *entry)
{
	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry)) {
#if defined(CONFIG_HNAT_V1)
		return (entry->ipv4_hnapt.iblk2.qid +
				  ((entry->ipv4_hnapt.iblk2.qid1 & 0x03) << 4));
#elif defined (CONFIG_HNAT_V2)
		return (entry->ipv4_hnapt.iblk2.qid);
#else
		return (entry->ipv4_hnapt.iblk2.qid);
#endif
	} else {
#if defined(CONFIG_HNAT_V1)
		return (entry->ipv6_5t_route.iblk2.qid + ((entry->ipv6_5t_route.iblk2.qid1 & 0x03) << 4));
#elif defined (CONFIG_HNAT_V2)
		return (entry->ipv6_5t_route.iblk2.qid);
#else
		return (entry->ipv6_5t_route.iblk2.qid);
#endif
	}
}

int ppe_fqos(struct foe_entry *entry)
{
	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry))
		return entry->ipv4_hnapt.iblk2.fqos;
	else
		return entry->ipv6_5t_route.iblk2.fqos;
}

int info_blk2(struct foe_entry *entry)
{
	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry))
		return entry->ipv4_hnapt.info_blk2;
	else
		return entry->ipv6_5t_route.info_blk2;
}
void foe_dump_entry(uint32_t index, struct foe_entry *entry, uint32_t ppe_index)
{
	u32 i = 0;
	u32 print_cnt;
	u32 *p = (uint32_t *)entry;

	NAT_PRINT("==========<Flow Table(%d) Entry=%d (%p)>===============\n", ppe_index, index, entry);
	if (debug_level >= 2) {
		print_cnt = 24;
		for (i = 0; i < print_cnt; i++)
			NAT_PRINT("%02d: %08X\n", i, *(p + i));
	}
	NAT_PRINT("-----------------<Flow Info>------------------\n");
	NAT_PRINT("Information Block 1: %08X\n", entry->ipv4_hnapt.info_blk1);
	NAT_PRINT("Information Block 2=%x (FP=%d FQOS=%d QID=%d)",
				  info_blk2(entry),
				  ppe_force_port(entry),
				  ppe_fqos(entry),ppe_qid(entry));

	if (IS_IPV4_HNAPT(entry)) {
		NAT_PRINT("Create IPv4 HNAPT entry\n");
		NAT_PRINT
		    ("IPv4 Org IP/Port: %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_hnapt.sip), IP_FORMAT2(entry->ipv4_hnapt.sip),
		     IP_FORMAT1(entry->ipv4_hnapt.sip), IP_FORMAT0(entry->ipv4_hnapt.sip),
		     entry->ipv4_hnapt.sport,
		     IP_FORMAT3(entry->ipv4_hnapt.dip), IP_FORMAT2(entry->ipv4_hnapt.dip),
		     IP_FORMAT1(entry->ipv4_hnapt.dip), IP_FORMAT0(entry->ipv4_hnapt.dip),
		     entry->ipv4_hnapt.dport);
		NAT_PRINT
		    ("IPv4 New IP/Port: %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_hnapt.new_sip), IP_FORMAT2(entry->ipv4_hnapt.new_sip),
		     IP_FORMAT1(entry->ipv4_hnapt.new_sip), IP_FORMAT0(entry->ipv4_hnapt.new_sip),
		     entry->ipv4_hnapt.new_sport,
		     IP_FORMAT3(entry->ipv4_hnapt.new_dip), IP_FORMAT2(entry->ipv4_hnapt.new_dip),
		     IP_FORMAT1(entry->ipv4_hnapt.new_dip), IP_FORMAT0(entry->ipv4_hnapt.new_dip),
		     entry->ipv4_hnapt.new_dport);

	} else if (IS_IPV4_HNAT(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv4_hnapt.info_blk2);
		NAT_PRINT("Create IPv4 HNAT entry\n");
		NAT_PRINT("IPv4 Org IP: %u.%u.%u.%u->%u.%u.%u.%u\n",
			  IP_FORMAT3(entry->ipv4_hnapt.sip), IP_FORMAT2(entry->ipv4_hnapt.sip),
			  IP_FORMAT1(entry->ipv4_hnapt.sip), IP_FORMAT0(entry->ipv4_hnapt.sip),
			  IP_FORMAT3(entry->ipv4_hnapt.dip), IP_FORMAT2(entry->ipv4_hnapt.dip),
			  IP_FORMAT1(entry->ipv4_hnapt.dip), IP_FORMAT0(entry->ipv4_hnapt.dip));
		NAT_PRINT("IPv4 New IP: %u.%u.%u.%u->%u.%u.%u.%u\n",
			  IP_FORMAT3(entry->ipv4_hnapt.new_sip), IP_FORMAT2(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT1(entry->ipv4_hnapt.new_sip), IP_FORMAT0(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT3(entry->ipv4_hnapt.new_dip), IP_FORMAT2(entry->ipv4_hnapt.new_dip),
			  IP_FORMAT1(entry->ipv4_hnapt.new_dip), IP_FORMAT0(entry->ipv4_hnapt.new_dip));
	}
	if (IS_IPV6_1T_ROUTE(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv6_1t_route.info_blk2);
		NAT_PRINT("Create IPv6 Route entry\n");
		NAT_PRINT("Destination IPv6: %08X:%08X:%08X:%08X",
			  entry->ipv6_1t_route.ipv6_dip3, entry->ipv6_1t_route.ipv6_dip2,
			  entry->ipv6_1t_route.ipv6_dip1, entry->ipv6_1t_route.ipv6_dip0);
	} else if (IS_IPV4_DSLITE(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv4_dslite.info_blk2);
		NAT_PRINT("Create IPv4 Ds-Lite entry\n");
		NAT_PRINT
		    ("IPv4 Ds-Lite: %u.%u.%u.%u.%d->%u.%u.%u.%u:%d\n ",
		     IP_FORMAT3(entry->ipv4_dslite.sip), IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip), IP_FORMAT0(entry->ipv4_dslite.sip),
		     entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip), IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip), IP_FORMAT0(entry->ipv4_dslite.dip),
		     entry->ipv4_dslite.dport);
		NAT_PRINT("EG DIPv6: %08X:%08X:%08X:%08X->%08X:%08X:%08X:%08X\n",
			  entry->ipv4_dslite.tunnel_sipv6_0, entry->ipv4_dslite.tunnel_sipv6_1,
			  entry->ipv4_dslite.tunnel_sipv6_2, entry->ipv4_dslite.tunnel_sipv6_3,
			  entry->ipv4_dslite.tunnel_dipv6_0, entry->ipv4_dslite.tunnel_dipv6_1,
			  entry->ipv4_dslite.tunnel_dipv6_2, entry->ipv4_dslite.tunnel_dipv6_3);
	} else if (IS_IPV4_MAPE(entry)){
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv4_dslite.info_blk2);
		NAT_PRINT("Create IPv4 MAP-E entry\n");
		NAT_PRINT
		    ("IPv4 MAPE: %u.%u.%u.%u.%d->%u.%u.%u.%u:%d\n ",
		     IP_FORMAT3(entry->ipv4_dslite.sip), IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip), IP_FORMAT0(entry->ipv4_dslite.sip),
		     entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip), IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip), IP_FORMAT0(entry->ipv4_dslite.dip),
		     entry->ipv4_dslite.dport);
		NAT_PRINT("EG DIPv6: %08X:%08X:%08X:%08X->%08X:%08X:%08X:%08X\n",
			  entry->ipv4_dslite.tunnel_sipv6_0, entry->ipv4_dslite.tunnel_sipv6_1,
			  entry->ipv4_dslite.tunnel_sipv6_2, entry->ipv4_dslite.tunnel_sipv6_3,
			  entry->ipv4_dslite.tunnel_dipv6_0, entry->ipv4_dslite.tunnel_dipv6_1,
			  entry->ipv4_dslite.tunnel_dipv6_2, entry->ipv4_dslite.tunnel_dipv6_3);
		NAT_PRINT
		    ("IPv4 Org IP/Port: %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_dslite.sip), IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip), IP_FORMAT0(entry->ipv4_dslite.sip),
		     entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip), IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip), IP_FORMAT0(entry->ipv4_dslite.dip),
		     entry->ipv4_dslite.dport);
		NAT_PRINT
		    ("IPv4 New IP/Port: %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_dslite.new_sip), IP_FORMAT2(entry->ipv4_dslite.new_sip),
		     IP_FORMAT1(entry->ipv4_dslite.new_sip), IP_FORMAT0(entry->ipv4_dslite.new_sip),
		     entry->ipv4_dslite.new_sport,
		     IP_FORMAT3(entry->ipv4_dslite.new_dip), IP_FORMAT2(entry->ipv4_dslite.new_dip),
		     IP_FORMAT1(entry->ipv4_dslite.new_dip), IP_FORMAT0(entry->ipv4_dslite.new_dip),
		     entry->ipv4_dslite.new_dport);
 	}
#if(0)
	else if (IS_IPV4_MAPT(entry)){

		NAT_PRINT("Information Block 2: %08X\n", entry->ipv4_dslite.info_blk2);
		NAT_PRINT("Create IPv4 MAP-T entry (4 to 6)\n");
		NAT_PRINT
		    ("IPv4 MAPE: %u.%u.%u.%u.%d->%u.%u.%u.%u:%d\n ",
		     IP_FORMAT3(entry->ipv4_dslite.sip), IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip), IP_FORMAT0(entry->ipv4_dslite.sip),
		     entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip), IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip), IP_FORMAT0(entry->ipv4_dslite.dip),
		     entry->ipv4_dslite.dport);
		NAT_PRINT("EG DIPv6: %08X:%08X:%08X:%08X->%08X:%08X:%08X:%08X\n",
			  entry->ipv4_dslite.tunnel_sipv6_0, entry->ipv4_dslite.tunnel_sipv6_1,
			  entry->ipv4_dslite.tunnel_sipv6_2, entry->ipv4_dslite.tunnel_sipv6_3,
			  entry->ipv4_dslite.tunnel_dipv6_0, entry->ipv4_dslite.tunnel_dipv6_1,
			  entry->ipv4_dslite.tunnel_dipv6_2, entry->ipv4_dslite.tunnel_dipv6_3);
		NAT_PRINT
		    ("IPv4 Org IP/Port: %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_dslite.sip), IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip), IP_FORMAT0(entry->ipv4_dslite.sip),
		     entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip), IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip), IP_FORMAT0(entry->ipv4_dslite.dip),
		     entry->ipv4_dslite.dport);
		NAT_PRINT
		    ("L4 New Port: %d->%d\n", entry->ipv4_dslite.new_sport, entry->ipv4_dslite.new_dport);
		// ------------------
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv6_6rd.info_blk2);
		NAT_PRINT("Create MAP-T (6 to 4)\n");
		if (IS_IPV6_FLAB_EBL()) {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X (Flow Label=%08X)\n",
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.ipv6_dip0, entry->ipv6_6rd.ipv6_dip1,
			     entry->ipv6_6rd.ipv6_dip2, entry->ipv6_6rd.ipv6_dip3,
			     ((entry->ipv6_5t_route.sport << 16) | (entry->ipv6_5t_route.
								    dport)) & 0xFFFFF);
			NAT_PRINT ("L4 New Port: %d->%d\n", entry->ipv6_6rd.new_sport, entry->ipv6_6rd.new_dport);
		} else {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X:%d-> %08X:%08X:%08X:%08X:%d\n",
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.sport, entry->ipv6_6rd.ipv6_dip0,
			     entry->ipv6_6rd.ipv6_dip1, entry->ipv6_6rd.ipv6_dip2,
			     entry->ipv6_6rd.ipv6_dip3, entry->ipv6_6rd.dport);
			NAT_PRINT ("L4 New Port: %d->%d\n", entry->ipv6_6rd.new_sport, entry->ipv6_6rd.new_dport);
		}

	}
#endif
	else if (IS_IPV6_3T_ROUTE(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv6_3t_route.info_blk2);
		NAT_PRINT("Create IPv6 3-Tuple entry\n");
		NAT_PRINT
		    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X (Prot=%d)\n",
		     entry->ipv6_3t_route.ipv6_sip0, entry->ipv6_3t_route.ipv6_sip1,
		     entry->ipv6_3t_route.ipv6_sip2, entry->ipv6_3t_route.ipv6_sip3,
		     entry->ipv6_3t_route.ipv6_dip0, entry->ipv6_3t_route.ipv6_dip1,
		     entry->ipv6_3t_route.ipv6_dip2, entry->ipv6_3t_route.ipv6_dip3,
		     entry->ipv6_3t_route.prot);
	} else if (IS_IPV6_5T_ROUTE(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv6_5t_route.info_blk2);
		NAT_PRINT("Create IPv6 5-Tuple entry\n");
		if (IS_IPV6_FLAB_EBL()) {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X (Flow Label=%08X)\n",
			     entry->ipv6_5t_route.ipv6_sip0, entry->ipv6_5t_route.ipv6_sip1,
			     entry->ipv6_5t_route.ipv6_sip2, entry->ipv6_5t_route.ipv6_sip3,
			     entry->ipv6_5t_route.ipv6_dip0, entry->ipv6_5t_route.ipv6_dip1,
			     entry->ipv6_5t_route.ipv6_dip2, entry->ipv6_5t_route.ipv6_dip3,
			     ((entry->ipv6_5t_route.sport << 16) | (entry->ipv6_5t_route.
								    dport)) & 0xFFFFF);
		} else {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X %d-> %08X:%08X:%08X:%08X %d\n",
			     entry->ipv6_5t_route.ipv6_sip0, entry->ipv6_5t_route.ipv6_sip1,
			     entry->ipv6_5t_route.ipv6_sip2, entry->ipv6_5t_route.ipv6_sip3,
			     entry->ipv6_5t_route.sport, entry->ipv6_5t_route.ipv6_dip0,
			     entry->ipv6_5t_route.ipv6_dip1, entry->ipv6_5t_route.ipv6_dip2,
			     entry->ipv6_5t_route.ipv6_dip3, entry->ipv6_5t_route.dport);
		}
	} else if (IS_IPV6_6RD(entry)) {
		NAT_PRINT("Information Block 2: %08X\n", entry->ipv6_6rd.info_blk2);
		NAT_PRINT("Create IPv6 6RD entry\n");
		if (IS_IPV6_FLAB_EBL()) {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X (Flow Label=%08X)\n",
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.ipv6_dip0, entry->ipv6_6rd.ipv6_dip1,
			     entry->ipv6_6rd.ipv6_dip2, entry->ipv6_6rd.ipv6_dip3,
			     ((entry->ipv6_5t_route.sport << 16) | (entry->ipv6_5t_route.
								    dport)) & 0xFFFFF);
			NAT_PRINT
			    ("EG SIPv4->DIPv4: %u.%u.%u.%u -> %u.%u.%u.%u\n",
			     IP_FORMAT3(entry->ipv6_6rd.tunnel_sipv4), IP_FORMAT2(entry->ipv6_6rd.tunnel_sipv4),
			     IP_FORMAT1(entry->ipv6_6rd.tunnel_sipv4), IP_FORMAT0(entry->ipv6_6rd.tunnel_sipv4),
			     IP_FORMAT3(entry->ipv6_6rd.tunnel_dipv4), IP_FORMAT2(entry->ipv6_6rd.tunnel_dipv4),
			     IP_FORMAT1(entry->ipv6_6rd.tunnel_dipv4), IP_FORMAT0(entry->ipv6_6rd.tunnel_dipv4));

		} else {
			NAT_PRINT
			    ("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X %d-> %08X:%08X:%08X:%08X %d\n",
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.sport, entry->ipv6_6rd.ipv6_dip0,
			     entry->ipv6_6rd.ipv6_dip1, entry->ipv6_6rd.ipv6_dip2,
			     entry->ipv6_6rd.ipv6_dip3, entry->ipv6_6rd.dport);

			NAT_PRINT
			    ("EG SIPv4->DIPv4: %u.%u.%u.%u -> %u.%u.%u.%u\n",
			     IP_FORMAT3(entry->ipv6_6rd.tunnel_sipv4), IP_FORMAT2(entry->ipv6_6rd.tunnel_sipv4),
			     IP_FORMAT1(entry->ipv6_6rd.tunnel_sipv4), IP_FORMAT0(entry->ipv6_6rd.tunnel_sipv4),
			     IP_FORMAT3(entry->ipv6_6rd.tunnel_dipv4), IP_FORMAT2(entry->ipv6_6rd.tunnel_dipv4),
			     IP_FORMAT1(entry->ipv6_6rd.tunnel_dipv4), IP_FORMAT0(entry->ipv6_6rd.tunnel_dipv4));
		}
	}

	if (IS_IPV4_HNAPT(entry) || IS_IPV4_HNAT(entry)) {
		NAT_PRINT("DMAC=%02X:%02X:%02X:%02X:%02X:%02X SMAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
			  entry->ipv4_hnapt.dmac_hi[3], entry->ipv4_hnapt.dmac_hi[2],
			  entry->ipv4_hnapt.dmac_hi[1], entry->ipv4_hnapt.dmac_hi[0],
			  entry->ipv4_hnapt.dmac_lo[1], entry->ipv4_hnapt.dmac_lo[0],
			  entry->ipv4_hnapt.smac_hi[3], entry->ipv4_hnapt.smac_hi[2],
			  entry->ipv4_hnapt.smac_hi[1], entry->ipv4_hnapt.smac_hi[0],
			  entry->ipv4_hnapt.smac_lo[1], entry->ipv4_hnapt.smac_lo[0]);
		NAT_PRINT("State = %s, ",
			  entry->bfib1.state ==
			  0 ? "Invalid" : entry->bfib1.state ==
			  1 ? "Unbind" : entry->bfib1.state ==
			  2 ? "BIND" : entry->bfib1.state ==
			  3 ? "FIN" : "Unknown");
		NAT_PRINT("Vlan_Layer = %u, ",
			  entry->bfib1.vlan_layer);
		NAT_PRINT("Eth_type = 0x%x, Vid1 = 0x%x, Vid2 = 0x%x\n",
			  entry->ipv4_hnapt.etype, entry->ipv4_hnapt.vlan1,
			  entry->ipv4_hnapt.vlan2_winfo);
		NAT_PRINT("mib = %d, multicast = %d, pppoe = %d, proto = %s, act_dp = %d rx_idx = %d, dma_ring_no = %d\n",
			  entry->ipv4_hnapt.iblk2.mibf,
			  entry->ipv4_hnapt.iblk2.mcast,
			  entry->ipv4_hnapt.bfib1.psn,
			  entry->ipv4_hnapt.bfib1.udp == 0 ? "TCP" :
			  entry->ipv4_hnapt.bfib1.udp == 1 ? "UDP" : "Unknown",
			  entry->ipv4_hnapt.act_dp,
			  entry->ipv4_hnapt.rxif_idx,
			  entry->ipv4_hnapt.iblk2.dr_idx
			  );
		NAT_PRINT("=========================================\n\n");
	} else {
		NAT_PRINT("DMAC=%02X:%02X:%02X:%02X:%02X:%02X SMAC=%02X:%02X:%02X:%02X:%02X:%02X\n",
			  entry->ipv6_5t_route.dmac_hi[3], entry->ipv6_5t_route.dmac_hi[2],
			  entry->ipv6_5t_route.dmac_hi[1], entry->ipv6_5t_route.dmac_hi[0],
			  entry->ipv6_5t_route.dmac_lo[1], entry->ipv6_5t_route.dmac_lo[0],
			  entry->ipv6_5t_route.smac_hi[3], entry->ipv6_5t_route.smac_hi[2],
			  entry->ipv6_5t_route.smac_hi[1], entry->ipv6_5t_route.smac_hi[0],
			  entry->ipv6_5t_route.smac_lo[1], entry->ipv6_5t_route.smac_lo[0]);
		NAT_PRINT("State = %s, STC = %s, ", entry->bfib1.state ==
			  0 ? "Invalid" : entry->bfib1.state ==
			  1 ? "Unbind" : entry->bfib1.state ==
			  2 ? "BIND" : entry->bfib1.state ==
			  3 ? "FIN" : "Unknown", entry->bfib1.stc ==
			  0 ? "Dynamic" : entry->bfib1.stc ==
			  1 ? "static" : "Unknown");

		NAT_PRINT("Vlan_Layer = %u, ",
			  entry->bfib1.vlan_layer);
		NAT_PRINT("Eth_type = 0x%x, Vid1 = 0x%x, Vid2 = 0x%x\n",
			  entry->ipv6_5t_route.etype,
			  entry->ipv6_5t_route.vlan1,
			  entry->ipv6_5t_route.vlan2_winfo);
		NAT_PRINT("mib = %d, multicast = %d, pppoe = %d, proto = %s, act_dp = %d rx_idx = %d",
			  entry->ipv6_5t_route.iblk2.mibf,
			  entry->ipv6_5t_route.iblk2.mcast,
			  entry->ipv6_5t_route.bfib1.psn,
			  entry->ipv6_5t_route.bfib1.udp ==
			  0 ? "TCP" : entry->ipv6_5t_route.bfib1.udp ==
			  1 ? "UDP" : "Unknown",
			  entry->ipv6_5t_route.act_dp,
			  entry->ipv6_5t_route.rxif_idx);
		NAT_PRINT(" Remove tunnel = %u\n", entry->bfib1.rmt);
		NAT_PRINT("=========================================\n\n");
	}

}

int foe_get_ppe_entries(struct hwnat_args *opt1, int count, int ppe_idx)
{
	struct foe_entry *entry;
	struct hnat_mib_struct *mib_entry_p;
	int hash_index = 0;
	struct foe_entry *foe_base = (ppe_idx == 0) ? ppe_foe_base : ppe1_foe_base;
	struct hnat_mib_struct *mib_base = (ppe_idx == 0) ? hnat_entry_mib : hnat_entry1_mib;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &foe_base[hash_index];
		mib_entry_p = &mib_base[hash_index];
		if (entry->bfib1.state == opt1->entry_state) {
			opt1->entries[count].ppe_index = ppe_idx;
			opt1->entries[count].hash_index = hash_index;
			opt1->entries[count].pkt_type = entry->ipv4_hnapt.bfib1.pkt_type;

			/* Extra info */
			opt1->entries[count].fport = opt1->entries[count].fqos = opt1->entries[count].qid = 0;
			if (IS_IPV4_GRP(entry)) {
				opt1->entries[count].fport = ppe_force_port(entry);
				opt1->entries[count].fqos = entry->ipv4_hnapt.iblk2.fqos;
				opt1->entries[count].qid = ppe_qid(entry);
				opt1->entries[count].dr_idx = entry->ipv4_hnapt.iblk2.dr_idx;
			}
			if (fe_feature & HNAT_IPV6) {
				if (IS_IPV6_GRP(entry)) {
					opt1->entries[count].fport = ppe_force_port(entry);
					opt1->entries[count].fqos = entry->ipv6_3t_route.iblk2.fqos;
					opt1->entries[count].qid =  ppe_qid(entry);
					opt1->entries[count].dr_idx = entry->ipv6_3t_route.iblk2.dr_idx;
				}
			}
			opt1->entries[count].rxif_idx = get_rxif_idx(entry);
			opt1->entries[count].act_mode = get_act_mode(entry);
			opt1->entries[count].act_dp = get_act_dp(entry);

			/* mib count: show in 'hwnat -g' */
			if (entry->bfib1.state == BIND) {
				ppe_get_mib_by_entry(entry, hash_index, ppe_idx);
				opt1->entries[count].packets = mib_entry_p->pkt_count;
				opt1->entries[count].bytes = mib_entry_p->byte_count;
			}

			if (IS_IPV4_HNAT(entry)) {
				opt1->entries[count].ing_sipv4 = entry->ipv4_hnapt.sip;
				opt1->entries[count].ing_dipv4 = entry->ipv4_hnapt.dip;
				opt1->entries[count].eg_sipv4 = entry->ipv4_hnapt.new_sip;
				opt1->entries[count].eg_dipv4 = entry->ipv4_hnapt.new_dip;
				count++;
			} else if (IS_IPV4_HNAPT(entry)) {
				opt1->entries[count].ing_sipv4 = entry->ipv4_hnapt.sip;
				opt1->entries[count].ing_dipv4 = entry->ipv4_hnapt.dip;
				opt1->entries[count].eg_sipv4 = entry->ipv4_hnapt.new_sip;
				opt1->entries[count].eg_dipv4 = entry->ipv4_hnapt.new_dip;
				opt1->entries[count].ing_sp = entry->ipv4_hnapt.sport;
				opt1->entries[count].ing_dp = entry->ipv4_hnapt.dport;
				opt1->entries[count].eg_sp = entry->ipv4_hnapt.new_sport;
				opt1->entries[count].eg_dp = entry->ipv4_hnapt.new_dport;
				count++;
			}
			if (fe_feature & HNAT_IPV6) {
				if (IS_IPV6_1T_ROUTE(entry)) {
					opt1->entries[count].ing_dipv6_0 = entry->ipv6_1t_route.ipv6_dip3;
					opt1->entries[count].ing_dipv6_1 = entry->ipv6_1t_route.ipv6_dip2;
					opt1->entries[count].ing_dipv6_2 = entry->ipv6_1t_route.ipv6_dip1;
					opt1->entries[count].ing_dipv6_3 = entry->ipv6_1t_route.ipv6_dip0;
					count++;
				} else if (IS_IPV4_DSLITE(entry)) {
					opt1->entries[count].ing_sipv4 = entry->ipv4_dslite.sip;
					opt1->entries[count].ing_dipv4 = entry->ipv4_dslite.dip;
					opt1->entries[count].ing_sp = entry->ipv4_dslite.sport;
					opt1->entries[count].ing_dp = entry->ipv4_dslite.dport;
					opt1->entries[count].eg_sipv6_0 = entry->ipv4_dslite.tunnel_sipv6_0;
					opt1->entries[count].eg_sipv6_1 = entry->ipv4_dslite.tunnel_sipv6_1;
					opt1->entries[count].eg_sipv6_2 = entry->ipv4_dslite.tunnel_sipv6_2;
					opt1->entries[count].eg_sipv6_3 = entry->ipv4_dslite.tunnel_sipv6_3;
					opt1->entries[count].eg_dipv6_0 = entry->ipv4_dslite.tunnel_dipv6_0;
					opt1->entries[count].eg_dipv6_1 = entry->ipv4_dslite.tunnel_dipv6_1;
					opt1->entries[count].eg_dipv6_2 = entry->ipv4_dslite.tunnel_dipv6_2;
					opt1->entries[count].eg_dipv6_3 = entry->ipv4_dslite.tunnel_dipv6_3;
					count++;
				} else if (IS_IPV6_3T_ROUTE(entry)) {
					opt1->entries[count].ing_sipv6_0 = entry->ipv6_3t_route.ipv6_sip0;
					opt1->entries[count].ing_sipv6_1 = entry->ipv6_3t_route.ipv6_sip1;
					opt1->entries[count].ing_sipv6_2 = entry->ipv6_3t_route.ipv6_sip2;
					opt1->entries[count].ing_sipv6_3 = entry->ipv6_3t_route.ipv6_sip3;
					opt1->entries[count].ing_dipv6_0 = entry->ipv6_3t_route.ipv6_dip0;
					opt1->entries[count].ing_dipv6_1 = entry->ipv6_3t_route.ipv6_dip1;
					opt1->entries[count].ing_dipv6_2 = entry->ipv6_3t_route.ipv6_dip2;
					opt1->entries[count].ing_dipv6_3 = entry->ipv6_3t_route.ipv6_dip3;
					opt1->entries[count].prot = entry->ipv6_3t_route.prot;
					count++;
				} else if (IS_IPV6_5T_ROUTE(entry)) {
					opt1->entries[count].ing_sipv6_0 = entry->ipv6_5t_route.ipv6_sip0;
					opt1->entries[count].ing_sipv6_1 = entry->ipv6_5t_route.ipv6_sip1;
					opt1->entries[count].ing_sipv6_2 = entry->ipv6_5t_route.ipv6_sip2;
					opt1->entries[count].ing_sipv6_3 = entry->ipv6_5t_route.ipv6_sip3;
					opt1->entries[count].ing_sp = entry->ipv6_5t_route.sport;
					opt1->entries[count].ing_dp = entry->ipv6_5t_route.dport;

					opt1->entries[count].ing_dipv6_0 = entry->ipv6_5t_route.ipv6_dip0;
					opt1->entries[count].ing_dipv6_1 = entry->ipv6_5t_route.ipv6_dip1;
					opt1->entries[count].ing_dipv6_2 = entry->ipv6_5t_route.ipv6_dip2;
					opt1->entries[count].ing_dipv6_3 = entry->ipv6_5t_route.ipv6_dip3;
					opt1->entries[count].ipv6_flowlabel = IS_IPV6_FLAB_EBL();
					count++;
				} else if (IS_IPV6_6RD(entry)) {
					opt1->entries[count].ing_sipv6_0 = entry->ipv6_6rd.ipv6_sip0;
					opt1->entries[count].ing_sipv6_1 = entry->ipv6_6rd.ipv6_sip1;
					opt1->entries[count].ing_sipv6_2 = entry->ipv6_6rd.ipv6_sip2;
					opt1->entries[count].ing_sipv6_3 = entry->ipv6_6rd.ipv6_sip3;

					opt1->entries[count].ing_dipv6_0 = entry->ipv6_6rd.ipv6_dip0;
					opt1->entries[count].ing_dipv6_1 = entry->ipv6_6rd.ipv6_dip1;
					opt1->entries[count].ing_dipv6_2 = entry->ipv6_6rd.ipv6_dip2;
					opt1->entries[count].ing_dipv6_3 = entry->ipv6_6rd.ipv6_dip3;
					opt1->entries[count].ing_sp = entry->ipv6_6rd.sport;
					opt1->entries[count].ing_dp = entry->ipv6_6rd.dport;
					opt1->entries[count].ipv6_flowlabel = IS_IPV6_FLAB_EBL();

					opt1->entries[count].eg_sipv4 = entry->ipv6_6rd.tunnel_sipv4;
					opt1->entries[count].eg_dipv4 = entry->ipv6_6rd.tunnel_dipv4;
					count++;
				}
			}

		}

		if (count >= 16 * 1024)
			break;
	}

	return count;

}


int foe_get_all_entries(struct hwnat_args *opt1)
{
	int ppe_count, ppe1_count = 0;		/* valid entry count */

	ppe_count = foe_get_ppe_entries(opt1, 0, 0);
	if (ppe_count < 16 * 1024) {
		ppe1_count = foe_get_ppe_entries(opt1, ppe_count, 1);
	}

	opt1->num_of_entries = ppe1_count;

	if (opt1->num_of_entries > 0)
		return HWNAT_SUCCESS;
	else
		return HWNAT_ENTRY_NOT_FOUND;
}

int foe_bind_entry(struct hwnat_args *opt1)
{
	struct foe_entry *entry;

	if (((u32)opt1->entry_num >= FOE_4TB_SIZ))
		return HWNAT_FAIL;

	entry = &ppe_foe_base[(u32)opt1->entry_num];

	/* restore right information block1 */
	entry->bfib1.time_stamp = reg_read(FOE_TS) & 0xFFFF;
	entry->bfib1.state = BIND;

	return HWNAT_SUCCESS;
}

int foe_un_bind_entry(struct hwnat_args *opt)
{
	struct foe_entry *entry;
	int ppe_index = opt->debug;

	if (((u32)opt->entry_num >= FOE_4TB_SIZ))
		return HWNAT_FAIL;

	if (ppe_index == 0)
		entry = &ppe_foe_base[opt->entry_num];
	else if (ppe_index == 1)
		entry = &ppe1_foe_base[opt->entry_num];
	else {
		pr_notice("%s, wrong ppe index: %d\n", __func__, ppe_index);
		return HWNAT_FAIL;
	}

	entry->ipv4_hnapt.udib1.state = INVALID;
	entry->ipv4_hnapt.udib1.time_stamp = reg_read(FOE_TS) & 0xFF;

	ppe_set_cache_ebl(ppe_index);	/*clear HWNAT cache */

	return HWNAT_SUCCESS;
}

int _foe_drop_entry(unsigned int entry_num, unsigned int ppe_index)
{
	struct foe_entry *entry;

	if (ppe_index == 0)
		entry = &ppe_foe_base[entry_num];
	else if (ppe_index == 1)
		entry = &ppe1_foe_base[entry_num];
	else {
		pr_notice("%s, wrong ppe index: %d\n", __func__, ppe_index);
		return HWNAT_FAIL;
	}

	entry->ipv4_hnapt.iblk2.dp = 7;

	ppe_set_cache_ebl(ppe_index);	/*clear HWNAT cache */

	return HWNAT_SUCCESS;
}
EXPORT_SYMBOL(_foe_drop_entry);

int foe_drop_entry(struct hwnat_args *opt)
{
	if (((u32)opt->entry_num >= FOE_4TB_SIZ))
		return HWNAT_FAIL;

	if (((u32)opt->debug >= FOE_PPE_SIZE))
		return HWNAT_FAIL;

	return _foe_drop_entry(opt->entry_num, opt->debug);
}

int foe_del_entry_by_num(uint32_t entry_num, uint32_t ppe_index)
{
	struct foe_entry *entry;

	if (entry_num >= FOE_4TB_SIZ)
		return HWNAT_FAIL;

	if (ppe_index == 0)
		entry = &ppe_foe_base[entry_num];
	else if (ppe_index == 1)
		entry = &ppe1_foe_base[entry_num];
	else {
		pr_notice("%s, wrong ppe index: %d\n", __func__, ppe_index);
		return HWNAT_FAIL;
	}

	memset(entry, 0, sizeof(struct foe_entry));

	ppe_set_cache_ebl(ppe_index);	/*clear HWNAT cache */

	return HWNAT_SUCCESS;
}

int foe_is_wifi_src_entry(struct foe_entry *entry) {

	uint32_t rxif_idx = get_rxif_idx(entry);

	if (dst_port[rxif_idx] != NULL &&
	    (strncmp(dst_port[rxif_idx]->name, "rai0", 4) == 0 ||
	     strncmp(dst_port[rxif_idx]->name, "ra0", 3) == 0 ||
	     strncmp(dst_port[rxif_idx]->name, "rax0", 4) == 0))
		return 1;

	return 0;
}

int foe_is_wifi_dst_entry(struct foe_entry *entry) {

	uint32_t act_dp = get_act_dp(entry);

	if (dst_port[act_dp] != NULL &&
	    (strncmp(dst_port[act_dp]->name, "rai0", 4) == 0 ||
	     strncmp(dst_port[act_dp]->name, "ra0", 3) == 0 ||
	     strncmp(dst_port[act_dp]->name, "rax0", 4) == 0))
		return 1;

	return 0;
}

int foe_compare_wifi_mac(struct foe_entry *entry, unsigned char *mac) {

	if (IS_IPV4_GRP(entry)) {
		if ((entry->ipv4_hnapt.dmac_hi[3] == mac[0] &&
		     entry->ipv4_hnapt.dmac_hi[2] == mac[1] &&
		     entry->ipv4_hnapt.dmac_hi[1] == mac[2] &&
		     entry->ipv4_hnapt.dmac_hi[0] == mac[3] &&
		     entry->ipv4_hnapt.dmac_lo[1] == mac[4] &&
		     entry->ipv4_hnapt.dmac_lo[0] == mac[5] &&
		     foe_is_wifi_dst_entry(entry)) ||
		    (entry->ipv4_hnapt.smac_hi[3] == mac[0] &&
		     entry->ipv4_hnapt.smac_hi[2] == mac[1] &&
		     entry->ipv4_hnapt.smac_hi[1] == mac[2] &&
		     entry->ipv4_hnapt.smac_hi[0] == mac[3] &&
		     entry->ipv4_hnapt.smac_lo[1] == mac[4] &&
		     entry->ipv4_hnapt.smac_lo[0] == mac[5] &&
		     foe_is_wifi_src_entry(entry))) {
			return 1;
		}

	} else if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			if ((entry->ipv6_3t_route.dmac_hi[3] == mac[0] &&
			     entry->ipv6_3t_route.dmac_hi[2] == mac[1] &&
			     entry->ipv6_3t_route.dmac_hi[1] == mac[2] &&
			     entry->ipv6_3t_route.dmac_hi[0] == mac[3] &&
			     entry->ipv6_3t_route.dmac_lo[1] == mac[4] &&
			     entry->ipv6_3t_route.dmac_lo[0] == mac[5] &&
			     foe_is_wifi_dst_entry(entry)) ||
			    (entry->ipv6_3t_route.smac_hi[3] == mac[0] &&
			     entry->ipv6_3t_route.smac_hi[2] == mac[1] &&
			     entry->ipv6_3t_route.smac_hi[1] == mac[2] &&
			     entry->ipv6_3t_route.smac_hi[0] == mac[3] &&
			     entry->ipv6_3t_route.smac_lo[1] == mac[4] &&
			     entry->ipv6_3t_route.smac_lo[0] == mac[5] &&
			     foe_is_wifi_src_entry(entry))) {
				return 1;
			}

		}
	}
	return 0;

}

int foe_del_entry_by_mac(unsigned char *mac)
{
	struct foe_entry *entry;
	int hash_index, del0 = 0, del1 = 0;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];

		/* avoid to remove static bind entry */
		if(entry->bfib1.state == BIND && entry->bfib1.stc == 0) {
			if (foe_compare_wifi_mac(entry, mac)) {
				memset(entry, 0, sizeof(struct foe_entry));
				del0 = 1;

				if (debug_level >= 3)
					pr_info("%s, del ppe0 idx = %d\n", __func__, hash_index);
			}
		}

		entry = &ppe1_foe_base[hash_index];

		/* avoid to remove static bind entry */
		if(entry->bfib1.state == BIND && entry->bfib1.stc == 0) {
			if (foe_compare_wifi_mac(entry, mac)) {
				memset(entry, 0, sizeof(struct foe_entry));
				del1 = 1;

				if (debug_level >= 3)
					pr_info("%s, del ppe1 idx = %d\n", __func__, hash_index);
			}
		}
	}

	if (del0)
		ppe_set_cache_ebl(0);	/*clear HWNAT cache */
	if (del1)
		ppe_set_cache_ebl(1);	/*clear HWNAT cache */

	return HWNAT_SUCCESS;
}


void foe_del_entry_by_dev(struct net_device *dev)
{
	struct foe_entry *entry;
	int dev_idx, hash_index, del0 = 0, del1 = 0;
	uint32_t fport, sport;

	for (dev_idx = 0; dev_idx < MAX_IF_NUM; dev_idx++) {
		if (dst_port[dev_idx] == dev) {
			break;
		}
	}

	if (dev_idx >= MAX_IF_NUM)
		return;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];

		fport = get_act_dp(entry);
		sport = get_rxif_idx(entry);

		if (dev_idx == fport || dev_idx == sport) {
			if(entry->bfib1.state == BIND) {
				memset(entry, 0, sizeof(struct foe_entry));

				del0 = 1;
				if (debug_level >= 3)
					pr_info("%s, del ppe0 idx = %d\n", __func__, hash_index);
			}

		}

		entry = &ppe1_foe_base[hash_index];

		fport = get_act_dp(entry);
		sport = get_rxif_idx(entry);

		if (dev_idx == fport || dev_idx == sport) {
			if(entry->bfib1.state == BIND) {
				memset(entry, 0, sizeof(struct foe_entry));

				del1 = 1;
				if (debug_level >= 3)
					pr_info("%s, del ppe1 idx = %d\n", __func__, hash_index);
			}

		}
	}

	if (del0)
		ppe_set_cache_ebl(0);	/*clear HWNAT cache */
	if (del1)
		ppe_set_cache_ebl(1);	/*clear HWNAT cache */
}

void foe_tbl_clean(void)
{
	u32 foe_tbl_size;

	foe_tbl_size = FOE_4TB_SIZ * sizeof(struct foe_entry);
	memset(ppe_foe_base, 0, foe_tbl_size);
	memset(ppe1_foe_base, 0, foe_tbl_size);
	ppe_set_cache_ebl(0);	/*clear HWNAT cache */
	ppe_set_cache_ebl(1);	/*clear HWNAT cache */
}
EXPORT_SYMBOL(foe_tbl_clean);

bool foe_has_bind_entry(void) {

	int hash_index;
	struct foe_entry *entry;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {

		entry = &ppe_foe_base[hash_index];

		if(entry->bfib1.state == BIND) {
			return true;
		}

		entry = &ppe1_foe_base[hash_index];

		if(entry->bfib1.state == BIND) {
			return true;
		}
	}

	return false;
}

void hw_nat_l2_info(struct foe_entry *entry, struct hwnat_tuple *opt)
{
	if (opt->pkt_type == IPV4_NAPT || opt->pkt_type == IPV4_NAT) {
		foe_set_mac_hi_info(entry->ipv4_hnapt.dmac_hi, opt->dmac);
		foe_set_mac_lo_info(entry->ipv4_hnapt.dmac_lo, opt->dmac);
		foe_set_mac_hi_info(entry->ipv4_hnapt.smac_hi, opt->smac);
		foe_set_mac_lo_info(entry->ipv4_hnapt.smac_lo, opt->smac);
		entry->ipv4_hnapt.vlan1 = opt->vlan1;
		/* warp hwnat not support vlan2 */
		/*mt7622 wifi hwnat not support vlan2*/
		//entry->ipv4_hnapt.vlan2_winfo = opt->vlan2;
		entry->ipv4_hnapt.etype = opt->set_idx;
		entry->ipv4_hnapt.pppoe_id = opt->pppoe_id;
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			foe_set_mac_hi_info(entry->ipv6_5t_route.dmac_hi, opt->dmac);
			foe_set_mac_lo_info(entry->ipv6_5t_route.dmac_lo, opt->dmac);
			foe_set_mac_hi_info(entry->ipv6_5t_route.smac_hi, opt->smac);
			foe_set_mac_lo_info(entry->ipv6_5t_route.smac_lo, opt->smac);
			entry->ipv6_5t_route.vlan1 = opt->vlan1;
			/*mt7622 wifi hwnat not support vlan2*/
			//entry->ipv6_5t_route.vlan2_winfo = opt->vlan2;
			entry->ipv6_5t_route.etype = opt->set_idx;
			entry->ipv6_5t_route.pppoe_id = opt->pppoe_id;
		}
	} else if ((opt->pkt_type) == IPV4_DSLITE) {
		foe_set_mac_hi_info(entry->ipv4_dslite.dmac_hi, opt->dmac);
		foe_set_mac_lo_info(entry->ipv4_dslite.dmac_lo, opt->dmac);
		foe_set_mac_hi_info(entry->ipv4_dslite.smac_hi, opt->smac);
		foe_set_mac_lo_info(entry->ipv4_dslite.smac_lo, opt->smac);
		entry->ipv4_dslite.vlan1 = opt->vlan1;
			/*mt7622 wifi hwnat not support vlan2*/
		//entry->ipv4_dslite.vlan2_winfo = opt->vlan2;
		entry->ipv4_dslite.pppoe_id = opt->pppoe_id;
		entry->ipv4_dslite.etype = opt->set_idx;
	} else if ((opt->pkt_type) == IPV6_6RD) {
		foe_set_mac_hi_info(entry->ipv6_6rd.dmac_hi, opt->dmac);
		foe_set_mac_lo_info(entry->ipv6_6rd.dmac_lo, opt->dmac);
		foe_set_mac_hi_info(entry->ipv6_6rd.smac_hi, opt->smac);
		foe_set_mac_lo_info(entry->ipv6_6rd.smac_lo, opt->smac);
		entry->ipv6_6rd.vlan1 = opt->vlan1;
			/*mt7622 wifi hwnat not support vlan2*/
		entry->ipv6_6rd.vlan2_winfo = opt->vlan2;
		entry->ipv6_6rd.pppoe_id = opt->pppoe_id;
		entry->ipv6_6rd.etype = opt->set_idx;
	}
}

struct test_hdr {
        u8	flow_lbl[3];
};

void hw_nat_l3_info(struct foe_entry *entry, struct hwnat_tuple *opt)
{
	struct test_hdr test;

	test.flow_lbl[0] = 0x56;
	test.flow_lbl[1] = 0x12;
	test.flow_lbl[2] = 0xab;

	if (opt->pkt_type == IPV4_NAPT || opt->pkt_type == IPV4_NAT) {
		entry->ipv4_hnapt.sip = opt->ing_sipv4;
		entry->ipv4_hnapt.dip = opt->ing_dipv4;
		entry->ipv4_hnapt.new_sip = opt->eg_sipv4;
		entry->ipv4_hnapt.new_dip = opt->eg_dipv4;
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			entry->ipv6_5t_route.ipv6_sip0 = opt->ing_sipv6_0;
			entry->ipv6_5t_route.ipv6_sip1 = opt->ing_sipv6_1;
			entry->ipv6_5t_route.ipv6_sip2 = opt->ing_sipv6_2;
			entry->ipv6_5t_route.ipv6_sip3 = opt->ing_sipv6_3;

			entry->ipv6_5t_route.ipv6_dip0 = opt->ing_dipv6_0;
			entry->ipv6_5t_route.ipv6_dip1 = opt->ing_dipv6_1;
			entry->ipv6_5t_route.ipv6_dip2 = opt->ing_dipv6_2;
			entry->ipv6_5t_route.ipv6_dip3 = opt->ing_dipv6_3;
		}

/*		pr_info("opt->ing_sipv6_0 = %x\n", opt->ing_sipv6_0);*/
/*		pr_info("opt->ing_sipv6_1 = %x\n", opt->ing_sipv6_1);*/
/*		pr_info("opt->ing_sipv6_2 = %x\n", opt->ing_sipv6_2);*/
/*		pr_info("opt->ing_sipv6_3 = %x\n", opt->ing_sipv6_3);*/
/*		pr_info("opt->ing_dipv6_0 = %x\n", opt->ing_dipv6_0);*/
/*		pr_info("opt->ing_dipv6_1 = %x\n", opt->ing_dipv6_1);*/
/*		pr_info("opt->ing_dipv6_2 = %x\n", opt->ing_dipv6_2);*/
/*		pr_info("opt->ing_dipv6_3 = %x\n", opt->ing_dipv6_3);*/

/*		pr_info("entry->ipv6_5t_route.ipv6_sip0 = %x\n", entry->ipv6_5t_route.ipv6_sip0);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_sip1 = %x\n", entry->ipv6_5t_route.ipv6_sip1);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_sip2 = %x\n", entry->ipv6_5t_route.ipv6_sip2);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_sip3 = %x\n", entry->ipv6_5t_route.ipv6_sip3);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_dip0 = %x\n", entry->ipv6_5t_route.ipv6_dip0);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_dip1 = %x\n", entry->ipv6_5t_route.ipv6_dip1);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_dip2 = %x\n", entry->ipv6_5t_route.ipv6_dip2);*/
/*		pr_info("entry->ipv6_5t_route.ipv6_dip3 = %x\n", entry->ipv6_5t_route.ipv6_dip3);*/
	} else if((opt->pkt_type) == IPV4_DSLITE){
		entry->ipv4_dslite.iblk2.mibf = 1;
		if ((opt->rmt == 0)) {
			entry->ipv4_dslite.tunnel_sipv6_0 = opt->ing_sipv6_0;
			entry->ipv4_dslite.tunnel_sipv6_1 = opt->ing_sipv6_1;
			entry->ipv4_dslite.tunnel_sipv6_2 = opt->ing_sipv6_2;
			entry->ipv4_dslite.tunnel_sipv6_3 = opt->ing_sipv6_3;
			entry->ipv4_dslite.tunnel_dipv6_0 = opt->ing_dipv6_0;
			entry->ipv4_dslite.tunnel_dipv6_1 = opt->ing_dipv6_1;
			entry->ipv4_dslite.tunnel_dipv6_2 = opt->ing_dipv6_2;
			entry->ipv4_dslite.tunnel_dipv6_3 = opt->ing_dipv6_3;
			entry->ipv4_dslite.iblk2.mibf = 1;
			entry->ipv4_dslite.priority = 0xf;
			entry->ipv4_dslite.hop_limit = 120;
			memcpy(entry->ipv4_dslite.flow_lbl, test.flow_lbl, 3);
			/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
			entry->bfib1.pkt_type = IPV4_DSLITE;
		}
	} else if((opt->pkt_type) == IPV6_6RD){
		/* fill in ipv4 6rd entry */
		entry->ipv6_6rd.tunnel_sipv4 = opt->ing_sipv4;
		entry->ipv6_6rd.tunnel_dipv4 = opt->ing_dipv4;
		//entry->ipv6_6rd.hdr_chksum = ppe_get_chkbase(&ppe_parse_result->iph);
		entry->ipv6_6rd.hdr_chksum = opt->checksum;
		pr_notice("opt->checksum = %x\n", opt->checksum);
		entry->ipv6_6rd.flag = opt->frag;
		entry->ipv6_6rd.ttl = opt->ttl;
		entry->ipv6_6rd.dscp = 0;
		entry->ipv6_6rd.iblk2.mibf = 1;
		reg_modify_bits(PPE_6RD_ID, 0, 0, 16);
		reg_modify_bits(PPE1_6RD_ID, 0, 0, 16);
		pr_notice("PPE_6RD_ID = %x\n", reg_read(PPE_6RD_ID));
		entry->ipv6_6rd.per_flow_6rd_id = 1;
		/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
		entry->bfib1.pkt_type = IPV6_6RD;
	}
}

void hw_nat_l4_info(struct foe_entry *entry, struct hwnat_tuple *opt)
{
	if ((opt->pkt_type) == IPV4_NAPT) {
		entry->ipv4_hnapt.dport = opt->ing_dp;
		entry->ipv4_hnapt.sport = opt->ing_sp;
		entry->ipv4_hnapt.new_dport = opt->eg_dp;
		entry->ipv4_hnapt.new_sport = opt->eg_sp;
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			entry->ipv6_5t_route.dport = opt->ing_dp;
			entry->ipv6_5t_route.sport = opt->ing_sp;
		}
	}
}

void hw_nat_ib1_info(struct foe_entry *entry, struct hwnat_tuple *opt)
{

	entry->ipv4_hnapt.bfib1.cah = 1;

	if (opt->pkt_type == IPV4_NAPT || opt->pkt_type == IPV4_NAT) {
		entry->ipv4_hnapt.bfib1.pkt_type = opt->pkt_type;
		entry->ipv4_hnapt.bfib1.stc = 1;
		entry->ipv4_hnapt.bfib1.udp = opt->is_udp; /* tcp/udp */
		entry->ipv4_hnapt.bfib1.state = BIND;
		entry->ipv4_hnapt.bfib1.ka = 1; /* keepalive */
		entry->ipv4_hnapt.bfib1.ttl = 1; /* TTL-1 */
		entry->ipv4_hnapt.bfib1.psn = opt->pppoe_act; /* insert / remove */
		entry->ipv4_hnapt.bfib1.vlan_layer = opt->vlan_layer;
		entry->ipv4_hnapt.bfib1.time_stamp = reg_read(FOE_TS) & 0x3FFF;

	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			entry->ipv6_5t_route.bfib1.pkt_type = IPV6_ROUTING;
			entry->ipv6_5t_route.bfib1.stc = 1;
			entry->ipv6_5t_route.bfib1.udp = opt->is_udp; /* tcp/udp */
			entry->ipv6_5t_route.bfib1.state = BIND;
			entry->ipv6_5t_route.bfib1.ka = 0; /* keepalive */
			entry->ipv6_5t_route.bfib1.ttl = 1; /* TTL-1 */
			entry->ipv6_5t_route.bfib1.psn = opt->pppoe_act; /* insert / remove */
			entry->ipv6_5t_route.bfib1.vlan_layer = opt->vlan_layer;
			entry->ipv6_5t_route.bfib1.time_stamp = reg_read(FOE_TS) & 0x3FFF;
		}
	} else if ((opt->pkt_type) == IPV4_DSLITE){
		entry->ipv4_dslite.bfib1.rmt = opt->rmt;
		if (opt->rmt == 0)
			entry->ipv4_dslite.bfib1.pkt_type = IPV4_DSLITE;

		entry->ipv4_dslite.bfib1.stc = 1;
		entry->ipv4_dslite.bfib1.udp = opt->is_udp; /* tcp/udp */
		entry->ipv4_dslite.bfib1.state = BIND;
		entry->ipv4_dslite.bfib1.ka = 0; /* keepalive */
		entry->ipv4_dslite.bfib1.ttl = 1; /* TTL-1 */
		entry->ipv4_dslite.bfib1.vlan_layer = opt->vlan_layer;
		entry->ipv4_dslite.bfib1.time_stamp = reg_read(FOE_TS) & 0x3FFF;
	} else if ((opt->pkt_type) == IPV6_6RD){
		entry->ipv6_6rd.bfib1.rmt = opt->rmt;
		if (opt->rmt == 0)
			entry->ipv6_6rd.bfib1.pkt_type = IPV6_6RD;

		entry->ipv6_6rd.bfib1.stc = 1;
		entry->ipv6_6rd.bfib1.udp = opt->is_udp; /* tcp/udp */
		entry->ipv6_6rd.bfib1.state = BIND;
		entry->ipv6_6rd.bfib1.ka = 0; /* keepalive */
		entry->ipv6_6rd.bfib1.ttl = 1; /* TTL-1 */
		entry->ipv6_6rd.bfib1.vlan_layer = opt->vlan_layer;
		entry->ipv6_6rd.bfib1.time_stamp = reg_read(FOE_TS) & 0x3FFF;
	}
}

void hw_nat_ib2_info(struct foe_entry *entry, struct hwnat_tuple *opt)
{
#if defined(CONFIG_HNAT_V1)
	int dp_mask = 0x7;
#elif defined(CONFIG_HNAT_V2)
	int dp_mask = 0xf;
#endif
	if (opt->pkt_type == IPV4_NAPT || opt->pkt_type == IPV4_NAT) {
		entry->ipv4_hnapt.iblk2.dp = (opt->dst_port & dp_mask); /* 0:cpu, 1:GE1 */
		entry->ipv4_hnapt.iblk2.dscp = opt->dscp;
	#if defined(CONFIG_HNAT_V2)
		/* set ring id */
		entry->ipv4_hnapt.iblk2.dr_idx = opt->dr_idx;
		entry->ipv4_hnapt.iblk2.fdr_en = (opt->dr_idx != 0) ? 1 : 0;

		/* qdma id: */
		entry->ipv4_hnapt.iblk2.qid = opt->qid;
	#elif defined(CONFIG_HNAT_V1)
		if ((opt->dst_port) >= 8) {
		    entry->ipv4_hnapt.iblk2.dp1 = 1;
		} else {
		    entry->ipv4_hnapt.iblk2.dp1 = 0;
		}

		/* qdma id: */
		entry->ipv4_hnapt.iblk2.qid1 = ((opt->qid & 0x30) >> 4);
		entry->ipv4_hnapt.iblk2.qid = (opt->qid & 0x0f);
	#endif /* CONFIG_HNAT_V1 */

		entry->ipv4_hnapt.iblk2.fqos = opt->fqos;

		entry->ipv4_hnapt.iblk2.acnt = opt->dst_port;
		entry->ipv4_hnapt.iblk2.mcast = 0;
		entry->ipv4_hnapt.iblk2.mibf = 1;

	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			entry->ipv6_5t_route.iblk2.dp = (opt->dst_port & dp_mask); /* 0:cpu, 1:GE1 */
			entry->ipv6_5t_route.iblk2.dscp = opt->dscp;
			entry->ipv6_5t_route.iblk2.acnt = opt->dst_port;
			entry->ipv6_5t_route.iblk2.mibf = 1;
		}
	} else if ((opt->pkt_type) == IPV4_DSLITE){
		entry->ipv4_dslite.iblk2.dp = (opt->dst_port & dp_mask); /* 0:cpu, 1:GE1 */
		entry->ipv4_dslite.iblk2.dscp = opt->dscp;
		entry->ipv4_dslite.iblk2.acnt = opt->dst_port;
		entry->ipv4_dslite.iblk2.mibf = 1;
	} else if ((opt->pkt_type) == IPV6_6RD){
		entry->ipv6_6rd.iblk2.dp = (opt->dst_port & dp_mask); /* 0:cpu, 1:GE1 */
		entry->ipv6_6rd.iblk2.dscp = opt->dscp;
		entry->ipv6_6rd.iblk2.acnt = opt->dst_port;
		entry->ipv6_6rd.iblk2.mibf = 1;
	}
}

void hw_nat_semi_bind(struct foe_entry *entry, struct hwnat_tuple *opt)
{
	u32 current_time;

	if ((opt->pkt_type) == IPV4_NAPT) {
		/* Set Current time to time_stamp field in information block 1 */
		current_time = reg_read(FOE_TS) & 0xFFFF;
		entry->bfib1.time_stamp = (uint16_t)current_time;
		/* Ipv4: TTL / Ipv6: Hot Limit filed */
		entry->ipv4_hnapt.bfib1.ttl = DFL_FOE_TTL_REGEN;
		/* enable cache by default */
		entry->ipv4_hnapt.bfib1.cah = 1;
		/* Change Foe Entry State to Binding State */
		entry->bfib1.state = BIND;
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		if (fe_feature & HNAT_IPV6) {
			/* Set Current time to time_stamp field in information block 1 */
			current_time = reg_read(FOE_TS) & 0xFFFF;
			entry->bfib1.time_stamp = (uint16_t)current_time;
			/* Ipv4: TTL / Ipv6: Hot Limit filed */
			entry->ipv4_hnapt.bfib1.ttl = DFL_FOE_TTL_REGEN;
			/* enable cache by default */
			entry->ipv4_hnapt.bfib1.cah = 1;
			/* Change Foe Entry State to Binding State */
			entry->bfib1.state = BIND;
		}
	}
}

int set_done_bit_zero(struct foe_entry *foe_entry)
{
	if (IS_IPV4_HNAT(foe_entry) || IS_IPV4_HNAPT(foe_entry))
		foe_entry->ipv4_hnapt.resv1 = 0;

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(foe_entry)) {
			//foe_entry->ipv4_dslite.resv1 = 0;
		} else if (IS_IPV6_3T_ROUTE(foe_entry)) {
			foe_entry->ipv6_3t_route.resv1 = 0;
		} else if (IS_IPV6_5T_ROUTE(foe_entry)) {
			foe_entry->ipv6_5t_route.resv1 = 0;
		} else if (IS_IPV6_6RD(foe_entry)) {
			foe_entry->ipv6_6rd.resv1 = 0;
		} else {
			pr_notice("%s:get packet format something wrong\n", __func__);
			return -1;
		}
	}
	return 0;
}

int get_entry_done_bit(struct foe_entry *foe_entry)
{
	int done_bit;

	done_bit = 0;
	if (IS_IPV4_HNAT(foe_entry) || IS_IPV4_HNAPT(foe_entry))
		done_bit = foe_entry->ipv4_hnapt.resv1;
#if(0)
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(foe_entry)) {
			done_bit = foe_entry->ipv4_dslite.resv1;
		} else if (IS_IPV6_3T_ROUTE(foe_entry)) {
			done_bit = foe_entry->ipv6_3t_route.resv1;
		} else if (IS_IPV6_5T_ROUTE(foe_entry)) {
			done_bit = foe_entry->ipv6_5t_route.resv1;
		} else if (IS_IPV6_6RD(foe_entry)) {
			done_bit = foe_entry->ipv6_6rd.resv1;
		} else {
			pr_notice("%s:get packet format something wrong\n", __func__);
			return -1;
		}
	}
#endif
	return done_bit;
}

int foe_add_entry_dvt(struct hwnat_tuple *opt)
{

	struct foe_entry *entry = NULL;
	struct hnat_mib_struct *mib_entry_p = NULL;

	s32 hash_index;

	pr_notice("%s, opt->hash_index = %d, FP = %d, RingId = %d, fqos = %d, qid = %d\n", __func__,
		opt->hash_index, opt->dst_port, opt->dr_idx, opt->fqos, opt->qid);

	// auto caculate ipv4 napt hash index
	if (opt->hash_index == 0 && opt->pkt_type == IPV4_NAPT) {

		opt->hash_index = get_hash_ipv4(opt->ing_sipv4, opt->ing_dipv4, opt->ing_sp, opt->ing_dp);

		pr_notice("%s, re-calculate opt->hash_index = %d\n", __func__, opt->hash_index);
	}

	hash_index = opt->hash_index;
	if (hash_index > 0 && hash_index < FOE_4TB_SIZ) {

		if (opt->set_idx == 0) {
			entry = &ppe_foe_base[hash_index];

			if (fe_feature & PPE_MIB)
				mib_entry_p = &hnat_entry_mib[hash_index];

		} else if (opt->set_idx == 1) {
			entry = &ppe1_foe_base[hash_index];

			if (fe_feature & PPE_MIB)
				mib_entry_p = &hnat_entry1_mib[hash_index];
		} else {
			pr_notice("wrong ppe idx (%d)!!!\n", opt->set_idx);
			return HWNAT_FAIL;
		}

		hw_nat_l2_info(entry, opt);
		hw_nat_l3_info(entry, opt);
		if ((opt->pkt_type != IPV4_DSLITE) && (opt->pkt_type != IPV6_6RD))
			hw_nat_l4_info(entry, opt);

		hw_nat_ib1_info(entry, opt);
		hw_nat_ib2_info(entry, opt);

		ppe_set_cache_ebl(opt->set_idx);	/*clear HWNAT cache */

		// reset mib
		if (fe_feature & PPE_MIB)
			ppe_reset_mib_entry(mib_entry_p);

		ppe_start_mib_timer();

		foe_dump_entry(hash_index, entry, opt->set_idx);

		return HWNAT_SUCCESS;
	}
	pr_notice("No entry idx (%d)!!!\n", hash_index);

	return HWNAT_FAIL;
}

int foe_add_entry(struct hwnat_tuple *opt)
{
	struct foe_pri_key key;
	struct foe_entry *entry = NULL;
	s32 hash_index;
#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
	int done_bit;
#endif

	if ((opt->pkt_type) == IPV4_NAPT) {
		key.ipv4_hnapt.sip = opt->ing_sipv4;
		key.ipv4_hnapt.dip = opt->ing_dipv4;
		key.ipv4_hnapt.sport = opt->ing_sp;
		key.ipv4_hnapt.dport = opt->ing_dp;
		key.ipv4_hnapt.is_udp = opt->is_udp;
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		key.ipv6_routing.sip0 = opt->ing_sipv6_0;
		key.ipv6_routing.sip1 = opt->ing_sipv6_1;
		key.ipv6_routing.sip2 = opt->ing_sipv6_2;
		key.ipv6_routing.sip3 = opt->ing_sipv6_3;
		key.ipv6_routing.dip0 = opt->ing_dipv6_0;
		key.ipv6_routing.dip1 = opt->ing_dipv6_1;
		key.ipv6_routing.dip2 = opt->ing_dipv6_2;
		key.ipv6_routing.dip3 = opt->ing_dipv6_3;
		key.ipv6_routing.sport = opt->ing_sp;
		key.ipv6_routing.dport = opt->ing_dp;
		key.ipv6_routing.is_udp = opt->is_udp;
	} else if ((opt->pkt_type) == IPV4_DSLITE) {
		key.ipv4_dslite.sip_v4 = opt->ing_sipv4;
		key.ipv4_dslite.dip_v4 = opt->ing_dipv4;
		key.ipv4_dslite.sip0_v6 = opt->ing_sipv6_0;
		key.ipv4_dslite.sip1_v6 = opt->ing_sipv6_1;
		key.ipv4_dslite.sip2_v6 = opt->ing_sipv6_2;
		key.ipv4_dslite.sip3_v6 = opt->ing_sipv6_3;
		key.ipv4_dslite.dip0_v6 = opt->ing_dipv6_0;
		key.ipv4_dslite.dip1_v6 = opt->ing_dipv6_1;
		key.ipv4_dslite.dip2_v6 = opt->ing_dipv6_2;
		key.ipv4_dslite.dip3_v6 = opt->ing_dipv6_3;
		key.ipv4_dslite.sport = opt->ing_sp;
		key.ipv4_dslite.dport = opt->ing_dp;
		key.ipv4_dslite.is_udp = opt->is_udp;

	}


	key.pkt_type = opt->pkt_type;
#if(0)
	if (fe_feature & MANUAL_MODE)
		hash_index = get_ppe_entry_idx(&key, entry, 0);
	else
		hash_index = get_ppe_entry_idx(&key, entry, 1);
#endif

	hash_index = opt->hash_index;
	pr_notice("opt->hash_index = %d\n", opt->hash_index);
	if (hash_index != -1) {
		//opt->hash_index = hash_index;
		entry =  &ppe_foe_base[hash_index];
		//if (fe_feature & MANUAL_MODE) {
		hw_nat_l2_info(entry, opt);
		hw_nat_l3_info(entry, opt);
		hw_nat_ib1_info(entry, opt);
		hw_nat_ib2_info(entry, opt);
		//}
#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
		done_bit = get_entry_done_bit(entry);
		if (done_bit == 1)
			pr_notice("mtk_entry_add number =%d\n", hash_index);
		else if (done_bit == 0)
			pr_notice("ppe table not ready\n");
		else
			pr_notice("%s: done_bit something wrong\n", __func__);

		if (done_bit != 1)
			return HWNAT_FAIL;
		hw_nat_semi_bind(entry, opt);
#endif
		foe_dump_entry(hash_index, entry, 0);
		return HWNAT_SUCCESS;
	}

	return HWNAT_FAIL;
}

int foe_del_entry(struct hwnat_tuple *opt)
{
	struct foe_pri_key key;
	s32 hash_index;
	struct foe_entry *entry = NULL;
	s32 rply_idx;
#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
	int done_bit;
#endif

	if (opt->set_idx != 0 && opt->set_idx != 1) {
		pr_notice("wrong ppe idx (%d)!!!\n", opt->set_idx);
		return HWNAT_ENTRY_NOT_FOUND;
	}

	if ((opt->pkt_type) == IPV4_NAPT) {
		key.ipv4_hnapt.sip = opt->ing_sipv4;
		key.ipv4_hnapt.dip = opt->ing_dipv4;
		key.ipv4_hnapt.sport = opt->ing_sp;
		key.ipv4_hnapt.dport = opt->ing_dp;
		/* key.ipv4_hnapt.is_udp=opt->is_udp; */
	} else if ((opt->pkt_type) == IPV6_ROUTING) {
		key.ipv6_routing.sip0 = opt->ing_sipv6_0;
		key.ipv6_routing.sip1 = opt->ing_sipv6_1;
		key.ipv6_routing.sip2 = opt->ing_sipv6_2;
		key.ipv6_routing.sip3 = opt->ing_sipv6_3;
		key.ipv6_routing.dip0 = opt->ing_dipv6_0;
		key.ipv6_routing.dip1 = opt->ing_dipv6_1;
		key.ipv6_routing.dip2 = opt->ing_dipv6_2;
		key.ipv6_routing.dip3 = opt->ing_dipv6_3;
		key.ipv6_routing.sport = opt->ing_sp;
		key.ipv6_routing.dport = opt->ing_dp;
		/* key.ipv6_routing.is_udp=opt->is_udp; */
	}

	key.pkt_type = opt->pkt_type;

	/* find bind entry */
	/* hash_index = FoeHashFun(&key,BIND); */
	hash_index = get_ppe_entry_idx(&key, entry, 1);
	if (hash_index != -1) {
		opt->hash_index = hash_index;
		rply_idx = reply_entry_idx(opt, hash_index);
#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
			entry =  &ppe_foe_base[hash_index];
			done_bit = get_entry_done_bit(entry);
			if (done_bit == 1) {
				set_done_bit_zero(entry);
			} else if (done_bit == 0) {
				pr_notice("%s : ppe table not ready\n", __func__);
			} else {
				pr_notice("%s: done_bit something wrong\n", __func__);
				set_done_bit_zero(entry);
			}
			if (done_bit != 1)
				return HWNAT_FAIL;
#endif
		foe_del_entry_by_num(hash_index, opt->set_idx);
		pr_notice("Clear Entry index = %d, ppe index = %d\n", hash_index, opt->set_idx);
		if (rply_idx != -1) {
		pr_notice("Clear Entry index = %d\n", rply_idx);
			foe_del_entry_by_num(rply_idx, opt->set_idx);
		}

		return HWNAT_SUCCESS;
	}
	pr_notice("HWNAT ENTRY NOT FOUND\n");
	return HWNAT_ENTRY_NOT_FOUND;
}
EXPORT_SYMBOL(foe_del_entry);

int get_five_tule(struct sk_buff *skb)
{
	struct ethhdr *eth = NULL;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;
	u8 ipv6_head_len = 0;

	memset(&ppe_parse_rx_result, 0, sizeof(ppe_parse_rx_result));
	eth = (struct ethhdr *)skb->data;
	ppe_parse_rx_result.eth_type = eth->h_proto;
	/* set layer4 start addr */
	if ((ppe_parse_rx_result.eth_type == htons(ETH_P_IP)) ||
	    (ppe_parse_rx_result.eth_type == htons(ETH_P_PPP_SES) &&
	    (ppe_parse_rx_result.ppp_tag == htons(PPP_IP)))) {
		iph = (struct iphdr *)(skb->data + ETH_HLEN);
		memcpy(&ppe_parse_rx_result.iph, iph, sizeof(struct iphdr));
		if (iph->protocol == IPPROTO_TCP) {
			skb_set_transport_header(skb, ETH_HLEN + (iph->ihl * 4));
			th = (struct tcphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_rx_result.th, th, sizeof(struct tcphdr));
			ppe_parse_rx_result.pkt_type = IPV4_HNAPT;
			if (iph->frag_off & htons(IP_MF | IP_OFFSET)) {
				if (debug_level >= 2)
					DD;
				return 1;
			}
		} else if (iph->protocol == IPPROTO_UDP) {
			skb_set_transport_header(skb, ETH_HLEN + (iph->ihl * 4));
			uh = (struct udphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_rx_result.uh, uh, sizeof(struct udphdr));
			ppe_parse_rx_result.pkt_type = IPV4_HNAPT;
			if (iph->frag_off & htons(IP_MF | IP_OFFSET)) {
				if (USE_3T_UDP_FRAG == 0)
					return 1;
			}
		} else if (iph->protocol == IPPROTO_GRE) {
			if (debug_level >= 2)
				/* do nothing */
				return 1;
		}
		if (fe_feature & HNAT_IPV6) {
			if (iph->protocol == IPPROTO_IPV6) {
				ip6h = (struct ipv6hdr *)((uint8_t *)iph + iph->ihl * 4);
				memcpy(&ppe_parse_rx_result.ip6h, ip6h, sizeof(struct ipv6hdr));
				if (ip6h->nexthdr == NEXTHDR_TCP) {
					skb_set_transport_header(skb, ETH_HLEN + (sizeof(struct ipv6hdr)));
					th = (struct tcphdr *)skb_transport_header(skb);
					memcpy(&ppe_parse_rx_result.th.source, &th->source, sizeof(th->source));
					memcpy(&ppe_parse_rx_result.th.dest, &th->dest, sizeof(th->dest));
				} else if (ip6h->nexthdr == NEXTHDR_UDP) {
					skb_set_transport_header(skb, ETH_HLEN + (sizeof(struct ipv6hdr)));
					uh = (struct udphdr *)skb_transport_header(skb);
					memcpy(&ppe_parse_rx_result.uh.source, &uh->source, sizeof(uh->source));
					memcpy(&ppe_parse_rx_result.uh.dest, &uh->dest, sizeof(uh->dest));
				}
					ppe_parse_rx_result.pkt_type = IPV6_6RD;
		/* identification field in outer ipv4 header is zero*/
		/*after erntering binding state.*/
		/* some 6rd relay router will drop the packet */
			}
		}
		if ((iph->protocol != IPPROTO_TCP) && (iph->protocol != IPPROTO_UDP) &&
		    (iph->protocol != IPPROTO_GRE) && (iph->protocol != IPPROTO_IPV6))
			return 1;
				/* Packet format is not supported */

	} else if (ppe_parse_rx_result.eth_type == htons(ETH_P_IPV6) ||
		   (ppe_parse_rx_result.eth_type == htons(ETH_P_PPP_SES) &&
		    ppe_parse_rx_result.ppp_tag == htons(PPP_IPV6))) {
		ip6h = (struct ipv6hdr *)skb_network_header(skb);
		memcpy(&ppe_parse_rx_result.ip6h, ip6h, sizeof(struct ipv6hdr));
		if (ip6h->nexthdr == NEXTHDR_TCP) {
			skb_set_transport_header(skb, ETH_HLEN + (sizeof(struct ipv6hdr)));
			th = (struct tcphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_rx_result.th, th, sizeof(struct tcphdr));
			ppe_parse_rx_result.pkt_type = IPV6_5T_ROUTE;
		} else if (ip6h->nexthdr == NEXTHDR_UDP) {
			skb_set_transport_header(skb, ETH_HLEN + (sizeof(struct ipv6hdr)));
			uh = (struct udphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_rx_result.uh, uh, sizeof(struct udphdr));
			ppe_parse_rx_result.pkt_type = IPV6_5T_ROUTE;
		} else if (ip6h->nexthdr == NEXTHDR_IPIP) {
			ipv6_head_len = sizeof(struct iphdr);
			memcpy(&ppe_parse_rx_result.iph, ip6h + ipv6_head_len,
			       sizeof(struct iphdr));
			ppe_parse_rx_result.pkt_type = IPV4_DSLITE;
		} else {
			ppe_parse_rx_result.pkt_type = IPV6_3T_ROUTE;
		}

	} else {
				if (debug_level >= 2)
					DD;
		return 1;
	}
	return 0;
}

int decide_qid(u16 hash_index, struct sk_buff *skb)
{
	struct foe_entry *entry;
	u32 saddr;
	u32 daddr;

	u32 ppe_saddr;
	u32 ppe_daddr;
	u32 ppe_sport;
	u32 ppe_dport;

	u32 sport = 0;
	u32 dport = 0;

	u32 ipv6_sip_127_96;
	u32 ipv6_sip_95_64;
	u32 ipv6_sip_63_32;
	u32 ipv6_sip_31_0;

	u32 ipv6_dip_127_96;
	u32 ipv6_dip_95_64;
	u32 ipv6_dip_63_32;
	u32 ipv6_dip_31_0;

	u32 ppe_saddr_127_96;
	u32 ppe_saddr_95_64;
	u32 ppe_saddr_63_32;
	u32 ppe_saddr_31_0;

	u32 ppe_daddr_127_96;
	u32 ppe_daddr_95_64;
	u32 ppe_daddr_63_32;
	u32 ppe_daddr_31_0;

	u32 ppe_sportv6;
	u32 ppe_dportv6;

	entry = &ppe_foe_base[hash_index];
	if (IS_IPV4_HNAPT(entry)) {
		saddr = ntohl(ppe_parse_rx_result.iph.saddr);
		daddr = ntohl(ppe_parse_rx_result.iph.daddr);
		if (ppe_parse_rx_result.iph.protocol == IPPROTO_TCP) {
			sport = ntohs(ppe_parse_rx_result.th.source);
			dport = ntohs(ppe_parse_rx_result.th.dest);
		} else if (ppe_parse_rx_result.iph.protocol == IPPROTO_UDP) {
			sport = ntohs(ppe_parse_rx_result.uh.source);
			dport = ntohs(ppe_parse_rx_result.uh.dest);
		}
		ppe_saddr = entry->ipv4_hnapt.sip;
		ppe_daddr = entry->ipv4_hnapt.dip;
		ppe_sport = entry->ipv4_hnapt.sport;
		ppe_dport = entry->ipv4_hnapt.dport;
		if (debug_level >= 2) {
			pr_notice("ppe_saddr = %x, ppe_daddr=%x, ppe_sport=%d, ppe_dport=%d, saddr=%x, daddr=%x, sport= %d, dport=%d\n",
				ppe_saddr, ppe_daddr, ppe_sport, ppe_dport, saddr, daddr, sport, dport);
		}
		if ((saddr == ppe_saddr) && (daddr == ppe_daddr) &&
		    (sport == ppe_sport) && (dport == ppe_dport) &&
		    (entry->bfib1.state == BIND)) {
			if (entry->ipv4_hnapt.iblk2.dp == 2 && DP_GMAC2 >= 1 && DP_GMAC2 < MAX_IF_NUM) {
				skb->dev = dst_port[DP_GMAC2];
				if (debug_level >= 2)
					pr_notice("qid = %d\n", entry->ipv4_hnapt.iblk2.qid);
				skb->mark = entry->ipv4_hnapt.iblk2.qid;
			} else if (entry->ipv4_hnapt.iblk2.dp == 1 && DP_GMAC1 >= 1 && DP_GMAC1 < MAX_IF_NUM) {
				skb->dev = dst_port[DP_GMAC1];
				if (debug_level >= 2)
					pr_notice("qid = %d\n", entry->ipv4_hnapt.iblk2.qid);
				skb->mark = entry->ipv4_hnapt.iblk2.qid;
			}
			return 0;
		} else {
			return -1;
		}
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_5T_ROUTE(entry)) {
			ipv6_sip_127_96 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[0]);
			ipv6_sip_95_64 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[1]);
			ipv6_sip_63_32 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[2]);
			ipv6_sip_31_0 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[3]);

			ipv6_dip_127_96 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[0]);
			ipv6_dip_95_64 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[1]);
			ipv6_dip_63_32 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[2]);
			ipv6_dip_31_0 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[3]);

			ppe_saddr_127_96 = entry->ipv6_5t_route.ipv6_sip0;
			ppe_saddr_95_64 = entry->ipv6_5t_route.ipv6_sip1;
			ppe_saddr_63_32 = entry->ipv6_5t_route.ipv6_sip2;
			ppe_saddr_31_0 = entry->ipv6_5t_route.ipv6_sip3;

			ppe_daddr_127_96 = entry->ipv6_5t_route.ipv6_dip0;
			ppe_daddr_95_64 = entry->ipv6_5t_route.ipv6_dip1;
			ppe_daddr_63_32 = entry->ipv6_5t_route.ipv6_dip2;
			ppe_daddr_31_0 = entry->ipv6_5t_route.ipv6_dip3;

			ppe_sportv6 = entry->ipv6_5t_route.sport;
			ppe_dportv6 = entry->ipv6_5t_route.dport;
			if (ppe_parse_rx_result.iph.protocol == IPPROTO_TCP) {
				sport = ntohs(ppe_parse_rx_result.th.source);
				dport = ntohs(ppe_parse_rx_result.th.dest);
			} else if (ppe_parse_rx_result.iph.protocol == IPPROTO_UDP) {
				sport = ntohs(ppe_parse_rx_result.uh.source);
				dport = ntohs(ppe_parse_rx_result.uh.dest);
			}
			if ((ipv6_sip_127_96 == ppe_saddr_127_96) && (ipv6_sip_95_64 == ppe_saddr_95_64) &&
			    (ipv6_sip_63_32 == ppe_saddr_63_32) && (ipv6_sip_31_0 == ppe_saddr_31_0) &&
			    (ipv6_dip_127_96 == ppe_daddr_127_96) && (ipv6_dip_95_64 == ppe_daddr_95_64) &&
			    (ipv6_dip_63_32 == ppe_daddr_63_32) && (ipv6_dip_31_0 == ppe_daddr_31_0) &&
			    (sport == ppe_sportv6) && (dport == ppe_dportv6) &&
			    (entry->bfib1.state == BIND)) {
				if (entry->ipv6_5t_route.iblk2.dp == 2 && DP_GMAC2 >= 1 && DP_GMAC2 < MAX_IF_NUM) {
					skb->dev = dst_port[DP_GMAC2];
						/* if (entry->ipv6_3t_route.iblk2.qid >= 11) */
					skb->mark = (entry->ipv6_3t_route.iblk2.qid);
				} else if (entry->ipv6_5t_route.iblk2.dp == 1 && DP_GMAC1 >= 1 && DP_GMAC1 < MAX_IF_NUM) {
					skb->dev = dst_port[DP_GMAC1];
					skb->mark = (entry->ipv6_3t_route.iblk2.qid);
				}
			} else {
				return -1;
			}
		}
	}
	return 0;
}

void set_qid(struct sk_buff *skb)
{
	struct foe_pri_key key = {0};
	s32 hash_index;
	struct foe_entry *entry = NULL;

	get_five_tule(skb);
	if (ppe_parse_rx_result.pkt_type == IPV4_HNAPT) {
		key.ipv4_hnapt.sip = ntohl(ppe_parse_rx_result.iph.saddr);
		key.ipv4_hnapt.dip = ntohl(ppe_parse_rx_result.iph.daddr);

		if (ppe_parse_rx_result.iph.protocol == IPPROTO_TCP) {
			key.ipv4_hnapt.sport = ntohs(ppe_parse_rx_result.th.source);
			key.ipv4_hnapt.dport = ntohs(ppe_parse_rx_result.th.dest);
		} else if (ppe_parse_rx_result.iph.protocol == IPPROTO_UDP) {
			key.ipv4_hnapt.sport = ntohs(ppe_parse_rx_result.uh.source);
			key.ipv4_hnapt.dport = ntohs(ppe_parse_rx_result.uh.dest);
		}
		/* key.ipv4_hnapt.is_udp=opt->is_udp; */
	} else if (ppe_parse_rx_result.pkt_type == IPV6_5T_ROUTE) {
		key.ipv6_routing.sip0 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[0]);
		key.ipv6_routing.sip1 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[1]);
		key.ipv6_routing.sip2 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[2]);
		key.ipv6_routing.sip3 = ntohl(ppe_parse_rx_result.ip6h.saddr.s6_addr32[3]);
		key.ipv6_routing.dip0 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[0]);
		key.ipv6_routing.dip1 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[1]);
		key.ipv6_routing.dip2 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[2]);
		key.ipv6_routing.dip3 = ntohl(ppe_parse_rx_result.ip6h.daddr.s6_addr32[3]);
		if (ppe_parse_rx_result.ip6h.nexthdr == IPPROTO_TCP) {
			key.ipv6_routing.sport = ntohs(ppe_parse_rx_result.th.source);
			key.ipv6_routing.dport = ntohs(ppe_parse_rx_result.th.dest);
		} else if (ppe_parse_rx_result.ip6h.nexthdr == IPPROTO_UDP) {
			key.ipv6_routing.sport = ntohs(ppe_parse_rx_result.uh.source);
			key.ipv6_routing.dport = ntohs(ppe_parse_rx_result.uh.dest);
		}
	}

	key.pkt_type = ppe_parse_rx_result.pkt_type;

	/* find bind entry */
	/* hash_index = FoeHashFun(&key,BIND); */
	hash_index = get_ppe_entry_idx(&key, entry, 1);
	if (hash_index != -1)
		decide_qid(hash_index, skb);
	if (debug_level >= 6)
		pr_notice("hash_index = %d\n", hash_index);
}
