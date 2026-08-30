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
#include <net/dsa.h>
#include <net/rtnetlink.h>
#include <net/netevent.h>
#include <net/gre.h>
#include <net/pptp.h>
#include <linux/platform_device.h>
#include "ra_nat.h"
#include "foe_fdb.h"
#include "frame_engine.h"
#include "util.h"
#include "hnat_ioctl.h"
#include "hnat_define.h"
#include "hnat_config.h"
#include "hnat_dbg_proc.h"
#include "mcast_tbl.h"
#include "hnat_common.h"
#include "hnat_fast.h"
#include "hnat_power.h"
#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
#include "medmcu_common.h"
#endif
#ifdef CONFIG_TUNNEL_FAST_PATH
#include "hnat_tnl.h"
#endif /* CONFIG_TUNNEL_FAST_PATH */

#define ETH_QOS_QCOUNT (64)
#define WIFI2_QOS_QID (59)
#define WIFI5_QOS_QID (31)
#define WIFI6_QOS_QID (47)

/* global variable */
static spinlock_t ppe_cache_lock;
unsigned int dbg_cpu_reason_cnt[32];
EXPORT_SYMBOL(dbg_cpu_reason_cnt);
int hwnat_dbg_entry;
EXPORT_SYMBOL(hwnat_dbg_entry);
unsigned int SwitchDslMape;
int get_brlan;
u32 br_netmask;
u32 br0_ip;
char br0_mac_address[6];
u32 ppe_sw_fast;
u32 ppe_hw_fast;
/* When QoS is enabled (bit0 is 1), QoS on fort port (bitX, X is PSE port) is disabled */
/* When QoS is disabled (bit0 is 0), bitX is ignored */
u16 set_fqos;
u8 eth_qos_enable = 1;
u8 wifi_qos_enable;
u8 md_qos_enable;
u8 xlat_enable = 1;
u8 pptp_enable = 1;
u8 forbid_bind_ecn;
u32 rndis_bind_count = 0;

#if defined(CONFIG_HNAT_V1)
u32 rndis_mod = 2;
#elif defined(CONFIG_HNAT_V2)
u32 rndis_mod = 3;
#endif

u8 second_fast_path = 0;

#ifndef L2TP_UDP_PORT_MIN
#define L2TP_UDP_PORT_MIN           1701
#endif

#ifndef L2TP_UDP_PORT_MAX
#define L2TP_UDP_PORT_MAX           1704
#endif

#define ESP_UDP_PORT           4500
#define VXLAN_UDP_PORT		4789

static const char *const mtk_hnat_feature_name[] = {
	"GE2_SUPPORT", "HNAT_IPV6", "HNAT_VLAN_TX", "HNAT_MCAST", "HNAT_QDMA", "WARP_WHNAT", "WIFI_HNAT", "HNAT_WAN_P4", "WAN_TO_WLAN_QOS", "HNAT_SP_TAG",
	"QDMA_TX_RX", "PPE_MIB", "PACKET_SAMPLING", "HNAT_OPENWRT", "HNAT_WLAN_QOS", "WLAN_OPTIMIZE", "UDP_FRAG", "AUTO_MODE", "SEMI_AUTO_MODE", "MANUAL_MODE",
	"PRE_BIND", "HNAT_IPI", "DBG_IPV6_SIP", "DBG_IPV4_SIP", "DBG_SP", "ETH_QOS", "SW_DVFS", "AUTO_HNAT"
};

static struct work_struct hnat_work;

u8 USE_3T_UDP_FRAG;
EXPORT_SYMBOL(USE_3T_UDP_FRAG);
struct foe_entry *ppe_foe_base;
EXPORT_SYMBOL(ppe_foe_base);
struct foe_entry *ppe1_foe_base;
EXPORT_SYMBOL(ppe1_foe_base);

struct MED_HNAT_INFO_HOST *med_info_base;

struct mib_entry *ppe_mib_base;
struct mib_entry *ppe1_mib_base;

dma_addr_t ppe_phy_mib_base;
dma_addr_t ppe1_phy_mib_base;

dma_addr_t ppe_phy_foe_base;
dma_addr_t ppe1_phy_foe_base;

struct ps_entry *ppe_ps_base;
dma_addr_t ppe_phy_ps_base;

u32 DP_GMAC1;
u32 DP_GMAC2;
u32 DP_EDMA0;
u32 DP_EDMA1;

#ifdef CONFIG_RAETH_EDMA
	struct net_device *aqr_dev1;
	struct net_device *aqr_dev2;
#endif

/* #define DSCP_REMARK_TEST */
/* #define PREBIND_TEST */
#define DD \
{\
pr_notice("%s %d\n", __func__, __LINE__); \
}

/*HWNAT IPI*/
/*unsigned int ipidbg[NR_CPUS][10];*/
/*unsigned int ipidbg2[NR_CPUS][10];*/
/*extern int32_t HnatIPIExtIfHandler(struct sk_buff * skb);*/
/*extern int32_t HnatIPIForceCPU(struct sk_buff * skb);*/
/*extern int HnatIPIInit();*/
/*extern int HnatIPIDeInit();*/
#if(0)
void skb_dump(struct sk_buff* sk) {
        unsigned int i;

        pr_notice("\nskb_dump: from %s with len %d (%d) headroom=%d tailroom=%d\n",
                sk->dev?sk->dev->name:"ip stack",sk->len,sk->truesize,
                skb_headroom(sk),skb_tailroom(sk));

        for(i=(unsigned int)sk->head;i<=(unsigned int)sk->data + 30;i++) {
                if((i % 16) == 0)
                    pr_notice("\n");

                if(i==(unsigned int)sk->head) pr_notice("@h");
                if(i==(unsigned int)sk->data) pr_notice("@d");
                pr_notice("%02X-",*((unsigned char*)i));
        }
        pr_notice("\n");
}
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

static inline void hwnat_set_6rd_id(struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	reg_modify_bits(PPE_6RD_ID, ntohs(ppe_parse_result->iph.id), 0, 16);
	reg_modify_bits(PPE1_6RD_ID, ntohs(ppe_parse_result->iph.id), 0, 16);
	entry->ipv6_6rd.per_flow_6rd_id = 1;
}


uint16_t IS_IF_PCIE_WLAN(struct sk_buff *skb)
{
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb))
		return IS_IF_PCIE_WLAN_HEAD(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb))
		return IS_IF_PCIE_WLAN_TAIL(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_CB(skb))
		return IS_IF_PCIE_WLAN_CB(skb);
	else
		return 0;
}

uint16_t is_if_pcie_wlan_rx(struct sk_buff *skb)
{
	return IS_IF_PCIE_WLAN_HEAD(skb);
}

uint16_t is_magic_tag_protect_valid(struct sk_buff *skb)
{
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb))
		return IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb))
		return IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_CB(skb))
		return IS_MAGIC_TAG_PROTECT_VALID_CB(skb);
	else
		return 0;
}

unsigned char *FOE_INFO_START_ADDR(struct sk_buff *skb)
{
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb))
		return FOE_INFO_START_ADDR_HEAD(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb))
		return FOE_INFO_START_ADDR_TAIL(skb);
	else if (IS_MAGIC_TAG_PROTECT_VALID_CB(skb))
		return FOE_INFO_START_ADDR_CB(skb);

	pr_notice("!!!FOE_INFO_START_ADDR Error!!!!\n");
	return FOE_INFO_START_ADDR_HEAD(skb);
}

void FOE_INFO_DUMP(struct sk_buff *skb)
{
	pr_notice("FOE_INFO_START_ADDR(skb) =%p\n", FOE_INFO_START_ADDR(skb));
	pr_notice("FOE_TAG_PROTECT(skb) =%x\n", FOE_TAG_PROTECT(skb));
	pr_notice("FOE_ENTRY_NUM(skb) =%x\n", FOE_ENTRY_NUM(skb));
	pr_notice("FOE_ALG(skb) =%x\n", FOE_ALG(skb));
	pr_notice("FOE_AI(skb) =%x\n", FOE_AI(skb));
	pr_notice("FOE_SP(skb) =%x\n", FOE_SP(skb));
	pr_notice("FOE_IF_IDX(skb) =%x\n", FOE_IF_IDX(skb));
	pr_notice("FOE_MAGIC_TAG(skb) =%x\n", FOE_MAGIC_TAG(skb));
	if (fe_feature & WARP_WHNAT) {
		pr_notice("FOE_WDMA_ID(skb) =%x\n", FOE_WDMA_ID(skb));
		pr_notice("FOE_RX_ID(skb) =%x\n", FOE_RX_ID(skb));
		pr_notice("FOE_WC_ID(skb) =%x\n", FOE_WC_ID(skb));
		pr_notice("FOE_FOE_BSS_IDIF(skb) =%x\n", FOE_BSS_ID(skb));
	}
	pr_notice("FOE_MINFO(skb) =%x\n", FOE_MINFO(skb));
	pr_notice("FOE_MINFO_NTYPE(skb) =%x\n", FOE_MINFO_NTYPE(skb));
	pr_notice("FOE_MINFO_CHID(skb) =%x\n", FOE_MINFO_CHID(skb));
}

void FOE_INFO_DUMP_TAIL(struct sk_buff *skb)
{
	pr_notice("FOE_INFO_START_ADDR_TAIL(skb) =%p\n", FOE_INFO_START_ADDR_TAIL(skb));
	pr_notice("FOE_TAG_PROTECT_TAIL(skb) =%x\n", FOE_TAG_PROTECT_TAIL(skb));
	pr_notice("FOE_ENTRY_NUM_TAIL(skb) =%x\n", FOE_ENTRY_NUM_TAIL(skb));
	pr_notice("FOE_ALG_TAIL(skb) =%x\n", FOE_ALG_TAIL(skb));
	pr_notice("FOE_AI_TAIL(skb) =%x\n", FOE_AI_TAIL(skb));
	pr_notice("FOE_SP_TAIL(skb) =%x\n", FOE_SP_TAIL(skb));
	pr_notice("FOE_MAGIC_TAG_TAIL(skb) =%x\n", FOE_MAGIC_TAG_TAIL(skb));
	if (fe_feature & WARP_WHNAT) {
		pr_notice("FOE_WDMA_ID_TAIL(skb) =%x\n", FOE_WDMA_ID_TAIL(skb));
		pr_notice("FOE_RX_ID_TAIL(skb) =%x\n", FOE_RX_ID_TAIL(skb));
		pr_notice("FOE_WC_ID_TAIL(skb) =%x\n", FOE_WC_ID_TAIL(skb));
		pr_notice("FOE_FOE_BSS_IDIF_TAIL(skb) =%x\n", FOE_BSS_ID_TAIL(skb));
	}
}

#if 0
extern int32_t ppe_parse_layer_info(struct sk_buff *skb, struct pkt_parse_result *ppe_parse_result);

u32 syn_seq = 0;
u32 ppe_tx_wifi_cnt = 0;
int32_t ppe_get_tcp_seq(struct sk_buff *skb, const char *func) {

	struct iphdr *iph = NULL;
	struct tcphdr *th = NULL;

	iph = (struct iphdr *)skb_network_header(skb);

	if (iph->protocol == IPPROTO_TCP) {

		th = (struct tcphdr *)skb_transport_header(skb);
		if (th->syn == 1)
			syn_seq = ntohl(th->seq);

		pr_notice("%s %s, source:%u, dest:%u, syn_seq:%u, seq:%u, relative_seq:%u, ppe_tx_wifi_cnt:%u\n", func, __func__,
			ntohs(th->source), ntohs(th->dest), syn_seq, ntohl(th->seq), ntohl(th->seq) - syn_seq, ppe_tx_wifi_cnt++);

		return ntohl(th->seq);
	}
	return -1;
}
#endif


int hwnat_info_region;
uint16_t tx_decide_which_region(struct sk_buff *skb)
{
	u32 alg_tmp, sp_tmp, entry_tmp, ai_tmp;

	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb) && IS_SPACE_AVAILABLE_HEAD(skb)) {
		hwnat_info_region = USE_HEAD_ROOM;

		return USE_HEAD_ROOM;	/* use headroom */
	} else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb) && IS_SPACE_AVAILABLE_TAIL(skb) &&
		   IS_SPACE_AVAILABLE_HEAD(skb)) {
		FOE_INFO_START_ADDR(skb);
		alg_tmp = FOE_ALG_TAIL(skb);;
		sp_tmp = FOE_SP_TAIL(skb);
		entry_tmp = FOE_ENTRY_NUM_TAIL(skb);
		ai_tmp = FOE_AI_TAIL(skb);
		FOE_SP(skb) = sp_tmp & 0xf;
		FOE_ENTRY_NUM(skb) = entry_tmp & 0x7fff;
		FOE_AI(skb) = ai_tmp & 0x1f;
		FOE_ALG(skb) = alg_tmp & 0x1;
		FOE_TAG_PROTECT(skb) = FOE_TAG_PROTECT_TAIL(skb);
		FOE_MAGIC_TAG(skb) = FOE_MAGIC_TAG_TAIL(skb);
		if (fe_feature & WARP_WHNAT) {
			FOE_WDMA_ID(skb) = FOE_WDMA_ID_TAIL(skb);
			FOE_RX_ID(skb) = FOE_RX_ID_TAIL(skb);
			FOE_WC_ID(skb) = FOE_WC_ID_TAIL(skb);
			FOE_BSS_ID(skb) = FOE_BSS_ID_TAIL(skb);
		}
		FOE_MINFO(skb) = FOE_MINFO_TAIL(skb);
		FOE_MINFO_NTYPE(skb) = FOE_MINFO_NTYPE_TAIL(skb);
		FOE_MINFO_CHID(skb)= FOE_MINFO_CHID_TAIL(skb);
		hwnat_info_region = USE_TAIL_ROOM;
		return USE_TAIL_ROOM;	/* use tailroom */
	}
	hwnat_info_region = ALL_INFO_ERROR;
	return ALL_INFO_ERROR;
}

uint16_t remove_vlan_tag(struct sk_buff *skb)
{
	struct ethhdr *eth;
	struct vlan_ethhdr *veth;
	u16 vir_if_idx;

	if (skb_vlan_tag_present(skb)) { /*hw vlan rx enable*/
		vir_if_idx = skb_vlan_tag_get(skb) & 0x3fff;
		__vlan_hwaccel_clear_tag(skb);
		return vir_if_idx;
	}

	veth = (struct vlan_ethhdr *)skb_mac_header(skb);
	/* something wrong */
	if ((veth->h_vlan_proto != htons(ETH_P_8021Q))) {
		/* if (pr_debug_ratelimited()) */
		if (debug_level >= 7)
			pr_notice("HNAT: Reentry packet is untagged frame?\n");
		return 65535;
	}
	/*we just want to get vid*/
	vir_if_idx = ntohs(veth->h_vlan_TCI) & 0x3fff;

	if (skb_cloned(skb) || skb_shared(skb)) {
		struct sk_buff *new_skb;

		new_skb = skb_copy(skb, GFP_ATOMIC);
		kfree_skb(skb);
		if (!new_skb)
			return 65535;
		skb = new_skb;
		/*logic error*/
		/* kfree_skb(new_skb); */
	}

	/* remove VLAN tag */
	skb->data = skb_mac_header(skb);
	skb->mac_header = skb->mac_header + VLAN_HLEN;
	memmove(skb_mac_header(skb), skb->data, ETH_ALEN * 2);

	skb_pull(skb, VLAN_HLEN);
	skb->data += ETH_HLEN;	/* pointer to layer3 header */
	eth = (struct ethhdr *)skb_mac_header(skb);

	skb->protocol = eth->h_proto;
	return vir_if_idx;
}

#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
void hnat_info_init(struct device *dev)
{
	struct medmcu_desc_info_t *desc_info = get_desc_info();
	//dma_addr_t info_phy_base;
	//u32 info_tbl_size;

	//info_tbl_size = MED_INFO_SIZE * sizeof(struct MED_HNAT_INFO_HOST);
	//med_info_base = dma_alloc_coherent(dev, info_tbl_size, &info_phy_base, GFP_KERNEL);
	med_info_base = desc_info[HNAT_INFO_HOST].base_virt;

	//pr_notice("info_tbl_size = %d\n", info_tbl_size);
	//pr_notice("MED_HNAT_INFO_HOST = %p, fdma_phy_base =%p\n", MED_HNAT_INFO_HOST, fdma_phy_base);

	//reg_write(MEDHW_SSR1_DST_RB0_BASE, info_phy_base);
	//reg_write(MEDHW_SSR1_DST_RB0_SIZE, MED_INFO_SIZE);

}
#endif

int foe_alloc_tbl(u32 num_of_entry, struct device *dev)
{
	u32 foe_tbl_size;
	u32 mib_tbl_size;

#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
	hnat_info_init(dev);
#endif
	foe_tbl_size = num_of_entry * sizeof(struct foe_entry);
	mib_tbl_size = num_of_entry * sizeof(struct mib_entry);


	pr_notice("%s(), num_of_entry: %d, foe_entry: %ld, mib_entry: %ld, foe_tbl: %d, mib_tbl: %d\n", __func__,
		 num_of_entry, sizeof(struct foe_entry), sizeof(struct mib_entry), foe_tbl_size, mib_tbl_size);


	/* ppe0 entry table */
	ppe_foe_base = dma_alloc_coherent(dev, foe_tbl_size, &ppe_phy_foe_base, GFP_KERNEL);

	if (!ppe_foe_base) {
		pr_notice("%s(), ppe_foe_base is null\n", __func__);
		goto error;
	}

	memset(ppe_foe_base, 0, foe_tbl_size);

	/* ppe1 entry table */
	ppe1_foe_base = dma_alloc_coherent(dev, foe_tbl_size, &ppe1_phy_foe_base, GFP_KERNEL);

	if (!ppe1_foe_base) {
		pr_notice("%s(), ppe1_foe_base is null\n", __func__);
		goto error;
	}

	memset(ppe1_foe_base, 0, foe_tbl_size);

	if (fe_feature & PPE_MIB) {

		/* PPE0 mib */
		ppe_mib_base = dma_alloc_coherent(dev, mib_tbl_size, &ppe_phy_mib_base, GFP_KERNEL);

		if (!ppe_mib_base) {
			pr_notice("%s(), ppe_mib_base is null\n", __func__);
			goto error;
		}

		memset(ppe_mib_base, 0, mib_tbl_size);

		/* PPE1 mib */
		ppe1_mib_base = dma_alloc_coherent(dev, mib_tbl_size, &ppe1_phy_mib_base, GFP_KERNEL);

		if (!ppe1_mib_base) {
			pr_notice("%s(), ppe1_mib_base is null\n", __func__);
			goto error;
		}

		memset(ppe1_mib_base, 0, mib_tbl_size);
	}

	return 1;

error:

	if (ppe_foe_base)
		dma_free_coherent(dev, foe_tbl_size, ppe_foe_base, ppe_phy_foe_base);
	if (ppe1_foe_base)
		dma_free_coherent(dev, foe_tbl_size, ppe1_foe_base, ppe1_phy_foe_base);
	if (ppe_mib_base)
		dma_free_coherent(dev, mib_tbl_size, ppe_mib_base, ppe_phy_mib_base);
	if (ppe1_mib_base)
		dma_free_coherent(dev, mib_tbl_size, ppe1_mib_base, ppe1_phy_mib_base);
	return 0;
}

static uint8_t *show_cpu_reason(struct sk_buff *skb)
{
	static u8 buf[32];
	int ret;

	switch (FOE_AI(skb)) {
	case TTL_0:
		return "IPv4(IPv6) TTL(hop limit)\n";
	case HAS_OPTION_HEADER:
		return "Ipv4(IPv6) has option(extension) header\n";
	case NO_FLOW_IS_ASSIGNED:
		return "No flow is assigned\n";
	case IPV4_WITH_FRAGMENT:
		return "IPv4 HNAT doesn't support IPv4 /w fragment\n";
	case IPV4_HNAPT_DSLITE_WITH_FRAGMENT:
		return "IPv4 HNAPT/DS-Lite doesn't support IPv4 /w fragment\n";
	case IPV4_HNAPT_DSLITE_WITHOUT_TCP_UDP:
		return "IPv4 HNAPT/DS-Lite can't find TCP/UDP sport/dport\n";
	case IPV6_5T_6RD_WITHOUT_TCP_UDP:
		return "IPv6 5T-route/6RD can't find TCP/UDP sport/dport\n";
	case TCP_FIN_SYN_RST:
		return "Ingress packet is TCP fin/syn/rst\n";
	case UN_HIT:
		return "FOE Un-hit\n";
	case HIT_UNBIND:
		return "FOE Hit unbind\n";
	case HIT_UNBIND_RATE_REACH:
		return "FOE Hit unbind & rate reach\n";
	case HIT_BIND_TCP_FIN:
		return "Hit bind PPE TCP FIN entry\n";
	case HIT_BIND_TTL_1:
		return "Hit bind PPE entry and TTL(hop limit) = 1 and TTL(hot limit) - 1\n";
	case HIT_BIND_WITH_VLAN_VIOLATION:
		return "Hit bind and VLAN replacement violation\n";
	case HIT_BIND_EXCEED_MDMA_MAX_LENGTH:
		return "Hit bind and exceed MDMA max length\n";
	case UN_HIT_MC_REPLACE_UC_IN_PPE_TABLE:
		return "Un-hit and PPE entry and multicast replace unicast in PPE tabl\n";
	case HIT_BIND_KEEPALIVE_DUP_OLD_HDR:
		return "Hit bind and keep alive with duplicate old-header packet\n";
	case HIT_BIND_FORCE_TO_CPU:
		return "FOE Hit bind & force to CPU\n";
	case HIT_BIND_EXCEED_MTU:
		return "Hit bind and exceed MTU\n";
	case HIT_BIND_MULTICAST_TO_CPU:
		return "Hit bind multicast packet to CPU\n";
	case HIT_BIND_MULTICAST_TO_GMAC_CPU:
		return "Hit bind multicast packet to GMAC & CPU\n";
	case HIT_PRE_BIND:
		return "Pre bind\n";
	}

	ret = snprintf(buf, sizeof(buf), "CPU Reason Error - %X\n", FOE_AI(skb));
	if (ret < 0 || ret >= sizeof(buf))
		pr_notice("%s(), err: %d\n", __func__, ret);

	return buf;
}

#if (1)
uint32_t foe_dump_pkt_tx(struct sk_buff *skb, struct foe_entry *entry)
{

	NAT_PRINT("\nTx===<FOE_Entry=%d, name=%s>=====\n", FOE_ENTRY_NUM(skb), skb->dev->name);
	foe_dump_pkt(skb, entry);

	return 1;
#if(0)
	struct foe_entry *entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	int i;

	NAT_PRINT("\nTx===<FOE_Entry=%d>=====\n", FOE_ENTRY_NUM(skb));
	pr_notice("Tx handler skb_headroom size = %u, skb->head = %p, skb->data = %p\n",
		skb_headroom(skb), skb->head, skb->data);
	for (i = 0; i < skb_headroom(skb); i++) {
		pr_notice("tx_skb->head[%d]=%x\n", i, *(unsigned char *)(skb->head + i));
		/* pr_notice("%02X-",*((unsigned char*)i)); */
	}

	NAT_PRINT("==================================\n");
	return 1;
#endif
}
#endif

uint32_t foe_dump_pkt(struct sk_buff *skb, struct foe_entry *entry)
{
	//struct foe_entry *entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];

	//NAT_PRINT("\nRx===<FOE_Entry=%d>=====\n", FOE_ENTRY_NUM(skb));
	NAT_PRINT("RcvIF=%s\n", skb->dev->name);
	NAT_PRINT("FOE_Entry=%d\n", FOE_ENTRY_NUM(skb));
	NAT_PRINT("CPU Reason=%s", show_cpu_reason(skb));
	NAT_PRINT("ALG=%d\n", FOE_ALG(skb));
	NAT_PRINT("SP=%d\n", FOE_SP(skb));

	/* some special alert occurred, so entry_num is useless (just skip it) */
	if (FOE_ENTRY_NUM(skb) == 0x3fff)
		return 1;

	/* PPE: IPv4 packet=IPV4_HNAT IPv6 packet=IPV6_ROUTE */
	if (IS_IPV4_GRP(entry)) {
		NAT_PRINT("Information Block 1=%x\n", entry->ipv4_hnapt.info_blk1);
		NAT_PRINT("SIP=%s\n", ip_to_str(entry->ipv4_hnapt.sip));
		NAT_PRINT("DIP=%s\n", ip_to_str(entry->ipv4_hnapt.dip));
		NAT_PRINT("SPORT=%d\n", entry->ipv4_hnapt.sport);
		NAT_PRINT("DPORT=%d\n", entry->ipv4_hnapt.dport);
		NAT_PRINT("Information Block 2=%x\n", entry->ipv4_hnapt.info_blk2);
		NAT_PRINT("State = %s, proto = %s\n",
			  entry->bfib1.state ==
			  0 ? "Invalid" : entry->bfib1.state ==
			  1 ? "Unbind" : entry->bfib1.state ==
			  2 ? "BIND" : entry->bfib1.state ==
			  3 ? "FIN" : "Unknown", entry->ipv4_hnapt.bfib1.udp ==
			  0 ? "TCP" : entry->ipv4_hnapt.bfib1.udp ==
			  1 ? "UDP" : "Unknown");
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			NAT_PRINT("Information Block 1=%x\n", entry->ipv6_5t_route.info_blk1);
			NAT_PRINT("IPv6_SIP=%08X:%08X:%08X:%08X\n",
				  entry->ipv6_5t_route.ipv6_sip0,
				  entry->ipv6_5t_route.ipv6_sip1,
				  entry->ipv6_5t_route.ipv6_sip2, entry->ipv6_5t_route.ipv6_sip3);
			NAT_PRINT("IPv6_DIP=%08X:%08X:%08X:%08X\n",
				  entry->ipv6_5t_route.ipv6_dip0,
				  entry->ipv6_5t_route.ipv6_dip1,
				  entry->ipv6_5t_route.ipv6_dip2, entry->ipv6_5t_route.ipv6_dip3);
			if (IS_IPV6_FLAB_EBL()) {
				NAT_PRINT("Flow Label=%08X\n", (entry->ipv6_5t_route.sport << 16) |
					  (entry->ipv6_5t_route.dport));
			} else {
				NAT_PRINT("SPORT=%d\n", entry->ipv6_5t_route.sport);
				NAT_PRINT("DPORT=%d\n", entry->ipv6_5t_route.dport);
			}
			NAT_PRINT("Information Block 2=%x\n", entry->ipv6_5t_route.info_blk2);
			NAT_PRINT("State = %s, proto = %s\n",
				  entry->bfib1.state ==
				  0 ? "Invalid" : entry->bfib1.state ==
				  1 ? "Unbind" : entry->bfib1.state ==
				  2 ? "BIND" : entry->bfib1.state ==
				  3 ? "FIN" : "Unknown", entry->ipv6_5t_route.bfib1.udp ==
				  0 ? "TCP" : entry->ipv6_5t_route.bfib1.udp ==
				  1 ? "UDP" : "Unknown");
		}
	}
	if ((!IS_IPV4_GRP(entry)) && (!(IS_IPV6_GRP(entry))))
		NAT_PRINT("unknown Pkt_type=%d\n", entry->bfib1.pkt_type);

	NAT_PRINT("==================================\n");
	return 1;
}

