/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2018-2019 MediaTek Inc.
 *
 * Author: Kurt Yang <kurt.yang@mediatek.com>
 *
 */
#ifndef MTK_FE_DMA_H
#define MTK_FE_DMA_H

#include <linux/stdarg.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include "mtk_frame_engine.h"

enum mtk_fe_dma_channel {
	MTK_FE_EDMA_0,
	MTK_FE_EDMA_1,
	MTK_FE_EDMA_2,
	MTK_FE_DMA_NUM,
};

enum mtk_fe_dma_port {
	MTK_FE_GMAC0 = 1,
	MTK_FE_GMAC1 = 2,
	MTK_FE_PPE0  = 3,
	MTK_FE_PPE1  = 4,
};

enum mtk_fe_dma_events {
	MTK_FE_RX_DONE,
	MTK_FE_RX_DONE_SKB,
	MTK_FE_TX_DONE,
	MTK_FE_EVENTS_NUM,
};

/* Capability for FE DMA configuration */
#define MTK_FE_DMA_RX_VLAN_UNTAG	BIT(0)

#define MTK_FE_DMA_CAPS(caps, _x)		(((caps) & (_x)) == (_x))

struct dma_rx_buf;

/*
 * struct mtk_fe_dma - the dma module of MTK frame engine.
 *
 * @user: the name of the user, for debuging.
 * @dma_channel: @see enum mtk_fe_dma_channel.
 * @do_close: close operation implemented by each xDMA modules.
 * @do_xmit: tx operation which should be override by each xDMA implementations
 * @is_tx_ring_full: for xDMA implementee to check if ring is full.
 * @do_tx_housekeeping: for xDMA implementee to reclaim the tx done buf.
 * @do_poll: rx operation which should be override by each xDMA implementations
 * @do_hw_conf: configure HW capability for each xDMA
 * @recycle_buf: signal xDMA implementee to recycle the buf.
 * @suspend_buf: signal xDMA implementee to suspend the buf.
 * @resume_buf: signal xDMA implementee to resume the buf.
 * @max_tx_buf_len: xDMA implementee should set up this value.
 * @rx_netdev:  used when user sets dma to netdev mode. This netdev is owned by
 *		client side, and fe_dma would delever data to this netdev.
 * @netdev:	Used for netdev mode, as the representative of the DMA instance
 *		So, when user can call dev_queue_xmit(skb) to xmit into this
		DMA, by setting skb->dev = fe_dma->netdev.
 * @rx_napi:	internal usage. we still use napi/dummy_dev to do rx polling
 * @tx_napi:	internal usage. we still use napi/dummy_dev to do tx polling
 * @fe:		the pointer to the coresponding mtk_fe_token.
 * @dest_port:  @see enum mtk_fe_dma_port.
 * @txdone_handler: tx done notification callback, with the buf_obj which
 *					was passed in fe_dma_xmit(_skb).
 * @rxdone_skb_handler: rx done notification callback, this is skb version.
 * @rxdone_handler: rx done notification callback, this is raw data version.
 * @rxdone_handler_data: registered by fe_dma_set_notify_cb, it will be passed
 *			as 'priv' in rxdone_handler/rxdone_skb_handler callback
 * @rx_recycle_mode: internal usage.
 * @caps: capability for DMA configuration.
 */
struct mtk_fe_dma {
	char *user;
	u32 dma_channel;

	void (*do_close)(struct mtk_fe_dma *dma);
	int (*do_pm_handle)(int channel, bool suspend);
	int (*do_xmit)(struct mtk_fe_dma *dma,
					void *data,
					int head_len,
					skb_frag_t *frags,
					int nr_frags,
					u8 tso,
					u8 is_checksum_offload,
					u16 vlan_tci,
					void *buf_obj);
	int (*is_tx_ring_full)(struct mtk_fe_dma *dma, int tx_num);
	int (*do_tx_housekeeping)(struct mtk_fe_dma *dma, int tx_num);
	int (*do_poll)(struct mtk_fe_dma *dma, int budget, int ring_no);
	void (*do_hw_conf)(struct mtk_fe_dma *dma, u32 cap, bool on);

	int (*recycle_buf)(struct mtk_fe_dma *dma, struct dma_rx_buf *buf);
	int (*suspend_buf)(struct mtk_fe_dma *dma, int ring_no);
	int (*resume_buf)(struct mtk_fe_dma *dma, int ring_no);

	int max_tx_buf_len;

	struct net_device *rx_netdev;
	struct net_device *netdev;

	struct napi_struct rx_napi_0;
	struct napi_struct rx_napi_1;
	struct napi_struct tx_napi;
	struct mtk_fe_token *fe;

