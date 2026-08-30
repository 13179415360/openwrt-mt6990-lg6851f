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

#include <net/mtk/mtk_frame_engine.h>
#include <net/mtk/mtk_fe_dma.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include "hnat_common.h"
#include "hnat_define.h"
#include "hnat_fast.h"
#ifdef CONFIG_TUNNEL_FAST_PATH
#include "hnat_tnl.h"
#endif /* CONFIG_TUNNEL_FAST_PATH */
#include "ra_nat.h"


static struct mtk_hnat_fast hnat_fast;
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
static int ppe_edma_tx_hw_vlan = 1;
static int ppe_edma_rx_hw_vlan = 0;
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */

uint16_t ppe_remove_vlan_tag(struct sk_buff *skb)
{
	struct vlan_ethhdr *veth;
	u16 vir_if_idx;
	char *new_data;

	veth = (struct vlan_ethhdr *)skb->data;

	/* something wrong */
	if (veth->h_vlan_proto != htons(ETH_P_8021Q)) {

		if (debug_level >= 7)
			pr_notice("%s, HNAT: Reentry packet is untagged frame?\n", __func__);
		return 65535;
	}

	vir_if_idx = ntohs(veth->h_vlan_TCI) & 0x3fff;

	new_data = skb->data + VLAN_HLEN;
	memmove(new_data, skb->data, ETH_HLEN - ETH_TLEN);
	skb->data = new_data;
	skb->len -= VLAN_HLEN;

	return vir_if_idx;
}



uint32_t ppe_hnat_fast_pingpong(struct sk_buff *skb)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	struct ethhdr *eth = NULL;
	u16 idx, vir_if_idx;
	struct net_device *dev;

	if (debug_level >= 10)
		pr_notice("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d\n",
			__func__, FOE_AI(skb), FOE_SP(skb));

	if (ppe_edma_rx_hw_vlan) {
		vir_if_idx = skb->vlan_tci;
		/* clear the fields modified by __vlan_hwaccel_put_tag() */
		__vlan_hwaccel_clear_tag(skb);
	} else
		vir_if_idx = ppe_remove_vlan_tag(skb);

	if (debug_level >= 5)
		pr_notice("%s, ppe_edma_rx_hw_vlan:%d, vir_if_idx:%d\n", __func__, ppe_edma_rx_hw_vlan, vir_if_idx);

	/* recover to right incoming interface */
	if (vir_if_idx > 0 && vir_if_idx < MAX_IF_NUM) {
		skb->dev = dst_port[vir_if_idx];
		FOE_IF_IDX(skb) = vir_if_idx;
	} else {
		pr_notice("%s, FOE_AI(skb):%x, FOE_SP(skb):%d, wrong idx:%d\n",
			__func__, FOE_AI(skb), FOE_SP(skb), vir_if_idx);

		{
			uint32_t *d = (uint32_t *) skb->data;

			pr_notice("%s, [0]%08x %08x %08x %08x [4]%08x %08x %08x %08x [8]%08x %08x\n",
				__func__, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9]);
		}

		/* temp solution: drop packet to prevent KE */
		kfree_skb(skb);
		return 0;
	}

	skb->protocol = eth_type_trans(skb, skb->dev);
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

	if (debug_level >= 5)
		pr_notice("%s end, name:%s, vir_if_idx:%d, pkt_type:%d\n",
			__func__, skb->dev->name, vir_if_idx, skb->pkt_type);

#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
	return 1;
}

