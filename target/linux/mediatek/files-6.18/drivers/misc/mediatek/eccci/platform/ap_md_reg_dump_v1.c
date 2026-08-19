/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include "ccci_core.h"
#include "ccci_platform.h"

#include "md_sys1_platform.h"
#include "modem_reg_base.h"
#include "modem_secure_base.h"
#include "ap_md_reg_dump_v1.h"
#include <linux/soc/mediatek/mtk_sip_svc.h> /* LG6851F Stage5K: MediaTek SIP command encoding */

/* LG6851F Stage5K: verified vendor CCCI SIP ABI. */
#ifndef MTK_SIP_KERNEL_CCCI_CONTROL
#define MTK_SIP_KERNEL_CCCI_CONTROL MTK_SIP_SMC_CMD(0x505)
#endif

#define TAG "mcd"

#define RAnd2W(a, b, c)  ccci_write32(a, b, (ccci_read32(a, b)&c))

static size_t mdreg_write32(size_t reg_id, size_t value)
{
	struct arm_smccc_res res;

	arm_smccc_smc(MTK_SIP_KERNEL_CCCI_CONTROL, MD_DBGSYS_REG_DUMP, reg_id,
			value, 0, 0, 0, 0, &res);

	return res.a0;
}

static unsigned int ioremap_dump_flag=1;

/*
 * This file is generated.
 * From 20220930_MT6980_MDReg_remap.xlsx
 * With ap_md_reg_dump_code_gentool.py v0.1
 * Date 2022-09-30 12:33:40.730566
 */
static void __iomem *AP_MDSRC_REQ;
static void __iomem *MD_DBG_SYS_TIMEOUT;
static void __iomem *PC_Monitor;
static void __iomem *PLL_reg_CLK_CTL;
static void __iomem *PLL_SPM;
static void __iomem *DCM;
static void __iomem *BUS;
static void __iomem *BUSMON;
static void __iomem *ECT;
static void __iomem *MD_RGU;
static void __iomem *TOPSM_OST;
static void __iomem *TOPSM_OST_AP;
static void __iomem *USIP;
static void __iomem *SONIC;
static void __iomem *DSP_related_bus;

static struct dump_reg_ioremap dump_reg_tab_v1[] = {
	{&AP_MDSRC_REQ,0x12001404, 0x4},
	{&MD_DBG_SYS_TIMEOUT,0x0D10111C, 0x4},
	{&PC_Monitor,0x0D130000, 0x21F0},
	{&PLL_reg_CLK_CTL,0x0D103800, 0xF704},
	{&PLL_SPM,0x12001514, 0x80},
	{&DCM,0x0D112700, 0x170},
	{&BUS,0x0D102000, 0xA5000},
	{&BUSMON,0x0D12C000, 0x12F24},
	{&ECT,0x0D101100, 0x4F18},
	{&MD_RGU,0x0D10E100, 0x25C},
	{&TOPSM_OST,0x0D10A000, 0x5224},
	{&TOPSM_OST_AP,0x12830000, 0x314},
	{&USIP,0x0D120400, 0xCD78},
	{&SONIC,0x0D15001C, 0x29FEC},
	{&DSP_related_bus,0x0D114000, 0x82108}
};

