// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_MSG_DATA_H__
#define __CCCI_MSG_DATA_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/skbuff.h>

#include "ccci_kmem.h"
#include "ccci_msg_id.h"

typedef struct ccci_init_data {
	struct platform_device *plat_dev;
	u32 hif_ids;

} ccci_init_data_t;


typedef struct ccci_send_data {
	u8 hif_id;
	int qno;
	ccci_kmem_data_t *data;
	int blocking;

} ccci_send_data_t;


typedef struct ccci_state_data {
	CLDMA_STATE_T new_state;
	CLDMA_STATE_T old_state;

} ccci_state_data_t;

typedef struct ccci_ioctrl_data {
	unsigned long user_arg;

} ccci_ioctrl_data_t;



#endif	/* __CCCI_MSG_DATA_H__ */
