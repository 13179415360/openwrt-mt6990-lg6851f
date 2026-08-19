// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/dmapool.h>
#include <linux/interrupt.h>
#include <linux/syscore_ops.h>
#include <mt-plat/aee.h>

#include "ccci_hif_cldma.h"
#include "ccci_hif_driver.h"

#include "ccci_msg_id.h"
#include "ccci_msg_data.h"
#include "ccci_msg_center.h"
#include "ccci_hif.h"
#include "ccci_fsm_cldma.h"
#include "ccci_state_mgr.h"
//#include "ccci_bm.h"

#include "mt-plat/mtk_mhccif.h"
#include <linux/sched/clock.h> /* LG6851F Stage5I: local_clock declaration */


#define TAG "cldma_hif"
/*Workaround (Enable __CLDMA_RX_POLLING_MODE__):for not receive RX DONE Interrupt*/
//#define __CLDMA_RX_POLLING_MODE__


struct ccci_md_cd_ctrl *ccci_md_ctrls[HIF_ID_MAX] = {NULL};


//static int net_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { 0, 0, 0, NET_RX_BUF, NET_RX_BUF, NET_RX_BUF, 0, NET_RX_BUF };
static int normal_rx_queue_buffer_size[CLDMA_RXQ_NUM] = { SKB_4K, SKB_4K, SKB_4K, SKB_4K, SKB_4K, SKB_4K, SKB_4K, SKB_4K };
//static int net_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 0, 0, 0, 256, 256, 64, 0, 16 };
//static int net_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 0, 0, 0, 256, 256, 64, 0, 16 };
static int normal_rx_queue_buffer_number[CLDMA_RXQ_NUM] = { 16, 16, 16, 16, 16, 16, 16, 16 };
static int normal_tx_queue_buffer_number[CLDMA_TXQ_NUM] = { 16, 16, 16, 16, 16, 16, 16, 16 };

//static int net_rx_queue2ring[CLDMA_RXQ_NUM] = { -1, -1, -1, 0, 1, 2, -1, 3 };
//static int net_tx_queue2ring[CLDMA_TXQ_NUM] = { -1, -1, -1, 0, 1, 2, -1, 3 };
static int normal_rx_queue2ring[CLDMA_RXQ_NUM] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static int normal_tx_queue2ring[CLDMA_TXQ_NUM] = { 0, 1, 2, 3, 4, 5, 6, 7 };
//static int net_rx_ring2queue[NET_RXQ_NUM] = { 3, 4, 5, 7 };
//static int net_tx_ring2queue[NET_TXQ_NUM] = { 3, 4, 5, 7 };
static int normal_rx_ring2queue[NORMAL_RXQ_NUM] = { 0, 1, 2, 3, 4, 5, 6, 7};
static int normal_tx_ring2queue[NORMAL_TXQ_NUM] = { 0, 1, 2, 3, 4, 5, 6, 7};


#define NET_TX_QUEUE_MASK 0x00
#define NET_RX_QUEUE_MASK 0x00
#define NORMAL_TX_QUEUE_MASK 0xFF
#define NORMAL_RX_QUEUE_MASK 0xFF
#define NONSTOP_QUEUE_MASK 0xF0	/* Rx, for convenience, queue 0,1,2,3 are non-stop */
#define NONSTOP_QUEUE_MASK_32 0xF0F0F0F0
#define CLDMA_MONITOR_TX_Q_TIMER_SEC 10
#define CLDMA_CHECK_TX_Q_MSEC 10
#define CLDMA_TX_RX_POLLING_MODE_TIMER_SEC 10
#define CLDMA_TX_RX_POLLING_MODE_LONG_TIMER_SEC 3600



//#define IS_NET_QUE(md_id, qno) ((ccci_md_in_ee_dump(md_id) == 0) && ((1<<qno) & NET_RX_QUEUE_MASK))

static void cldma_md_cd_rxq0_tasklet(unsigned long data);
void cldma_gpd_dump(unsigned int q);
void cldma_regs_dump(void);
void cldma_regs_dump_impl(unsigned int len, unsigned int offset, unsigned int is_rh, unsigned int is_ao);

static inline struct cldma_request *cldma_ring_step_backward(
		struct cldma_ring *ring,
		struct cldma_request *req)
{
	if (req->entry.prev == &ring->gpd_ring)
		return list_first_entry(&ring->gpd_ring, struct cldma_request, entry);

	else
		return list_entry(req->entry.prev, struct cldma_request, entry);
}

static inline struct cldma_request *cldma_ring_step_forward(
		struct cldma_ring *ring,
		struct cldma_request *req)
{
	/* it means req is the last one and needs to start from list's first */
	if (req->entry.next == &ring->gpd_ring)
		return list_first_entry(&ring->gpd_ring, struct cldma_request, entry);

	else
		return list_entry(req->entry.next, struct cldma_request, entry);
}

static void cldma_tx_queue_empty_handler(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct md_cd_queue *queue)
{
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	int pending_gpd = 0;

	if (md_ctrl->txq_active & (1 << queue->index)) {
		/* check if there is any pending TGPD with HWO=1 */
		spin_lock_irqsave(&queue->ring_lock, flags);

		req = cldma_ring_step_backward(queue->tr_ring, queue->tx_xmit);

		tgpd = (struct cldma_tgpd *)req->gpd;

		//if ((tgpd->gpd_flags & 0x1) && req->skb)
		if ((tgpd->gpd_flags & 0x1) && req->data)
			pending_gpd = 1;

		spin_unlock_irqrestore(&queue->ring_lock, flags);

		/* resume channel */
		if (pending_gpd) {
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

			ccci_cldma_hw_resume_queue(ccci_hw_ctrls[md_ctrl->hif_id]->hw_info, queue->index, 0);

			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

			CCCI_DEBUG_LOG(-1, TAG, "[%s] resume txq %d in tx empty\n",
				__func__, queue->index);
		}
	}
}

static void cldma_tx_queue_handle(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned int L2TISAR0)
{
	int i, ret;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] L2TISAR0: %x\n", __func__, L2TISAR0);

#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_TX_IRQ, (L2TISAR0 & TXRX_STATUS_BITMASK));
#endif
	if (md_ctrl->tx_busy_warn_cnt && (L2TISAR0 & TXRX_STATUS_BITMASK))
		md_ctrl->tx_busy_warn_cnt = 0;

	ccci_cldma_hw_tx_done(hw_ctrl->hw_info, L2TISAR0);

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		if ((L2TISAR0 & TXRX_STATUS_BITMASK) & (1 << i)) {

#ifdef ENABLE_CLDMA_TIMER
			if (IS_NET_QUE(md_ctrl->md_id, i)) {
				md_ctrl->txq[i].timeout_end = local_clock();
				ret = del_timer(&md_ctrl->txq[i].timeout_timer);
				CCCI_DEBUG_LOG(-1, TAG, "qno%d del_timer %d ptr=0x%p\n", i, ret,
					     &md_ctrl->txq[i].timeout_timer);
			}
#endif
			/* disable TX_DONE interrupt */
			ccci_cldma_hw_mask_eqirq(hw_ctrl->hw_info , i, 0);
			ccci_cldma_hw_mask_txrxirq(hw_ctrl->hw_info , i, 0);

			ret = queue_delayed_work(md_ctrl->txq[i].worker,
					&md_ctrl->txq[i].cldma_tx_work,	msecs_to_jiffies(0));

			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] txq%d queue work = %d\n",
				__func__, i, ret);
		}

		if (L2TISAR0 & (EMPTY_STATUS_BITMASK & ((1 << i) << EQ_STA_BIT_OFFSET))) {
			cldma_tx_queue_empty_handler(md_ctrl, &md_ctrl->txq[i]);
		}
	}
}

static int cldma_rx_worker_start(
		struct ccci_md_cd_ctrl *md_ctrl,
		int qno)
{
	int ret = 0;

	if (qno < 0 || qno >= CLDMA_RXQ_NUM) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] invalid qno %d\n",
			__func__, qno);
		return -1;
	}

	if (qno != 0)
		ret = queue_work(md_ctrl->rxq[qno].worker, &md_ctrl->rxq[qno].cldma_rx_work);

	else
		tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);

	return ret;
}

static void cldma_rx_queue_handle(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl,
		unsigned int L2RISAR0)
{
	int i;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] L2RISAR0: %x\n", __func__, L2RISAR0);

#ifdef CLDMA_TRACE
		trace_cldma_irq(CCCI_TRACE_RX_IRQ, (L2RISAR0 & TXRX_STATUS_BITMASK));
#endif
		/* clear IP busy register wake up cpu case */
//		cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY,
//			      cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY));
	ccci_cldma_hw_rx_done(hw_ctrl->hw_info, L2RISAR0);

	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
		if ((L2RISAR0 & (1 << i)) || (L2RISAR0 & ((1 << i) << EQ_STA_BIT_OFFSET))) {
			//md_ctrl->traffic_info.latest_q_rx_isr_time[i] = local_clock();

			/* disable RX_DONE and QUEUE_EMPTY interrupt */
			ccci_cldma_hw_mask_eqirq(hw_ctrl->hw_info, i, 1);
			ccci_cldma_hw_mask_txrxirq(hw_ctrl->hw_info, i, 1);

			/*always start work due to no napi*/
			cldma_rx_worker_start(md_ctrl, i);
		}
	}
}

static void cldma_irq_work_cb(void *data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] hif: %d; CLDMA IRQ!\n",
		__func__, md_ctrl->hif_id);

	//md_ctrl->traffic_info.latest_isr_time = local_clock();

	/* get L2 interrupt status */
	L2TISAR0 = ccci_cldma_hw_get_status(hw_info,
				EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK, 0);
	L2RISAR0 = ccci_cldma_hw_get_status(hw_info,
				EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK, 1);

	L2TIMR0 = ccci_cldma_hw_eqirq_ismask(hw_info, 0) |
				ccci_cldma_hw_txrxirq_ismask(hw_info, 0);
	L2RIMR0 = ccci_cldma_hw_eqirq_ismask(hw_info, 1) |
				ccci_cldma_hw_txrxirq_ismask(hw_info, 1);

	CCCI_DEBUG_LOG(-1, TAG, "[%s] wakeup_src: %d; CLDMA IRQ L2 " \
		"STATUS:(Tx:%x/Rx:%x),MASK:(Tx_Msk:%x/Rx_Msk:%x)\n",
		__func__, atomic_read(&md_ctrl->wakeup_src),
		L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);

	if (unlikely(0 == (L2TISAR0 | L2RISAR0)))
		ccci_cldma_hw_error_check(hw_info);

	L2TISAR0 &= (~L2TIMR0);
	L2RISAR0 &= (~L2RIMR0);

#ifndef __CLDMA_TX_POLLING_MODE__
	if (L2TISAR0)
		cldma_tx_queue_handle(md_ctrl, hw_ctrl, L2TISAR0);
#endif

#ifndef __CLDMA_RX_POLLING_MODE__
	/*Once RX POLLING workaround is used, not expect to call this due to TX_DONE Interrupt*/
	if (L2RISAR0)
		cldma_rx_queue_handle(md_ctrl, hw_ctrl, L2RISAR0);
#endif

}

#if (defined(__CLDMA_RX_POLLING_MODE__) || defined(__CLDMA_TX_POLLING_MODE__))
static void cldma_irq_work_polling_mode(void *data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;
	unsigned int L2TIMR0, L2RIMR0, L2TISAR0, L2RISAR0;
	int notify_tx_irq = 0, notify_rx_irq = 0;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] hif: %d; CLDMA IRQ!\n",
		__func__, md_ctrl->hif_id);

	//md_ctrl->traffic_info.latest_isr_time = local_clock();

	/* get L2 interrupt status */
	L2TISAR0 = ccci_cldma_hw_get_status(hw_info,
				EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK, 0);
	L2RISAR0 = ccci_cldma_hw_get_status(hw_info,
				EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK, 1);

	L2TIMR0 = ccci_cldma_hw_eqirq_ismask(hw_info, 0) |
				ccci_cldma_hw_txrxirq_ismask(hw_info, 0);
	L2RIMR0 = ccci_cldma_hw_eqirq_ismask(hw_info, 1) |
				ccci_cldma_hw_txrxirq_ismask(hw_info, 1);

	CCCI_DEBUG_LOG(-1, TAG, "[%s] wakeup_src: %d; CLDMA IRQ L2 " \
		"STATUS:(Tx:%x/Rx:%x),MASK:(Tx_Msk:%x/Rx_Msk:%x)\n",
		__func__, atomic_read(&md_ctrl->wakeup_src),
		L2TISAR0, L2RISAR0, L2TIMR0, L2RIMR0);

	if (unlikely(0 == (L2TISAR0 | L2RISAR0)))
		ccci_cldma_hw_error_check(hw_info);

	L2TISAR0 &= (~L2TIMR0);
	L2RISAR0 &= (~L2RIMR0);

#ifdef __CLDMA_TX_POLLING_MODE__
	if (L2TISAR0) {
		notify_tx_irq = 1;
		cldma_tx_queue_handle(md_ctrl, hw_ctrl, L2TISAR0);
	}
#endif

#ifdef __CLDMA_RX_POLLING_MODE__
	if (L2RISAR0) {
		notify_rx_irq = 1;
		cldma_rx_queue_handle(md_ctrl, hw_ctrl, L2RISAR0);
	}
#endif

	if ((notify_tx_irq == 1) || (notify_rx_irq == 1)) {
		CCCI_DEBUG_LOG(-1, TAG,
		"[%s] Detect CLDMA0 HW Reg: notify_tx_irq=%d, notify_rx_irq=%d\n",
		__func__, notify_tx_irq, notify_rx_irq);
		mod_timer(&md_ctrl->cldma_polling_mode_timer,
			jiffies + CLDMA_TX_RX_POLLING_MODE_LONG_TIMER_SEC*HZ);

	} else {
		CCCI_DEBUG_LOG(-1, TAG,
		"[%s] Not detect CLDMA0 HW IRQ! re-arrange short timer to polling\n",
		__func__);
		mod_timer(&md_ctrl->cldma_polling_mode_timer,
			jiffies + CLDMA_TX_RX_POLLING_MODE_TIMER_SEC*HZ);

	}

}
#endif