uint32_t hnat_cpu_reason_cnt(struct sk_buff *skb)
{
	switch (FOE_AI(skb)) {
	case TTL_0:
		dbg_cpu_reason_cnt[0]++;
		return 0;
	case HAS_OPTION_HEADER:
		dbg_cpu_reason_cnt[1]++;
		return 0;
	case NO_FLOW_IS_ASSIGNED:
		dbg_cpu_reason_cnt[2]++;
		return 0;
	case IPV4_WITH_FRAGMENT:
		dbg_cpu_reason_cnt[3]++;
		return 0;
	case IPV4_HNAPT_DSLITE_WITH_FRAGMENT:
		dbg_cpu_reason_cnt[4]++;
		return 0;
	case IPV4_HNAPT_DSLITE_WITHOUT_TCP_UDP:
		dbg_cpu_reason_cnt[5]++;
		return 0;
	case IPV6_5T_6RD_WITHOUT_TCP_UDP:
		dbg_cpu_reason_cnt[6]++;
		return 0;
	case TCP_FIN_SYN_RST:
		dbg_cpu_reason_cnt[7]++;
		return 0;
	case UN_HIT:
		dbg_cpu_reason_cnt[8]++;
		return 0;
	case HIT_UNBIND:
		dbg_cpu_reason_cnt[9]++;
		return 0;
	case HIT_UNBIND_RATE_REACH:
		dbg_cpu_reason_cnt[10]++;
		return 0;
	case HIT_BIND_TCP_FIN:
		dbg_cpu_reason_cnt[11]++;
		return 0;
	case HIT_BIND_TTL_1:
		dbg_cpu_reason_cnt[12]++;
		return 0;
	case HIT_BIND_WITH_VLAN_VIOLATION:
		dbg_cpu_reason_cnt[13]++;
		return 0;
	case HIT_BIND_EXCEED_MDMA_MAX_LENGTH:
		dbg_cpu_reason_cnt[14]++;
		return 0;
	case UN_HIT_MC_REPLACE_UC_IN_PPE_TABLE:
		dbg_cpu_reason_cnt[15]++;
		return 0;
	case HIT_BIND_KEEPALIVE_DUP_OLD_HDR:
		dbg_cpu_reason_cnt[16]++;
		return 0;
	case HIT_BIND_FORCE_TO_CPU:
		dbg_cpu_reason_cnt[17]++;
		return 0;
	case HIT_BIND_EXCEED_MTU:
		dbg_cpu_reason_cnt[18]++;
		return 0;
	case HIT_BIND_MULTICAST_TO_CPU:
		dbg_cpu_reason_cnt[19]++;
		return 0;
	case HIT_BIND_MULTICAST_TO_GMAC_CPU:
		dbg_cpu_reason_cnt[20]++;
		return 0;
	case HIT_PRE_BIND:
		dbg_cpu_reason_cnt[21]++;
		return 0;
	}

	return 0;
}

int get_bridge_info(void)
{
	struct net_device *br0_dev;
	struct in_device *br0_in_dev;

	if (fe_feature & HNAT_OPENWRT)
		br0_dev = dev_get_by_name(&init_net, "br-lan");
	else
		br0_dev = dev_get_by_name(&init_net, "br0");

	if (!br0_dev) {
		pr_notice("br0_dev = NULL\n");
		return 1;
	}
	br0_in_dev = in_dev_get(br0_dev);
	if (!br0_in_dev) {
		pr_notice("br0_in_dev = NULL\n");
		return 1;
	}
	br_netmask = ntohl(br0_in_dev->ifa_list->ifa_mask);
	br0_ip = ntohl(br0_in_dev->ifa_list->ifa_address);
	if (br0_dev)
		dev_put(br0_dev);

	if (br0_in_dev)
		in_dev_put(br0_in_dev);
	else
		pr_notice("br0_in_dev = NULL\n");

	pr_notice("br0_ip = %x\n", br0_ip);
	pr_notice("br_netmask = %x\n", br_netmask);
	get_brlan = 1;

	return 0;
}

int bridge_lan_subnet(struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	u32 daddr = 0;
	u32 saddr = 0;
	u32 eth_type;
	u32 ppp_tag = 0;
	struct vlan_hdr *vh = NULL;
	struct ethhdr *eth = NULL;
	struct pppoe_hdr *peh = NULL;
	u8 vlan1_gap = 0;
	u8 vlan2_gap = 0;
	u8 pppoe_gap = 0;
	int ret;
#ifdef	CONFIG_RAETH_HW_VLAN_TX
	struct vlan_hdr pseudo_vhdr;
#endif

	eth = (struct ethhdr *)skb->data;
	if (is_multicast_ether_addr(&eth->h_dest[0]))
		return 0;
	eth_type = eth->h_proto;
	if ((eth_type == htons(ETH_P_8021Q)) ||
	    (((eth_type) & 0x00FF) == htons(ETH_P_8021Q)) || skb_vlan_tag_present(skb)) {
#ifdef	CONFIG_RAETH_HW_VLAN_TX
		pseudo_vhdr.h_vlan_TCI = htons(skb_vlan_tag_get(skb));
		pseudo_vhdr.h_vlan_encapsulated_proto = eth->h_proto;
		vh = (struct vlan_hdr *)&pseudo_vhdr;
		vlan1_gap = VLAN_HLEN;
#else
		vlan1_gap = VLAN_HLEN;
		vh = (struct vlan_hdr *)(skb->data + ETH_HLEN);
#endif

		/* VLAN + PPPoE */
		if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
			pppoe_gap = 8;
			eth_type = vh->h_vlan_encapsulated_proto;
			/* Double VLAN = VLAN + VLAN */
		} else if ((vh->h_vlan_encapsulated_proto == htons(ETH_P_8021Q)) ||
			   ((vh->h_vlan_encapsulated_proto) & 0x00FF) == htons(ETH_P_8021Q)) {
			vlan2_gap = VLAN_HLEN;
			vh = (struct vlan_hdr *)(skb->data + ETH_HLEN + VLAN_HLEN);
			/* VLAN + VLAN + PPPoE */
			if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
				pppoe_gap = 8;
				eth_type = vh->h_vlan_encapsulated_proto;
			} else {
				eth_type = vh->h_vlan_encapsulated_proto;
			}
		}
	} else if (ntohs(eth_type) == ETH_P_PPP_SES) {
		/* PPPoE + IP */
		pppoe_gap = 8;
		peh = (struct pppoe_hdr *)(skb->data + ETH_HLEN + vlan1_gap);
		ppp_tag = peh->tag[0].tag_type;
	}

	if (get_brlan == 0) {
		ret = get_bridge_info(); /*return 1 br0 get fail*/
		if (ret == 1)
			return 0;
	}
	/* set layer4 start addr */
	if ((eth_type == htons(ETH_P_IP)) || (eth_type == htons(ETH_P_PPP_SES) && ppp_tag == htons(PPP_IP))) {
		iph = (struct iphdr *)(skb->data + ETH_HLEN + vlan1_gap + vlan2_gap + pppoe_gap);
		daddr = ntohl(iph->daddr);
		saddr = ntohl(iph->saddr);
	}

	if (((br0_ip & br_netmask) == (daddr & br_netmask)) &&
	    ((daddr & br_netmask) == (saddr & br_netmask)))
		return 1;
	return 0;
}

int bridge_short_cut_rx(struct sk_buff *skb)
{
	struct iphdr *iph = NULL;
	u32 daddr;
	int ret;

	if (get_brlan == 0) {
		ret = get_bridge_info(); /*return 1 get br0 fail*/
		if (ret == 1)
			return 0;
	}

	iph = (struct iphdr *)(skb->data);
	daddr = ntohl(iph->daddr);
	if ((br0_ip & br_netmask) == (daddr & br_netmask))
		return 1;
	else
		return 0;
}

#define PPE_FAST_PATH_OK(dev) (netif_running(dev) && netif_carrier_ok(dev))

#define IS_FAST_PATH_UP \
	((dst_port[DP_GMAC1] && PPE_FAST_PATH_OK(dst_port[DP_GMAC1])) || \
	 (second_fast_path && dst_port[DP_GMAC2] && PPE_FAST_PATH_OK(dst_port[DP_GMAC2])))

#define IS_FAST_PATH_USER(skb) \
	(FOE_MAGIC_TAG(skb) == FOE_MAGIC_PCI || \
	 FOE_MAGIC_TAG(skb) == FOE_MAGIC_WLAN || \
	 FOE_MAGIC_TAG(skb) == FOE_MAGIC_RNDIS || \
	 FOE_MAGIC_TAG(skb) == FOE_MAGIC_SNPS)

uint32_t ppe_extif_rx_prepare(struct sk_buff *skb)
{
	u16 vir_if_idx = 0;
	struct ethhdr *eth = (struct ethhdr *)skb_mac_header(skb);
	struct net_device *dst_dev;

	/* Check the supported protocol */
	if (skb->protocol != htons(ETH_P_8021Q) &&
	    skb->protocol != htons(ETH_P_IP) &&
	    skb->protocol != htons(ETH_P_IPV6) &&
	    skb->protocol != htons(ETH_P_PPP_SES) &&
	    skb->protocol != htons(ETH_P_PPP_DISC)) {
		if (debug_level >= 10)
			pr_notice("%s, not support protocol = 0x%x\n", __func__, skb->protocol);
		return 1;
	} else if (is_multicast_ether_addr(&eth->h_dest[0])) {
		if (debug_level >= 10)
			pr_notice("%s, not support multicast\n", __func__);
		return 1;
	}

	if (debug_level >= 10)
		pr_notice("%s, name = %s, protocol = 0x%x, skb-headroom=%d\n", __func__,
			skb->dev->name, skb->protocol, skb_headroom(skb));

	/* Check the sw-fast-path hook is enabled */
	if (!ppe_hook_rx_eth) {
		if (debug_level >= 3)
			pr_notice("%s no rx hook, can't do pingpong\n", __func__);
		return 1;
	}

	/* Check the source interface is registered */
	vir_if_idx = FOE_IF_IDX(skb);
	if (vir_if_idx == INVALID_IFIDX) {
		if (debug_level >= 1)
			pr_notice("%s UnKnown Interface, vir_if_idx=%d\n", __func__, vir_if_idx);
		return 1;
	}

	/* Check the fast path is running */
	dst_dev = (second_fast_path && !PPE_FAST_PATH_OK(dst_port[DP_GMAC1])) ?
		  dst_port[DP_GMAC2] : dst_port[DP_GMAC1];

	if (!dst_dev || !PPE_FAST_PATH_OK(dst_dev)) {
		if (debug_level >= 1)
			pr_notice("%s Fast path %s is Down\n", __func__, dst_dev->name);
		return 1;
	}

	/* Check the headroom and tailroom */
	if (skb_headroom(skb) < FOE_INFO_LEN + ETH_HLEN + VLAN_HLEN) {
		if (debug_level >= 3)
			pr_notice("%s headroom isn't enough\n", __func__);
		return 1;
	}

	if (!IS_SPACE_AVAILABLE_HEAD(skb) && !IS_SPACE_AVAILABLE_TAIL(skb)) {
		if (debug_level >= 3)
			pr_notice("%s, check why! skb with vlan return to caller, headroom:%d, tailroom:%d\n",
				__func__, skb_headroom(skb), skb_tailroom(skb));
		return 1;
	}

	/* ----------- modification ----------- */

	skb_set_network_header(skb, 0);

	/* push vlan tag to stand for actual incoming interface, */
	/* so HNAT module can know the actual incoming interface from vlan id. */
	skb_push(skb, ETH_HLEN);/* pointer to layer2 header before calling hard_start_xmit */

	/* push the poped vlan back */
	if (skb_vlan_tag_present(skb)) {
		skb = vlan_insert_tag(skb, skb->vlan_proto, skb_vlan_tag_get(skb));
		if (skb == NULL) {
			if (debug_level >= 3)
				pr_notice("%s, vlan_insert_tag() frees the skb\n", __func__);
			return 0;
		}
		__vlan_hwaccel_clear_tag(skb);
	}

	return 0;
}

uint32_t ppe_extif_rx_send(struct sk_buff *skb)
{
	/* push the source interface */
	skb = vlan_insert_tag(skb, htons(ETH_P_8021Q), FOE_IF_IDX(skb));
	if (skb == NULL) {
		if (debug_level >= 3)
			pr_notice("%s, vlan_insert_tag() frees the skb\n", __func__);
		return 0;
	}

	/* assign destination interface */
	skb->dev = (second_fast_path && !PPE_FAST_PATH_OK(dst_port[DP_GMAC1])) ?
		    dst_port[DP_GMAC2] : dst_port[DP_GMAC1];

	/* redirect to PPE */
	if (IS_SPACE_AVAILABLE_HEAD(skb)) {

		FOE_AI(skb) = UN_HIT;
		FOE_TAG_PROTECT(skb) = TAG_PROTECT;
		FOE_MAGIC_TAG(skb) = FOE_MAGIC_PPE;

	} else if (IS_SPACE_AVAILABLE_TAIL(skb)) {

		FOE_AI_TAIL(skb) = UN_HIT;
		FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
		FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_PPE;

	} else {
		if (debug_level >= 3)
			pr_notice("%s, check why! skb with vlan return to caller, headroom:%d, tailroom:%d\n",
				__func__, skb_headroom(skb), skb_tailroom(skb));
		return 1;
	}

	if (debug_level >= 10)
		pr_notice("%s, send to ppe via %s tx\n", __func__, skb->dev->name);

	dev_queue_xmit(skb);

	return 0;
}

/* push different VID for SNPS */
uint32_t ppe_snps_rx_handler(struct sk_buff *skb)
{
	if (ppe_extif_rx_prepare(skb))
		return 1;

	if (ppe_extif_rx_send(skb))
		return 1;

	return 0;
}

/* push different VID for WiFi pseudo interface or USB external NIC */
uint32_t ppe_extif_rx_handler(struct sk_buff *skb)
{
	u16 vir_if_idx = 0;
	struct ethhdr *eth = (struct ethhdr *)skb_mac_header(skb);

	/* PPE can only handle IPv4/IPv6/PPP packets */
	if (((skb->protocol != htons(ETH_P_8021Q)) &&
	    (skb->protocol != htons(ETH_P_IP)) && (skb->protocol != htons(ETH_P_IPV6)) &&
	    (skb->protocol != htons(ETH_P_PPP_SES)) && (skb->protocol != htons(ETH_P_PPP_DISC))) ||
			is_multicast_ether_addr(&eth->h_dest[0])) {

			if (debug_level >= 10)
				pr_notice("%s not support, skb->protocol = 0x%x, multicast:%d\n", __func__, skb->protocol, is_multicast_ether_addr(&eth->h_dest[0]));
			hwnat_magic_tag_set_zero(skb);
			return 1;
	}

	if (debug_level >= 10)
		pr_notice("%s, name = %s, protocol = 0x%x, skb-headroom=%d\n", __func__,
			skb->dev->name, skb->protocol, skb_headroom(skb));

	skb_set_network_header(skb, 0);

#ifdef CONFIG_SUPPORT_WLAN_OPTIMIZE
		if (bridge_short_cut_rx(skb))
			return 1;	/* Bridge ==> sw path (rps) */
#endif

	vir_if_idx = FOE_IF_IDX(skb);
	if (vir_if_idx == INVALID_IFIDX) {
		if (debug_level >= 1)
			pr_notice("%s UnKnown Interface, vir_if_idx=%d\n", __func__, vir_if_idx);
		return 1;
	}

	/* currently, skb->data points to layer 3 */
	if (skb_headroom(skb) < FOE_INFO_LEN + ETH_HLEN + VLAN_HLEN) {
		if (debug_level >= 3)
			pr_notice("%s headroom isn't enough\n", __func__);
		return 1;
	}

	/* push vlan tag to stand for actual incoming interface, */
	/* so HNAT module can know the actual incoming interface from vlan id. */
	skb_push(skb, ETH_HLEN);/* pointer to layer2 header before calling hard_start_xmit */

	if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0 && DP_EDMA0 >= 1 && DP_EDMA0 < MAX_IF_NUM)
		skb->dev = dst_port[DP_EDMA0];
	else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1 && DP_EDMA1 >= 1 && DP_EDMA1 < MAX_IF_NUM)
		skb->dev = dst_port[DP_EDMA1];
	else if (DP_GMAC1 >= 1 && DP_GMAC1 < MAX_IF_NUM && dst_port[DP_GMAC1] != NULL)
		skb->dev = dst_port[DP_GMAC1];	/* we use GMAC1 to send the packet to PPE */
	else if (DP_GMAC2 >= 1 && DP_GMAC2 < MAX_IF_NUM && dst_port[DP_GMAC2] != NULL)
		skb->dev = dst_port[DP_GMAC2];	/* we use GMAC2 to send the packet to PPE */
	else {
		if (debug_level >= 7)
			pr_notice("%s no ETH interface to tx\n", __func__);
		return 1;
	}

#ifdef CONFIG_SUPPORT_WLAN_QOS
		set_qid(skb);
#endif
	skb->vlan_proto = htons(ETH_P_8021Q);
#ifdef	CONFIG_RAETH_HW_VLAN_TX
	skb->vlan_present = true;
	skb->vlan_tci |= vir_if_idx;
#else
	skb = vlan_insert_tag(skb, skb->vlan_proto, vir_if_idx);
	if (skb == NULL) {
		if (debug_level >= 3)
			pr_notice("%s, vlan_insert_tag() frees the skb\n", __func__);
		return 0;
	}
#endif
	if (IS_SPACE_AVAILABLE_HEAD(skb)) {
		/* redirect to PPE */
		FOE_AI(skb) = UN_HIT;
		FOE_TAG_PROTECT(skb) = TAG_PROTECT;

		if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0)
			FOE_MAGIC_TAG(skb) = FOE_MAGIC_PPE0;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1)
			FOE_MAGIC_TAG(skb) = FOE_MAGIC_PPE1;
		else
			FOE_MAGIC_TAG(skb) = FOE_MAGIC_PPE;

	} else if (IS_SPACE_AVAILABLE_TAIL(skb)) {
		/* redirect to PPE */

		FOE_AI_TAIL(skb) = UN_HIT;
		FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;

		if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0)
			FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_PPE0;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1)
			FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_PPE1;
		else
			FOE_MAGIC_TAG_TAIL(skb) = FOE_MAGIC_PPE;

	} else {

		if (debug_level >= 3)
			pr_notice("%s, check why! skb with vlan return to caller, headroom:%d, tailroom:%d\n",
				__func__, skb_headroom(skb), skb_tailroom(skb));
		return 1;
	}

#ifdef CONFIG_SUPPORT_WLAN_QOS
		/*if (debug_level >= 2)*/
			/*pr_notice("skb->dev = %s\n", skb->dev);*/
		if ((!skb->dev) || ((skb->dev != dst_port[DP_GMAC2]) &&
		    (skb->dev != dst_port[DP_GMAC1])))
			skb->dev = dst_port[DP_GMAC1];	/* we use GMAC1 to send the packet to PPE */
#endif
	if (debug_level >= 10)
		pr_notice("%s, send to ppe via ETH tx\n", __func__);
	dev_queue_xmit(skb);

	return 0;
}

int32_t ppe_eth_tx_fport_handler(struct sk_buff *skb)
{
	int ret = 0;
	struct ethhdr *eth_hdr = (struct ethhdr *)skb_mac_header(skb);

	/* PPE can only handle IPv4/IPv6/PPP packets */
	if (((skb->protocol != htons(ETH_P_8021Q)) &&
	    (skb->protocol != htons(ETH_P_IP)) && (skb->protocol != htons(ETH_P_IPV6)) &&
	    (skb->protocol != htons(ETH_P_PPP_SES)) && (skb->protocol != htons(ETH_P_PPP_DISC))) ||
	    is_multicast_ether_addr(&eth_hdr->h_dest[0])) {
		hwnat_magic_tag_set_zero(skb);

		if (debug_level >= 10)
			pr_info("%s not support, skb->protocol = 0x%x, multicast:%d\n",
				__func__, skb->protocol, is_multicast_ether_addr(&eth_hdr->h_dest[0]));
	} else {
		if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb)) {
			if (FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_PPE && FOE_AI(skb) == UN_HIT) {
				ret = 1;
				FOE_MAGIC_TAG(skb) = 0;
			}
		} else if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb)) {
			if (FOE_MAGIC_TAG_TAIL(skb) == FOE_MAGIC_PPE && FOE_AI_TAIL(skb) == UN_HIT) {
				ret = 1;
				FOE_MAGIC_TAG(skb) = 0;
			}
		}
	}

	if (debug_level >= 10)
		pr_info("%s end, ret:%d\n", __func__, ret);

	return ret;
}



uint32_t ppe_extif_pingpong_handler(struct sk_buff *skb)
{
	struct ethhdr *eth = NULL;
	u16 vir_if_idx = 0, idx;
	struct net_device *dev;
#ifdef CONFIG_RAETH_EDMA
	struct net_device *aqr_dev;
#endif

	if (skb == NULL) {
		if (debug_level >= 7)
			pr_notice("%s skb == NULL\n", __func__);
		return 1;
	}

	if (debug_level >= 10)
		pr_notice("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d, name:%s\n",
			__func__, FOE_AI(skb), FOE_SP(skb), skb->dev->name);

	vir_if_idx = remove_vlan_tag(skb);

  	if ((skb == NULL) || (vir_if_idx == 65535)) {
		if (debug_level >= 7)
  			pr_notice("%s, vir_if_idx is 65535\n", __func__);
  		return 1;
  	}

	/* recover to right incoming interface */
	if (vir_if_idx < MAX_IF_NUM && dst_port[vir_if_idx]) {
		skb->dev = dst_port[vir_if_idx];
		FOE_IF_IDX(skb) = vir_if_idx;
	} else {
		if (debug_level >= 1)
			pr_notice("%s : HNAT: unknown interface (vir_if_idx=%d)\n", __func__, vir_if_idx);
		return 1;
	}

	eth = (struct ethhdr *)skb_mac_header(skb);

	if (eth->h_dest[0] & 1) {
		if (ether_addr_equal(eth->h_dest, skb->dev->broadcast))
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	} else {
		skb->pkt_type = PACKET_OTHERHOST;

		for (idx = 0; idx < MAX_IF_NUM; idx++) {
			dev = dst_port[idx];
			if (dev && ether_addr_equal(eth->h_dest, dev->dev_addr)) {
				skb->pkt_type = PACKET_HOST;
				break;
			}
		}
	}


	skb->ip_summed = CHECKSUM_UNNECESSARY;
	if (debug_level >= 7)
		pr_notice("%s, name = %s, vir_if_idx =%d, pkt_type:%d\n",
			__func__, skb->dev->name, vir_if_idx, skb->pkt_type);

	return 1;
}

uint32_t keep_alive_handler(struct sk_buff *skb, struct foe_entry *entry)
{
	struct ethhdr *eth = NULL;
	u16 eth_type = ntohs(skb->protocol);
	u32 vlan1_gap = 0;
	u32 vlan2_gap = 0;
	u32 pppoe_gap = 0;
	struct vlan_hdr *vh;
	struct iphdr *iph = NULL;
	struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;

/* try to recover to original SMAC/DMAC, but we don't have such information.*/
/* just use SMAC as DMAC and set Multicast address as SMAC.*/
	eth = (struct ethhdr *)(skb->data - ETH_HLEN);

	hwnat_memcpy(eth->h_dest, eth->h_source, ETH_ALEN);
	hwnat_memcpy(eth->h_source, eth->h_dest, ETH_ALEN);
	eth->h_source[0] = 0x1;	/* change to multicast packet, make bridge not learn this packet */
	if (eth_type == ETH_P_8021Q) {
		vlan1_gap = VLAN_HLEN;
		vh = (struct vlan_hdr *)skb->data;

		if (ntohs(vh->h_vlan_TCI) == wan_vid) {
			/* It make packet like coming from LAN port */
			vh->h_vlan_TCI = htons(lan_vid);

		} else {
			/* It make packet like coming from WAN port */
			vh->h_vlan_TCI = htons(wan_vid);
		}

		if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
			pppoe_gap = 8;
		} else if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_8021Q) {
			vlan2_gap = VLAN_HLEN;
			vh = (struct vlan_hdr *)(skb->data + VLAN_HLEN);

			/* VLAN + VLAN + PPPoE */
			if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
				pppoe_gap = 8;
			} else {
				/* VLAN + VLAN + IP */
				eth_type = ntohs(vh->h_vlan_encapsulated_proto);
			}
		} else {
			/* VLAN + IP */
			eth_type = ntohs(vh->h_vlan_encapsulated_proto);
		}
	}

	/* Only Ipv4 NAT need KeepAlive Packet to refresh iptable */
	if (eth_type == ETH_P_IP) {
		iph = (struct iphdr *)(skb->data + vlan1_gap + vlan2_gap + pppoe_gap);
		/* Recover to original layer 4 header */
		if (iph->protocol == IPPROTO_TCP) {
			th = (struct tcphdr *)((uint8_t *)iph + iph->ihl * 4);
			foe_to_org_tcphdr(entry, iph, th);

		} else if (iph->protocol == IPPROTO_UDP) {
			uh = (struct udphdr *)((uint8_t *)iph + iph->ihl * 4);
			foe_to_org_udphdr(entry, iph, uh);
		}
		/* Recover to original layer 3 header */
		foe_to_org_iphdr(entry, iph);
		skb->pkt_type = PACKET_HOST;
	} else if (eth_type == ETH_P_IPV6) {
		skb->pkt_type = PACKET_HOST;
	} else {
		skb->pkt_type = PACKET_HOST;
	}
/* Ethernet driver will call eth_type_trans() to update skb->pkt_type.*/
/* If(destination mac != my mac)*/
/*   skb->pkt_type=PACKET_OTHERHOST;*/
/* In order to pass ip_rcv() check, we change pkt_type to PACKET_HOST here*/
/*	skb->pkt_type = PACKET_HOST;*/
	return 1;
}

uint32_t keep_alive_old_pkt_handler(struct sk_buff *skb)
{
	struct ethhdr *eth = NULL;
	u16 vir_if_idx = 0;
	struct net_device *dev;

	if (skb == NULL) {
		if (debug_level >= 7)
			pr_notice("%s skb == NULL\n", __func__);
		return 1;
	}

	if ((FOE_SP(skb) == 0) || (FOE_SP(skb) == 5)) {

		vir_if_idx = remove_vlan_tag(skb);

		/* recover to right incoming interface */
		if (vir_if_idx < MAX_IF_NUM && dst_port[vir_if_idx]) {
			skb->dev = dst_port[vir_if_idx];
			FOE_IF_IDX(skb) = vir_if_idx;
		} else {
			pr_notice("%s unknown If (vir_if_idx=%d)\n",  __func__, vir_if_idx);
			return 1;
		}
	}

	eth = (struct ethhdr *)skb_mac_header(skb);

	if (eth->h_dest[0] & 1) {
		if (ether_addr_equal(eth->h_dest, skb->dev->broadcast))
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	} else {
		skb->pkt_type = PACKET_OTHERHOST;
		for (vir_if_idx = 0; vir_if_idx < MAX_IF_NUM; vir_if_idx++) {
			dev = dst_port[vir_if_idx];
			if (dev && ether_addr_equal(eth->h_dest, dev->dev_addr)) {
				skb->pkt_type = PACKET_HOST;
				break;
			}
		}
	}

	return 0;
}

