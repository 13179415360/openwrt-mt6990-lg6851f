/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Harry Huang <harry.huang@mediatek.com>
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/dma-map-ops.h>
#include <net/ra_nat.h>

struct net_device	*dst_port[MAX_IF_NUM];
EXPORT_SYMBOL(dst_port);
u8 dst_port_type[MAX_IF_NUM];
EXPORT_SYMBOL(dst_port_type);

int (*ppe_hook_rx_wifi)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_wifi);
int (*ppe_hook_tx_wifi)(struct sk_buff *skb, int gmac_no) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_wifi);

int (*ppe_hook_rx_modem_thread)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_modem_thread);
int (*ppe_hook_rx_modem)(struct sk_buff *skb, u8 drop, u8 channel) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_modem);
int (*ppe_hook_tx_modem)(struct sk_buff *skb, u32 net_type, u32 channel_id) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_modem);

int (*ppe_hook_rx_rndis)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_rndis);
int (*ppe_hook_tx_rndis)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_rndis);


int (*ppe_hook_rx_eth)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_eth);
int (*ppe_hook_tx_eth)(struct sk_buff *skb, int gmac_no) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_eth);
int (*ppe_hook_eth_tx_fport)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_eth_tx_fport);

int (*ppe_hook_rx_ext)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_ext);
int (*ppe_hook_tx_ext)(struct sk_buff *skb, int gmac_no) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_ext);

int (*ppe_hook_rx_snps)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_rx_snps);
int (*ppe_hook_tx_snps)(struct sk_buff *skb) = NULL;
EXPORT_SYMBOL(ppe_hook_tx_snps);

void (*ppe_dev_register_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_register_hook);
void (*ppe_dev_unregister_hook)(struct net_device *dev) = NULL;
EXPORT_SYMBOL(ppe_dev_unregister_hook);

int (*ppe_get_dev_stats)(struct net_device *dev, struct rtnl_link_stats64 *storage) = NULL;
EXPORT_SYMBOL(ppe_get_dev_stats);

int (*ppe_get_conn_stats)(struct hnat_flow_tuple *tuple, u64 *pkt_cnt, u64 *byte_cnt) = NULL;
EXPORT_SYMBOL(ppe_get_conn_stats);

int (*ppe_del_entry_by_mac)(unsigned char *mac) = NULL;
EXPORT_SYMBOL(ppe_del_entry_by_mac);

/* USB skb-recycle interface */
typedef int (*ppe_usb_tx_func)(struct net_device *net, char *buff, int len,
			       int dma_id, int queue_id);

typedef void (*ppe_usb_rx_done_func)(void *context);

void (*ppe_usb_tx_done_hook)(char *buff) = NULL;
EXPORT_SYMBOL(ppe_usb_tx_done_hook);

int (*ppe_usb_rx_send_hook)(struct net_device *net, void *context,
			    char *buff, int len) = NULL;
EXPORT_SYMBOL(ppe_usb_rx_send_hook);

int (*ppe_usb_tx_link_num_hook)(void) = NULL;
EXPORT_SYMBOL(ppe_usb_tx_link_num_hook);

void (*ppe_usb_set_cb_hook)(ppe_usb_tx_func usb_tx,
			    ppe_usb_rx_done_func usb_rx_done) = NULL;
EXPORT_SYMBOL(ppe_usb_set_cb_hook);

void (*ppe_usb_resume_queue_hook)(struct net_device *net, int dma_id,
				  int queue_id) = NULL;
EXPORT_SYMBOL(ppe_usb_resume_queue_hook);

void (*ppe_usb_suspend_queue_hook)(struct net_device *net, int dma_id,
				   int queue_id) = NULL;

EXPORT_SYMBOL(ppe_usb_suspend_queue_hook);


void  hwnat_magic_tag_set_zero(struct sk_buff *skb)
{
	if (FOE_MAGIC_VALID(FOE_MAGIC_TAG_HEAD(skb))) {
		if (IS_SPACE_AVAILABLE_HEAD(skb)) {
			/* FOE_MAGIC_TAG_HEAD(skb) = 0; */
			/* FOE_TAG_PROTECT_HEAD(skb) = 0; */
			memset(FOE_INFO_START_ADDR_HEAD(skb), 0, FOE_INFO_LEN);
		}
	}
	if (FOE_MAGIC_VALID(FOE_MAGIC_TAG_TAIL(skb))) {
		if (IS_SPACE_AVAILABLE_TAIL(skb)) {
			/* FOE_MAGIC_TAG_TAIL(skb) = 0; */
			/* FOE_TAG_PROTECT_TAIL(skb) = 0; */
			memset(FOE_INFO_START_ADDR_TAIL(skb), 0, FOE_INFO_LEN);
		}
	}
}
EXPORT_SYMBOL(hwnat_magic_tag_set_zero);

void hwnat_check_magic_tag(struct sk_buff *skb)
{
	if (IS_SPACE_AVAILABLE_HEAD(skb)) {
		/* FOE_MAGIC_TAG_HEAD(skb) = 0; */
		/* FOE_AI(skb) = UN_HIT; */
		memset(FOE_INFO_START_ADDR_HEAD(skb), 0, FOE_INFO_LEN);
	}
	if (IS_SPACE_AVAILABLE_TAIL(skb)) {
		/* FOE_MAGIC_TAG_TAIL(skb) = 0; */
		/* FOE_AI_TAIL(skb) = UN_HIT; */
		memset(FOE_INFO_START_ADDR_TAIL(skb), 0, FOE_INFO_LEN);
	}
}
EXPORT_SYMBOL(hwnat_check_magic_tag);

void hwnat_set_headroom_zero(struct sk_buff *skb)
{
	if (skb->cloned != 1 && skb->head) {
		if (IS_SPACE_AVAILABLE_HEAD(skb))  {
			if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb))
				memset(FOE_INFO_START_ADDR_HEAD(skb), 0,
				       FOE_INFO_LEN);
		}
	}
}
EXPORT_SYMBOL(hwnat_set_headroom_zero);

void hwnat_set_tailroom_zero(struct sk_buff *skb)
{
	if (skb->cloned != 1 && skb->head) {
		if (IS_SPACE_AVAILABLE_TAIL(skb))  {
			if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb))
				memset(FOE_INFO_START_ADDR_TAIL(skb), 0,
				       FOE_INFO_LEN);
		}
	}
}
EXPORT_SYMBOL(hwnat_set_tailroom_zero);

void hwnat_copy_headroom(u8 *data, struct sk_buff *skb)
{
	if (IS_MAGIC_TAG_PROTECT_VALID_HEAD(skb))
		if (IS_SPACE_AVAILABLE_HEAD(skb))
			memcpy(data, skb->head, FOE_INFO_LEN);
}
EXPORT_SYMBOL(hwnat_copy_headroom);

void hwnat_copy_tailroom(u8 *data, int size, struct sk_buff *skb)
{
	if (IS_MAGIC_TAG_PROTECT_VALID_TAIL(skb)) {
		if (size - (skb_tail_pointer(skb) - skb->head) >= FOE_INFO_LEN)
			memcpy((data + size - FOE_INFO_LEN),
			       (skb_end_pointer(skb) - FOE_INFO_LEN),
			       FOE_INFO_LEN);
	}
}
EXPORT_SYMBOL(hwnat_copy_tailroom);

void hwnat_setup_dma_ops(struct device *dev, bool coherent)
{
	arch_setup_dma_ops(dev, 0, 0, NULL, coherent);
}
EXPORT_SYMBOL(hwnat_setup_dma_ops);

