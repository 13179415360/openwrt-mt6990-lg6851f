// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



#ifndef __CCCI_STATE_MSG_H__
#define __CCCI_STATE_MSG_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#include "ccci_core.h"
#include "ccci_hif.h"


extern int ccci_state_mgr_init(void);

extern CLDMA_STATE_T ccci_cldma_state_get(void);


#endif	/* __CCCI_STATE_MSG_H__ */