int hitbind_force_to_cpu_handler(struct sk_buff *skb, struct foe_entry *entry)
{
	uint32_t act_dp = 0;
#ifdef CONFIG_RAETH_EDMA
	struct net_device *aqr_dev;
#endif
        if (debug_level >= 10)
		pr_notice("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_AI(skb), FOE_SP(skb));


	if (skb == NULL) {
		if (debug_level >= 7)
			pr_notice("%s, skb == NULL\n", __func__);
		return 1;
	}

	act_dp = get_act_dp(entry);
	skb->dev = dst_port[act_dp];

#ifdef CONFIG_RAETH_EDMA

	if(FOE_SP(skb) == MDMA_PSE_PORT) {
		// MD loopback,scenario AQR0 <=> MD force bind

		if (debug_level >= 7) {
			pr_notice("[HS-ethernet/HWNAT/RX-bind] md loopback");
		}

		if (aqr_dev1 != NULL) {
			aqr_dev = aqr_dev1;
		} else {
			aqr_dev1 = ra_dev_get_by_name(AQR_DEV_NAME);
			aqr_dev = aqr_dev1;
		}
		skb->dev = aqr_dev;

	} else {

		if (strcmp(dst_port[entry->ipv4_hnapt.act_dp]-> name, DEV_NAME) == 0) {
			if (debug_level >= 7) {
				pr_notice("[HS-ethernet/HWNAT/RX-bind] hitbind_force_to_cpu_handler aqr0");
			}

			if (aqr_dev1 != NULL) {
				aqr_dev = aqr_dev1;
			} else {
				aqr_dev1 = ra_dev_get_by_name(AQR_DEV_NAME);
				aqr_dev = aqr_dev1;
			}
			skb->dev = aqr_dev;
		} else if (strcmp(dst_port[entry->ipv4_hnapt.act_dp]-> name, DEV2_NAME) == 0) {
			if (debug_level >= 7) {
				pr_notice("[HS-ethernet/HWNAT/RX-bind] hitbind_force_to_cpu_handler aqr1");
			}

			if (aqr_dev2 != NULL) {
				aqr_dev = aqr_dev2;
			} else {
				aqr_dev2 = ra_dev_get_by_name(AQR_DEV2_NAME);
				aqr_dev = aqr_dev2;
			}
			skb->dev = aqr_dev;
		}
	}
#endif

	/* interface is unknown */
	if (!skb->dev) {
		if (debug_level >= 1)
			pr_notice("%s, interface is unknown, act_dp = %d\n", __func__, act_dp);
		kfree_skb(skb);
		return 0;
	}
	skb_set_network_header(skb, 0);
	skb_push(skb, ETH_HLEN); /* pointer to layer2 header */

	if (debug_level >= 7)
		pr_notice("%s, bind to cpu done if name = %s\n",  __func__, skb->dev->name);



	dev_queue_xmit(skb);
	return 0;
}

int hitbind_force_mcast_to_wifi_handler(struct sk_buff *skb)
{
	//int i = 0;
	//struct sk_buff *skb2;
#if(0)
	if (fe_feature & WIFI_HNAT) {
		if (!(fe_feature & GE2_SUPPORT))
			remove_vlan_tag(skb);	/* pointer to layer3 header */
		/*if we only use GMAC1, we need to use vlan id to identify LAN/WAN port*/
		/*otherwise, CPU send untag packet to switch so we don't need to*/
		/*remove vlan tag before sending to WiFi interface*/

		skb_set_network_header(skb, 0);
		skb_push(skb, ETH_HLEN);	/* pointer to layer2 header */

		for (i = 0; i < MAX_IF_NUM; i++) {
			if ((strncmp(dst_port[i]->name, "eth", 3) != 0)) {
				skb2 = skb_clone(skb, GFP_ATOMIC);

				if (!skb2)
					return -ENOMEM;

				skb2->dev = dst_port[i];
				dev_queue_xmit(skb2);
			}
		}
	}
	kfree_skb(skb);
#endif

//dvt test harry
if (debug_level >= 8) {
	pr_notice("muticast to CPU\n");
}
	return 0;
}

void get_cpu_reason_entry(int cpu_reason, struct sk_buff *skb)
{
	if (FOE_AI(skb) == cpu_reason)
		hwnat_dbg_entry = FOE_ENTRY_NUM(skb);
}

int32_t get_pppoe_sid(struct sk_buff *skb, uint32_t vlan_gap, u16 *sid, uint16_t *ppp_tag)
{
	struct pppoe_hdr *peh = NULL;

	peh = (struct pppoe_hdr *)(skb->data + ETH_HLEN + vlan_gap);

	if (debug_level >= 6) {
		NAT_PRINT("\n==============\n");
		NAT_PRINT(" Ver=%d\n", peh->ver);
		NAT_PRINT(" Type=%d\n", peh->type);
		NAT_PRINT(" Code=%d\n", peh->code);
		NAT_PRINT(" sid=%x\n", ntohs(peh->sid));
		NAT_PRINT(" Len=%d\n", ntohs(peh->length));
		NAT_PRINT(" tag_type=%x\n", ntohs(peh->tag[0].tag_type));
		NAT_PRINT(" tag_len=%d\n", ntohs(peh->tag[0].tag_len));
		NAT_PRINT("=================\n");
	}

	*ppp_tag = peh->tag[0].tag_type;
	if (fe_feature & HNAT_IPV6) {
		if (peh->ver != 1 || peh->type != 1 ||
		    (*ppp_tag != htons(PPP_IP) &&
		    *ppp_tag != htons(PPP_IPV6))) {
			return 1;
		    }
	} else {
		if (peh->ver != 1 || peh->type != 1 || *ppp_tag != htons(PPP_IP))
			return 1;
	}

	*sid = peh->sid;
	return 0;
}

/* HNAT_V2 can push special tag */
int32_t is_special_tag(u16 eth_type, struct pkt_parse_result *ppe_parse_result)
{
	/* Please modify this function to speed up the packet with special tag
	 * Ex:
	 *    Ralink switch = 0x81xx
	 *    Realtek switch = 0x8899
	 */
	if ((eth_type & 0x00FF) == htons(ETH_P_8021Q)) {	/* Ralink Special Tag: 0x81xx */
		ppe_parse_result->vlan_tag = eth_type;
		return 1;
	} else {
		return 0;
	}
}

int32_t is8021Q(u16 eth_type, struct pkt_parse_result *ppe_parse_result)
{
	if (eth_type == htons(ETH_P_8021Q)) {
		ppe_parse_result->vlan_tag = eth_type;
		return 1;
	} else {
		return 0;
	}
}

int32_t is_hw_vlan_tx(struct sk_buff *skb, struct pkt_parse_result *ppe_parse_result)
{
#ifdef CONFIG_RAETH_HW_VLAN_TX
		if (skb_vlan_tag_present(skb)) {
			ppe_parse_result->vlan_tag = htons(ETH_P_8021Q);
			return 1;
		} else {
			return 0;
		}
#else
		return 0;
#endif
}

int32_t is_dsa(struct sk_buff *skb, u16 eth_type, struct pkt_parse_result *ppe_parse_result)
{
	if (netdev_uses_dsa(skb->dev)) {
		/* vlan tag: 0x0001, 0x0002, 0x0003, 0x0004 */
		ppe_parse_result->vlan_tag = htons(eth_type);
		return 1;
	} else {
		return 0;
	}
}

bool is_same_subnet(struct foe_entry *entry, uint32_t egress_src_ip, uint32_t egress_dst_ip) {

	 /* Tunnel encapsulation fragmented packets when the LAN/WAN mtu are both 1500.
	 * E.g.
	 *   ori [mac+vlan+ipv4+tcp]+payload
	 *   1st {mac+ipv4(MF)+tunnel}+[mac+vlan+ipv4+tcp]+payload1
	 *   2nd {mac+ipv4(OF)}+payload2
	 * The FOE_AI is determined by 5T according to ori's content of [].
	 * However, the packets may pass the 3T 24-prefix check due to outer header of {}.
	 */
#ifdef CONFIG_TUNNEL_FAST_PATH
	return 0;

#else /* CONFIG_TUNNEL_FAST_PATH */
	bool same = (((ntohl(egress_src_ip) >> 8) == (ntohl(egress_dst_ip) >> 8)) &&
		((entry->ipv4_hnapt.sip >> 8) == (entry->ipv4_hnapt.dip >> 8)) &&
		((ntohl(egress_src_ip) >> 8) == (entry->ipv4_hnapt.sip >> 8)));

	if (!same && debug_level >= 7) {
		pr_notice("%s, %x->%x => %x->%x\n",
		__func__, entry->ipv4_hnapt.sip, entry->ipv4_hnapt.dip,
		egress_src_ip, egress_dst_ip);
	}

	return same;
#endif /* CONFIG_TUNNEL_FAST_PATH */
}

bool is_tunnel_port(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	/* L2TP: UDP port: 1701
	 * IPSEC (ESP over UDP) port: 4500
	 * GRE: IP protocol: 47
	 */

	/* tunnel term is not allowed to bind */
	if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_HNAPT) {
		if (entry->ipv4_hnapt.dport >= L2TP_UDP_PORT_MIN &&
		    entry->ipv4_hnapt.dport <= L2TP_UDP_PORT_MAX) {
			if (debug_level >= 7)
				pr_notice("%s, port:%d is not allowed to HWNAT (IPv4 term) !!\n",
					__func__, entry->ipv4_hnapt.dport);
			return 1;
		}

		/* clat: 4to6 */
		if (ppe_parse_result->pkt_type == IPV4_DSLITE) {
			if (entry->ipv4_hnapt.sport == ESP_UDP_PORT &&
			    entry->ipv4_hnapt.dport == ESP_UDP_PORT) {
				if (debug_level >= 7)
					pr_notice("%s, (clat_4to6) port:%d is not allowed to HWNAT (IPv4 term) !!\n",
						__func__, entry->ipv4_hnapt.dport);
				return 1;
			}
		}

		/* VXLAN */
		if (entry->ipv4_hnapt.dport == VXLAN_UDP_PORT ||
		    entry->ipv4_hnapt.new_dport == VXLAN_UDP_PORT) {
			if (debug_level >= 7)
				pr_notice("%s, port:%d is not allowed to HWNAT (VXLAN) !!\n",
					__func__, entry->ipv4_hnapt.dport);
			return 1;
		}


	} else if (entry->ipv6_5t_route.bfib1.pkt_type == IPV6_5T_ROUTE) {
		if (entry->ipv6_5t_route.dport >= L2TP_UDP_PORT_MIN &&
		     entry->ipv6_5t_route.dport <= L2TP_UDP_PORT_MAX) {
			if (debug_level >= 7)
				pr_notice("%s, port:%d is not allowed to HWNAT (IPv6 term) !!\n",
					__func__, entry->ipv6_5t_route.dport);
			return 1;
		}

		/* clat: 6to4 */
		if (ppe_parse_result->pkt_type == IPV6_6RD) {
			if (entry->ipv6_5t_route.sport == ESP_UDP_PORT &&
			    entry->ipv6_5t_route.dport == ESP_UDP_PORT) {
				if (debug_level >= 7)
					pr_notice("%s, (clat_6to4) port:%d is not allowed to HWNAT (IPv6 term) !!\n",
						__func__, entry->ipv4_hnapt.dport);
				return 1;
			}
		}

		/* VXLAN */
		if (entry->ipv6_5t_route.dport == VXLAN_UDP_PORT) {
			if (debug_level >= 7)
				pr_notice("%s, port:%d is not allowed to HWNAT (VXLAN) !!\n",
					__func__, entry->ipv6_5t_route.dport);
			return 1;
		}
	}

	return 0;
}


bool ppe_set_v4_packet_type(struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result, struct iphdr *iph)
{

	/* 464xlat: 6to4 */
	if (entry->ipv4_hnapt.bfib1.pkt_type == IPV6_5T_ROUTE) {
		ppe_parse_result->pkt_type = IPV6_6RD;

		/* no data ack and payload: 2 (it's may be padding) */
		if (xlat_enable == 1 && ntohs(iph->tot_len) <= 42) {
			if (debug_level >= 7)
				pr_notice("%s, 464XLAT 6to4 (%d) is not allowed to HWNAT\n", __func__, ntohs(iph->tot_len));
			return 1;
		}

	} else
		ppe_parse_result->pkt_type = IPV4_HNAPT;
	return 0;
}

bool ppe_set_v6_packet_type(struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result, struct ipv6hdr *ip6h)
{

	/* 464xlat: 4to6 */
	if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_HNAPT) {
		ppe_parse_result->pkt_type = IPV4_DSLITE;

		/* no data ack and payload: 6 (it's may be padding) */
		if (xlat_enable == 1  && ntohs(ip6h->payload_len) <= 26) {
			if (debug_level >= 7)
				pr_notice("%s, 464XLAT 4to6 (%d) is not allowed to HWNAT\n", __func__, ntohs(ip6h->payload_len));
			return 1;
		}
	} else
		ppe_parse_result->pkt_type = IPV6_5T_ROUTE;
	return 0;
}

int32_t ppe_parse_layer_med(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{

	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;
	struct pptp_gre_header *gh = NULL;
	u8 ipv6_head_len = 0;

	memset(ppe_parse_result, 0, sizeof(*ppe_parse_result));
	//hwnat_memcpy(ppe_parse_result->dmac, eth->h_dest, ETH_ALEN);
	//hwnat_memcpy(ppe_parse_result->smac, eth->h_source, ETH_ALEN);
	ppe_parse_result->dmac[0] = 00;
	ppe_parse_result->dmac[1] = 00;
	ppe_parse_result->dmac[2] = 00;
	ppe_parse_result->dmac[3] = 01;
	ppe_parse_result->dmac[4] = 00;
	ppe_parse_result->dmac[5] = 00;

	ppe_parse_result->smac[0] = 00;
	ppe_parse_result->smac[1] = 00;
	ppe_parse_result->smac[2] = 00;
	ppe_parse_result->smac[3] = 01;
	ppe_parse_result->smac[4] = 00;
	ppe_parse_result->smac[5] = 00;


	/* we cannot speed up multicase packets because both wire and wireless PCs might join same multicast group. */
	ppe_parse_result->is_mcast = 0;
	ppe_parse_result->vlan_layer = 0;
	/* set layer2 start addr */

	//skb_set_mac_header(skb, 0);

	/* set layer3 start addr */


	skb_set_network_header(skb, 0);

	/* set layer4 start addr */

	iph = (struct iphdr *)skb_network_header(skb);
	memcpy(&ppe_parse_result->iph, iph, sizeof(struct iphdr));

	if (iph->version ==4) {

		if ((forbid_bind_ecn == DIR_BIDIRECTION || forbid_bind_ecn == DIR_UPLINK) &&
		    (iph->tos & 0x1) != 0) {

			if (debug_level >= 7)
				pr_notice("%s, tos:0x%x, go sw path. forbid_bind_ecn:%d\n", __func__, iph->tos, forbid_bind_ecn);
			return 1;
		}

		if (iph->protocol == IPPROTO_TCP) {
			if (debug_level >= 6)
				pr_notice("MD TX TCP!!!!!\n");
			skb_set_transport_header(skb, (iph->ihl * 4));
			th = (struct tcphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->th, th, sizeof(struct tcphdr));

			if (ppe_set_v4_packet_type(entry, ppe_parse_result, iph))
				return 1;

			/* L4 header checksum is wrong if bind */
			if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
				if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
					if (debug_level >= 7)
						pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
					return 1;
				}

		} else if (iph->protocol == IPPROTO_UDP) {
			if (debug_level >= 6)
				pr_notice("MD TX UDP!!!!!\n");
			skb_set_transport_header(skb, (iph->ihl * 4));
			uh = (struct udphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->uh, uh, sizeof(struct udphdr));

			if (ppe_set_v4_packet_type(entry, ppe_parse_result, iph))
				return 1;

			/* L4 header checksum is wrong if bind */
			if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
				if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
					if (debug_level >= 7)
						pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
					return 1;
				}

		} else if (iph->protocol == IPPROTO_GRE) {
			if (pptp_enable == 0)
				return 1;
			ppe_parse_result->pkt_type = IPV4_NAT;


			skb_set_transport_header(skb, (iph->ihl * 4));
			gh = (struct pptp_gre_header *)skb_transport_header(skb);

			ppe_parse_result->gre_call_id = gh->call_id;

			if (debug_level >= 7)
				pr_notice("%s, gre call id:%d\n", __func__,
					ppe_parse_result->gre_call_id);

		}
	} else {
		ip6h = (struct ipv6hdr *)skb_network_header(skb);

		if (ip6h-> version == 6) {

			if ((forbid_bind_ecn == DIR_BIDIRECTION || forbid_bind_ecn == DIR_UPLINK) &&
			    (ip6h->flow_lbl[0] & 0x10) != 0) {

				if (debug_level >= 7)
					pr_notice("%s, flow_lbl[0]:0x%x, go sw path. forbid_bind_ecn:%d\n", __func__, ip6h->flow_lbl[0], forbid_bind_ecn);
				return 1;
			}

			memcpy(&ppe_parse_result->ip6h, ip6h, sizeof(struct ipv6hdr));

			if (ip6h->nexthdr == NEXTHDR_TCP) {
				if (debug_level >= 6)
					pr_notice("ipv6 + TCP\n");
				skb_set_transport_header(skb, (sizeof(struct ipv6hdr)));
				th = (struct tcphdr *)skb_transport_header(skb);
				memcpy(&ppe_parse_result->th, th, sizeof(struct tcphdr));

				if (ppe_set_v6_packet_type(entry, ppe_parse_result, ip6h))
					return 1;

			} else if (ip6h->nexthdr == NEXTHDR_UDP) {
				if (debug_level >= 6)
					pr_notice("ipv6 + UDP\n");
				skb_set_transport_header(skb, (sizeof(struct ipv6hdr)));
				uh = (struct udphdr *)skb_transport_header(skb);
				memcpy(&ppe_parse_result->uh, uh, sizeof(struct udphdr));

				if (ppe_set_v6_packet_type(entry, ppe_parse_result, ip6h))
					return 1;

			} else if (ip6h->nexthdr == NEXTHDR_IPIP) {
				ipv6_head_len = sizeof(struct iphdr);
				memcpy(&ppe_parse_result->iph, ip6h + ipv6_head_len,
				       sizeof(struct iphdr));
				ppe_parse_result->pkt_type = IPV4_DSLITE;
			} else {
				ppe_parse_result->pkt_type = IPV6_3T_ROUTE;
			}
		}else {
			if (debug_level >= 6)
				pr_notice("Not support protocol = %x\n", ip6h-> version);
		}
	}

	if (debug_level >= 11) {
		pr_notice("--------------\n");
		pr_notice("DMAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			ppe_parse_result->dmac[0], ppe_parse_result->dmac[1],
			 ppe_parse_result->dmac[2], ppe_parse_result->dmac[3],
			 ppe_parse_result->dmac[4], ppe_parse_result->dmac[5]);
		pr_notice("SMAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			ppe_parse_result->smac[0], ppe_parse_result->smac[1],
			 ppe_parse_result->smac[2], ppe_parse_result->smac[3],
			 ppe_parse_result->smac[4], ppe_parse_result->smac[5]);
		pr_notice("Eth_Type=%x\n", ppe_parse_result->eth_type);
		if (ppe_parse_result->vlan1_gap > 0)
			pr_notice("VLAN1 ID=%x\n", ntohs(ppe_parse_result->vlan1));

		if (ppe_parse_result->vlan2_gap > 0)
			pr_notice("VLAN2 ID=%x\n", ntohs(ppe_parse_result->vlan2));

		if (ppe_parse_result->pppoe_gap > 0) {
			pr_notice("PPPOE Session ID=%x\n", ppe_parse_result->pppoe_sid);
			pr_notice("PPP Tag=%x\n", ntohs(ppe_parse_result->ppp_tag));
		}
		pr_notice("PKT_TYPE=%s\n",
			ppe_parse_result->pkt_type ==
			 0 ? "IPV4_HNAPT" : ppe_parse_result->pkt_type ==
			 1 ? "IPV4_HNAT" : ppe_parse_result->pkt_type ==
			 3 ? "IPV4_DSLITE" : ppe_parse_result->pkt_type ==
			 5 ? "IPV6_ROUTE" : ppe_parse_result->pkt_type == 7 ? "IPV6_6RD" : "Unknown");
		if (ppe_parse_result->pkt_type == IPV4_HNAT) {
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));
			pr_notice("TOS=%x\n", ntohs(ppe_parse_result->iph.tos));
		} else if (ppe_parse_result->pkt_type == IPV4_HNAPT) {
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));
			pr_notice("TOS=%x\n", ntohs(ppe_parse_result->iph.tos));

			if (ppe_parse_result->iph.protocol == IPPROTO_TCP) {
				pr_notice("TCP SPORT=%d\n", ntohs(ppe_parse_result->th.source));
				pr_notice("TCP DPORT=%d\n", ntohs(ppe_parse_result->th.dest));
			} else if (ppe_parse_result->iph.protocol == IPPROTO_UDP) {
				pr_notice("UDP SPORT=%d\n", ntohs(ppe_parse_result->uh.source));
				pr_notice("UDP DPORT=%d\n", ntohs(ppe_parse_result->uh.dest));
			}
		} else if (ppe_parse_result->pkt_type == IPV6_5T_ROUTE) {
			pr_notice("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X:%d-> %08X:%08X:%08X:%08X:%d\n",
				ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]),
			     ntohs(ppe_parse_result->th.source),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]),
			     ntohs(ppe_parse_result->th.dest));
		} else if (ppe_parse_result->pkt_type == IPV6_6RD) {
			/* fill in ipv4 6rd entry */
			pr_notice("packet_type = IPV6_6RD\n");
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));

			pr_notice("Checksum=%x\n", ntohs(ppe_parse_result->iph.check));
			pr_notice("ipV4 ID =%x\n", ntohs(ppe_parse_result->iph.id));
			pr_notice("Flag=%x\n", ntohs(ppe_parse_result->iph.frag_off) >> 13);
			pr_notice("TTL=%x\n", ppe_parse_result->iph.ttl);
			pr_notice("TOS=%x\n", ppe_parse_result->iph.tos);
		}
	}

	return 0;
}

