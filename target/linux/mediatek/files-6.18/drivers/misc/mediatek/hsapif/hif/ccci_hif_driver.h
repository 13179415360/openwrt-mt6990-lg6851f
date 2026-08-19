/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_HIF_DRIVER_H__
#define __CCCI_HIF_DRIVER_H__

#include "ccci_config.h"
#include "ccci_msg_data.h"
//#include "ccci_hif.h"
#include "ccci_core.h"
#include "ccci_hif_cldma.h"


typedef enum {
	DUMP_IP_BUSY_INIT = 0,
	DUMP_IP_BUSY_TX_DONE_INT = 1,
	DUMP_IP_BUSY_RX_DONE_INT = 2,
	DUMP_IP_BUSY_PM_SUSPEND = 3,
	DUMP_IP_BUSY_PM_RESUME = 4,
	DUMP_IP_BUSY_TX_ALL_DONE = 5,
	DUMP_IP_BUSY_RX_ALL_DONE = 6
} DUMP_CLDMA_IP_BUSY_STAGE;

typedef enum {
	CLEAR_IP_BUSY_RX_0 = 0,
	CLEAR_IP_BUSY_RX_OTHERS = 1
} CLEAR_CLDMA_IP_BUSY_STAGE;




extern struct ccci_cldma_hw_ctrl *ccci_hw_ctrls[HIF_ID_MAX];
extern struct ccci_md_cd_ctrl *ccci_md_ctrls[HIF_ID_MAX];


static inline void ccci_hif_reset_seq_num(
		struct ccci_hif_traffic *traffic_info)
{
	/* it's redundant to use 2 arrays, but this makes sequence checking easy */
	memset(traffic_info->seq_nums[OUT], 0, sizeof(traffic_info->seq_nums[OUT]));
	memset(traffic_info->seq_nums[IN], 0, sizeof(traffic_info->seq_nums[IN]));
}

static inline void ccci_hif_inc_tx_seq_num(
		struct ccci_hif_traffic *traffic_info,
		struct ccci_header *ccci_h)
{
	u16 channel = GET_CH_INDEX(ccci_h->channel);

	if (channel >= ARRAY_SIZE(traffic_info->seq_nums[OUT])) {
		CCCI_ERROR_LOG(-1, "hif",
			"[%s] ignore seq inc on channel %x\n",
			__func__, *(((u32 *) ccci_h) + 2));
		return;		/* for force assert channel, etc. */
	}

	ccci_h->seq_num = traffic_info->seq_nums[OUT][channel]++;
	ccci_h->assert_bit = 1;
	/* for rpx channel, can only set assert_bit when md is in single-task phase. */
	/* when md is in multi-task phase, assert bit should be 0, since ipc task are preemptible */
	//if ((ccci_h->channel == CCCI_RPC_TX || ccci_h->channel == CCCI_FS_TX) &&
	//		ccci_fsm_get_md_state(md_id) != BOOT_WAITING_FOR_HS2)
	//	ccci_h->assert_bit = 0;

	CCCI_NORMAL_LOG(-1, "hif",
			"[%s] channel %d assert_bit %d seq_num %d(%x) (data: %X, %d(%X))\n",
			__func__, ccci_h->channel, ccci_h->assert_bit, ccci_h->seq_num,
			ccci_h->seq_num, ccci_h->data[0], ccci_h->data[1], ccci_h->data[1]);
}

