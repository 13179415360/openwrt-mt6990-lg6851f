// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_CORE_H__
#define __CCCI_CORE_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>

#include "ccci_errno.h"

#define CCCI_MAGIC_NUM 0xFFFFFFFF


/* ============================================================= */
/* common structures */
/* ============================================================= */
typedef enum {
	IN = 0,
	OUT = 1,
	INOUT =2
} DIRECTION;


/* ======================================================================= */
/* CCCI Channel ID and Message ID definations */
/* ======================================================================= */
#define MGR_TX_Q_NUM        0
#define MGR_RX_Q_NUM        0

#define CCCI_CH_BASE 0x1000

#define TIMESYNC_TX_Q_NUM        6
#define TIMESYNC_RX_Q_NUM        6

typedef enum {
	CCCI_PORT_MGR_TX = CCCI_CH_BASE,
	CCCI_PORT_MGR_RX,

	CCCI_PORT_RBACK_TX,
	CCCI_PORT_RBACK_RX,

	CCCI_PORT_GNSS_TX,
	CCCI_PORT_GNSS_RX,

	CCCI_PORT_META_TX,
	CCCI_PORT_META_RX,

	CCCI_PORT_LOG_TX,
	CCCI_PORT_LOG_RX,

	CCCI_PORT_ADB_TX,
	CCCI_PORT_ADB_RX,

	CCCI_PORT_MINI_TX,
	CCCI_PORT_MINI_RX,

	CCCI_PORT_DICP_TX,
	CCCI_PORT_DICP_RX,

	CCCI_PORT_MINIDUMP_TX,
	CCCI_PORT_MINIDUMP_RX,

	CCCI_PORT_GCCCI0_TX,
	CCCI_PORT_GCCCI0_RX,

	CCCI_PORT_GCCCI1_TX,
	CCCI_PORT_GCCCI1_RX,

	CCCI_PORT_GCCCI2_TX,
	CCCI_PORT_GCCCI2_RX,

	CCCI_PORT_TIMESYNC_TX,
	CCCI_PORT_TIMESYNC_RX,

	CCCI_PORT_AGNSS_TX,
	CCCI_PORT_AGNSS_RX,

	CCCI_PORT_RFS_TX,
	CCCI_PORT_RFS_RX,

	CCCI_CH_MAX_NUM,

} CCCI_CH_T;


#define CCCI_CH_COUNT (CCCI_CH_MAX_NUM - CCCI_CH_BASE)

#define GET_CH_INDEX(CHANNEL) (CHANNEL - CCCI_CH_BASE)


#define MGR_MSG_ID_PORT_ENUM 0x9


#define MGR_MSG_ID_HS1 0x0
#define MGR_MSG_ID_HS2 0x1
#define MGR_MSG_ID_HS3 0x2

/*The following definitions will be only used in HS Phase1 workaround solution*/
/*Once HS flow supports CCCI_CTRL_MSG_HEADER (i.e., Phase2 final solution), the following msgs are not used*/
#define MGR_MSG_ID_PHASE1_HS1 0xA
#define MGR_MSG_ID_PHASE1_HS2 0xB
#define MGR_MSG_ID_PHASE1_HS3 0xC


#define HOST_FEATURE_QUERY_PATTERN 0x49434343
#define AP_FEATURE_QUERY_PATTERN 0x49434343

/*Note: If one newly state is added, it must add its corresponding state errno to CLDMA_ERROR_T in ccci_errno.h*/
/*Because in ccci_check_can_write(): it returns "-(cla_state + ERR_DRV_BASE_ID)" as error code*/
typedef enum {
	CLA_READY = 0,
	CLA_NONE,
	CLA_INITING,
	CLA_ERROR,
	CLA_STOP,
	CLA_WAITING_HS1,
	CLA_WAITING_PORT_ENUM_SEND_HS2,
	CLA_WAITING_HS3,
	CLA_RECV_HOST_SUSPEND_REQ,
	CLA_SEND_HOST_SUSPEND_ACK,
	CLA_SEND_HOST_REMOTE_WAKEUP,
	CLA_RECV_HOST_RESUME_REQ,

} CLDMA_STATE_T;


typedef enum {
	HOST_USER_READY = 0,
	HOST_USER_ERROR,
	HOST_USER_STOP,

} HOST_USERT_STATE_T;


typedef enum {
	MD_PORT_CONFIG = 0,
	CLDMA_PORT_CONFIG = 1,

	CLDMA_RUNTIME_FEATURE_ID_MAX,

} cldma_runtime_feature_id_t;

#define CLDMA_FEATURE_COUNT 64

typedef enum {
	CCCI_FEATURE_NOT_EXIST = 0,
	CCCI_FEATURE_NOT_SUPPORT = 1,
	CCCI_FEATURE_MUST_SUPPORT = 2,
	CCCI_FEATURE_OPTIONAL_SUPPORT = 3,
	CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT = 4,

} cldma_feature_support_type;

typedef struct cldma_feature_support {
	u8 support_mask:4;
	u8 version:4;

} cldma_feature_support_t;

typedef struct cldma_query_feature {
	u32 head_pattern;
	struct cldma_feature_support feature_set[CLDMA_FEATURE_COUNT];
	u32 tail_pattern;

} cldma_query_feature_t;


typedef struct cldma_runtime_feature {
	u8 feature_id;	/*for debug only*/
	struct cldma_feature_support support_info;
	u8 reserved[2];
	u32 data_len;
	u8 data[0];

} cldma_runtime_feature_t;

//typedef int (* ccci_init_callback_t)(
//		struct ccci_init_data *init_data);

enum dipc_port_cfg_t {
	DIPC_PORT_CFG_DISABLE = 0,
	DIPC_PORT_CFG_ENABLE,

};




#endif	/* __CCCI_CORE_H__ */
