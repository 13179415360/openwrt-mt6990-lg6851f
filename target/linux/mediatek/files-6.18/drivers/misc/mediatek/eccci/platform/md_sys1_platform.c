// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */
#include <linux/arm-smccc.h>
#include <linux/bootprof.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include "ccci_config.h"
#include "ccci_common_config.h"
#include <linux/clk.h>
#include <linux/soc/mediatek/mtk_sip_svc.h> /* LG6851F Stage5K: MediaTek SIP command encoding */

/* LG6851F Stage5K: verified vendor CCCI SIP ABI. */
#ifndef MTK_SIP_KERNEL_CCCI_CONTROL
#define MTK_SIP_KERNEL_CCCI_CONTROL MTK_SIP_SMC_CMD(0x505)
#endif
#ifdef USING_PM_RUNTIME
#include <linux/pm_runtime.h>
#else
#include <dt-bindings/clock/mt6779-clk.h>
#endif
#ifdef CONFIG_MTK_EMI_BWL
#include <emi_mbw.h>
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
#include <mach/mt6605.h>
#endif

#ifdef CONFIG_MTK_QOS_SUPPORT
#include <linux/pm_qos.h>
#include <helio-dvfsrc-opp.h>
#endif

#include <linux/regulator/consumer.h> /* for MD PMIC */

#include "ccci_core.h"
#include "ccci_platform.h"
#include "modem_sys.h"

#include "md_sys1_platform.h"
#include "modem_secure_base.h"
#include "modem_reg_base.h"

#ifndef _CTRL_PLANE_GEN98_
#include "ap_md_reg_dump.h"
#else
#include "ap_md_reg_dump_v1.h"
#endif


struct ccci_md_regulator {
	struct regulator *reg_ref;
	unsigned char *reg_name;
	unsigned long reg_vol0;
	unsigned long reg_vol1;
};
static struct ccci_md_regulator md_reg_table[] = {
#ifndef _CTRL_PLANE_GEN98_
	{ NULL, "vmd11", 825000, 825000},
	{ NULL, "vsram_md", 825000, 825000},
	{ NULL, "vrfdig", 700000, 700000},
#else
	{ NULL, "cpu", 800000, 800000},
	{ NULL, "vsram_modem", 800000, 800000},
	{ NULL, "vrfdig", 725000, 725000},
#endif
};

static struct ccci_plat_val md_cd_plat_val_ptr;

static struct ccci_clk_node clk_table[] = {
/* #ifdef USING_PM_RUNTIME */
	{ NULL, "scp-sys-md1-main"},
/* #endif */
};
#define TAG "mcd"

#define ROr2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)|c))
#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))
#define RabIsc(a, b, c) ((ccci_read32(a, b)&c) != c)

static int md_cd_io_remap_md_side_register(struct ccci_modem *md);
static void md_cd_dump_debug_register(struct ccci_modem *md, bool isr_skip_dump);
static void md_cd_dump_md_bootup_status(struct ccci_modem *md);
static void md_cd_get_md_bootup_status(struct ccci_modem *md,
	unsigned int *buff, int length);
static void md_cd_check_emi_state(struct ccci_modem *md, int polling);
static int md_start_platform(struct ccci_modem *md);
static int md_cd_power_on(struct ccci_modem *md);
static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout);
static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode);
static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode);
static int md_cd_let_md_go(struct ccci_modem *md);
static void md_cd_lock_modem_clock_src(int locked);

static int ccci_modem_remove(struct platform_device *dev);
static void ccci_modem_shutdown(struct platform_device *dev);
static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state);
static int ccci_modem_resume(struct platform_device *dev);
static int ccci_modem_pm_suspend(struct device *device);
static int ccci_modem_pm_resume(struct device *device);
static int ccci_modem_pm_restore_noirq(struct device *device);


static struct ccci_plat_ops md_cd_plat_ptr = {
	.init = &ccci_platform_init_6880,
#ifndef _CTRL_PLANE_GEN98_
	.md_dump_reg = &md_dump_register_6880,
#else
	.md_dump_reg = &md_dump_register_6980,
#endif
	.remap_md_reg = &md_cd_io_remap_md_side_register,
	.lock_modem_clock_src = &md_cd_lock_modem_clock_src,
	.dump_md_bootup_status = &md_cd_dump_md_bootup_status,
	.get_md_bootup_status = &md_cd_get_md_bootup_status,
	.debug_reg = &md_cd_dump_debug_register,
	.check_emi_state = &md_cd_check_emi_state,
	.soft_power_off = &md_cd_soft_power_off,
	.soft_power_on = &md_cd_soft_power_on,
	.start_platform = &md_start_platform,
	.power_on = &md_cd_power_on,
	.let_md_go = &md_cd_let_md_go,
	.power_off = &md_cd_power_off,
	.vcore_config = NULL,
};

#ifdef ENABLE_DEBUG_DUMP /* Fix me! */
void md1_subsys_debug_dump(enum subsys_id sys)
{
	struct ccci_modem *md = NULL;

	if (sys != SYS_MD1)
		return;
		/* add debug dump */

	CCCI_NORMAL_LOG(0, TAG, "%s\n", __func__);
	md = ccci_md_get_modem_by_id(0);
	if (md != NULL) {
		CCCI_NORMAL_LOG(0, TAG, "%s dump start\n", __func__);
		md->ops->dump_info(md, DUMP_FLAG_CCIF_REG | DUMP_FLAG_CCIF |
			DUMP_FLAG_REG | DUMP_FLAG_QUEUE_0_1 |
			DUMP_MD_BOOTUP_STATUS, NULL, 0);
		mdelay(1000);
		md->ops->dump_info(md, DUMP_FLAG_REG, NULL, 0);
	}
	CCCI_NORMAL_LOG(0, TAG, "%s exit\n", __func__);
}


struct pg_callbacks md1_subsys_handle = {
	.debug_dump = md1_subsys_debug_dump,
};