	int dest_port;

	int (*txdone_handler)(void *buf_obj);
	int (*rxdone_skb_handler)(struct sk_buff *skb, int dma_channel,
				int ring_no, u32 foe_info, void *priv);
	int (*rxdone_handler)(void *data, int len, u32 foe_info, void *priv);
	void *rx_handler_data;

	bool rx_recycle_mode;
	u32 caps;

	u32	msg_enable;  // Kurt: really nned this? used by netif_err...
};

typedef int (*fe_dma_open_fn)(int channel, struct mtk_fe_dma **dma,
		u32 tx_ring_num, u32 rx_ring_num);
typedef int (*fe_dma_pm_fn)(int channel, bool suspend);
typedef int (*fe_dma_dbg_fn)(char *argv[]);

extern int mtk_fe_edma_probe(struct platform_device *pdev,
		fe_dma_open_fn *fn, fe_dma_pm_fn *pm_fn, fe_dma_dbg_fn *dbg_fn);

#define DMA_MAX_FRAG_SIZE 1984
// How to implement this? SKB_DATA_ALIGN can't be used as static define.
//	SKB_DATA_ALIGN(MTK_RX_HLEN + MTK_MAX_RX_LENGTH - MTK_RX_ETH_HLEN) +
//	SKB_DATA_ALIGN(sizeof(struct skb_shared_info))


/*
 * struct dma_rx_buf - context of the dma rx buffer.
 *                     These information are used when recycle mode.
 *
 * @data: the real dma rx buffer.
 * @skb: the coresponding skb of this buffer. For skb recycle.
 * @dma: the coresponding dma instance of this buffer.
 * @ring_no: the coresponding ring number of this buffer.
 * @list: used when xDMP implementee uses list to keep many dma_rx_buf.
 */
struct dma_rx_buf {
	u8 data[DMA_MAX_FRAG_SIZE];
	struct sk_buff *skb;
	struct mtk_fe_dma *dma;
	int ring_no;
	struct list_head list;
};

/*
 * Open a fe_dma channel.
 *
 * @fe: the frame engine token.
 * @channel: @see enum mtk_fe_dma_channel
 * @dma: to get mtk_fe_dma instance.
 * return: 0 is success.
 */
int fe_dma_napi_enable(struct mtk_fe_dma **dma);


/*
 * Close a fe_dma channel.
 *
 * @dma: the mtk_fe_dma instance.
 * return: 0 is success.
 */
int fe_dma_napi_disable(struct mtk_fe_dma *dma);

int fe_dma_probe(struct mtk_fe_token *fe, u32 channel,
	struct mtk_fe_dma **dma, u32 tx_ring_num, u32 rx_ring_num);

int fe_dma_pm_resume(struct mtk_fe_dma *dma);

int fe_dma_cleanup(struct mtk_fe_dma *dma);

int fe_dma_pm_suspend(struct mtk_fe_dma *dma);

/*
 * transmit data with length on a fe_dma module.
 *
 * @dma: the mtk_fe_dma instance.
 * @data: the packet data.
 * @len: length of the packet.
 * @vlan: tci of vlan tag.
 * @buf_obj: a buf object used for sender to indentify.
 * return: 0 is success.
 */
int fe_dma_xmit(struct mtk_fe_dma *dma, void *data, int len,
		u16 vlan_tci, void *buf_obj);

/*
 * transmit skb on a fe_dma module.
 *
 * @dma: the mtk_fe_dma instance.
 * @skb: the sk_buff contain the real packet.
 * @buf_obj: a buf object used for sender to indentify.
 * return: 0 is success.
 */
int fe_dma_xmit_skb(struct mtk_fe_dma *dma, struct sk_buff *skb,
		void *buf_obj, u16 vlan_tci);

/*
 * Configure FE DMA engine tx into SKB mode or Raw data mode.
 *
 * @dma: the mtk_fe_dma instance.
 * @on: 1 is skb mode, 0 is raw data mode.
 * return: 0 is success.
 */
int fe_dma_set_skb_txmode(struct mtk_fe_dma *dma, bool on);

/*
 * Configure FE DMA engine rx into buffer recycle mode.
 *
 * @dma: the mtk_fe_dma instance.
 * @on: 1 is recycle mode, 0 is normal mode.
 * return: 0 is success.
 */
int fe_dma_set_rx_recycle(struct mtk_fe_dma *dma, bool on);

/*
 * Configure HW capability for this DMA.
 *
 * @dma: the mtk_fe_dma instance.
 * @caps: capability to configure.
 */
void fe_dma_hw_cap_conf(struct mtk_fe_dma *dma, u32 caps, bool on);

