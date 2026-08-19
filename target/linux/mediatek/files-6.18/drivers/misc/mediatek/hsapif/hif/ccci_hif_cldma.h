/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_HIF_CLDMA_H__
#define __CCCI_HIF_CLDMA_H__

#include "ccci_config.h"
#include "ccci_comm_config.h"
#include "ccci_msg_data.h"

//#include "ccci_hif_driver.h"
#include "ccci_hif.h"
#include "ccci_core.h"
//#include "ccci_bm.h"
#include "ccci_kmem.h"
#include "hw/ccci_hw_cldma.h"

#define CLDMA_TXQ_NUM 8
#define CLDMA_RXQ_NUM 8
#define NET_TXQ_NUM 4
#define NET_RXQ_NUM 4
#define NORMAL_TXQ_NUM 8
#define NORMAL_RXQ_NUM 8

#define MAX_BD_NUM (MAX_SKB_FRAGS + 1)
//#define TRAFFIC_MONITOR_INTERVAL 10	/* seconds */
#define SKB_RX_QUEUE_MAX_LEN 200000
#define CLDMA_ACTIVE_T 20
#define CLDMA_AP_MTU_SIZE	(NET_RX_BUF)	/*sync with runtime data*/

#define MAX_TXQ_NUM (16)
#define MAX_RXQ_NUM (16)
#define PACKET_HISTORY_DEPTH 16	/* must be power of 2 */


typedef enum {
	AP_TO_MD = 0,
	MD_TO_AP = 1,
	sAP_TO_eAP = 2,

} dir_enum;

typedef enum{
	MODE_BIT_32=0,
	MODE_BIT_36=1,
	MODE_BIT_40=2,
	MODE_BIT_64=3,

} hw_mode_enum;

enum RX_COLLECT_RESULT {
	ONCE_MORE,
	ALL_CLEAR,
	LOW_MEMORY,
	ERROR_STOP,
};

#if PACKET_HISTORY_DEPTH
struct ccci_log {
	struct ccci_header msg;
	u64 tv;
	int droped;
};
#endif

struct ccci_hif_traffic {
#if PACKET_HISTORY_DEPTH
		struct ccci_log tx_history[MAX_TXQ_NUM][PACKET_HISTORY_DEPTH];
		struct ccci_log rx_history[MAX_RXQ_NUM][PACKET_HISTORY_DEPTH];
		int tx_history_ptr[MAX_TXQ_NUM];
		int rx_history_ptr[MAX_RXQ_NUM];
#endif
		//unsigned long logic_ch_pkt_cnt[CCCI_CH_MAX_NUM-CCCI_CH_BASE];
		//unsigned long logic_ch_pkt_pre_cnt[CCCI_CH_MAX_NUM-CCCI_CH_BASE];
		short seq_nums[2][CCCI_CH_COUNT];

		unsigned long long latest_isr_time;
		unsigned long long latest_q_rx_isr_time[MAX_RXQ_NUM];
		unsigned long long latest_q_rx_time[MAX_RXQ_NUM];

		/* interrupt statistics */
		unsigned long long isr_cnt;
		unsigned long long rx_done_isr_cnt[MAX_RXQ_NUM];
		unsigned long long tx_done_isr_cnt[MAX_TXQ_NUM];
};

struct ccci_irq_hd {
	int(*usr_hd)(void *);
	void *param;
};

struct ccci_cldma_hw_info {
	struct device *pcie_dev;			// pcie device for DMA address mapping
	u8 hif_id; //single handle function multi HW IP
	dir_enum direction;
	hw_mode_enum hw_mode;
	void __iomem *cldma_ap_ao_base;
	void __iomem *cldma_ap_pdn_base;
	void __iomem *cldma0_lh_indma_ao_base;
	void __iomem *cldma0_lh_indma_pd_base;
	u32 cldma_irq_id;
	u64 cldma_irq_flags;
	unsigned long cldma_phy_ao_base;
	unsigned long cldma_phy_pd_base;
	u32 cldma_phy_interrupt_id;
	char other_attr[0];
};

struct irq_ops {
	void (*register_isr)(struct ccci_cldma_hw_info *hw_ctrl, int(*)(void*), void *param);
	void (*enable_irqs)(struct ccci_cldma_hw_info *hw_ctrl);
	void (*disable_irq)(struct ccci_cldma_hw_info *hw_ctrl);
};

struct ccci_cldma_hw_ctrl {
	u8 hif_id;
	struct device *dma_dev;
	struct ccci_cldma_hw_info *hw_info;
	spinlock_t spinlock;
	struct ccci_irq_hd irqhd;
	struct irq_ops irqops;
	struct ccci_hw_ops* opts;
};