void md_io_remap_internal_dump_register(struct ccci_modem *md)
{
	unsigned int i;
	for(i = 0; i < ARRAY_SIZE(dump_reg_tab_v1); i++) {
		*dump_reg_tab_v1[i].dump_reg = ioremap(dump_reg_tab_v1[i].addr, dump_reg_tab_v1[i].size);
		if (*dump_reg_tab_v1[i].dump_reg == NULL) {
			/* ioremap fail, skip internal dump. */
			ioremap_dump_flag = 0;
			CCCI_MEM_LOG_TAG(0, TAG,
				"Dump MD failed to ioremap 0x%lu bytes from 0x%llu\n",
				dump_reg_tab_v1[i].size, dump_reg_tab_v1[i].addr);
			CCCI_MEM_LOG_TAG(0, TAG,
				"MD ioremap fail, skip internal dump.ioremap_dump_flag:%u\n",
				ioremap_dump_flag);
			return;
		}
	}
}
void internal_md_dump_debug_register_v1(unsigned int md_index)
{
	CCCI_ERROR_LOG(-1,TAG,
			"internal_md_dump_debug_register_v1 + \n");
	/* ioremap reg from dump_reg_tab,check ioremap result */
	if (ioremap_dump_flag == 0) {
		CCCI_MEM_LOG_TAG(md_index, TAG,
			"ioremap_dump_flag=%u, skip %s\n",
			ioremap_dump_flag, __func__);
		return;
	}

	/* module_in_order */

	/* AP_MDSRC_REQ */
	/* 0x1C00_1404 - 0x1C00_1407 */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"md_dbg_sys: 0x%X\n", ccci_read32(AP_MDSRC_REQ, 0x0));

	/* MD_DBG_SYS_TIMEOUT */
	/* 0xA060_111C – 0xA060_111F */
	mdreg_write32(MD_REG_MD_DBG_SYS_TIMEOUT_ADDR, 0x8000C350); /* addr 0xD10111C */
	/* 0xA060_111C – 0xA060_111F */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"md_dbg_sys time out: 0x%X\n", ccci_read32(MD_DBG_SYS_TIMEOUT, 0x0));

	/* PC_Monitor */
	/* Stop PCMon */
	mdreg_write32(MD_REG_PC_MONITOR_ADDR, 0x2222); /* addr 0xD131C00 */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD PC monitor\n");
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"common: 0x0D131C00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00001C00), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00001D00), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00001E00), 0xB0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00001F00), 0xB0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00002000), 0xB0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00002100), 0xB0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x000021B0), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"core0/1/2/3: [0]0x0D130000, [1]0x0D130700, [2]0x0D130E00, [3]0x0D131500\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00000000), 0x700);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00000700), 0x700);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00000E00), 0x700);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PC_Monitor + 0x00001500), 0x700);
	/* Re-Start PCMon */
	mdreg_write32(MD_REG_PC_MONITOR_ADDR, 0x1111); /* addr 0xD131C00 */

	/* PLL_reg_CLK_CTL */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD PLL\n");
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"GLOBAL CON: [0]0x0D111010, [1]0x0D111F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D810), 0xC0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000E700), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"CLKSW: [0]0x0D112000, [1]0x0D112200, [2]0x0D112300, [3]0x0D112400, [4]0x0D112500, [5]0x0D112600, [6]0x0D112700, [7]0x0D112800, [8]0x0D112900, [9]0x0D112E00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000E800), 0x1C8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000EA00), 0x14);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000EB00), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000EC00), 0x48);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000ED00), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000EE00), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000EF00), 0xC8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000F000), 0x6C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000F100), 0x90);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000F600), 0x104);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"PLLMIXED:[0]0x0D110000,[1]0x0D110130,[2]0x0D110160,[3]0x0D110300,[4]0x0D110400,[5]0x0D110500,[6]0x0D110800,[7]0x0D110C14,[8]0x0D110D00,[9]0x0D110E00,[10]0x0D110F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000C800), 0xA0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000C930), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000C960), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000CB00), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000CC00), 0xD0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000CD00), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D000), 0x50);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D414), 0x30);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D500), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D600), 0x34);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x0000D700), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"CLKCTL: [0]0x0D103800, [1]0x0D103910\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x00000000), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_reg_CLK_CTL + 0x00000110), 0x20);

	/* PLL_SPM */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"SPM_26M_TIMESTAMP: [0]0x12001514\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(PLL_SPM + 0x00000000), 0x80);

	/* DCM */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"CLKSW: [0]0x0D112700, [1]0x0D112800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DCM + 0x00000000), 0xD0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DCM + 0x00000100), 0x70);

	/* BUS */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD Bus status:\n");
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"mdperisys_misc_reg: [0]0x0D102000, [1]0x0D102400, [2]0x0D102600, [3]0x0D102700\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00000000), 0x90);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00000400), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00000600), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00000700), 0x110);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"shaolin_bus_config: [0]0x0D134000, mdmcu_corebus_intf[1]0x0D12A000, mdmcu_bus_intf[2]0x0D128000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00032000), 0x60);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00028000), 0xF0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00026000), 0x4C0);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"mdinfra_bus4x: [0]0x0D13C000, mdinfra_bus2x, mdperi_bus[1]0x0D13D000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0003A000), 0x430);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0003B000), 0x410);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"modeml1_ao_bus_intf: [0]0x0D148000, [1]0x0D149000, mmw_rxdfe_bus_ao: [2]0x0D1A4000, mmw_rf_ctrl_ao_bus_intf: [3]0x0D19C000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00046000), 0x260);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00047000), 0x240);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x000A2000), 0x2DC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0009A000), 0x12C);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"cs_nr_bus_intf: [0]0x0D183000, cmcs_bus_intf: [1]0x0D182000, cs_bus_intf: [2]0x0D14F000, [3]0x0D14E000, [4]0x0D14E800, dfesys_bus_intf: [5]0x0D198000, [6]0x0D19A000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00081000), 0x174);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00080000), 0xD8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0004D000), 0xDC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0004C000), 0x214);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0004C800), 0xE8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00096000), 0xA0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00098000), 0x158);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"mmw_rf_ctrl_bus_intf: [0]0x0D19D000, [1]0x0D19F000, [2]0x0D19F800, mmw_txdfe_bus_intf: [3]0x0D1A0000, [4]0x0D1A3000, mmw_rxdfe_bus_intf: [2]0x0D1A5000, [3]0x0D1A7000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0009B000), 0x3BC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0009D000), 0x0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0009D800), 0x160);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x0009E000), 0x1E4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x000A1000), 0x2E4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x000A3000), 0xB88);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x000A5000), 0x0);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"mdao_aoc_bus_ck_dbg: [0]0x0D149800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUS + 0x00047800), 0x1A4);

	/* BUSMON */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD Bus REC: [0]0x0D12C000, [1]0x0D13E000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000000), 0x108);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000200), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000220), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000280), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x000002A0), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000400), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000500), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000700), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000820), 0x4C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x000008FC), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000A00), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000B00), 0x424);
	/* [Pre-Action] Disable bus his rec & select entry 0 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x0); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 1 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x100010); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 2 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x200020); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 3 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x300030); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 4 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x400040); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 5 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x500050); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 6 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x600060); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	/* [Pre-Action] Select entry 7 */
	mdreg_write32(MD_REG_BUSMON_ADDR_0, 0x700070); /* addr 0xD12C408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00000860), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012000), 0x108);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012200), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012220), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012280), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x000122A0), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012400), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012500), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012700), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012820), 0x4C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x000128FC), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012A00), 0x1C);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012B00), 0x424);
	/* [Pre-Action] Disable bus his rec & select entry 0 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x0); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 1 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x100010); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 2 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x200020); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 3 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x300030); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 4 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x400040); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 5 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x500050); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 6 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x600060); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);
	/* [Pre-Action] Select entry 7 */
	mdreg_write32(MD_REG_BUSMON_ADDR_1, 0x700070); /* addr 0xD13E408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012830), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(BUSMON + 0x00012860), 0xC);

	/* ECT */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD ECT: [0]0x0D104130, [1]0x0D104134, [2]0x0D105130, [3]0x0D105134, [4]0x0D106014, [5]0x0D10600C, [6]0x0D101100\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00003030), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00003034), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00004030), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00004034), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00004F14), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00004F0C), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(ECT + 0x00000000), 0x10);

	/* MD_RGU */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD RGU: [0]0x0D10E100, [1]0x0D10E300\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(MD_RGU + 0x00000000), 0xE0);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(MD_RGU + 0x00000200), 0x5C);

	/* TOPSM_OST */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD TOPSM status: 0x0D10C000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00002000), 0x8E8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD OST status: [0]0x0D10D000, [1]0x0D10D200\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00003000), 0xF4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00003200), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD CSC: 0x0D10F000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00005000), 0x224);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD BUCK SWITCH: 0x0D10A000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00000000), 0x214);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump BUS PROTECT TOP: 0x0D10B300\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST + 0x00001300), 0x20);

	/* TOPSM_OST_AP */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump AP SEQUENCER:\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(TOPSM_OST_AP + 0x00000000), 0x314);

	/* USIP */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump MD USIP: [0]0x0D120400, [1]0x0D120610, [2]0x0D121400, [3]0x0D121610, [4]0x0D122000, [5]0x0D12D000, [6]0x0D122068\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00000000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00000210), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001210), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C00), 0x88);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x0000CC00), 0x178);
	/* [Pre-Action] config usip bus dbg sel 1 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x10000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 2 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x20000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 3 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x30000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 4 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x40000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 5 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x50000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 6 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x60000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 7 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x70000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);
	/* [Pre-Action] config usip bus dbg sel 8 */
	mdreg_write32(MD_REG_USIP_ADDR, 0x80000000); /* addr 0xD12200C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(USIP + 0x00001C68), 0x4);

	/* SONIC */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore dbus recorder: [0]0x0D177000, [1]0x0D177090, [2]0x0D177100\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026FE4), 0x24);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00027074), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000270E4), 0x200);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri perick abus: [0]0x0D172040\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00022024), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri dspcoreck abus: [0]0x0D170040\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00020024), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri dspck abus: [0]0x0D171040\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00021024), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri dbus1: [0]0x0D17A000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00029FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri dbus2: [0]0x0D175000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00024FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore peri dbus3: [0]0x0D174000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00023FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th0 reg: [0]0x0D16001C, [1]0x0D16006C, [2]0x0D1600E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010000), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010050), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000100CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th0 PC trace status reg: [0]0x0D160A20\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A04), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th0 PC trace: [0]0x0D160A40\n");
	/* [Pre-Action] mcore th0 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x0); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x1); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x2); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x3); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x4); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x5); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x6); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	/* [Pre-Action] mcore th0 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_0, 0x7); /* addr 0xD160A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A24), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th1 reg: [0]0x0D16061C, [1]0x0D16066C, [2]0x0D1606E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010600), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010650), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000106CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th1 PC trace status reg: [0]0x0D160AA0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010A84), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th1 PC trace: [0]0x0D160AC0\n");
	/* [Pre-Action] mcore th1 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x0); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x1); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x2); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x3); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x4); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x5); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x6); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	/* [Pre-Action] mcore th1 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_1, 0x7); /* addr 0xD160ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010AA4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th2 reg: [0]0x0D160C1C, [1]0x0D160C6C, [2]0x0D160CE8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010C00), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010C50), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010CCC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th2 PC trace status reg: [0]0x0D161620\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011604), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th2 PC trace: [0]0x0D161640\n");
	/* [Pre-Action] mcore th2 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x0); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x1); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x2); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x3); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x4); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x5); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x6); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	/* [Pre-Action] mcore th2 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_2, 0x7); /* addr 0xD16163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011624), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th3 reg: [0]0x0D16121C, [1]0x0D16126C, [2]0x0D1612E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011200), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011250), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000112CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th3 PC trace status reg: [0]0x0D1616A0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011684), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th3 PC trace: [0]0x0D1616C0\n");
	/* [Pre-Action] mcore th3 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x0); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x1); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x2); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x3); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x4); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x5); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x6); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	/* [Pre-Action] mcore th3 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_3, 0x7); /* addr 0xD1616BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000116A4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th4 reg: [0]0x0D16181C, [1]0x0D16186C, [2]0x0D1618E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011800), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011850), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000118CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th4 PC trace status reg: [0]0x0D162220\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012204), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th4 PC trace: [0]0x0D162240\n");
	/* [Pre-Action] mcore th4 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x0); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x1); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x2); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x3); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x4); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x5); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x6); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	/* [Pre-Action] mcore th4 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_4, 0x7); /* addr 0xD16223C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012224), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th5 reg: [0]0x0D161E1C, [1]0x0D161E6C, [2]0x0D161EE8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011E00), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011E50), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011ECC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th5 PC trace status reg: [0]0x0D1622A0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012284), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th5 PC trace: [0]0x0D1622C0\n");
	/* [Pre-Action] mcore th5 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x0); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x1); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x2); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x3); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x4); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x5); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x6); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	/* [Pre-Action] mcore th5 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_5, 0x7); /* addr 0xD1622BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000122A4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th6 reg: [0]0x0D16241C, [1]0x0D16246C, [2]0x0D1624E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012400), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012450), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000124CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th6 PC trace status reg: [0]0x0D162E20\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E04), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th6 PC trace: [0]0x0D162E40\n");
	/* [Pre-Action] mcore th6 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x0); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x1); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x2); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x3); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x4); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x5); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x6); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	/* [Pre-Action] mcore th6 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_6, 0x7); /* addr 0xD162E3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E24), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th7 reg: [0]0x0D162A1C, [1]0x0D162A6C, [2]0x0D162AE8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012A00), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012A50), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012ACC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th7 PC trace status reg: [0]0x0D162EA0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012E84), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th7 PC trace: [0]0x0D162EC0\n");
	/* [Pre-Action] mcore th7 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x0); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x1); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x2); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x3); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x4); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x5); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x6); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	/* [Pre-Action] mcore th7 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_7, 0x7); /* addr 0xD162EBC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00012EA4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th8 reg: [0]0x0D16301C, [1]0x0D16306C, [2]0x0D1630E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013000), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013050), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000130CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th8 PC trace status reg: [0]0x0D163A20\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A04), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th8 PC trace: [0]0x0D163A40\n");
	/* [Pre-Action] mcore th8 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x0); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x1); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x2); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x3); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x4); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x5); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x6); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	/* [Pre-Action] mcore th8 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_8, 0x7); /* addr 0xD163A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A24), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th9 reg: [0]0x0D16361C, [1]0x0D16366C, [2]0x0D1636E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013600), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013650), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000136CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th9 PC trace status reg: [0]0x0D163AA0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013A84), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th9 PC trace: [0]0x0D163AC0\n");
	/* [Pre-Action] mcore th9 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x0); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x1); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x2); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x3); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x4); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x5); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x6); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	/* [Pre-Action] mcore th9 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_9, 0x7); /* addr 0xD163ABC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013AA4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th10 reg: [0]0x0D163C1C, [1]0x0D163C6C, [2]0x0D163CE8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013C00), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013C50), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013CCC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th10 PC trace status reg: [0]0x0D164620\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014604), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th10 PC trace: [0]0x0D164640\n");
	/* [Pre-Action] mcore th10 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x0); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x1); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x2); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x3); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x4); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x5); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x6); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	/* [Pre-Action] mcore th10 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_10, 0x7); /* addr 0xD16463C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014624), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th11 reg: [0]0x0D16421C, [1]0x0D16426C, [2]0x0D1642E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014200), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014250), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000142CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th11 PC trace status reg: [0]0x0D1646A0\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00014684), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore th11 PC trace: [0]0x0D1646C0\n");
	/* [Pre-Action] mcore th11 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x0); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x1); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x2); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x3); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x4); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x5); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x6); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	/* [Pre-Action] mcore th11 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_11, 0x7); /* addr 0xD1646BC */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000146A4), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit0 dbus: [0]0x0D160400\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000103E4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit1 dbus: [0]0x0D161000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00010FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit2 dbus: [0]0x0D161C00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00011BE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit3 dbus: [0]0x0D162800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000127E4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit4 dbus: [0]0x0D163400\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000133E4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore crit5 dbus: [0]0x0D164000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00013FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore grp0 dbus: [0]0x0D166800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000167E4), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore grp1 dbus: [0]0x0D166A00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000169E4), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore grp0 dbus: [0]0x0D166E00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00016DE4), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore grp1 dbus: [0]0x0D166F00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00016EE4), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore L1 dbus: [0]0x0D166000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00015FE4), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore A2D32: [0]0x0D167400\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000173E4), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore A2D128: [0]0x0D167480\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00017464), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore A2D peri: [0]0x0D178000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00027FE4), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump mcore CTI reg: [0]0x0D173000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00022FE4), 0x14);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore0 dbus recorder: [0]0x0D157000, [1]0x0D157090, [2]0x0D157100\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00006FE4), 0x24);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00007074), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000070E4), 0x200);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore peri perick abus: [0]0x0D155840\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00005824), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore peri dspcoreck abus: [0]0x0D155040\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00005024), 0x20);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore peri dbus1: [0]0x0D156000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00005FE4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore peri dbus2: [0]0x0D156800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000067E4), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th0 reg: [0]0x0D15001C, [1]0x0D15006C, [2]0x0D1500E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000000), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000050), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000000CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th0 PC trace status reg: [0]0x0D150420\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000404), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th0 PC trace: [0]0x0D150440\n");
	/* [Pre-Action] vcore th0 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x0); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x1); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x2); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x3); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x4); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x5); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x6); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	/* [Pre-Action] vcore th0 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_12, 0x7); /* addr 0xD15043C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000424), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th1 reg: [0]0x0D15061C, [1]0x0D15066C, [2]0x0D1506E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000600), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000650), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000006CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th1 PC trace status reg: [0]0x0D150A20\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A04), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th1 PC trace: [0]0x0D150A40\n");
	/* [Pre-Action] vcore th1 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x0); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x1); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x2); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x3); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x4); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x5); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x6); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	/* [Pre-Action] vcore th1 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_13, 0x7); /* addr 0xD150A3C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A24), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th2 reg: [0]0x0D150C1C, [1]0x0D150C6C, [2]0x0D150CE8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000C00), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000C50), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000CCC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th2 PC trace status reg: [0]0x0D151020\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001004), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th2 PC trace: [0]0x0D151040\n");
	/* [Pre-Action] vcore th2 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x0); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x1); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x2); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x3); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x4); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x5); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x6); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	/* [Pre-Action] vcore th2 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_14, 0x7); /* addr 0xD15103C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001024), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th3 reg: [0]0x0D15121C, [1]0x0D15126C, [2]0x0D1512E8\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001200), 0x4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001250), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000012CC), 0xC);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th3 PC trace status reg: [0]0x0D151620\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001604), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th3 PC trace: [0]0x0D151640\n");
	/* [Pre-Action] vcore th3 PC sel 0 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x0); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 1 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x1); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 2 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x2); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 3 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x3); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 4 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x4); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 5 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x5); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 6 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x6); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	/* [Pre-Action] vcore th3 PC sel 7 (dbg apb) */
	mdreg_write32(MD_REG_SONIC_ADDR_15, 0x7); /* addr 0xD15163C */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001624), 0x40);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore L1 dbus: [0]0x0D153000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00002FE4), 0x18);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore A2D32: [0]0x0D154400\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000043E4), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore A2D128: [0]0x0D154480\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00004464), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore A2D peri: [0]0x0D157800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000077E4), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore CTI reg: [0]0x0D157900\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000078E4), 0x14);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th0 dbus: [0]0x0D150480\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000464), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th1 dbus: [0]0x0D150A80\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00000A64), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th2 dbus: [0]0x0D151080\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001064), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump vcore th3 dbus: [0]0x0D151680\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00001664), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump Mcoresys Bus REC: [0]0x0D176000, [1]0x0D176D00\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00025FE4), 0x108);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000261E4), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026204), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026264), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026284), 0x38);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000263E4), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000264E4), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000266E4), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026804), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026834), 0x10);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000268E0), 0x20);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x000269E4), 0x8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026AE4), 0x200);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026CE4), 0x200);
	/* [Pre-Action] Disable bus his rec & select entry 0 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x0); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 1 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x100010); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 2 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x200020); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 3 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x300030); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 4 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x400040); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 5 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x500050); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 6 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x600060); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);
	/* [Pre-Action] Select entry 7 */
	mdreg_write32(MD_REG_SONIC_ADDR_16, 0x700070); /* addr 0xD176408 */
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026814), 0xC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(SONIC + 0x00026844), 0xC);

	/* DSP_related_bus */
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump rxddm bus: [0]0x0D144000, [1]0x0D145000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00030000), 0x148);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00031000), 0xF0);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump cmcs nr bus: [0]0x0D180000, [1]0x0D181000, [3]0x0D182000, [4]0x0D185000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0006C000), 0x1D4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0006D000), 0x298);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0006E000), 0xDC);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00071000), 0x114);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump tx nr bus: [0]0x0D190000, [1]0x0D191000, [2]0x0D192000, , [3]0x0D192800,[4]0x0D193000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0007C000), 0x100);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0007D000), 0xF8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0007E000), 0x200);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0007E800), 0x34);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0007F000), 0xF0);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump rake bus: [0]0x0D114000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00000000), 0x28);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump bigram sys bus: [0]0x0D11C000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00008000), 0x8);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump inr sys bus: [0]0x0D11E000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0000A000), 0x4);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump rxcpc nr bus: [0]0x0D142000, [1]0x0D142800, [2]0x0D143000, [3]0x0D143800\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0002E000), 0xE4);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0002E800), 0xF8);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0002F000), 0x160);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x0002F800), 0xF0);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump rxdbrp bus: [0]0x0D146000, [1]0x0D147000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00032000), 0x190);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00033000), 0x128);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump rxbrp bus: [0]0x0D194000, [1]0x0D196000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00080000), 0x190);
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00082000), 0x108);
	CCCI_MEM_LOG_TAG(md_index, TAG,
		"Dump gram sys bus: [0]0x0D158000\n");
	ccci_util_mem_dump(md_index, CCCI_DUMP_MEM_DUMP,
		(DSP_related_bus + 0x00044000), 0x4);

	CCCI_ERROR_LOG(-1,TAG,
			"internal_md_dump_debug_register_v1 - \n");
}

void md_dump_register_6980(unsigned int md_index)
{
	CCCI_ERROR_LOG(-1, TAG,
		 "md_dump_register_6980 + \n");

	internal_md_dump_debug_register_v1(md_index);
}