static irqreturn_t cldma_isr(int irq, void *data)
{
	cldma_irq_work_cb(data);

	return IRQ_HANDLED;
}

static void cldma_irq_work(struct work_struct *work)
{
	cldma_irq_work_cb(container_of(work,
		struct ccci_md_cd_ctrl,	cldma_irq_work));
}


static void cldma_register_isr(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	int ret;

	ret = request_irq(hw_ctrl->hw_info->cldma_irq_id,
			cldma_isr,
			hw_ctrl->hw_info->cldma_irq_flags,
			"CLDMA_IRQ",
			(void *)md_ctrl);


	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] request CLDMA_IRQ: %llu error %d\n",
			__func__, hw_ctrl->hw_info->cldma_irq_flags, ret);
		return;
	}

	atomic_set(&md_ctrl->cldma_irq_enabled, 1);

}

static inline void cldma_disable_irq(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 1, 0) == 1) {
		disable_irq(hw_ctrl->hw_info->cldma_irq_id);
	}
}

static inline void cldma_enable_irq(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	if (atomic_cmpxchg(&md_ctrl->cldma_irq_enabled, 0, 1) == 0) {
		enable_irq(hw_ctrl->hw_info->cldma_irq_id);
	}
}

static void cldma_monitor_tx_status (void)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	int qinx = 0;
	int number_tx_queue_done = 0;
	unsigned long flags;
	for (qinx=0; qinx < CLDMA_TXQ_NUM; qinx++) {
		struct md_cd_queue * queue = &md_ctrl->txq[qinx];
		spin_lock_irqsave(&queue->ring_lock, flags);
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] HW txq inx=%d, budget=%d\n",
			__func__, qinx, queue->budget);
		if(queue->budget == queue->tr_ring->length) {
			//Each TGPD's HWO bit should be 0
			struct list_head *listptr;
			list_for_each(listptr, &queue->tr_ring->gpd_ring) {
				struct cldma_request *req;
				struct cldma_tgpd *tgpd;
				req = list_entry(listptr, struct cldma_request, entry);
				tgpd = (struct cldma_tgpd *)req->gpd;
				CCCI_DEBUG_LOG(-1, TAG,
					"[%s] HW tx status: qinx=%d,req=0x%px,gpd_flags=%d, req_data=0x%px\n",
					__func__, qinx, req, tgpd->gpd_flags, req->data);
			}
			number_tx_queue_done++;
		}
		spin_unlock_irqrestore(&queue->ring_lock, flags);
	}

	if (number_tx_queue_done == CLDMA_TXQ_NUM) {
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] HW tx done for TXQ\n",
			__func__);
		ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_SEND_HOST_PM_SUSPEND_ACK, NULL);
#if 0
		/*Send PCIE_SUSPEND_ACK back to Host*/
		mtk_mhccif_irq_to_host(MHCCIF_D2H_INT_DIPC);
		ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
				CLA_SEND_HOST_SUSPEND_ACK,
				NULL);
#endif
	} else {
		/*HW TXQ is still ongoing - check its status later again*/
		/*Wait for msecs to complete HW TXQ transmission*/
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] HW not done for TXQ, check later!\n",
			__func__);
		queue_delayed_work(md_ctrl->cldma_pcie_irq_worker,
			&(md_ctrl->cldma_pcie_irq_work),
			msecs_to_jiffies(CLDMA_CHECK_TX_Q_MSEC));
#if 0
		mod_timer(&md_ctrl->cldma_monitor_tx_status_timer, jiffies + CLDMA_MONITOR_TX_Q_TIMER_SEC*HZ);
#endif

	}

}


static void cldma_pcie_irq_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct ccci_md_cd_ctrl *md_ctrl = (container_of(dwork,
				struct ccci_md_cd_ctrl,	cldma_pcie_irq_work));
	u32 suspend_type = MHCCIF_H2D_INT_PCIE_PM_SUSPEND_REQ_AP;
	u32 resume_type = MHCCIF_H2D_INT_PCIE_PM_RESUME_REQ_AP;
	int ret = 0;

	CCCI_ERROR_LOG(-1, TAG,
			"[%s] md_ctrl's pcie_irq_type: %d error %d\n",
			__func__, md_ctrl->cldma_pcie_irq_type, ret);

	if(md_ctrl->cldma_pcie_irq_type == suspend_type) {
		CLDMA_STATE_T cldma_state;
		cldma_state = ccci_cldma_state_get();

		/*Change Host state to stop upper layer's app to execute TX transaction*/
		if (cldma_state == CLA_READY) {
			ret = ccci_fsm_cldma_add_cmd(
				CCCI_FSM_CMD_RECV_HOST_PM_SUSPEND_REQ, NULL);

#if 0
			/*Change state*/
			ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
				CLA_RECV_HOST_SUSPEND_REQ,
				NULL);
			/*Wait for msecs to complete HW TXQ transmission*/
			queue_delayed_work(md_ctrl->cldma_pcie_irq_worker,
				&(md_ctrl->cldma_pcie_irq_work),
				msecs_to_jiffies(CLDMA_CHECK_TX_Q_MSEC));
#endif
		} else if (cldma_state == CLA_RECV_HOST_SUSPEND_REQ) {
			/*Check HW TXQ status*/
			CCCI_ERROR_LOG(-1, TAG,
			"[%s] Call cldma_monitor_tx_status(),pcie_irq_type: %d\n",
			__func__, md_ctrl->cldma_pcie_irq_type);
			cldma_monitor_tx_status();
		} else {
			/*Something wrong here!*/
			CCCI_ERROR_LOG(-1, TAG,
			"[%s] Warning! Wrong!\n",
			__func__);
		}

	} else if (md_ctrl->cldma_pcie_irq_type == resume_type) {
		/*Change Host state to allow userspace's app to send Remote Wakeup command*/
		ret = ccci_fsm_cldma_add_cmd(
				CCCI_FSM_CMD_RECV_HOST_PM_RESUME_REQ, NULL);
	} else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] Do nothing!\n",
			__func__);
	}

}


static int cldma_schedule_check_hw_txq_work(
	int msg_id,
	unsigned int sub_id,
	void *msg_data,
	void *my_data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] md_ctrl's pcie_irq_type: %d prepare delayed work\n",
		__func__, md_ctrl->cldma_pcie_irq_type);

	/*Wait for msecs to complete HW TXQ transmission*/
	queue_delayed_work(md_ctrl->cldma_pcie_irq_worker,
		&(md_ctrl->cldma_pcie_irq_work),
		msecs_to_jiffies(CLDMA_CHECK_TX_Q_MSEC));
	return 0;
}

static irqreturn_t cldma_pcie_suspend_isr(int irq, void *data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)data;
	md_ctrl->cldma_pcie_irq_type = MHCCIF_H2D_INT_PCIE_PM_SUSPEND_REQ_AP;
	queue_delayed_work(md_ctrl->cldma_pcie_irq_worker,
		&(md_ctrl->cldma_pcie_irq_work),
		msecs_to_jiffies(0));

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] Recv ISR\n",
		__func__);

	return IRQ_HANDLED;
}

static irqreturn_t cldma_pcie_resume_isr(int irq, void *data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)data;
	md_ctrl->cldma_pcie_irq_type = MHCCIF_H2D_INT_PCIE_PM_RESUME_REQ_AP;
	queue_delayed_work(md_ctrl->cldma_pcie_irq_worker,
		&(md_ctrl->cldma_pcie_irq_work),
		msecs_to_jiffies(0));

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] Recv ISR\n",
		__func__);

	return IRQ_HANDLED;
}




static void cldma_register_pcie_isr(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct ccci_cldma_hw_ctrl *hw_ctrl)
{
	u32 alloc_irq_id;
	u32 suspend_irq_id = MHCCIF_H2D_INT_PCIE_PM_SUSPEND_REQ_AP;
	u32 resume_irq_id = MHCCIF_H2D_INT_PCIE_PM_RESUME_REQ_AP;
	int ret;

	alloc_irq_id = mtk_mhccif_alloc_irq(suspend_irq_id);
	ret = request_irq(alloc_irq_id,
		cldma_pcie_suspend_isr,
		IRQF_SHARED,
		"mhccif_suspend_irq",
		(void *)md_ctrl);

	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] request mhccif_suspend_irq: %d error %d\n",
			__func__, alloc_irq_id, ret);
		return;
	}

	alloc_irq_id = mtk_mhccif_alloc_irq(resume_irq_id);
	ret = request_irq(alloc_irq_id,
		cldma_pcie_resume_isr,
		IRQF_SHARED,
		"mhccif_resume_irq",
		(void *)md_ctrl);

	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] request mhccif_suspend_irq: %d error %d\n",
			__func__, alloc_irq_id, ret);
		return;
	}

}






static inline void cldma_tgpd_set_data_ptr(
		struct cldma_tgpd *tgpd,
		dma_addr_t data_ptr)
{
#ifdef CONFIG_64BIT
	cldma_write32(&tgpd->data_buff_bd_ptr_h, 0, (u32)(data_ptr >> 32));
#endif
	cldma_write32(&tgpd->data_buff_bd_ptr_l, 0, (u32)data_ptr);


}

/* this is called inside queue->ring_lock */
int cldma_gpd_tx_request(
		struct md_cd_queue *queue,
		struct cldma_request *tx_req,
		//struct sk_buff *skb,
		ccci_kmem_data_t *data,
		unsigned int ioc_override)
{
	struct cldma_tgpd *tgpd;
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[queue->hif_id];
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[queue->hif_id];

	CCCI_DEBUG_LOG(-1, TAG, "[%s]\n", __func__);

	tgpd = tx_req->gpd;
	/* override current IOC setting */
	if (ioc_override & 0x80) {
		tx_req->ioc_override = 0x80 | (!!(tgpd->gpd_flags & 0x80));	/* backup current IOC setting */
		if (ioc_override & 0x1)
			tgpd->gpd_flags |= 0x80;
		else
			tgpd->gpd_flags &= 0x7F;
	}
	//((struct ccci_header *)skb->data)->channel |= 0x1000;
	//((struct ccci_header *)data->off_data)->channel += 0x1000;
	/* update GPD */
	//tx_req->data_buffer_ptr_saved = dma_map_single(hw_ctrl->dma_dev,
	//		skb->data, skb->len, DMA_TO_DEVICE);
	tx_req->data_buffer_ptr_saved = dma_map_single(hw_ctrl->dma_dev,
			data->off_data, data->data_len, DMA_TO_DEVICE);

	if (dma_mapping_error(hw_ctrl->dma_dev, tx_req->data_buffer_ptr_saved)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] error dma mapping\n", __func__);
		return -1;
	}

	cldma_tgpd_set_data_ptr(tgpd, tx_req->data_buffer_ptr_saved);
	cldma_write16(&tgpd->data_buff_len, 0, data->data_len);
	tgpd->non_used = 1;
	/*
	 * set HWO
	 * use cldma_timeout_lock to avoid race conditon with cldma_stop. this lock must cover TGPD setting, as even
	 * without a resume operation, CLDMA still can start sending next HWO=1 TGPD if last TGPD was just finished.
	 */
	spin_lock(&md_ctrl->cldma_timeout_lock);

	if (md_ctrl->txq_active & (1 << queue->index))
		cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) | 0x1);

	spin_unlock(&md_ctrl->cldma_timeout_lock);

	/* mark cldma_request as available */
	//tx_req->skb = skb;
	tx_req->data = data;

	return 0;
}

