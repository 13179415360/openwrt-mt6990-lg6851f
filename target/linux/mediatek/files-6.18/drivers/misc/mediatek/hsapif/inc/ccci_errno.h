// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_ERROR_NO_H__
#define __CCCI_ERROR_NO_H__


//Note: If one newly state is added, it must add its corresponding state errno to CLDMA_ERROR_T in ccci_errno.h
//Because in ccci_check_can_write(): it returns "-(cla_state + ERR_DRV_BASE_ID)" as error code
typedef enum {
	ERR_DRV_BASE_ID = 0x7000,
	ERR_DRV_NONE,
	ERR_DRV_INITING,
	ERR_DRV_ERROR,
	ERR_DRV_STOP,
	ERR_DRV_WAITING_HS1,
	ERR_DRV_SEND_HS2,
	ERR_DRV_WAITING_HS3,
	ERR_DRV_HOST_SUSPEND_REQ,
	ERR_DRV_HOST_SUSPEND_ACK,
	ERR_DRV_HOST_REMOTE_WAKEUP,
	ERR_DRV_HOST_RESUME_REQ,
	ERR_DRV_DIPC_PORT_CFG_DISABLE,

	ERR_HOST_USER_BASE_ID = 0x7100,
	ERR_HOST_USER_ERROR,
	ERR_HOST_USER_STOP,


} CLDMA_ERROR_T;





//typedef int (* ccci_init_callback_t)(
//		struct ccci_init_data *init_data);





#endif	/* __CCCI_ERROR_NO_H__ */