void ccci_dump(void)
{
	md1_subsys_debug_dump(SYS_MD1);
}
EXPORT_SYMBOL(ccci_dump);
#endif
static int md_cd_get_modem_hw_info(struct platform_device *dev_ptr,
	struct ccci_dev_cfg *dev_cfg, struct md_hw_info *hw_info)
{
	struct device_node *node = NULL;
	int idx = 0;
	int ret = -1;
#ifdef USING_PM_RUNTIME
	int retval = 0;
#endif

	if (dev_ptr->dev.of_node == NULL) {
		CCCI_ERROR_LOG(0, TAG, "modem OF node NULL\n");
		return -1;
	}

	memset(dev_cfg, 0, sizeof(struct ccci_dev_cfg));
	of_property_read_u32(dev_ptr->dev.of_node,
		"mediatek,md_id", &dev_cfg->index);
	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"modem hw info get idx:%d\n", dev_cfg->index);
	if (!get_modem_is_enabled(dev_cfg->index)) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"modem %d not enable, exit\n", dev_cfg->index + 1);
		return -1;
	}

	switch (dev_cfg->index) {
	case 0:		/* MD_SYS1 */
		dev_cfg->major = 0;
		dev_cfg->minor_base = 0;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,cldma_capability", &dev_cfg->capability);

#ifdef _CTRL_PLANE_GEN98_
		ret = of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,offset_epon_md1", &hw_info->md_epon_offset);
		if (ret < 0) {
			CCCI_ERROR_LOG(0, TAG, "%s:get DTS:mediatek,offset_epon_md1 fail\n",
				__func__);
			hw_info->md_epon_offset = 0;
		}

		ret = of_property_read_u32_index(dev_ptr->dev.of_node,
				"mediatek,offset_epon_md1", 1, &retval);
		if (ret < 0)
			hw_info->md_l2sram_base = NULL;
		else
			hw_info->md_l2sram_base = of_iomap(dev_ptr->dev.of_node, 0);
		CCCI_NORMAL_LOG(0, TAG, "%s, val: %s, 0x%x\n", __func__,
			hw_info->md_l2sram_base?"l2sram":"mddbgss", hw_info->md_epon_offset);
#endif

		/* MD excption irq */
		hw_info->md_wdt_irq_id =
			irq_of_parse_and_map(dev_ptr->dev.of_node, 0);

		/* Device tree using none flag to register irq,
		 * sensitivity has set at "irq_of_parse_and_map"
		 */
		hw_info->ap_ccif_irq1_flags = IRQF_TRIGGER_NONE;
		hw_info->md_wdt_irq_flags = IRQF_TRIGGER_NONE;

		hw_info->sram_size = CCIF_SRAM_SIZE;
		hw_info->md_rgu_base = MD_RGU_BASE;
		hw_info->md_boot_slave_En = MD_BOOT_VECTOR_EN;
		of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,md_generation", &md_cd_plat_val_ptr.md_gen);

		md_cd_plat_val_ptr.infra_ao_base =
				syscon_regmap_lookup_by_phandle(dev_ptr->dev.of_node,
				"ccci-infracfg");
		if (!md_cd_plat_val_ptr.infra_ao_base) {
			CCCI_ERROR_LOG(dev_cfg->index, TAG,
				"infra_ao fail: NULL!\n");
			return -1;
		}

		hw_info->plat_ptr = &md_cd_plat_ptr;
		hw_info->plat_val = &md_cd_plat_val_ptr;
		if ((hw_info->plat_ptr == NULL) || (hw_info->plat_val == NULL))
			return -1;
		hw_info->plat_val->offset_epof_md1 = 7*1024+0x234;
		for (idx = 0; idx < ARRAY_SIZE(clk_table); idx++) {
			clk_table[idx].clk_ref = devm_clk_get(&dev_ptr->dev,
				clk_table[idx].clk_name);
			if (IS_ERR(clk_table[idx].clk_ref)) {
				CCCI_ERROR_LOG(dev_cfg->index, TAG,
					 "md%d get %s failed\n",
						dev_cfg->index + 1,
						clk_table[idx].clk_name);
				clk_table[idx].clk_ref = NULL;
			}
		}
		node = of_find_compatible_node(NULL, NULL,
			"mediatek,topckgen");//"mediatek,mt6873-topckgen"
		if (!node) {
			CCCI_ERROR_LOG(-1, TAG,
				"%s: find node: 'mediatek,topckgen' fail.\n",
				__func__);
			// no need for colgin
			//node = of_find_compatible_node(NULL, NULL,
			//	"mediatek,mt6853-topckgen");
		}
		if (node)
			hw_info->ap_topclkgen_base = of_iomap(node, 0);

		else {
			CCCI_ERROR_LOG(-1, TAG,
				"%s:ioremap topclkgen base address fail\n",
				__func__);
			return -1;
		}

		/*
		* md_cd_plat_val_ptr.power_flow_config will decide use which flow:
		* bit0: means to set srcclkena
		* bit1: means to set srclken_o1_on
		* bit2: means to config md1_revert_sequencer_setting
		* bit3: means to config md_pll_setting
		* bit4: means to skip md_cd_topclkgen_on/off
		*/
		ret = of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,power_flow_config",
			&md_cd_plat_val_ptr.power_flow_config);
		if (ret < 0) {
			md_cd_plat_val_ptr.power_flow_config = 0;
			CCCI_ERROR_LOG(0, TAG, "%s:get DTS:power_flow_config fail\n",
				__func__);
		} else
			CCCI_NORMAL_LOG(dev_cfg->index, TAG,
				"%s:power_flow_config=0x%x\n",
				__func__, md_cd_plat_val_ptr.power_flow_config);

		ret = of_property_read_u32(dev_ptr->dev.of_node,
			"mediatek,srclken_o1", &md_cd_plat_val_ptr.srclken_o1_bit);
		if (ret < 0) {
			CCCI_ERROR_LOG(0, TAG,
				"%s:get DTS: srclken_o1 fail, no need set\n",
				__func__);
			md_cd_plat_val_ptr.srclken_o1_bit = -1;
		} else
			CCCI_NORMAL_LOG(dev_cfg->index, TAG,
				"%s:srclken_o1_bit=0x%x\n",
				__func__, md_cd_plat_val_ptr.srclken_o1_bit);

		break;
	default:
		return -1;
	}

	if (hw_info->md_wdt_irq_id == 0) {
		CCCI_ERROR_LOG(dev_cfg->index, TAG,
			"md_wdt_irq:%d\n",
			hw_info->md_wdt_irq_id);
		return -1;
	}

	/* Get spm sleep base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,sleep");
	hw_info->spm_sleep_base = (unsigned long)of_iomap(node, 0);
	if (!hw_info->spm_sleep_base) {
		CCCI_ERROR_LOG(0, TAG,
			"%s: spm_sleep_base of_iomap failed\n",
			__func__);
		return -1;
	}
	CCCI_INIT_LOG(-1, TAG, "spm_sleep_base:0x%lx\n",
			(unsigned long)hw_info->spm_sleep_base);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"dev_major:%d,minor_base:%d,capability:%d\n",
		dev_cfg->major, dev_cfg->minor_base, dev_cfg->capability);

	CCCI_DEBUG_LOG(dev_cfg->index, TAG,
		"ccif_irq1:%d,md_wdt_irq:%d\n",
		hw_info->ap_ccif_irq1_id, hw_info->md_wdt_irq_id);

	//xuxin-clk-pg//register_pg_callback(&md1_subsys_handle);
#ifdef USING_PM_RUNTIME
	pm_runtime_enable(&dev_ptr->dev);
	dev_pm_syscore_device(&dev_ptr->dev, true);

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON start\n");
	CCCI_NORMAL_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON start\n");

	// LK already turn on MD, but SPM LK and kernel not sync,
	// So it needs ccci to turn on MD again to let SPM get/put status sync
	// and already make sure MD has no problem when power on MD twice
	retval = pm_runtime_get_sync(&dev_ptr->dev); /* match lk on */

	CCCI_BOOTUP_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON end %d\n", retval);
	CCCI_NORMAL_LOG(dev_cfg->index, TAG,
		"[POWER ON] dummy: MD MTCMOS ON end %d\n", retval);

