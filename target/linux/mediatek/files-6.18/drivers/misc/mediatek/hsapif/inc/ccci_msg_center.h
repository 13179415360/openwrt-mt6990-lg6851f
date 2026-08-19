// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_MSG_CENTER_H__
#define __CCCI_MSG_CENTER_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "ccci_comm_config.h"
#include "ccci_msg_id.h"
#include "ccci_msg_data.h"

//#define USE_LOCK_TO_REG_ARRAY
//#define SUPPORT_ASYNC_SEDN_MSG


typedef int (* ccci_msg_cb_t)(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data);



extern
int ccci_msg_center_init(void);

extern
int	ccci_msg_register(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		ccci_msg_cb_t callback);

#ifdef USE_LOCK_TO_REG_ARRAY
extern
int	ccci_msg_unregister(
		int msg_id,
		unsigned int sub_id,
		ccci_msg_cb_t callback);
#endif

/* no compare sub_id, think the msg_id is one-to-one */
extern
int ccci_msg_send_to_one(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

/* the func help to free the msg_data */
extern
int ccci_msg_send(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

#ifdef SUPPORT_ASYNC_SEDN_MSG
/* the func help to free the msg_data */
extern
int ccci_msg_send_asyn(
		int msg_id,
		unsigned int sub_id,
		void *msg_data,
		int free_msg_data);

/* the func help to free the msg_data */
extern
int ccci_msg_send_wait(
		int msg_id,
		unsigned int sub_id,
		void *msg_data);

#endif


#endif	/* __CCCI_MSG_CENTER_H__ */
