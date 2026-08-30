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
#include <linux/seq_file.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include "ra_nat.h"
#include "frame_engine.h"
#include "foe_fdb.h"
#include "hnat_ioctl.h"
#include "util.h"
#include "hnat_config.h"
#include "hnat_define.h"
#include "hnat_dbg_proc.h"
#include "mcast_tbl.h"
#include "hnat_common.h"
#include "hnat_fast.h"
#ifdef CONFIG_TUNNEL_FAST_PATH
#include "hnat_tnl.h"
#endif /* CONFIG_TUNNEL_FAST_PATH */

extern u32 rndis_mod;

extern struct foe_entry *ppe_foe_base;
extern struct foe_entry *ppe1_foe_base;
extern struct mib_entry *ppe_mib_base;
extern struct mib_entry *ppe1_mib_base;

extern void ppe_set_fe_dma_tx_hw_vlan(bool on);
extern void ppe_set_fe_dma_rx_hw_vlan(bool on);

struct proc_dir_entry *hnat_proc_reg_dir;
static struct proc_dir_entry *proc_hnat_usage;
static struct proc_dir_entry *proc_cpu_reason;
static struct proc_dir_entry *proc_hnat_entry;
static struct proc_dir_entry *proc_hnat_setting;
static struct proc_dir_entry *proc_hnat_multicast;
static struct proc_dir_entry *proc_hnat_whitelist;
static struct proc_dir_entry *proc_hnat_type;
static struct proc_dir_entry *proc_hnat_qos;
static struct proc_dir_entry *proc_hnat_mib;
static struct proc_dir_entry *proc_hnat_med;
static struct proc_dir_entry *proc_hnat_dma;
static struct proc_dir_entry *proc_hnat_dbg;
static struct proc_dir_entry *proc_hnat_tunnel;

int dbg_cpu_reason;
EXPORT_SYMBOL(dbg_cpu_reason);

struct hwnat_interface hnat_if[MAX_IF_NUM];
EXPORT_SYMBOL(hnat_if);

/* store mib per entry: ppe0 and ppe1 */
struct hnat_mib_struct hnat_entry_mib[FOE_4TB_SIZ];
struct hnat_mib_struct hnat_entry1_mib[FOE_4TB_SIZ];
bool ppe_mib_counter_en;

static bool eswcnt_loop_enable;
static u32 eswcnt_loop_count;  /* 500 is about 5s, 1000 is about 10s */

int dbg_entry_state = BIND;
typedef int (*CPU_REASON_SET_FUNC) (int par1, int par2, int par3);
typedef int (*ENTRY_SET_FUNC) (int par1, int par2, int par3);
typedef int (*CR_SET_FUNC) (u32 par1, u32 par2, u32 par3);
typedef int (*MULTICAST_SET_FUNC) (int par1, int par2, int par3);
typedef int (*WHITELIST_SET_FUNC) (int par1, char *par2, int par3);
typedef int (*TYPE_SET_FUNC) (int par1, char *par2, int par3);
typedef int (*QOS_SET_FUNC) (int par1, int par2, int par3, int par4, int par5);
typedef int (*MIB_SET_FUNC) (int par1, int par2, char *par3);
typedef int (*DMA_SET_FUNC) (u64 par1, u64 par2, u64 par3);
typedef int (*TNL_SET_FUNC) (int par1, int par2, int par3);

int hnat_dump_type;

struct dump_dma_struct {
	char name[10];
	bool iomem;
	void *dma_tx_base;
	void *dma_tx_size;
	void *dma_rx_base;
	void *dma_rx_size;
};

u32 hnat_dbg_reg;