int ppe_hitbind_force_tx(struct sk_buff *skb, int dma_id, int ring_no)
{
	struct foe_entry *entry;
	uint32_t act_dp;

	entry = decide_which_ppe(skb);
	if (entry == NULL)
		return 0;

	act_dp = get_act_dp(entry);

	skb->dev = dst_port[act_dp];

	if (skb->dev == NULL) {
		pr_notice("%s, interface is unknown !!! act_dp:%d, dma_id:%d, ring_no:%d\n",
			__func__, act_dp, dma_id, ring_no);

		foe_dump_entry(FOE_ENTRY_NUM(skb), entry, 0);
		{
		uint32_t *d = (uint32_t *) skb->data;

		pr_notice("%s, [0]%08x %08x %08x %08x [4]%08x %08x %08x %08x [8]%08x %08x\n",
			__func__, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9]);
		}

		kfree_skb(skb);
		return 0;
	}

	if (debug_level >= 10) {
		pr_notice("%s, skb->dev->name:%s, act_dp:%d, dma_id:%d, ring_no:%d\n",
			__func__, skb->dev->name, act_dp, dma_id, ring_no);
	}

	/* if usb interface */
	if ((strncmp(skb->dev->name, "rndis", 5) == 0) ||
		(strncmp(skb->dev->name, "ncm", 3) == 0) ||
		(strncmp(skb->dev->name, "ecm", 3) == 0)) {
		/* call usb_tx */
		if (hnat_fast.usb_tx)
			hnat_fast.usb_tx(skb->dev, skb->data, skb->len, dma_id, ring_no);
		else
			pr_notice("%s, usb_tx is null\n", __func__);
	} else {
		/* call tx */
		dev_queue_xmit(skb);
	}

	return 0;
}

/* USB driver returns skb buffer to FE driver */
void ppe_usb_tx_done(char *buff)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	if (debug_level >= 10)
		pr_notice("%s, buff:%p\n", __func__, buff);
	recycle_fe_dma_buf(buff);

#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_usb_resume_queue(struct net_device *net, int dma_id, int ring_no)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)

	if (debug_level >= 10)
		pr_notice("%s, dev->name:%s, dma_id:%d, ring_no:%d\n", __func__, net->name, dma_id, ring_no);

	if (dma_id >= 0 && dma_id < EDMA_NUM && ring_no >= 0 && ring_no < EDMA_QUEUE_NUM) {

		fe_dma_resume_buf(dma_id, ring_no);

		hnat_fast.in_suspend[dma_id][ring_no] = 0;
	}
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_usb_suspend_queue(struct net_device *net, int dma_id, int ring_no)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	if (debug_level >= 10)
		pr_notice("%s, dev->name:%s, dma_id:%d, ring_no:%d\n", __func__, net->name, dma_id, ring_no);


	if (dma_id >= 0 && dma_id < EDMA_NUM && ring_no >= 0 && ring_no < EDMA_QUEUE_NUM) {

		fe_dma_suspend_buf(dma_id, ring_no);

		hnat_fast.in_suspend[dma_id][ring_no] = 1;
	}
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}



int ppe_usb_set_cb(ppe_usb_tx_func tx_send, ppe_usb_rx_done_func rx_done)
{
	hnat_fast.usb_tx = tx_send;
	hnat_fast.usb_rx_done = rx_done;

	return 0;
}