int32_t ppe_parse_layer_info(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	struct vlan_hdr *vh = NULL;
	struct ethhdr *eth = NULL;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ip6h = NULL;
	struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;
	struct pptp_gre_header *gh = NULL;
	u8 ipv6_head_len = 0;
#ifdef	CONFIG_RAETH_HW_VLAN_TX
	struct vlan_hdr pseudo_vhdr;
#endif
	ppe_parse_result->vlan_layer = 0;
	ppe_parse_result->vlan_tag = 0;

	memset(ppe_parse_result, 0, sizeof(*ppe_parse_result));
	eth = (struct ethhdr *)skb->data;
	hwnat_memcpy(ppe_parse_result->dmac, eth->h_dest, ETH_ALEN);
	hwnat_memcpy(ppe_parse_result->smac, eth->h_source, ETH_ALEN);
	ppe_parse_result->eth_type = eth->h_proto;
	/* we cannot speed up multicase packets because both wire and wireless PCs might join same multicast group. */
	if (fe_feature & HNAT_MCAST) {
		if (is_multicast_ether_addr(&eth->h_dest[0]))
			ppe_parse_result->is_mcast = 1;
		else
			ppe_parse_result->is_mcast = 0;
	} else {
		if (is_multicast_ether_addr(&eth->h_dest[0])) {
			if (debug_level >= 6)
				DD;
			return 1;
		}
	}

	if (is8021Q(ppe_parse_result->eth_type, ppe_parse_result) ||
	    is_special_tag(ppe_parse_result->eth_type, ppe_parse_result) ||
	    is_hw_vlan_tx(skb, ppe_parse_result) ||
	    is_dsa(skb, ppe_parse_result->eth_type, ppe_parse_result)) {

#ifdef CONFIG_RAETH_HW_VLAN_TX
			ppe_parse_result->vlan1_gap = 0;
			ppe_parse_result->vlan_layer++;
			pseudo_vhdr.h_vlan_TCI = htons(skb_vlan_tag_get(skb));
			pseudo_vhdr.h_vlan_encapsulated_proto = eth->h_proto;
			vh = (struct vlan_hdr *)&pseudo_vhdr;
#else
			ppe_parse_result->vlan1_gap = VLAN_HLEN;
			ppe_parse_result->vlan_layer++;
			vh = (struct vlan_hdr *)(skb->data + ETH_HLEN);
#endif

		if (debug_level >= 6)
			pr_notice("%s, htons(prot): %x, htons(8021Q): %x\n", __func__,
				ntohs(vh->h_vlan_encapsulated_proto), htons(ETH_P_8021Q));

		ppe_parse_result->vlan1 = vh->h_vlan_TCI;
		/* VLAN + PPPoE */
		if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
			ppe_parse_result->pppoe_gap = 8;
			if (get_pppoe_sid(skb, ppe_parse_result->vlan1_gap,
					  &ppe_parse_result->pppoe_sid,
					  &ppe_parse_result->ppp_tag)) {
				if (debug_level >= 6)
					DD;
				return 1;
			}
			ppe_parse_result->eth_type = vh->h_vlan_encapsulated_proto;
			/* Double VLAN = VLAN + VLAN */
		} else if (is8021Q(vh->h_vlan_encapsulated_proto, ppe_parse_result) ||
			   is_special_tag(vh->h_vlan_encapsulated_proto, ppe_parse_result)) {
			ppe_parse_result->vlan2_gap = VLAN_HLEN;
			ppe_parse_result->vlan_layer++;
			vh = (struct vlan_hdr *)(skb->data + ETH_HLEN + ppe_parse_result->vlan1_gap);
			ppe_parse_result->vlan2 = vh->h_vlan_TCI;

			/* VLAN + VLAN + PPPoE */
			if (ntohs(vh->h_vlan_encapsulated_proto) == ETH_P_PPP_SES) {
				ppe_parse_result->pppoe_gap = 8;
				if (get_pppoe_sid
				    (skb,
				     (ppe_parse_result->vlan1_gap + ppe_parse_result->vlan2_gap),
				     &ppe_parse_result->pppoe_sid, &ppe_parse_result->ppp_tag)) {
					if (debug_level >= 6)
						DD;
					return 1;
				}
				ppe_parse_result->eth_type = vh->h_vlan_encapsulated_proto;
			} else if (is8021Q(vh->h_vlan_encapsulated_proto, ppe_parse_result)) {
				/* VLAN + VLAN + VLAN */
				ppe_parse_result->vlan_layer++;
				vh = (struct vlan_hdr *)(skb->data + ETH_HLEN +
							 ppe_parse_result->vlan1_gap + VLAN_HLEN);

				/* VLAN + VLAN + VLAN */
				if (is8021Q(vh->h_vlan_encapsulated_proto, ppe_parse_result))
					ppe_parse_result->vlan_layer++;
			} else {
				/* VLAN + VLAN + IP */
				ppe_parse_result->eth_type = vh->h_vlan_encapsulated_proto;
			}
		} else {
			/* VLAN + IP */
			ppe_parse_result->eth_type = vh->h_vlan_encapsulated_proto;

			if (debug_level >= 6)
				pr_notice("%s, vlan_tag: %d, eth_type: %d\n", __func__,
					ppe_parse_result->vlan_tag, ppe_parse_result->eth_type);
		}
	} else if (ntohs(ppe_parse_result->eth_type) == ETH_P_PPP_SES) {
		/* PPPoE + IP */
		ppe_parse_result->pppoe_gap = 8;
		if (get_pppoe_sid(skb, ppe_parse_result->vlan1_gap,
				  &ppe_parse_result->pppoe_sid,
				  &ppe_parse_result->ppp_tag)) {
			if (debug_level >= 6)
				DD;
			return 1;
		}
	}

	/* set layer2 start addr */
	skb_set_mac_header(skb, 0);

	/* set layer3 start addr */
	skb_set_network_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
			       ppe_parse_result->vlan2_gap + ppe_parse_result->pppoe_gap);

	/* set layer4 start addr */
	if ((ppe_parse_result->eth_type == htons(ETH_P_IP)) ||
	    (ppe_parse_result->eth_type == htons(ETH_P_PPP_SES) &&
	    (ppe_parse_result->ppp_tag == htons(PPP_IP)))) {
		iph = (struct iphdr *)skb_network_header(skb);
		memcpy(&ppe_parse_result->iph, iph, sizeof(struct iphdr));

		if ((forbid_bind_ecn == DIR_BIDIRECTION || forbid_bind_ecn == DIR_DOWNLINK) &&
		    (iph->tos & 0x1) != 0) {
			if (debug_level >= 7)
				pr_notice("%s, tos:0x%x, go sw path. forbid_bind_ecn:%d\n", __func__, iph->tos, forbid_bind_ecn);
			return 1;
		}

		if (iph->protocol == IPPROTO_TCP) {
			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap + (iph->ihl * 4));
			th = (struct tcphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->th, th, sizeof(struct tcphdr));

			if (ppe_set_v4_packet_type(entry, ppe_parse_result, iph))
				return 1;

			/* L4 header checksum is wrong if bind */
			if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
				if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
					if (debug_level >= 7)
						pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
					return 1;
				}

		} else if (iph->protocol == IPPROTO_UDP) {
			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap + (iph->ihl * 4));
			uh = (struct udphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->uh, uh, sizeof(struct udphdr));

			if (ppe_set_v4_packet_type(entry, ppe_parse_result, iph))
				return 1;

			/* L4 header checksum is wrong if bind */
			if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
				if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
					if (debug_level >= 7)
						pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
					return 1;
				}

		} else if (iph->protocol == IPPROTO_GRE) {
			if (pptp_enable == 0)
				return 1;
			ppe_parse_result->pkt_type = IPV4_NAT;

			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap + (iph->ihl * 4));
			gh = (struct pptp_gre_header *)skb_transport_header(skb);

			ppe_parse_result->gre_call_id = gh->call_id;

			if (debug_level >= 7)
				pr_notice("%s, gre call id:%04x\n", __func__,
					ppe_parse_result->gre_call_id);
		}
		if (fe_feature & HNAT_IPV6) {
			if (iph->protocol == IPPROTO_IPV6) {
				ip6h = (struct ipv6hdr *)((uint8_t *)iph + iph->ihl * 4);
				memcpy(&ppe_parse_result->ip6h, ip6h, sizeof(struct ipv6hdr));

				if (ip6h->nexthdr == NEXTHDR_TCP) {
					skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
								 ppe_parse_result->vlan2_gap +
								 ppe_parse_result->pppoe_gap +
								 (sizeof(struct ipv6hdr)));

					th = (struct tcphdr *)skb_transport_header(skb);

					memcpy(&ppe_parse_result->th.source, &th->source, sizeof(th->source));
					memcpy(&ppe_parse_result->th.dest, &th->dest, sizeof(th->dest));
				} else if (ip6h->nexthdr == NEXTHDR_UDP) {
					skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
								 ppe_parse_result->vlan2_gap +
								 ppe_parse_result->pppoe_gap +
								 (sizeof(struct ipv6hdr)));

					uh = (struct udphdr *)skb_transport_header(skb);
					memcpy(&ppe_parse_result->uh.source, &uh->source, sizeof(uh->source));
					memcpy(&ppe_parse_result->uh.dest, &uh->dest, sizeof(uh->dest));
				}
				ppe_parse_result->pkt_type = IPV6_6RD;

	/* identification field in outer ipv4 header is zero*/
	/*after erntering binding state.*/
	/* some 6rd relay router will drop the packet */
			}
		}
		if ((iph->protocol != IPPROTO_TCP) && (iph->protocol != IPPROTO_UDP) &&
		    (iph->protocol != IPPROTO_GRE) && (iph->protocol != IPPROTO_IPV6)) {
			if (debug_level >= 6)
				DD;
			return 1;
		}
	/* Packet format is not supported */
	} else if (ppe_parse_result->eth_type == htons(ETH_P_IPV6) ||
		   (ppe_parse_result->eth_type == htons(ETH_P_PPP_SES) &&
		    ppe_parse_result->ppp_tag == htons(PPP_IPV6))) {
		ip6h = (struct ipv6hdr *)skb_network_header(skb);
		memcpy(&ppe_parse_result->ip6h, ip6h, sizeof(struct ipv6hdr));

		if ((forbid_bind_ecn == DIR_BIDIRECTION || forbid_bind_ecn == DIR_DOWNLINK) &&
		    (ip6h->flow_lbl[0] & 0x10) != 0) {
			if (debug_level >= 7)
				pr_notice("%s, flow_lbl[0]:0x%x, go sw path. forbid_bind_ecn:%d\n", __func__, ip6h->flow_lbl[0], forbid_bind_ecn);
			return 1;
		}

		if (ip6h->nexthdr == NEXTHDR_TCP) {
			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap +
						 (sizeof(struct ipv6hdr)));

			th = (struct tcphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->th, th, sizeof(struct tcphdr));

			if (ppe_set_v6_packet_type(entry, ppe_parse_result, ip6h))
				return 1;

		} else if (ip6h->nexthdr == NEXTHDR_UDP) {
			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap +
						 (sizeof(struct ipv6hdr)));
			uh = (struct udphdr *)skb_transport_header(skb);
			memcpy(&ppe_parse_result->uh, uh, sizeof(struct udphdr));

			if (ppe_set_v6_packet_type(entry, ppe_parse_result, ip6h))
				return 1;

		} else if (ip6h->nexthdr == NEXTHDR_IPIP) {

			skb_set_transport_header(skb, ETH_HLEN + ppe_parse_result->vlan1_gap +
						 ppe_parse_result->vlan2_gap +
						 ppe_parse_result->pppoe_gap +
						 (sizeof(struct ipv6hdr)) +
						 sizeof(struct iphdr));
			ipv6_head_len = sizeof(struct iphdr);
			memcpy(&ppe_parse_result->iph, ip6h + ipv6_head_len,
			       sizeof(struct iphdr));

			if(SwitchDslMape == 1) {
				if (ppe_parse_result->iph.protocol == IPPROTO_TCP) {
					th = (struct tcphdr *)skb_transport_header(skb);
					memcpy(&ppe_parse_result->th, th, sizeof(struct tcphdr));

					iph = (struct iphdr *)&ppe_parse_result->iph;
					/* L4 header checksum is wrong if bind */
					if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
						if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
							if (debug_level >= 7)
								pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
							return 1;
						}

				} else if (ppe_parse_result->iph.protocol == IPPROTO_UDP) {
					uh = (struct udphdr *)skb_transport_header(skb);
					memcpy(&ppe_parse_result->uh, uh, sizeof(struct udphdr));

					iph = (struct iphdr *)&ppe_parse_result->iph;
					/* L4 header checksum is wrong if bind */
					if (iph != NULL && (iph->frag_off & htons(IP_MF | IP_OFFSET)))
						if (!is_same_subnet(entry, iph->saddr, iph->daddr)){
							if (debug_level >= 7)
								pr_notice("[%s][%d], 3T link on different subnet is not allowed to HWNAT !!\n", __func__, __LINE__);
							return 1;
						}

				}
				ppe_parse_result->pkt_type = IPV4_MAP_E;
			} else {
				ppe_parse_result->pkt_type = IPV4_DSLITE;
			}

		} else {
			ppe_parse_result->pkt_type = IPV6_3T_ROUTE;
		}

	} else {
		if (debug_level >= 6)
			DD;
		return 1;
	}

	if (debug_level >= 11) {
		pr_notice("--------------\n");
		pr_notice("DMAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			ppe_parse_result->dmac[0], ppe_parse_result->dmac[1],
			 ppe_parse_result->dmac[2], ppe_parse_result->dmac[3],
			 ppe_parse_result->dmac[4], ppe_parse_result->dmac[5]);
		pr_notice("SMAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			ppe_parse_result->smac[0], ppe_parse_result->smac[1],
			 ppe_parse_result->smac[2], ppe_parse_result->smac[3],
			 ppe_parse_result->smac[4], ppe_parse_result->smac[5]);
		pr_notice("Eth_Type=%x\n", ppe_parse_result->eth_type);
		if (ppe_parse_result->vlan1_gap > 0)
			pr_notice("VLAN1 ID=%x\n", ntohs(ppe_parse_result->vlan1));

		if (ppe_parse_result->vlan2_gap > 0)
			pr_notice("VLAN2 ID=%x\n", ntohs(ppe_parse_result->vlan2));

		if (ppe_parse_result->pppoe_gap > 0) {
			pr_notice("PPPOE Session ID=%x\n", ppe_parse_result->pppoe_sid);
			pr_notice("PPP Tag=%x\n", ntohs(ppe_parse_result->ppp_tag));
		}
		pr_notice("PKT_TYPE=%s\n",
			ppe_parse_result->pkt_type ==
			 0 ? "IPV4_HNAPT" : ppe_parse_result->pkt_type ==
			 1 ? "IPV4_HNAT" : ppe_parse_result->pkt_type ==
			 3 ? "IPV4_DSLITE" : ppe_parse_result->pkt_type ==
			 5 ? "IPV6_ROUTE" : ppe_parse_result->pkt_type == 7 ? "IPV6_6RD" : "Unknown");
		if (ppe_parse_result->pkt_type == IPV4_HNAT) {
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));
			pr_notice("TOS=%x\n", ntohs(ppe_parse_result->iph.tos));
		} else if (ppe_parse_result->pkt_type == IPV4_HNAPT) {
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));
			pr_notice("TOS=%x\n", ntohs(ppe_parse_result->iph.tos));

			if (ppe_parse_result->iph.protocol == IPPROTO_TCP) {
				pr_notice("TCP SPORT=%d\n", ntohs(ppe_parse_result->th.source));
				pr_notice("TCP DPORT=%d\n", ntohs(ppe_parse_result->th.dest));
			} else if (ppe_parse_result->iph.protocol == IPPROTO_UDP) {
				pr_notice("UDP SPORT=%d\n", ntohs(ppe_parse_result->uh.source));
				pr_notice("UDP DPORT=%d\n", ntohs(ppe_parse_result->uh.dest));
			}
		} else if (ppe_parse_result->pkt_type == IPV6_5T_ROUTE) {
			pr_notice("ING SIPv6->DIPv6: %08X:%08X:%08X:%08X:%d-> %08X:%08X:%08X:%08X:%d\n",
				ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]),
			     ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]),
			     ntohs(ppe_parse_result->th.source),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]),
			     ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]),
			     ntohs(ppe_parse_result->th.dest));
		} else if (ppe_parse_result->pkt_type == IPV6_6RD) {
			/* fill in ipv4 6rd entry */
			pr_notice("packet_type = IPV6_6RD\n");
			pr_notice("SIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.saddr)));
			pr_notice("DIP=%s\n", ip_to_str(ntohl(ppe_parse_result->iph.daddr)));

			pr_notice("Checksum=%x\n", ntohs(ppe_parse_result->iph.check));
			pr_notice("ipV4 ID =%x\n", ntohs(ppe_parse_result->iph.id));
			pr_notice("Flag=%x\n", ntohs(ppe_parse_result->iph.frag_off) >> 13);
			pr_notice("TTL=%x\n", ppe_parse_result->iph.ttl);
			pr_notice("TOS=%x\n", ppe_parse_result->iph.tos);
		}
	}

	return 0;
}

int32_t ppe_fill_L2_info(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	/* if this entry is already in binding state, skip it */
	if (entry->bfib1.state == BIND) {
		if (debug_level >= 6)
			DD;
		return 1;
	}

	/* Set VLAN Info - VLAN1/VLAN2 */
	/* Set Layer2 Info - DMAC, SMAC */
	if ((ppe_parse_result->pkt_type == IPV4_HNAT) || (ppe_parse_result->pkt_type == IPV4_HNAPT)) {
		if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_DSLITE ||
		    entry->ipv4_hnapt.bfib1.pkt_type == IPV4_MAP_E) {/* DS-Lite WAN->LAN */
			if (fe_feature & HNAT_IPV6) {
				foe_set_mac_hi_info(entry->ipv4_dslite.dmac_hi, ppe_parse_result->dmac);
				foe_set_mac_lo_info(entry->ipv4_dslite.dmac_lo, ppe_parse_result->dmac);
				foe_set_mac_hi_info(entry->ipv4_dslite.smac_hi, ppe_parse_result->smac);
				foe_set_mac_lo_info(entry->ipv4_dslite.smac_lo, ppe_parse_result->smac);
				entry->ipv4_dslite.vlan1 = ntohs(ppe_parse_result->vlan1);
				entry->ipv4_dslite.pppoe_id = ntohs(ppe_parse_result->pppoe_sid);
				entry->ipv4_dslite.vlan2_winfo = ntohs(ppe_parse_result->vlan2);

				entry->ipv4_dslite.etype = ntohs(ppe_parse_result->vlan_tag);
			} else {
				if (debug_level >= 6)
					DD;
				return 1;
			}

		} else {	/* IPv4 WAN<->LAN */
			foe_set_mac_hi_info(entry->ipv4_hnapt.dmac_hi, ppe_parse_result->dmac);
			foe_set_mac_lo_info(entry->ipv4_hnapt.dmac_lo, ppe_parse_result->dmac);
			foe_set_mac_hi_info(entry->ipv4_hnapt.smac_hi, ppe_parse_result->smac);
			foe_set_mac_lo_info(entry->ipv4_hnapt.smac_lo, ppe_parse_result->smac);
			entry->ipv4_hnapt.vlan1 = ntohs(ppe_parse_result->vlan1);
#ifdef VPRI_REMARK_TEST
			/* VPRI=0x7 */
			entry->ipv4_hnapt.vlan1 |= (7 << 13);
#endif
			entry->ipv4_hnapt.pppoe_id = ntohs(ppe_parse_result->pppoe_sid);
			entry->ipv4_hnapt.vlan2_winfo = ntohs(ppe_parse_result->vlan2);

			entry->ipv4_hnapt.etype = ntohs(ppe_parse_result->vlan_tag);
		}
	} else {
		if (fe_feature & HNAT_IPV6) {
			foe_set_mac_hi_info(entry->ipv6_5t_route.dmac_hi, ppe_parse_result->dmac);
			foe_set_mac_lo_info(entry->ipv6_5t_route.dmac_lo, ppe_parse_result->dmac);
			foe_set_mac_hi_info(entry->ipv6_5t_route.smac_hi, ppe_parse_result->smac);
			foe_set_mac_lo_info(entry->ipv6_5t_route.smac_lo, ppe_parse_result->smac);
			entry->ipv6_5t_route.vlan1 = ntohs(ppe_parse_result->vlan1);
			entry->ipv6_5t_route.pppoe_id = ntohs(ppe_parse_result->pppoe_sid);
			entry->ipv6_5t_route.vlan2_winfo = ntohs(ppe_parse_result->vlan2);

			entry->ipv6_5t_route.etype = ntohs(ppe_parse_result->vlan_tag);
		} else {
				if (debug_level >= 6)
					DD;
				return 1;
		}
	}

/* VLAN Layer:*/
/* 0: outgoing packet is untagged packet*/
/* 1: outgoing packet is tagged packet*/
/* 2: outgoing packet is double tagged packet*/
/* 3: outgoing packet is triple tagged packet*/
/* 4: outgoing packet is fourfold tagged packet*/
	entry->bfib1.vlan_layer = ppe_parse_result->vlan_layer;

#ifdef VLAN_LAYER_TEST
	/* outgoing packet is triple tagged packet */
	entry->bfib1.vlan_layer = 3;
	entry->ipv4_hnapt.vlan1 = 2;
	entry->ipv4_hnapt.vlan2 = 1;
#endif
	if (ppe_parse_result->pppoe_gap)
		entry->bfib1.psn = 1;
	else
		entry->bfib1.psn = 0;

	/* configure PPE to fill vlan tag */
	if (ppe_parse_result->vlan_tag == htons(ETH_P_8021Q))
		/* 0x8100 */
		entry->ipv4_hnapt.bfib1.vpm = 1;
	else {
		/* DSA: special tag */
		entry->ipv4_hnapt.bfib1.vpm = 0;

		if (IS_IPV4_GRP(entry))
			entry->ipv4_hnapt.etype = ppe_parse_result->vlan_tag;

		else if (IS_IPV6_GRP(entry))
			entry->ipv6_5t_route.etype = ppe_parse_result->vlan_tag;
	}

	if (debug_level >= 6)
		pr_info("%s, vpm:%d, etype:%d\n", __func__, entry->ipv4_hnapt.bfib1.vpm, ppe_parse_result->vlan_tag);

	return 0;
}


static uint16_t ppe_get_chkbase(struct iphdr *iph)
{
	u16 org_chksum = ntohs(iph->check);
	u16 org_tot_len = ntohs(iph->tot_len);
	u16 org_id = ntohs(iph->id);
	u16 chksum_tmp, tot_len_tmp, id_tmp;
	u32 tmp = 0;
	u16 chksum_base = 0;

	chksum_tmp = ~(org_chksum);
	tot_len_tmp = ~(org_tot_len);
	id_tmp = ~(org_id);
	tmp = chksum_tmp + tot_len_tmp + id_tmp;
	tmp = ((tmp >> 16) & 0x7) + (tmp & 0xFFFF);
	tmp = ((tmp >> 16) & 0x7) + (tmp & 0xFFFF);
	chksum_base = tmp & 0xFFFF;

	return chksum_base;
}


int32_t ppe_fill_L3_info_med(struct sk_buff *skb, struct foe_entry *entry,
			 struct pkt_parse_result *ppe_parse_result)
{
	/* IPv4 or IPv4 over PPPoE */

	if ((ppe_parse_result->pkt_type == IPV4_HNAT) ||
	    (ppe_parse_result->pkt_type == IPV4_HNAPT)) {
			if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_DSLITE ||
			    entry->ipv4_hnapt.bfib1.pkt_type == IPV4_MAP_E) {/* DS-Lite WAN->LAN */
			if (fe_feature & HNAT_IPV6) {
				if (fe_feature & PPE_MIB) {
					entry->ipv4_dslite.iblk2.mibf = 1;
				}
				entry->ipv4_dslite.bfib1.rmt = 1;	/* remove outer IPv6 header */
				entry->ipv4_dslite.iblk2.dscp = ppe_parse_result->iph.tos;
			}

		} else {
			entry->ipv4_hnapt.new_sip = ntohl(ppe_parse_result->iph.saddr);
			entry->ipv4_hnapt.new_dip = ntohl(ppe_parse_result->iph.daddr);
			entry->ipv4_hnapt.iblk2.dscp = ppe_parse_result->iph.tos;
			if (fe_feature & PPE_MIB)
				entry->ipv4_hnapt.iblk2.mibf = 1;
		}
	}

		if (ppe_parse_result->pkt_type == IPV6_6RD) {
				/* fill in ipv4 6rd entry */
			entry->ipv6_6rd.tunnel_sipv4 = ntohl(ppe_parse_result->iph.saddr);
			entry->ipv6_6rd.tunnel_dipv4 = ntohl(ppe_parse_result->iph.daddr);
			entry->ipv6_6rd.hdr_chksum = ppe_get_chkbase(&ppe_parse_result->iph);
			entry->ipv6_6rd.flag = (ntohs(ppe_parse_result->iph.frag_off) >> 13);
			entry->ipv6_6rd.ttl = ppe_parse_result->iph.ttl;
			entry->ipv6_6rd.dscp = ppe_parse_result->iph.tos;
			if (fe_feature & PPE_MIB)
				entry->ipv6_6rd.iblk2.mibf = 1;

			hwnat_set_6rd_id(entry, ppe_parse_result);
				/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
			entry->bfib1.pkt_type = IPV6_6RD;
			entry->bfib1.rmt = 0;
		}
		/* IPv6 or IPv6 over PPPoE */
		if (ppe_parse_result->pkt_type == IPV6_3T_ROUTE ||
		    ppe_parse_result->pkt_type == IPV6_5T_ROUTE) {
				/* incoming packet is 6RD and need to remove outer IPv4 header */
			if (entry->bfib1.pkt_type == IPV6_6RD) {
				entry->ipv6_3t_route.bfib1.rmt = 1;
				entry->ipv6_3t_route.iblk2.dscp =
					(ppe_parse_result->ip6h.
					priority << 4 | (ppe_parse_result->ip6h.flow_lbl[0] >> 4));
				if (fe_feature & PPE_MIB)
					entry->ipv6_3t_route.iblk2.mibf = 1;

			} else {
				/* fill in ipv6 routing entry */
				entry->ipv6_3t_route.ipv6_sip0 =
					ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]);
				entry->ipv6_3t_route.ipv6_sip1 =
					ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]);
				entry->ipv6_3t_route.ipv6_sip2 =
					ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]);
				entry->ipv6_3t_route.ipv6_sip3 =
					ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]);
				entry->ipv6_3t_route.ipv6_dip0 =
					ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]);
				entry->ipv6_3t_route.ipv6_dip1 =
					ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]);
				entry->ipv6_3t_route.ipv6_dip2 =
					ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]);
				entry->ipv6_3t_route.ipv6_dip3 =
					ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]);
				entry->ipv6_3t_route.iblk2.dscp = (ppe_parse_result->ip6h.
					priority << 4 | (ppe_parse_result->ip6h.flow_lbl[0] >> 4));
				if (fe_feature & PPE_MIB)
					entry->ipv6_3t_route.iblk2.mibf = 1;
			}
		} else if (ppe_parse_result->pkt_type == IPV4_DSLITE) {
				/* fill in DSLite entry */
				entry->ipv4_dslite.tunnel_sipv6_0 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]);
				entry->ipv4_dslite.tunnel_sipv6_1 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]);
				entry->ipv4_dslite.tunnel_sipv6_2 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]);
				entry->ipv4_dslite.tunnel_sipv6_3 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]);

				entry->ipv4_dslite.tunnel_dipv6_0 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]);
				entry->ipv4_dslite.tunnel_dipv6_1 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]);
				entry->ipv4_dslite.tunnel_dipv6_2 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]);
				entry->ipv4_dslite.tunnel_dipv6_3 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]);
				if (fe_feature & PPE_MIB)
					entry->ipv4_dslite.iblk2.mibf = 1;

				memcpy(entry->ipv4_dslite.flow_lbl, ppe_parse_result->ip6h.flow_lbl,
				       sizeof(ppe_parse_result->ip6h.flow_lbl));
				entry->ipv4_dslite.priority = ppe_parse_result->ip6h.priority;
				entry->ipv4_dslite.hop_limit = ppe_parse_result->ip6h.hop_limit;
				/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
				entry->bfib1.pkt_type = IPV4_DSLITE;
				entry->bfib1.rmt = 0;
		};

	return 0;
}


int32_t ppe_fill_L3_info(struct sk_buff *skb, struct foe_entry *entry,
			 struct pkt_parse_result *ppe_parse_result)
{
	/* IPv4 or IPv4 over PPPoE */
	if ((ppe_parse_result->eth_type == htons(ETH_P_IP)) ||
	    (ppe_parse_result->eth_type == htons(ETH_P_PPP_SES) &&
	     ppe_parse_result->ppp_tag == htons(PPP_IP))) {
		if ((ppe_parse_result->pkt_type == IPV4_HNAT) ||
		    (ppe_parse_result->pkt_type == IPV4_HNAPT)) {
			if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_DSLITE ||
			    entry->ipv4_hnapt.bfib1.pkt_type == IPV4_MAP_E) {/* DS-Lite WAN->LAN */
				if (fe_feature & HNAT_IPV6) {
					if (fe_feature & PPE_MIB) {
						entry->ipv4_dslite.iblk2.mibf = 1;
					}
					entry->ipv4_dslite.bfib1.rmt = 1;	/* remove outer IPv6 header */
					entry->ipv4_dslite.iblk2.dscp = ppe_parse_result->iph.tos;
				}

			} else {

				entry->ipv4_hnapt.new_sip = ntohl(ppe_parse_result->iph.saddr);
				entry->ipv4_hnapt.new_dip = ntohl(ppe_parse_result->iph.daddr);
				entry->ipv4_hnapt.iblk2.dscp = ppe_parse_result->iph.tos;
#ifdef DSCP_REMARK_TEST
				entry->ipv4_hnapt.iblk2.dscp = 0xff;
#endif
				if (fe_feature & PPE_MIB)
					entry->ipv4_hnapt.iblk2.mibf = 1;
			}
		}

		if (ppe_parse_result->pkt_type == IPV6_6RD) {
			/* fill in ipv4 6rd entry */
			entry->ipv6_6rd.tunnel_sipv4 = ntohl(ppe_parse_result->iph.saddr);
			entry->ipv6_6rd.tunnel_dipv4 = ntohl(ppe_parse_result->iph.daddr);
			entry->ipv6_6rd.hdr_chksum = ppe_get_chkbase(&ppe_parse_result->iph);
			entry->ipv6_6rd.flag = (ntohs(ppe_parse_result->iph.frag_off) >> 13);
			entry->ipv6_6rd.ttl = ppe_parse_result->iph.ttl;
			entry->ipv6_6rd.dscp = ppe_parse_result->iph.tos;
			if (fe_feature & PPE_MIB) {
				entry->ipv6_6rd.iblk2.mibf = 1;

			}
			hwnat_set_6rd_id(entry, ppe_parse_result);
			/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
			entry->bfib1.pkt_type = IPV6_6RD;
			entry->bfib1.rmt = 0;

		}
	}

	/* IPv6 or IPv6 over PPPoE */
	if (ppe_parse_result->eth_type == htons(ETH_P_IPV6) ||
	    (ppe_parse_result->eth_type == htons(ETH_P_PPP_SES) &&
		  ppe_parse_result->ppp_tag == htons(PPP_IPV6))) {
		if (ppe_parse_result->pkt_type == IPV6_3T_ROUTE ||
		    ppe_parse_result->pkt_type == IPV6_5T_ROUTE) {
			/* incoming packet is 6RD and need to remove outer IPv4 header */
			if (entry->bfib1.pkt_type == IPV6_6RD) {
				entry->ipv6_3t_route.bfib1.rmt = 1;
				entry->ipv6_3t_route.iblk2.dscp =
				    (ppe_parse_result->ip6h.
				     priority << 4 | (ppe_parse_result->ip6h.flow_lbl[0] >> 4));
				if (fe_feature & PPE_MIB)
					entry->ipv6_3t_route.iblk2.mibf = 1;
			} else {
				/* fill in ipv6 routing entry */
				entry->ipv6_3t_route.ipv6_sip0 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]);
				entry->ipv6_3t_route.ipv6_sip1 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]);
				entry->ipv6_3t_route.ipv6_sip2 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]);
				entry->ipv6_3t_route.ipv6_sip3 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]);

					entry->ipv6_3t_route.ipv6_dip0 =
					    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]);
					entry->ipv6_3t_route.ipv6_dip1 =
					    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]);
					entry->ipv6_3t_route.ipv6_dip2 =
					    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]);
					entry->ipv6_3t_route.ipv6_dip3 =
					    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]);
					entry->ipv6_3t_route.iblk2.dscp =
					    (ppe_parse_result->ip6h.
					     priority << 4 | (ppe_parse_result->ip6h.flow_lbl[0] >> 4));

	/*#ifdef DSCP_REMARK_TEST*/
	/*				entry->ipv6_3t_route.iblk2.dscp = 0xff;*/
	/*#endif*/

					if (fe_feature & PPE_MIB)
						entry->ipv6_3t_route.iblk2.mibf = 1;
				}
		} else if (ppe_parse_result->pkt_type == IPV4_DSLITE ||
			   ppe_parse_result->pkt_type == IPV4_MAP_E) {
				/* fill in DSLite entry */
				entry->ipv4_dslite.tunnel_sipv6_0 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[0]);
				entry->ipv4_dslite.tunnel_sipv6_1 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[1]);
				entry->ipv4_dslite.tunnel_sipv6_2 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[2]);
				entry->ipv4_dslite.tunnel_sipv6_3 =
				    ntohl(ppe_parse_result->ip6h.saddr.s6_addr32[3]);

				entry->ipv4_dslite.tunnel_dipv6_0 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[0]);
				entry->ipv4_dslite.tunnel_dipv6_1 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[1]);
				entry->ipv4_dslite.tunnel_dipv6_2 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[2]);
				entry->ipv4_dslite.tunnel_dipv6_3 =
				    ntohl(ppe_parse_result->ip6h.daddr.s6_addr32[3]);
				if (fe_feature & PPE_MIB)
					entry->ipv4_dslite.iblk2.mibf = 1;

				memcpy(entry->ipv4_dslite.flow_lbl, ppe_parse_result->ip6h.flow_lbl,
				       sizeof(ppe_parse_result->ip6h.flow_lbl));
				entry->ipv4_dslite.priority = ppe_parse_result->ip6h.priority;
				entry->ipv4_dslite.hop_limit = ppe_parse_result->ip6h.hop_limit;
			if(SwitchDslMape == 1) {
				entry->ipv4_dslite.new_sip = ntohl(ppe_parse_result->iph.saddr);
				entry->ipv4_dslite.new_dip = ntohl(ppe_parse_result->iph.daddr);
				entry->bfib1.pkt_type = IPV4_MAP_E;
			} else {
				/* IPv4 DS-Lite and IPv6 6RD shall be turn on by SW during initialization */
				entry->bfib1.pkt_type = IPV4_DSLITE;
				entry->bfib1.rmt = 0;

			}


			};
		}
	if ((!IS_IPV4_GRP(entry)) && (!(IS_IPV6_GRP(entry)))) {
		NAT_PRINT("unknown Pkt_type=%d\n", entry->bfib1.pkt_type);
		return 1;
	}

	return 0;
}