#endif

	return 0;
}

static int md_cd_io_remap_md_side_register(struct ccci_modem *md)
{
	struct md_pll_reg *md_reg;
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;

#ifdef _CTRL_PLANE_GEN98_
	/* call internal_dump io_remap */
	md_io_remap_internal_dump_register(md);
#endif

	md_info->md_boot_slave_En =
	 ioremap(md->hw_info->md_boot_slave_En, 0x4);
	md_info->md_rgu_base =
	 ioremap(md->hw_info->md_rgu_base, 0x300);

	md_reg = kzalloc(sizeof(struct md_pll_reg), GFP_KERNEL);
	if (md_reg == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
		 "md_sw_init:alloc md reg map mem fail\n");
		return -1;
	}

	md_reg->md_boot_stats_select =
		ioremap(MD1_BOOT_STATS_SELECT, 4);
	md_reg->md_boot_stats = ioremap(MD1_CFG_BOOT_STATS, 4);
	/*just for dump end*/

	md_info->md_pll_base = md_reg;

#ifdef MD_PEER_WAKEUP
	md_info->md_peer_wakeup = ioremap(MD_PEER_WAKEUP, 0x4);
#endif
	return 0;
}

static void md_cd_lock_modem_clock_src(int locked)
{
	struct arm_smccc_res res;
	unsigned int settle;
	unsigned int ret;
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_CLOCK_REQUEST,
			MD_REG_AP_MDSRC_REQ,
			locked, 0, 0, 0, 0, &res);

	if (locked) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL,
				MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_SETTLE,
				0, 0, 0, 0, 0, &res);
		settle = res.a0;

		mdelay(settle);
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL,
				MD_CLOCK_REQUEST,
				MD_REG_AP_MDSRC_ACK,
				0, 0, 0, 0, 0, &res);
		ret = res.a0;

		CCCI_NOTICE_LOG(-1, TAG,
				"settle = %u; ret = %u\n", settle, ret);
	}
}

static void md_cd_dump_md_bootup_status(struct ccci_modem *md)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	/*To avoid AP/MD interface delay,
	 * dump 3 times, and buy-in the 3rd dump value.
	 */

	ccci_write32(md_reg->md_boot_stats_select, 0, 0);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));

	ccci_write32(md_reg->md_boot_stats_select, 0, 1);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats1:0x%X\n",
		ccci_read32(md_reg->md_boot_stats, 0));
}

static void md_cd_get_md_bootup_status(
	struct ccci_modem *md, unsigned int *buff, int length)
{
	struct md_sys1_info *md_info = (struct md_sys1_info *)md->private_data;
	struct md_pll_reg *md_reg = md_info->md_pll_base;

	CCCI_NOTICE_LOG(md->index, TAG, "md_boot_stats len %d\n", length);

	if (length < 2 || buff == NULL) {
		md_cd_dump_md_bootup_status(md);
		return;
	}

	ccci_write32(md_reg->md_boot_stats_select, 0, 2);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[0] = ccci_read32(md_reg->md_boot_stats, 0);

	ccci_write32(md_reg->md_boot_stats_select, 0, 3);
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	ccci_read32(md_reg->md_boot_stats, 0);	/* dummy read */
	buff[1] = ccci_read32(md_reg->md_boot_stats, 0);
	CCCI_NOTICE_LOG(md->index, TAG,
		"md_boot_stats0 / 1:0x%X / 0x%X\n", buff[0], buff[1]);

}


