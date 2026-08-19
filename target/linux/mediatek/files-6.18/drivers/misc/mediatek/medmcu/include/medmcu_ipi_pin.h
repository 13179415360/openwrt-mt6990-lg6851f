/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SCP_IPI_PIN_H_
#define _SCP_IPI_PIN_H_

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

/* scp awake timeout count definition */
#define SCP_AWAKE_TIMEOUT 100000
/* scp Core ID definition */
enum scp_core_id {
	SCP_A_ID = 0,
	SCP_CORE_TOTAL = 1,
};

/* this is only for ipi to distinguish core0 and core1 */
enum {
	SCP_CORE0_ID = 0,
	SCP_CORE1_ID = 1,
};

enum {
/* core1 */
	/* the following will use mbox0 */
	IPI_OUT_AUDIO_VOW_1       =  0,
	IPI_IN_AUDIO_VOW_ACK_1	  =  1,
	IPI_IN_AUDIO_VOW_1        =  2,

/* core0 */
	/* the following will use mbox1 */
	IPI_OUT_APCCCI_0          =  3,
	IPI_OUT_DVFS_SET_FREQ_0	  =  4,
	IPI_OUT_C_SLEEP_0         =  5,
	IPI_OUT_TEST_0            =  6,
	IPI_IN_APCCCI_0           =  7,
	IPI_IN_SCP_ERROR_INFO_0   =  8,
	IPI_IN_SCP_READY_0        =  9,
	IPI_IN_SCP_RAM_DUMP_0     = 10,

	/* the following will use mbox2 */
	IPI_OUT_MED_TABLE_ADDR    = 11,
	IPI_IN_MED_CCCI           = 12,
	IPI_OUT_MED_STOP          = 13,
	IPI_IN_MED_STOP_ACK       = 14,

/* core1 */
	/* the following will use mbox3 */
	IPI_OUT_QUERY_MED_SUSPEND = 15,
	IPI_OUT_DVFS_SET_FREQ_1   = 16,
	IPI_OUT_C_SLEEP_1         = 17,
	IPI_OUT_TEST_1            = 18,
	IPI_OUT_MED_UNMASK_IRQ    = 19,
	IPI_OUT_LOGGER_ENABLE_1   = 20,
	IPI_OUT_LOGGER_WAKEUP_1   = 21,
	IPI_OUT_LOGGER_INIT_1     = 22,
	IPI_OUT_SCPCTL_1          = 23,
	IPI_OUT_SCP_LOG_FILTER_1  = 24,
	IPI_IN_MED_SUSPEND_RESPONSE = 25,
	IPI_IN_SCP_ERROR_INFO_1   = 26,
	IPI_IN_LOGGER_WAKEUP_1    = 27,
	IPI_IN_LOGGER_INIT_1      = 28,
	IPI_IN_SCP_READY_1        = 29,
	IPI_IN_SCP_RAM_DUMP_1     = 30,

	/* the following will use mbox4 */
	IPI_OUT_SCP_MPOOL_1       = 31,
	IPI_IN_SCP_MPOOL_1        = 32,
	SCP_IPI_COUNT
};

extern struct mtk_mbox_device scp_mboxdev;
extern struct mtk_ipi_device scp_ipidev;
extern struct mtk_mbox_pin_send *scp_mbox_pin_send;
extern struct mtk_mbox_pin_recv *scp_mbox_pin_recv;

extern char *core_ids[SCP_CORE_TOTAL];

extern void scp_reset_awake_counts(void);
extern int scp_awake_counts[];

extern unsigned int is_scp_ready(enum scp_core_id scp_id);

#endif
