/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _SCP_IPI_WRAPPER_H_
#define _SCP_IPI_WRAPPER_H_

#include "medmcu_ipi_pin.h"
#include "medmcu_mbox_layout.h"

/* retry times * 1000 = 0x7FFF_FFFF, mbox wait maximium */
#define SCP_IPI_LEGACY_WAIT 0x20C49B

struct scp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
};

/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum ipi_id {
	IPI_MPOOL,
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	SCP_NR_IPI,
};

#endif
