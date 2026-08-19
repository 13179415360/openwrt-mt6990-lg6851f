/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_FEATURE_DEFINE_H__
#define __SCP_FEATURE_DEFINE_H__

#define TEST_WITHOUT_MODEM               (0)
//TODO; should be configured to 0 once DPMAIF driver is ready 
#define TEST_WITHOUT_CCCI                (0) // linked with _DPMAIF_MED_SUPPORT_

#if (CONFIG_MTK_ECCCI_DATA_PLANE_TYPE == 3)
#define MED_V2
#endif

/* Platform configs*/
#define SCP_BOOT_TIME_OUT_MONITOR        (0)
#define SCP_RESERVED_MEM                 (1)
#define SCP_LOGGER_ENABLE                (1)
#define SCP_DVFS_INIT_ENABLE             (0)

/* Rescovery feature option*/
#define SCP_RECOVERY_SUPPORT             (1)
/* Recovery timeout value (ms)*/
#define SCP_SYS_RESET_TIMEOUT            1000

/* aed definition*/
#define SCP_AED_STR_LEN                  (512)

/* sub feature register API marco*/
#define SCP_REGISTER_SUB_SENSOR          (1)

/* emi mpu define*/
//TODO; EMI protection needs to be enabled
#define ENABLE_SCP_EMI_PROTECTION        (0)

/* enable medmcu suspend resume */
#define ENABLE_MEDMCU_SUSPEND_RESUME     (1)

#define MPU_REGION_ID_SCP_SMEM           7
#define MPU_DOMAIN_D0                    0
#define MPU_DOMAIN_D3                    3


#define SCPSYS_CORE0                     0
#define SCPSYS_CORE1                     1

/* scp feature ID list */
enum feature_id {
	FLP_FEATURE_ID,
	RTOS_FEATURE_ID,
	SPEAKER_PROTECT_FEATURE_ID,
	VCORE_TEST_FEATURE_ID,
	NUM_FEATURE_ID,
};

/* scp sensor type ID list */
enum scp_sensor_id {
	ACCELEROMETER_FEATURE_ID = 0,
	NUM_SENSOR_TYPE,
};

struct scp_feature_tb {
	uint32_t feature;
	uint32_t freq;
	uint32_t enable;
	uint32_t sys_id; /* run at which subsys? */
};

struct scp_sub_feature_tb {
	uint32_t feature;
	uint32_t freq;
	uint32_t enable;
};


extern struct scp_feature_tb feature_table[NUM_FEATURE_ID];
extern struct scp_sub_feature_tb sensor_type_table[NUM_SENSOR_TYPE];
extern void scp_register_feature(enum feature_id id);
extern void scp_deregister_feature(enum feature_id id);
#if 0
extern void scp_register_sensor(enum feature_id id,
		enum scp_sensor_id sensor_id);
extern void scp_deregister_sensor(enum feature_id id,
		enum scp_sensor_id sensor_id);
#endif
#endif