int32_t ppe_fill_L4_info(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	if (ppe_parse_result->pkt_type == IPV4_HNAPT) {
		/* DS-LIte WAN->LAN */
		if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_DSLITE)
			return 0;
		if (entry->ipv4_hnapt.bfib1.pkt_type == IPV4_MAP_E) {
		/* Set Layer4 Info - NEW_SPORT, NEW_DPORT */
			if (ppe_parse_result->iph.protocol == IPPROTO_TCP) {
				entry->ipv4_dslite.new_sport = ntohs(ppe_parse_result->th.source);
				entry->ipv4_dslite.new_dport = ntohs(ppe_parse_result->th.dest);
				entry->ipv4_dslite.bfib1.udp = TCP;
			} else if (ppe_parse_result->iph.protocol == IPPROTO_UDP) {
				entry->ipv4_dslite.new_sport = ntohs(ppe_parse_result->uh.source);
				entry->ipv4_dslite.new_dport = ntohs(ppe_parse_result->uh.dest);
				entry->ipv4_dslite.bfib1.udp = UDP;
			}
		}
		/* Set Layer4 Info - NEW_SPORT, NEW_DPORT */
		if (ppe_parse_result->iph.protocol == IPPROTO_TCP) {
			entry->ipv4_hnapt.new_sport = ntohs(ppe_parse_result->th.source);
			entry->ipv4_hnapt.new_dport = ntohs(ppe_parse_result->th.dest);
			entry->ipv4_hnapt.bfib1.udp = TCP;
		} else if (ppe_parse_result->iph.protocol == IPPROTO_UDP) {
			entry->ipv4_hnapt.new_sport = ntohs(ppe_parse_result->uh.source);
			entry->ipv4_hnapt.new_dport = ntohs(ppe_parse_result->uh.dest);
			entry->ipv4_hnapt.bfib1.udp = UDP;
		}
	} else if (ppe_parse_result->pkt_type == IPV4_HNAT) {

		if (ppe_parse_result->iph.protocol == IPPROTO_GRE) {

			/* Keep the same GRE caller ID field */
			entry->ipv4_hnapt.new_sport = ntohs(ppe_parse_result->gre_call_id);

			if (debug_level >= 7)
				pr_notice("%s, sport:%04x, gre call id:%04x\n",
					__func__, entry->ipv4_hnapt.sport, entry->ipv4_hnapt.new_sport);
		}

	}

	/*else if (ppe_parse_result.pkt_type == IPV6_1T_ROUTE)*/
		/* do nothing */
	/*else if (ppe_parse_result.pkt_type == IPV6_3T_ROUTE)*/
		/* do nothing */
	/*else if (ppe_parse_result.pkt_type == IPV6_5T_ROUTE)*/
		/* do nothing */
	return 0;
}

static void ppe_set_infoblk2(struct _info_blk2 *iblk2, uint32_t fpidx, uint32_t dr_idx,
			     struct pkt_parse_result *ppe_parse_result)
{
/* Replace 802.1Q priority by user priority */

/*#ifdef FORCE_UP_TEST*/
/*	u32 reg;*/
/**/
/*	iblk2->fp = 1;*/
/*	iblk2->up = 7;*/
/*	reg = reg_read(RALINK_ETH_SW_BASE + 0x2704);*/
/*	reg |= (0x1 << 11);*/
/*	reg_write(RALINK_ETH_SW_BASE + 0x2704, reg);*/
/*#endif*/
	/* we need to lookup another multicast table if this is multicast flow */
	if (debug_level >= 6) {
		pr_notice("%s, fpidx = %x\n", __func__, fpidx);
	}


#if defined(CONFIG_ODU_MCAST_SUPPORT)
	// ODU project specific flow, unicast flow
	if (debug_level >= 6) {
		pr_notice("Kernel config ODU MCAST support set as 0\n");
	}
	iblk2->mcast = 0;
#else
	if ((fe_feature & HNAT_MCAST) && (ppe_parse_result->is_mcast)) {
		//iblk2->mcast = 1;
		/* not yet support mcast to be bind entry, always set iblk2->mcast to 0 */
		iblk2->mcast = 0;

		if (fe_feature & WIFI_HNAT) {
			if ((fpidx == WDMA0_PSE_PORT) || (fpidx == WDMA1_PSE_PORT) ||
			    (fpidx == WDMA2_PSE_PORT) ||
			    (fpidx == MDMA_PSE_PORT))
				fpidx = 0;	/* multicast flow not go to WDMA*/
		}
	} else {
		iblk2->mcast = 0;
	}
#endif

#if defined(CONFIG_HNAT_V2)
	iblk2->dp = fpidx & 0xf;
#endif

#if defined(CONFIG_HNAT_V1)
	iblk2->dp = fpidx & 0x7;

	if (fpidx >= 8)
		iblk2->dp1 = 1;
	else
		iblk2->dp1 = 0;
#endif
	/* set queue id */
	iblk2->dr_idx = dr_idx;
	iblk2->fdr_en = (fpidx == GDMA1_PSE_PORT || fpidx == GDMA2_PSE_PORT) ? 0 : 1;

	if (!(fe_feature & HNAT_QDMA))
		iblk2->fqos = 0;	/* PDMA MODE should not goes to QoS */

	iblk2->acnt = fpidx;
	iblk2->pcpl = 0;
}

/*for 16 queue test*/
unsigned char queue_number;

void set_ppe_qid(struct sk_buff *skb, struct foe_entry *entry)
{
	unsigned int qidx = 0;

	if (IS_IPV4_GRP(entry)) {
		if (skb->mark > 63)
			skb->mark = 0;
	#ifdef CONFIG_MTK_ETHERNET_SOC
		qidx = M2Q_table[(skb->mark) & 0x3f];
	#endif
#if defined(CONFIG_HNAT_V2)
		entry->ipv4_hnapt.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
		entry->ipv4_hnapt.iblk2.qid1 = ((qidx & 0x30) >> 4);
		entry->ipv4_hnapt.iblk2.qid = (qidx & 0x0f);
#endif
	}

	if (IS_IPV6_GRP(entry)) {
		if (skb->mark > 63)
			skb->mark = 0;
	#ifdef CONFIG_MTK_ETHERNET_SOC
		qidx = M2Q_table[(skb->mark) & 0x3f];
	#endif
#if defined(CONFIG_HNAT_V2)
		entry->ipv6_3t_route.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
		entry->ipv6_3t_route.iblk2.qid1 = ((qidx & 0x30) >> 4);
		entry->ipv6_3t_route.iblk2.qid = (qidx & 0x0f);
#endif
	}

}


void set_eth_fqos(struct sk_buff *skb, struct foe_entry *entry, int gmac_no)
{
	int fqos = 0;

	if (set_fqos & 0x1)
		fqos = !(set_fqos & (1 << gmac_no));

	if (debug_level >= 1)
		pr_info("%s, gmac_no:%d, set_fqos:0x%x, fqos:%d\n", __func__, gmac_no, set_fqos, fqos);

	if (IS_IPV4_GRP(entry)) {

		if (FOE_SP(skb) == 5) {
			if ((fe_feature & ETH_QOS) && (fe_feature & AUTO_HNAT))
				entry->ipv4_hnapt.iblk2.fqos = fqos;
			else
				entry->ipv4_hnapt.iblk2.fqos = 0;
		} else {
			if (fe_feature & ETH_QOS)
				entry->ipv4_hnapt.iblk2.fqos = fqos;
			else
				entry->ipv4_hnapt.iblk2.fqos = 0;
		}
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {

			if (FOE_SP(skb) == 5) {
				if ((fe_feature & ETH_QOS) && (fe_feature & AUTO_HNAT))
					entry->ipv6_5t_route.iblk2.fqos = fqos;
				else
					entry->ipv6_5t_route.iblk2.fqos = 0;
			} else {
				if (fe_feature & ETH_QOS)
					entry->ipv6_5t_route.iblk2.fqos = fqos;
				else
					entry->ipv6_5t_route.iblk2.fqos = 0;
			}
		}
	}
}

void set_wifi_auto_qos(struct sk_buff *skb, struct foe_entry *entry, int gmac_no)
{
#if defined(CONFIG_MTK_WIFI7_CHIP_MT7990)
	unsigned int qidx = 0, fqos = 0;


	if (wifi_qos_enable == 0)
		return;

	if (debug_level >= 1)
		pr_info("%s, gmac_no:%d, bss_id:%d, wifi_qos_enable:%d\n", __func__, gmac_no, FOE_BSS_ID(skb), wifi_qos_enable);

	if (wifi_qos_enable == 1) {
		/* 2G */
		if (gmac_no == WDMA0_PSE_PORT && FOE_BSS_ID(skb) == 0) {
			qidx = WIFI2_QOS_QID;
			fqos = 1;
		}
		/* 5G */
		else if (gmac_no == WDMA0_PSE_PORT && FOE_BSS_ID(skb) != 0) {
			qidx = 0;
			fqos = 0;
		}
		/* 6G */
		else if (gmac_no == WDMA1_PSE_PORT && FOE_BSS_ID(skb) != 0) {
			qidx = 0;
			fqos = 0;
		}
	} else if (wifi_qos_enable == 2) {
		/* 2G */
		if (gmac_no == WDMA0_PSE_PORT && FOE_BSS_ID(skb) == 0) {
			qidx = WIFI2_QOS_QID;
			fqos = 1;
		}
		/* 5G */
		else if (gmac_no == WDMA0_PSE_PORT && FOE_BSS_ID(skb) != 0) {
			qidx = WIFI5_QOS_QID;
			fqos = 1;
		}
		/* 6G */
		else if (gmac_no == WDMA1_PSE_PORT && FOE_BSS_ID(skb) != 0) {
			qidx = WIFI6_QOS_QID;
			fqos = 1;
		}
	}

	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.iblk2.fqos = fqos;
#if defined(CONFIG_HNAT_V2)
		entry->ipv4_hnapt.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
		entry->ipv4_hnapt.iblk2.qid1 = ((qidx & 0x30) >> 4);
		entry->ipv4_hnapt.iblk2.qid = (qidx & 0x0f);
#endif
	} else if (IS_IPV6_GRP(entry)) {
		entry->ipv6_5t_route.iblk2.fqos = fqos;
#if defined(CONFIG_HNAT_V2)
		entry->ipv6_3t_route.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
		entry->ipv6_3t_route.iblk2.qid1 = ((qidx & 0x30) >> 4);
		entry->ipv6_3t_route.iblk2.qid = (qidx & 0x0f);
#endif
	}

#endif /* CONFIG_MTK_WIFI7_CHIP_MT7990 */
}


void set_warp_wifi_dp(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result, int gmac_no)
{
	if (debug_level >= 1) {
		pr_notice("FP = %x, FOE_WDMA_ID = %x, FOE_WC_ID = %x, FOE_BSS_ID = %x\n", gmac_no, FOE_WDMA_ID(skb), FOE_WC_ID(skb), FOE_BSS_ID(skb));
	}

	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.minfo = 0;
		ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, gmac_no, (FOE_RX_ID(skb) & 0x03), ppe_parse_result);
		entry->ipv4_hnapt.winfo =
				((FOE_WC_ID(skb) & 0x3ff) << 6) |
				(FOE_BSS_ID(skb) & 0x3f);
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			ppe_set_infoblk2(&entry->ipv6_3t_route.iblk2, gmac_no, (FOE_RX_ID(skb) & 0x03), ppe_parse_result);
			entry->ipv6_3t_route.minfo = 0;
			entry->ipv6_3t_route.winfo =
				((FOE_WC_ID(skb) & 0x3ff) << 6) |
				(FOE_BSS_ID(skb) & 0x3f);
		}
	}
}

/*port means pse port*/
void set_dst_port(struct foe_entry *entry, int port, int dr_idx, struct pkt_parse_result *ppe_parse_result)
{
	if (IS_IPV4_GRP(entry))
		ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, port, dr_idx, ppe_parse_result);	/* 0=PDMA */

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry))
			ppe_set_infoblk2(&entry->ipv6_3t_route.iblk2, port, dr_idx, ppe_parse_result);
	}
}

void set_fast_path_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result)
{
	u8 pse_port;

	if (fe_feature & HNAT_QDMA) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
		pse_port = PDMA_RX;
	} else {
		pse_port = PDMA_RX;
	}

	set_dst_port(entry, pse_port, 0, ppe_parse_result);
}

void set_fast_path_info_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result)
{
	u8 pse_port;

	if (fe_feature & HNAT_QDMA) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
	}

	pse_port = gmac_no;

	set_dst_port(entry, pse_port, 0, ppe_parse_result);
}

void set_rndis_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no, int dr_idx,
		 struct pkt_parse_result *ppe_parse_result)
{
	if (debug_level >= 1)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d, gmac_no:%d, dr_idx:%d\n",
			__func__, FOE_AI(skb), FOE_SP(skb), gmac_no, dr_idx);

	if (fe_feature & HNAT_QDMA) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
	}

	set_dst_port(entry, gmac_no, dr_idx, ppe_parse_result);

	/* Set wifi, modem info as zero */
	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.winfo = 0;
		entry->ipv4_hnapt.minfo = 0;
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			entry->ipv6_3t_route.winfo = 0;
			entry->ipv6_3t_route.minfo = 0;
		}
	}
}


void set_wifi_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result)
{
	int sw_fast_path;

	if (gmac_no == WDMA0_PSE_PORT || gmac_no == WDMA1_PSE_PORT || gmac_no == WDMA2_PSE_PORT)
		sw_fast_path = 0; /* hwnat */
	else {
		sw_fast_path = 1;  /* driver fast path */

		if (gmac_no != EDMA0_PSE_PORT && gmac_no != ADMA_PSE_PORT)
#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
			gmac_no = EDMA0_PSE_PORT;
#else
			gmac_no = ADMA_PSE_PORT;
#endif
	}

	/* hw path */
	if (sw_fast_path == 0) {
		if (fe_feature & HNAT_QDMA) {
			set_ppe_qid(skb, entry);
			set_eth_fqos(skb, entry, gmac_no);
		}

		/* pause frame enhancement */
		set_wifi_auto_qos(skb, entry, gmac_no);
	}

	if (debug_level >= 1)
		pr_info("%s, gmac_no:%d, sw_fast_path:%d\n", __func__, gmac_no, sw_fast_path);

	if (fe_feature & WARP_WHNAT) {
		if (!sw_fast_path) {
			set_warp_wifi_dp(skb, entry, ppe_parse_result, gmac_no);
		} else {
			set_dst_port(entry, gmac_no, 0, ppe_parse_result);
		}
	} else {
		pr_notice("Warp wifi hwnat not support==> fast path\n");
		set_dst_port(entry, gmac_no, 0, ppe_parse_result);
	}
}

void set_modem_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result)
{
	int dr_idx = 0;


	if (debug_level >= 3)
		pr_notice("%s, skb->mark: 0x%x, dr_idx: %d\n", __func__, skb->mark, dr_idx);

#if defined(CONFIG_HNAT_V2)
	/* decide mdma rx ring index */
	if (skb->mark == 0x80000000) {
		dr_idx = 1;
		skb->mark &= (~0x80000000);
	}
#endif /* CONFIG_HNAT_V2 */

	if ((fe_feature & HNAT_QDMA) && md_qos_enable) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
	}

	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.winfo = 0;
		ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, MDMA_PSE_PORT, dr_idx, ppe_parse_result);
		entry->ipv4_hnapt.minfo = ((FOE_MINFO_NTYPE(skb) & 0x7)  << 12) |
					      ((FOE_MINFO_CHID(skb) & 0xff) << 4) |
					      (BIT(15));
	#if defined(CONFIG_HNAT_V1)
		if (skb->mark == 0x80000000) {
			entry->ipv4_hnapt.minfo |= BIT(0);
			if (debug_level >= 6)
				pr_notice("%s, minfo:0x%x\n", __func__, entry->ipv4_hnapt.minfo);

		}
	#endif /* CONFIG_HNAT_V1 */
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			ppe_set_infoblk2(&entry->ipv6_3t_route.iblk2, MDMA_PSE_PORT, dr_idx, ppe_parse_result);
			entry->ipv6_3t_route.winfo = 0;
			entry->ipv6_3t_route.minfo = ((FOE_MINFO_NTYPE(skb) & 0x7)  << 12) |
						      ((FOE_MINFO_CHID(skb) & 0xff) << 4) |
						      (BIT(15));
		#if defined(CONFIG_HNAT_V1)
			if (skb->mark == 0x80000000) {
				entry->ipv6_3t_route.minfo |= BIT(0);
				if (debug_level >= 6)
					pr_notice("%s, minfo:0x%x\n", __func__, entry->ipv6_3t_route.minfo);

			}
		#endif /* CONFIG_HNAT_V1 */
		}
	}
}

void set_snps_info(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
		 struct pkt_parse_result *ppe_parse_result)
{
	if (debug_level >= 1)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d, gmac_no:%d\n",
			__func__, FOE_AI(skb), FOE_SP(skb), gmac_no);

	if (fe_feature & HNAT_QDMA) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
	}

	set_dst_port(entry, gmac_no, 0, ppe_parse_result);

	/* Set wifi, modem info as zero */
	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.winfo = 0;
		entry->ipv4_hnapt.minfo = 0;
	}
	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			entry->ipv6_3t_route.winfo = 0;
			entry->ipv6_3t_route.minfo = 0;
		}
	}
}

/*wan at p4 ==>wan_p4 =1 */
/*sp_tag enable ==> sp_tag = 1*/
int eth_sptag_lan_port_ipv4(struct foe_entry *entry, int wan_p4, struct pkt_parse_result *ppe_parse_result)
{
	if (wan_p4 == 1) {
		if (((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 1) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 2) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 3) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 4)) {
			if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0, ppe_parse_result);
			else
				return 1;
		}
	} else {
		if (((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 2) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 3) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 4) ||
		    ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 5)) {
			if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0, ppe_parse_result);
			else
				return 1;
		}
	}
	return 0;
}

int eth_sptag_wan_port_ipv4(struct foe_entry *entry, int wan_p4, struct pkt_parse_result *ppe_parse_result)
{
	if (wan_p4 == 1) {
		if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 5) {
			if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0, ppe_parse_result);

			else
				return 1;
		}
	} else {
		if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 1) {
			if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0, ppe_parse_result);

			else
				return 1;
		}
	}
	return 0;
}

int eth_sptag_lan_port_ipv6(struct foe_entry *entry, int wan_p4, struct pkt_parse_result *ppe_parse_result)
{
	if (wan_p4 == 1) {
		if (((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 1) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 2) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 3) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 4)) {
			if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv6_5t_route.iblk2, 1, 0, ppe_parse_result);
			else
				return 1;
		}
	} else {
		if (((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 2) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 3) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 4) ||
		    ((entry->ipv6_5t_route.vlan1 & VLAN_VID_MASK) == 5)) {
			if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv6_5t_route.iblk2, 1, 0, ppe_parse_result);
			else
				return 1;
		}
	}
	return 0;
}

int eth_sptag_wan_port_ipv6(struct foe_entry *entry, int wan_p4, struct pkt_parse_result *ppe_parse_result)
{
	if (wan_p4 == 1) {
		if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 5) {
			if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv6_5t_route.iblk2, 1, 0, ppe_parse_result);
			else
				return 1;
		}
	} else {
		if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == 1) {
			if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv6_5t_route.iblk2, 1, 0, ppe_parse_result);

			else
				return 1;
		}
	}
	return 0;
}

int set_eth_dp_gmac1(struct foe_entry *entry, int gmac_no, struct pkt_parse_result *ppe_parse_result)
{
				/* only one GMAC */
	if (IS_IPV4_GRP(entry)) {
#ifdef	CONFIG_RAETH_SPECIAL_TAG
			if (fe_feature & HNAT_WAN_P4) {
				/* sp tag enable, wan at port4 */
				eth_sptag_lan_port_ipv4(entry, 1, ppe_parse_result);
				eth_sptag_wan_port_ipv4(entry, 1, ppe_parse_result);
			} else {
				eth_sptag_lan_port_ipv4(entry, 0, ppe_parse_result);
				eth_sptag_wan_port_ipv4(entry, 0, ppe_parse_result);
			} /* not support one arm */
#else
			if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == lan_vid) {
				if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
					ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
							 ppe_parse_result);
				else
					return 1;
			} else if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == wan_vid) {
				if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
					ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
							 ppe_parse_result);

				else
					return 1;
			} else {/* one-arm */
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
						 ppe_parse_result);
			}
#endif
	}

	if (IS_IPV6_GRP(entry)) {
#ifdef	CONFIG_RAETH_SPECIAL_TAG
		if (fe_feature & HNAT_WAN_P4) { /* sp tag enable, wan at port4 */
			eth_sptag_lan_port_ipv4(entry, 1, ppe_parse_result);
			eth_sptag_wan_port_ipv4(entry, 1, ppe_parse_result);
		} else {
			eth_sptag_lan_port_ipv4(entry, 0, ppe_parse_result);
			eth_sptag_wan_port_ipv4(entry, 0, ppe_parse_result);
		}
#else
		if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == lan_vid) {
				if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
					ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
							 ppe_parse_result);
				else
					return 1;
		} else if ((entry->ipv4_hnapt.vlan1 & VLAN_VID_MASK) == wan_vid) {
			if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
						ppe_parse_result);

			else
				return 1;
		} else/* one-arm */
				ppe_set_infoblk2(&entry->ipv4_hnapt.iblk2, 1, 0,
						ppe_parse_result);
	}

#endif
	return 0;
}

int set_eth_dp_gmac2(struct foe_entry *entry, int gmac_no,
		     struct pkt_parse_result *ppe_parse_result)
{
		/* RT3883/MT7621 with 2xGMAC - Assuming GMAC2=WAN  and GMAC1=LAN */
	if (gmac_no == 1) {
		if ((bind_dir == DOWNSTREAM_ONLY) || (bind_dir == BIDIRECTION))
			set_dst_port(entry, 1, 0, ppe_parse_result); /*pse port1 */

		else
			return 1;
	} else if (gmac_no == 2) {
		if ((bind_dir == UPSTREAM_ONLY) || (bind_dir == BIDIRECTION))
			set_dst_port(entry, 2, 0, ppe_parse_result); /*pse port1 */
		else
			return 1;
	}
	return 0;
}

bool eth_auto_qos_handler(struct foe_entry *entry, int ppe_idx, int hash_idx){

	int qidx = 0, fqos = 0, switch_port = 0, ret = 0, vid;
	u8 dmac[6] = {0};

	if (entry->bfib1.state != BIND)
		return false;

#if defined(CONFIG_HNAT_V2)
	/* QDMA */
	if (entry->bfib1.sp == 5)
		return false;
#endif /* CONFIG_HNAT_V2 */

	if (ppe_fqos(entry) == 1)
		return false;

	if (ppe_force_port(entry) != GDMA2_PSE_PORT)
		return false;

	if (IS_IPV4_GRP(entry)) {
		foe_revert_mac_hi_info(dmac, entry->ipv4_hnapt.dmac_hi);
		foe_revert_mac_lo_info(dmac, entry->ipv4_hnapt.dmac_lo);

	}
	else if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			foe_revert_mac_hi_info(dmac, entry->ipv6_5t_route.dmac_hi);
			foe_revert_mac_lo_info(dmac, entry->ipv6_5t_route.dmac_lo);
		}
	}

	/* internal evb: default is 1 */
	vid = (entry->bfib1.vlan_layer == 1) ? get_vlan1(entry) : 1;

#ifdef CONFIG_MTK_ETHERNET_SOC
	ret = mtk_eth_search_port_by_mac_table(dmac, vid, &switch_port);

	if (debug_level >= 1)
		pr_notice("%s, [%d][%d] vid:%d, dmac=%02x:%02x:%02x:%02x:%02x:%02x, ret:%d, switch_port:%d\n",
			__func__, ppe_idx, hash_idx, vid,
			dmac[0], dmac[1], dmac[2], dmac[3], dmac[4], dmac[5],
			ret, switch_port);

#else /* CONFIG_MTK_ETHERNET_SOC */
	if (debug_level >= 1)
		pr_notice("%s, CONFIG_MTK_ETHERNET_SOC is undefined\n", __func__);
	return false;
#endif /* CONFIG_MTK_ETHERNET_SOC */

	if (ret == 0 && switch_port >= 0 && switch_port < 4) {
		qidx = ETH_QOS_QCOUNT - 1 - switch_port;
		fqos = 1;
	} else
		return false;

	/* cache disable before modify */
	if (ppe_idx == 0)
		reg_modify_bits(CAH_CTRL, 0, 0, 1);
	else if (ppe_idx == 1)
		reg_modify_bits(CAH_CTRL_PPE1, 0, 0, 1);

	if (IS_IPV4_GRP(entry)) {
		entry->ipv4_hnapt.iblk2.fqos = fqos;
#if defined(CONFIG_HNAT_V2)
		entry->ipv4_hnapt.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
		entry->ipv4_hnapt.iblk2.qid1 = ((qidx & 0x30) >> 4);
		entry->ipv4_hnapt.iblk2.qid = (qidx & 0x0f);
#endif

	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV6_GRP(entry)) {
			entry->ipv6_5t_route.iblk2.fqos = fqos;
#if defined(CONFIG_HNAT_V2)
			entry->ipv6_5t_route.iblk2.qid = qidx;
#endif
#if defined(CONFIG_HNAT_V1)
			entry->ipv6_5t_route.iblk2.qid1 = ((qidx & 0x30) >> 4);
			entry->ipv6_5t_route.iblk2.qid = (qidx & 0x0f);
#endif
		}
	}
	return true;
}


