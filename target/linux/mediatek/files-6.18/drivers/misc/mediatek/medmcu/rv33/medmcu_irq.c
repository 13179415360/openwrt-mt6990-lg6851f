// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
//#include <mt-plat/sync_write.h>
#include "medmcu_helper.h"
#include "medmcu_common.h"
#include "medmcu_ipi_pin.h"
#include "medmcu_excep.h"
#include "medmcu_dvfs.h"

/*
 * handler for wdt irq for scp
 * dump scp register
 */
static void scp_A_wdt_handler(void)
{
	pr_notice("[SCP] %s\n", __func__);

//	scp_dump_last_regs();

#if SCP_RECOVERY_SUPPORT
	if (scp_set_reset_status() == RESET_STATUS_STOP) {
		pr_debug("[SCP] start to reset scp...\n");
		scp_send_reset_wq(RESET_TYPE_WDT);
	} else
		pr_notice("%s: scp resetting\n", __func__);
#endif
}

static void wait_scp_wdt_irq_done(void)
{
	int retry = 0;
	unsigned long c0, c1;

	/* clr after SCP side INT trigger,
	 * or SCP may lost INT max wait = 200ms
	 */
	for (retry = SCP_AWAKE_TIMEOUT; retry > 0; retry--) {
		c0 = readl(SCP_GPR_CORE0_REBOOT);
		c1 = readl(SCP_GPR_CORE1_REBOOT);
		if ((c0 == CORE_RDY_TO_REBOOT) && (c1 == CORE_RDY_TO_REBOOT))
			break;
		udelay(2);
	}

	if (retry == 0)
		pr_debug("[SCP] SCP wakeup timeout c0:%lx c1:%lx\n", c0, c1);

	udelay(10);
}

/*
 * dispatch scp irq
 * reset scp and generate exception if needed
 * @param irq:      irq id
 * @param dev_id:   should be NULL
 */
irqreturn_t scp_A_irq_handler(int irq, void *dev_id)
{
	struct arm_smccc_res ares;
	unsigned int reg0 = readl(R_CORE0_WDT_IRQ);
	unsigned int reg1 = readl(R_CORE1_WDT_IRQ);

	if (reg0 | reg1) {
		scp_A_wdt_handler();
		/* clear IRQ */
		wait_scp_wdt_irq_done();
		if (reg0)
			arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
				MTK_SIP_MEDMCU_WDT_IRQ_CLR,
				0, 0, 0, 0, 0, 0,
				&ares);
		if (reg1)
			arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
				MTK_SIP_MEDMCU_WDT_IRQ_CLR,
				1, 0, 0, 0, 0, 0,
				&ares);
	}
	return IRQ_HANDLED;
}