static int dump_emi_last_bm(struct ccci_modem *md)
{
	u32 buf_len = 1024;
	char *buf = NULL;

	buf = kzalloc(buf_len, GFP_ATOMIC);
	if (!buf) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"alloc memory failed for emi last bm\n");
		return -1;
	}

	//dump_last_bm(buf, buf_len);
	CCCI_MEM_LOG_TAG(md->index, TAG, "Dump EMI last bm\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_MEM_DUMP, buf, buf_len);

	kfree(buf);

	return 0;
}

#define PCCOM_ADD 0x20291E50
static void md_pccom_verifty(int prt_count)
{
	void __iomem *pccon_vradd = ioremap(PCCOM_ADD, 0x4);
	while(prt_count-- > 0){
		CCCI_DEBUG_LOG(MD_SYS1, TAG, "MD PC: 0x%x \n", ccci_read32(pccon_vradd, 0x0));
		mdelay(1);  // 50ms-->1ms for boot time optimization
	}
}

void mtk_suspend_emiisu(void);

void __weak mtk_suspend_emiisu(void)
{
	CCCI_DEBUG_LOG(-1, TAG, "No %s\n", __func__);
}

static atomic_t reg_dump_ongoing;
static void md_cd_dump_debug_register(struct ccci_modem *md, bool isr_skip_dump)
{
	/* MD no need dump because of bus hang happened - open for debug */
	unsigned int reg_value[2] = { 0 };
	unsigned int ccif_sram[
		CCCI_EE_SIZE_CCIF_SRAM/sizeof(unsigned int)] = { 0 };

#ifndef _CTRL_PLANE_GEN98_
	unsigned int reg0_check_value = 0x5443000CU;
#else
	unsigned int reg0_check_value = 0x5443000FU;
#endif

	/* EMI debug feature */
	mtk_suspend_emiisu();
	dump_emi_last_bm(md);
	md_cd_get_md_bootup_status(md, reg_value, 2);
	md->ops->dump_info(md, DUMP_FLAG_CCIF, ccif_sram, 0);
	/* copy from HS1 timeout */
	if ((reg_value[0] == 0) && (ccif_sram[1] == 0)) {
		CCCI_NORMAL_LOG(-1, TAG, "%s, MD boot status fail\n",__func__);
		return;
	} else if (!((reg_value[0] == reg0_check_value) || (reg_value[0] == 0) ||
		(reg_value[0] >= 0x53310000 && reg_value[0] <= 0x533100FF))) {
		CCCI_NORMAL_LOG(-1, TAG, "%s, MD BROM fail\n",__func__);
		return;
	}
	if (unlikely(in_interrupt()) && isr_skip_dump) {
		CCCI_MEM_LOG_TAG(md->index, TAG,
			"In interrupt, skip dump MD debug register.\n");
		return;
	}

	if (atomic_cmpxchg(&reg_dump_ongoing, 0, 1) == 1) {
		CCCI_NORMAL_LOG(md->index, TAG, "[%s] one dump already on-going\n", __func__);
		return;
	}

	md_cd_lock_modem_clock_src(1);
	if (md->hw_info->plat_ptr->md_dump_reg)
		md->hw_info->plat_ptr->md_dump_reg(md->index);

	md_cd_lock_modem_clock_src(0);

	atomic_set(&reg_dump_ongoing, 0);
}

#ifndef CCCI_KMODULE_ENABLE
void md_cd_dump_pccif_reg(struct ccci_modem *md)
{
	struct md_hw_info *hw_info = md->hw_info;

	md_cd_lock_modem_clock_src(1);

	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_CON(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_CON,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_CON));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_BUSY(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_BUSY,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_BUSY));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_START(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_START,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_START));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_TCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_TCHNUM,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_TCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_RCHNUM(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_RCHNUM,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_RCHNUM));
	CCCI_MEM_LOG_TAG(md->index, TAG,
		"AP_ACK(%p)=%x\n",
		hw_info->md_pcore_pccif_base + APCCIF_ACK,
		ccif_read32(hw_info->md_pcore_pccif_base, APCCIF_ACK));

	md_cd_lock_modem_clock_src(0);
}
#endif
static void md_cd_check_emi_state(struct ccci_modem *md, int polling)
{
}

static int md1_pmic_setting_init(struct platform_device *plat_dev)
{
	int idx, ret;

	CCCI_BOOTUP_LOG(-1, TAG, "get pmic setting\n");
	for (idx = 0; idx < ARRAY_SIZE(md_reg_table); idx++) {
		md_reg_table[idx].reg_ref =
			devm_regulator_get_optional(&plat_dev->dev,
			md_reg_table[idx].reg_name);
		if (IS_ERR(md_reg_table[idx].reg_ref)) {
			ret = PTR_ERR(md_reg_table[idx].reg_ref);
			if ((ret != -ENODEV) && plat_dev->dev.of_node) {
				CCCI_ERROR_LOG(-1, TAG,
					"get regulator(%s) fail: ret = %d\n",
					md_reg_table[idx].reg_name, ret);
				//return -1;
			} else
				CCCI_ERROR_LOG(-1, TAG,
					"get regulator(%s) fail 1: ret = %d\n",
					md_reg_table[idx].reg_name, ret);

			md_reg_table[idx].reg_ref = NULL;
			return -1;
		} else
			CCCI_NORMAL_LOG(-1, TAG,
					"get regulator(%s) successfully\n",
					md_reg_table[idx].reg_name);
	}
	return 0;
}

