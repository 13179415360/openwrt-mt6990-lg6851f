// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/cdev.h>

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_core.h"
#include "ccci_msg_id.h"
#include "ccci_msg_center.h"
#include "ccci_state_mgr.h"

#define TAG "sta"

static CLDMA_STATE_T s_cldma_state;
static spinlock_t s_state_lock;

CLDMA_STATE_T ccci_cldma_state_get(void)
{
	CLDMA_STATE_T state;
	unsigned long flags;

	spin_lock_irqsave(&s_state_lock, flags);
	state = s_cldma_state;
	spin_unlock_irqrestore(&s_state_lock, flags);

	return state;
}

static int	ccci_cldma_state_set(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	ccci_state_data_t state_data;
	unsigned long flags;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] new_state: %d; old_state: %d\n",
		__func__, (CLDMA_STATE_T)sub_id, s_cldma_state);

	spin_lock_irqsave(&s_state_lock, flags);

	state_data.new_state = (CLDMA_STATE_T)sub_id;
	state_data.old_state = s_cldma_state;

	if (state_data.new_state != state_data.old_state) {
		s_cldma_state = state_data.new_state;
		spin_unlock_irqrestore(&s_state_lock, flags);

		ccci_msg_send(CCCI_CLDMA_STATE_CHANGE_ID,
				CCCI_NONE_SUB_ID, &state_data);

	} else
		spin_unlock_irqrestore(&s_state_lock, flags);

	return 0;
}

int ccci_state_mgr_init(void)
{
	int ret = 0;

	spin_lock_init(&s_state_lock);

	s_cldma_state = CLA_NONE;

	if (ccci_msg_register(CCCI_CLDMA_STATE_SET_ID, CCCI_NONE_SUB_ID,
			NULL, &ccci_cldma_state_set) < 0)
		return -1;

	CCCI_INIT_LOG(-1, TAG,
		"[%s]\n", __func__);

	return ret;
}


