/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_DVR_CLDMA_H__
#define __CCCI_DVR_CLDMA_H__

/*   For Chip bring-up, CLDMA0's clk will be always on automatically.
 *   Before sanity, CLDMA0 will take over its clk management by itself.
 */
#define __SUPPORT_CLDMA0_SELF_CLK_MGT__

#include <linux/clk.h>
#include "ccci_config.h"
#include "ccci_msg_data.h"

extern void hsapif_release_wakelock(void);

struct cldma_clk_node {
	struct clk *clk_ref;
	unsigned char *clk_name;
};


#endif	/* __CCCI_DVR_CLDMA_H__ */