#ifndef __CLDMA_SUPPORT_AOC_V2__
int hsapif_cldma_restore_reg(void)
{
	unsigned long flags;
	u32 cldma_ro_status = 0;
	u32 pdn_write_value = 0, pdn_read_value = 0;
	dma_addr_t ul_start_addr_h_0 = 0, ul_start_addr_l_0 = 0;
	dma_addr_t ul_current_addr_h_0 = 0, ul_current_addr_l_0 = 0;
	dma_addr_t ao_backup_addr_h = 0, ao_backup_addr_1 = 0, ao_bk_addr = 0;
	int i;
	struct ccci_md_cd_ctrl *md_ctrl;
	struct ccci_cldma_hw_ctrl *hw_ctrl;
	struct ccci_cldma_hw_info *hw_info;
	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	hw_info = hw_ctrl->hw_info;

	/*Check PD domain TXQ#0 start address: REG_CLDMA_UL_START_ADDRL_0 and H_0*/
	ul_start_addr_l_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_ADDRL_0);
	ul_start_addr_h_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_ADDRH_0);

	ul_current_addr_l_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CURRENT_ADDRL_0);
	ul_current_addr_h_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CURRENT_ADDRH_0);

	if (ul_start_addr_l_0 || ul_start_addr_h_0)
	{
		/*If the value can be read from this reg, it means that infra is not powered-off => Infra PD domain is not reset!*/
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
		cldma_ro_status = cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_STATUS);
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] No need to restore: ro_status=%d, UL_START_ADDR_H_0=0x%llx, UL_START_ADDR_L_0=0x%llx\n",
			__func__, cldma_ro_status, ul_start_addr_h_0, ul_start_addr_l_0);

		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] UL_CURRENT_ADDR_H_0=0x%llx, UL_CURRENT_ADDR_L_0=0x%llx\n",
			__func__, ul_current_addr_h_0, ul_current_addr_l_0);

		/*Check AO domain: status of all RXQs */
		if (!cldma_ro_status)
		{
			/*It means that no RXQ channels are resumed & active due to start/resume for receiving*/
			/*Now, resume only RXQ#0 or all RXQs (TBD)*/
			CCCI_NORMAL_LOG(-1, TAG, "[%s] Resume RX chnls\n",__func__);
			cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_RESUME_CMD, 0x0F & 0x1);
			/*dummy read*/
			cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_RESUME_CMD);
		} else {
			CCCI_NORMAL_LOG(-1, TAG, "[%s] No need to resume REG_CLDMA_SO_STATUS(active now!)\n",__func__);
		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	} else {
		CCCI_NORMAL_LOG(-1, TAG, "[%s] Resume cldma pd registers:1\n",__func__);
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

		/*re-config address mode flag for pd register- ref to: ccci_cldma_hw_set_info()*/
		if (hw_info->hw_mode == MODE_BIT_64) {
			pdn_write_value = (0x4 << 5);

		} else if (hw_info->hw_mode == MODE_BIT_40) {
			pdn_write_value = (0x2 << 5);

		} else if (hw_info->hw_mode == MODE_BIT_36) {
			pdn_write_value = (0x1 << 5);

		} else {
			CCCI_NORMAL_LOG(0, TAG,
				"[%s] error: hw_mode value(%d) is incorrect\n",
				__func__, hw_info->hw_mode);
			return -1;
		}
		/*Because PD domain reg will be reset to 0 after power down - so pdn_read_value is the default value*/
		pdn_read_value = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CFG);
		/*PD domain - TX CFG mode: including address mode*/
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CFG,
			(pdn_read_value & (~(0x7 << 5))) | pdn_write_value);

		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_RESUME_CMD, 0x0F & 0x1);
		/*dummy read*/
		cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_SO_RESUME_CMD);

		/*Set start address for each TXQ*/
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			/*Check if AO domain reg has the backup value of previous value of PD domain reg*/
			if ((cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_UL_CURRENT_ADDRL_0_AO + i*8) == 0)
				&& (cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_UL_CURRENT_ADDRL_0_AO + i*8 + 4) == 0)) {

				CCCI_NORMAL_LOG(-1, TAG, "[%s] Restore start addr of TXQ=%d, tr_done_gpd_addr=0x%llx\n",
					__func__, i, md_ctrl->txq[i].tr_done->gpd_addr);
				/*Restore each TXQ's start address to PD domain regs*/
				ccci_cldma_hw_set_start_address(hw_ctrl,
					i,
					md_ctrl->txq[i].tr_done->gpd_addr,
					0);

				/*Set AO domain regs for backup PD domain usage*/
				ccci_cldma_hw_set_start_address_ao_backup(hw_ctrl,
					i,
					md_ctrl->txq[i].tr_done->gpd_addr,
					0);

			}else {
				/*Use AO domain backup regs to restore the PD domain regs*/
				ao_backup_addr_h = cldma_read32(hw_info->cldma_ap_ao_base,
										REG_CLDMA_UL_CURRENT_ADDRL_0_AO + i*8 + 4);

				ao_backup_addr_1 = cldma_read32(hw_info->cldma_ap_ao_base,
										REG_CLDMA_UL_CURRENT_ADDRL_0_AO + i*8);
#ifdef CONFIG_64BIT
				ao_bk_addr = ((ao_backup_addr_h << 32) | ao_backup_addr_1);
#else
				ao_bk_addr = ao_backup_addr_1;
#endif
				CCCI_NORMAL_LOG(-1, TAG, "[%s] Restore start address of TXQ=%d, ao_bk_addr=0x%llx\n",
					__func__, i, ao_bk_addr);

				ccci_cldma_hw_set_start_address(hw_ctrl,
					i,
					ao_bk_addr,
					0);

				/*Set AO domain regs for backup PD domain usage*/
				ccci_cldma_hw_set_start_address_ao_backup(hw_ctrl,
					i,
					ao_bk_addr,
					0);
			}


		}/*End of for-loop each TXQ check & restore*/

		/*wait write done*/
		wmb();
		/*start all Tx and Rx queues*/
		md_ctrl->txq_started = 0;
		//md_ctrl->txq_active |= 0x0F;
		md_ctrl->txq_active = 0xFF;

		/*enable L2 DONE and ERROR interrupts - ref to ccci_cldma_hw_start()*/
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L2TIMCR0,
			(HSAPIF_CLDMA_TX_INT_DONE | HSAPIF_CLDMA_TX_INT_QUEUE_EMPTY /*| HSAPIF_CLDMA_TX_INT_ERROR*/));

		/* enable all L3 interrupts */
#if 0
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3TIMCR0, HSAPIF_CLDMA_BM_INT_ALL);
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3TIMCR1, HSAPIF_CLDMA_BM_INT_ALL);
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3RIMCR0, HSAPIF_CLDMA_BM_INT_ALL);
		cldma_write32(hw_info->cldma_ap_pdn_base, REG_CLDMA_L3RIMCR1, HSAPIF_CLDMA_BM_INT_ALL);
#endif
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	}
	return 0;
}
#endif

int hsapif_cldma_syssuspend(void)
{
#ifdef __CLDMA_SUPPORT_AOC_V2__
	CCCI_NORMAL_LOG(-1, TAG, "[%s] Prepare backup PD regs\n",__func__);
	return 0;
#else
	unsigned long flags;
	u32 cldma_ro_status = 0;
	dma_addr_t ul_current_addr_h_0 = 0, ul_current_addr_l_0 = 0;
	dma_addr_t ul_addr_h = 0, ul_addr_1 = 0;
	int i;
	struct ccci_md_cd_ctrl *md_ctrl;
	struct ccci_cldma_hw_ctrl *hw_ctrl;
	struct ccci_cldma_hw_info *hw_info;
	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	hw_info = hw_ctrl->hw_info;

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	ul_addr_1 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_ADDRL_0);
	ul_addr_h = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_START_ADDRH_0);

	ul_current_addr_l_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CURRENT_ADDRL_0);
	ul_current_addr_h_0 = cldma_read32(hw_info->cldma_ap_pdn_base, REG_CLDMA_UL_CURRENT_ADDRH_0);

	cldma_ro_status = cldma_read32(hw_info->cldma_ap_ao_base, REG_CLDMA_SO_STATUS);

	CCCI_NORMAL_LOG(-1, TAG, "[%s] UL_START_ADDR_H_0=0x%llx, UL_START_ADDR_L_0=0x%llx, ro_status=%d\n",
		__func__, ul_addr_h, ul_addr_1, cldma_ro_status);

	CCCI_NORMAL_LOG(-1, TAG,
			"[%s] UL_CURRENT_ADDR_H_0=0x%llx, UL_CURRENT_ADDR_L_0=0x%llx\n",
			__func__, ul_current_addr_h_0, ul_current_addr_l_0);

	/*Dump all TXQ tr_done_gpd_addr for debugging*/
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
		CCCI_NORMAL_LOG(-1, TAG, "[%s] TXQ=%d, tr_done_gpd_addr=0x%llx\n",
			__func__, i, md_ctrl->txq[i].tr_done->gpd_addr);

	}

	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	return 0;
#endif
}

void hsapif_cldma_sysresume(void)
{
	CCCI_NORMAL_LOG(-1, TAG, "[%s] Prepare restoring PD regs\n", __func__);
#ifdef __CLDMA_SUPPORT_AOC_V2__
	struct ccci_md_cd_ctrl *md_ctrl;
	struct ccci_cldma_hw_ctrl *hw_ctrl;
	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	while (1) {
		if(ccci_cldma_hw_restore_done(hw_ctrl->hw_info)) {
			CCCI_NORMAL_LOG(-1, TAG, "[%s] Restore done PD regs\n", __func__);
			return;
		}
	}
#else
	hsapif_cldma_restore_reg();
#endif
}

static struct syscore_ops hsapif_sysops = {
        .suspend = hsapif_cldma_syssuspend,
        .resume = hsapif_cldma_sysresume,
};

void hsapif_obtain_wakelock(void) {

	struct ccci_md_cd_ctrl *md_ctrl;
	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] \n", __func__);
	__pm_stay_awake(md_ctrl->cldma0_wakelock);
}

void hsapif_release_wakelock(void) {

	struct ccci_md_cd_ctrl *md_ctrl;
	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] \n", __func__);
	__pm_relax(md_ctrl->cldma0_wakelock);
}


static int cldma_hif_init(struct ccci_md_cd_ctrl *md_ctrl)
{
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	int i;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] PD MISC CODA ver:[0x%px]=0x%08x\n",
		__func__, hw_ctrl->hw_info->cldma_ap_pdn_base + REG_CLDMA_CODA_VERSION,
		cldma_read32(hw_ctrl->hw_info->cldma_ap_pdn_base, REG_CLDMA_CODA_VERSION));

	CCCI_INIT_LOG(-1, TAG, "[%s] md_ctrl: %p; hif_id: %d\n",
		__func__, md_ctrl, md_ctrl->hif_id);

	//md_ctrl->ops = &ccci_hif_cldma_ops;
	md_ctrl->txq_active = 0;
	md_ctrl->rxq_active = 0;
	md_ctrl->is_late_init = 0;

	tasklet_init(&md_ctrl->cldma_rxq0_task,
			cldma_md_cd_rxq0_tasklet, (unsigned long)md_ctrl);

	spin_lock_init(&md_ctrl->cldma_timeout_lock);

//	CCCI_DEBUG_LOG(-1, TAG,
//		"[%s] QUEUE_LEN(md_ctrl->txq): %d; QUEUE_LEN(md_ctrl->rxq): %d\n",
//		__func__, QUEUE_LEN(md_ctrl->txq), QUEUE_LEN(md_ctrl->rxq));

	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++){
//		CCCI_DEBUG_LOG(-1, TAG,
//			"[%s] md_ctrl: %p; md_ctrl->txq[%d]: %p\n",
//			__func__, md_ctrl, i, &md_ctrl->txq[i]);

		md_cd_queue_struct_init(&md_ctrl->txq[i],
						md_ctrl->hif_id, OUT, i);

		//md_ctrl->txq[i].modem = ccci_md_get_modem_by_id(md_id);
	}

	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++){
//		CCCI_DEBUG_LOG(-1, TAG,
//			"[%s] md_ctrl->rxq[%d]: %p\n",
//			__func__, i, &md_ctrl->rxq[i]);

		md_cd_queue_struct_init(&md_ctrl->rxq[i],
						md_ctrl->hif_id, IN, i);

		//md_ctrl->rxq[i].modem = ccci_md_get_modem_by_id(md_id);
	}

	/*init virtual runtime data sram layout*/
	md_ctrl->cldma_sram_layout = kzalloc(sizeof(struct cldma_sram_layout), GFP_KERNEL);
    if (md_ctrl->cldma_sram_layout == NULL) {
        CCCI_ERROR_LOG(-1, TAG,
        	"[%s] error: Fail to kzalloc cldma_sram_layout\n",
        	__func__);
        return -1;
    }

	//md->mem_layout.ccci_rt_smem_size= CCCI_SMEM_SIZE_RUNTIME_AP;
    //md->mem_layout.ccci_rt_smem_base= (unsigned char *)kzalloc(md->mem_layout.ccci_rt_smem_size, GFP_KERNEL);
    //if (md->mem_layout.ccci_rt_smem_base == NULL) {
    //    CCCI_ERROR_LOG(md_id, TAG, "Fail to kmalloc runtime data (size=%d)\n", 	md->mem_layout.ccci_rt_smem_size);
    //    return -1;
    //}
	/* registers CLDMA callback isr with PCIE driver */
    cldma_register_isr(md_ctrl, hw_ctrl);
	cldma_disable_irq(md_ctrl, hw_ctrl);

	ccci_cldma_hw_set_info(hw_ctrl);

	md_ctrl->cldma_irq_worker =	alloc_workqueue("cldma_worker",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);

	INIT_WORK(&md_ctrl->cldma_irq_work, cldma_irq_work);

	cldma_register_pcie_isr(md_ctrl, hw_ctrl);
	md_ctrl->cldma_pcie_irq_worker = alloc_workqueue("cldma_pcie_worker",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_DELAYED_WORK(&md_ctrl->cldma_pcie_irq_work, cldma_pcie_irq_work);

#ifdef __TRAFFIC_MONITOR_INTERVAL__

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0))
	timer_setup(&md_ctrl->traffic_monitor, md_cd_traffic_monitor_func, 0);
#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)) */
	init_timer(&md_ctrl->traffic_monitor);
	md_ctrl->traffic_monitor.function = md_cd_traffic_monitor_func;
	md_ctrl->traffic_monitor.data = (unsigned long)md_ctrl;
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,15,0)) */

#endif

	/* register SYS CORE suspend resume call back */
	register_syscore_ops(&hsapif_sysops);

	md_ctrl->tx_busy_warn_cnt = 0;

	md_ctrl->cldma0_wakelock = wakeup_source_register(NULL, "CLDMA0_WAKEUP_LOCK");
	if (!md_ctrl->cldma0_wakelock) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] Init wakeup source failed!\n", __func__);
	}

	//hsapif_obtain_wakelock();

	return 0;
}

static inline void cldma_rgpd_set_data_ptr(
		struct cldma_rgpd *rgpd,
		dma_addr_t data_ptr)
{
#ifdef CONFIG_64BIT
	cldma_write32(&rgpd->data_buff_bd_ptr_h, 0, (u32)(data_ptr >> 32));
#endif
	cldma_write32(&rgpd->data_buff_bd_ptr_l, 0, (u32)data_ptr);

	//CCCI_DEBUG_LOG(-1, TAG, "%s: L:0x%x, H:0x%x, DMA address:0x%x\n",
	//	__func__, rgpd->data_buff_bd_ptr_l, rgpd->data_buff_bd_ptr_h, data_ptr);
}