struct mh_hw_info {
	long mhccif_physic_base_address;
	void __iomem *mhccif_virtual_base_address;

};


struct cldma_sram_layout {
	struct ccci_header dl_header;
	//struct md_query_ap_feature md_rt_data;
	struct ccci_header up_header;
	//struct ap_query_md_feature ap_rt_data;
};

/*
 * CLDMA feature options:
 * CLDMA_NO_TX_IRQ: mask all TX interrupts, collect TX_DONE skb when get Rx interrupt or Tx busy.
 * ENABLE_CLDMA_TIMER: use a timer to detect TX packet sent or not. not usable if TX interrupts are masked.
 * CLDMA_NET_TX_BD: use BD to support scatter/gather IO for net device
 */
/* #define CLDMA_NO_TX_IRQ */
/* #define ENABLE_CLDMA_TIMER */
/*#define CLDMA_NET_TX_BD*/

struct cldma_request {
	void *gpd;		/* virtual address for CPU */
	dma_addr_t gpd_addr;	/* physical address for DMA */
	//struct sk_buff *skb;
	ccci_kmem_data_t *data;
	dma_addr_t data_buffer_ptr_saved;
	struct list_head entry;
	struct list_head bd;
	/* inherit from skb */
	unsigned char ioc_override;	/* bit7: override or not; bit0: IOC setting */
}__packed;

typedef enum {
	RING_GPD = 0,
	RING_GPD_BD = 1,
	RING_SPD = 2,
} CLDMA_RING_TYPE;

struct md_cd_queue {
	unsigned char index;
	struct ccci_modem *modem;
	struct cldma_ring *tr_ring;
	struct cldma_request *tr_done;
	int budget;		/* same as ring buffer size by default */
	struct cldma_request *rx_refill;	/* only for Rx */
	struct cldma_request *tx_xmit;	/* only for Tx */
	wait_queue_head_t req_wq;	/* only for Tx */
	spinlock_t ring_lock;
	//struct ccci_skb_queue skb_list; /* only for network Rx */

	struct workqueue_struct *worker;
	struct work_struct cldma_rx_work;
	struct delayed_work cldma_tx_work;

	wait_queue_head_t rx_wq;
	struct task_struct *rx_thread;

#ifdef ENABLE_CLDMA_TIMER
	struct timer_list timeout_timer;
	unsigned long long timeout_start;
	unsigned long long timeout_end;
#endif
	u8 hif_id;
	DIRECTION dir;
	unsigned int busy_count;
};


/*
 * In a ideal design, all read/write pointers should be member of cldma_ring, and they will complete
 * a ring buffer object with buffer itself and Tx/Rx funcitions. but this will change too much of the original
 * code and we have to drop it. so here the cldma_ring is quite light and most of ring buffer opertions are
 * still in queue struct.
 */
struct cldma_ring {
	struct list_head gpd_ring;	/* ring of struct cldma_request */
	int length;		/* number of struct cldma_request */
	int pkt_size;		/* size of each packet in ring */
	CLDMA_RING_TYPE type;

	int (*handle_tx_request)(struct md_cd_queue *queue, struct cldma_request *req,
			ccci_kmem_data_t *data, unsigned int ioc_override);
	int (*handle_rx_done)(struct md_cd_queue *queue, int budget, int blocking, gfp_t gfp_flags);
	int (*handle_tx_done)(struct md_cd_queue *queue, int budget, int blocking);
};


struct ccci_md_cd_ctrl {
	//unsigned char md_id;
	u8 hif_id;
	struct md_cd_queue txq[CLDMA_TXQ_NUM];
	struct md_cd_queue rxq[CLDMA_RXQ_NUM];
	unsigned short txq_active;
	unsigned short rxq_active;
	unsigned short txq_started;
	atomic_t cldma_irq_enabled;

	spinlock_t cldma_timeout_lock;	/* this lock is using to protect CLDMA, not only for timeout checking */
	struct work_struct cldma_irq_work;
	struct workqueue_struct *cldma_irq_worker;

	struct delayed_work cldma_pcie_irq_work;
	struct workqueue_struct *cldma_pcie_irq_worker;
	u32 cldma_pcie_irq_type;