int ppe_usb_rx_send(struct net_device *net, void *context, char *buff, int len)
{

#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	int i;
	struct vlan_ethhdr *veth;
	struct ethhdr *eth = (struct ethhdr *) buff;

	if (debug_level >= 10)
		pr_notice("%s, context: %p, buff: %p, len:%d\n", __func__, context, buff, len);

		/* PPE can only handle IPv4/IPv6/PPP packets */
	if (((eth->h_proto != htons(ETH_P_8021Q)) &&
	    (eth->h_proto != htons(ETH_P_IP)) && (eth->h_proto != htons(ETH_P_IPV6)) &&
	    (eth->h_proto != htons(ETH_P_PPP_SES)) && (eth->h_proto != htons(ETH_P_PPP_DISC))) ||
			is_multicast_ether_addr(&eth->h_dest[0])) {
		if (debug_level >= 10)
			pr_notice("%s not support, eth->h_proto:0x%x, multicast:%d\n",
				__func__, eth->h_proto, is_multicast_ether_addr(&eth->h_dest[0]));
		return -1;
	}

	if (!hnat_fast.init_done) {
		if (debug_level >= 10)
			pr_notice("%s, fe_token:%p, edma0:%p, init_done:%d\n",
				__func__, hnat_fast.fe_token, hnat_fast.edma0, hnat_fast.init_done);
		return 1;
	}

	for (i = 0; i < MAX_IF_NUM; i++) {
		if (dst_port[i] == net)
			break;
	}

	if (i == 0 || i == MAX_IF_NUM) {
		if (debug_level >= 1)
			pr_notice("%s UnKnown Interface, i:%d\n", __func__, i);
		return 1;
	}

	if (debug_level >= 5)
		pr_notice("%s, Interface i:%d, ppe_edma_tx_hw_vlan:%d\n", __func__, i, ppe_edma_tx_hw_vlan);

	if (ppe_edma_tx_hw_vlan) {
		/* send to PPE through EDMA0 */
		fe_dma_xmit(hnat_fast.edma0, buff, len, VLAN_CFI_MASK | i, context);
	} else {
		/* add sw vlan tag */
		veth = (struct vlan_ethhdr *)(buff - VLAN_HLEN);
		memmove((void *)veth, buff, ETH_HLEN - ETH_TLEN);

		veth->h_vlan_proto = htons(ETH_P_8021Q);
		veth->h_vlan_TCI = htons(i);

		if (debug_level >= 9) {
			uint32_t *d = (uint32_t *)veth;

			pr_notice("%s, [0]%08x %08x %08x %08x [4]%08x %08x %08x %08x [8]%08x %08x\n",
				__func__, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7], d[8], d[9]);
		}

		/* send to PPE through EDMA0 */
		fe_dma_xmit(hnat_fast.edma0, (void *)veth, len + VLAN_HLEN, 0, context);
	}

	return 0;
#else
	return 1;
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

int ppe_usb_tx_link_num(void)
{
	return EDMA_NUM * EDMA_QUEUE_NUM;
}


/* FE driver notify EDMA RX */
int ppe_fe_dma_rx_skb(struct sk_buff *skb, int dma_id, int ring_no, u32 foe_info, void *priv)
{
	if (IS_SPACE_AVAILABLE_HEAD(skb)) {
		*(u32 *)(FOE_INFO_START_ADDR_HEAD(skb)) = foe_info;
		FOE_ALG(skb) = 0;
		FOE_TAG_PROTECT(skb) = TAG_PROTECT;
	}
	if (IS_SPACE_AVAILABLE_TAIL(skb)) {
		*(u32 *)(FOE_INFO_START_ADDR_TAIL(skb) + 2) = foe_info;
		FOE_ALG_TAIL(skb) = 0;
		FOE_TAG_PROTECT_TAIL(skb) = TAG_PROTECT;
	}

	if (debug_level >= 10)
		pr_info("%s, FOE_AI(skb):0x%x, FOE_SP(skb):%d, dma_id:%d, ring_no:%d\n",
			__func__, FOE_AI(skb), FOE_SP(skb), dma_id, ring_no);

	/* DL bind */
	if (FOE_AI(skb) == HIT_BIND_FORCE_TO_CPU) {

		#ifdef CONFIG_TUNNEL_FAST_PATH
		{
			struct foe_entry *entry;

			entry = decide_which_ppe(skb);
			if (entry == NULL)
				return 0;

			/* pointer to layer3 header */
			skb->dev = dst_port[get_act_dp(entry)];
			skb->protocol = eth_type_trans(skb, skb->dev);

			if (ppe_tunnel_init_rx(skb, entry)) /* L2TP initiator */
				return 0; /* release skb */

			/* pointer to layer2 header */
			skb_set_network_header(skb, 0);
			skb_push(skb, ETH_HLEN);
		}
		#endif /* CONFIG_TUNNEL_FAST_PATH */

		/* to bind interface tx */
		return ppe_hitbind_force_tx(skb, dma_id, ring_no);

	} else if (FOE_AI(skb) == PACKET_FORWARD_PATH_WITHOUT_PPE) {
	/* MD->PPE->QDMA->ADMA->out, FOE_AI is 0x1e */
		/* to bind interface tx */
		return ppe_hitbind_force_tx(skb, dma_id, ring_no);
	} else {
	/* UL unbind */
		/* for debug */
		if (FOE_AI(skb) == HIT_BIND_KEEPALIVE_DUP_OLD_HDR)
			if (debug_level >= 3)
				pr_notice("USB RxGot HIT_BIND_KEEPALIVE_DUP_OLD_HDR packet (hash index=%d), dma_id:%d, ring_no:%d\n",
					FOE_ENTRY_NUM(skb), dma_id, ring_no);

		return ppe_hnat_fast_pingpong(skb);
	}
}