static inline void cldma_rgpd_set_next_ptr(
		struct cldma_rgpd *rgpd,
		dma_addr_t next_ptr)
{
#ifdef CONFIG_64BIT
	cldma_write32(&rgpd->next_gpd_ptr_h, 0, (u32)(next_ptr >> 32));
#endif
	cldma_write32(&rgpd->next_gpd_ptr_l, 0, (u32)next_ptr);

	CCCI_DEBUG_LOG(-1, TAG, "[%s]: L:0x%x, H:0x%x, DMA address:0x%llx\n",
		__func__, rgpd->next_gpd_ptr_l, rgpd->next_gpd_ptr_h, next_ptr);
}

static inline void cldma_tgpd_set_next_ptr(
		struct cldma_tgpd *tgpd,
		dma_addr_t next_ptr)
{
#ifdef CONFIG_64BIT
	cldma_write32(&tgpd->next_gpd_ptr_h, 0, (u32)(next_ptr >> 32));
#endif
	cldma_write32(&tgpd->next_gpd_ptr_l, 0, (u32)next_ptr);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s]: L:0x%x, H:0x%x, DMA address:0x%llx\n",
		__FUNCTION__, tgpd->next_gpd_ptr_l,tgpd->next_gpd_ptr_h, next_ptr);
}

static int cldma_hif_rx_ring_init(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct cldma_ring *ring)
{
	int i;
	struct cldma_request *item, *first_item = NULL;
	struct cldma_rgpd *gpd = NULL, *prev_gpd = NULL;
	unsigned long flags;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			if (!item) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: Fail to kzalloc item\n",
					__func__);
				return -1;
			}

			//item->skb = ext_ccci_alloc_skb(ring->pkt_size, 1, 1);
			item->data = ext_ccci_data_alloc(GFP_KERNEL, ring->pkt_size);
			if (!item->data)
				goto data_fail;

			item->gpd = dma_pool_zalloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &item->gpd_addr);
			if (!item->gpd)
				goto data_fail;

			gpd = (struct cldma_rgpd *)item->gpd;

			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

			//item->data_buffer_ptr_saved = dma_map_single(hw_ctrl->dma_dev,
			//		item->skb->data, skb_data_size(item->skb), DMA_FROM_DEVICE);
			item->data_buffer_ptr_saved = dma_map_single(hw_ctrl->dma_dev,
					item->data->off_data, item->data->alloc_len, DMA_FROM_DEVICE);

			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] item[%d]: %px; gpd: %px; data_buffer_ptr_saved: 0x%llx; off_data: %px; alloc_len: %d\n",
				__func__, i, item, item->gpd, item->data_buffer_ptr_saved, item->data->off_data, item->data->alloc_len);

			if (dma_mapping_error(hw_ctrl->dma_dev, item->data_buffer_ptr_saved)) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: dma%d mapping fail.\n", __func__, i);

				item->data_buffer_ptr_saved = 0;
				spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
				dma_pool_free(md_ctrl->gpd_dmapool, item->gpd, item->gpd_addr);
data_fail:
				//ext_ccci_free_skb(item->skb);
				if (item->data != NULL)
					ext_ccci_data_free(item->data);
				kfree(item);

				return -1;
			}
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

			cldma_rgpd_set_data_ptr(gpd, item->data_buffer_ptr_saved);
			gpd->data_allow_len = ring->pkt_size > MAX_GPD_RECV_DATA_LEN ? MAX_GPD_RECV_DATA_LEN : ring->pkt_size;
			gpd->gpd_flags = 0x81;	/* IOC|HWO */

			if (i == 0)
				first_item = item;
			else
				cldma_rgpd_set_next_ptr(prev_gpd, item->gpd_addr);

			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);
			prev_gpd = gpd;
		}

		cldma_rgpd_set_next_ptr(gpd, first_item->gpd_addr);

		CCCI_DEBUG_LOG(-1, TAG, "[%s] rgpd_index: %d, req: %p,  "\
			"rgpd->gpd:%p, rgpd->gpd_addr:0x%llx ,rgpd->data_buffer_ptr_saved:0x%llx ,pkt_size %d\n",
			__func__, i, first_item, first_item->gpd, first_item->gpd_addr,
			first_item->data_buffer_ptr_saved, ring->pkt_size);

		return 0;
	}

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] error: ring->type(%d) is incorrect.\n",
		__func__, ring->type);

	return -1;
}

static int cldma_hif_tx_ring_init(struct ccci_md_cd_ctrl *md_ctrl, struct cldma_ring *ring)
{
	int i;
	struct cldma_request *item = NULL, *first_item = NULL;
	struct cldma_tgpd *tgpd = NULL, *prev_gpd = NULL;

	if (ring->type == RING_GPD) {
		for (i = 0; i < ring->length; i++) {
			item = kzalloc(sizeof(struct cldma_request), GFP_KERNEL);
			if (!item) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: Fail to kzalloc item\n",
					__func__);
				return -1;
			}

			item->gpd = dma_pool_zalloc(md_ctrl->gpd_dmapool, GFP_KERNEL, &item->gpd_addr);
			if (!item->gpd) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: Fail to kzalloc item->gpd\n",
					__func__);
				kfree(item);
				return -1;
			}

			tgpd = (struct cldma_tgpd *)item->gpd;
			tgpd->gpd_flags = 0x80;	/* IOC */
			//ext_ccci_data_clear(item->data);

			if (i == 0)
				first_item = item;
			else
				cldma_tgpd_set_next_ptr(prev_gpd, item->gpd_addr);

			INIT_LIST_HEAD(&item->bd);
			INIT_LIST_HEAD(&item->entry);
			list_add_tail(&item->entry, &ring->gpd_ring);

			prev_gpd = tgpd;
		}

		cldma_tgpd_set_next_ptr(tgpd, first_item->gpd_addr);
	    CCCI_DEBUG_LOG(-1, TAG, "[%s] tgpd_index: %d, req: %p,tgpd->gpd: %p, "\
	    	"tgpd->gpd_addr: 0x%llx ,tgpd->data_buffer_ptr_saved:0x%llx \n",
			__func__, i, first_item, first_item->gpd,
			first_item->gpd_addr, first_item->data_buffer_ptr_saved);

	    return 0;
	}

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] error: ring->type(%d) is incorrect.\n",
		__func__, ring->type);

	return -1;
}

static void cldma_queue_switch_ring(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct md_cd_queue *queue)
{
	struct cldma_request *req;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] txq:%d; dir:%d; hif_id:%d\n",
		__func__, queue->index, queue->dir, md_ctrl->hif_id);

	if (queue->dir == OUT) {
		queue->tr_ring = &md_ctrl->normal_tx_ring[normal_tx_queue2ring[queue->index]];

		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->tx_xmit = req;
		queue->budget = queue->tr_ring->length;

	} else if (queue->dir == IN) {
		queue->tr_ring = &md_ctrl->normal_rx_ring[normal_rx_queue2ring[queue->index]];

		req = list_first_entry(&queue->tr_ring->gpd_ring, struct cldma_request, entry);
		queue->tr_done = req;
		queue->rx_refill = req;
		queue->budget = queue->tr_ring->length;
	}

	/* work should be flushed by then */
	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] queue %d/%d switch ring to %p ,RGPD: 0x%llx, req: %p\n",
		__func__, queue->index, queue->dir, queue->tr_ring,
		queue->tr_done->gpd_addr,queue->tr_done);
}

/*
static void cldma_test_recv_data(ccci_kmem_data_t *src_data)
{
	ccci_kmem_data_t *des_data;
	struct ccci_header *ccci_h;
	int ret;

	des_data = ext_ccci_data_alloc(GFP_KERNEL, src_data->data_len);
	if (!des_data)
		return -1;

	memcpy(des_data->off_data, src_data->off_data, src_data->data_len);
	des_data->data_len = src_data->data_len;

	ccci_h = (struct ccci_header *)des_data->off_data;
	//ccci_h->channel -= 0x1000;
	ccci_h->channel += 1;
	ret = ccci_msg_send_to_one(GET_CH_INDEX(ccci_h->channel), CCCI_NONE_SUB_ID, des_data);

	if (ret < 0 && ret != -CCCI_HSAPIF_ERR_DROP_PACKET) {
		ext_ccci_data_free(des_data);
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: receive data fail: %d\n",
			__func__, ret);
	}

}
*/

static int cldma_send_skb(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
		//unsigned char hif_id, int qno, struct sk_buff *skb, int skb_from_pool, int blocking)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	ccci_send_data_t *send_data = (ccci_send_data_t *)msg_data;
	struct md_cd_queue *queue;
	struct cldma_request *tx_req;
	int ret = 0;
	struct ccci_header ch_h;
	struct ccci_header *ccci_h = &ch_h;
	unsigned int ioc_override = 0;
	unsigned long flags;
	unsigned int tx_bytes = 0;
	//struct ccci_buffer_ctrl *buf_ctrl = NULL;

#ifdef CLDMA_TRACE
	static unsigned long long last_leave_time[CLDMA_TXQ_NUM] = { 0 };
	static unsigned int sample_time[CLDMA_TXQ_NUM] = { 0 };
	static unsigned int sample_bytes[CLDMA_TXQ_NUM] = { 0 };
	unsigned long long total_time = 0;
	unsigned int tx_interal;
	total_time = sched_clock();
	if (last_leave_time[qno] == 0)
		tx_interal = 0;
	else
		tx_interal = total_time - last_leave_time[qno];
#endif

#ifdef __TRAFFIC_MONITOR_INTERVAL__
	if ((jiffies - md_ctrl->traffic_stamp) / HZ >= TRAFFIC_MONITOR_INTERVAL) {
		md_ctrl->traffic_stamp = jiffies;
		mod_timer(&md_ctrl->traffic_monitor, jiffies);
	}
#endif
	if (send_data->qno >= QUEUE_LEN(md_ctrl->txq)) {
		ret = -CCCI_HSAPIF_ERR_INVALID_QUEUE_INDEX;

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: qno: %d\n",
			__func__, send_data->qno);

		goto __EXIT_FUN;
	}

	ioc_override = 0x0;
#if 0
	if (send_data->skb_from_pool && skb_headroom(send_data->skb) == NET_SKB_PAD) {
		buf_ctrl = (struct ccci_buffer_ctrl *)skb_push(send_data->skb, sizeof(struct ccci_buffer_ctrl));

		if (likely(buf_ctrl->head_magic == CCCI_BUF_MAGIC))
			ioc_override = buf_ctrl->ioc_override;

		skb_pull(send_data->skb, sizeof(struct ccci_buffer_ctrl));

	} else
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] send request: skb %p use default value!\n",
			__func__, send_data->skb);

	ccci_h = *(struct ccci_header *)send_data->skb->data;
#endif
	(*ccci_h) = *(struct ccci_header *)(send_data->data->off_data);

	queue = &md_ctrl->txq[send_data->qno];
	//tx_bytes = send_data->skb->len;
	tx_bytes = send_data->data->data_len;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] hif_id: %d; qno: %d; channel: %d; tgpd: %llX; data_len: %d; blocking: %d\n",
		__func__, md_ctrl->hif_id, send_data->qno, ccci_h->channel, queue->tx_xmit->gpd_addr,
		tx_bytes, send_data->blocking);

 retry:
	spin_lock_irqsave(&queue->ring_lock, flags);

	/* we use irqsave as network require a lock in softirq, cause a potential deadlock */
	tx_req = queue->tx_xmit;

	//if (queue->budget > 0 && tx_req->skb == NULL) {
	if (queue->budget > 0 && tx_req->data == NULL) {
		//ccci_hif_inc_tx_seq_num(&md_ctrl->traffic_info, (struct ccci_header *)send_data->skb->data);
		ccci_hif_inc_tx_seq_num(&md_ctrl->traffic_info, (struct ccci_header *)(send_data->data->off_data));
		/* wait write done */
		wmb();

		queue->budget--;
		//queue->tr_ring->handle_tx_request(queue, tx_req, send_data->skb, ioc_override);
		queue->tr_ring->handle_tx_request(queue, tx_req, send_data->data, ioc_override);

		/* step forward */
		queue->tx_xmit = cldma_ring_step_forward(queue->tr_ring, tx_req);

		spin_unlock_irqrestore(&queue->ring_lock, flags);

		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] txq = %d, ch=0x%x , tx_xmit=%p budget = %d\n",
			__func__, send_data->qno, (u32)*((u32 *)ccci_h+2),
			queue->tx_xmit->gpd, queue->budget);

		/* update log */
#ifdef __TRAFFIC_MONITOR_INTERVAL__
		md_ctrl->tx_pre_traffic_monitor[queue->index]++;