	struct ccci_hif_traffic traffic_info;
	atomic_t wakeup_src;
	struct wakeup_source *cldma0_wakelock;

#ifdef __TRAFFIC_MONITOR_INTERVAL__
	unsigned tx_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned rx_traffic_monitor[CLDMA_RXQ_NUM];
	unsigned tx_pre_traffic_monitor[CLDMA_TXQ_NUM];
	unsigned long long tx_done_last_start_time[CLDMA_TXQ_NUM];
	unsigned int tx_done_last_count[CLDMA_TXQ_NUM];
	struct timer_list traffic_monitor;
	unsigned long traffic_stamp;
#endif
	unsigned int tx_busy_warn_cnt;
	struct dma_pool *gpd_dmapool;	/* here we assume T/R GPD/BD/SPD have the same size  */
	struct cldma_ring net_tx_ring[NET_TXQ_NUM];
	struct cldma_ring net_rx_ring[NET_RXQ_NUM];
	struct cldma_ring normal_tx_ring[NORMAL_TXQ_NUM];
	struct cldma_ring normal_rx_ring[NORMAL_RXQ_NUM];
	struct tasklet_struct cldma_rxq0_task;
	struct cldma_sram_layout *cldma_sram_layout;
	//struct ccci_hif_ops *ops;
	void* hif_hw_info;
	unsigned char is_late_init;
	struct timer_list cldma_monitor_tx_status_timer;
	struct timer_list cldma_polling_mode_timer;
};

struct cldma_tgpd {
	u8 gpd_flags;
	u16 non_used; /* original checksum bits, now for debug:1 for Tx in; 2 for Tx done */
	u8 debug_id;
	u32 next_gpd_ptr_h;
	u32 next_gpd_ptr_l;
	u32 data_buff_bd_ptr_h;
	u32 data_buff_bd_ptr_l;
	u16 data_buff_len;
	u16 non_used1;
} __packed;

struct cldma_tbd {
	u8 bd_flags;
	u8 non_used; /* original checksum bits */
	u16 non_used2;
	u32 next_bd_ptr_h;
	u32 next_bd_ptr_l;
	u32 data_buff_ptr_h;
	u32 data_buff_ptr_l;
	u16 data_buff_len;
	u16 non_used1;
} __packed;

struct cldma_rgpd {
	u8 gpd_flags;
	u8 non_used; /* original checksum bits */
	u16 data_allow_len;
	u32 next_gpd_ptr_h;
	u32 next_gpd_ptr_l;
	u32 data_buff_bd_ptr_h;
	u32 data_buff_bd_ptr_l;
	u16 data_buff_len;
	u8 non_used1;
	u8 debug_id;
} __packed;

#define QUEUE_LEN(a) (sizeof(a)/sizeof(struct md_cd_queue))


static inline void md_cd_queue_struct_init(
		struct md_cd_queue *queue,
		unsigned char hif_id,
		DIRECTION dir,
		unsigned char index)
{
	queue->dir = dir;
	queue->index = index;
	queue->hif_id = hif_id;
	queue->tr_ring = NULL;
	queue->tr_done = NULL;
	queue->tx_xmit = NULL;
	init_waitqueue_head(&queue->req_wq);
	spin_lock_init(&queue->ring_lock);
	queue->busy_count = 0;
}

struct coda_t {
	char *name;
	char *hand;
	unsigned long cldma_phy_base;
	unsigned int offset;
	unsigned int len;
	unsigned int is_rh;
	unsigned int is_ao;
};

static struct coda_t ccci_coda_cldma[] = {

	{"CLDMA INDMA AO", "RIGHT", CLDMA_R_AO_BASE, 0x0, 0x400, 1, 1,},

	{"CLDMA OUTDMA AO", "RIGHT", CLDMA_R_AO_BASE, 0x400, 0x400, 1, 1,},

	{"CLDMA MISC AO", "RIGHT", CLDMA_R_AO_BASE, 0x800, 0x200, 1, 1,},

	{"CLDMA INDMA PD", "RIGHT", CLDMA_R_PD_BASE, 0x0, 0x400, 1, 0,},

	{"CLDMA OUTDMA PD", "RIGHT", CLDMA_R_PD_BASE, 0x400, 0x400, 1, 0,},

	{"CLDMA MISC PD", "RIGHT", CLDMA_R_PD_BASE, 0x800, 0x200, 1, 0,},

	{"CLDMA INDMA AO", "LEFT", CLDMA_L_AO_BASE, 0x0, 0x400, 0, 1,},

	{"CLDMA OUTDMA AO", "LEFT", CLDMA_L_AO_BASE, 0x400, 0x400, 0, 1,},

	{"CLDMA MISC AO", "LEFT", CLDMA_L_AO_BASE, 0x800, 0x200, 0, 1,},

	{"CLDMA INDMA PD", "LEFT", CLDMA_L_PD_BASE, 0x0, 0x400, 0, 0,},

	{"CLDMA OUTDMA PD", "LEFT", CLDMA_L_PD_BASE, 0x400, 0x400, 0, 0,},

	{"CLDMA MISC PD", "LEFT", CLDMA_L_PD_BASE, 0x800, 0x200, 0, 0,},

};

#endif	/* __CCCI_HIF_CLDMA_H__ */