static void md1_pmic_setting_on(void)
{
	int ret = -1, idx;

	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON]%s start\n", __func__);
	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON]%s start\n", __func__);

	for (idx = 0; idx < ARRAY_SIZE(md_reg_table); idx++) {
		if (IS_ERR(md_reg_table[idx].reg_ref)) {
			ret = PTR_ERR(md_reg_table[idx].reg_ref);
			if (ret != -ENODEV) {
				CCCI_ERROR_LOG(-1, TAG,
					"%s:get regulator(%s) fail, ret = %d\n",
					__func__, md_reg_table[idx].reg_name, ret);
				CCCI_BOOTUP_LOG(-1, TAG, "bypass pmic_%s set\n",
						md_reg_table[idx].reg_name);
				continue;
			}
		} else {
#ifdef _CTRL_PLANE_GEN98_
			/* Vmodem->2ms->Vsram_md */
			if (strcmp(md_reg_table[idx].reg_name,
				"vsram_modem") == 0)
				udelay(2000);
#endif
			ret = regulator_set_voltage(md_reg_table[idx].reg_ref,
				md_reg_table[idx].reg_vol0,
				md_reg_table[idx].reg_vol1);
			if (ret) {
				CCCI_ERROR_LOG(-1, TAG, "pmic_%s set fail\n",
					md_reg_table[idx].reg_name);
				continue;
			} else
				CCCI_BOOTUP_LOG(-1, TAG,
					"[POWER ON]pmic set_voltage %s=%ld uV\n",
					md_reg_table[idx].reg_name,
					md_reg_table[idx].reg_vol0);

			ret = regulator_sync_voltage(
				md_reg_table[idx].reg_ref);
			if (ret)
				CCCI_ERROR_LOG(-1, TAG, "pmic_%s sync fail\n",
					md_reg_table[idx].reg_name);
			else {
				CCCI_BOOTUP_LOG(-1, TAG,
					"[POWER ON]pmic get_voltage %s=%d uV\n",
					md_reg_table[idx].reg_name,
					regulator_get_voltage(
					md_reg_table[idx].reg_ref));
			}
		}
	}
	CCCI_BOOTUP_LOG(-1, TAG, "[POWER ON]%s end\n", __func__);
	CCCI_NORMAL_LOG(-1, TAG, "[POWER ON]%s end\n", __func__);
}

static void flight_mode_set_by_atf(struct ccci_modem *md,
		unsigned int flightMode)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_FLIGHT_MODE_SET,
		flightMode, 0, 0, 0, 0, 0, &res);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[%s] flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
		__func__, res.a0, res.a1, res.a2, res.a3);
}

static int md_cd_soft_power_off(struct ccci_modem *md, unsigned int mode)
{
	flight_mode_set_by_atf(md, true);
	return 0;
}

static int md_cd_soft_power_on(struct ccci_modem *md, unsigned int mode)
{
	flight_mode_set_by_atf(md, false);
	return 0;
}

static int md_start_platform(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	int timeout = 100; /* 100 * 20ms = 2s */
	int ret = -1;
#ifndef USING_PM_RUNTIME
	int retval = 0;
#endif

	if ((md->per_md_data.config.setting&MD_SETTING_FIRST_BOOT) == 0)
		return 0;

	while (md1_pmic_setting_init(md->plat_dev) != 0) {
		msleep(10);
	}

	while (timeout > 0) {
		arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
				MD_CHECK_DONE,
				0, 0, 0, 0, 0, &res);
		ret = res.a0;
		if (!ret) {
			CCCI_BOOTUP_LOG(md->index, TAG, "BROM PASS\n");
			break;
		}
		timeout--;
		msleep(20);
	}

#ifndef USING_PM_RUNTIME
	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk\n");
	retval = clk_prepare_enable(clk_table[0].clk_ref); /* match lk on */
	if (retval)
		CCCI_ERROR_LOG(md->index, TAG,
			"dummy md sys clk fail: ret = %d\n", retval);
	CCCI_BOOTUP_LOG(md->index, TAG, "dummy md sys clk done\n");
#endif

	//md_cd_dump_md_bootup_status(md);  // no need for colgin
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_CHECK_FLAG, 0, 0, 0, 0, 0, &res);
	CCCI_NORMAL_LOG(md->index, TAG,
			"flag_1=%lu, flag_2=%lu, flag_3=%lu, flag_4=%lu\n",
			res.a0, res.a1, res.a2, res.a3);

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
		MD_BOOT_STATUS, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
			res.a0, res.a1, res.a2);
	CCCI_NORMAL_LOG(md->index, TAG,
			"AP: boot_ret=%lu, boot_status_0=%lX, boot_status_1=%lX\n",
			res.a0, res.a1, res.a2);
	if (ret != 0) {
		/* BROM */
		CCCI_ERROR_LOG(md->index, TAG, "BROM Failed\n");
	}

	CCCI_NORMAL_LOG(md->index, TAG, "start md_cd_power_off\n");
	md_cd_power_off(md, 0);
	CCCI_NORMAL_LOG(md->index, TAG, "md_cd_power_off end\n");
	return ret;
}

static int md_cd_topclkgen_on(struct ccci_modem *md)
{
	unsigned int reg_value;

	if (md_cd_plat_val_ptr.power_flow_config & (1 << SKIP_TOPCLK_BIT)) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] bypass %s\n", __func__);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER ON] bypass %s\n", __func__);
		return 0;
	}

	reg_value = ccci_read32(md->hw_info->ap_topclkgen_base, 0);
	reg_value &= ~((1<<8) | (1<<9));
	ccci_write32(md->hw_info->ap_topclkgen_base, 0, reg_value);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s end: set md1_clk_mod = 0x%x\n",
		__func__, ccci_read32(md->hw_info->ap_topclkgen_base, 0));
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s end: set md1_clk_mod = 0x%x\n",
		__func__, ccci_read32(md->hw_info->ap_topclkgen_base, 0));

	return 0;
}