#endif
		//ccci_md_add_log_history(&md_ctrl->traffic_info, OUT, (int)queue->index, &ccci_h, 0);
		/*
		 * make sure TGPD is ready by here, otherwise there is race conditon between ports over the same queue.
		 * one port is just setting TGPD, another port may have resumed the queue.
		 */
		/* put it outside of spin_lock_irqsave to avoid disabling IRQ too long */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

		if (md_ctrl->txq_active & (1 << send_data->qno)) {
#ifdef ENABLE_CLDMA_TIMER
			if (IS_NET_QUE(md_ctrl->md_id, qno)) {
				queue->timeout_start = local_clock();
				ret = mod_timer(&queue->timeout_timer, jiffies + CLDMA_ACTIVE_T * HZ);
				CCCI_DEBUG_LOG(md_ctrl->md_id, TAG,
					"md_ctrl->txq_active=%d, qno%d ,ch%d, start_timer=%d\n",
					md_ctrl->txq_active, qno, ccci_h.channel, ret);
				ret = 0;
			}
#endif
			md_ctrl->tx_busy_warn_cnt = 0;

			/* Tx queue will be started on sending due to IP busy issue */
			if (md_ctrl->txq_started & (1 << send_data->qno)) {
				if(!ccci_cldma_hw_queue_isactive(hw_ctrl->hw_info, queue->index, 0))
					ccci_cldma_hw_resume_queue(hw_ctrl->hw_info, send_data->qno, 0);
			} else {
				ccci_cldma_hw_queue_start(hw_ctrl, send_data->qno, 0);
				md_ctrl->txq_started |= 1 << send_data->qno;
			}

		} else {
			/*
			* [NOTICE] Dont return error
			* SKB has been put into cldma chain,
			* However, if txq_active is disable, that means cldma_stop for some case,
			* and cldma no need resume again.
			* This package will be dropped by cldma.
			*/
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] ch=%d qno=%d cldma maybe stop, this package will be dropped!\n",
				__func__, ccci_h->channel, send_data->qno);
		}

		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

		//cldma_test_recv_data(send_data->data);

	} else {
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] txbusy: budget=%d, skb=%p, hw_queue_isactive=%d, tx_xmit=0x%p\n",
			__func__, queue->budget, tx_req->data,
			ccci_cldma_hw_queue_isactive(hw_ctrl->hw_info, send_data->qno, 0), queue->tx_xmit);

		//if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id) & MODEM_CAP_TXBUSY_STOP))
		//	cldma_queue_broadcast_state(md_ctrl, TX_FULL, OUT, queue->index);

		spin_unlock_irqrestore(&queue->ring_lock, flags);

		/* check CLDMA status */
		if (ccci_cldma_hw_queue_isactive(hw_ctrl->hw_info, send_data->qno, 0)) {
			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] 1 ch=%d qno=%d free slot 0 \n",
				__func__, ccci_h->channel, send_data->qno);

			queue->busy_count++;
			md_ctrl->tx_busy_warn_cnt = 0;

		} else {
			if (ccci_cldma_hw_queue_ismask(hw_ctrl->hw_info, send_data->qno, 0))
				CCCI_DEBUG_LOG(-1, TAG,
					"[%s] 2 ch=%d qno=%d free slot 0 \n",
					__func__, ccci_h->channel, send_data->qno);

			if (++md_ctrl->tx_busy_warn_cnt == 1000) {
				CCCI_NORMAL_LOG(-1, TAG,
					"[%s] tx busy: dump CLDMA and GPD status\n", __func__);

				//CCCI_MEM_LOG_TAG(-1, TAG, "tx busy: dump CLDMA and GPD status\n");
				//md_ctrl->ops->dump_status(md_ctrl->md_id, HIF_ID_CLDMA, DUMP_FLAG_CLDMA, NULL, -1);

#if defined(CONFIG_MTK_AEE_FEATURE)
				aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT, "cldma", "TX busy debug");
#endif
			}

			/* resume channel */
			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
			ccci_cldma_hw_resume_queue(hw_ctrl->hw_info, send_data->qno, 0);
			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

		}

#if defined(CLDMA_NO_TX_IRQ)
		queue->tr_ring->handle_tx_done(queue, 0, 0);
#endif
		if (send_data->blocking) {
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] tx busy, wait event interruptible budget(%d) > 0.\n",
				__func__, queue->budget);

			ret = wait_event_interruptible_exclusive(queue->req_wq, (queue->budget > 0));
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto __EXIT_FUN;
			}
#ifdef CLDMA_TRACE
			trace_cldma_error(qno, ccci_h.channel, ret, __LINE__);
#endif
			goto retry;

		} else {
			ret = -EBUSY;
			goto __EXIT_FUN;

		}
	}

 __EXIT_FUN:

#ifdef CLDMA_TRACE
	if (unlikely(ret)) {
		CCCI_DEBUG_LOG(md_ctrl->md_id, TAG, "txq_active=%d, qno=%d is 0,drop ch%d package,ret=%d\n",
			     md_ctrl->txq_active, qno, ccci_h.channel, ret);
		trace_cldma_error(qno, ccci_h.channel, ret, __LINE__);
	} else {
		last_leave_time[qno] = sched_clock();
		total_time = last_leave_time[qno] - total_time;
		sample_time[queue->index] += (total_time + tx_interal);
		sample_bytes[queue->index] += tx_bytes;
		trace_cldma_tx(qno, ccci_h.channel, md_ctrl->txq[qno].budget, tx_interal, total_time,
			       tx_bytes, 0, 0);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_tx(qno, ccci_h.channel, 0, 0, 0, 0,
				sample_time[queue->index], sample_bytes[queue->index]);
			sample_time[queue->index] = 0;
			sample_bytes[queue->index] = 0;
		}
	}
#endif
	return ret;
}

/* this function may be called from both workqueue and ISR (timer) */
int cldma_gpd_tx_done(
		struct md_cd_queue *queue,
		int budget,
		int blocking)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[queue->hif_id];
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[queue->hif_id];
	unsigned long flags;
	struct cldma_request *req;
	struct cldma_tgpd *tgpd;
	struct ccci_header *ccci_h;
	int count = 0;
	//struct sk_buff *skb_free;
	ccci_kmem_data_t *data_free;
	dma_addr_t dma_free;
	unsigned int dma_len;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] queue->index: %d\n", __func__, queue->index);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] Dump CLDMA IP BUSY, qno=%d\n", __func__, queue->index);
	dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_TX_DONE_INT);

	while (1) {
		spin_lock_irqsave(&queue->ring_lock, flags);

		req = queue->tr_done;
		tgpd = (struct cldma_tgpd *)req->gpd;
		//if (!((tgpd->gpd_flags & 0x1) == 0 && req->skb)) {
		if (!((tgpd->gpd_flags & 0x1) == 0 && req->data)) {
			spin_unlock_irqrestore(&queue->ring_lock, flags);
			break;
		}
		/* restore IOC setting */
		if (req->ioc_override & 0x80) {
			if (req->ioc_override & 0x1)
				tgpd->gpd_flags |= 0x80;
			else
				tgpd->gpd_flags &= 0x7F;

			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] TX_collect: qno%d, ioc_override=0x%x,gpd_flags=0x%x\n",
				__func__, queue->index, req->ioc_override, tgpd->gpd_flags);
		}

		tgpd->non_used = 2;
		/* update counter */
		queue->budget++;
		/* save skb reference */
		dma_free = req->data_buffer_ptr_saved;
		dma_len = tgpd->data_buff_len;
		//skb_free = req->skb;
		data_free = req->data;
		/* mark cldma_request as available */
		//req->skb = NULL;
		req->data = NULL;

		/* step forward */
		queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);

		//if (likely(ccci_md_get_cap_by_id(md_ctrl->md_id) & MODEM_CAP_TXBUSY_STOP))
		//	cldma_queue_broadcast_state(md_ctrl, TX_IRQ, OUT, queue->index);

		spin_unlock_irqrestore(&queue->ring_lock, flags);
		count++;
		/*
		 * After enabled NAPI, when free skb, cosume_skb() will eventually called nf_nat_cleanup_conntrack(),
		 * which will call spin_unlock_bh() to let softirq to run.
			so there is a chance a Rx softirq is triggered (cldma_rx_collect)
		 * and if it's a TCP packet, it will send ACK
			-- another Tx is scheduled which will require queue->ring_lock,
		 * cause a deadlock!
		 *
		 * This should not be an issue any more,
			after we start using dev_kfree_skb_any() instead of dev_kfree_skb().
		 */
		dma_unmap_single(hw_ctrl->dma_dev, dma_free, dma_len, DMA_TO_DEVICE);

		//ccci_h = (struct ccci_header *)(skb_free->data);
		ccci_h = (struct ccci_header *)(data_free->off_data);
		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1)
			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] CLDMA_AP wakeup source:(%d/%d)\n",
				__func__, queue->index, ccci_h->channel);

		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] harvest Tx msg (%x %x %x %x) txq=%d len=%d, budget =%d, free_skb = %d, tr_done = %p\n",
			__func__, ccci_h->data[0], ccci_h->data[1], *(((u32 *) ccci_h) + 2), ccci_h->reserved,
			queue->index, data_free->data_len, queue->budget, count, queue->tr_done->gpd);

		//ccci_channel_update_packet_counter(md_ctrl->traffic_info.logic_ch_pkt_cnt, ccci_h);
		//ext_ccci_free_skb(skb_free);
		ext_ccci_data_free(data_free);
#ifdef __TRAFFIC_MONITOR_INTERVAL__
		md_ctrl->tx_traffic_monitor[queue->index]++;
#endif
//		if(!cldma_hw_queue_isactive(hw_ctrl, queue->index, 0)){
//		    spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
//		    cldma_hw_resume_queue (hw_ctrl ,queue->index, 0);
//		    spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
//		}
	}
	if (count)
		wake_up_nr(&queue->req_wq, count);

	return count;
}

static void cldma_tx_done(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct md_cd_queue *queue =
			container_of(dwork, struct md_cd_queue, cldma_tx_work);
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[queue->hif_id];
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[queue->hif_id];

	int count;
	unsigned int L2TISAR0 = 0;

	count = queue->tr_ring->handle_tx_done(queue, 0, 0);
	if (count && md_ctrl->tx_busy_warn_cnt)
		md_ctrl->tx_busy_warn_cnt = 0;

	/* greedy mode */
	L2TISAR0 = ccci_cldma_hw_get_status(hw_ctrl->hw_info,
			EMPTY_STATUS_BITMASK | TXRX_STATUS_BITMASK, 0);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] count = %d; L2TISAR0=0x%x\n",
		__func__, count, L2TISAR0);

	if (L2TISAR0)
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] tx greedy mode.\n",
			__func__);

	if ((L2TISAR0 & EMPTY_STATUS_BITMASK) &
			((1 << queue->index) << EQ_STA_BIT_OFFSET)) {
		ccci_cldma_hw_tx_done(hw_ctrl->hw_info, ((1 << queue->index) << EQ_STA_BIT_OFFSET));
		cldma_tx_queue_empty_handler(md_ctrl, queue);
	}

	if (L2TISAR0 & TXRX_STATUS_BITMASK & (1 << queue->index)) {
		ccci_cldma_hw_tx_done(hw_ctrl->hw_info, (1 << queue->index));
		queue_delayed_work(queue->worker, &queue->cldma_tx_work, msecs_to_jiffies(0));

	} else {

#ifndef CLDMA_NO_TX_IRQ
		unsigned long flags;
		/* enable TX_DONE interrupt */
		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

		if (md_ctrl->txq_active & (1 << queue->index)) {
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] Dump CLDMA IP BUSY, qno=%d\n", __func__, queue->index);
			dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_TX_ALL_DONE);

			ccci_cldma_hw_dismask_eqirq(hw_ctrl->hw_info, queue->index , 0);
			ccci_cldma_hw_dismask_txrxirq(hw_ctrl->hw_info, queue->index, 0);

#ifdef __CLDMA_TX_POLLING_MODE__
			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] Re-arrange short timer to polling TX IRQ\n",
				__func__);
			mod_timer(&md_ctrl->cldma_polling_mode_timer,
				jiffies + CLDMA_TX_RX_POLLING_MODE_TIMER_SEC*HZ);
#endif

		}
		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
#endif
	}

}

static void cldma_tx_queue_init(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct md_cd_queue *queue)
{
	cldma_queue_switch_ring(md_ctrl, queue);

	queue->worker = alloc_workqueue("hif%d_tx%d_worker",
			WQ_UNBOUND | WQ_MEM_RECLAIM | (queue->index == 0 ? WQ_HIGHPRI : 0),
			1, md_ctrl->hif_id, queue->index);

	INIT_DELAYED_WORK(&queue->cldma_tx_work, cldma_tx_done);

	queue->hif_id = md_ctrl->hif_id;

	CCCI_DEBUG_LOG(-1, TAG, "[%s] txq%d work=%p\n",
		__func__, queue->index, &queue->cldma_tx_work);

#ifdef ENABLE_CLDMA_TIMER
	init_timer(&queue->timeout_timer);
	queue->timeout_timer.function = cldma_timeout_timer_func;
	queue->timeout_timer.data = (unsigned long)queue;
	queue->timeout_start = 0;
	queue->timeout_end = 0;
#endif
}

#ifdef __MONITOX_TX_STATUS_TIMER__
static void cldma_monitor_tx_status (struct timer_list *t)
{
	//struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	struct ccci_md_cd_ctrl *md_ctrl = from_timer(md_ctrl, t, cldma_monitor_tx_status_timer);
	int qinx = 0;
	int number_tx_queue_done = 0;
	unsigned long flags;
	for (qinx=0; qinx < CLDMA_TXQ_NUM; qinx++) {
		struct md_cd_queue * queue = &md_ctrl->txq[qinx];
		spin_lock_irqsave(&queue->ring_lock, flags);
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] HW txq inx=%d, budget=%d\n",
			__func__, qinx, queue->budget);
		if(queue->budget == queue->tr_ring->length) {
			//Each TGPD's HWO bit should be 0
			struct list_head *listptr;
			list_for_each(listptr, &queue->tr_ring->gpd_ring) {
				struct cldma_request *req;
				struct cldma_tgpd *tgpd;
				req = list_entry(listptr, struct cldma_request, entry);
				tgpd = (struct cldma_tgpd *)req->gpd;
				CCCI_NORMAL_LOG(-1, TAG,
					"[%s] HW tx status: qinx=%d,req=%p,gpd_flags=%d, req_data=%p\n",
					__func__, qinx, req, tgpd->gpd_flags, req->data);
			}
			number_tx_queue_done++;
		}
		spin_unlock_irqrestore(&queue->ring_lock, flags);
	}

	if (number_tx_queue_done == CLDMA_TXQ_NUM) {
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] HW tx done for TXQ\n",
			__func__);
	}
#if 0
	mod_timer(&md_ctrl->cldma_monitor_tx_status_timer, jiffies + CLDMA_MONITOR_TX_Q_TIMER_SEC*HZ);
#endif
}
#endif /*__MONITOX_TX_STATUS_TIMER__*/

