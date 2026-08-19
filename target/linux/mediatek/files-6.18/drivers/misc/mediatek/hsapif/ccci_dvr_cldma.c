// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include "ccci_dvr_cldma.h"
#include "ccci_hif_driver.h"

#include "ccci_msg_id.h"
#include "ccci_msg_data.h"
#include "ccci_msg_center.h"

#include "ccci_hif.h"
//#include "ccci_bm.h"
#include "ccci_kmem.h"
#include "ccci_port_base.h"
#include "ccci_state_mgr.h"
#include "ccci_fsm_cldma.h"

#define TAG "cldma_dvr"

extern void ext_ccci_log_init(void);

static struct cldma_clk_node cldma_clk_table[] = {
	{ NULL, "infra-cldma0"},
	{ NULL, "infra-cldma1"},
	{ NULL, "infra-cldma2"},
	{ NULL, "infra-cldma3"},
	{ NULL, "infra-cldma4"},
	{ NULL, "infra-cldma5"},
};

static int ccci_cldma_dvr_read_dts_info(ccci_init_data_t *init_data)
{
	int ret;

	if (init_data->plat_dev->dev.of_node == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] %s is not node\n",
			__func__, init_data->plat_dev->name);

		return -1;
	}

	ret = of_property_read_u32(init_data->plat_dev->dev.of_node,
					"mediatek,hif_ids", &init_data->hif_ids);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: read 'mediatek,hif_ids' fail: %d\n",
			__func__, ret);

		return ret;
	}

	CCCI_INIT_LOG(-1, TAG,
		"[%s] hif_ids: 0x%X\n",
		__func__, init_data->hif_ids);

	return 0;
}

#ifdef __SUPPORT_CLDMA0_SELF_CLK_MGT__
static void hsapif_set_clk_cg(struct platform_device *plat_dev, int enable_clk)
{
	struct clk *cldma_clk = NULL;
	int ret = 0;
	int idx = 0;

#ifdef __CLDMA_SUPPORT_AOC_V2__
	CCCI_INIT_LOG(-1, TAG,
		"[%s] infra-cldma0 ~ infra-cldma5 clk_op=%d\n",
		__func__, enable_clk);

	for (idx = 0; idx < ARRAY_SIZE(cldma_clk_table); idx++) {
		cldma_clk_table[idx].clk_ref = devm_clk_get(&plat_dev->dev,
				cldma_clk_table[idx].clk_name);
		if (IS_ERR(cldma_clk_table[idx].clk_ref)) {
			CCCI_ERROR_LOG(-1, TAG,
					"[%s] Fail to get %s\n",
					__func__, cldma_clk_table[idx].clk_name);
			cldma_clk_table[idx].clk_ref = NULL;
		}
	}

	for (idx = 0; idx < ARRAY_SIZE(cldma_clk_table); idx++) {
		if (cldma_clk_table[idx].clk_ref == NULL)
			continue;
		if(enable_clk) {
			ret = clk_prepare_enable(cldma_clk_table[idx].clk_ref);
			CCCI_INIT_LOG(-1, TAG,
					"[%s] %s clk_ret=%d\n",
					__func__, cldma_clk_table[idx].clk_name, ret);
		} else {
			clk_disable_unprepare(cldma_clk_table[idx].clk_ref);
		}
	}
#else
	CCCI_INIT_LOG(-1, TAG,
		"[%s]infra-cldma0-rh clk_op=%d\n",
		__func__, enable_clk);

	cldma_clk = devm_clk_get(&plat_dev->dev,"infra-cldma0-rh");
	if (IS_ERR(cldma_clk)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s]Fail to get infra-cldma0-rh\n",
			__func__);
	}

	if(enable_clk) {
		ret = clk_prepare_enable(cldma_clk);
		CCCI_INIT_LOG(-1, TAG,
			"[%s] clk_ret=%d\n",
			__func__, ret);
	} else {
		clk_disable_unprepare(cldma_clk);
	}
#endif
}
#endif

static int ccci_cldma_dvr_probe(struct platform_device *plat_dev)
{
	int ret;
	ccci_init_data_t init_data;

	init_data.plat_dev = plat_dev;
	//init_data.hif_ids = (1 << HIF_ID_CLDMA);

	ret = ccci_cldma_dvr_read_dts_info(&init_data);
	if (ret < 0)
		return ret;

#ifdef __SUPPORT_CLDMA0_SELF_CLK_MGT__
	hsapif_set_clk_cg(plat_dev, 1);
#endif

	ret = ccci_msg_send(CCCI_INIT_ID, CCCI_NONE_SUB_ID, &init_data);
	if (ret < 0)
		return ret;

	ret = ccci_msg_send(CCCI_LATE_INIT_ID, CCCI_NONE_SUB_ID, &init_data);
	if (ret < 0)
		return ret;

	ret = ccci_msg_send(CCCI_START_ID, CCCI_NONE_SUB_ID, &init_data);
	if (ret < 0)
		return ret;

	ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_CALL_MHCCIF_IRQ, NULL);
	if (ret < 0)
		return ret;

	CCCI_INIT_LOG(-1, TAG, "[%s] succ.\n", __func__);

	return 0;
}