static int mtk_ccci_cfg_srclken_o1_on(struct ccci_modem *md)
{
	unsigned int val = 0;
	struct md_hw_info *hw_info = md->hw_info;

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << SRCLKEN_O1_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] bypass %s step\n", __func__);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] bypass %s step\n", __func__);
		return 0;
	}

	if (md_cd_plat_val_ptr.srclken_o1_bit < 0)
		return -1;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on start\n", __func__);

	if (hw_info->spm_sleep_base) {
		ccci_write32((void __iomem *)hw_info->spm_sleep_base, 0, 0x0B160001);
		val = ccci_read32(hw_info->spm_sleep_base, 0);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base: val:0x%x\n", val);

		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x +\n", val);
		val |= md_cd_plat_val_ptr.srclken_o1_bit;
		ccci_write32((void __iomem *)hw_info->spm_sleep_base, 8, val);
		val = ccci_read32(hw_info->spm_sleep_base, 8);
		CCCI_NORMAL_LOG(-1, TAG, "spm_sleep_base+8: val:0x%x -\n", val);
	}

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on done\n",
		__func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s: set srclken_o1_on done\n",
		__func__);

	return 0;
}

static int md_cd_srcclkena_setting(struct ccci_modem *md)
{
	unsigned int reg_value;
	int ret;

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << SRCCLKENA_SETTING_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] bypass md_cd_srcclkena_setting step\n");
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] bypass md_cd_srcclkena_setting step\n");
		return 0;
	}

	ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:read INFRA_AO_MD_SRCCLKENA fail,ret=%d\n",
			__func__, ret);
		return ret;
	}

	reg_value &= ~(0xFF);
	reg_value |= 0x21;
	ret = regmap_write(md->hw_info->plat_val->infra_ao_base,
		INFRA_AO_MD_SRCCLKENA, reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:write INFRA_AO_MD_SRCCLKENA value=%u fail,ret=%d\n",
			__func__, reg_value, ret);
		return -1;
	}

	ret = regmap_read(md->hw_info->plat_val->infra_ao_base,
			INFRA_AO_MD_SRCCLKENA, &reg_value);
	if (ret) {
		CCCI_ERROR_LOG(md->index, TAG,
			"%s:re-read INFRA_AO_MD_SRCCLKENA fail,ret=%d\n",
			__func__, ret);
	}
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON]%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__, reg_value);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON]%s: set md1_srcclkena bit(0x1000_0F0C)=0x%x\n",
		__func__, reg_value);
	return 0;
}

/*
 * revert sequencer setting to AOC2.5 for gen98:
 * 1.disable sequencer
 * 2.wait sequencer done
 * 3.set sequencer mux to AOC2.5
 */
static int md1_revert_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF] %s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s start\n", __func__);

	if (!(md_cd_plat_val_ptr.power_flow_config & (1 << REVERT_SEQUENCER_BIT))) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		return 0;
	}

	//reg = ioremap_wc(0x1C803000, 0x1000);  // Lepin
	reg = ioremap_wc(0x12830000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER OFF] ioremap 0x1000 bytes from 0x12830000 fail\n");
		return -1;
	}

	/* disable sequencer */
	ccci_write32(reg, 0x204, 0x0);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] disable sequencer done\n");

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x1010001)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x12830200=0x%x,0x12830204=0x%x,0x12830208=0x%x,0x12830310=0x%x\n",
				ccci_read32(reg, 0x200), ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208), ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] wait sequencer done\n");

	/* revert mux of sequencer to AOC1.0 */
	ccci_write32(reg, 0x208, 0x5000D);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF] %s end\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s end\n", __func__);

	iounmap(reg);

	return 0;
}

static int md1_enable_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	if (!md_cd_plat_val_ptr.md_first_power_on) {
		CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF]%s:md_first_power_on=%u,exit\n",
			__func__, md_cd_plat_val_ptr.md_first_power_on);
		CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF]%s:md_first_power_on=%u,exit\n",
			__func__, md_cd_plat_val_ptr.md_first_power_on);
		return 0;
	}

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF] %s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF] %s start\n", __func__);

	//reg = ioremap_wc(0x1C803000, 0x1000);  // Lepin
	reg = ioremap_wc(0x12830000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(
			md->index, TAG,
			"[POWER OFF] %s:ioremap 0x1000 bytes from 0x12830000 fail\n",
			__func__);
		return -1;
	}

	/* enable sequencer */
	ccci_write32(reg, 0x204, 0x1);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF] %s enable sequencer done\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF] %s enable sequencer done\n", __func__);

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x4040080)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(
				md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x12830200=0x%x,0x12830204=0x%x,0x12830208=0x%x,0x12830310=0x%x\n",
				ccci_read32(reg, 0x200),
				ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208),
				ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	iounmap(reg);
	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF] %s end\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF] %s end\n", __func__);

	return 0;
}

static int md1_disable_sequencer_setting(struct ccci_modem *md)
{
	void __iomem *reg = NULL;
	int count = 0;

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON] %s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s start\n", __func__);

	//reg = ioremap_wc(0x1C803000, 0x1000);  // Lepin
	reg = ioremap_wc(0x12830000, 0x1000);
	if (reg == NULL) {
		CCCI_ERROR_LOG(
			md->index, TAG,
			"[POWER ON] %s:ioremap 0x100 bytes from 0x12830000 fail\n",
			__func__);
		return -1;
	}

	/* disable sequencer */
	ccci_write32(reg, 0x204, 0x0);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] %s:disable sequencer done\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER ON] %s:disable sequencer done\n", __func__);

	/* retry 1000 * 1ms = 1s*/
	while (1) {
		/* wait sequencer done */
		if (ccci_read32(reg, 0x310) == 0x1010001)
			break;
		count++;
		udelay(1000);
		if (count == 1000) {
			CCCI_ERROR_LOG(
				md->index, TAG,
				"[POWER OFF] wait sequencer fail,0x12830200=0x%x,0x12830204=0x%x,0x12830208=0x%x,0x12830310=0x%x\n",
				ccci_read32(reg, 0x200),
				ccci_read32(reg, 0x204),
				ccci_read32(reg, 0x208),
				ccci_read32(reg, 0x310));
			iounmap(reg);
			return -2;
		}
	}

	iounmap(reg);
	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON] %s end\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON] %s end\n", __func__);
	return 0;
}