#if 0
static void dump_cldma_debug_info(struct timer_list *t)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	struct md_cd_queue * tx_queue = &md_ctrl->txq[0];
	struct md_cd_queue * rx_queue = &md_ctrl->rxq[0];
	int i,j;
	/*Dump RXQ#0 - max: CLDMA_RXQ_NUM*/
	for (i=0; i < 1; i++) {
		struct list_head *listptr;
		list_for_each(listptr, &rx_queue->tr_ring->gpd_ring) {
			struct cldma_request *req;
			struct cldma_rgpd *rgpd;
			req = list_entry(listptr, struct cldma_request, entry);
			rgpd = (struct cldma_rgpd *)req->gpd;
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] HW Rx: RXQ_No=%d,RGPD_DMA_Addr=0x%llx,gpd_flags=%d, data_buf_len=%d\n",
				__func__, i, req->gpd_addr, rgpd->gpd_flags, rgpd->data_buff_len);
		}
	}
	/*Dump TXQ#0 - max: CLDMA_TXQ_NUM*/
	for (j=0; j < 1; j++) {
		struct list_head *listptr;
		list_for_each(listptr, &tx_queue->tr_ring->gpd_ring) {
			struct cldma_request *req;
			struct cldma_tgpd *tgpd;
			req = list_entry(listptr, struct cldma_request, entry);
			tgpd = (struct cldma_tgpd *)req->gpd;
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] HW Tx: TXQ_No=%d,TGPD_DMA_Addr=0x%llx,gpd_flags=%d, data_buf_len=%d\n",
				__func__, i, req->gpd_addr, tgpd->gpd_flags, tgpd->data_buff_len);
		}
	}

}
#endif

void cldma_gpd_dump(unsigned int q)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	int i, j;
	int *val;
	int maxq_num;

	maxq_num = (q >= CLDMA_RXQ_NUM) ? CLDMA_RXQ_NUM : q+1;
	i = (q >= CLDMA_RXQ_NUM) ? 0 : q;
	/*Dump RXQ#0 - max: CLDMA_RXQ_NUM*/
	for (j = 0; i < maxq_num; i++, j = 0) {
		CCCI_MEM_LOG_TAG(-1, TAG, "[%s] CLDMA%d RXQ%d GPD:\n", __func__, HIF_ID_CLDMA, i);
		struct md_cd_queue * rx_queue = &md_ctrl->rxq[i];
		struct list_head *listptr;
		list_for_each(listptr, &rx_queue->tr_ring->gpd_ring) {
			struct cldma_request *req;
			req = list_entry(listptr, struct cldma_request, entry);
			val = req->gpd;
			CCCI_MEM_LOG_TAG(-1, TAG,
				"[%s] %d: %08X %08X %08X %08X %08X %08X\n",
				__func__, j, *val, *(val+1), *(val+2), *(val+3), *(val+4), *(val+5));
			j++;
		}
	}

	maxq_num = (q >= CLDMA_TXQ_NUM) ? CLDMA_TXQ_NUM : q+1;
	i = (q >= CLDMA_TXQ_NUM) ? 0 : q;
	/*Dump TXQ#0 - max: CLDMA_TXQ_NUM*/
	for (j = 0; i < maxq_num; i++, j = 0) {
		CCCI_MEM_LOG_TAG(-1, TAG, "[%s] CLDMA%d TXQ%d GPD:\n", __func__, HIF_ID_CLDMA, i);
		struct md_cd_queue * tx_queue = &md_ctrl->txq[i];
		struct list_head *listptr;
		list_for_each(listptr, &tx_queue->tr_ring->gpd_ring) {
			struct cldma_request *req;
			req = list_entry(listptr, struct cldma_request, entry);
			val = req->gpd;
			CCCI_MEM_LOG_TAG(-1, TAG,
				"[%s] %d: %08X %08X %08X %08X %08X %08X\n",
				__func__, j, *val, *(val+1), *(val+2), *(val+3), *(val+4), *(val+5));
			j++;
		}
	}
}

void cldma_regs_dump_impl(unsigned int len, unsigned int offset, unsigned int is_rh, unsigned int is_ao)
{
	int i = 0;
	unsigned long long base_addr = 0;
	int round_up_len = round_up(len, 4);
	int b4_fix_num = round_up_len / 4;
	int b16_fix_num = b4_fix_num / 4;
	int b16_tail_num = b4_fix_num % 4;
	unsigned char buf[256] = {0};
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	struct ccci_cldma_hw_info *hw_info = hw_ctrl->hw_info;
	int count = 0;

	for(i = 0;  i < b16_fix_num; i++) {
		if (is_rh) {
			if (is_ao) {
				if (hw_info->cldma_ap_ao_base == 0x0) {
					return;
				}
				base_addr = hw_info->cldma_ap_ao_base + offset + i * 16;
			} else {
				base_addr = hw_info->cldma_ap_pdn_base + offset + i * 16;
			}
		} else {
			if (is_ao) {
				if (hw_info->cldma0_lh_indma_ao_base == 0x0) {
					return;
				}
				base_addr = hw_info->cldma0_lh_indma_ao_base + offset + i * 16;
			} else {
				base_addr = hw_info->cldma0_lh_indma_pd_base + offset + i * 16;
			}
		}
		CCCI_MEM_LOG_TAG(-1, TAG,
				"[cldma_regs_dump] 0x%04X: %08X %08X %08X %08X\n",
				i * 16,
				cldma_read32(base_addr, 0),
				cldma_read32(base_addr, 4),
				cldma_read32(base_addr, 8),
				cldma_read32(base_addr, 12));
	}

	if (b16_tail_num) {
		if (is_rh) {
			if (is_ao) {
				if (hw_info->cldma_ap_ao_base == 0x0) {
					return;
				}
				base_addr = hw_info->cldma_ap_ao_base + offset + i * 16;
			} else {
				base_addr = hw_info->cldma_ap_pdn_base + offset + i * 16;
			}
		} else {
			if (is_ao) {
				if (hw_info->cldma0_lh_indma_ao_base == 0x0) {
					return;
				}
				base_addr = hw_info->cldma0_lh_indma_ao_base + offset + i * 16;
			} else {
				base_addr = hw_info->cldma0_lh_indma_pd_base + offset + i * 16;
			}
		}
		count = sprintf(buf, "0x%04X:", i * 16);
		if (count < 0) {
			CCCI_ERROR_LOG(-1, TAG,
				"[cldma_regs_dump] 1. %d failed to format buf\n",
				count);
			return;
		}
		for (i = 0; i < b16_tail_num; i++) {
			count = sprintf(buf + 7 + i * 9, "%08X", cldma_read32(base_addr, i * 4));
			if (count < 0) {
				CCCI_ERROR_LOG(-1, TAG,
					"[cldma_regs_dump] 2. %d failed to format buf\n",
					count);
				return;
			}
		}
		CCCI_MEM_LOG_TAG(-1, TAG,
				"[cldma_regs_dump] %s", buf);
	}
}

void cldma_regs_dump(void)
{
	int i = 0;
	int coda_number = 0;
	struct coda_t *codas, *coda;

	codas = ccci_coda_cldma;
	coda_number = ARRAY_SIZE(ccci_coda_cldma);

	for (i = 0; i < coda_number; i++) {
		coda = codas + i;
		//CCCI_MEM_LOG_TAG(-1, TAG,
		//	"[%s] coda[%d/%d]: name: %s; hand: %s; pa: 0x%08X; offset: 0x%X; len: 0x%X; is_rh: %d; is_ao: %d\n",
		//	__func__, i, coda_number, coda->name, coda->hand, coda->cldma_phy_base,
		//	coda->offset, coda->len, coda->is_rh, coda->is_ao);
		CCCI_MEM_LOG_TAG(-1, TAG,
			"[%s] CLDMA 0 DEBUG DUMP -- %s\n",
			__func__,
			coda->hand);
		CCCI_MEM_LOG_TAG(-1, TAG,
			"[%s] dump %s register address: PA=0x%lX, len:0x%X\n",
			__func__,
			coda->name,
			coda->cldma_phy_base + coda->offset,
			coda->len);
		cldma_regs_dump_impl(coda->len, coda->offset, coda->is_rh, coda->is_ao);
	}
}

#if (defined(__CLDMA_RX_POLLING_MODE__) || defined(__CLDMA_TX_POLLING_MODE__))
static void simulate_cldma0_irq_monitor(struct timer_list *t)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	CCCI_NORMAL_LOG(-1, TAG,
			"[%s] CLDMA0 IRQ Polling Mode\n",
			__func__);
	cldma_irq_work_polling_mode(md_ctrl);

}
#endif


/* may be called from workqueue or NAPI or tasklet (queue0) context, only NAPI and tasklet with blocking=false */
static int cldma_gpd_rx_collect(
		struct md_cd_queue *queue,
		int budget,
		int blocking,
		gfp_t gfp_flags)
{
	struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[queue->hif_id];
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[queue->hif_id];

	struct cldma_request *req;
	struct cldma_rgpd *rgpd;
	struct ccci_header ccci_h;
	//struct sk_buff *skb = NULL;
	ccci_kmem_data_t *data;
	//struct sk_buff *new_skb = NULL;
	ccci_kmem_data_t *new_data = NULL;
	int ret = 0, count = 0, rxbytes = 0;
	int over_budget = 0, skb_handled = 0, retry = 0;
	unsigned long long skb_bytes = 0;
	unsigned long flags;
	//char is_net_queue = IS_NET_QUE(md_ctrl->md_id, queue->index);
	//char using_napi = is_net_queue ? (ccci_md_get_cap_by_id(md_ctrl->md_id) & MODEM_CAP_NAPI) : 0;
	unsigned int L2RISAR0 = 0;
	unsigned int skb_size;

#ifdef CLDMA_TRACE
	unsigned long long port_recv_time = 0;
	unsigned long long skb_alloc_time = 0;
	unsigned long long total_handle_time = 0;
	unsigned long long temp_time = 0;
	unsigned long long total_time = 0;
	unsigned int rx_interal;
	static unsigned long long last_leave_time[CLDMA_RXQ_NUM] = { 0 };
	static unsigned int sample_time[CLDMA_RXQ_NUM] = { 0 };
	static unsigned int sample_bytes[CLDMA_RXQ_NUM] = { 0 };

	total_time = sched_clock();
	if (last_leave_time[queue->index] == 0)
		rx_interal = 0;
	else
		rx_interal = total_time - last_leave_time[queue->index];
#endif

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] budget = %d, blocking = %d\n",
		__func__, budget, blocking);

again:
	while (1) {

#ifdef CLDMA_TRACE
		total_handle_time = port_recv_time = sched_clock();
#endif

		req = queue->tr_done;
		rgpd = (struct cldma_rgpd *)req->gpd;
		//if (!((cldma_read8(&rgpd->gpd_flags, 0) & 0x1) == 0 && req->skb)) {
		if (!((cldma_read8(&rgpd->gpd_flags, 0) & 0x1) == 0 && req->data)) {
			CCCI_DEBUG_LOG(-1, TAG,
				"HWO:%x, SKB:%p\n",
				(cldma_read8(&rgpd->gpd_flags, 0) & 0x1),
				req->data);
			break;
		}

		//skb = req->skb;
		data = req->data;
		/* update skb */
		//skb_size = skb_data_size(skb);
		skb_size = data->alloc_len;
		//if (req->data_buffer_ptr_saved == (dma_addr_t)0)
		//	CCCI_ERROR_LOG(-1, TAG,
		//		"[%s] (hif_id: %d; q: %d) error: dma_unmap_single with NULL\n",
		//		__func__, queue->hif_id, queue->index);

		if (skb_size > 4096)
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] (hif_id: %d; q: %d) error: dma_unmap_single with abnormal skb size:%d\n",
				__func__, queue->hif_id, queue->index, skb_size);

		spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

		if (req->data_buffer_ptr_saved != 0) {
			dma_unmap_single(hw_ctrl->dma_dev,
					req->data_buffer_ptr_saved,
					data->alloc_len,
					DMA_FROM_DEVICE);

			req->data_buffer_ptr_saved = 0;
		}

		spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

		/*init skb struct*/
		//skb->len = 0;
		//skb_reset_tail_pointer(skb);
		/*set data len*/
		//skb_put(skb, rgpd->data_buff_len);
		//skb_bytes = skb->len;
		data->data_len = rgpd->data_buff_len;
		skb_bytes = data->data_len;
		//((struct ccci_header *)skb->data)->channel &= 0x0FFF;
		//ccci_h = *((struct ccci_header *)skb->data);
		//((struct ccci_header *)data->off_data)->channel -= 0x1000;
		ccci_h = *((struct ccci_header *)data->off_data);

		/* check wakeup source */
		if (atomic_cmpxchg(&md_ctrl->wakeup_src, 1, 0) == 1)
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] CLDMA_MD wakeup source:(%d/%d/%x)\n",
				__func__, queue->index, ccci_h.channel, ccci_h.reserved);

//		CCCI_ERROR_LOG(-1, TAG, "[%s] data: %p; recv Rx msg (%x %x %x %x) rxq=%d len=%d(%x)\n",
//			__func__, data, ccci_h.data[0], ccci_h.data[1], *(((u32 *)&ccci_h) + 2),
//			ccci_h.reserved, queue->index, rgpd->data_buff_len, rgpd->data_buff_len);

		/* upload skb */
		//if (!is_net_queue) {
		//	ret = ccci_md_recv_skb(md_ctrl->md_id, md_ctrl->hif_id, skb);

		//ret = ccci_msg_send_to_one(ccci_h.channel, CCCI_NONE_SUB_ID, skb);
		ret = ccci_msg_send_to_one(GET_CH_INDEX(ccci_h.channel), CCCI_NONE_SUB_ID, data);

		if (ret >= 0) {
			CCCI_NORMAL_LOG(-1, TAG, "[%s] req: %p; rgpd: %llX; data: %p; recv Rx msg (%x %x %x %x) rxq=%d ccci_ch=%d len=%d(%x); ret = %d;\n",
				__func__, req, req->gpd_addr, data, ccci_h.data[0], ccci_h.data[1], *(((u32 *)&ccci_h) + 2),
				ccci_h.reserved, queue->index, ccci_h.channel, rgpd->data_buff_len, rgpd->data_buff_len, ret);

			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] Dump CLDMA IP BUSY, qno=%d\n", __func__, queue->index);
			dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_RX_DONE_INT);
		}

		//} else if (using_napi) {
		//	ccci_md_recv_skb(md_ctrl->md_id, md_ctrl->hif_id, skb);
		//	ret = 0;