/* FE driver returns skb buffer to USB driver */
int ppe_fe_dma_tx_done(void *context)
{
	if (debug_level >= 10)
		pr_notice("%s, context: %p\n", __func__, context);

	if (hnat_fast.usb_rx_done)
		hnat_fast.usb_rx_done(context);
	else
		pr_notice("%s, usb_rx_done is null\n", __func__);

	return 0;
}

void ppe_fe_dma_deinit(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	if (hnat_fast.edma0 != NULL) {
		fe_dma_set_notify_cb(hnat_fast.edma0, MTK_FE_TX_DONE, NULL);
		fe_dma_set_notify_cb(hnat_fast.edma0, MTK_FE_RX_DONE_SKB, NULL);
		fe_dma_napi_disable(hnat_fast.edma0);
	}

	if (hnat_fast.edma1 != NULL) {
		fe_dma_set_notify_cb(hnat_fast.edma1, MTK_FE_TX_DONE, NULL);
		fe_dma_set_notify_cb(hnat_fast.edma1, MTK_FE_RX_DONE_SKB, NULL);
		fe_dma_napi_disable(hnat_fast.edma1);
	}
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_fe_dma_cleanup(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	if (hnat_fast.edma0 != NULL) {
		fe_dma_cleanup(hnat_fast.edma0);
		hnat_fast.edma0 = NULL;
	}
	if (hnat_fast.edma1 != NULL) {
		fe_dma_cleanup(hnat_fast.edma1);
		hnat_fast.edma1 = NULL;
	}

	if (hnat_fast.fe_token != NULL) {
		release_FE(hnat_fast.fe_token);
		hnat_fast.fe_token = NULL;
	}

	hnat_fast.init_done = false;
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_fe_dma_pm_suspend(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	pr_info("%s, fe_dma_pm_suspend\n", __func__);

	if (hnat_fast.edma0 != NULL)
		fe_dma_pm_suspend(hnat_fast.edma0);

	if (hnat_fast.edma1 != NULL)
		fe_dma_pm_suspend(hnat_fast.edma1);

#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_set_fe_dma_tx_hw_vlan(bool on)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	ppe_edma_tx_hw_vlan = on;
	pr_notice("%s, ppe_edma_tx_hw_vlan = %d\n", __func__, ppe_edma_tx_hw_vlan);
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_set_fe_dma_rx_hw_vlan(bool on)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	ppe_edma_rx_hw_vlan = on;
	fe_dma_hw_cap_conf(hnat_fast.edma0, MTK_FE_DMA_RX_VLAN_UNTAG, on);
	fe_dma_hw_cap_conf(hnat_fast.edma1, MTK_FE_DMA_RX_VLAN_UNTAG, on);

	pr_notice("%s, ppe_edma_rx_hw_vlan = %d\n", __func__, ppe_edma_rx_hw_vlan);
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}


void ppe_fe_dma_init(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)

	int edma_cnt, q_cnt;
	/* open edma0 */
	fe_dma_napi_enable(&hnat_fast.edma0);

	if (hnat_fast.edma0 == NULL) {
		pr_notice("%s, edma0 is null. ERROR! ERROR! ERROR!\n", __func__);
		goto error;
	}
	fe_dma_set_skb_txmode(hnat_fast.edma0, 0);
	fe_dma_set_rx_recycle(hnat_fast.edma0, 1);

	if (ppe_edma_rx_hw_vlan)
		fe_dma_hw_cap_conf(hnat_fast.edma0, MTK_FE_DMA_RX_VLAN_UNTAG, 1);
	else
		fe_dma_hw_cap_conf(hnat_fast.edma0, MTK_FE_DMA_RX_VLAN_UNTAG, 0);

	fe_dma_set_notify_cb(hnat_fast.edma0, MTK_FE_TX_DONE, ppe_fe_dma_tx_done);
	fe_dma_set_notify_cb(hnat_fast.edma0, MTK_FE_RX_DONE_SKB, ppe_fe_dma_rx_skb);

	/* open edma1 */
	fe_dma_napi_enable(&hnat_fast.edma1);
	if (hnat_fast.edma1 == NULL) {
		pr_notice("%s, edma1 is null. ERROR! ERROR! ERROR!\n", __func__);
		goto error;
	}
	/* edma1 txdone is dev_kfree_skb_any */
	fe_dma_set_skb_txmode(hnat_fast.edma1, 1);
	fe_dma_set_rx_recycle(hnat_fast.edma1, 1);

	if (ppe_edma_rx_hw_vlan)
		fe_dma_hw_cap_conf(hnat_fast.edma1, MTK_FE_DMA_RX_VLAN_UNTAG, 1);
	else
		fe_dma_hw_cap_conf(hnat_fast.edma1, MTK_FE_DMA_RX_VLAN_UNTAG, 0);

	fe_dma_set_notify_cb(hnat_fast.edma1, MTK_FE_RX_DONE_SKB, ppe_fe_dma_rx_skb);

	for (edma_cnt = 0; edma_cnt < EDMA_NUM; edma_cnt++)
		for (q_cnt = 0; q_cnt < EDMA_QUEUE_NUM; q_cnt++)
			hnat_fast.in_suspend[edma_cnt][q_cnt] = 0;

	hnat_fast.init_done = true;
	return;

error:
	ppe_fe_dma_deinit();

#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_fe_dma_probe(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)

	acquire_FE("hw_nat driver", &hnat_fast.fe_token);

	if (hnat_fast.fe_token == NULL) {
		pr_notice("%s, fe token is null. ERROR! ERROR! ERROR!\n", __func__);
		return;
	}

	pr_info("%s, fe_dma_probe\n", __func__);
	/* EDMA 0: tx_ring * 1 + rx_ring * 2 */
	fe_dma_probe(hnat_fast.fe_token, MTK_FE_EDMA_0, &hnat_fast.edma0, 1, 2);
	/* EDMA 1: tx_ring * 1 + rx_ring * 1 */
	fe_dma_probe(hnat_fast.fe_token, MTK_FE_EDMA_1, &hnat_fast.edma1, 1, 1);
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}

void ppe_fe_dma_pm_resume(void)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	pr_info("%s, fe_dma_pm_resume\n", __func__);
	/* EDMA 0: tx_ring * 1 + rx_ring * 2 */
	fe_dma_pm_resume(hnat_fast.edma0);
	/* EDMA 1: tx_ring * 1 + rx_ring * 1 */
	fe_dma_pm_resume(hnat_fast.edma1);
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */
}