static int md_cd_power_on(struct ccci_modem *md)
{
	int ret = 0;

#if defined(CONFIG_MTK_ECCCI_FLASHLESS)
	/* md_first_power_on set 1 */
	md_cd_plat_val_ptr.md_first_power_on = 1;
	CCCI_NORMAL_LOG(md->index, TAG, "set md_fist_power_on = 1\n", __func__);
#endif

	/* step 1: PMIC setting */
	md1_pmic_setting_on();

	/* modem topclkgen on setting */
	ret = md_cd_topclkgen_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] md_cd_topclkgen_on fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] md_cd_topclkgen_on fail, ret=%d\n", ret);
		return ret;
	}

	/* step 2: MD srcclkena setting */
	ret = md_cd_srcclkena_setting(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] md_cd_srcclkena_setting fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] md_cd_srcclkena_setting fail, ret=%d\n", ret);
		return ret;
	}

	ret = mtk_ccci_cfg_srclken_o1_on(md);
	if (ret) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] mtk_ccci_cfg_srclken_o1_on fail, ret=%d\n", ret);
		CCCI_ERROR_LOG(md->index, TAG,
			"[POWER ON] mtk_ccci_cfg_srclken_o1_on fail, ret=%d\n", ret);
		return ret;
	}

	/* disable sequencer setting to AOC2.5 for gen98 */
	if (md_cd_plat_val_ptr.md_gen >= 6298) {
		ret = md1_disable_sequencer_setting(md);
		if (ret)
			return ret;
	}

	/* steip 3: power on MD_INFRA and MODEM_TOP */
	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON start\n");
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER ON] MD MTCMOS ON start\n");
	switch (md->index) {
	case MD_SYS1:
#ifdef USING_PM_RUNTIME
		pm_runtime_get_sync(&md->plat_dev->dev);
#else
		ret = clk_prepare_enable(clk_table[0].clk_ref);
#endif
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER ON] MD MTCMOS ON end: ret = %d\n", ret);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER ON] MD MTCMOS ON end: ret = %d\n", ret);
		break;
	}
#ifndef USING_PM_RUNTIME
	if (ret)
		return ret;
#endif

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 1, 0);
#endif

	/* md_first_power_on set 1 */
	md_cd_plat_val_ptr.md_first_power_on = 1;

	return 0;
}

static int md_cd_let_md_go(struct ccci_modem *md)
{
	struct arm_smccc_res res;

	if (MD_IN_DEBUG(md))
		return -1;
	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER ON]set MD boot slave\n");
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER ON]set MD boot slave\n");

	md_pccom_verifty(1);

	/* for boot time profiling */
	bootprof_log_boot("ungate Modem");
	printk("ungate Modem\n");

#if defined(CONFIG_MTK_ECCCI_FLASHLESS)
	/* config modem UART to loopback mode */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
			MD_UART_LOOPBACK, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"config md uart result:0x%lx\n", res.a0);
	CCCI_NORMAL_LOG(md->index, TAG,
			"config md uart result:0x%lx\n", res.a0);
#endif

	/* make boot vector take effect */
	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_POWER_CONFIG,
			MD_KERNEL_BOOT_UP, 0, 0, 0, 0, 0, &res);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"MD: boot_ret=%lu, boot_status_0=%lu, boot_status_1=%lu\n",
			res.a0, res.a1, res.a2);
	CCCI_NORMAL_LOG(md->index, TAG,
			"MD: boot_ret=%lu, boot_status_0=%lu, boot_status_1=%lu\n",
			res.a0, res.a1, res.a2);
	md_pccom_verifty(5);
	return 0;
}

static int md_cd_topclkgen_off(struct ccci_modem *md)
{
	unsigned int reg_value;

	if (md_cd_plat_val_ptr.power_flow_config & (1 << SKIP_TOPCLK_BIT)) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER OFF] bypass %s\n", __func__);
		return 0;
	}

	CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF]%s start\n", __func__);
	CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF]%s start\n", __func__);

	reg_value = ccci_read32(md->hw_info->ap_topclkgen_base, 0);
	reg_value |= ((1<<8) | (1<<9));
	ccci_write32(md->hw_info->ap_topclkgen_base, 0, reg_value);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"[POWER OFF]%s end: set md1_clk_mod = 0x%x\n",
		__func__, ccci_read32(md->hw_info->ap_topclkgen_base, 0));
	CCCI_NORMAL_LOG(md->index, TAG,
		"[POWER OFF]%s end: set md1_clk_mod = 0x%x\n",
		__func__, ccci_read32(md->hw_info->ap_topclkgen_base, 0));

	return 0;
}

static int md_cd_power_off(struct ccci_modem *md, unsigned int timeout)
{
	int ret = 0;
	unsigned int reg_value;

#ifdef FEATURE_INFORM_NFC_VSIM_CHANGE
	/* notify NFC */
	inform_nfc_vsim_change(md->index, 0, 0);
#endif

	switch (md->index) {
	case MD_SYS1:

		/* revert sequencer setting to AOC1.0 for gen98 */
		if (md_cd_plat_val_ptr.md_gen >= 6298) {
			ret = md1_revert_sequencer_setting(md);
			if (ret)
				return ret;
		}

		/* 1. power off MD MTCMOS */
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER OFF] MD MTCMOS OFF start\n");
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER OFF] MD MTCMOS OFF start\n");
#ifdef USING_PM_RUNTIME
		pm_runtime_put_sync(&md->plat_dev->dev);
		CCCI_BOOTUP_LOG(md->index, TAG, "power off md1\n");
#else
		clk_disable_unprepare(clk_table[0].clk_ref);
		CCCI_BOOTUP_LOG(md->index, TAG, "CCF:disable md1 clk\n");
