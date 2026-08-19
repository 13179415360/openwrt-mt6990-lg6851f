/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MEDMCU_COMMON_H__
#define __MEDMCU_COMMON_H__
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include "medmcu_feature_define.h"

/* MT6990 ATF implements the vendor MEDMCU service at function 0x528.
 * This service ID existed in the vendor 5.4/5.15 header but is not part of
 * the upstream MediaTek SIP header yet.
 */
#ifndef MTK_SIP_KERNEL_MEDMCU_CONTROL
#define MTK_SIP_KERNEL_MEDMCU_CONTROL MTK_SIP_SMC_CMD(0x528)
#endif

#define MEDMCU_MAGIC_NUM 0x4944484766AAAA66

enum MEDMCU_DESC_TYPE {
	TX_DESC         = 0,
	RX_DESC         = 1,
	HNAT_INFO       = 2,
	HNAT_INFO_HOST  = 3,
	PIT             = 4,
	PIT_NAT         = 5,
	DRB0            = 6,
	DRB1            = 7,
#ifdef MED_V2
	DRB2            = 8,
	RX_DESC_1       = 9,
#endif
	NR_DESC_TYPE
};

enum MEDMCU_SIP_ID {
	MTK_SIP_MEDMCU_PRE_CTRL = 0,
	MTK_SIP_MEDMCU_RSTN_SET = 1,
	MTK_SIP_MEDMCU_RSTN_CLR = 2,
	MTK_SIP_MEDMCU_L2TCM_RESTORE = 3,
	MTK_SIP_MEDMCU_WDT_SET = 4,
	MTK_SIP_MEDMCU_WDT_IRQ_CLR = 5,
	MTK_SIP_MEDMCU_WDT_START = 6,
	MTK_SIP_MEDMCU_WDT_DISABLE = 7,
	MTK_SIP_MEDMCU_GPR_RESV_MEM = 8,
	MTK_SIP_MEDMCU_LOG_DRAM = 9,
	MTK_SIP_MEDMCU_SEC_CTRL = 10,
	MTK_SIP_MEDMCU_DBG_CTRL = 11,
	MTK_SIP_MEDMCU_REBOOT_STATUS = 12,
};

struct medmcu_desc_info_t {
	void __iomem *base_virt;
	unsigned long long sz;
	phys_addr_t phy_base;
};

struct medmcu_globals {
	void __iomem *medhw_base;
	void __iomem *fe_base;
	struct medmcu_desc_info_t desc[NR_DESC_TYPE];
#ifdef MED_V2
	struct clk *medsys_clk;
#endif
	struct wakeup_source *dl_wakelock;
	unsigned int ignore_stop_ipi_flag;
};

extern struct medmcu_desc_info_t *get_desc_info(void);
extern void __iomem *get_medhw_base(void);

#endif