void ppe_hnat_fast_dump(void)
{
	int edma_cnt, q_cnt;

	for (edma_cnt = 0; edma_cnt < EDMA_NUM; edma_cnt++)
		for (q_cnt = 0; q_cnt < EDMA_QUEUE_NUM; q_cnt++)
			pr_info("edma_cnt:%d, ring_cnt:%d, in_suspend: %d\n",
				edma_cnt, q_cnt, hnat_fast.in_suspend[edma_cnt][q_cnt]);

}

bool ppe_hnat_fast_init_done(void)
{
	if (debug_level == 7)
		pr_info("%s, init_done:%d\n", __func__, hnat_fast.init_done);

	return hnat_fast.init_done;
}

uint32_t ppe_hnat_fast_send_skb(struct sk_buff *skb)
{
#if defined(CONFIG_MTK_HNAT_FAST_SUPPORT) && defined(CONFIG_FE_MEDIATEK_SOC)
	u16 vir_if_idx = 0;
	struct ethhdr *eth = (struct ethhdr *)skb_mac_header(skb);

	/* PPE can only handle IPv4/IPv6/PPP packets */
	if (((skb->protocol != htons(ETH_P_8021Q)) &&
		(skb->protocol != htons(ETH_P_IP)) && (skb->protocol != htons(ETH_P_IPV6)) &&
		(skb->protocol != htons(ETH_P_PPP_SES)) && (skb->protocol != htons(ETH_P_PPP_DISC))) ||
		is_multicast_ether_addr(&eth->h_dest[0])) {
		if (debug_level >= 10)
			pr_notice("%s not support, skb->protocol:0x%x, multicast:%d\n",
				__func__, skb->protocol, is_multicast_ether_addr(&eth->h_dest[0]));
		return 1;
	}

	if (!hnat_fast.init_done) {
		if (debug_level >= 10)
			pr_notice("%s, fe_token:%p, edma0:%p, init_done:%d\n",
				__func__, hnat_fast.fe_token, hnat_fast.edma0, hnat_fast.init_done);
		return 1;
	}

	if (debug_level >= 10)
		pr_notice("%s, dev->name:%s, protocol:0x%x, skb_headroom:%d\n",
			__func__, skb->dev->name, skb->protocol, skb_headroom(skb));

	skb_set_network_header(skb, 0);

	/* currently, skb->data points to layer 3 */
	if (skb_headroom(skb) < FOE_INFO_LEN + ETH_HLEN + VLAN_HLEN) {
		if (debug_level >= 3)
			pr_notice("%s headroom isn't enough\n", __func__);
		return 1;
	}

	/* push vlan tag to stand for actual incoming interface, */
	/* so HNAT module can know the actual incoming interface from vlan id. */
	skb_push(skb, ETH_HLEN);/* pointer to layer2 header before calling hard_start_xmit */

	vir_if_idx = FOE_IF_IDX(skb);
	if (vir_if_idx == INVALID_IFIDX) {
		if (debug_level >= 1)
			pr_notice("%s UnKnown Interface, vir_if_idx=%d\n", __func__, vir_if_idx);
		return 1;
	}

	if (ppe_edma_tx_hw_vlan) {
		/* wifi sw fast path use edma1 tx */
		fe_dma_xmit_skb(hnat_fast.edma1, skb, (void *)skb, VLAN_CFI_MASK | vir_if_idx);
	} else {
		/* add sw vlan tag */
		skb->vlan_proto = htons(ETH_P_8021Q);
		skb = vlan_insert_tag(skb, skb->vlan_proto, vir_if_idx);
		if (skb == NULL) {
			if (debug_level >= 3)
				pr_notice("%s[%d], vlan_insert_tag() return NULL!\n", __func__, __LINE__);
			return 0;
		}
		/* wifi sw fast path use edma1 tx */
		fe_dma_xmit_skb(hnat_fast.edma1, skb, (void *)skb, 0);
	}
	return 0;

#else
	return 1;
#endif /* CONFIG_MTK_HNAT_FAST_SUPPORT && CONFIG_FE_MEDIATEK_SOC */


}