//		} else {
//#ifdef CCCI_SKB_TRACE
//#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,2))
//			skb->tstamp = sched_clock();
//#else /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,2)) */
//			skb->tstamp.tv64 = sched_clock();
//#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,2)) */
//#endif
//			ccci_skb_enqueue(&queue->skb_list, skb);
//			wake_up_all(&queue->rx_wq);
//			ret = 0;
//		}
#ifdef CLDMA_TRACE
		port_recv_time = ((skb_alloc_time = sched_clock()) - port_recv_time);
#endif
		new_data = NULL;

		if (ret >= 0 || ret == -CCCI_HSAPIF_ERR_DROP_PACKET ||
			ret == -CCCI_HSAPIF_ERR_DIPC_PORT_CFG_DISABLE ||
			ret == -CCCI_HSAPIF_ERR_INVALID_MSG_ID) {
			//new_skb = ccci_alloc_skb(queue->tr_ring->pkt_size, !is_net_queue, blocking);
			//new_skb = ext_ccci_alloc_skb(queue->tr_ring->pkt_size, 1, blocking);
			new_data = ext_ccci_data_alloc(gfp_flags, queue->tr_ring->pkt_size);
			//if (!new_skb)
			if (!new_data) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] alloc skb fail on q%d\n",
					__func__, queue->index);
			} else if (ret == -CCCI_HSAPIF_ERR_DIPC_PORT_CFG_DISABLE ||
				ret == -CCCI_HSAPIF_ERR_INVALID_MSG_ID) {
				ext_ccci_data_free(data);
			}
		}

		//if (new_skb) {
		if (new_data) {
			/* mark cldma_request as available */
			//req->skb = NULL;
			req->data = NULL;
			cldma_rgpd_set_data_ptr(rgpd, 0);
			/* step forward */
			queue->tr_done = cldma_ring_step_forward(queue->tr_ring, req);
			/* update log */
#ifdef __TRAFFIC_MONITOR_INTERVAL__
			md_ctrl->rx_traffic_monitor[queue->index]++;
#endif
			rxbytes += skb_bytes;

			//ccci_md_add_log_history(&md_ctrl->traffic_info, IN, (int)queue->index, &ccci_h,
			//	(ret >= 0 ? 0 : 1));

			//ccci_channel_update_packet_counter(md_ctrl->traffic_info.logic_ch_pkt_cnt, &ccci_h);
			ccci_hif_check_rx_seq_num(&md_ctrl->traffic_info, &ccci_h, queue->index);

			/* refill */
			req = queue->rx_refill;
			rgpd = (struct cldma_rgpd *)req->gpd;

			spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);

			//req->data_buffer_ptr_saved =
			//		dma_map_single(hw_ctrl->dma_dev, new_skb->data,
			//				skb_data_size(new_skb), DMA_FROM_DEVICE);
			req->data_buffer_ptr_saved =
					dma_map_single(hw_ctrl->dma_dev, new_data->off_data,
							new_data->alloc_len, DMA_FROM_DEVICE);

			if (dma_mapping_error(hw_ctrl->dma_dev, req->data_buffer_ptr_saved)) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: dma mapping\n", __func__);
				req->data_buffer_ptr_saved = 0;

				spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

				//ext_ccci_free_skb(new_skb);
				ext_ccci_data_free(new_data);
				return -1;
			}

			spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

			cldma_rgpd_set_data_ptr(rgpd, req->data_buffer_ptr_saved);
			cldma_write16(&rgpd->data_buff_len, 0, 0);
			/* set HWO, no need to hold ring_lock as no racer */
			cldma_write8(&rgpd->gpd_flags, 0, 0x81);
			/* mark cldma_request as available */
			//req->skb = new_skb;
			req->data = new_data;
			/* step forward */
			queue->rx_refill = cldma_ring_step_forward(queue->tr_ring, req);
			skb_handled = 1;

		} else {
			/* undo skb, as it remains in buffer and will be handled later */
			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] rxq%d leave skb %p in ring, ret = 0x%x\n",
				__func__, queue->index, data, ret);
			/* no need to retry if port refused to recv */
			return ONCE_MORE;
		}
#ifdef CLDMA_TRACE
		temp_time = sched_clock();
		skb_alloc_time = temp_time - skb_alloc_time;
		total_handle_time = temp_time - total_handle_time;
		trace_cldma_rx(queue->index, 0, count, port_recv_time, skb_alloc_time,
				total_handle_time, skb_bytes);
#endif
		/* check budget, only NAPI and queue0 are allowed to reach budget, as they can be scheduled again */
		if (++count >= budget && !blocking) {
			over_budget = 1;
			break;
		}
	}
	/*
	 * do not use if(count == RING_BUFFER_SIZE) to resume Rx queue.
	 * resume Rx queue every time. we may not handle all RX ring buffer at one time due to
	 * user can refuse to receive patckets. so when a queue is stopped after it consumes all
	 * GPD, there is a chance that "count" never reaches ring buffer size and the queue is stopped
	 * permanentely.
	 */
	ret = ALL_CLEAR;
	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	if (md_ctrl->rxq_active & (1 << queue->index)) {
		/* resume Rx queue */
		if(!ccci_cldma_hw_queue_isactive(hw_ctrl->hw_info, queue->index, 1))
			ccci_cldma_hw_resume_queue(hw_ctrl->hw_info, queue->index, 1);

		/* greedy mode */
		L2RISAR0 = ccci_cldma_hw_get_status(hw_ctrl->hw_info, (1 << queue->index), 1);
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] resume queue, L2RISAR0: 0x%x\n",
			__func__, ccci_cldma_hw_get_status(hw_ctrl->hw_info, 0xFFFF, 1));

		if (L2RISAR0)
			retry = 1;
		else
			retry = 0;

		/* where are we going */
		if (over_budget) {
			/* remember only NAPI and queue0 can reach here */
			/*Enter this if statement: It means that skb_handled must be 1*/
			/*In this way, it should mark the following line to fix coverity checking*/
			//retry = skb_handled ? retry : 1;
			ret = retry ? ONCE_MORE : ALL_CLEAR;

		} else {
			if (retry) {
				/* clear IP busy register wake up cpu case */
//				cldma_write32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY,
//						cldma_read32(md_ctrl->cldma_ap_pdn_base, CLDMA_AP_CLDMA_IP_BUSY));
				ccci_cldma_hw_rx_done(hw_ctrl->hw_info, L2RISAR0);

				spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
				goto again;

			} else {
				ret = ALL_CLEAR;

			}
		}
	}
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

#ifdef CLDMA_TRACE
	if (count) {
		last_leave_time[queue->index] = sched_clock();
		total_time = last_leave_time[queue->index] - total_time;
		sample_time[queue->index] += (total_time + rx_interal);
		sample_bytes[queue->index] += rxbytes;
		trace_cldma_rx_done(queue->index, rx_interal, total_time, count, rxbytes, 0, ret);
		if (sample_time[queue->index] >= trace_sample_time) {
			trace_cldma_rx_done(queue->index, 0, 0, 0, 0,
					sample_time[queue->index], sample_bytes[queue->index]);
			sample_time[queue->index] = 0;
			sample_bytes[queue->index] = 0;
		}
	} else {
		trace_cldma_error(queue->index, -1, 0, __LINE__);
	}
#endif

	return ret;
}

static void cldma_md_cd_rxq0_tasklet(unsigned long data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	struct md_cd_queue *queue = &md_ctrl->rxq[0];
	int ret;

	//md_ctrl->traffic_info.latest_q_rx_time[queue->index] = local_clock();
	ret = queue->tr_ring->handle_rx_done(queue, queue->budget, 0, GFP_ATOMIC);
	if (ret != ALL_CLEAR)
		tasklet_hi_schedule(&md_ctrl->cldma_rxq0_task);

	else {

		/* clear IP busy register wake up cpu case */
		CCCI_DEBUG_LOG(-1, TAG,
		"[%s] Rx qno=%d done!Clear RH CLDMA_IP_BUSY\n",
		__func__, queue->index);
		clear_cldma0_ip_busy_status_reg(CLEAR_IP_BUSY_RX_0);

		/* enable RX_DONE and QUEUE_EMPTY interrupt */
		ccci_cldma_hw_dismask_eqirq(hw_ctrl->hw_info, queue->index, 1);
		ccci_cldma_hw_dismask_txrxirq(hw_ctrl->hw_info, queue->index, 1);
#ifdef __CLDMA_RX_POLLING_MODE__
				CCCI_DEBUG_LOG(-1, TAG,
					"[%s] Re-arrange short timer to polling RX IRQ\n",
					__func__);
				mod_timer(&md_ctrl->cldma_polling_mode_timer,
					jiffies + CLDMA_TX_RX_POLLING_MODE_TIMER_SEC*HZ);
#endif
	}
	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] rxq0 tasklet result %d\n",
		__func__, ret);
}

static void cldma_rx_done(struct work_struct *work)
{
	struct md_cd_queue *queue = container_of(work, struct md_cd_queue, cldma_rx_work);
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[queue->hif_id];
	int ret;
#ifdef __CLDMA_RX_POLLING_MODE__
		struct ccci_md_cd_ctrl *md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
#endif

	//md_ctrl->traffic_info.latest_q_rx_time[queue->index] = local_clock();

	ret = queue->tr_ring->handle_rx_done(queue, queue->budget, 1, GFP_KERNEL);
	if (ret != ALL_CLEAR)
		queue_work(queue->worker, &queue->cldma_rx_work);

	else {

		/* clear IP busy register wake up cpu case */
		CCCI_DEBUG_LOG(-1, TAG,
		"[%s] Rx qno=%d done!Clear RH CLDMA_IP_BUSY\n",
		__func__, queue->index);
		clear_cldma0_ip_busy_status_reg(CLEAR_IP_BUSY_RX_OTHERS);

		/* enable RX_DONE && EMPTY interrupt */
		ccci_cldma_hw_dismask_txrxirq(hw_ctrl->hw_info, queue->index, 1);
		ccci_cldma_hw_dismask_eqirq(hw_ctrl->hw_info, queue->index, 1);
#ifdef __CLDMA_RX_POLLING_MODE__
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] Re-arrange short timer to polling RX IRQ\n",
			__func__);
		mod_timer(&md_ctrl->cldma_polling_mode_timer,
			jiffies + CLDMA_TX_RX_POLLING_MODE_TIMER_SEC*HZ);
#endif
	}
}

static void cldma_rx_queue_init(
		struct ccci_md_cd_ctrl *md_ctrl,
		struct md_cd_queue *queue)
{
	cldma_queue_switch_ring(md_ctrl, queue);

	INIT_WORK(&queue->cldma_rx_work, cldma_rx_done);
	/*
	 * we hope work item of different CLDMA queue can work concurrently, but work items of the same
	 * CLDMA queue must be work sequentially as wo didn't implement any lock in rx_done or tx_done.
	 */
	queue->worker = alloc_workqueue("hif%d_rx%d_worker",
			WQ_UNBOUND | WQ_MEM_RECLAIM, 1,
			md_ctrl->hif_id, queue->index);

	//ext_ccci_skb_queue_init(&queue->skb_list, queue->tr_ring->pkt_size, SKB_RX_QUEUE_MAX_LEN, 0);

	init_waitqueue_head(&queue->rx_wq);

	queue->hif_id = md_ctrl->hif_id;

//	if (IS_NET_QUE(md_ctrl->md_id, queue->index))
//		queue->rx_thread = kthread_run(cldma_net_rx_push_thread, queue, "cldma_rxq%d", queue->index);
//	CCCI_DEBUG_LOG(-1, TAG, "[%s] rxq%d work=%p\n",
//		__func__, queue->index, &queue->cldma_rx_work);
}

int cldma_hif_late_init(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	int i;
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	char dma_pool_name[32];
	int count = 0;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] md_ctrl: %p; hif_id: %d\n",
		__func__, md_ctrl, md_ctrl->hif_id);

	atomic_set(&md_ctrl->wakeup_src, 0);
	ccci_hif_reset_seq_num(&md_ctrl->traffic_info);


	/* init ring buffers */
	count = sprintf(dma_pool_name, "cldma_requeset_DMA_hif%d", md_ctrl->hif_id);
	if (count < 0) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format dma_pool_name\n",
				__func__, count);
		return -1;
	}

	md_ctrl->gpd_dmapool = dma_pool_create(dma_pool_name,
			hw_ctrl->dma_dev, sizeof(struct cldma_tgpd), 16, 0);
	if (!md_ctrl->gpd_dmapool) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] failed to create gpd_dmapool\n",
				__func__);
		return -1;
	}

	for (i = 0; i < NORMAL_TXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_tx_ring[i].gpd_ring);

		md_ctrl->normal_tx_ring[i].length =
			normal_tx_queue_buffer_number[normal_tx_ring2queue[i]];
		md_ctrl->normal_tx_ring[i].type = RING_GPD;
		md_ctrl->normal_tx_ring[i].handle_tx_request = (&cldma_gpd_tx_request);
		md_ctrl->normal_tx_ring[i].handle_tx_done = (&cldma_gpd_tx_done);

		cldma_hif_tx_ring_init(md_ctrl, &md_ctrl->normal_tx_ring[i]);

		CCCI_DEBUG_LOG(-1, TAG, "[%s] normal_tx_ring %d: %p\n",
			__func__, i, &md_ctrl->normal_tx_ring[i]);

	}

	for (i = 0; i < NORMAL_RXQ_NUM; i++) {
		INIT_LIST_HEAD(&md_ctrl->normal_rx_ring[i].gpd_ring);

		md_ctrl->normal_rx_ring[i].length =
			normal_rx_queue_buffer_number[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].pkt_size =
			normal_rx_queue_buffer_size[normal_rx_ring2queue[i]];
		md_ctrl->normal_rx_ring[i].type = RING_GPD;
		md_ctrl->normal_rx_ring[i].handle_rx_done = (&cldma_gpd_rx_collect);

		cldma_hif_rx_ring_init(md_ctrl, &md_ctrl->normal_rx_ring[i]);

		CCCI_DEBUG_LOG(-1, TAG, "[%s] normal_rx_ring %d: %p\n",
			__func__, i, &md_ctrl->normal_rx_ring[i]);
	}

	/* init queue */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++){
		//CCCI_DEBUG_LOG(-1, TAG, "[%s], md_ctrl: %p; md_ctrl->txq[%d]: %p\n",
		//	__func__, md_ctrl, i, &md_ctrl->txq[i]);

		cldma_tx_queue_init(md_ctrl, &md_ctrl->txq[i]);
	}

	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++){
		//CCCI_DEBUG_LOG(-1, TAG, "[%s] cldma_rx_queue_init:0x%x\n",
		//	__func__, &md_ctrl->rxq[i]);

		cldma_rx_queue_init(md_ctrl, &md_ctrl->rxq[i]);
	}