void eth_auto_qos_worker(struct work_struct *work) {

	int hash_idx, ppe_idx;
	struct foe_entry *entry;
	bool disable_cache[] = {false, false};
	bool ret;


	for (ppe_idx = 0; ppe_idx < 2; ppe_idx ++) {
		for (hash_idx = 0; hash_idx < FOE_4TB_SIZ; hash_idx ++) {

			if (ppe_idx == 0)
				entry = &ppe_foe_base[hash_idx];
			else
				entry = &ppe1_foe_base[hash_idx];

			ret = eth_auto_qos_handler(entry, ppe_idx, hash_idx);

			if (ret)
				disable_cache[ppe_idx] = ret;


		}
	}

	if (disable_cache[0] == true || disable_cache[1] == true) {
		/*make sure data write to dram*/
		wmb();

		/* enable cache */
		if (disable_cache[0])
			ppe_set_cache_ebl(0);

		if (disable_cache[1])
			ppe_set_cache_ebl(1);
	}
}

void set_eth_auto_qos(struct sk_buff *skb, int gmac_no)
{
	struct foe_entry *entry_output;

	/* don't enable eth auto qos: QDMA TX packet, ETH1 TX packet */
	if (!(fe_feature & ETH_QOS) || !eth_qos_enable ||
	    FOE_SP(skb) == 5 || gmac_no != GDMA2_PSE_PORT)
		return;

	entry_output = decide_which_ppe(skb);
	if (entry_output == NULL)
		return;

	schedule_work(&hnat_work);
}


uint32_t ppe_set_ext_if_num(struct sk_buff *skb, struct foe_entry *entry)
{
	u32 offset = 0;
	u32 i = 0;
	int dev_match = 0;

	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == skb->dev) {
			offset = i;
			dev_match = 1;
			break;
		}
	}

#ifdef CONFIG_RAETH_EDMA
	for (i = 1; i < MAX_IF_NUM; i++) {
		if (dst_port[i]->name == NULL) {
			pr_err("dst_port name is NULL\n");
			break;
		}

		if ((strcmp(dst_port[i]->name, DEV_NAME) == 0 && strcmp(skb->dev->name, AQR_DEV_NAME) == 0) ||
		    (strcmp(dst_port[i]->name, DEV2_NAME) == 0 && strcmp(skb->dev->name, AQR_DEV2_NAME) == 0)) {
			offset = i;
			dev_match = 1;
			if (debug_level >= 7)
				pr_notice("[HS-ethernet/HWNAT/TX] %s : dev_match Interfacess=%s, vir_if_idx=%x\n", __func__, skb->dev->name, offset);
			break;
		}
	}
#endif

	if (dev_match == 0) {
		if (debug_level >= 1)
			pr_notice("%s UnKnown Interface, offset =%x\n", __func__, i);
		return 1;
	}

	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		entry->ipv4_hnapt.act_dp = offset;
		return 0;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(entry))
			entry->ipv4_dslite.act_dp = offset;
		else if (IS_IPV6_3T_ROUTE(entry))
			entry->ipv6_3t_route.act_dp = offset;
		else if (IS_IPV6_5T_ROUTE(entry))
			entry->ipv6_5t_route.act_dp = offset;
		else if (IS_IPV6_6RD(entry))
			entry->ipv6_6rd.act_dp = offset;
		else {
			if (debug_level >= 1)
				pr_notice("%s UnKnown packet type \n", __func__);
			return 1;
		}
	}

	return 0;
}

int ppe_forbit_bind(struct sk_buff *skb, struct foe_entry *entry) {

	uint32_t act_dp = get_act_dp(entry);
	uint32_t rxif_idx = FOE_IF_IDX(skb);

	if (entry->ipv4_hnapt.sip == 0) {
		if (debug_level >= 3)
			pr_notice("%s(), sip is 0\n", __func__);
		return 1;
	}

	if (entry->bfib1.state != UNBIND) {
		if (debug_level >= 3)
			pr_notice("%s(), state is %d\n", __func__, entry->bfib1.state);
		return 1;
	}

	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		if (entry->ipv4_hnapt.sip == entry->ipv4_hnapt.new_dip) {
			if (debug_level >= 3)
				pr_notice("%s(), ip is revert\n", __func__);
			return 1;
		}
	}

	/* invalid index */
	if (rxif_idx == INVALID_IFIDX || act_dp == INVALID_IFIDX) {
		if (debug_level >= 3)
			pr_notice("%s(), invalid port: %d,%d\n", __func__, rxif_idx, act_dp);
		return 1;
	}

	/* empty net device*/
	if (!dst_port[rxif_idx] || !dst_port[act_dp]) {
		if (debug_level >= 3)
			pr_notice("%s(), empty net device: %d,%d\n", __func__, rxif_idx, act_dp);
		return 1;
	}

	/* modem in and modem out: caused by skb reuse */
	if (rxif_idx >= DP_CCMNI0 && act_dp >= DP_CCMNI0){
	    	if (debug_level >= 3)
			pr_notice("%s(), both are modem port: %d,%d\n", __func__, rxif_idx, act_dp);
		return 1;
	}

	return 0;
}

void ppe_set_entry_bind(struct sk_buff *skb, struct foe_entry *entry)
{
	u32 current_time;
	u32 act_dp;
	int forbit;

	forbit = ppe_forbit_bind(skb, entry);
	if (forbit)
		return;

	/* Set Current time to time_stamp field in information block 1 */
	current_time = reg_read(FOE_TS) & 0x3FFF;
	entry->bfib1.time_stamp = (uint16_t)current_time;

	/* Ipv4: TTL / Ipv6: Hot Limit filed */
	entry->ipv4_hnapt.bfib1.ttl = DFL_FOE_TTL_REGEN;

	/* enable cache by default */

	entry->ipv4_hnapt.bfib1.cah = 1;

	hwnat_set_packet_sampling(entry);

#ifdef CONFIG_RA_HW_NAT_PREBIND
	entry->udib1.preb = 1;
#else
	if (debug_level >= 1) {
		act_dp = get_act_dp(entry);
		pr_notice("%s,!!!!!FOE_IF_IDX = %d(%s), act_dp=%d(%s)\n",
			__func__,
			FOE_IF_IDX(skb), dst_port[FOE_IF_IDX(skb)]->name,
			act_dp, dst_port[act_dp]->name);
	}

	set_rxif_idx(entry, FOE_IF_IDX(skb));

	/* Change Foe Entry State to Binding State */
	entry->bfib1.state = BIND;

	/* mib related work */
	ppe_reset_entry_mib(entry);
	ppe_start_mib_timer();

	/* awake AP CPU to avoid netsys suspend */
	hnat_wakelock_awake();
#endif
}

void ppe_dev_reg_handler(struct net_device *dev)
{
	int i, err;


	if (dev == NULL)
		return;

	if (strncmp(dev->name, "ccmni", 5) == 0) {

		err = kstrtoint(&dev->name[5], 10, &i);
		if (err != 0) {
			pr_notice("%s, err: %d\n", __func__, err);
			return;
		}

		i += DP_CCMNI0;

		if (i < 0 || i >= MAX_IF_NUM) {
			pr_notice("%s, err, %d is out of bound\n", __func__, i);
			return;
		}

		if (dst_port[i] == dev) {
			pr_notice("%s, (%d)%s - redundant\n", __func__, i, dev->name);
			return;

		} else if (dst_port[i] == NULL) {
			dst_port[i] = dev;
			dst_port_type[i] = HW_PATH;

			pr_notice("%s, (%d)%s - succeed\n", __func__, i, dev->name);

		} else {

			pr_notice("%s, ERR (%d)%s - occupied, check why ?\n", __func__, i, dev->name);
			return;
		}

	} else {

		for (i = 1; i < MAX_IF_NUM; i++) {

			if (dst_port[i] == dev) {
				pr_notice("%s, (%d)%s - redundant\n", __func__, i, dev->name);
				return;
			}
		}

		for (i = 1; i < MAX_IF_NUM; i++) {

			if (dst_port[i] == NULL) {
				dst_port[i] = dev;
				dst_port_type[i] = SW_PATH;

				pr_notice("%s, (%d)%s - succeed\n", __func__, i, dev->name);
				break;
			}
		}
	}
}

void ppe_dev_unreg_handler(struct net_device *dev)
{
	int i, del = 0;

	if (dev == NULL)
		return;

	for (i = 1; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			ppe_reset_dev_mib(dev);
			foe_del_entry_by_dev(dev);
			dst_port[i] = NULL;
			del = 1;
			break;
		}
	}

	if (del == 1)
		pr_notice("%s, (%d)%s - removed\n", __func__, i, dev->name);

}
#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
int get_done_bit(struct sk_buff *skb, struct foe_entry *entry)
{
	int done_bit;

	done_bit = 0;

	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		done_bit = entry->ipv4_hnapt.resv1;
		return done_bit;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(entry)) {
			done_bit = entry->ipv4_dslite.resv1;
		} else if (IS_IPV6_3T_ROUTE(entry)) {
			done_bit = entry->ipv6_3t_route.resv1;
		} else if (IS_IPV6_5T_ROUTE(entry)) {
			done_bit = entry->ipv6_5t_route.resv1;
		} else if (IS_IPV6_6RD(entry)) {
			done_bit = entry->ipv6_6rd.resv1;
		} else {
			if (debug_level >= 6)
				pr_notice("get packet format something wrong\n");
			return 0;
		}
	}

	if ((done_bit != 0) && (done_bit != 1)) {
		pr_notice("done bit something wrong, done_bit = %d\n", done_bit);
		done_bit = 0;
	}
	/* pr_notice("index = %d, done_bit=%d\n", FOE_ENTRY_NUM(skb), done_bit); */
	return done_bit;
}

void set_ppe_table_done(struct foe_entry *entry)
{
	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		entry->ipv4_hnapt.resv1 = 1;
		return;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(entry))
			entry->ipv4_dslite.resv1 = 1;
		else if (IS_IPV6_3T_ROUTE(entry))
			entry->ipv6_3t_route.resv1 = 1;
		else if (IS_IPV6_5T_ROUTE(entry))
			entry->ipv6_5t_route.resv1 = 1;
		else if (IS_IPV6_6RD(entry))
			entry->ipv6_6rd.resv1 = 1;
		else
			if (debug_level >= 6)
				pr_notice("get packet format something wrong\n");
	}
}
#endif

int get_skb_interface(struct sk_buff *skb)
{
	if ((strncmp(skb->dev->name, "rai", 3) == 0) ||
	    (strncmp(skb->dev->name, "apclii", 6) == 0) ||
	    (strncmp(skb->dev->name, "wdsi", 4) == 0) ||
	    (strncmp(skb->dev->name, "wlan", 4) == 0))
		return 1;
	else
		return 0;
}

void ppe_setfoe_ebl(uint32_t foe_ebl)
{
	u32 ppe_flow_set = 0;

	ppe_flow_set = reg_read(PPE_FLOW_SET);

	/* FOE engine need to handle unicast/multicast/broadcast flow */
	if (foe_ebl == 1) {
		ppe_flow_set |= (BIT_IPV4_NAPT_EN | BIT_IPV4_NAT_EN);
		ppe_flow_set |= (BIT_IPV4_NAT_FRAG_EN | BIT_UDP_IP4F_NAT_EN);	/* ip fragment */
		ppe_flow_set |= (BIT_IPV4_HASH_GREK);

		ppe_flow_set |= BIT_IPV6_6RD_EN | BIT_IPV6_3T_ROUTE_EN | BIT_IPV6_5T_ROUTE_EN;
			/* ppe_flow_set |= (BIT_IPV6_HASH_FLAB); // flow label */

		ppe_flow_set |= (BIT_IPV6_HASH_GREK);
		ppe_flow_set |= (BIT_IPV4_464XLAT_EN);

		if (SwitchDslMape == 1)
			ppe_flow_set |= (BIT_IPV4_MAPE_EN);
		else
			ppe_flow_set |= (BIT_IPV4_DSL_EN);
	} else {
		ppe_flow_set &= ~(BIT_IPV4_NAPT_EN | BIT_IPV4_NAT_EN);
		ppe_flow_set &= ~(BIT_IPV4_NAT_FRAG_EN);

		ppe_flow_set &= ~(BIT_IPV6_6RD_EN | BIT_IPV6_3T_ROUTE_EN |
				  BIT_IPV6_5T_ROUTE_EN);
		if (SwitchDslMape == 1)
			ppe_flow_set &= ~(BIT_IPV4_MAPE_EN);
		else
			ppe_flow_set &= ~(BIT_IPV4_DSL_EN);

			/* ppe_flow_set &= ~(BIT_IPV6_HASH_FLAB); */

		ppe_flow_set &= ~(BIT_IPV6_HASH_GREK);
		ppe_flow_set &= ~(BIT_IPV4_464XLAT_EN);

	}

	reg_write(PPE_FLOW_SET, ppe_flow_set);
	reg_write(PPE1_FLOW_SET, ppe_flow_set);
}

void ppe_set_foe_table(void)
{
	/* set foe table */
	reg_write(PPE_FOE_BASE, ppe_phy_foe_base);
	reg_write(MIB_TB_BASE, ppe_phy_mib_base);

	reg_write(PPE1_FOE_BASE, ppe1_phy_foe_base);
	reg_write(MIB_TB_BASE_PPE1, ppe1_phy_mib_base);


	switch (FOE_4TB_SIZ) {
	case 1024:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_1K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_1K, 0, 3);
		break;

	case 2048:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_2K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_2K, 0, 3);
		break;

	case 4096:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_4K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_4K, 0, 3);
		break;

	case 8192:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_8K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_8K, 0, 3);
		break;

	case 16384:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_16K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_16K, 0, 3);
		break;

	case 32768:
		reg_modify_bits(PPE_FOE_CFG, FOE_TBL_SIZE_32K, 0, 3);
		reg_modify_bits(PPE1_FOE_CFG, FOE_TBL_SIZE_32K, 0, 3);
		break;
	}

	/* Set Hash Mode */
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_HASH_MODE, 14, 2);
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_HASH_MODE, 14, 2);

	reg_write(PPE_HASH_SEED, HASH_SEED);
	reg_write(PPE1_HASH_SEED, HASH_SEED);

	reg_modify_bits(PPE_FOE_CFG, 0, 18, 2);	/* disable */
	reg_modify_bits(PPE1_FOE_CFG, 0, 18, 2);	/* disable */

#ifdef CONFIG_RA_HW_NAT_PREBIND
	reg_modify_bits(PPE_FOE_CFG, 1, 6, 1);	/* pre-bind age enable */
	reg_modify_bits(PPE1_FOE_CFG, 1, 6, 1);	/* pre-bind age enable */

#endif
	/* Set action for FOE search miss */
	reg_modify_bits(PPE_FOE_CFG, FWD_CPU_BUILD_ENTRY, 4, 2);
	reg_modify_bits(PPE1_FOE_CFG, FWD_CPU_BUILD_ENTRY, 4, 2);
}

static void ppe_setage_out(void)
{
	/* set Bind Non-TCP/UDP Age Enable */
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_NTU_AGE, 7, 1);

	/* set Unbind State Age Enable */
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_UNB_AGE, 8, 1);

	/* set min threshold of packet count for aging out at unbind state */
	reg_modify_bits(PPE_FOE_UNB_AGE, DFL_FOE_UNB_MNP, 16, 16);

	/* set Delta time for aging out an unbind FOE entry */
	reg_modify_bits(PPE_FOE_UNB_AGE, DFL_FOE_UNB_DLTA, 0, 8);

#ifndef CONFIG_HW_NAT_MANUAL_MODE
		/* set Bind TCP Age Enable */
		reg_modify_bits(PPE_FOE_CFG, DFL_FOE_TCP_AGE, 9, 1);

		/* set Bind UDP Age Enable */
		reg_modify_bits(PPE_FOE_CFG, DFL_FOE_UDP_AGE, 10, 1);

		/* set Bind TCP FIN Age Enable */
		reg_modify_bits(PPE_FOE_CFG, DFL_FOE_FIN_AGE, 11, 1);

		/* set Delta time for aging out an bind UDP FOE entry */
		reg_modify_bits(PPE_FOE_BND_AGE0, DFL_FOE_UDP_DLTA, 0, 16);

		/* set Delta time for aging out an bind Non-TCP/UDP FOE entry */
		reg_modify_bits(PPE_FOE_BND_AGE0, DFL_FOE_NTU_DLTA, 16, 16);

		/* set Delta time for aging out an bind TCP FIN FOE entry */
		reg_modify_bits(PPE_FOE_BND_AGE1, DFL_FOE_FIN_DLTA, 16, 16);

		/* set Delta time for aging out an bind TCP FOE entry */
		reg_modify_bits(PPE_FOE_BND_AGE1, DFL_FOE_TCP_DLTA, 0, 16);
#else
		/* fix TCP last ACK issue */
		/* Only need to enable Bind TCP FIN aging out function */
		reg_modify_bits(PPE_FOE_CFG, DFL_FOE_FIN_AGE, 11, 1);
		/* set Delta time for aging out an bind TCP FIN FOE entry */
		reg_modify_bits(PPE_FOE_BND_AGE1, DFL_FOE_FIN_DLTA, 16, 16);

#endif
}

static void ppe_setfoe_ka(void)
{
	/* set Keep alive packet with new/org header */
	reg_modify_bits(PPE_FOE_CFG, DFL_FOE_KA, 12, 2);

#if defined(CONFIG_HNAT_V2)
	/* Keep alive always sync at TSO packet with Last=1 */
	reg_modify_bits(PPE_FOE_CFG, 1, 18, 1);

	/* Set Keep alive interval as 32s */
	reg_modify_bits(PPE_FOE_CFG, 1, 24, 1);
#endif /* CONFIG_HNAT_V2 */

	/* Keep alive timer value */
	reg_modify_bits(PPE_FOE_KA, DFL_FOE_KA_T, 0, 16);

	/* Keep alive time for bind FOE TCP entry */
	reg_modify_bits(PPE_FOE_KA, DFL_FOE_TCP_KA, 16, 8);

	/* Keep alive timer for bind FOE UDP entry */
	reg_modify_bits(PPE_FOE_KA, DFL_FOE_UDP_KA, 24, 8);

	/* Keep alive timer for bind Non-TCP/UDP entry */
	reg_modify_bits(PPE_BIND_LMT_1, DFL_FOE_NTU_KA, 16, 8);

#ifdef CONFIG_RA_HW_NAT_PREBIND
		reg_modify_bits(PPE_BIND_LMT_1, DFL_PBND_RD_LMT, 24, 8);
#endif
}

static void ppe_setfoe_bind_rate(uint32_t foe_bind_rate)
{
	/* Allowed max entries to be build during a time stamp unit */

	/* smaller than 1/4 of total entries */
	reg_modify_bits(PPE_FOE_LMT1, DFL_FOE_QURT_LMT, 0, 14);

	/* between 1/2 and 1/4 of total entries */
	reg_modify_bits(PPE_FOE_LMT1, DFL_FOE_HALF_LMT, 16, 14);

	/* between full and 1/2 of total entries */
	reg_modify_bits(PPE_FOE_LMT2, DFL_FOE_FULL_LMT, 0, 15);

	/* Set reach bind rate for unbind state */
	reg_modify_bits(PPE_FOE_BNDR, foe_bind_rate, 0, 16);
#ifdef CONFIG_RA_HW_NAT_PREBIND
		reg_modify_bits(PPE_FOE_BNDR, DFL_PBND_RD_PRD, 16, 16);
#endif
}

static void ppe_setfoe_glocfg_ebl(uint32_t ebl)
{
	if (ebl == 1) {
		/* PPE Engine Enable */
		reg_modify_bits(PPE_GLO_CFG, 1, 0, 1);

	if (fe_feature & HNAT_IPV6) {
		/* TSID Enable */
		reg_modify_bits(PPE_GLO_CFG, 1, 1, 1);
	}

	/* Enable multicast table lookup enable bit, to avoid netsys hang when iblk2->mcast = 1 */
	reg_modify_bits(PPE_GLO_CFG, 1, 7, 1);

	if (fe_feature & HNAT_MCAST) {
		/* multicast table lookup settings */
		reg_modify_bits(PPE_GLO_CFG, 0, 12, 2);	/* Decide by PPE entry hash index */
		reg_modify_bits(PPE_MCAST_PPSE, 0, 0, 4);	/* multicast port0 map to PDMA */
		reg_modify_bits(PPE_MCAST_PPSE, 1, 4, 4);	/* multicast port1 map to GMAC1 */
		reg_modify_bits(PPE_MCAST_PPSE, 2, 8, 4);	/* multicast port2 map to GMAC2 */
		reg_modify_bits(PPE_MCAST_PPSE, 5, 12, 4);	/* multicast port3 map to QDMA */
	}			/* CONFIG_PPE_MCAST // */


	reg_write(PPE_DFT_CPORT, 0);	/* default CPU port is port0 (PDMA) */

	//WDMA, MDMA source port = drop port
	/* default CPU port for PSE Port 15~Port8, note that edma1 also set cpu port to edma0*/
	reg_write(PPE_DFT_CPORT1, 0x0e7bb777);
	reg_write(PPE_SBW_CTRL, 0x7f);

	//6rd setting
	reg_modify_bits(PPE_GLO_CFG, 1, 20, 1);

	/* reg_write(PS_CFG, 1); //Enable PacketSampling */
		if (fe_feature & PPE_MIB) {
			reg_write(MIB_CFG, 0x03);	/* Enable MIB & read clear */
			reg_write(MIB_CAH_CTRL, 0x01);	/* enable mib cache */
		}

		/* PPE Packet with TTL=0 alert to cpu*/
		reg_modify_bits(PPE_GLO_CFG, DFL_TTL0_DRP, 4, 1);

	} else {
		/* PPE Engine Disable */
		reg_modify_bits(PPE_GLO_CFG, 0, 0, 1);
		if (fe_feature & PPE_MIB)
			reg_write(MIB_CFG, 0x00);	/* Disable MIB */
	}
}


static void ppe1_setage_out(void)
{
	/* set Bind Non-TCP/UDP Age Enable */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_NTU_AGE, 7, 1);

	/* set Unbind State Age Enable */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_UNB_AGE, 8, 1);

	/* set min threshold of packet count for aging out at unbind state */
	reg_modify_bits(PPE1_FOE_UNB_AGE, DFL_FOE_UNB_MNP, 16, 16);

	/* set Delta time for aging out an unbind FOE entry */
	reg_modify_bits(PPE1_FOE_UNB_AGE, DFL_FOE_UNB_DLTA, 0, 8);

	/* set Bind TCP Age Enable */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_TCP_AGE, 9, 1);

	/* set Bind UDP Age Enable */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_UDP_AGE, 10, 1);

	/* set Bind TCP FIN Age Enable */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_FIN_AGE, 11, 1);

	/* set Delta time for aging out an bind UDP FOE entry */
	reg_modify_bits(PPE1_FOE_BND_AGE0, DFL_FOE_UDP_DLTA, 0, 16);

	/* set Delta time for aging out an bind Non-TCP/UDP FOE entry */
	reg_modify_bits(PPE1_FOE_BND_AGE0, DFL_FOE_NTU_DLTA, 16, 16);

	/* set Delta time for aging out an bind TCP FIN FOE entry */
	reg_modify_bits(PPE1_FOE_BND_AGE1, DFL_FOE_FIN_DLTA, 16, 16);

	/* set Delta time for aging out an bind TCP FOE entry */
	reg_modify_bits(PPE1_FOE_BND_AGE1, DFL_FOE_TCP_DLTA, 0, 16);
}

static void ppe1_setfoe_ka(void)
{
	/* set Keep alive packet with new/org header */
	reg_modify_bits(PPE1_FOE_CFG, DFL_FOE_KA, 12, 2);

#if defined(CONFIG_HNAT_V2)
	/* Keep alive always sync at TSO packet with Last=1 */
	reg_modify_bits(PPE1_FOE_CFG, 1, 18, 1);

	/* Set Keep alive interval as 32s */
	reg_modify_bits(PPE1_FOE_CFG, 1, 24, 1);

#endif /* CONFIG_HNAT_V2 */

	/* Keep alive timer value */
	reg_modify_bits(PPE1_FOE_KA, DFL_FOE_KA_T, 0, 16);

	/* Keep alive time for bind FOE TCP entry */
	reg_modify_bits(PPE1_FOE_KA, DFL_FOE_TCP_KA, 16, 8);

	/* Keep alive timer for bind FOE UDP entry */
	reg_modify_bits(PPE1_FOE_KA, DFL_FOE_UDP_KA, 24, 8);

	/* Keep alive timer for bind Non-TCP/UDP entry */
	reg_modify_bits(PPE1_BIND_LMT_1, DFL_FOE_NTU_KA, 16, 8);

#ifdef CONFIG_RA_HW_NAT_PREBIND
		reg_modify_bits(PPE1_BIND_LMT_1, DFL_PBND_RD_LMT, 24, 8);
#endif
}

static void ppe1_setfoe_bind_rate(uint32_t foe_bind_rate)
{
	/* Allowed max entries to be build during a time stamp unit */

	/* smaller than 1/4 of total entries */
	reg_modify_bits(PPE1_FOE_LMT1, DFL_FOE_QURT_LMT, 0, 14);

	/* between 1/2 and 1/4 of total entries */
	reg_modify_bits(PPE1_FOE_LMT1, DFL_FOE_HALF_LMT, 16, 14);

	/* between full and 1/2 of total entries */
	reg_modify_bits(PPE1_FOE_LMT2, DFL_FOE_FULL_LMT, 0, 15);

	/* Set reach bind rate for unbind state */
	reg_modify_bits(PPE1_FOE_BNDR, foe_bind_rate, 0, 16);
#ifdef CONFIG_RA_HW_NAT_PREBIND
		reg_modify_bits(PPE1_FOE_BNDR, DFL_PBND_RD_PRD, 16, 16);
#endif
}

static void ppe1_setfoe_glocfg_ebl(uint32_t ebl)
{
	if (ebl == 1) {
		/* PPE Engine Enable */
		reg_modify_bits(PPE1_GLO_CFG, 1, 0, 1);

	if (fe_feature & HNAT_IPV6) {
		/* TSID Enable */
		reg_modify_bits(PPE1_GLO_CFG, 1, 1, 1);
	}

	/* Enable multicast table lookup enable bit, to avoid netsys hang when iblk2->mcast = 1 */
	reg_modify_bits(PPE1_GLO_CFG, 1, 7, 1);

	if (fe_feature & HNAT_MCAST) {
		/* multicast table lookup settings */
		reg_modify_bits(PPE1_GLO_CFG, 0, 12, 2);	/* Decide by PPE entry hash index */
		reg_modify_bits(PPE1_MCAST_PPSE, 0, 0, 4);	/* multicast port0 map to PDMA */
		reg_modify_bits(PPE1_MCAST_PPSE, 1, 4, 4);	/* multicast port1 map to GMAC1 */
		reg_modify_bits(PPE1_MCAST_PPSE, 2, 8, 4);	/* multicast port2 map to GMAC2 */
		reg_modify_bits(PPE1_MCAST_PPSE, 5, 12, 4);	/* multicast port3 map to QDMA */
	}			/* CONFIG_PPE_MCAST // */


	reg_write(PPE1_DFT_CPORT, 0);	/* default CPU port is port0 (PDMA) */

	/* default CPU port for PSE Port 15~Port8, note that edma1 also set cpu port to edma0*/
	reg_write(PPE1_DFT_CPORT1, 0x0e7bb777);
	reg_write(PPE1_SBW_CTRL, 0x7f);


	//6rd setting
	reg_modify_bits(PPE1_GLO_CFG, 1, 20, 1);


		if (fe_feature & PPE_MIB) {
			reg_write(MIB_CFG_PPE1, 0x03);	/* Enable MIB & read clear */
			reg_write(MIB_CAH_CTRL_PPE1, 0x01);	/* enable mib cache */
		}

		/* PPE Packet with TTL=0 alert to cpu*/
		reg_modify_bits(PPE1_GLO_CFG, DFL_TTL0_DRP, 4, 1);

	} else {
		/* PPE Engine Disable */
		reg_modify_bits(PPE1_GLO_CFG, 0, 0, 1);
		if (fe_feature & PPE_MIB)
			reg_write(MIB_CFG_PPE1, 0x00);	/* Disable MIB */
	}
}