static inline void ccci_hif_check_rx_seq_num(
		struct ccci_hif_traffic *traffic_info,
		struct ccci_header *ccci_h,
		int qno)
{
	u16 channel, seq_num;
	//unsigned int param[3] = {0};

	channel = GET_CH_INDEX(ccci_h->channel);
	seq_num = ccci_h->seq_num;

	if (ccci_h->assert_bit && traffic_info->seq_nums[IN][channel] != 0 &&
		((seq_num - traffic_info->seq_nums[IN][channel]) & 0x7FFF) != 1) {

		CCCI_ERROR_LOG(-1, "hif",
			"[%s] channel %d seq number out-of-order %d->%d (data: %X, %X)\n",
			__func__, channel, seq_num, traffic_info->seq_nums[IN][channel],
			ccci_h->data[0], ccci_h->data[1]);

		//ccci_md_dump_info(md_id, DUMP_FLAG_DATA_HIF, NULL, qno);
		//param[0] = channel;
		//param[1] = traffic_info->seq_nums[IN][channel];
		//param[2] = seq_num;
		//ccci_md_force_assert(md_id, MD_FORCE_ASSERT_BY_MD_SEQ_ERROR, (char *)param, sizeof(param));

	}

	CCCI_NORMAL_LOG(-1, "hif",
			"[%s] channel %d assert_bit %d new_seq_num %d(%x) old_seq_num %d(%x) (data: %X, %d(%X))\n",
			__func__, ccci_h->channel, ccci_h->assert_bit, seq_num, seq_num,
			traffic_info->seq_nums[IN][channel], traffic_info->seq_nums[IN][channel],
			ccci_h->data[0], ccci_h->data[1], ccci_h->data[1]);

	traffic_info->seq_nums[IN][channel] = seq_num;
}
/*
static inline unsigned int ccci_hif_get_seq_num(
		struct ccci_hif_traffic *traffic_info,
		DIRECTION dir,
		CCCI_CH ch)
{
	return traffic_info->seq_nums[dir][ch];
}
*/
extern int ccci_cldma_hw_init(
		ccci_init_data_t *pdata);

extern int ccci_cldma_hif_init(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data);

extern int ccci_cldma_hw_set_info(
		struct ccci_cldma_hw_ctrl *hw_ctrl);

extern void ccci_cldma_hw_set_start_address(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned char qno,
		u64 address ,
		unsigned char rx_tx);

#ifndef __CLDMA_SUPPORT_AOC_V2__
extern void ccci_cldma_hw_set_start_address_ao_backup(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned char qno,
		u64 address ,
		unsigned char rx_tx);
#endif

extern void ccci_cldma_hw_queue_start(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		u8 qno,
		unsigned char tx_rx);

extern void ccci_cldma_hw_start(
		struct ccci_cldma_hw_ctrl *hw_ctrl);

extern void ccci_cldma_hw_stop_queue(
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		u8 qno,
		u8 rx_tx);

extern void ccci_cldma_hw_stop(
		struct ccci_cldma_hw_ctrl *hw_ctrl);

extern unsigned int ccci_cldma_hw_get_status(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask,
		unsigned char tx_rx);

extern unsigned int ccci_cldma_hw_eqirq_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char tx_rx);

extern unsigned int ccci_cldma_hw_txrxirq_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char tx_rx);

extern void ccci_cldma_hw_error_check(
		struct ccci_cldma_hw_info *hw_info);

extern unsigned int ccci_cldma_hw_tx_done(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask);

extern unsigned int ccci_cldma_hw_rx_done(
		struct ccci_cldma_hw_info *hw_info,
		unsigned int bitmask);

extern void ccci_cldma_hw_mask_txrxirq(
		struct ccci_cldma_hw_info *hw_info ,
		unsigned char qno,
		unsigned char tx_rx);

extern void ccci_cldma_hw_mask_eqirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern void ccci_cldma_hw_resume_queue(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern void ccci_cldma_hw_dismask_txrxirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern void ccci_cldma_hw_dismask_eqirq(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern unsigned int ccci_cldma_hw_queue_isactive(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern unsigned int ccci_cldma_hw_queue_ismask(
		struct ccci_cldma_hw_info *hw_info,
		unsigned char qno,
		unsigned char tx_rx);

extern unsigned int ccci_cldma_hw_restore_done(
		struct ccci_cldma_hw_info *hw_info);

extern void clear_cldma0_ip_busy_status_reg(CLEAR_CLDMA_IP_BUSY_STAGE);
extern void dump_cldma0_ip_busy_status_reg(DUMP_CLDMA_IP_BUSY_STAGE);

#endif	/* __CCCI_HIF_DRIVER_H__ */