#if 0
	init_timer(&md_ctrl->cldma_monitor_tx_status_timer);
	md_ctrl->cldma_monitor_tx_status_timer.function = cldma_monitor_tx_status;
	md_ctrl->cldma_monitor_tx_status_timer.data = NULL;
#endif
	//timer_setup(&md_ctrl->cldma_monitor_tx_status_timer, cldma_monitor_tx_status, 0);
	//timer_setup(&md_ctrl->cldma_monitor_tx_status_timer, dump_cldma_debug_info, 0);

#if (defined(__CLDMA_RX_POLLING_MODE__) || defined(__CLDMA_TX_POLLING_MODE__))
	timer_setup(&md_ctrl->cldma_polling_mode_timer, simulate_cldma0_irq_monitor, 0);
	mod_timer(&md_ctrl->cldma_polling_mode_timer, jiffies + CLDMA_TX_RX_POLLING_MODE_TIMER_SEC*HZ);
#endif
	md_ctrl->is_late_init = 1;

	return 0;
}

static int cldma_hif_start(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	int i;
	unsigned long flags;
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];

	CCCI_INIT_LOG(-1, TAG, "[%s] hif_id: %d\n", __func__, md_ctrl->hif_id);

	/* workaround for set start command twice issue */
	ccci_cldma_hw_stop_queue(hw_ctrl, 0xFF, 0);
	ccci_cldma_hw_stop_queue(hw_ctrl, 0xFF, 1);

	cldma_enable_irq(md_ctrl, hw_ctrl);

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* set start address */
	for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
    {
		//cldma_queue_switch_ring(&md_ctrl->txq[i]);
		ccci_cldma_hw_set_start_address(hw_ctrl, i,
				md_ctrl->txq[i].tr_done->gpd_addr, 0);
	}
	for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++)
    {
		//cldma_queue_switch_ring(&md_ctrl->rxq[i]);
		ccci_cldma_hw_set_start_address(hw_ctrl, i,
				md_ctrl->rxq[i].tr_done->gpd_addr, 1);
    }
	/* wait write done */
	wmb();

	/* start all Rx queues, Tx queue will be started on sending due to IP busy issue */
	md_ctrl->txq_started = 0;
	md_ctrl->txq_active |= TXRX_STATUS_BITMASK;
	md_ctrl->rxq_active |= TXRX_STATUS_BITMASK;

	ccci_cldma_hw_queue_start(hw_ctrl, 0xFF, 1);
    ccci_cldma_hw_start(hw_ctrl);
#if 0
	//mod_timer(&md_ctrl->cldma_monitor_tx_status_timer, jiffies + msecs_to_jiffies(20));
	mod_timer(&md_ctrl->cldma_monitor_tx_status_timer, jiffies + CLDMA_MONITOR_TX_Q_TIMER_SEC*HZ);
#endif
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);
	/**eAP is ready and notify MD **/
	//eap_ready_notify(hw_ctrl);

	cldma_gpd_dump(0);
	return 0;
}

#ifdef __CLDMA0_RESET_HW__
void cldma_hif_reset_hw(struct ccci_cldma_hw_ctrl *hw_ctrl)
{
#if 0
	/*config MTU reg for 93*/
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_DL_MTU_SIZE, CLDMA_AP_MTU_SIZE);

	/*enable DMA for 93*/
	cldma_write32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG,
	      cldma_read32(md_ctrl->cldma_ap_ao_base, CLDMA_AP_SO_CFG) | 0x1);
#endif
}


static int cldma_md_cd_reset(struct ccci_md_cd_ctrl *md_ctrl, unsigned char is_hw)
{
	CCCI_DEBUG_LOG(-1, TAG,
		"[%s], reset cldma is_hw: %d\n",
		__FUNCTION__, is_hw);

	if(0 == is_hw){
	    md_ctrl->tx_busy_warn_cnt = 0;
	    ccci_hif_reset_seq_num(&md_ctrl->traffic_info);

#ifdef __TRAFFIC_MONITOR_INTERVAL__
	    md_cd_clear_traffic_data(md_id, HIF_ID_CLDMA_PCIE);
#endif

	} else if (1 == is_hw)
		cldma_hif_reset_hw(ccci_hw_ctrls[md_ctrl->hif_id]);

    return 0;
}
#endif /* __CLDMA0_RESET_HW__ */

/* only allowed when cldma is stopped */
static int cldma_hw_clear_all_queue(struct ccci_md_cd_ctrl *md_ctrl, DIRECTION dir)
{
	int i;
	struct cldma_request *req = NULL;
	struct cldma_tgpd *tgpd;
	unsigned long flags;
	//struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];

	CCCI_NORMAL_LOG(-1, TAG, "[%s], dir: %d\n", __func__, dir);

	if (dir == OUT) {
		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++) {
			spin_lock_irqsave(&md_ctrl->txq[i].ring_lock, flags);

			//req = list_first_entry(&md_ctrl->txq[i].tr_ring->gpd_ring, struct cldma_request, entry);
			//md_ctrl->txq[i].tr_done = req;
			//md_ctrl->txq[i].tx_xmit = req;
			//md_ctrl->txq[i].budget = md_ctrl->txq[i].tr_ring->length;

#if PACKET_HISTORY_DEPTH
			md_ctrl->traffic_info.tx_history_ptr[i] = 0;
#endif

			list_for_each_entry(req, &md_ctrl->txq[i].tr_ring->gpd_ring, entry) {
				tgpd = (struct cldma_tgpd *)req->gpd;

				cldma_write8(&tgpd->gpd_flags, 0, cldma_read8(&tgpd->gpd_flags, 0) & ~0x1);

				if (md_ctrl->txq[i].tr_ring->type != RING_GPD_BD)
					cldma_tgpd_set_data_ptr(tgpd, 0);

				cldma_write16(&tgpd->data_buff_len, 0, 0);

				//if (req->skb) {
				//	ext_ccci_free_skb(req->skb);
				//	req->skb = NULL;
				//}
				if (req->data)
					ext_ccci_data_free(req->data);

			}
			spin_unlock_irqrestore(&md_ctrl->txq[i].ring_lock, flags);
		}

	} else if (dir == IN) {
		struct cldma_rgpd *rgpd;

		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			spin_lock_irqsave(&md_ctrl->rxq[i].ring_lock, flags);

			req = list_first_entry(&md_ctrl->rxq[i].tr_ring->gpd_ring, struct cldma_request, entry);
			md_ctrl->rxq[i].tr_done = req;
			md_ctrl->rxq[i].rx_refill = req;

#if PACKET_HISTORY_DEPTH
			md_ctrl->traffic_info.rx_history_ptr[i] = 0;
#endif

			list_for_each_entry(req, &md_ctrl->rxq[i].tr_ring->gpd_ring, entry) {
				rgpd = (struct cldma_rgpd *)req->gpd;

				cldma_write8(&rgpd->gpd_flags, 0, 0x81);
				cldma_write16(&rgpd->data_buff_len, 0, 0);

				//if (req->skb != NULL) {
				//	req->skb->len = 0;
				//	skb_reset_tail_pointer(req->skb);
				//}
				if (req->data != NULL)
					req->data->data_len = 0;

			}
			spin_unlock_irqrestore(&md_ctrl->rxq[i].ring_lock, flags);

//			list_for_each_entry(req, &md_ctrl->rxq[i].tr_ring->gpd_ring, entry) {
//				rgpd = (struct cldma_rgpd *)req->gpd;
//
//				if (req->skb == NULL) {
//					struct md_cd_queue *queue = &md_ctrl->rxq[i];
//
//					/*which queue*/
//					CCCI_NORMAL_LOG(-1, TAG, "skb NULL in Rx queue %d/%d\n",
//							i, queue->index);
//
//					req->skb = ext_ccci_alloc_skb(queue->tr_ring->pkt_size, 1, 1);
//
//					req->data_buffer_ptr_saved = dma_map_single(
//							hw_ctrl->dma_dev, req->skb->data,
//							skb_data_size(req->skb), DMA_FROM_DEVICE);
//
//					if (dma_mapping_error(hw_ctrl->dma_dev, req->data_buffer_ptr_saved)) {
//						CCCI_ERROR_LOG(-1, TAG,
//								"[%s] error: dma mapping\n", __func__);
//						return -1;
//					}
//
//					cldma_rgpd_set_data_ptr(rgpd, req->data_buffer_ptr_saved);
//				}
//			}
		}
	}
	return 0;
}

static int cldma_hif_clear_all_queue(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;

	if (cldma_hw_clear_all_queue(md_ctrl, IN) < 0)
		return -1;

	if (cldma_hw_clear_all_queue(md_ctrl, OUT) < 0)
		return -1;


	return 0;
}

static int cldma_hif_stop(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	struct ccci_md_cd_ctrl *md_ctrl = (struct ccci_md_cd_ctrl *)my_data;
	struct ccci_cldma_hw_ctrl *hw_ctrl = ccci_hw_ctrls[md_ctrl->hif_id];
	int i;
	unsigned long flags;
#ifdef ENABLE_CLDMA_TIMER
	int qno;
#endif

	CCCI_NORMAL_LOG(-1, TAG, "[%s] hif_id: %d\n",
		__func__, md_ctrl->hif_id);

	spin_lock_irqsave(&md_ctrl->cldma_timeout_lock, flags);
	/* stop all Tx and Rx queues */
	md_ctrl->txq_active &= (~TXRX_STATUS_BITMASK);
	ccci_cldma_hw_stop_queue(hw_ctrl, 0xFF, 0);

	md_ctrl->rxq_active &= (~TXRX_STATUS_BITMASK);
	ccci_cldma_hw_stop_queue(hw_ctrl, 0xFF, 1);

	/*disable all interrupt*/
	ccci_cldma_hw_stop(hw_ctrl);

	/* stop timer */
#ifdef ENABLE_CLDMA_TIMER
	if (md_ctrl->is_late_init) {
		for (qno = 0; qno < CLDMA_TXQ_NUM; qno++)
			del_timer(&md_ctrl->txq[qno].timeout_timer);
	}
#endif
	spin_unlock_irqrestore(&md_ctrl->cldma_timeout_lock, flags);

	/* flush work */
	cldma_disable_irq(md_ctrl, hw_ctrl);

	if (md_ctrl->is_late_init) {
		flush_work(&md_ctrl->cldma_irq_work);

		for (i = 0; i < QUEUE_LEN(md_ctrl->txq); i++)
			flush_delayed_work(&md_ctrl->txq[i].cldma_tx_work);

		for (i = 0; i < QUEUE_LEN(md_ctrl->rxq); i++) {
			/*q0 is handled in tasklet, no need flush*/
			if (i == 0)
				continue;

			flush_work(&md_ctrl->rxq[i].cldma_rx_work);
		}
	}

	return 0;
}


int ccci_cldma_hif_init(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	ccci_init_data_t *pdata = (ccci_init_data_t *)msg_data;
	struct ccci_md_cd_ctrl *md_ctrl;

	if (!(pdata->hif_ids & (1 << HIF_ID_CLDMA)))
		return 0;

	if (ccci_cldma_hw_init(pdata) < 0)
		return -1;

	md_ctrl = ccci_md_ctrls[HIF_ID_CLDMA];
	if (cldma_hif_init(md_ctrl) < 0)
		return -1;


	if (ccci_msg_register(CCCI_LATE_INIT_ID, CCCI_NONE_SUB_ID,
			md_ctrl, &cldma_hif_late_init) < 0)
		return -1;

	if (ccci_msg_register(CCCI_START_ID, CCCI_NONE_SUB_ID,
			md_ctrl, &cldma_hif_start) < 0)
		return -1;

	if (ccci_msg_register(CCCI_CLEAR_ALL_Q_ID, CCCI_NONE_SUB_ID,
			md_ctrl, &cldma_hif_clear_all_queue) < 0)
		return -1;

	if (ccci_msg_register(CCCI_STOP_ID, CCCI_NONE_SUB_ID,
			md_ctrl, &cldma_hif_stop) < 0)
		return -1;

	if (ccci_msg_register(CCCI_CLDMA_SEND_DATA_ID, 1 << HIF_ID_CLDMA,
			md_ctrl, &cldma_send_skb) < 0)
		return -1;

	if (ccci_msg_register(CCCI_CLDMA_MONITOR_HW_TXQ_ID, CCCI_NONE_SUB_ID,
			md_ctrl, &cldma_schedule_check_hw_txq_work) < 0)
		return -1;

	return 0;
}