void foe_free_tbl(uint32_t num_of_entry, struct device *dev)
{
	u32 foe_tbl_size, mib_tbl_size;

	foe_tbl_size = num_of_entry * sizeof(struct foe_entry);
	mib_tbl_size = num_of_entry * sizeof(struct mib_entry);
	dma_free_coherent(dev, foe_tbl_size, ppe_foe_base, ppe_phy_foe_base);
	dma_free_coherent(dev, foe_tbl_size, ppe1_foe_base, ppe1_phy_foe_base);
	dma_free_coherent(dev, mib_tbl_size, ppe_mib_base, ppe_phy_mib_base);
	dma_free_coherent(dev, mib_tbl_size, ppe1_mib_base, ppe1_phy_mib_base);


}


int32_t ppe_eng_start(void)
{
	/* Set PPE Flow Set */
	ppe_setfoe_ebl(1);

	/* Set Auto Age-Out Function */
	ppe_setage_out();

	/* Set PPE FOE KEEPALIVE TIMER */
	ppe_setfoe_ka();

	/* Set PPE FOE Bind Rate */
	ppe_setfoe_bind_rate((fe_feature & AUTO_HNAT)? 10 : DFL_FOE_BNDR);

	/* Set PPE Global Configuration */
	ppe_setfoe_glocfg_ebl(1);

	/* Set Auto Age-Out Function */
	ppe1_setage_out();

	/* Set PPE FOE KEEPALIVE TIMER */
	ppe1_setfoe_ka();

	/* Set PPE FOE Bind Rate */
	ppe1_setfoe_bind_rate((fe_feature & AUTO_HNAT)? 10 : DFL_FOE_BNDR);

	/* Set PPE Global Configuration */
	ppe1_setfoe_glocfg_ebl(1);

	/*PSE ring full drop enable*/
	//reg_write(PSE_PPE0_DROP, 0x700);
	//reg_write(PSE_PPE1_DROP, 0x700);

	return 0;
}

void ppe_eng_stop(void)
{
	/* disable scan mode */
	reg_modify_bits(PPE_FOE_CFG, 0, 16, 2);
	reg_modify_bits(PPE1_FOE_CFG, 0, 16, 2);

	/*disable TB CFG*/
	reg_write(PPE_TB_CFG, 0x2f000);
	reg_write(PPE1_TB_CFG, 0x2f000);

	/* Set PPE FOE ENABLE */
	ppe_setfoe_glocfg_ebl(0);
	ppe1_setfoe_glocfg_ebl(0);

	/* Set PPE Flow Set */
	ppe_setfoe_ebl(0);

	/* clear FOE table */
	reg_write(PPE_FOE_BASE, 0);
	reg_write(PPE1_FOE_BASE, 0);
}

struct net_device *ra_dev_get_by_name(const char *name)
{
	return dev_get_by_name(&init_net, name);
}

void eth_register(void)
{
	struct net_device *dev;
	int i;

	if (fe_feature & AUTO_HNAT) {
#ifndef CONFIG_MTK_SGMII_NETSYS
		return;
#endif
	}

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_LAN);
	ppe_dev_reg_handler(dev);
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			DP_GMAC1 = i;
			dst_port_type[i] = HW_PATH;
			break;
		}
	}

	if (fe_feature & AUTO_HNAT) {
#ifdef CONFIG_MTK_SGMII_SNPS
		return;
#endif
	}

	if (fe_feature & GE2_SUPPORT) {
		if (fe_feature & AUTO_HNAT) {
			second_fast_path = 1;
		}

		dev = ra_dev_get_by_name(DEV_NAME_HNAT_WAN);
		ppe_dev_reg_handler(dev);
		for (i = 0; i < MAX_IF_NUM; i++) {
			if (dst_port[i] == dev) {
				DP_GMAC2 = i;
				dst_port_type[i] = HW_PATH;
				break;
			}
		}
	}


}
#if(0)
void modem_if_register(void)
{
	struct net_device *dev;

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI0);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI1);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI2);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI3);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI4);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI5);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI6);
	ppe_dev_reg_handler(dev);

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_CCCI7);
	ppe_dev_reg_handler(dev);
}
#endif
void rndis_if_register(void)
{
	struct net_device *dev;

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_RNDIS0);
	ppe_dev_reg_handler(dev);
}

void wifi_if_register(void)
{
	struct net_device *dev;

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_RA0);
	ppe_dev_reg_handler(dev);
	dev = ra_dev_get_by_name(DEV_NAME_HNAT_RAI0);
	ppe_dev_reg_handler(dev);
	dev = ra_dev_get_by_name(DEV_NAME_HNAT_RAX0);
	ppe_dev_reg_handler(dev);
	dev = ra_dev_get_by_name(DEV_NAME_HNAT_APCLI0);
	ppe_dev_reg_handler(dev);
	dev = ra_dev_get_by_name(DEV_NAME_HNAT_APCLI1);
	ppe_dev_reg_handler(dev);
}

void ext_if_register(void)
{
	struct net_device *dev;
	int i;

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_EDMA0);
	ppe_dev_reg_handler(dev);
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			pr_notice("%s :dst_port[%d] =%s\n", __func__, i, dev->name);
			DP_EDMA0 = i;
			dst_port_type[i] = SW_PATH;
			break;
		}
	}

	dev = ra_dev_get_by_name(DEV_NAME_HNAT_EDMA1);
	ppe_dev_reg_handler(dev);
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == dev) {
			pr_notice("%s :dst_port[%d] =%s\n", __func__, i, dev->name);
			DP_EDMA1 = i;
			dst_port_type[i] = SW_PATH;
			break;
		}
	}

}

void snps_if_register(void)
{
	struct net_device *dev;

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
	/* use edma */
	dev = ra_dev_get_by_name(DEV_NAME_HNAT_SNPS);
	ppe_dev_reg_handler(dev);
#else
	/* use adma and check at least one eth is running */
	if (IS_FAST_PATH_UP) {
		dev = ra_dev_get_by_name(DEV_NAME_HNAT_SNPS);
		ppe_dev_reg_handler(dev);
	}
#endif
}

void ppe_set_dst_port(uint32_t ebl)
{
	int j;

	for (j = 0; j < MAX_IF_NUM; j++)
		dst_port_type[j] = HW_PATH;

	if (ebl) {

		// HNAT + eth, default case if not EDMA involved
		eth_register();

		//modem_if_register();
		wifi_if_register();
		rndis_if_register();
		if (fe_feature & AUTO_HNAT)
			snps_if_register();
	} else {
		 /* disable */
		if(DP_GMAC1 >= 1 && DP_GMAC1 < MAX_IF_NUM && dst_port[DP_GMAC1] != NULL)
			dev_put(dst_port[DP_GMAC1]);

		if(DP_GMAC2 >= 1 && DP_GMAC2 < MAX_IF_NUM && dst_port[DP_GMAC2] != NULL)
			dev_put(dst_port[DP_GMAC2]);

		for (j = 0; j < MAX_IF_NUM; j++) {
			if (dst_port[j])
				dst_port[j] = NULL;
		}
	}
}


uint32_t set_gdma_fwd(uint32_t ebl)
{
	u32 data = 0;

	data = reg_read(FE_GDMA1_FWD_CFG);

	if (ebl) {
		data &= ~0x7777;
		/* Uni-cast frames forward to PPE */
		data |= GDM1_UFRC_P_PPE;
		/* Broad-cast MAC address frames forward to PPE */
		data |= GDM1_BFRC_P_PPE;
		/* Multi-cast MAC address frames forward to PPE */
		data |= GDM1_MFRC_P_PPE;
		/* Other MAC address frames forward to PPE */
		data |= GDM1_OFRC_P_PPE;

	} else {
		data &= ~0x7777;
		/* Uni-cast frames forward to CPU */
		data |= GDM1_UFRC_P_CPU;
		/* Broad-cast MAC address frames forward to CPU */
		data |= GDM1_BFRC_P_CPU;
		/* Multi-cast MAC address frames forward to CPU */
		data |= GDM1_MFRC_P_CPU;
		/* Other MAC address frames forward to CPU */
		data |= GDM1_OFRC_P_CPU;
	}

	//reg_write(FE_GDMA1_FWD_CFG, data);

	if (fe_feature & GE2_SUPPORT) {
		data = reg_read(FE_GDMA2_FWD_CFG);

		if (ebl) {
			data &= ~0x7777;
			/* Uni-cast frames forward to PPE */
			data |= GDM1_UFRC_P_PPE;
			/* Broad-cast MAC address frames forward to PPE */
			data |= GDM1_BFRC_P_PPE;
			/* Multi-cast MAC address frames forward to PPE */
			data |= GDM1_MFRC_P_PPE;
			/* Other MAC address frames forward to PPE */
			data |= GDM1_OFRC_P_PPE;

		} else {
			data &= ~0x7777;
			/* Uni-cast frames forward to CPU */
			data |= GDM1_UFRC_P_CPU;
			/* Broad-cast MAC address frames forward to CPU */
			data |= GDM1_BFRC_P_CPU;
			/* Multi-cast MAC address frames forward to CPU */
			data |= GDM1_MFRC_P_CPU;
			/* Other MAC address frames forward to CPU */
			data |= GDM1_OFRC_P_CPU;
		}
		//reg_write(FE_GDMA2_FWD_CFG, data);
	}

	return 0;
}

static int is_cah_ctrl_request_done(u32 ppe_id)
{
	int count = 1000;

	if (ppe_id == 0) {

		/* waiting for 1sec to make sure action was finished */
		do {
			if ((reg_read(CAH_CTRL) & 0x100) == 0)
				return 1;
			udelay(1000);
		} while (--count);

	} else if (ppe_id == 1) {

		/* waiting for 1sec to make sure action was finished */
		do {
			if ((reg_read(CAH_CTRL_PPE1) & 0x100) == 0)
				return 1;
			udelay(1000);
		} while (--count);

	}

	return 0;
}

static void ppe_write_cache_line(u32 ppe_id, u32 line, u32 tag, u32 state)
{
	if (ppe_id == 0) {

		/* write tag filed of the cache line */
		reg_modify_bits(CAH_LINE_RW, line, 0, 16);
		reg_modify_bits(CAH_LINE_RW, 0x1F, 16, 8);
		reg_modify_bits(CAH_CTRL, 0, 18, 2);
		reg_write(CAH_WDATA, (state << 16) | tag);

		/* software access cache command = write */
		reg_modify_bits(CAH_CTRL, 3, 12, 2);

		/* trigger software access cache request */
		reg_modify_bits(CAH_CTRL, 1, 8, 1);

		if (!is_cah_ctrl_request_done(ppe_id))
			pr_info("%s write tag timeout\n", __func__);

	} else if (ppe_id == 1) {


		/* write tag filed of the cache line */
		reg_modify_bits(CAH_LINE_RW_PPE1, line, 0, 16);
		reg_modify_bits(CAH_LINE_RW_PPE1, 0x1F, 16, 8);
		reg_modify_bits(CAH_CTRL_PPE1, 0, 18, 2);
		reg_write(CAH_WDATA_PPE1, (state << 16) | tag);

		/* software access cache command = write */
		reg_modify_bits(CAH_CTRL_PPE1, 3, 12, 2);

		/* trigger software access cache request */
		reg_modify_bits(CAH_CTRL_PPE1, 1, 8, 1);

		if (!is_cah_ctrl_request_done(ppe_id))
			pr_info("%s write tag timeout\n", __func__);
	}

}


static void ppe_cache_clear(u32 ppe_id)
{
	u32 flow_cfg;
	u32 scan_mode;
	u32 cah_en;

	spin_lock_bh(&ppe_cache_lock);

	if (ppe_id == 0) {
		/* disable table learning */
		flow_cfg = reg_read(PPE_FLOW_SET);
		reg_write(PPE_FLOW_SET, 0);

		/* wait PPE return to idle */
		udelay(1);

		/* disable scan mode */
		scan_mode = ((reg_read(PPE_FOE_CFG) & 0x30000) >> 16);
		reg_modify_bits(PPE_FOE_CFG, 0, 16, 2);

		/* disable cache */
		cah_en = reg_read(CAH_CTRL) & 0x1;
		reg_modify_bits(CAH_CTRL, 0, 0, 1);

		/* invalidate PPE cache lines */
		reg_modify_bits(CAH_CTRL, 1, 9, 1);
		reg_modify_bits(CAH_CTRL, 0, 9, 1);

		/* lock the preserved cache line */
		ppe_write_cache_line(ppe_id, 0, 0x7FFF, 3);

		/* restore cache enable */
		reg_modify_bits(CAH_CTRL, cah_en, 0, 1);
		/* restore scan mode */
		reg_modify_bits(PPE_FOE_CFG, scan_mode, 16, 2);
		/* restore table learning */
		reg_write(PPE_FLOW_SET, flow_cfg);
	} else if (ppe_id == 1) {

		/* disable table learning */
		flow_cfg = reg_read(PPE1_FLOW_SET);
		reg_write(PPE1_FLOW_SET, 0);

		/* wait PPE return to idle */
		udelay(1);

		/* disable scan mode */
		scan_mode = ((reg_read(PPE1_FOE_CFG) & 0x30000) >> 16);
		reg_modify_bits(PPE1_FOE_CFG, 0, 16, 2);

		/* disable cache */
		cah_en = reg_read(CAH_CTRL_PPE1) & 0x1;
		reg_modify_bits(CAH_CTRL_PPE1, 0, 0, 1);

		/* invalidate PPE cache lines */
		reg_modify_bits(CAH_CTRL_PPE1, 1, 9, 1);
		reg_modify_bits(CAH_CTRL_PPE1, 0, 9, 1);

		/* lock the preserved cache line */
		ppe_write_cache_line(ppe_id, 0, 0x7FFF, 3);

		/* restore cache enable */
		reg_modify_bits(CAH_CTRL_PPE1, cah_en, 0, 1);
		/* restore scan mode */
		reg_modify_bits(PPE1_FOE_CFG, scan_mode, 16, 2);
		/* restore table learning */
		reg_write(PPE1_FLOW_SET, flow_cfg);

	}

	spin_unlock_bh(&ppe_cache_lock);

}

void ppe_set_cache_ebl(u32 ppe_index)
{
	if (ppe_index == 0) {
		/* clear cache table before enabling cache */
		ppe_cache_clear(ppe_index);

		/* Cache enable */
		reg_modify_bits(CAH_CTRL, 1, 0, 1);

	} else if (ppe_index == 1) {

		/* clear cache table before enabling cache */
		ppe_cache_clear(ppe_index);

		/* Cache enable */
		reg_modify_bits(CAH_CTRL_PPE1, 1, 0, 1);
	}
}

void ppe_cache_lock_init(void)
{
	spin_lock_init(&ppe_cache_lock);
}


void ppe_set_mtu(void)
{

#ifdef CONFIG_MTK_WAN_MTU_SIZE
	u32 mtu_vlan0, mtu_vlan1;

	if (CONFIG_MTK_WAN_MTU_SIZE == 1570 || CONFIG_MTK_WAN_MTU_SIZE == 2000) {

		mtu_vlan0 = CONFIG_MTK_WAN_MTU_SIZE + ETH_HLEN;
		mtu_vlan1 = mtu_vlan0 + VLAN_HLEN;

		reg_modify_bits(PPE_MTU_VLYR_0, mtu_vlan1, 16, 16);
		reg_modify_bits(PPE_MTU_VLYR_0, mtu_vlan0, 0, 16);

		reg_modify_bits(PPE1_MTU_VLYR_0, mtu_vlan1, 16, 16);
		reg_modify_bits(PPE1_MTU_VLYR_0, mtu_vlan0, 0, 16);
	}
#endif
}


void ppe_set_ip_prot(void)
{
	/* IP Protocol Field for IPv4 NAT or IPv6 3-tuple flow */
	/* Don't forget to turn on related bits in PPE_IP_PROT_CHK register if you want to support
	 * another IP protocol.
	 */
	/* FIXME: enable it to support IP fragement */
	reg_write(PPE_IP_PROT_CHK, 0xFFFFFFFF);	/* IPV4_NXTH_CHK and IPV6_NXTH_CHK */

	if (pptp_enable == 1)
		reg_modify_bits(PPE_IP_PROT_0, IPPROTO_GRE, 0, 8);

	/* reg_modify_bits(PPE_IP_PROT_0, IPPROTO_TCP, 8, 8); */
	/* reg_modify_bits(PPE_IP_PROT_0, IPPROTO_UDP, 16, 8); */
	/* reg_modify_bits(PPE_IP_PROT_0, IPPROTO_IPV6, 24, 8); */
	reg_write(PPE1_IP_PROT_CHK, 0xFFFFFFFF);

}