int ccci_cldma_dvr_remove(struct platform_device *dev)
{
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] \n", __func__);

	return 0;
}

void ccci_cldma_dvr_shutdown(struct platform_device *dev)
{
#ifdef __SUPPORT_CLDMA0_SELF_CLK_MGT__
	hsapif_set_clk_cg(dev, 0);
#endif
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] \n", __func__);
}

int ccci_cldma_dvr_suspend(struct platform_device *dev, pm_message_t state)
{
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] \n", __func__);

	dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_PM_SUSPEND);

	return 0;
}

int ccci_cldma_dvr_resume(struct platform_device *dev)
{
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] \n", __func__);

	dump_cldma0_ip_busy_status_reg(DUMP_IP_BUSY_PM_RESUME);

	return 0;
}

int ccci_cldma_dvr_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] pdev == NULL\n", __func__);

		return -1;
	}

	return ccci_cldma_dvr_suspend(pdev, PMSG_SUSPEND);
}

int ccci_cldma_dvr_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] pdev == NULL\n", __func__);

		return -1;
	}

	return ccci_cldma_dvr_resume(pdev);
}

int ccci_cldma_dvr_pm_restore_noirq(struct device *device)
{
	int ret;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] \n", __func__);

	ret = ccci_msg_send(CCCI_PM_RESTORE_NOIRQ, CCCI_NONE_SUB_ID, NULL);

	/* restore IRQ */
//#ifdef FEATURE_PM_IPO_H
//	irq_set_irq_type(md_ctrl->cldma_irq_id, IRQF_TRIGGER_HIGH);
//	irq_set_irq_type(md_ctrl->md_wdt_irq_id, IRQF_TRIGGER_RISING);
//#endif

	return ret;
}

static const struct dev_pm_ops ccci_cldma_pm_ops = {
	.suspend =       ccci_cldma_dvr_pm_suspend,
	.resume =        ccci_cldma_dvr_pm_resume,
	.freeze =        ccci_cldma_dvr_pm_suspend,
	.thaw =          ccci_cldma_dvr_pm_resume,
	.poweroff =      ccci_cldma_dvr_pm_suspend,
	.restore =       ccci_cldma_dvr_pm_resume,
	.restore_noirq = ccci_cldma_dvr_pm_restore_noirq,

};

#ifdef CONFIG_OF
static const struct of_device_id ccci_cldma_of_ids[] = {
	{.compatible = "mediatek,cldma_sys_ap",},
	{},
};
#endif

static struct platform_driver ccci_cldma_dvr = {
	.driver = {
		   .name = "ccci_cldma_driver",

#ifdef CONFIG_OF
		   .of_match_table = ccci_cldma_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_cldma_pm_ops,
#endif
		   },

	.probe =    ccci_cldma_dvr_probe,
	.remove =   ccci_cldma_dvr_remove,
	.shutdown = ccci_cldma_dvr_shutdown,
	.suspend =  ccci_cldma_dvr_suspend,
	.resume =   ccci_cldma_dvr_resume,

};

static int ccci_cldma_dvr_register_msg(void)
{
	int ret = -1;

	if (ccci_msg_register(CCCI_INIT_ID, CCCI_NONE_SUB_ID,
				NULL, &ccci_cldma_hif_init) < 0)
		return ret;

	if (ccci_msg_register(CCCI_START_ID, CCCI_NONE_SUB_ID,
				NULL, &ext_ccci_all_port_init) < 0)
		return ret;

	return 0;
}

static int __init ccci_driver_init(void)
{
	int ret;
	//struct device_node *node;

	//Init CLDMA proc node for gettting dump log.
#ifdef CONFIG_MTK_HSAPIF_DUMP
	ext_ccci_log_init();
#endif

	CCCI_INIT_LOG(-1, TAG, "[%s] ======== start. ========\n", __func__);

	//node = of_find_compatible_node(NULL, NULL, "mediatek,cldma_sys_ap");
	//CCCI_NORMAL_LOG(-1, TAG, "[%s] mediatek,cldma_sys_ap node: %p\n", __func__, node);

	if (ext_ccci_kmem_init() < 0)
		return -1;

	//if (ext_ccci_subsys_bm_init() < 0)
	//	return -1;

	if (ccci_msg_center_init() < 0)
		return -1;

	if (ccci_state_mgr_init() < 0)
		return -1;

	if (ccci_fsm_cldma_init() < 0)
		return -1;

	if (ccci_cldma_dvr_register_msg() < 0)
		return -1;

	ret = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_INITING, NULL);
	if (ret < 0)
		return ret;

	ret = platform_driver_register(&ccci_cldma_dvr);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] ccci cldma driver register fail(%d)\n",
			__func__, ret);

		return ret;
	}

	CCCI_INIT_LOG(-1, TAG, "[%s] ======== end. ========\n", __func__);

	return 0;
}

module_init(ccci_driver_init);

MODULE_AUTHOR("Xin Xu <xin.xu@mediatek.com>");
MODULE_DESCRIPTION("CCCI cldma driver v1.0");
MODULE_LICENSE("GPL");
