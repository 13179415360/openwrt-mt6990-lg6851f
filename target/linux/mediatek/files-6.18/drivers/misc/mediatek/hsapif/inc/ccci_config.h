// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_CONFIG_H__
#define __CCCI_CONFIG_H__


#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>



#include "ccci_debug.h"

#define CCCI_ADDRESS_ALIGN_LEN (128)

#define CCCI_MTU            (3584-CCCI_ADDRESS_ALIGN_LEN)
#define CCCI_NET_MTU        (1500)
#define SKB_POOL_SIZE_4K    (256)
#define SKB_POOL_SIZE_1_5K  (256)
#define SKB_POOL_SIZE_16    (64)
#define BM_POOL_SIZE        (SKB_POOL_SIZE_4K+SKB_POOL_SIZE_1_5K+SKB_POOL_SIZE_16)
#define RELOAD_TH           (3)	/*reload pool if pool size dropped below 1/RELOAD_TH */

#endif	/* __CCCI_CONFIG_H__ */