int ppe_fill_act_mode(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{
	if (IS_IPV4_HNAT(entry) || IS_IPV4_HNAPT(entry)) {
		entry->ipv4_hnapt.act_mode = HWNAT_NAPT;

		if ((entry->ipv4_hnapt.sport == entry->ipv4_hnapt.new_sport) &&
		    (entry->ipv4_hnapt.dport == entry->ipv4_hnapt.new_dport)) {
			if ((entry->ipv4_hnapt.sip == entry->ipv4_hnapt.new_sip) &&
			    (entry->ipv4_hnapt.dip == entry->ipv4_hnapt.new_dip))
				entry->ipv4_hnapt.act_mode = HWNAT_BRIDGE;
			else
				entry->ipv4_hnapt.act_mode = HWNAT_NAT;
		}
		return 0;
	}

	if (fe_feature & HNAT_IPV6) {
		if (IS_IPV4_DSLITE(entry))
			entry->ipv4_dslite.act_mode = HWNAT_ROUTE;
		else if (IS_IPV6_3T_ROUTE(entry))
			entry->ipv6_3t_route.act_mode = HWNAT_ROUTE;
		else if (IS_IPV6_5T_ROUTE(entry))
			entry->ipv6_5t_route.act_mode = HWNAT_ROUTE;
		else if (IS_IPV6_6RD(entry))
			entry->ipv6_6rd.act_mode = HWNAT_ROUTE;
		else
			if (debug_level >= 6)
				pr_notice("get packet format something wrong\n");
	}

	return 0;
}

int ppe_fill_table_med(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{

	/* get start addr for each layer */
	if (ppe_parse_layer_med(skb, entry,ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}


	/* Set Layer2 Info */
	if (ppe_fill_L2_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	/* Set Layer3 Info */
	if (ppe_fill_L3_info_med(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	/* Set Layer4 Info */
	if (ppe_fill_L4_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	if (is_tunnel_port(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		return 1;
	}

	return 0;
}

int ppe_fill_table(struct sk_buff *skb, struct foe_entry *entry, struct pkt_parse_result *ppe_parse_result)
{

	/* get start addr for each layer */
	if (ppe_parse_layer_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}


	/* Set Layer2 Info */
	if (ppe_fill_L2_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	/* Set Layer3 Info */
	if (ppe_fill_L3_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	/* Set Layer4 Info */
	if (ppe_fill_L4_info(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}

	/* Set ACT mode */
	ppe_fill_act_mode(skb, entry, ppe_parse_result);

	if (is_tunnel_port(skb, entry, ppe_parse_result)) {
		if (debug_level >= 6)
			DD;
		return 1;
	}

	return 0;
}

int check_entry_region(struct sk_buff *skb)
{
	u8 which_region;


	which_region = tx_decide_which_region(skb);

	//if (debug_level >= 6)
		//pr_notice(" which_region = %d\n", which_region);

	if (which_region == ALL_INFO_ERROR) {
		if (debug_level >= 10)
			pr_notice("ppe_tx_handler : ALL_INFO_ERROR\n");
		return 1;
	}

	if (FOE_ENTRY_NUM(skb) >= (FOE_4TB_SIZ - 1))
		return 1;


	if (skb->mark & 0x40000000) {
		skb->mark &= (~0x40000000);
		FOE_ALG(skb) = 1;
	}

	return 0;
}

int check_magic_tag_valid(struct sk_buff *skb)
{
	if(is_magic_tag_protect_valid(skb))
		return 0;
	else
		return 1;
}

int check_use_UDP_3T(struct sk_buff *skb, struct foe_entry *entry)
{
#ifdef CONFIG_SUPPORT_WLAN_OPTIMIZE
		if (bridge_lan_subnet(skb)) {
			if (!get_skb_interface(skb))
				USE_3T_UDP_FRAG = 0;
			else
				USE_3T_UDP_FRAG = 1;
			if (USE_3T_UDP_FRAG == 0)
				return 1;
		} else {
			USE_3T_UDP_FRAG = 0;
		}
#else
#if(0)
			if (bridge_lan_subnet(skb))
				USE_3T_UDP_FRAG = 1;
			else
				USE_3T_UDP_FRAG = 0;
#endif
#endif

	return 0;
}

void clear_mib_count(struct sk_buff *skb, int pse_port)
{
	int count = 100000;

	if ((pse_port == WDMA1_PSE_PORT) || (pse_port == EDMA1_PSE_PORT))
	{
		reg_write(MIB_SER_CR_PPE1, FOE_ENTRY_NUM(skb) | (1 << 16));
		do {
			if (!((reg_read(MIB_SER_CR_PPE1) & 0x10000) >> 16))
				break;
				/* usleep_range(100, 110); */
		} while (--count);
		reg_read(MIB_SER_R0_PPE1);
		reg_read(MIB_SER_R1_PPE1);
		reg_read(MIB_SER_R2_PPE1);
	#if defined(CONFIG_HNAT_V2)
		reg_read(MIB_SER_R3_PPE1);
	#endif /* CONFIG_HNAT_V2 */
	} else {
		reg_write(MIB_SER_CR, FOE_ENTRY_NUM(skb) | (1 << 16));
		do {
			if (!((reg_read(MIB_SER_CR) & 0x10000) >> 16))
				break;
				/* usleep_range(100, 110); */
		} while (--count);
		reg_read(MIB_SER_R0);
		reg_read(MIB_SER_R1);
		reg_read(MIB_SER_R2);
	#if defined(CONFIG_HNAT_V2)
		reg_read(MIB_SER_R3);
	#endif /* CONFIG_HNAT_V2 */
	}
}

int ppe_common_part_med(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result, int eth_pdu)
{
	int ret;

	ret = 0;

#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
		if (get_done_bit(skb, entry) != 0)
			return 1;
#endif

	ret = check_use_UDP_3T(skb, entry);

	//dvt use;
	ret = 0;
#if(0)
	if (ret)
		return ret;
#endif

	if (!eth_pdu)
		ret = ppe_fill_table_med(skb, entry, ppe_parse_result);
	else
		ret = ppe_fill_table(skb, entry, ppe_parse_result);

	if (ret)
		return ret;

	if ((fe_feature & HNAT_QDMA) && (fe_feature & HNAT_MCAST)) {
		if (ppe_parse_result->is_mcast) {
			//foe_mcast_entry_qid(ppe_parse_result->vlan1,
					   // ppe_parse_result->dmac,
					    //M2Q_table[skb->mark]);
			//foe_mcast_entry_qid(ppe_parse_result->vlan1,
					    //ppe_parse_result->dmac,
					    //0);
			}
	}
	if (fe_feature & PPE_MIB)
		clear_mib_count(skb, gmac_no);



	return 0;
}

int ppe_common_part(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret;

	ret = 0;

#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
		if (get_done_bit(skb, entry) != 0)
			return 1;
#endif

	ret = check_use_UDP_3T(skb, entry);

	//dvt use;
	ret = 0;
#if(0)
	if (ret)
		return ret;
#endif

	ret = ppe_fill_table(skb, entry, ppe_parse_result);

	if (ret)
		return ret;

	if ((fe_feature & HNAT_QDMA) && (fe_feature & HNAT_MCAST)) {
		if (ppe_parse_result->is_mcast) {
			//foe_mcast_entry_qid(ppe_parse_result->vlan1,
					   // ppe_parse_result->dmac,
					    //M2Q_table[skb->mark]);
			//foe_mcast_entry_qid(ppe_parse_result->vlan1,
					    //ppe_parse_result->dmac,
					    //0);
			}
	}
	if (fe_feature & PPE_MIB)
		clear_mib_count(skb, gmac_no);

	return 0;
}

void set_entry_done(struct sk_buff *skb, struct foe_entry *entry)
{
	struct foe_entry *entry_output;

	//entry->ipv4_hnapt.udib1.ilgf = 1;

	ppe_set_entry_bind(skb, entry); /* Enter binding state */

#ifdef CONFIG_HW_NAT_SEMI_AUTO_MODE
	set_ppe_table_done(entry);
#endif

	/* Fill to ppe entry */
	entry_output = decide_which_ppe(skb);
	if (entry_output == NULL)
		return;

	*entry_output = *entry;

	/*make sure data write to dram*/
	wmb();

	/* Dump Binding Entry */
	if (debug_level >= 1)
		foe_dump_entry(FOE_ENTRY_NUM(skb), entry, -1);
}


int ppe_common_eth(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret;

	ret = ppe_common_part(skb, entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;

	if (fe_feature & HNAT_QDMA) {
		set_ppe_qid(skb, entry);
		set_eth_fqos(skb, entry, gmac_no);
	}

	if (fe_feature & GE2_SUPPORT)
		ret = set_eth_dp_gmac2(entry, gmac_no, ppe_parse_result);
	else
		ret = set_eth_dp_gmac1(entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;
	/* For force to cpu handler, record if name */
	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

	/* create a work queue to do it */
	set_eth_auto_qos(skb, gmac_no);

	return 0;
}

int ppe_common_wifi(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret;

	ret = ppe_common_part(skb, entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;

	/* Set force port info */
	set_wifi_info(skb, entry, gmac_no, ppe_parse_result);

	/* For force to cpu handler, record if name */
	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

	return 0;
}

int ppe_common_modem(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result, int eth_pdu)
{
	int ret;

	ret = ppe_common_part_med(skb, entry, gmac_no, ppe_parse_result, eth_pdu);

	if (ret)
		return ret;

	/* Set force port info */
	set_modem_info(skb, entry, gmac_no, ppe_parse_result);

	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

	return 0;
}

int ppe_common_rndis(struct sk_buff *skb, struct foe_entry *entry,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret, gmac_no = EDMA0_PSE_PORT, dr_idx = 0;

	if (!ppe_hnat_fast_init_done()) {
		gmac_no = ADMA_PSE_PORT;
		dr_idx = 0;
	} else if (rndis_bind_count % rndis_mod == 0) {
		gmac_no = EDMA0_PSE_PORT;
		dr_idx = 0;
	} else if (rndis_bind_count % rndis_mod == 1) {
		gmac_no = EDMA1_PSE_PORT;
		dr_idx = 0;
	}
#if defined(CONFIG_HNAT_V2)
	else if (rndis_bind_count % rndis_mod == 2) {
		gmac_no = EDMA0_PSE_PORT;
		dr_idx = 1;
	} else if (rndis_bind_count % rndis_mod == 3) {
		gmac_no = EDMA1_PSE_PORT;
		dr_idx = 1;
	}
#endif /* CONFIG_HNAT_V2 */

	ret = ppe_common_part(skb, entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;

	rndis_bind_count ++;

	/* Set force port info */
	set_rndis_info(skb, entry, gmac_no, dr_idx, ppe_parse_result);

	/* For force to cpu handler, record if name */
	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

	return 0;
}


int ppe_common_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret;

	ret = ppe_common_part(skb, entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;

	/* Set force port info */
	set_fast_path_info_ext(skb, entry, gmac_no, ppe_parse_result);

	/* For force to cpu handler, record if name */
	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

#ifdef CONFIG_RAETH_EDMA
	foe_dump_pkt(skb, entry);
#endif

	return 0;
}

int ppe_common_snps(struct sk_buff *skb, struct foe_entry *entry, int gmac_no,
				    struct pkt_parse_result *ppe_parse_result)
{
	int ret;

	ret = ppe_common_part(skb, entry, gmac_no, ppe_parse_result);

	if (ret)
		return ret;

	/* Set force port info */
	set_snps_info(skb, entry, gmac_no, ppe_parse_result);

	/* For force to cpu handler, record if name */
	if (ppe_set_ext_if_num(skb, entry)) {
		memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
		return 1;
	}
	set_entry_done(skb, entry);

	return 0;
}

int set_pre_bind(struct sk_buff *skb,struct foe_entry *entry)
{
/*#ifdef PREBIND_TEST*/
/*		if (jiffies % 2 == 0) {*/
/*			pr_notice("drop prebind packet jiffies=%lu\n", jiffies);*/
/*			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);*/
/*			return 0;*/
/*		}*/
/*#endif*/
	if (entry->udib1.preb && entry->bfib1.state != BIND) {
		entry->bfib1.state = BIND;
		entry->udib1.preb = 0;
		/* Dump Binding Entry */
		if (debug_level >= 1) {
			foe_dump_entry(FOE_ENTRY_NUM(skb), entry, -1);
		} else {
			/* drop duplicate prebind notify packet */
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}
	}

	return 1;
}

int ppe_modem_fill_dummy_info(struct sk_buff *skb, int rx)
{
	unsigned char mac[ETH_ALEN] = {0x00,0x55,0x7B,0xB5,0x7D,0xF7};
	struct ethhdr *eth;
	struct iphdr *iph;

	/* Starts from L3 header */

	if (debug_level >= 3) {
		pr_notice("%s: FOE_IF_IDX=%d headroom=%d tailroom=%d\n",
			__func__, FOE_IF_IDX(skb), skb_headroom(skb), skb_tailroom(skb));
	}

	if (FOE_IF_IDX(skb) == 0) {
		pr_err("%s: invalid port: %d\n", __func__, FOE_IF_IDX(skb));
		return 1;
	}

	if (dst_port[FOE_IF_IDX(skb)] == NULL) {
		pr_err("%s: unregistered port: %d\n", __func__, FOE_IF_IDX(skb));
		return 1;
	}

	/* Fill in dev, mac_header, and dummy eth_header */
	if (rx) {
		skb->dev = dst_port[FOE_IF_IDX(skb)];
		skb_set_mac_header(skb, -ETH_HLEN);
		eth = eth_hdr(skb);
		memcpy(eth->h_dest, skb->dev->dev_addr, ETH_ALEN);
		memcpy(eth->h_source, mac, ETH_ALEN);
	} else {
		skb_set_mac_header(skb, -ETH_HLEN);
		eth = eth_hdr(skb);
		memcpy(eth->h_dest, mac, ETH_ALEN);
		memcpy(eth->h_source, skb->dev->dev_addr, ETH_ALEN);
	}

	skb_set_network_header(skb, 0);
	iph = ip_hdr(skb);
	if (iph->version == IPVERSION)
		eth->h_proto = htons(ETH_P_IP);
	else
		eth->h_proto = htons(ETH_P_IPV6);

	/* Fill in skb protocol and pkt_type */
	if (rx) {
		skb->protocol = eth->h_proto;
		skb->pkt_type = PACKET_HOST;
		/* Ends with L3 header */
	} else {
		//skb_push(skb, -skb_mac_offset(skb));
		/* Ends with L3 header */
	}

	return 0;
}

int tx_cpu_handler_rndis(struct sk_buff *skb, struct foe_entry *entry)
{
	int ret;
	struct pkt_parse_result ppe_parse_result;

	if (debug_level >= 17) {
		if (FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_term_tx(skb, entry, 0)) /* usb<-modem */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_rndis(skb, entry, &ppe_parse_result);

		if (ret)
			return ret;
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {

			if (debug_level >= 3)
				pr_notice("USB Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}
	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3)
			NAT_PRINT("FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
	}

	return 1;
}

int tx_cpu_handler_wifi(struct sk_buff *skb, struct foe_entry *entry, int gmac_no)
{
	int ret;
	struct pkt_parse_result ppe_parse_result;

	if (debug_level >= 17) {
		if (FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_term_tx(skb, entry, gmac_no)) /* wifi<-modem */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_wifi(skb, entry, gmac_no, &ppe_parse_result);

		if (ret)
			return ret;
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {

			if (debug_level >= 3)
				pr_notice("Wifi Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}
	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3)
			NAT_PRINT("FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
	}

	return 1;
}

int tx_cpu_handler_modem(struct sk_buff *skb, struct foe_entry *entry, int gmac_no, int eth_pdu)
{
	int ret;
	struct pkt_parse_result ppe_parse_result;

	if (debug_level >= 10) {
		pr_notice("%s,  cpu_reason = %x, gmac_no = %x FOE_ALG(skb) = %x eth_pdu = %d!!\n",
				__func__, FOE_AI(skb), gmac_no, FOE_ALG(skb), eth_pdu);
		if (debug_level >= 17 && FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_init_tx(skb, entry, gmac_no)) /* pci->modem */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_modem(skb, entry, gmac_no, &ppe_parse_result, eth_pdu);

		if (ret)
			return ret;
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {
			if (debug_level >= 3)
				pr_notice("Modem Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}

	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3) {
			//FOE_INFO_DUMP(skb);
			NAT_PRINT("tx_cpu_handler_modem : FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
		}
	}
	return 1;
}

int tx_cpu_handler_eth(struct sk_buff *skb, struct foe_entry *entry, int gmac_no)
{
	int ret;
	struct pkt_parse_result ppe_parse_result;

	if (debug_level >=10)
		pr_notice("%s,  cpu_reason =0x%x, gmac_no = %x FOE_ALG(skb) = %x, dev = %s \n", __func__, FOE_AI(skb), gmac_no, FOE_ALG(skb), skb->dev->name);
	if (debug_level >= 17) {
		if (FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_init_tx(skb, entry, gmac_no)) /* eth->eth */
		return 1; /* tx out */

	if (ppe_tunnel_term_tx(skb, entry, gmac_no)) /* eth<-eth */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */


	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_eth(skb, entry, gmac_no, &ppe_parse_result);

		if (ret)
			return ret;

	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {
			if (debug_level >= 3)
				pr_notice("ETH Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}
	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3)
			NAT_PRINT("FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
	}
#ifdef CONFIG_RA_HW_NAT_PREBIND
		if (FOE_AI(skb) == HIT_PRE_BIND)
			return set_pre_bind(skb, entry);
#endif

	return 1;
}

int tx_cpu_handler_ext(struct sk_buff *skb, struct foe_entry *entry, int gmac_no)
{
	int ret;
	struct pkt_parse_result ppe_parse_result;

	if (debug_level >= 17) {
		if (FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_term_tx(skb, entry, gmac_no)) /* pci<-modem */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_ext(skb, entry, gmac_no, &ppe_parse_result);

		if (ret)
			return ret;
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {
			if (debug_level >= 3)
				pr_notice("ext Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}

	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3)
			NAT_PRINT("FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
	}
#ifdef CONFIG_RA_HW_NAT_PREBIND
		if (FOE_AI(skb) == HIT_PRE_BIND)
			return set_pre_bind(skb, entry);
#endif

	return 1;
}

int tx_cpu_handler_snps(struct sk_buff *skb, struct foe_entry *entry)
{
	int ret, gmac_no;
	struct pkt_parse_result ppe_parse_result;

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
	gmac_no = EDMA0_PSE_PORT;
#else
	gmac_no = ADMA_PSE_PORT;
#endif

	if (debug_level >= 17) {
		if (FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt_tx(skb, entry);
	}

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_init_tx(skb, entry, gmac_no)) /* L2TP initiator */
		return 1; /* tx out */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
	    (FOE_ALG(skb) == 0)) {

		ret = ppe_common_snps(skb, entry, gmac_no, &ppe_parse_result);

		if (ret)
			return ret;
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {

		if (ppe_entry_check_state(entry, BIND)) {
			if (debug_level >= 3)
				pr_notice("SNPS Tx Got HITBIND_KEEPALIVE_DUP_OLD packet (%s,%d)\n", skb->dev->name,
					FOE_ENTRY_NUM(skb));
			memset(FOE_INFO_START_ADDR(skb), 0, FOE_INFO_LEN);
			return 0;
		}
	} else if ((FOE_AI(skb) == HIT_UNBIND_RATE_REACH) &&
		  (FOE_ALG(skb) == 1)) {
		if (debug_level >= 3)
			NAT_PRINT("FOE_ALG=1 (Entry=%d)\n", FOE_ENTRY_NUM(skb));
	}

	return 1;
}

void rx_debug_log(struct sk_buff *skb)
{
	struct foe_entry *entry;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return;

	if (debug_level >= 7) {
		hnat_cpu_reason_cnt(skb);
		if (debug_level >= 17 && FOE_AI(skb) == dbg_cpu_reason)
			foe_dump_pkt(skb, entry);
	}
}

int rx_cpu_handler_modem_thread(struct sk_buff *skb)
{
    struct foe_entry *entry;

	entry = decide_which_ppe(skb);

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_term_rx(skb, entry)) /* pci<-modem */
		return 0; /* release skb */
#endif /* CONFIG_TUNNEL_FAST_PATH */
    return 1;
}


int rx_cpu_handler_modem(struct sk_buff *skb)
{
	if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {
		if (debug_level >= 3)
			pr_notice("MODEM Rx Got HITBIND_KEEPALIVE_DUP_OLD packet (%d)\n",
				FOE_ENTRY_NUM(skb));
		return 1;
	}

	return 1;
}

int rx_cpu_handler_eth(struct sk_buff *skb)
{
	//struct foe_entry *entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];

	struct foe_entry *entry;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_init_rx(skb, entry)) /* eth->eth */
		return 0; /* release skb */

	if (ppe_tunnel_term_rx(skb, entry)) /* eth<-eth */
		return 0; /* release skb */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	if (FOE_AI(skb) == HIT_BIND_FORCE_TO_CPU) {

		return hitbind_force_to_cpu_handler(skb, entry);
		/* handle the incoming packet which came back from PPE */
	} else if ((fe_feature & AUTO_HNAT) && (FOE_AI(skb) == PACKET_FORWARD_PATH_WITHOUT_PPE) && (FOE_SP(skb) == QDMA_RX)) {

		return hitbind_force_to_cpu_handler(skb, entry);
		/* handle the incoming packet which came back from QDMA */
	} else if ((is_if_pcie_wlan_rx(skb) && ((FOE_SP(skb) == 0) || (FOE_SP(skb) == 5) || (FOE_SP(skb) == 11) || (FOE_SP(skb) == 12))) &&
		   (FOE_AI(skb) != HIT_BIND_KEEPALIVE_DUP_OLD_HDR)) {
		return ppe_extif_pingpong_handler(skb);
	} else if (FOE_AI(skb) == HIT_BIND_MULTICAST_TO_CPU ||
		   FOE_AI(skb) == HIT_BIND_MULTICAST_TO_GMAC_CPU) {
		return hitbind_force_mcast_to_wifi_handler(skb);
	} else if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {
		if (debug_level >= 3)
			pr_notice("ETH RxGot HIT_BIND_KEEPALIVE_DUP_OLD_HDR packet (hash index=%d)\n",
				FOE_ENTRY_NUM(skb));
		keep_alive_old_pkt_handler(skb);
		/*change to multicast packet, make bridge not learn this packet */
		/*after kernel-2.6.36 src mac = multicast will drop by bridge,*/
		/*so we need recover correcet interface*/
		/*eth->h_source[0] = 0x1;*/

		return 1;
	}

	return 1;
}

int rx_cpu_handler_wifi(struct sk_buff *skb)
{
	struct foe_entry *entry; // = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	//int sw_fast_path;
	/*struct ethhdr *eth = (struct ethhdr *)(skb->data - ETH_HLEN);*/

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 1;

	if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR) {
		if (debug_level >= 3)
			pr_notice("WIFI Rx Got HITBIND_KEEPALIVE_DUP_OLD packet (%d)\n",
				FOE_ENTRY_NUM(skb));
	}

	if (debug_level >= 10)
		pr_notice("%s, FOE_MAGIC_TAG(skb) = 0x%x, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n", __func__, FOE_MAGIC_TAG(skb), FOE_AI(skb), FOE_SP(skb));

	if((FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED0) ||
	   (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED1) ||
	   (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED2))
		return 1;

	/* the incoming packet is from PCI or WiFi interface */
	/* if (is_if_pcie_wlan_rx(skb)) { */
		/* return ppe_extif_rx_handler(skb); */
	if ((FOE_MAGIC_TAG(skb) == FOE_MAGIC_PCI) ||
	    (FOE_MAGIC_TAG(skb) == FOE_MAGIC_RNDIS)) {
/*		if(fe_feature & HNAT_IPI)*/
/*			return HnatIPIExtIfHandler(skb);*/
		return ppe_extif_rx_handler(skb);
	} else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WLAN) {
		return ppe_hnat_fast_send_skb(skb);
	} else if (FOE_AI(skb) == HIT_BIND_FORCE_TO_CPU) {
/*		if(fe_feature & HNAT_IPI)*/
/*			return HnatIPIForceCPU(skb);*/

		return hitbind_force_to_cpu_handler(skb, entry);

		/* handle the incoming packet which came back from PPE */
	} else if ((is_if_pcie_wlan_rx(skb) && ((FOE_SP(skb) == 0) || (FOE_SP(skb) == 5))) &&
		   (FOE_AI(skb) != HIT_BIND_KEEPALIVE_DUP_OLD_HDR)) {
		return ppe_extif_pingpong_handler(skb);
	}
	return 1;
}

int rx_cpu_handler_ext(struct sk_buff *skb)
{
	struct foe_entry *entry = NULL;

	if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0 ||
	    FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1)
		entry = decide_which_ppe(skb);

#ifdef CONFIG_TUNNEL_FAST_PATH
	if (ppe_tunnel_init_rx(skb, entry)) /* pci->modem */
		return 0; /* release skb */
#endif /* CONFIG_TUNNEL_FAST_PATH */

	/* the incoming packet is from PCI */
	if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_PCI) {

		if (debug_level >= 7)
			pr_notice("%s, FOE_MAGIC_PCI go ppe_extif_rx_handler!\n", __func__);

		return ppe_extif_rx_handler(skb);

        } else if (FOE_AI(skb) == HIT_BIND_FORCE_TO_CPU) {

		if (debug_level >= 7) {
			pr_notice("%s, HIT_BIND_FORCE_TO_CPU, FOE_SP(skb):%d\n", __func__, FOE_SP(skb));
		}
		entry = decide_which_ppe(skb);
		if (entry == NULL)
			return 1;

		return hitbind_force_to_cpu_handler(skb, entry);

		/* handle the incoming packet which came back from PPE */
	} else if ((is_if_pcie_wlan_rx(skb) && ((FOE_SP(skb) == 11) || (FOE_SP(skb) == 12)))  &&
		   (FOE_AI(skb) != HIT_BIND_KEEPALIVE_DUP_OLD_HDR)) {
		if (debug_level >= 7) {
			pr_notice("%s, FOE_SP(skb):%d, FOE_AI(skb):0x%x, SP match to handler\n",
				__func__, FOE_SP(skb), FOE_AI(skb));
		}
		return ppe_extif_pingpong_handler(skb);
	}
	return 1;
}

int rx_cpu_handler_rndis(struct sk_buff *skb)
{
	/* the incoming packet is from USB interface */
	if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_RNDIS) {

	#ifdef CONFIG_TUNNEL_FAST_PATH
		if (ppe_tunnel_init_rx(skb, NULL)) /* usb->modem */
			return 0; /* release skb */
	#endif /* CONFIG_TUNNEL_FAST_PATH */

		return ppe_extif_rx_handler(skb);

	}
	return 1;
}

int rx_cpu_handler_snps(struct sk_buff *skb)
{
	/* the incoming packet is from SNPS mac */
	if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_SNPS) {

	#ifdef CONFIG_TUNNEL_FAST_PATH
		if (ppe_tunnel_init_rx(skb, NULL)) /* SWNAT and GRE initiator */
			return 0; /* release skb */

		if (ppe_tunnel_term_rx(skb, NULL)) /* SWNAT and L2TP termintator */
			return 0; /* release skb */
	#endif /* CONFIG_TUNNEL_FAST_PATH */

#ifdef CONFIG_MTK_HNAT_FAST_SUPPORT
		return ppe_hnat_fast_send_skb(skb);
#else
		return ppe_snps_rx_handler(skb);
#endif
	}
	return 1;
}

void foe_format_create(struct sk_buff *skb)
{

	if (IS_SPACE_AVAILABLE_HEAD(skb)) {
		FOE_TAG_PROTECT_HEAD(skb) = TAG_PROTECT;
	#if !defined(CONFIG_MTK_ADMAV2)
		if (FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_GE || FOE_MAGIC_TAG_HEAD(skb) == FOE_MAGIC_EDMARX) {

			u32 alg_tmp, sp_tmp, entry_tmp, ai_tmp;

			alg_tmp = 0;
			sp_tmp = FOE_SP_HEAD(skb);
			entry_tmp = FOE_ENTRY_NUM_HEAD(skb);
			ai_tmp = FOE_AI_HEAD(skb);
			FOE_SP(skb) = sp_tmp & 0xf;
			FOE_ENTRY_NUM(skb) = entry_tmp & 0x7fff;
			FOE_AI(skb) = ai_tmp & 0x1f;
			FOE_ALG(skb) = alg_tmp & 0x1;

		}
	#endif	/* !defined(CONFIG_MTK_ADMAV2) */
		if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED0)
			FOE_SP(skb) = WDMA0_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED1)
			FOE_SP(skb) = WDMA1_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED2)
			FOE_SP(skb) = WDMA2_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_MED)
			FOE_SP(skb) = MDMA_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0)
			FOE_SP(skb) = EDMA0_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1)
			FOE_SP(skb) = EDMA1_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_RNDIS)
			FOE_SP(skb) = ADMA_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_SNPS)
			FOE_SP(skb) = ADMA_PSE_PORT;
	}
	if (IS_SPACE_AVAILABLE_TAIL(skb)) {
		FOE_ALG_TAIL(skb) = 0;
		FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
		FOE_ENTRY_NUM_MSB_TAIL(skb) = FOE_ENTRY_NUM(skb) & 0x3fff;
		FOE_ENTRY_NUM_LSB_TAIL(skb) = (FOE_ENTRY_NUM(skb) & 0x4000) >> 14;
		FOE_AI_TAIL(skb) = FOE_AI(skb);
		if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED0)
			FOE_SP_TAIL(skb) = WDMA0_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED1)
			FOE_SP_TAIL(skb) = WDMA1_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_WED2)
			FOE_SP_TAIL(skb) = WDMA2_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_MED)
			FOE_SP_TAIL(skb) = MDMA_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA0)
			FOE_SP_TAIL(skb) = EDMA0_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_EDMA1)
			FOE_SP_TAIL(skb) = EDMA1_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_RNDIS)
			FOE_SP_TAIL(skb) = ADMA_PSE_PORT;
		else if (FOE_MAGIC_TAG(skb) == FOE_MAGIC_SNPS)
			FOE_SP_TAIL(skb) = ADMA_PSE_PORT;
	}
}

void pse_set_reserve_q(void)
{
	/* port 5,8,9,10 input queue */
	reg_modify_bits(PSE_IQ_REV3, PSE_Q_RES, 16, 8);
	reg_modify_bits(PSE_IQ_REV5, PSE_Q_RES, 0, 8);
	reg_modify_bits(PSE_IQ_REV5, PSE_Q_RES, 16, 8);
	reg_modify_bits(PSE_IQ_REV6, PSE_Q_RES, 0, 8);

	/* port 1,2,6,8,9,10,11,12 output queue */
	reg_modify_bits(PSE_OQ_TH1, PSE_Q_RES, 16, 8);
	reg_modify_bits(PSE_OQ_TH2, PSE_Q_RES, 0, 8);
	reg_modify_bits(PSE_OQ_TH4, PSE_Q_RES, 0, 8);
	reg_modify_bits(PSE_OQ_TH5, PSE_Q_RES, 0, 8);
	reg_modify_bits(PSE_OQ_TH5, PSE_Q_RES, 16, 8);
	reg_modify_bits(PSE_OQ_TH6, PSE_Q_RES, 0, 8);
	reg_modify_bits(PSE_OQ_TH6, PSE_Q_RES, 16, 8);
	reg_modify_bits(PSE_OQ_TH7, PSE_Q_RES, 0, 8);

#if defined(CONFIG_HNAT_V2)
	/* port 1, 2, 5, 8, 9, 10, 11, 12 IQ RLS */
	reg_modify_bits(PSE_IQ_REV_RLS1, PSE_IQ_RLS, 16, 8);
	reg_modify_bits(PSE_IQ_REV_RLS2, PSE_IQ_RLS, 0, 8);
	reg_modify_bits(PSE_IQ_REV_RLS3, PSE_IQ_RLS, 16, 8);
	reg_modify_bits(PSE_IQ_REV_RLS5, PSE_IQ_RLS, 0, 8);
	reg_modify_bits(PSE_IQ_REV_RLS5, PSE_IQ_RLS, 16, 8);
	reg_modify_bits(PSE_IQ_REV_RLS6, PSE_IQ_RLS, 0, 8);
	reg_modify_bits(PSE_IQ_REV_RLS6, PSE_IQ_RLS, 16, 8);
	reg_modify_bits(PSE_IQ_REV_RLS7, PSE_IQ_RLS, 0, 8);
#endif /* CONFIG_HNAT_V2 */
}

void ppe_eng_init(void)
{
	ppe_set_ip_prot();

	ppe_set_cache_ebl(0);
	ppe_set_cache_ebl(1);

	ppe_set_mtu();

	/* Initialize PPE related register */
	ppe_eng_start();
}

int check_whitelist(struct sk_buff *skb)
{
	int i, dev_match;

	dev_match = 1;
	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == skb->dev) {
			dev_match = 0;
			/* pr_notice("%s : Interface=%s, vir_if_idx=%x\n", __func__, skb->dev, vir_if_idx); */
			break;
		}
	}

#ifdef CONFIG_RAETH_EDMA
	for (i = 1; i < MAX_IF_NUM; i++) {
		if(dst_port[i]->name == NULL) {
			dev_match = 1;
			if (debug_level >= 7) {
				pr_err("[HS-ethernet/HWNAT/TX] %s : dst_port[%d] name is NULL\n", __func__, i);
			}
			return dev_match;
		}

		if ((strcmp(dst_port[i]->name, DEV_NAME) == 0 && strcmp(skb->dev->name, AQR_DEV_NAME) == 0) ||
		    (strcmp(dst_port[i]->name, DEV2_NAME) == 0 && strcmp(skb->dev->name, AQR_DEV2_NAME) == 0)) {
			dev_match = 0;
			if (debug_level >= 7) {
				pr_notice("[HS-ethernet/HWNAT/TX] %s : dst_port[%d]  Interface =%s Match\n", __func__, i, skb->dev->name);
			}
			break;
		}
	}
#endif

	if (dev_match == 1) {
		pr_err("%s : dev not found\n", __func__);
	}

	return dev_match;
}

struct foe_entry  *decide_which_ppe(struct sk_buff *skb)
{
	struct foe_entry *entry;

	if (FOE_ENTRY_NUM(skb) >= FOE_4TB_SIZ) {
		pr_err("%s, entry index %d is out of bound\n", __func__, FOE_ENTRY_NUM(skb));
		return NULL;
	}

	if((FOE_SP(skb) == GDMA1_PSE_PORT) || (FOE_SP(skb) == GDMA2_PSE_PORT)) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == WDMA0_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == WDMA1_PSE_PORT) {
		entry = &ppe1_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == WDMA2_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == MDMA_PSE_PORT) {
		entry = &ppe1_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == EDMA0_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == EDMA1_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == ADMA_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
	} else if(FOE_SP(skb) == QDMA_PSE_PORT) {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];

		/* MD->PPE->QDMA->ADMA->out, FOE_AI is 0x1e, find ppe1 for modem ingress packet */
		if (FOE_AI(skb) == PACKET_FORWARD_PATH_WITHOUT_PPE)
			entry = &ppe1_foe_base[FOE_ENTRY_NUM(skb)];
	} else {
		entry = &ppe_foe_base[FOE_ENTRY_NUM(skb)];
		if (debug_level >= 15) {
			/* [MAGIC is PPE] extif -> eth_tx (pse port is uninitialized) -> eth rx -> pingpong */
			/* [MAGIC is WLAN] rx wifi (pse port is uninitialized) */
			if (FOE_MAGIC_TAG(skb) != FOE_MAGIC_PPE && FOE_MAGIC_TAG(skb) != FOE_MAGIC_WLAN) {
				//FOE_INFO_DUMP(skb);
				pr_notice("%s, SP port error = %d, %s\n", __func__, FOE_SP(skb), skb->dev->name);
			}
		}
	}
	return entry;
}

void hwnat_config_setting(void)
{
	hnat_chip_name |= MT7621_HWNAT;
	hnat_chip_name |= MT7622_HWNAT;
	hnat_chip_name |= MT7623_HWNAT;
	hnat_chip_name |= LEOPARD_HWNAT;
}

void fe_feature_setting(void)
{
	fe_feature |= GE2_SUPPORT;
	fe_feature |= HNAT_IPV6;
	fe_feature |= HNAT_VLAN_TX;
	fe_feature |= HNAT_MCAST;
	fe_feature |= HNAT_QDMA;
	fe_feature |= WARP_WHNAT;
	fe_feature |= WIFI_HNAT;
	fe_feature |= HNAT_WAN_P4;
	fe_feature |= WAN_TO_WLAN_QOS;
	fe_feature |= HNAT_SP_TAG;
	fe_feature |= QDMA_TX_RX;
	fe_feature |= PPE_MIB;
	fe_feature |= PACKET_SAMPLING;
	fe_feature |= HNAT_OPENWRT;
	fe_feature |= HNAT_WLAN_QOS;
	fe_feature |= WLAN_OPTIMIZE;
	fe_feature |= UDP_FRAG;
	fe_feature |= AUTO_MODE;
	fe_feature |= SEMI_AUTO_MODE;
	fe_feature |= MANUAL_MODE;
	fe_feature |= PRE_BIND;
	fe_feature |= HNAT_IPI;
	fe_feature |= DBG_IPV6_SIP;
	fe_feature |= DBG_IPV4_SIP;
	fe_feature |= DBG_SP;
	fe_feature |= ETH_QOS;
	fe_feature |= SW_DVFS;
	fe_feature |= AUTO_HNAT;

	/* please declare int i if enable this section
	for (i = 0; i < ARRAY_SIZE(mtk_hnat_feature_name); i++) {
		if (fe_feature & BIT(i))
			pr_notice("!! hwnat feature :%s\n", mtk_hnat_feature_name[i]);
	}
	*/
}

void hnat_work_init(int enable) {
	if (enable) {
		INIT_WORK(&hnat_work, eth_auto_qos_worker);
	} else {
		cancel_work_sync(&hnat_work);
	}
}