void dbg_dump_entry(uint32_t index, struct foe_entry *entry)
{
	if (IS_IPV4_HNAPT(entry)) {
		NAT_PRINT
		    ("NAPT(%d): %u.%u.%u.%u:%d->%u.%u.%u.%u:%d ->", index,
		     IP_FORMAT3(entry->ipv4_hnapt.sip),
		     IP_FORMAT2(entry->ipv4_hnapt.sip),
		     IP_FORMAT1(entry->ipv4_hnapt.sip),
		     IP_FORMAT0(entry->ipv4_hnapt.sip),
		     entry->ipv4_hnapt.sport,
		     IP_FORMAT3(entry->ipv4_hnapt.dip), IP_FORMAT2(entry->ipv4_hnapt.dip),
		     IP_FORMAT1(entry->ipv4_hnapt.dip), IP_FORMAT0(entry->ipv4_hnapt.dip),
		     entry->ipv4_hnapt.dport);
		NAT_PRINT
		    (" %u.%u.%u.%u:%d->%u.%u.%u.%u:%d\n",
		     IP_FORMAT3(entry->ipv4_hnapt.new_sip),
		     IP_FORMAT2(entry->ipv4_hnapt.new_sip),
		     IP_FORMAT1(entry->ipv4_hnapt.new_sip),
		     IP_FORMAT0(entry->ipv4_hnapt.new_sip), entry->ipv4_hnapt.new_sport,
		     IP_FORMAT3(entry->ipv4_hnapt.new_dip),
		     IP_FORMAT2(entry->ipv4_hnapt.new_dip),
		     IP_FORMAT1(entry->ipv4_hnapt.new_dip),
		     IP_FORMAT0(entry->ipv4_hnapt.new_dip), entry->ipv4_hnapt.new_dport);
	} else if (IS_IPV4_HNAT(entry)) {
		NAT_PRINT("NAT(%d): %u.%u.%u.%u->%u.%u.%u.%u ->", index,
			  IP_FORMAT3(entry->ipv4_hnapt.sip),
			  IP_FORMAT2(entry->ipv4_hnapt.sip),
			  IP_FORMAT1(entry->ipv4_hnapt.sip),
			  IP_FORMAT0(entry->ipv4_hnapt.sip),
			  IP_FORMAT3(entry->ipv4_hnapt.dip),
			  IP_FORMAT2(entry->ipv4_hnapt.dip),
			  IP_FORMAT1(entry->ipv4_hnapt.dip), IP_FORMAT0(entry->ipv4_hnapt.dip));
		NAT_PRINT(" %u.%u.%u.%u->%u.%u.%u.%u\n",
			  IP_FORMAT3(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT2(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT1(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT0(entry->ipv4_hnapt.new_sip),
			  IP_FORMAT3(entry->ipv4_hnapt.new_dip),
			  IP_FORMAT2(entry->ipv4_hnapt.new_dip),
			  IP_FORMAT1(entry->ipv4_hnapt.new_dip),
			  IP_FORMAT0(entry->ipv4_hnapt.new_dip));
	}

	if (IS_IPV6_1T_ROUTE(entry)) {
		NAT_PRINT("IPv6_1T(%d): %08X:%08X:%08X:%08X\n", index,
			  entry->ipv6_1t_route.ipv6_dip3,
			  entry->ipv6_1t_route.ipv6_dip2,
			  entry->ipv6_1t_route.ipv6_dip1, entry->ipv6_1t_route.ipv6_dip0);
	} else if (IS_IPV4_DSLITE(entry)) {
		NAT_PRINT
		    ("IPv4 Ds-Lite(%d): %u.%u.%u.%u.%d->%u.%u.%u.%u:%d ->", index,
		     IP_FORMAT3(entry->ipv4_dslite.sip),
		     IP_FORMAT2(entry->ipv4_dslite.sip),
		     IP_FORMAT1(entry->ipv4_dslite.sip),
		     IP_FORMAT0(entry->ipv4_dslite.sip), entry->ipv4_dslite.sport,
		     IP_FORMAT3(entry->ipv4_dslite.dip),
		     IP_FORMAT2(entry->ipv4_dslite.dip),
		     IP_FORMAT1(entry->ipv4_dslite.dip),
		     IP_FORMAT0(entry->ipv4_dslite.dip), entry->ipv4_dslite.dport);
		NAT_PRINT(" %08X:%08X:%08X:%08X->%08X:%08X:%08X:%08X\n",
			  entry->ipv4_dslite.tunnel_sipv6_0,
			  entry->ipv4_dslite.tunnel_sipv6_1,
			  entry->ipv4_dslite.tunnel_sipv6_2,
			  entry->ipv4_dslite.tunnel_sipv6_3,
			  entry->ipv4_dslite.tunnel_dipv6_0,
			  entry->ipv4_dslite.tunnel_dipv6_1,
			  entry->ipv4_dslite.tunnel_dipv6_2, entry->ipv4_dslite.tunnel_dipv6_3);
	} else if (IS_IPV6_3T_ROUTE(entry)) {
		NAT_PRINT
		    ("IPv6_3T(%d): %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X (Prot=%d)\n",
		     index,
		     entry->ipv6_3t_route.ipv6_sip0,
		     entry->ipv6_3t_route.ipv6_sip1,
		     entry->ipv6_3t_route.ipv6_sip2,
		     entry->ipv6_3t_route.ipv6_sip3,
		     entry->ipv6_3t_route.ipv6_dip0,
		     entry->ipv6_3t_route.ipv6_dip1,
		     entry->ipv6_3t_route.ipv6_dip2,
		     entry->ipv6_3t_route.ipv6_dip3, entry->ipv6_3t_route.prot);
	} else if (IS_IPV6_5T_ROUTE(entry)) {
		if (IS_IPV6_FLAB_EBL()) {
			NAT_PRINT
			    ("IPv6_5T(%d): %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X",
			     index,
			     entry->ipv6_5t_route.ipv6_sip0,
			     entry->ipv6_5t_route.ipv6_sip1,
			     entry->ipv6_5t_route.ipv6_sip2,
			     entry->ipv6_5t_route.ipv6_sip3,
			     entry->ipv6_5t_route.ipv6_dip0,
			     entry->ipv6_5t_route.ipv6_dip1,
			     entry->ipv6_5t_route.ipv6_dip2, entry->ipv6_5t_route.ipv6_dip3);
			NAT_PRINT("(Flow Label=%08X)\n",
				  ((entry->ipv6_5t_route.
				    sport << 16) | (entry->ipv6_5t_route.dport)) & 0xFFFFF);
		} else {
			NAT_PRINT
			    ("IPv6_5T(%d): %08X:%08X:%08X:%08X:%d-> ",
			     index,
			     entry->ipv6_5t_route.ipv6_sip0,
			     entry->ipv6_5t_route.ipv6_sip1,
			     entry->ipv6_5t_route.ipv6_sip2,
			     entry->ipv6_5t_route.ipv6_sip3, entry->ipv6_5t_route.sport);
			NAT_PRINT("%08X:%08X:%08X:%08X:%d\n",
				  entry->ipv6_5t_route.ipv6_dip0,
				  entry->ipv6_5t_route.ipv6_dip1,
				  entry->ipv6_5t_route.ipv6_dip2,
				  entry->ipv6_5t_route.ipv6_dip3, entry->ipv6_5t_route.dport);
		}
	} else if (IS_IPV6_6RD(entry)) {
		if (IS_IPV6_FLAB_EBL()) {
			NAT_PRINT
			    ("IPv6_6RD(%d): %08X:%08X:%08X:%08X-> %08X:%08X:%08X:%08X",
			     index,
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.ipv6_dip0, entry->ipv6_6rd.ipv6_dip1,
			     entry->ipv6_6rd.ipv6_dip2, entry->ipv6_6rd.ipv6_dip3);
			NAT_PRINT("(Flow Label=%08X)\n",
				  ((entry->ipv6_5t_route.
				    sport << 16) | (entry->ipv6_5t_route.dport)) & 0xFFFFF);
		} else {
			NAT_PRINT
			    ("IPv6_6RD(%d): %08X:%08X:%08X:%08X:%d-> ",
			     index,
			     entry->ipv6_6rd.ipv6_sip0, entry->ipv6_6rd.ipv6_sip1,
			     entry->ipv6_6rd.ipv6_sip2, entry->ipv6_6rd.ipv6_sip3,
			     entry->ipv6_6rd.sport);
			NAT_PRINT(" %08X:%08X:%08X:%08X:%d\n", entry->ipv6_6rd.ipv6_dip0,
				  entry->ipv6_6rd.ipv6_dip1, entry->ipv6_6rd.ipv6_dip2,
				  entry->ipv6_6rd.ipv6_dip3, entry->ipv6_6rd.dport);
		}
	}
}

void dbg_dump_cr(struct seq_file *seq)
{
	u32 i;

	seq_puts(seq, "+-----------------------------------------------+\n");
	for (i = 0; i < MTK_FE_RANGE; i += 0x10) {

		seq_printf(seq, "%08x: %08x %08x %08x %08x\n", MTK_FE_BASE + i,
			reg_read(fe_base + i), reg_read(fe_base + i + 4),
			reg_read(fe_base + i + 8), reg_read(fe_base + i + 0xc));
	}
	seq_puts(seq, "-------------------------------------------------\n");
}

int hnat_set_usage(int ignore, int ignore2, int ignore3)
{
	pr_notice("[Usage: Get all CPU reason count] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_CPU_REASON);

	pr_notice("[Usage: Dump entry having the CPU reason] echo 1 [cpu_reason] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_CPU_REASON);

	pr_notice("  (2)IPv4(IPv6) TTL(hop limit) = 0\n");
	pr_notice("  (3)IPv4(IPv6) has option(extension) header\n");
	pr_notice("  (7)No flow is assigned\n");
	pr_notice("  (8)IPv4 HNAT doesn't support IPv4 /w fragment\n");
	pr_notice("  (9)IPv4 HNAPT/DS-Lite doesn't support IPv4 /w fragment\n");
	pr_notice("  (10)IPv4 HNAPT/DS-Lite can't find TCP/UDP sport/dport\n");
	pr_notice("  (11)IPv6 5T-route/6RD can't find TCP/UDP sport/dport\n");
	pr_notice("  (12) Ingress packet is TCP fin/syn/rst\n");
	pr_notice("  (13) FOE Un-hit\n");
	pr_notice("  (14) FOE Hit unbind\n");
	pr_notice("  (15) FOE Hit unbind & rate reach\n");
	pr_notice("  (16) Hit bind PPE TCP FIN entry\n");
	pr_notice("  (17) Hit bind PPE entry and TTL(hop limit) = 1\n");
	pr_notice("  (18) Hit bind and VLAN replacement violation\n");
	pr_notice("  (19) Hit bind and keep alive with unicast old-header packet\n");
	pr_notice("  (20) Hit bind and keep alive with multicast new-header packet\n");
	pr_notice("  (21) Hit bind and keep alive with duplicate old-header packet\n");
	pr_notice("  (22) FOE Hit bind & force to CPU\n");
	/* Hit bind and remove tunnel IP header, */
	/* but inner IP has option/next header */
	pr_notice("  (23) HIT_BIND_WITH_OPTION_HEADER\n");
	pr_notice("  (28) Hit bind and exceed MTU\n");
	pr_notice("  (27) HIT_BIND_PACKET_SAMPLING\n");
	pr_notice("  (24) Switch clone multicast packet to CPU\n");
	pr_notice("  (25) Switch clone multicast packet to GMAC1 & CPU\n");
	pr_notice("  (26) HIT_PRE_BIND\n");

	return 0;
}

int entry_set_usage(int ignore, int ignore2, int ignore3)
{
	pr_notice("[Usage: Get all bind entry] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_ENTRY);

	pr_notice("[Usage: set entry state] echo 1 [STATE] > /proc/%s/%s \n",
		HNAT_PROCREG_DIR, PROCREG_PPE_ENTRY);
	pr_notice("  STATE (INVALID:0, UNBIND:1, BIND:2, FIN:3)\n");
	pr_notice("[Usage: get entry detail] echo 2 [index] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_ENTRY);
	pr_notice("[Usage: delete entry] echo 3 [index] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_ENTRY);

	return 0;
}

int cr_set_usage(u32 level, u32 ignore2, u32 ignore3)
{
	pr_notice("[Usage: get FE CR] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set debug level] echo 0 [level] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set binding threshold] echo 1 [threshold] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set bind lifetime] echo 2 [tcp_life] [udp_life] [fin_life] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set keep alive interval] echo 3 [tcp_interval] [udp_interval] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: enable 464XLAT] echo 4 [1:enable,0:disable] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: read reg] echo 5 [step(in hex)] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: write reg] echo 6 [step(in hex)] [value(in hex)] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: dump esw cnt] echo 7 [1:enable, 0:disable] [loop counnt (500:~5s)] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: enable ppe_edma_hw_vlan] echo 8 [1:Tx, 0:Rx] [1:enable, 0:disable] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set rndis_mod] echo 9 [1~3] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	pr_notice("[Usage: set forbid bind ecn] echo 10 [0:disable, 1:bi-dir, 2:downlink, 3:uplink] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	debug_level = level;

	pr_notice("debug_level = %d\n", debug_level);

	return 0;
}

int multicast_set_usage(int ignore, int ignore2, int ignore3)
{
	pr_notice("[Usage: get hnat multicast table] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MULTICAST);

	return 0;
}

int whitelist_set_usage(int ignore, char *ignore2, int ignore3)
{
	pr_notice("[Usage: get hnat whitelist table] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_WHITELIST);
	pr_notice("[Usage: set hnat whitelist table] echo 1 rax /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_WHITELIST);
	return 0;
}

int mib_set_usage(int ignore, int ignore2, char *ignore3)
{
	pr_notice("[Usage: set which interface getting mib] echo 1 [rax] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MIB);

	pr_notice("[Usage: set entry index getting mib] echo 2 [entry_idx] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MIB);

	pr_notice("[Usage: set mib counter enable/disable] echo 3 [disable:0/enable:1] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MIB);

	pr_notice("[Usage: set accounting group get mib] echo 4 [ac_grp] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MIB);

	pr_notice("[Usage: set entry and ppe index get ipv4 napt mib] echo 5 [entry_idx][ppe_idx] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MIB);

	return 0;
}

int type_set_usage(int ignore, char *ignore2, int ignore3)
{
	pr_notice("[Usage: get hnat type table] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_TYPE);
	pr_notice("[Usage: set hnat interface SW fast] echo 1 rax /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_TYPE);
	pr_notice("[Usage: set hnat interface HW fast] echo 2 rax /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_TYPE);

	return 0;
}

int qos_get_usage(void)
{
	pr_notice("[Usage: get qos state] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	pr_notice("[Usage: set qos state] echo 0 [enable] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	pr_notice("[Usage: set qos state] echo 0 1 [disable_fport1] [disable_fport2] [disable_fport3] [disable_fport4] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	pr_notice("[Usage: set eth qos enable] echo 1 [enable] > /proc/%s/%s\n",
			HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	pr_notice("[Usage: set md qos enable] echo 2 [enable] > /proc/%s/%s\n",
			HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	pr_notice("[Usage: set wifi qos enable] echo 3 [enable] > /proc/%s/%s\n",
			HNAT_PROCREG_DIR, PROCREG_PPE_QOS);

	return 0;
}

int qos_set_default(int fqos, int fport1, int fport2, int fport3, int fport4)
{
	set_fqos = (fqos & 0x1);

	/* When QoS is enabled (bit0 is 1), QoS on fort port (bitX, X is PSE port) is disabled */
	/* When QoS is disabled (bit0 is 0), bitX is ignored */

	if (set_fqos)
		if (fport1 >= 0 && fport1 < 16 && fport2 >= 0 && fport2 < 16 &&
		    fport3 >= 0 && fport3 < 16 && fport4 >= 0 && fport4 < 16)
			set_fqos |= (1 << fport1) | (1 << fport2) | (1 << fport3) | (1 << fport4);

	pr_notice("set_fqos = 0x%x\n", set_fqos);

	return 0;
}

int qos_set_eth_enable(int qos, int ignore2, int ignore3, int ignore4, int ignore5)
{

	eth_qos_enable = (u8)qos;
	pr_notice("eth_qos_enable = %d\n", qos);

	return 0;
}

int qos_set_wifi_enable(int qos, int ignore2, int ignore3, int ignore4, int ignore5)
{

	wifi_qos_enable = (u8)qos;
	pr_notice("wifi_qos_enable = %d\n", qos);

	return 0;
}

int qos_set_md_enable(int qos, int ignore2, int ignore3, int ignore4, int ignore5)
{

	md_qos_enable = (u8)qos;
	pr_notice("md_qos_enable = %d\n", qos);

	return 0;
}


void med_set_usage(void) {
	/* "hnat_med" */
	pr_notice("[Usage: read med info] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_MED);
}

int dma_set_usage(u64 ignore, u64 ignore2, u64 ignore3)
{
	/* "hnat_dma" */

	pr_notice("<Usage> set dma dump type : echo 1 [dma_type(mdma:0, wdma0:1, wdma1:2, edma0:3, edma1:4, edma2:5)] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_DMA);

	pr_notice("<Usage> dump dma : cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_DMA);

	pr_notice("<Usage> set payload range : echo 2 [payload_phy_start]  >  /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_DMA);

	return 0;
}

void dbg_set_usage(void) {
	/* "hnat_dbg" */
	pr_notice("[Usage: read dbg] echo r [reg] > /proc/%s/%s;cat /proc/%s/%s\n",
			HNAT_PROCREG_DIR, PROCREG_PPE_DBG, HNAT_PROCREG_DIR, PROCREG_PPE_DBG);
	pr_notice("[Usage: write dbg] echo w [reg] [value] > /proc/%s/%s;cat /proc/%s/%s\n",
			HNAT_PROCREG_DIR, PROCREG_PPE_DBG, HNAT_PROCREG_DIR, PROCREG_PPE_DBG);
}

int tnl_set_usage(int level, int ignore2, int ignore3)
{
	pr_notice("<Usage> set tunnel debug level : echo 1 [level] >  /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_TUNNEL);
	pr_notice("<Usage> set tunnel config : echo 2 [cfg] >  /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_TUNNEL);
	pr_notice("<Usage> set debug level : echo 0 [level] >  /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_PPE_SETTING);

	return 0;
}

int whitelist_set_if(int level, char *interface, int ignore3)
{
	struct net_device *dev;

	dev = dev_get_by_name(&init_net, interface);
	ppe_dev_reg_handler(dev);

	return 0;
}

int mib_set_if(int ignore, int ignore2, char *interface)
{
	struct net_device *dev;
	struct rtnl_link_stats64 stats = {0};
	int ret;

	dev = dev_get_by_name(&init_net, interface);

	ret = ppe_get_dev_stats_handler(dev, &stats);

	if (ret == 1) { /* succeed */
		pr_notice("interface %s\n", interface);
		pr_notice("tx: %llu pkt, %llu bytes\n", stats.tx_packets, stats.tx_bytes);
		pr_notice("rx: %llu pkt, %llu bytes\n", stats.rx_packets, stats.rx_bytes);
	} else {
		pr_notice("%s, interface %s, get mib error!\n", __func__, interface);
	}
	return 0;
}

int type_set_sw(int level, char *interface, int ignore3)
{
	struct net_device *dev;
	int i;

	dev = dev_get_by_name(&init_net, interface);
	for (i = 0; i < MAX_IF_NUM; i++) {
		if(dst_port[i] == dev) {
			dst_port_type[i] = SW_PATH;
			pr_notice("set %s software fast path\n", interface);
			break;
		}
	}
	return 0;
}

int type_set_hw(int level, char *interface, int ignore3)
{
	struct net_device *dev;
	int i;

	dev = dev_get_by_name(&init_net, interface);
	for (i = 0; i < MAX_IF_NUM; i++) {
		if(dst_port[i] == dev) {
			dst_port_type[i] = HW_PATH;
			pr_notice("set %s hardware acceleration\n", interface);
			break;
		}
	}
	return 0;
}
int whitelist_del_if(int level, char *interface, int ignore3)
{
	struct net_device *dev;

	dev = dev_get_by_name(&init_net, interface);
	ppe_dev_unreg_handler(dev);

	return 0;
}

int mib_set_idx(int entry_num, int ignore2, char *ignore3)
{
	ppe_mib_dump(entry_num);

	return 0;
}

int mib_set_counter(int mib_counter, int ignore2, char *ignore3)
{
	ppe_mib_counter_en = mib_counter;
	pr_notice("\nppe_mib_counter_en = %d\n", ppe_mib_counter_en);

	ppe_start_mib_timer();

	return 0;
}

int mib_set_agcnt(int agcnt, int ignore2, char *ignore3)
{
	struct hwnat_ac_args args = {0};
	args.ag_index = agcnt;

	ppe_get_agcnt(&args);

	pr_notice("\n%s, acnt:%d, packet cnt:%lld, byte cnt: %lld\n",
		__func__, agcnt, args.ag_pkt_cnt, args.ag_byte_cnt);

	return 0;
}

int mib_set_ipv4_idx(int entry_num, int ppe_index, char *ignore3)
{
	struct foe_entry *entry = NULL;
	struct hnat_mib_struct *mib_entry_p = NULL;

	pr_notice("%s, entry_num:%d, ppe_index: %d\n", __func__, entry_num, ppe_index);

	if (entry_num >= FOE_4TB_SIZ)
		return HWNAT_FAIL;

	if (ppe_index == 0) {
		entry = &ppe_foe_base[entry_num];

		if (fe_feature & PPE_MIB)
			mib_entry_p = &hnat_entry_mib[entry_num];

	} else if (ppe_index == 1) {
		entry = &ppe1_foe_base[entry_num];

		if (fe_feature & PPE_MIB)
			mib_entry_p = &hnat_entry1_mib[entry_num];
	} else
		return HWNAT_FAIL;

	// update mib before query
	ppe_mib_update();

	pr_notice("\n**********PPE%d ENTRY(%d)******************\n", ppe_index, entry_num);

	if (fe_feature & PPE_MIB)
		pr_notice("ppe%d(%d), pkt_cnt: %llu, byte_cnt:%llu\n",
			ppe_index, entry_num, mib_entry_p->pkt_count, mib_entry_p->byte_count);
	else
		pr_notice("please enable CONFIG_PPE_MIB=y\n");

	pr_notice("\n********************************************\n");
	return 0;
}


int binding_threshold(u32 threshold, u32 ignore1, u32 ignore2)
{
	pr_notice("Binding Threshold =%d\n", threshold);
	reg_write(PPE_FOE_BNDR, threshold);
	return 0;
}

int bind_life_time(u32 tcp_life, u32 udp_life, u32 fin_life)
{
	pr_notice("tcp_life = %d, udp_life = %d, fin_life = %d\n",
		tcp_life, udp_life, fin_life);
	ppe_set_bind_lifetime(tcp_life, udp_life, fin_life);
	return 0;
}

int keep_alive_interval(u32 tcp_interval, u32 udp_interval, u32 ignore2)
{
	if (tcp_interval > 255 || udp_interval > 255) {
		tcp_interval = 255;
		udp_interval = 255;
		pr_notice("TCP/UDP keep alive max interval = 255\n");
	} else {
		pr_notice("tcp_interval = %d, udp_interval = %d\n",
			tcp_interval, udp_interval);
	}

	ppe_set_ka_interval(tcp_interval, udp_interval);
	return 0;
}

int enable_464xlat(u32 enable, u32 ignore1, u32 ignore2)
{
	pr_notice("%s, enable = %d\n", __func__, enable);

	ppe_set_464_enable(enable);
	xlat_enable = enable;

	return 0;
}

int hnat_reg_read(u32 step, u32 ignore1, u32 ignore2) {

	u32 i;
	void *reg = fe_base + step;

	pr_notice("%s, reg: 0x%x\n", __func__, MTK_FE_BASE + step);


	for (i = 0; i < 0x100; i += 0x10) {

		pr_notice("%08x : %08x %08x %08x %08x\n", MTK_FE_BASE + step + i,
			reg_read(reg + i), reg_read(reg + i + 4),
			reg_read(reg + i + 8), reg_read(reg + i + 0xc));
	}
	pr_notice("-------------------------------------------------\n");


	return 0;
}

int hnat_reg_write(u32 step, u32 value, u32 ignore2) {

	void *reg = fe_base + step;

	pr_notice("%s, reg: 0x%x, value: 0x%x\n", __func__, MTK_FE_BASE + step, value);

	reg_write(reg, value);
	hnat_reg_read(step, 0, 0);

	return 0;
}

bool hnat_reg_valid(u32 reg)
{

	if ((reg & 0xf) == 0x0 || (reg & 0xf) == 0x4 || (reg & 0xf) == 0x8 || (reg & 0xf) == 0xc)
		return 1;

	return 0;
}

bool hnat_dbg_write(u32 regv, u32 value)
{

	void *reg;

	pr_notice("%s, reg: 0x%x, value: 0x%x\n", __func__, regv, value);

	if (regv < MTK_FE_BASE || regv >= MTK_FE_BASE + MTK_FE_RANGE) {
		/* invalid */
		return 0;
	}

	reg = fe_base + (regv - MTK_FE_BASE);

	reg_write(reg, value);

	return 1;
}


static int hnat_dump_eswcnt_cycle(void *arg){

	void *reg = fe_base;
	u32 loop_count = 0;
	u32 len, cidx, didx, diff;

	u32 i, step_cidx[] = {
		//0x4800,	/* WDMA0 TX */
		//0x4c00,	/* WDMA1 TX */
		0x5400,	/* MDMA TX */
	};


	u32 step_didx[] = {
		0x4900,	/* WDMA0 RX */
		0x4d00,	/* WDMA1 RX */
		//0x5500,	/* MDMA RX */
		//0x5510,	/* MDMA RX Ring 1 */
		//0x5900,	/* FDMA RX */
	};

	while (eswcnt_loop_enable && loop_count <= eswcnt_loop_count) {


		for (i = 0; i < ARRAY_SIZE(step_cidx); i++) {
			len = reg_read(reg + step_cidx[i] + 4);
			cidx = reg_read(reg + step_cidx[i] + 8);
			didx = reg_read(reg + step_cidx[i] + 0xc);
			diff = (cidx >= didx) ? (cidx - didx) : (cidx + len - didx);
			pr_alert("%08x : %08x %08x %08x %08x %d\n", MTK_FE_BASE + step_cidx[i],
				reg_read(reg + step_cidx[i]), len, cidx, didx, diff % len);
		}

		for (i = 0; i < ARRAY_SIZE(step_didx); i++) {
			len = reg_read(reg + step_didx[i] + 4);
			cidx = reg_read(reg + step_didx[i] + 8);
			didx = reg_read(reg + step_didx[i] + 0xc);
			diff = (didx >= cidx) ? (didx - cidx) : (didx + len - cidx);
			pr_alert("%08x : %08x %08x %08x %08x %d\n", MTK_FE_BASE + step_didx[i],
				reg_read(reg + step_didx[i]), len, cidx, didx, diff % len);
		}

		pr_alert("\n");
		loop_count++;
	}
	eswcnt_loop_enable = false;

	return 0;
}


int hnat_dump_eswcnt(u32 enable, u32 loop_count, u32 ignore2)
{

	pr_notice("%s, enable = %d, loop_count = %d, eswcnt_loop_enable = %d\n",
		__func__, enable, loop_count, eswcnt_loop_enable);

	if (enable && !eswcnt_loop_enable) {

		eswcnt_loop_enable = true;
		eswcnt_loop_count = loop_count;

		kthread_run(hnat_dump_eswcnt_cycle, NULL, "thread-1");

	} else if (enable == 0){

		eswcnt_loop_enable = false;
	} else
		pr_notice("%s, wrong ! enable twice\n", __func__);

	return 0;
}


int set_edma_hw_vlan(u32 direction, u32 enable, u32 ignore2)
{
	pr_notice("%s, direction = %s, enable = %d\n", __func__, direction ? "tx":"rx", enable);

	if (direction) {
		/* edma_tx */
		if (enable)
			ppe_set_fe_dma_tx_hw_vlan(1);
		else
			ppe_set_fe_dma_tx_hw_vlan(0);
	}
	else {
		/* edma_rx */
		if (enable)
			ppe_set_fe_dma_rx_hw_vlan(1);
		else
			ppe_set_fe_dma_rx_hw_vlan(0);
	}

	return 0;
}


int set_rndis_mod(u32 val, u32 ignore1, u32 ignore2)
{
        pr_notice("%s, val = %d\n", __func__, val);

        if(val > 3 || val < 1) //rndis_mod should be 1~3
		rndis_mod = 3;
	else
		rndis_mod = val;

	pr_notice("%s, rndis_mod set to %d\n", __func__, rndis_mod);

        return 0;
}


int set_forbid_bind_ecn(u32 direction, u32 ignore1, u32 ignore2)
{
	pr_notice("%s, direction = %d\n", __func__, direction);

	/* [0:disable, 1:bi-dir, 2:downlink, 3:uplink] */
	forbid_bind_ecn = direction;

	return 0;
}



int hnat_cpu_reason(int cpu_reason, int ignore1, int ignore2)
{
	dbg_cpu_reason = cpu_reason;
	pr_notice("show cpu reason = %d entry index = %d\n",
		cpu_reason, hwnat_dbg_entry);
	/* foe_dump_entry(hwnat_dbg_entry); */

	return 0;
}

int entry_set_state(int state, int ignore1, int ignore2)
{
	dbg_entry_state = state;
	pr_notice("ENTRY STATE = %s\n",
		dbg_entry_state ==
		0 ? "Invalid" : dbg_entry_state ==
		1 ? "Unbind" : dbg_entry_state ==
		2 ? "BIND" : dbg_entry_state ==   3 ?
		"FIN" : "Unknown");
	return 0;
}

int entry_detail(int index, int ignore1, int ignore2)
{
	struct foe_entry *entry = NULL;

	entry = &ppe_foe_base[index];
	foe_dump_entry(index, entry, 0);
	return 0;
}

int entry_delete(int index, int ignore1, int ignore2)
{
	pr_notice("delete entry idx = %d\n", index);
	foe_del_entry_by_num(index, 0);
	return 0;
}

int dma_set_dump_type(u64 dump_type, u64 ignore2, u64 ignore3)
{
	hnat_dump_type = (int)dump_type;
	pr_notice("hnat_dump_type = %d\n", (int)dump_type);

	return 0;
}


int dma_set_payload_range(u64 phy_start, u64 ignore2, u64 ignore3)
{
	void __iomem *vir_start;
	char *d = NULL;
	int i;

	pr_notice("phy_start = 0x%llx\n", phy_start);

	vir_start = phys_to_virt(phy_start);
	if (vir_start == NULL) {
		pr_notice("phys_to_virt fail, phy_start = %llx, vir_start=%px\n", phy_start, vir_start);
		return 0;
	}

	d = (char *)vir_start;

	for (i = 0; i < 64; i += 8) {
		pr_info("payload: %02d/%08llx: %02x %02x %02x %02x %02x %02x %02x %02x\n", i, phy_start + i,
			d[i], d[i + 1], d[i + 2], d[i + 3], d[i + 4], d[i + 5], d[i + 6], d[i + 7]);
	}


	return 0;
}

#ifdef CONFIG_TUNNEL_FAST_PATH
static u8 tnl_dst_port_type[MAX_IF_NUM];
#endif /* CONFIG_TUNNEL_FAST_PATH */

int tnl_set_dbg_level(int level, int ignore1, int ignore2)
{
#ifdef CONFIG_TUNNEL_FAST_PATH
	pr_notice("set tunnel debug level = %d\n", level);
	hnat_tnl_set_dbg_level(level);
#endif /* CONFIG_TUNNEL_FAST_PATH */
	return 0;
}

int tnl_set_cfg(int val, int en, int ignore2)
{
#ifdef CONFIG_TUNNEL_FAST_PATH
	pr_notice("set tunnel configure = %d, en = %d\n", val, en);
	hnat_tnl_cfg(val, en);
#endif /* CONFIG_TUNNEL_FAST_PATH */
	return 0;
}

int tnl_set_skip_nat(int en, int ignore1, int ignore2)
{
#ifdef CONFIG_TUNNEL_FAST_PATH
	int i;

	pr_notice("set tunnel skip-nat, en = %d\n", en);

	switch (en) {
	case 0:
		for (i = 0; i < MAX_IF_NUM; i++) {
			if (dst_port[i]) {
				dst_port_type[i] = tnl_dst_port_type[i];
			}
		}
		hnat_tnl_cfg(31, en);
		break;
	case 1:
		for (i = 0; i < MAX_IF_NUM; i++) {
			if (dst_port[i]) {
				tnl_dst_port_type[i] = dst_port_type[i];
				dst_port_type[i] = SW_PATH;
			}
		}
		hnat_tnl_cfg(31, en);
		break;
	default:
		pr_notice("skip-nat %d fail!\n", en);
		break;
	}
#endif /* CONFIG_TUNNEL_FAST_PATH */

	return 0;
}

int tnl_set_bypass(int dp, int ul, int ignore2)
{
#ifdef CONFIG_TUNNEL_FAST_PATH
	pr_notice("set tunnel bypass, dp = %d\n", dp);

	if ((dp >= 0) && (dp < MAX_IF_NUM) &&
	    (ul >= 0) && (ul < MAX_IF_NUM)) {
		/* tunnel init skip-nat
		 * 0: disable
		 * 1: enable
		 */
		tnl_set_skip_nat((dp == 0 && ul == 0) ? 0 : 1, 0, 0);

		/* tunnel term bypass
		 * 0: disable
		 * 1: act_dp
		 */
		hnat_tnl_cfg(40, dp);
		hnat_tnl_cfg(41, ul);
	} else {
		pr_notice("bypass %d fail!\n", dp);
	}
#endif /* CONFIG_TUNNEL_FAST_PATH */
	return 0;
}

static const CPU_REASON_SET_FUNC hnat_set_func[] = {
	[0] = hnat_set_usage,
	[1] = hnat_cpu_reason,
};

static const ENTRY_SET_FUNC entry_set_func[] = {
	[0] = entry_set_usage,
	[1] = entry_set_state,
	[2] = entry_detail,
	[3] = entry_delete,
};

static const CR_SET_FUNC cr_set_func[] = {
	[0] = cr_set_usage,
	[1] = binding_threshold,
	[2] = bind_life_time,
	[3] = keep_alive_interval,
	[4] = enable_464xlat,
	[5] = hnat_reg_read,
	[6] = hnat_reg_write,
	[7] = hnat_dump_eswcnt,
	[8] = set_edma_hw_vlan,
	[9] = set_rndis_mod,
	[10] = set_forbid_bind_ecn
};

//TBD multcast forward setting
static const MULTICAST_SET_FUNC multicast_set_func[] = {
	[0] = multicast_set_usage,
};

static const WHITELIST_SET_FUNC whitelist_set_func[] = {
	[0] = whitelist_set_usage,
	[1] = whitelist_set_if,
	[2] = whitelist_del_if,
};

static const TYPE_SET_FUNC type_set_func[] = {
	[0] = type_set_usage,
	[1] = type_set_sw,
	[2] = type_set_hw,
};

static const QOS_SET_FUNC qos_set_func[] = {
	[0] = qos_set_default,
	[1] = qos_set_eth_enable,
	[2] = qos_set_md_enable,
	[3] = qos_set_wifi_enable
};

static const MIB_SET_FUNC mib_set_func[] = {
	[0] = mib_set_usage,
	[1] = mib_set_if,
	[2] = mib_set_idx,
	[3] = mib_set_counter,
	[4] = mib_set_agcnt,
	[5] = mib_set_ipv4_idx
};

ssize_t usage_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *data)
{
	return 0;
}

static const DMA_SET_FUNC dma_set_func[] = {
	[0] = dma_set_usage,
	[1] = dma_set_dump_type,
	[2] = dma_set_payload_range
};

static const TNL_SET_FUNC tnl_set_func[] = {
	[0] = tnl_set_usage,
	[1] = tnl_set_dbg_level,
	[2] = tnl_set_cfg,
	[3] = tnl_set_skip_nat,
	[4] = tnl_set_bypass,
};

ssize_t cpu_reason_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 1:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	}

	if (hnat_set_func[arg0] &&
	    (ARRAY_SIZE(hnat_set_func) > arg0)) {
		(*hnat_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*hnat_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t entry_write(struct file *file, const char __user *buffer,
		    size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 1:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 2:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg2 = 0;
		else
			ret = kstrtol(p_token, 10, &arg2);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg3 = 0;
		else
			ret = kstrtol(p_token, 10, &arg3);
		break;
	case 3:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg2 = 0;
		else
			ret = kstrtol(p_token, 10, &arg2);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg3 = 0;
		else
			ret = kstrtol(p_token, 10, &arg3);
		break;
	}

	if (entry_set_func[arg0] &&
	    (ARRAY_SIZE(entry_set_func) > arg0)) {
		(*entry_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*entry_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t setting_write(struct file *file, const char __user *buffer,
		      size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
	case 1:
	case 4:
	case 9:
	case 10:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 5:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 16, &arg1);
		break;

	case 2:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg2 = 0;
		else
			ret = kstrtol(p_token, 10, &arg2);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg3 = 0;
		else
			ret = kstrtol(p_token, 10, &arg3);
		break;
	case 3:
	case 7:
	case 8:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg2 = 0;
		else
			ret = kstrtol(p_token, 10, &arg2);
		break;
	case 6:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 16, &arg1);
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg2 = 0;
		else
			ret = kstrtol(p_token, 16, &arg2);
		break;
	}

	if (cr_set_func[arg0] &&
	    (ARRAY_SIZE(cr_set_func) > arg0)) {
		(*cr_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*cr_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t multicast_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	}

	if (multicast_set_func[arg0] &&
	    (ARRAY_SIZE(multicast_set_func) > arg0)) {
		(*multicast_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*multicast_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t whitelist_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg3 = 0;
	char *p_token = NULL;
	char *arg2 = NULL;
	char *p_delimiter = " \t";
	char *p_delimiter1 = " \n";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 1:
		p_token = strsep(&p_buf, p_delimiter1);
		arg2 = p_token;
		break;
	case 2:
		p_token = strsep(&p_buf, p_delimiter1);
		arg2 = p_token;
		break;
	}

	if (whitelist_set_func[arg0] &&
	    (ARRAY_SIZE(whitelist_set_func) > arg0)) {
		(*whitelist_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*whitelist_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t type_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg3 = 0;
	char *p_token = NULL;
	char *arg2 = NULL;
	char *p_delimiter = " \t";
	char *p_delimiter1 = " \n";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 1:
		p_token = strsep(&p_buf, p_delimiter1);
		arg2 = p_token;
		break;
	case 2:
		p_token = strsep(&p_buf, p_delimiter1);
		arg2 = p_token;
		break;
	}

	if (type_set_func[arg0] &&
	    (ARRAY_SIZE(type_set_func) > arg0)) {
		(*type_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*type_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t qos_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)

{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0, arg4 = 0, arg5 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 1:
	case 2:
	case 3:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtol(p_token, 10, &arg1);
		break;
	case 0:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			break;
		ret = kstrtol(p_token, 10, &arg1);

		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			break;
		ret = kstrtol(p_token, 10, &arg2);

		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			break;
		ret = kstrtol(p_token, 10, &arg3);

		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			break;
		ret = kstrtol(p_token, 10, &arg4);

		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			break;
		ret = kstrtol(p_token, 10, &arg5);
		break;
	}

	if (qos_set_func[arg0] &&
	    (ARRAY_SIZE(qos_set_func) > arg0)) {
		(*qos_set_func[arg0]) (arg1, arg2, arg3, arg4, arg5);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*qos_set_func[0]) (0, 0, 0, 0, 0);
	}

	return len;
}

ssize_t mib_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	long arg0 = 0, arg_long = 0, arg_long2 = 0;
	char *p_token = NULL;
	char *arg_char = NULL;
	char *p_delimiter = " \t";
	char *p_delimiter1 = " \n";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
	case 2:
	case 3:
	case 4:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg_long = 0;
		else
			ret = kstrtol(p_token, 10, &arg_long);
		break;
	case 1:
		p_token = strsep(&p_buf, p_delimiter1);
		arg_char = p_token;
		break;
	case 5:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg_long = 0;
		else
			ret = kstrtol(p_token, 10, &arg_long);

		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg_long2 = 0;
		else
			ret = kstrtol(p_token, 10, &arg_long2);
		break;
	}

	if (mib_set_func[arg0] &&
	    (ARRAY_SIZE(mib_set_func) > arg0)) {
		(*mib_set_func[arg0]) (arg_long, arg_long2, arg_char);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*mib_set_func[0]) (0, 0, 0);
	}

	return len;
}


ssize_t med_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	return 0;
}

ssize_t dma_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	u64 arg0 = 0, arg1 = 0, arg2 = 0, arg3 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else
		ret = kstrtoull(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
		/* no parameter */
		break;

	case 1:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtoull(p_token, 10, &arg1);
		break;

	case 2:
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			arg1 = 0;
		else
			ret = kstrtoull(p_token, 16, &arg1);
		break;

	}

	if (dma_set_func[arg0] &&
	    (ARRAY_SIZE(dma_set_func) > arg0)) {
		(*dma_set_func[arg0]) (arg1, arg2, arg3);
	} else {
		pr_notice("no handler defined for command id(0x%08llx)\n\r", arg0);
		(*dma_set_func[0]) (0, 0, 0);
	}

	return len;
}

ssize_t dbg_write(struct file *file, const char __user *buffer,
			 size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	int len = count;
	u32 arg1 = 0, arg2 = 0;
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf) || len <= 0) {
		pr_notice("len: %d, input handling fail!\n", len);
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;

	/* action */
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		return -1;

	switch (*p_token) {
	case 'r':

		/* register */
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			goto error;
		else
			ret = kstrtou32(p_token, 16, &arg1);

		if (!hnat_reg_valid(arg1))
			goto error;

		hnat_dbg_reg = arg1;
		break;
	case 'w':

		/* register */
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			goto error;
		else
			ret = kstrtou32(p_token, 16, &arg1);

		if (!hnat_reg_valid(arg1))
			goto error;

		/* value */
		p_token = strsep(&p_buf, p_delimiter);
		if (!p_token)
			goto error;
		else
			ret = kstrtou32(p_token, 16, &arg2);

		if (!hnat_dbg_write(arg1, arg2))
			goto error;

		hnat_dbg_reg = arg1;
		break;

	}

	return len;

error:
	hnat_dbg_reg = 0;
	return -1;

}

ssize_t tunnel_write(struct file *file, const char __user *buffer,
			size_t count, loff_t *data)
{
	char buf[32];
	char *p_buf;
	uint32_t len = count, i;
	long arg0 = 0, val[3] = {0};
	char *p_token = NULL;
	char *p_delimiter = " \t";
	int ret;

	if (len >= sizeof(buf)) {
		pr_notice("input handling fail!\n");
		len = sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pr_notice("write parameter data = %s\n\r", buf);

	p_buf = buf;
	p_token = strsep(&p_buf, p_delimiter);
	if (!p_token)
		arg0 = 0;
	else if (strncmp(p_token, "debug", strlen(p_token)) == 0)
		arg0 = 1;
	else if (strncmp(p_token, "cfg", strlen(p_token)) == 0)
		arg0 = 2;
	else if (strncmp(p_token, "skip-nat", strlen(p_token)) == 0)
		arg0 = 3;
	else if (strncmp(p_token, "bypass", strlen(p_token)) == 0)
		arg0 = 4;
	else
		ret = kstrtol(p_token, 10, &arg0);

	switch (arg0) {
	case 0:
	case 1:
	case 3:
		for (i = 0; i < 1; i++){
			p_token = strsep(&p_buf, p_delimiter);
			if (!p_token)
				val[i] = 0;
			else
				ret = kstrtol(p_token, 10, &val[i]);
		}
		break;
	case 2:
	case 4:
		for (i = 0; i < 2; i++){
			p_token = strsep(&p_buf, p_delimiter);
			if (!p_token)
				val[i] = 0;
			else
				ret = kstrtol(p_token, 10, &val[i]);
		}
		break;
	}

	if (tnl_set_func[arg0] &&
	    (ARRAY_SIZE(tnl_set_func) > arg0)) {
		(*tnl_set_func[arg0]) (val[0], val[1], val[2]);
	} else {
		pr_notice("no handler defined for command id(0x%08lx)\n\r", arg0);
		(*tnl_set_func[0]) (0, 0, 0);
	}

	return len;
}

int usage_read(struct seq_file *seq, void *v)
{
	/* START */
	pr_notice("\n");
	pr_notice("********************************************************\n");
	pr_notice("****************[HNAT_DEBUG_PROC_USAGE_START]***********\n");
	pr_notice("********************************************************\n");


	/* "hnat_setting" */
	pr_notice("****************[1.hnat_setting]************************\n");
	cr_set_usage(debug_level, 0, 0);
	pr_notice("\n");

	/* "hnat_entry" */
	pr_notice("****************[2.hnat_entry]**************************\n");
	entry_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "cpu_reason" */
	pr_notice("****************[3.cpu_reason]**************************\n");
	hnat_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "hnat_multicast" */
	pr_notice("****************[4.hnat_multicast]**********************\n");
	multicast_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "hnat_whitelist" */
	pr_notice("****************[5.hnat_whitelist]**********************\n");
	whitelist_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "hnat_type" */
	pr_notice("****************[6.hnat_type]***************************\n");
	type_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "hnat_qos" */
	pr_notice("****************[7.hnat_qos]****************************\n");
	qos_get_usage();
	pr_notice("\n");

	/* "hnat_mib" */
	pr_notice("****************[8.hnat_mib]****************************\n");
	mib_set_usage(debug_level, 0, 0);
	pr_notice("\n");

	/* "hnat_med" */
	pr_notice("****************[9.hnat_med]****************************\n");
	med_set_usage();
	pr_notice("\n");

	/* "hnat_mdma" */
	pr_notice("****************[10.hnat_dma]**************************\n");
	dma_set_usage(0, 0, 0);
	pr_notice("\n");

	/* "hnat_dbg" */
	pr_notice("****************[11.hnat_dbg]********************\n");
	dbg_set_usage();
	pr_notice("\n");

	/* END */
	pr_notice("********************************************************\n");
	pr_notice("****************[HNAT_DEBUG_PROC_USAGE_END]*************\n");
	pr_notice("********************************************************\n");
	pr_notice("\n");

	return 0;
}


int cpu_reason_read(struct seq_file *seq, void *v)
{
	int i;

	pr_notice("============ CPU REASON =========\n");
	pr_notice("(2)IPv4(IPv6) TTL(hop limit) = %u\n",
		dbg_cpu_reason_cnt[0]);
	pr_notice("(3)Ipv4(IPv6) has option(extension) header = %u\n",
		dbg_cpu_reason_cnt[1]);
	pr_notice("(7)No flow is assigned = %u\n", dbg_cpu_reason_cnt[2]);
	pr_notice("(8)IPv4 HNAT doesn't support IPv4 /w fragment = %u\n",
		dbg_cpu_reason_cnt[3]);
	pr_notice("(9)IPv4 HNAPT/DS-Lite doesn't support IPv4 /w fragment = %u\n",
		dbg_cpu_reason_cnt[4]);
	pr_notice("(10)IPv4 HNAPT/DS-Lite can't find TCP/UDP sport/dport = %u\n",
		dbg_cpu_reason_cnt[5]);
	pr_notice("(11)IPv6 5T-route/6RD can't find TCP/UDP sport/dport = %u\n",
		dbg_cpu_reason_cnt[6]);
	pr_notice("(12)Ingress packet is TCP fin/syn/rst = %u\n",
		dbg_cpu_reason_cnt[7]);
	pr_notice("(13)FOE Un-hit = %u\n", dbg_cpu_reason_cnt[8]);
	pr_notice("(14)FOE Hit unbind = %u\n", dbg_cpu_reason_cnt[9]);
	pr_notice("(15)FOE Hit unbind & rate reach = %u\n", dbg_cpu_reason_cnt[10]);
	pr_notice("(16)Hit bind PPE TCP FIN entry = %u\n", dbg_cpu_reason_cnt[11]);
	pr_notice("(17)Hit bind PPE entry and TTL(hop limit) = 1 and TTL(hot limit) - 1 = %u\n",
		dbg_cpu_reason_cnt[12]);
	pr_notice("(18)Hit bind and VLAN replacement violation = %u\n",
		dbg_cpu_reason_cnt[13]);
	pr_notice("(19)Hit bind and keep alive with unicast old-header packet = %u\n",
		dbg_cpu_reason_cnt[14]);
	pr_notice("(20)Hit bind and keep alive with multicast new-header packet = %u\n",
		dbg_cpu_reason_cnt[15]);
	pr_notice("(21)Hit bind and keep alive with duplicate old-header packet = %u\n",
		dbg_cpu_reason_cnt[16]);
	pr_notice("(22)FOE Hit bind & force to CPU = %u\n", dbg_cpu_reason_cnt[17]);
	pr_notice("(28)Hit bind and exceed MTU =%u\n", dbg_cpu_reason_cnt[18]);
	pr_notice("(24)Hit bind multicast packet to CPU = %u\n",
		dbg_cpu_reason_cnt[19]);
	pr_notice("(25)Hit bind multicast packet to GMAC & CPU = %u\n",
		dbg_cpu_reason_cnt[20]);
	pr_notice("(26)Pre bind = %u\n", dbg_cpu_reason_cnt[21]);

	for (i = 0; i < 22; i++)
		dbg_cpu_reason_cnt[i] = 0;
	return 0;
}

int entry_read(struct seq_file *seq, void *v)
{
	struct foe_entry *entry;
	struct foe_entry *entry1;
	int hash_index;
	int cnt;

	cnt = 0;
	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];
		if (entry->bfib1.state == dbg_entry_state) {
			cnt++;
			dbg_dump_entry(hash_index, entry);
		}
	}
	pr_notice("PPE0 Total State = %s cnt = %d\n",
		dbg_entry_state ==
		0 ? "Invalid" : dbg_entry_state ==
		1 ? "Unbind" : dbg_entry_state ==
		2 ? "BIND" : dbg_entry_state ==   3 ? "FIN" : "Unknown", cnt);

	cnt = 0;
	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry1 = &ppe1_foe_base[hash_index];
		if (entry1->bfib1.state == dbg_entry_state) {
			cnt++;
			dbg_dump_entry(hash_index, entry1);
		}
	}
	pr_notice("PPE1 Total State = %s cnt = %d\n",
		dbg_entry_state ==
		0 ? "Invalid" : dbg_entry_state ==
		1 ? "Unbind" : dbg_entry_state ==
		2 ? "BIND" : dbg_entry_state ==   3 ? "FIN" : "Unknown", cnt);
	return 0;
}

int setting_read(struct seq_file *seq, void *v)
{
	dbg_dump_cr(seq);
	return 0;
}

int multicast_read(struct seq_file *seq, void *v)
{
	foe_mcast_entry_dump();
	return 0;
}

int whitelist_read(struct seq_file *seq, void *v)
{
	dump_dport();
	return 0;
}

int type_read(struct seq_file *seq, void *v)
{
	int i;

	for (i = 0; i < MAX_IF_NUM; i++) {
		if(dst_port[i] != NULL)
			pr_notice("%s -> %s\n",dst_port[i]->name,
				dst_port_type[i] == HW_PATH ? "HW_fast" :
				dst_port_type[i] == SW_PATH ? "SW_fast" : "Unknown");
	}
	return 0;
}

int qos_read(struct seq_file *seq, void *v)
{
	pr_notice("support hw qos = 0x%x\n", set_fqos);

	return 0;
}

int mib_read(struct seq_file *seq, void *v)
{
	u8 fport, sport, fport1, sport1, i;
	int hash_index;
	struct foe_entry *entry;
	struct foe_entry *entry1;
	u64 pkt_cnt, byte_cnt, pkt_cnt1, byte_cnt1;

	for (i = 0; i < MAX_IF_NUM; i++)
		hnat_if[i].dev = dst_port[i];

	pkt_cnt = byte_cnt = 0;
	pkt_cnt1 = byte_cnt1 = 0;

	for (hash_index = 0; hash_index < FOE_4TB_SIZ; hash_index++) {
		entry = &ppe_foe_base[hash_index];
		entry1 = &ppe1_foe_base[hash_index];

		fport = get_act_dp(entry);
		sport = get_rxif_idx(entry);

		fport1 = get_act_dp(entry1);
		sport1 = get_rxif_idx(entry1);

		ppe_mib_dump_ppe0(hash_index, &pkt_cnt, &byte_cnt);
		ppe_mib_dump_ppe1(hash_index, &pkt_cnt1, &byte_cnt1);

		hnat_if[sport].rx_byte_cnt += byte_cnt;
		hnat_if[fport].tx_byte_cnt += byte_cnt;

		hnat_if[sport].rx_pkt_cnt += pkt_cnt;
		hnat_if[fport].tx_pkt_cnt += pkt_cnt;


		hnat_if[sport1].rx_byte_cnt += byte_cnt1;
		hnat_if[fport1].tx_byte_cnt += byte_cnt1;

		hnat_if[sport1].rx_pkt_cnt += pkt_cnt1;
		hnat_if[fport1].tx_pkt_cnt += pkt_cnt1;
	}

	for (i = 0; i < MAX_IF_NUM; i++) {
		if((hnat_if[i].dev) != NULL) {
			pr_notice("Interface : %s\n", hnat_if[i].dev->name);
			pr_notice("Rx pkt cnt =%llu, Rx byte cnt=%llu\n", hnat_if[i].rx_pkt_cnt, hnat_if[i].rx_byte_cnt);
			pr_notice("Tx pkt cnt =%llu, Tx byte cnt=%llu\n", hnat_if[i].tx_pkt_cnt, hnat_if[i].tx_byte_cnt);

        #ifdef CONFIG_TUNNEL_FAST_PATH
			pr_notice("Rx swnat cnt = %llu, byte_cnt = %llu\n", hnat_tnl_get_swnat_cnt(0, i), hnat_tnl_get_swnat_byte_cnt(0, i));
			pr_notice("Tx swnat cnt = %llu, byte_cnt = %llu\n", hnat_tnl_get_swnat_cnt(1, i), hnat_tnl_get_swnat_byte_cnt(1, i));
        #endif /* CONFIG_TUNNEL_FAST_PATH */
		}
	}

	return 0;
}

int med_read(struct seq_file *seq, void *v)
{
#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT

	struct MED_HNAT_INFO_HOST *med_dmad;
	unsigned int wdix, rdix, i, j;
	u32 *p;

	rdix = reg_read(MEDHW_SSR1_DST_RB0_RIDX);
	wdix = reg_read(MEDHW_SSR1_DST_RB0_WIDX);
	med_dmad = &med_info_base[0];
	p = (u32 *)med_dmad;
	pr_notice("wdix = %x, rdix = %x\n", wdix, rdix);
	for (i = 0; i < MED_INFO_SIZE; i++) {
		pr_notice("********** HNAT_INFO_HOST(%d)*********\n", i);
		for (j = 0; j < 2; j++)
			pr_notice("%02d: %08X\n", j, *(p + j));
	}
#endif /* CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT */
	return 0;
}


int dma_dump_payload(struct seq_file *seq, u64 phy_start)
{
	void *vir_start;
	u8 d[64];
	u64 phy_end;
	int i;

	phy_end = phy_start + sizeof(d) - 1;
	if (!phy_start || phy_end < phy_start ||
	    !pfn_valid(PHYS_PFN(phy_start)) ||
	    !pfn_valid(PHYS_PFN(phy_end))) {
		seq_printf(seq, "payload: skipped invalid/non-RAM DMA address %016llx\n",
			   phy_start);
		return 0;
	}

	vir_start = phys_to_virt(phy_start);
	if (!virt_addr_valid(vir_start) ||
	    copy_from_kernel_nofault(d, vir_start, sizeof(d))) {
		seq_printf(seq, "payload: inaccessible DMA address %016llx\n",
			   phy_start);
		return 0;
	}

	for (i = 0; i < 64; i += 8) {
		seq_printf(seq, "payload: %02d/%08llx: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			i, phy_start + i,
			d[i], d[i + 1], d[i + 2], d[i + 3], d[i + 4], d[i + 5], d[i + 6], d[i + 7]);
	}


	return 0;
}


int dma_read(struct seq_file *seq, void *v)
{
	int i;
	u32 tx_size = 0;
	u32 rx_size = 0;
	u32 tx_base_phy, rx_base_phy;
	void __iomem *dma_base;
	void __iomem *dma_base_rx;
	u32 *p;

	struct dump_dma_struct *regp;
	struct dump_dma_struct reg[] = {
		{"MDMA", true, MDMA_TX_BASE_PTR_0, MDMA_TX_MAX_CNT_0, MDMA_RX_BASE_PTR_0, MDMA_RX_MAX_CNT_0},
		{"WDMA0", false, WDMA0_TX_BASE_PTR_0, WDMA0_TX_MAX_CNT_0, WDMA0_RX_BASE_PTR_0, WDMA0_RX_MAX_CNT_0},
		{"WDMA1", false, WDMA1_TX_BASE_PTR_0, WDMA1_TX_MAX_CNT_0, WDMA1_RX_BASE_PTR_0, WDMA1_RX_MAX_CNT_0},
		{"EDMA0", false, EDMA0_TX_BASE_PTR_0, EDMA0_TX_MAX_CNT_0, EDMA0_RX_BASE_PTR_0, EDMA0_RX_MAX_CNT_0},
		{"EDMA1", false, EDMA1_TX_BASE_PTR_0, EDMA1_TX_MAX_CNT_0, EDMA1_RX_BASE_PTR_0, EDMA1_RX_MAX_CNT_0},
		{"EDMA2", false, EDMA2_TX_BASE_PTR_0, EDMA2_TX_MAX_CNT_0, EDMA2_RX_BASE_PTR_0, EDMA2_RX_MAX_CNT_0},
	};

	/* reset as default: mdma (0) */
	if (hnat_dump_type >= ARRAY_SIZE(reg))
		hnat_dump_type = 0;

	regp = &reg[hnat_dump_type];

	/* dump */
	tx_base_phy = reg_read(regp->dma_tx_base);
	tx_size = reg_read(regp->dma_tx_size);

	rx_base_phy = reg_read(regp->dma_rx_base);
	rx_size = reg_read(regp->dma_rx_size);

	pr_notice("%s, [%s] tx_size:%d, rx_size: %d\n", __func__, regp->name, tx_size, rx_size);

	if (regp->iomem) {

		dma_base = ioremap(tx_base_phy, tx_size * 32);

		if (dma_base == NULL) {
			pr_notice("ioremap fail, dma_base=%px\n", dma_base);
			return 0;
		}


		dma_base_rx = ioremap(rx_base_phy, rx_size * 32);

		if (dma_base_rx == NULL) {
			pr_notice("ioremap fail, dma_base_rx=%px\n", dma_base_rx);

			iounmap(dma_base);
			return 0;
		}
	} else {

		dma_base = phys_to_virt(tx_base_phy);

		if (dma_base == NULL) {
			pr_notice("phys_to_virt fail, dma_base=%px\n", dma_base);
			return 0;
		}

		dma_base_rx = phys_to_virt(rx_base_phy);

		if (dma_base_rx == NULL) {
			pr_notice("phys_to_virt fail, dma_base_rx=%px\n", dma_base_rx);

			return 0;
		}
	}

	p = (u32 *)dma_base;
	seq_printf(seq, "!!!!!!!!![%s] TX_RING!!!!!!!\n", regp->name);

	for (i = 0; i < tx_size; i++) {

		seq_printf(seq, "TX(%d)[%px/%08llX] %08X %08X %08X %08X %08X %08X %08X %08X\n",
			i, p, tx_base_phy + ((u64)p - (u64)dma_base),
			*p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));

		dma_dump_payload(seq, *p);

		p += 8;
	}

	p = (u32 *)dma_base_rx;
	seq_printf(seq, "!!!!!!!!! [%s] RX_RING!!!!!!!\n", regp->name);

	for (i = 0; i < rx_size; i++) {
		seq_printf(seq, "RX(%d)[%px/%08llX] %08X %08X %08X %08X %08X %08X %08X %08X\n",
			i, p, rx_base_phy + ((u64)p - (u64)dma_base_rx),
			*p, *(p + 1), *(p + 2), *(p + 3), *(p + 4), *(p + 5), *(p + 6), *(p + 7));

		dma_dump_payload(seq, *p);

		p += 8;
	}

	if (regp->iomem) {
		iounmap(dma_base);
		iounmap(dma_base_rx);
	}

	return 0;
}

int dbg_read(struct seq_file *seq, void *v)
{
	u32 i;
	void *reg;

	if (hnat_dbg_reg < MTK_FE_BASE || hnat_dbg_reg >= MTK_FE_BASE + MTK_FE_RANGE) {
		seq_printf(seq, "out of FE range: 0x%08x-0x%08x\n", MTK_FE_BASE, MTK_FE_BASE + MTK_FE_RANGE - 1);
		return 0;
	}

	reg = fe_base + (hnat_dbg_reg - MTK_FE_BASE);

	seq_printf(seq, "reg: 0x%x\n", hnat_dbg_reg);


	for (i = 0; i < 0x100; i += 0x10) {

		if (hnat_dbg_reg + i + 0xc < MTK_FE_BASE + MTK_FE_RANGE)
			seq_printf(seq, "%08x : %08x %08x %08x %08x\n", hnat_dbg_reg + i,
				reg_read(reg + i), reg_read(reg + i + 4),
				reg_read(reg + i + 8), reg_read(reg + i + 0xc));
	}

	return 0;
}

int tunnel_read(struct seq_file *seq, void *v)
{
#ifdef CONFIG_TUNNEL_FAST_PATH
	hnat_tnl_dump_db();
#endif /* CONFIG_TUNNEL_FAST_PATH */
	return 0;
}

static int usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, usage_read, NULL);
}


static int cpu_reason_open(struct inode *inode, struct file *file)
{
	return single_open(file, cpu_reason_read, NULL);
}

static int entry_open(struct inode *inode, struct file *file)
{
	return single_open(file, entry_read, NULL);
}

static int setting_open(struct inode *inode, struct file *file)
{
	return single_open(file, setting_read, NULL);
}

static int multicast_open(struct inode *inode, struct file *file)
{
	return single_open(file, multicast_read, NULL);
}

static int whitelist_open(struct inode *inode, struct file *file)
{
	return single_open(file, whitelist_read, NULL);
}

static int type_open(struct inode *inode, struct file *file)
{
	return single_open(file, type_read, NULL);
}

static int qos_open(struct inode *inode, struct file *file)
{
	return single_open(file, qos_read, NULL);
}

static int mib_open(struct inode *inode, struct file *file)
{
	return single_open(file, mib_read, NULL);
}

static int med_open(struct inode *inode, struct file *file)
{
	return single_open(file, med_read, NULL);
}

static int dma_open(struct inode *inode, struct file *file)
{
	return single_open(file, dma_read, NULL);
}

static int dbg_open(struct inode *inode, struct file *file)
{
	return single_open(file, dbg_read, NULL);
}

static int tunnel_open(struct inode *inode, struct file *file)
{
	return single_open(file, tunnel_read, NULL);
}

static const struct PROC_STRUCT hnat_usage_fops = {
	PROC_OWNER
	.PROC_OPEN = usage_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = usage_write,
	.PROC_RELEASE = single_release
};


static const struct PROC_STRUCT cpu_reason_fops = {
	PROC_OWNER
	.PROC_OPEN = cpu_reason_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = cpu_reason_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_entry_fops = {
	PROC_OWNER
	.PROC_OPEN = entry_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = entry_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_setting_fops = {
	PROC_OWNER
	.PROC_OPEN = setting_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = setting_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_multicast_fops = {
	PROC_OWNER
	.PROC_OPEN = multicast_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = multicast_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_whitelist_fops = {
	PROC_OWNER
	.PROC_OPEN = whitelist_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = whitelist_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_type_fops = {
	PROC_OWNER
	.PROC_OPEN = type_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = type_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_qos_fops = {
	PROC_OWNER
	.PROC_OPEN = qos_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = qos_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_mib_fops = {
	PROC_OWNER
	.PROC_OPEN = mib_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = mib_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_med_fops = {
	PROC_OWNER
	.PROC_OPEN = med_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = med_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_dma_fops = {
	PROC_OWNER
	.PROC_OPEN = dma_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = dma_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_dbg_fops = {
	PROC_OWNER
	.PROC_OPEN = dbg_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = dbg_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hnat_tunnel_fops = {
	PROC_OWNER
	.PROC_OPEN = tunnel_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = tunnel_write,
	.PROC_RELEASE = single_release
};

int hnat_debug_proc_init(void)
{
	if (!hnat_proc_reg_dir)
		hnat_proc_reg_dir = proc_mkdir(HNAT_PROCREG_DIR, NULL);

	proc_hnat_usage = proc_create(PROCREG_PPE_USAGE, 0400,
				      hnat_proc_reg_dir, &hnat_usage_fops);
	if (!proc_hnat_usage)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_USAGE);

	proc_cpu_reason = proc_create(PROCREG_CPU_REASON, 0400,
				      hnat_proc_reg_dir, &cpu_reason_fops);
	if (!proc_cpu_reason)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_CPU_REASON);

	proc_hnat_entry = proc_create(PROCREG_PPE_ENTRY, 0400,
				      hnat_proc_reg_dir, &hnat_entry_fops);
	if (!proc_hnat_entry)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_ENTRY);

	proc_hnat_setting = proc_create(PROCREG_PPE_SETTING, 0400,
					hnat_proc_reg_dir, &hnat_setting_fops);
	if (!proc_hnat_setting)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_ENTRY);

	proc_hnat_multicast = proc_create(PROCREG_PPE_MULTICAST, 0400,
					  hnat_proc_reg_dir, &hnat_multicast_fops);
	if (!proc_hnat_multicast)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_MULTICAST);

	proc_hnat_whitelist = proc_create(PROCREG_PPE_WHITELIST, 0400,
					  hnat_proc_reg_dir, &hnat_whitelist_fops);
	if (!proc_hnat_whitelist)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_WHITELIST);

	proc_hnat_type = proc_create(PROCREG_PPE_TYPE, 0400,
					  hnat_proc_reg_dir, &hnat_type_fops);
	if (!proc_hnat_type)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_TYPE);

	proc_hnat_qos = proc_create(PROCREG_PPE_QOS, 0400,
					  hnat_proc_reg_dir, &hnat_qos_fops);
	if (!proc_hnat_qos)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_QOS);

	proc_hnat_mib = proc_create(PROCREG_PPE_MIB, 0400,
					  hnat_proc_reg_dir, &hnat_mib_fops);
	if (!proc_hnat_mib)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_MIB);

	proc_hnat_med = proc_create(PROCREG_PPE_MED, 0400,
					  hnat_proc_reg_dir, &hnat_med_fops);
	if (!proc_hnat_med)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_MED);


	proc_hnat_dma = proc_create(PROCREG_PPE_DMA, 0400,
					  hnat_proc_reg_dir, &hnat_dma_fops);
	if (!proc_hnat_dma)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_DMA);


	proc_hnat_dbg = proc_create(PROCREG_PPE_DBG, 0400,
					  hnat_proc_reg_dir, &hnat_dbg_fops);
	if (!proc_hnat_dbg)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_DBG);

	proc_hnat_tunnel = proc_create(PROCREG_PPE_TUNNEL, 0400,
					  hnat_proc_reg_dir, &hnat_tunnel_fops);
	if (!proc_hnat_tunnel)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_PPE_TUNNEL);

	return 0;
}

void hnat_debug_proc_exit(void)
{
	pr_notice("proc exit\n");

	if (proc_hnat_usage)
		remove_proc_entry(PROCREG_PPE_USAGE, hnat_proc_reg_dir);

	if (proc_cpu_reason)
		remove_proc_entry(PROCREG_CPU_REASON, hnat_proc_reg_dir);

	if (proc_hnat_entry)
		remove_proc_entry(PROCREG_PPE_ENTRY, hnat_proc_reg_dir);

	if (proc_hnat_setting)
		remove_proc_entry(PROCREG_PPE_SETTING, hnat_proc_reg_dir);

	if (proc_hnat_multicast)
		remove_proc_entry(PROCREG_PPE_MULTICAST, hnat_proc_reg_dir);

	if (proc_hnat_whitelist)
		remove_proc_entry(PROCREG_PPE_WHITELIST, hnat_proc_reg_dir);

	if (proc_hnat_type)
		remove_proc_entry(PROCREG_PPE_TYPE, hnat_proc_reg_dir);

	if (proc_hnat_qos)
		remove_proc_entry(PROCREG_PPE_QOS, hnat_proc_reg_dir);

	if (proc_hnat_mib)
		remove_proc_entry(PROCREG_PPE_MIB, hnat_proc_reg_dir);

	if (proc_hnat_med)
		remove_proc_entry(PROCREG_PPE_MED, hnat_proc_reg_dir);

	if (proc_hnat_dma)
		remove_proc_entry(PROCREG_PPE_DMA, hnat_proc_reg_dir);

	if (proc_hnat_dbg)
		remove_proc_entry(PROCREG_PPE_DBG, hnat_proc_reg_dir);

	if (proc_hnat_tunnel)
		remove_proc_entry(PROCREG_PPE_TUNNEL, hnat_proc_reg_dir);

	if (hnat_proc_reg_dir)
		remove_proc_entry(HNAT_PROCREG_DIR, 0);
}
