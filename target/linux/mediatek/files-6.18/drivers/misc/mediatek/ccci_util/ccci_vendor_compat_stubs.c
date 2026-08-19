// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/skbuff.h>

#include "ccci_vendor_compat.h"

/* LG6851F Stage4K: selected weak vendor fallback hooks */
/* Extracted from ccci_util_dummy.c; keep these definitions weak. */

bool __weak spm_is_md1_sleep(void)
{
	pr_notice("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
int __weak switch_sim_mode(int id, char *buf, unsigned int len)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
unsigned int __weak get_sim_switch_type(void)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
unsigned int __weak mt_irq_get_pending(unsigned int irq)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
int __weak mbim_start_xmit(struct sk_buff *skb, int ifid)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}
/* LG6851F Stage5E: EMI/MRDUMP weak compatibility fallbacks.
 *
 * These permit the imported modem stack to link while the real MT6990 EMI-MPU
 * and MRDUMP providers are absent. They intentionally do not claim that the
 * corresponding diagnostic/protection features are operational.
 */
#include <linux/errno.h>
#include <linux/compiler_attributes.h>
#include <soc/mediatek/emi.h>

void __weak mtk_clear_md_violation(void)
{
}

int __weak mtk_emimpu_md_handling_register(
		emimpu_md_handler md_handling_func)
{
	(void)md_handling_func;
	return -EOPNOTSUPP;
}

void __weak trigger_mrdump_after_mdee(void)
{
}
