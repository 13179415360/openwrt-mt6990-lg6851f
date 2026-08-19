// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



#ifndef __CCCI_FSM_CLDMA_H__
#define __CCCI_FSM_CLDMA_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

//Start UT Testing Area (Turn On/Off by self in UT codes)
#if 0
#define __CLDMA_UT__
#endif
//End UT Testing Area

/*Normal Feature for normal boot to linux shell: Must always enable it*/
#if 1
#define __SUPPORT_CCCI_CTRL_MSG_HEADER__
#define __SUPPORT_ASYNC_HS_FLOW__
#endif

enum host_bc_event {
	HOST_STA_EV_INVALID = 0,
	HOST_STA_EV_SUSPEND_REQ,
	HOST_STA_EV_SUSPEND_ACK,
	HOST_STA_EV_RESUNE_REQ,
	HOST_STA_EV_READY,
};


extern int ccci_fsm_cldma_init(void);

extern int ccci_fsm_cldma_add_cmd(
		int cmd_id,
		void *cmd_data);

extern void inject_host_pcie_status_event(
		int md_id,
		int event_type,
		char reason[]);
extern void cldma_bc_wakeup_event(void);

extern void hsapif_obtain_wakelock(void);
extern void hsapif_release_wakelock(void);

#endif	/* __CCCI_FSM_CLDMA_H__ */