/*
 * Setup and get the net_dev for transmitting data to this DMA.
 * After this API, this DMA engine will use Linux net_device/sk_buff on TX.
 *
 * @dma: the dma engine.
 * @dma_netdev: to get the net_dev for transmitting data to this DMA.
 * return: 0 is success.
 */
int fe_dma_setup_tx_netdev(struct mtk_fe_dma *dma,
		struct net_device **dma_netdev);

/*
 * Set rx net_dev to receive the data from the DMA.
 * After this API, this DMA engine will use Linux net_device/sk_buff on RX.
 *
 * @dma: the dma engine.
 * @netdev: the net_dev to receive the data from the DMA.
 * return: 0 is success.
 */
int fe_dma_set_rx_netdev(struct mtk_fe_dma *dma,
		struct net_device *netdev);

/*
 * Set tx destination port.
 *
 * @dma: the dma engine.
 * @dst: destination port. @see enum mtk_fe_dma_port.
 * return: 0 is success.
 */
int fe_dma_set_dest(struct mtk_fe_dma *dma, int dst);

/*
 * Set notification callback for specific event..
 *
 * @dma: the dma engine.
 * @event: notify for a specific event. @see enum mtk_fe_dma_events.
 * @cb_fn: callback function ptr.
 * return: 0 is success.
 */
int fe_dma_set_notify_cb(struct mtk_fe_dma *dma,
		int event, void *cb_fn, ...);

/*
 * For each xDMA modules to submit rx polling tasks in ring0 irq handler.
 *
 * return: 0 is success to submit.
 */
int fe_dma_schedule_rx0_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to submit rx polling tasks in ring1 irq handler.
 *
 * return: 0 is success to submit.
 */
int fe_dma_schedule_rx1_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to submit tx polling tasks in their irq handler.
 *
 * return: 0 is success to submit.
 */
int fe_dma_schedule_tx_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to signal it finish the rx0 polling tasks.
 *
 * return: 0 is success to submit.
 */
int fe_dma_complete_rx0_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to signal it finish the rx1 polling tasks.
 *
 * return: 0 is success to submit.
 */
int fe_dma_complete_rx1_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to signal it finish the tx polling tasks.
 *
 * return: 0 is success to submit.
 */
int fe_dma_complete_tx_poll(struct mtk_fe_dma *dma);

/*
 * For each xDMA modules to signal tx is done. (for the buf obj)
 *
 * return: 0 is success.
 */
int fe_dma_notify_txdone(struct mtk_fe_dma *dma, void *buf_obj);

/*
 * For each xDMA modules to deliver the rx data to the DMA client side.
 *
 * @dma: the dma engine.
 * @data: the packet data buffer.
 * @frag_size:
 * @pktlen: the length of the packet.
 * @is_checksum_offload: is this packet handled by HW checksum or not.
 * @vlan_tci: the tci of vlan tag.
 * @foe_info: the information for HWNAT.
 *
 * return: 0 is success to deliver.
 */
int fe_dma_deliver_rxdata(struct mtk_fe_dma *dma,
		u8 *data, u16 frag_size, u16 pktlen, u8 is_checksum_offload,
		u16 vlan_tci, u32 foe_info);

/*
 * For each xDMA modules to report the rx packet drop.
 *
 * @dma: the dma engine.
 * @drop_num: number of dropped packets.
 *
 * return: 0 is success.
 */
int fe_dma_rxdrop_counting(struct mtk_fe_dma *dma, int drop_num);

/*
 * Notify dma engine that skb has been processed and can reclaim it.
 * This API is usually used for recycle mode.
 *
 * @skb: the processed skb.
 * return: 0 is success.
 */
int recycle_fe_dma_skb(struct sk_buff *skb);

/*
 * Notify dma engine that data buf has been processed and can reclaim it.
 * This API is usually used for recycle mode.
 *
 * @buf: the processed data buffer.
 * return: 0 is success.
 */
int recycle_fe_dma_buf(void *buf);

/*
 * Notify dma engine that xDMA buffer need to be suspended.
 * This API is used for buffer flow control.
 *
 * @dma_channel: the dma_channel of xDMA.
 * @ring_no: the ring number of xDMA.
 * return: 0 is success.
 */
int fe_dma_suspend_buf(u32 dma_channel, u32 ring_no);

/*
 * Notify dma engine that xDMA buffer cab be resumed from suspend.
 * This API is used for buffer flow control.
 *
 * @dma_channel: the dma_channel of xDMA.
 * @ring_no: the ring number of xDMA.
 * return: 0 is success.
 */
int fe_dma_resume_buf(u32 dma_channel, u32 ring_no);


#endif /* MTK_FE_DMA_H */
