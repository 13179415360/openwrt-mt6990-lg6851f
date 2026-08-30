/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/timer.h>
#include <linux/types.h>
#include "foe_fdb.h"
#include "hnat_ioctl.h"
#include "hnat_define.h"
#include "hnat_power.h"
#include "hnat_fast.h"

static struct wakeup_source *hnat_wakelock;
static bool stay_awake;

#define HNAT_WS_TIMEOUT (5)
static struct timer_list hnat_ws_timer;

static void hnat_wakelock_start_relax_timer(void)
{
	mod_timer(&hnat_ws_timer, jiffies + HZ * HNAT_WS_TIMEOUT);

	if (debug_level == 3)
		pr_info("%s\n", __func__);
}

static void hnat_wakelock_check_bind_cycle(struct timer_list *t)
{
	if (foe_has_bind_entry())
		hnat_wakelock_awake();

	else
		hnat_wakelock_relax();
}


void hnat_wakelock_awake(void)
{
	if (!stay_awake) {
		__pm_stay_awake(hnat_wakelock);
		stay_awake = true;
	}

	hnat_wakelock_start_relax_timer();
}

void hnat_wakelock_relax(void)
{
	if (stay_awake) {
		__pm_relax(hnat_wakelock);
		stay_awake = false;

		if (debug_level == 3)
			pr_info("%s\n", __func__);
	}
}

void hnat_wakelock_register(void)
{
	hnat_wakelock = wakeup_source_register(NULL, "hnat_wakelock");

	if (!hnat_wakelock)
		pr_info("%s, init wakeup source fail!", __func__);

	/* setup ws timer */
	timer_setup(&hnat_ws_timer, hnat_wakelock_check_bind_cycle, 0);
}

void hnat_wakelock_unregister(void)
{
	hnat_wakelock_relax();

	wakeup_source_unregister(hnat_wakelock);

	/* delete ws timer */
	timer_delete_sync(&hnat_ws_timer);
}

/****************************************************************
 * platform device
 ****************************************************************/

static void hnat_device_remove(struct platform_device *dev)
{
}

static int hnat_device_probe(struct platform_device *pdev)
{
	stay_awake = false;

	return 0;
}

int hnat_pm_suspend(struct device *device)
{
	pr_notice("%s\n", __func__);

	/* release wake lock */
	hnat_wakelock_relax();

	ppe_stop();

	ppe_fe_dma_pm_suspend();

	return 0;

}

int hnat_pm_resume(struct device *device)
{
	pr_notice("%s\n", __func__);

	ppe_fe_dma_pm_resume();

	ppe_start();

	/* Set default opp */
	backto_default_opp();

	return 0;
}

#if defined(CONFIG_HNAT_V2)

static const struct dev_pm_ops hnat_pm_ops = {
	.suspend_late = hnat_pm_suspend,
	.resume_early = hnat_pm_resume,
};

static const struct of_device_id hnat_of_ids[] = {
	{ .compatible = "mediatek,hnat", },
	{}
};

static struct platform_driver mtk_hnat_device = {
	.probe  = hnat_device_probe,
	.remove = hnat_device_remove,
	.driver = {
		.name = "hnat",
		.pm = &hnat_pm_ops,
		.owner = THIS_MODULE,
		.of_match_table = hnat_of_ids,
	},
};

void hnat_driver_register(void)
{
	/* suspend/resume is only supported on 6990 */
	if (platform_driver_register(&mtk_hnat_device))
		pr_notice("[HNAT] hnat probe fail\n");

}

void hnat_driver_unregister(void)
{
	/* suspend/resume is only supported on 6990 */
	platform_driver_unregister(&mtk_hnat_device);
}

#endif /* CONFIG_HNAT_V2 */


