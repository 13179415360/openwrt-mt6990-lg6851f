// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



#ifndef __CCCI_MSG_ID_H__
#define __CCCI_MSG_ID_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "ccci_core.h"
#include "ccci_hif.h"

typedef enum ccci_msg_id {

	CCCI_INIT_ID = (CCCI_CH_MAX_NUM - CCCI_CH_BASE),
	CCCI_LATE_INIT_ID,
	CCCI_START_ID,
	CCCI_CLEAR_ALL_Q_ID,
	CCCI_STOP_ID,
	CCCI_PM_RESTORE_NOIRQ,

	CCCI_CLDMA_STATE_SET_ID,
	CCCI_CLDMA_STATE_CHANGE_ID,
	CCCI_CLDMA_MONITOR_HW_TXQ_ID,

	CCCI_CLDMA_BASE_ID,
	CCCI_CLDMA_SEND_DATA_ID = CCCI_CLDMA_BASE_ID + HIF_ID_CLDMA,

	/* define io ctrl id */
	CCCI_IOCTRL_ID,

	/* define command id */
	CCCI_COMMAND_ID,

	/* define event id */
	CCCI_EVENT_ID,


	/* define status id */
	CCCI_STATUS_ID,

	/* define dipc port cfg id */
	CCCI_DIPC_PORT_CFG_ID,

	CCCI_MAX_MSG_ID,

} CCCI_MSG_ID;



typedef enum ccci_comm_sub_id {
	CCCI_COMM_START   = 1, /* from md_init */
	CCCI_COMM_STOP       , /* from md_init */
	CCCI_COMM_WDT        , /* from MD */
	CCCI_COMM_EE         , /* from MD */
	CCCI_COMM_MD_HANG    , /* from status polling thread */

} CCCI_COMM_SUB_ID;

typedef enum ccci_fsm_cmd_sub_id {
	CCCI_FSM_CMD_CALL_MHCCIF_IRQ = 1,
	CCCI_FSM_CMD_SEND_HS2_OR_CTRL_MSG = 2,
	CCCI_FSM_CMD_RECV_HS1 = 3,
	CCCI_FSM_CMD_RECV_HS3 = 4,
	CCCI_FSM_CMD_RECV_HOST_PM_SUSPEND_REQ = 5,
	CCCI_FSM_CMD_SEND_HOST_PM_SUSPEND_ACK = 6,
	CCCI_FSM_CMD_SEND_HOST_REMOTE_WAKEUP = 7,
	CCCI_FSM_CMD_RECV_HOST_PM_RESUME_REQ = 8,
	CCCI_FSM_CMD_SEND_HOST_PM_RESUME_ACK = 9,

} CCCI_FSM_CMD_SUB_ID;

/* define none sub id */
#define CCCI_NONE_SUB_ID 0




#endif	/* __CCCI_MSG_ID_H__ */