#endif
		CCCI_BOOTUP_LOG(md->index, TAG,
			"[POWER OFF] MD MTCMOS OFF end: ret = %d\n", ret);
		CCCI_NORMAL_LOG(md->index, TAG,
			"[POWER OFF] MD MTCMOS OFF end: ret = %d\n", ret);

		/* 2. disable srcclkena */
		if (md_cd_plat_val_ptr.md_gen == 6297 &&
			(md_cd_plat_val_ptr.power_flow_config & (1 << SRCCLKENA_SETTING_BIT))) {
			CCCI_BOOTUP_LOG(md->index, TAG, "[POWER OFF] disable srcclkena start\n");
			CCCI_NORMAL_LOG(md->index, TAG, "[POWER OFF] disable srcclkena start\n");
			reg_value =
				regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_AO_MD_SRCCLKENA, &reg_value);
			reg_value &= ~(0xFF);
			regmap_write(md->hw_info->plat_val->infra_ao_base,
				INFRA_AO_MD_SRCCLKENA, reg_value);
			CCCI_BOOTUP_LOG(md->index, CORE,
				"[POWER OFF]%s: set md1_srcclkena=0x%x\n", __func__,
				regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_AO_MD_SRCCLKENA, &reg_value));
			CCCI_NORMAL_LOG(md->index, CORE,
				"[POWER OFF]%s: set md1_srcclkena=0x%x\n", __func__,
				regmap_read(md->hw_info->plat_val->infra_ao_base,
				INFRA_AO_MD_SRCCLKENA, &reg_value));
		}

		/* enable sequencer setting to AOC2.5 for gen98 */
		if (md_cd_plat_val_ptr.md_gen >= 6298) {
			ret = md1_enable_sequencer_setting(md);
			if (ret)
				return ret;
		}

		/* modem topclkgen off setting */
		md_cd_topclkgen_off(md);
		break;
	}
	return ret;
}

int md_cd_check_md_power_off(struct ccci_modem *md) {
	int ret = -1;  // return 0 for normal, -1 when timeout

	int cnt = 500; /*MD power off timeout is 5s*/
	int time_once = 10;
	static void __iomem *scpsys_base, *pwr_sta;
	unsigned int md_power_state;
	u32 val = 0;

	while (cnt > 0) {

			scpsys_base = ioremap(MD_SPM_BASE, PAGE_SIZE);
			pwr_sta = scpsys_base + MTCMOS_STA;
			val = readl(pwr_sta);
			CCCI_NORMAL_LOG(md->index, TAG,
					"SPM_BASE_MTCOMOS_STATE reg_value=0x%X\n", val);

			md_power_state =  val & MD_POWER_STATE_MASK;
			if (md_power_state) {
				CCCI_NORMAL_LOG(md->index, TAG,
						"poll md power state=0x%X reg=0x%X cnt:%d\n",
						md_power_state, val, cnt);
			} else {
				ret = 0;
				break;
			}
			msleep(time_once);
			cnt--;
	}
	return ret;
}

static int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

static void ccci_modem_shutdown(struct platform_device *dev)
{
}

static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_resume(struct platform_device *dev)
{
	struct ccci_modem *md = (struct ccci_modem *)dev->dev.platform_data;

	CCCI_DEBUG_LOG(md->index, TAG, "%s\n", __func__);
	return 0;
}

static int ccci_modem_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

static int ccci_modem_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	if (pdev == NULL) {
		CCCI_ERROR_LOG(MD_SYS1, TAG, "%s pdev == NULL\n", __func__);
		return -1;
	}
	return ccci_modem_resume(pdev);
}

static int ccci_modem_pm_restore_noirq(struct device *device)
{
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;

	/* set flag for next md_start */
	md->per_md_data.config.setting |= MD_SETTING_RELOAD;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	return 0;
}

#include <linux/module.h>
#include <linux/platform_device.h>

static int ccci_modem_probe(struct platform_device *plat_dev)
{
	struct ccci_dev_cfg dev_cfg;
	int ret;
	struct md_hw_info *md_hw;

	/* Allocate modem hardware info structure memory */
	md_hw = kzalloc(sizeof(struct md_hw_info), GFP_KERNEL);
	if (md_hw == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:alloc md hw mem fail\n", __func__);
		return -1;
	}
	ret = md_cd_get_modem_hw_info(plat_dev, &dev_cfg, md_hw);
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s:get hw info fail(%d)\n", __func__, ret);
		kfree(md_hw);
		md_hw = NULL;
		return -1;
	}
#ifdef CCCI_KMODULE_ENABLE
	ccci_init();
#endif
	ret = ccci_modem_init_common(plat_dev, &dev_cfg, md_hw);
	if (ret < 0) {
		kfree(md_hw);
		md_hw = NULL;
	}
	return ret;
}

static const struct dev_pm_ops ccci_modem_pm_ops = {
	.suspend = ccci_modem_pm_suspend,
	.resume = ccci_modem_pm_resume,
	.freeze = ccci_modem_pm_suspend,
	.thaw = ccci_modem_pm_resume,
	.poweroff = ccci_modem_pm_suspend,
	.restore = ccci_modem_pm_resume,
	.restore_noirq = ccci_modem_pm_restore_noirq,
};

#ifdef CONFIG_OF
static const struct of_device_id ccci_modem_of_ids[] = {
	{.compatible = "mediatek,mddriver",},
	{}
};
#endif


/* Linux 6.18 platform_driver::remove returns void. */
static void ccci_modem_remove_stage5k(struct platform_device *pdev)
{
	(void)ccci_modem_remove(pdev);
}

static struct platform_driver ccci_modem_driver = {

	.driver = {
		   .name = "driver_modem",
#ifdef CONFIG_OF
		   .of_match_table = ccci_modem_of_ids,
#endif

#ifdef CONFIG_PM
		   .pm = &ccci_modem_pm_ops,
#endif
		   },
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove_stage5k,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static int __init modem_cd_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"clmda modem platform driver register fail(%d)\n",
			ret);
		return ret;
	}
	return 0;
}

module_init(modem_cd_init);

MODULE_AUTHOR("CCCI");
MODULE_DESCRIPTION("CCCI modem driver v0.1");
MODULE_LICENSE("GPL");
