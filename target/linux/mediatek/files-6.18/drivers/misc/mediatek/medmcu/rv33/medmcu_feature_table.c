//* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>       /* needed by all modules */
#include "medmcu_feature_define.h"
#include "medmcu_ipi_pin.h"


/*scp feature list*/
struct scp_feature_tb feature_table[NUM_FEATURE_ID] = {
/* VFFP:20 + default:5 */
	{
		.feature	= FLP_FEATURE_ID,
		.freq		= 26,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature	= RTOS_FEATURE_ID,
		.freq		= 0,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
	{
		.feature	= SPEAKER_PROTECT_FEATURE_ID,
		.freq		= 200,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE1,
	},
	{
		.feature	= VCORE_TEST_FEATURE_ID,
		.freq		= 0,
		.enable		= 0,
		.sys_id		= SCPSYS_CORE0,
	},
};

/* Sensor type list*/
struct scp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE] = {
	{
		.feature = ACCELEROMETER_FEATURE_ID,
		.freq    = 2,
		.enable  = 0,
	},
};

