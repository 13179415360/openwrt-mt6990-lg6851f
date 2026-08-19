// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Pei-hsuan Cheng <pei-hsuan.cheng@mediatek.com>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_domain.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/seq_file.h>

#include "clk-mtk.h"
#include "clk-pll.h"
#include "clk-mux.h"

/*
 * MT6990 SDK compatibility:
 * Vendor MUX_HWV has 14 arguments and represents a mux gate controlled
 * by HW voter set/clr/status registers, but without FENC monitor fields.
 */
#ifndef MUX_HWV
#define MUX_HWV(_id, _name, _parents,					\
		_mux_ofs, _mux_set_ofs, _mux_clr_ofs,			\
		_hwv_sta_ofs, _hwv_set_ofs, _hwv_clr_ofs,		\
		_shift, _width, _gate, _upd_ofs, _upd)			\
	MUX_GATE_HWV_CLR_SET_UPD(_id, _name, _parents,			\
		_mux_ofs, _mux_set_ofs, _mux_clr_ofs,			\
		_hwv_sta_ofs, _hwv_set_ofs, _hwv_clr_ofs,		\
		_shift, _width, _gate, _upd_ofs, _upd)
#endif

#include "clk-gate.h"
#include "clkdbg.h"

#include <dt-bindings/clock/mt6990-clk.h>

/* bringup config */
#define MT_CCF_BRINGUP		0
#define MT_CCF_PLL_DISABLE	0
#define MT_CCF_MUX_DISABLE	0

/* Regular Number Definition */
#define INV_OFS	-1
#define INV_BIT	-1

/* TOPCK MUX SEL REG */
#define CLK_CFG_UPDATE				0x0004
#define CLK_CFG_UPDATE1				0x0008
#define CLK_CFG_UPDATE2				0x000C
#define VLP_CLK_CFG_UPDATE			0x0004
#define CLK_CFG_0				0x0010
#define CLK_CFG_0_SET				0x0014
#define CLK_CFG_0_CLR				0x0018
#define CLK_CFG_1				0x0020
#define CLK_CFG_1_SET				0x0024
#define CLK_CFG_1_CLR				0x0028
#define CLK_CFG_2				0x0030
#define CLK_CFG_2_SET				0x0034
#define CLK_CFG_2_CLR				0x0038
#define CLK_CFG_3				0x0040
#define CLK_CFG_3_SET				0x0044
#define CLK_CFG_3_CLR				0x0048
#define CLK_CFG_4				0x0050
#define CLK_CFG_4_SET				0x0054
#define CLK_CFG_4_CLR				0x0058
#define CLK_CFG_5				0x0060
#define CLK_CFG_5_SET				0x0064
#define CLK_CFG_5_CLR				0x0068
#define CLK_CFG_6				0x0070
#define CLK_CFG_6_SET				0x0074
#define CLK_CFG_6_CLR				0x0078
#define CLK_CFG_7				0x0080
#define CLK_CFG_7_SET				0x0084
#define CLK_CFG_7_CLR				0x0088
#define CLK_CFG_8				0x0090
#define CLK_CFG_8_SET				0x0094
#define CLK_CFG_8_CLR				0x0098
#define CLK_CFG_9				0x00A0
#define CLK_CFG_9_SET				0x00A4
#define CLK_CFG_9_CLR				0x00A8
#define CLK_CFG_10				0x00B0
#define CLK_CFG_10_SET				0x00B4
#define CLK_CFG_10_CLR				0x00B8
#define CLK_CFG_11				0x00C0
#define CLK_CFG_11_SET				0x00C4
#define CLK_CFG_11_CLR				0x00C8
#define CLK_CFG_12				0x00D0
#define CLK_CFG_12_SET				0x00D4
#define CLK_CFG_12_CLR				0x00D8
#define CLK_CFG_13				0x00E0
#define CLK_CFG_13_SET				0x00E4
#define CLK_CFG_13_CLR				0x00E8
#define CLK_CFG_14				0x00F0
#define CLK_CFG_14_SET				0x00F4
#define CLK_CFG_14_CLR				0x00F8
#define CLK_CFG_15				0x0100
#define CLK_CFG_15_SET				0x0104
#define CLK_CFG_15_CLR				0x0108
#define CLK_AUDDIV_0				0x0320
#define VLP_CLK_CFG_0				0x0008
#define VLP_CLK_CFG_0_SET				0x000C
#define VLP_CLK_CFG_0_CLR				0x0010
#define VLP_CLK_CFG_1				0x0014
#define VLP_CLK_CFG_1_SET				0x0018
#define VLP_CLK_CFG_1_CLR				0x001C
#define VLP_CLK_CFG_2				0x0020
#define VLP_CLK_CFG_2_SET				0x0024
#define VLP_CLK_CFG_2_CLR				0x0028
#define VLP_CLK_CFG_3				0x002C
#define VLP_CLK_CFG_3_SET				0x0030
#define VLP_CLK_CFG_3_CLR				0x0034
#define VLP_CLK_CFG_5				0x0044
#define VLP_CLK_CFG_5_SET				0x0048
#define VLP_CLK_CFG_5_CLR				0x004C

/* TOPCK MUX SHIFT */
#define TOP_MUX_AXI_SHIFT			0
#define TOP_MUX_PERI_HD_HAXI_SHIFT		1
#define TOP_MUX_PERI_HD_FAXI_SHIFT		2
#define TOP_MUX_BUS_AXIMEM_SHIFT		3
#define TOP_MUX_MEM_SUB_SHIFT			4
#define TOP_MUX_DVFSRC_SHIFT			5
#define TOP_MUX_MM_SHIFT			6
#define TOP_MUX_DBI_SHIFT			7
#define TOP_MUX_DISP_PWM_SHIFT			8
#define TOP_MUX_MFG_REF_SHIFT			9
#define TOP_MUX_UART_SHIFT			10
#define TOP_MUX_MSDC50_0_HCLK_SHIFT		11
#define TOP_MUX_MSDC50_0_SHIFT			12
#define TOP_MUX_MSDC30_1_SHIFT			13
#define TOP_MUX_MSDC_MACRO_SHIFT		14
#define TOP_MUX_AUDIO_SHIFT			15
#define TOP_MUX_AUD_INTBUS_SHIFT		16
#define TOP_MUX_AUD_ENGEN1_SHIFT		17
#define TOP_MUX_AUD_ENGEN2_SHIFT		18
#define TOP_MUX_AUD_1_SHIFT			19
#define TOP_MUX_AUD_2_SHIFT			20
#define TOP_MUX_ATB_SHIFT			21
#define TOP_MUX_I2C_SHIFT			22
#define TOP_MUX_TL_SHIFT			23
#define TOP_MUX_DPMAIF_MAIN_SHIFT		24
#define TOP_MUX_MCUPM_SHIFT			25
#define TOP_MUX_SFLASH_SHIFT			26
#define TOP_MUX_SPI_SHIFT			27
#define TOP_MUX_SPIS_SHIFT			28
#define TOP_MUX_NFI1X_SHIFT			29
#define TOP_MUX_SPINFI_BCLK_SHIFT		30
#define TOP_MUX_GCPU_SHIFT			0
#define TOP_MUX_ECC_SHIFT			1
#define TOP_MUX_HSM_CRYPTO_SHIFT		2
#define TOP_MUX_HSM_ARC_SHIFT			3
#define TOP_MUX_EIP97_SHIFT			4
#define TOP_MUX_SNPS_ETH_312P5M_SHIFT		5
#define TOP_MUX_SNPS_ETH_250M_SHIFT		6
#define TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT	7
#define TOP_MUX_SNPS_ETH_50M_RMII_SHIFT		8
#define TOP_MUX_NETSYS_500M_SHIFT		9
#define TOP_MUX_NETSYS_SHIFT			10
#define TOP_MUX_NETSYS_2X_SHIFT			11
#define TOP_MUX_NETSYS_WED_MCU_SHIFT		12
#define TOP_MUX_NETSYS_MED_MCU_SHIFT		13
#define TOP_MUX_SGMII_0_SHIFT			14
#define TOP_MUX_SGMII_SBUS_0_SHIFT		15
#define TOP_MUX_SGMII_1_SHIFT			16
#define TOP_MUX_SGMII_SBUS_1_SHIFT		17
#define TOP_MUX_USXGMII_SBUS_0_SHIFT		18
#define TOP_MUX_USXGMII_SBUS_1_SHIFT		19
#define TOP_MUX_AP2CONN_HOST_SHIFT		20
#define TOP_MUX_USB_TOP_SHIFT			21
#define TOP_MUX_SSUSB_XHCI_SHIFT		22
#define TOP_MUX_EMI_N_SHIFT			23
#define TOP_MUX_HSM_HSAH_SHIFT			24
#define TOP_MUX_RSA_SHIFT			25
#define TOP_MUX_MSDC_OCC_400_SHIFT		26
#define TOP_MUX_MSDC_OCC_200_SHIFT		27
#define TOP_MUX_DXCC_SHIFT			28
#define TOP_MUX_PCIE_250M_SHIFT			29
#define TOP_MUX_DSI_OCC_SHIFT			30
#define TOP_MUX_EMI_INTERFACE_546_SHIFT		0
#define TOP_MUX_EMI_INTERFACE_624_SHIFT		1
#define TOP_MUX_SCP_SHIFT			0
#define TOP_MUX_PWRAP_ULPOSC_SHIFT		1
#define TOP_MUX_VLP_DXCC_SHIFT			3
#define TOP_MUX_SPMI_P_MST_SHIFT		4
#define TOP_MUX_SPMI_M_MST_SHIFT		5
#define TOP_MUX_VLP_DVFSRC_SHIFT		6
#define TOP_MUX_PWM_VLP_SHIFT			7
#define TOP_MUX_AXI_VLP_SHIFT			8
#define TOP_MUX_DBGAO_26M_SHIFT			9
#define TOP_MUX_SYSTIMER_26M_SHIFT		10
#define TOP_MUX_PWRMCU_SHIFT			11
#define TOP_MUX_SSPM_F26M_SHIFT			12
#define TOP_MUX_APEINT_66M_SHIFT		13
#define TOP_MUX_SRCK_SHIFT			14
#define TOP_MUX_SRAMRC_SHIFT			15
#define TOP_MUX_TL_VLP_SHIFT			18
#define TOP_MUX_VLP_HSM_CRYPTO_SHIFT		19
#define TOP_MUX_VLP_HSM_ARC_SHIFT		20

/* TOPCK DIVIDER REG */
#define CLK_AUDDIV_2				0x0328
#define CLK_AUDDIV_3				0x0334

/* APMIXED PLL REG */
#define AP_PLL_CON0				0x000
#define APLL1_TUNER_CON0			0x040
#define APLL2_TUNER_CON0			0x054
#define ARMPLL_LL_CON0				0x2EC
#define ARMPLL_LL_CON1				0x2F0
#define ARMPLL_LL_CON2				0x2F4
#define ARMPLL_LL_CON3				0x2F8
#define ARMPLL_LL_CON4				0x2FC
#define CCIPLL_CON0				0x328
#define CCIPLL_CON1				0x32C
#define CCIPLL_CON2				0x330
#define CCIPLL_CON3				0x334
#define CCIPLL_CON4				0x338
#define MAINPLL_CON0				0x244
#define MAINPLL_CON1				0x248
#define MAINPLL_CON2				0x24C
#define MAINPLL_CON3				0x250
#define MAINPLL_CON4				0x254
#define UNIVPLL_CON0				0x258
#define UNIVPLL_CON1				0x25C
#define UNIVPLL_CON2				0x260
#define UNIVPLL_CON3				0x264
#define UNIVPLL_CON4				0x268
#define MSDCPLL_CON0				0x208
#define MSDCPLL_CON1				0x20C
#define MSDCPLL_CON2				0x210
#define MSDCPLL_CON3				0x214
#define MSDCPLL_CON4				0x218
#define MMPLL_CON0				0x26C
#define MMPLL_CON1				0x270
#define MMPLL_CON2				0x274
#define MMPLL_CON3				0x278
#define MMPLL_CON4				0x27C
#define APLL1_CON0				0x294
#define APLL1_CON1				0x298
#define APLL1_CON2				0x29C
#define APLL1_CON3				0x2A0
#define APLL1_CON4				0x2A4
#define APLL1_CON5				0x2A8
#define APLL2_CON0				0x2AC
#define APLL2_CON1				0x2B0
#define APLL2_CON2				0x2B4
#define APLL2_CON3				0x2B8
#define APLL2_CON4				0x2BC
#define APLL2_CON5				0x2C0
#define MPLL_CON0				0x2C4
#define MPLL_CON1				0x2C8
#define MPLL_CON2				0x2CC
#define MPLL_CON3				0x2D0
#define MPLL_CON4				0x2D4
#define MFGPLL_CON0				0x230
#define MFGPLL_CON1				0x234
#define MFGPLL_CON2				0x238
#define MFGPLL_CON3				0x23C
#define MFGPLL_CON4				0x240
#define NET1PLL_CON0				0x280
#define NET1PLL_CON1				0x284
#define NET1PLL_CON2				0x288
#define NET1PLL_CON3				0x28C
#define NET1PLL_CON4				0x290
#define NET2PLL_CON0				0x2D8
#define NET2PLL_CON1				0x2DC
#define NET2PLL_CON2				0x2E0
#define NET2PLL_CON3				0x2E4
#define NET2PLL_CON4				0x2E8
#define WEDMCUPLL_CON0				0x300
#define WEDMCUPLL_CON1				0x304
#define WEDMCUPLL_CON2				0x308
#define WEDMCUPLL_CON3				0x30C
#define WEDMCUPLL_CON4				0x310
#define MEDMCUPLL_CON0				0x314
#define MEDMCUPLL_CON1				0x318
#define MEDMCUPLL_CON2				0x31C
#define MEDMCUPLL_CON3				0x320
#define MEDMCUPLL_CON4				0x324
#define SGMIIPLL_CON0				0x33C
#define SGMIIPLL_CON1				0x340
#define SGMIIPLL_CON2				0x344
#define SGMIIPLL_CON3				0x348
#define SGMIIPLL_CON4				0x34C

static DEFINE_SPINLOCK(mt6990_clk_lock);

static void __iomem *apmixed_base;

/* hw voter */
#define HWV_PLL_SET				0x590
#define HWV_PLL_CLR				0x594
#define HWV_PLL_EN				0x1400
#define HWV_PLL_DONE				0x140C
#define HWV_PLL_SET_STA				0x1464
#define HWV_PLL_CLR_STA				0x1468
#define HWV_CLK_CFG_1_SET			0x410
#define HWV_CLK_CFG_1_CLR			0x414
#define HWV_CLK_CFG_1_STA			0x1C08

static const struct mtk_fixed_factor vlp_ck_divs[] = {
/*	FACTOR(CLK_VLP_CK_SCP, "vlp_scp_ck",
			"vlp_scp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWRAP_ULPOSC, "vlp_pwrap_ulposc_ck",
			"vlp_pwrap_ulposc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_APXGPT66M_BCLK, "vlp_gpt_bclk_ck",
			"vlp_gpt_bclk_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DXCC, "vlp_dxcc_ck",
			"vlp_dxcc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST, "vlp_spmi_p_ck",
			"vlp_spmi_p_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_M_MST, "vlp_spmi_m_ck",
			"vlp_spmi_m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DVFSRC, "vlp_dvfsrc_ck",
			"vlp_dvfsrc_sel", 1, 1),*/
	FACTOR(CLK_VLP_CK_PWM_VLP, "vlp_pwm_vlp_ck",
			"vlp_pwm_vlp_sel", 1, 1),
/*	FACTOR(CLK_VLP_CK_AXI_VLP, "vlp_axi_vlp_ck",
			"vlp_axi_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_DBGAO_26M, "vlp_dbgao_26m_ck",
			"vlp_dbgao_26m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SYSTIMER_26M, "vlp_systimer_26m_ck",
			"vlp_systimer_26m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_PWRMCU, "vlp_pwrmcu_ck",
			"vlp_pwrmcu_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SSPM_F26M, "vlp_sspm_f26m_ck",
			"vlp_sspm_f26m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_APEINT_66M, "vlp_apeint_66m_ck",
			"vlp_apeint_66m_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SRCK, "vlp_srck_ck",
			"vlp_srck_sel", 1, 1),
	FACTOR(CLK_VLP_CK_SRAMRC, "vlp_sramrc_ck",
			"vlp_sramrc_sel", 1, 1),
	FACTOR(CLK_VLP_CK_TL_VLP, "vlp_tl_vlp_ck",
			"vlp_tl_vlp_sel", 1, 1),
	FACTOR(CLK_VLP_CK_HSM_CRYPTO, "vlp_hsm_crypto_ck",
			"vlp_hsm_crypto_sel", 1, 1),
	FACTOR(CLK_VLP_CK_HSM_ARC, "vlp_hsm_arc_ck",
			"vlp_hsm_arc_sel", 1, 1),*/
	FACTOR(CLK_VLP_CK_OUT_26M, "vlp_out_26m_ck",
			"out_26m_ck_sel", 1, 1),
	FACTOR(CLK_VLP_CK_RGU_26M, "vlp_rgu_26m_ck",
			"rgu_26m_ck_sel", 1, 1),
	/*FACTOR(CLK_VLP_CK_SPM, "vlp_spm_ck",
			"mainpll_d7_d4", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_M_MST_32K, "vlp_spmi_m_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_M_TIA_32K, "vlp_spmi_m_tia_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_MST_32K, "vlp_spmi_p_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SPMI_P_TIA_32K, "vlp_spmi_p_tia_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_MD_OSC26M_VLP, "vlp_md_osc26m_vlp_ck",
			"osc_d10", 1, 1),
	FACTOR(CLK_VLP_CK_CLDMA_AO, "vlp_cldma_ao_ck",
			"mainpll_d7_d2", 1, 1),
	FACTOR(CLK_VLP_CK_MHCCIF_SLOW, "vlp_mhccif_slow_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_MHCCIF, "vlp_mhccif_ck",
			"hf_faxi_test_ck", 1, 1),*/
	FACTOR(CLK_VLP_CK_PCIE_OSC26M_VLP, "vlp_pcie_26m_ck",
			"osc_d10", 1, 1),
/*	FACTOR(CLK_VLP_CK_MD_BUCK_CTRL_OSC26M, "vlp_md_buck_26m_ck",
			"osc_d10", 1, 1),
	FACTOR(CLK_VLP_CK_SEJ_32K, "vlp_sej_32k_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_VLP_CK_SEJ_26M, "vlp_sej_26m_ck",
			"tck_26m_mx9_ck", 1, 1),*/
};

static const struct mtk_fixed_factor top_divs[] = {
	FACTOR(CLK_TOP_MFGPLL, "mfgpll_ck",
			"mfgpll", 1, 1),
	FACTOR(CLK_TOP_NET1PLL_D8, "net1pll_d8",
			"net1pll", 1, 8),
	FACTOR(CLK_TOP_NET1PLL_D5, "net1pll_d5",
			"net1pll", 1, 5),
	FACTOR(CLK_TOP_NET1PLL_D10, "net1pll_d10",
			"net1pll", 1, 10),
	FACTOR(CLK_TOP_NET1PLL_D50, "net1pll_d50",
			"net1pll", 1, 50),
	FACTOR(CLK_TOP_NET2PLL, "net2pll_ck",
			"net2pll", 1, 1),
	FACTOR(CLK_TOP_WEDMCUPLL, "wedmcupll_ck",
			"wedmcupll", 1, 1),
	FACTOR(CLK_TOP_MEDMCUPLL, "medmcupll_ck",
			"medmcupll", 1, 1),
	FACTOR(CLK_TOP_SGMIIPLL, "sgmiipll_ck",
			"sgmiipll", 1, 1),
	FACTOR(CLK_TOP_MAINPLL_D3, "mainpll_d3",
			"mainpll", 1, 3),
	FACTOR(CLK_TOP_MAINPLL_D4, "mainpll_d4",
			"mainpll", 1, 4),
	FACTOR(CLK_TOP_MAINPLL_D4_D2, "mainpll_d4_d2",
			"mainpll", 1, 8),
	FACTOR(CLK_TOP_MAINPLL_D4_D4, "mainpll_d4_d4",
			"mainpll", 1, 16),
	FACTOR(CLK_TOP_MAINPLL_D4_D8, "mainpll_d4_d8",
			"mainpll", 1, 32),
	FACTOR(CLK_TOP_MAINPLL_D4_D16, "mainpll_d4_d16",
			"mainpll", 1, 64),
	FACTOR(CLK_TOP_MAINPLL_D5, "mainpll_d5",
			"mainpll", 1, 5),
	FACTOR(CLK_TOP_MAINPLL_D5_D2, "mainpll_d5_d2",
			"mainpll", 1, 10),
	FACTOR(CLK_TOP_MAINPLL_D5_D4, "mainpll_d5_d4",
			"mainpll", 1, 20),
	FACTOR(CLK_TOP_MAINPLL_D5_D8, "mainpll_d5_d8",
			"mainpll", 1, 40),
	FACTOR(CLK_TOP_MAINPLL_D6, "mainpll_d6",
			"mainpll", 1, 6),
	FACTOR(CLK_TOP_MAINPLL_D6_D2, "mainpll_d6_d2",
			"mainpll", 1, 12),
	FACTOR(CLK_TOP_MAINPLL_D6_D4, "mainpll_d6_d4",
			"mainpll", 1, 24),
	FACTOR(CLK_TOP_MAINPLL_D7, "mainpll_d7",
			"mainpll", 1, 7),
	FACTOR(CLK_TOP_MAINPLL_D7_D2, "mainpll_d7_d2",
			"mainpll", 1, 14),
	FACTOR(CLK_TOP_MAINPLL_D7_D4, "mainpll_d7_d4",
			"mainpll", 1, 28),
	FACTOR(CLK_TOP_MAINPLL_D7_D8, "mainpll_d7_d8",
			"mainpll", 1, 56),
	FACTOR(CLK_TOP_MAINPLL_D9, "mainpll_d9",
			"mainpll", 1, 9),
	FACTOR(CLK_TOP_UNIVPLL_D3, "univpll_d3",
			"univpll", 1, 3),
	FACTOR(CLK_TOP_UNIVPLL_D4, "univpll_d4",
			"univpll", 1, 4),
	FACTOR(CLK_TOP_UNIVPLL_D4_D2, "univpll_d4_d2",
			"univpll", 1, 8),
	FACTOR(CLK_TOP_UNIVPLL_D4_D4, "univpll_d4_d4",
			"univpll", 1, 16),
	FACTOR(CLK_TOP_UNIVPLL_D4_D8, "univpll_d4_d8",
			"univpll", 1, 32),
	FACTOR(CLK_TOP_UNIVPLL_D5, "univpll_d5",
			"univpll", 1, 5),
	FACTOR(CLK_TOP_UNIVPLL_D5_D2, "univpll_d5_d2",
			"univpll", 1, 10),
	FACTOR(CLK_TOP_UNIVPLL_D5_D4, "univpll_d5_d4",
			"univpll", 1, 20),
	FACTOR(CLK_TOP_UNIVPLL_D5_D8, "univpll_d5_d8",
			"univpll", 1, 40),
	FACTOR(CLK_TOP_UNIVPLL_D6, "univpll_d6",
			"univpll", 1, 6),
	FACTOR(CLK_TOP_UNIVPLL_D6_D2, "univpll_d6_d2",
			"univpll", 1, 12),
	FACTOR(CLK_TOP_UNIVPLL_D6_D4, "univpll_d6_d4",
			"univpll", 1, 24),
	FACTOR(CLK_TOP_UNIVPLL_D6_D8, "univpll_d6_d8",
			"univpll", 1, 48),
	FACTOR(CLK_TOP_UNIVPLL_D7, "univpll_d7",
			"univpll", 1, 7),
	FACTOR(CLK_TOP_UNIVPLL_D7_D2, "univpll_d7_d2",
			"univpll", 1, 14),
	FACTOR(CLK_TOP_APLL1, "apll1_ck",
			"apll1", 1, 1),
	FACTOR(CLK_TOP_APLL1_D2, "apll1_d2",
			"apll1", 1, 2),
	FACTOR(CLK_TOP_APLL1_D4, "apll1_d4",
			"apll1", 1, 4),
	FACTOR(CLK_TOP_APLL1_D8, "apll1_d8",
			"apll1", 1, 8),
	FACTOR(CLK_TOP_APLL1_D3, "apll1_d3",
			"apll1", 1, 3),
	FACTOR(CLK_TOP_APLL2, "apll2_ck",
			"apll2", 1, 1),
	FACTOR(CLK_TOP_APLL2_D2, "apll2_d2",
			"apll2", 1, 2),
	FACTOR(CLK_TOP_APLL2_D4, "apll2_d4",
			"apll2", 1, 4),
	FACTOR(CLK_TOP_APLL2_D8, "apll2_d8",
			"apll2", 1, 8),
	FACTOR(CLK_TOP_APLL2_D3, "apll2_d3",
			"apll2", 1, 3),
	FACTOR(CLK_TOP_MMPLL_D4, "mmpll_d4",
			"mmpll", 1, 4),
	FACTOR(CLK_TOP_MMPLL_D4_D4, "mmpll_d4_d4",
			"mmpll", 1, 16),
	FACTOR(CLK_TOP_MMPLL_D5_D4, "mmpll_d5_d4",
			"mmpll", 1, 20),
	FACTOR(CLK_TOP_MMPLL_D6_D2, "mmpll_d6_d2",
			"mmpll", 1, 12),
	FACTOR(CLK_TOP_MMPLL_D7, "mmpll_d7",
			"mmpll", 1, 7),
	FACTOR(CLK_TOP_MSDCPLL, "msdcpll_ck",
			"msdcpll", 1, 1),
	FACTOR(CLK_TOP_MSDCPLL_D2, "msdcpll_d2",
			"msdcpll", 1, 2),
	FACTOR(CLK_TOP_EMIPLL, "emipll_ck",
			"emipll", 1, 1),
	FACTOR(CLK_TOP_CLKRTC, "clkrtc",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_TCK_26M_MX9, "tck_26m_mx9_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_F26M_CK_D2, "f26m_d2",
			"clk13m", 1, 1),
	FACTOR(CLK_TOP_OSC, "osc_ck",
			"ulposc", 1, 1),
	FACTOR(CLK_TOP_OSC_D2, "osc_d2",
			"ulposc", 1, 2),
	FACTOR(CLK_TOP_OSC_D4, "osc_d4",
			"ulposc", 1, 4),
	FACTOR(CLK_TOP_OSC_D7, "osc_d7",
			"ulposc", 1, 7),
	FACTOR(CLK_TOP_OSC_D8, "osc_d8",
			"ulposc", 1, 8),
	FACTOR(CLK_TOP_OSC_D16, "osc_d16",
			"ulposc", 1, 16),
	FACTOR(CLK_TOP_OSC_D10, "osc_d10",
			"ulposc", 1, 10),
	FACTOR(CLK_TOP_F26M, "f26m_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_RTC, "rtc_ck",
			"clk32k", 1, 1),
	FACTOR(CLK_TOP_AXI, "axi_ck",
			"axi_sel", 1, 1),
	FACTOR(CLK_TOPP_AXI, "peri_axi_ck",
			"peri_axi_sel", 1, 1),
	FACTOR(CLK_TOP_BUS, "bus_ck",
			"bus_aximem_sel", 1, 1),
	FACTOR(CLK_TOP_MEM_SUB, "mem_sub_ck",
			"mem_sub_sel", 1, 1),
	FACTOR(CLK_TOP_DVFSRC, "dvfsrc_ck",
			"dvfsrc_sel", 1, 1),
	FACTOR(CLK_TOP_MM, "mm_ck",
			"mm_sel", 1, 1),
	FACTOR(CLK_TOP_DBI, "dbi_ck",
			"dbi_sel", 1, 1),
	FACTOR(CLK_TOP_DISP_PWM, "disp_pwm_ck",
			"disp_pwm_sel", 1, 1),
	FACTOR(CLK_TOP_MFG_REF, "mfg_ref_ck",
			"mfg_ref_sel", 1, 1),
	FACTOR(CLK_TOP_MFG, "mfg_ck",
			"mfg_sel", 1, 1),
	FACTOR(CLK_TOP_UART, "uart_ck",
			"uart_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0_HCLK, "msdc5hclk_ck",
			"msdc5hclk_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC50_0, "msdc50_0_ck",
			"msdc50_0_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC30_1, "msdc30_1_ck",
			"msdc30_1_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_MACRO, "msdc_macro_ck",
			"msdc_macro_sel", 1, 1),
	FACTOR(CLK_TOP_AUDIO, "audio_ck",
			"audio_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_INTBUS, "aud_intbus_ck",
			"aud_intbus_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN1, "aud_engen1_ck",
			"aud_engen1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_ENGEN2, "aud_engen2_ck",
			"aud_engen2_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_1, "aud_1_ck",
			"aud_1_sel", 1, 1),
	FACTOR(CLK_TOP_AUD_2, "aud_2_ck",
			"aud_2_sel", 1, 1),
	FACTOR(CLK_TOP_ATB, "atb_ck",
			"atb_sel", 1, 1),
	FACTOR(CLK_TOP_I2C, "i2c_ck",
			"i2c_sel", 1, 1),
	FACTOR(CLK_TOP_TL, "tl_ck",
			"tl_sel", 1, 1),
	FACTOR(CLK_TOP_DPMAIF_MAIN, "dpmaif_main_ck",
			"dpmaif_main_sel", 1, 1),
	FACTOR(CLK_TOP_MCUPM, "mcupm_ck",
			"mcupm_sel", 1, 1),
	FACTOR(CLK_TOP_SFLASH, "sflash_ck",
			"sflash_sel", 1, 1),
	FACTOR(CLK_TOP_SPI, "spi_ck",
			"spi_sel", 1, 1),
	FACTOR(CLK_TOP_SPIS, "spis_ck",
			"spis_sel", 1, 1),
	FACTOR(CLK_TOP_NFI1X, "nfi1x_ck",
			"nfi1x_sel", 1, 1),
	FACTOR(CLK_TOP_SPINFI_BCLK, "spinfi_bclk_ck",
			"spinfi_bclk_sel", 1, 1),
	FACTOR(CLK_TOP_GCPU, "gcpu_ck",
			"gcpu_sel", 1, 1),
	FACTOR(CLK_TOP_ECC, "ecc_ck",
			"ecc_sel", 1, 1),
	FACTOR(CLK_TOP_HSM_CRYPTO, "hsm_crypto_ck",
			"hsm_crypto_sel", 1, 1),
	FACTOR(CLK_TOP_HSM_ARC, "hsm_arc_ck",
			"hsm_arc_sel", 1, 1),
	FACTOR(CLK_TOP_EIP97, "eip97_ck",
			"eip97_sel", 1, 1),
	FACTOR(CLK_TOP_SNPS_ETH_312P5M, "snps_eth_312p5m_ck",
			"eth_312p5m_sel", 1, 1),
	FACTOR(CLK_TOP_SNPS_ETH_250M, "snps_eth_250m_ck",
			"snps_eth_250m_sel", 1, 1),
	FACTOR(CLK_TOP_SNPS_ETH_62P4M_PTP, "eth_62p4m_ck",
			"eth_62p4m_sel", 1, 1),
	FACTOR(CLK_TOP_SNPS_ETH_50M_RMII, "snps_eth_50m_rmii_ck",
			"eth_50m_rmii_sel", 1, 1),
	FACTOR(CLK_TOP_NETSYS_500M, "netsys_500m_ck",
			"netsys_500m_sel", 1, 1),
	FACTOR(CLK_TOP_NETSYS, "netsys_ck",
			"netsys_sel", 1, 1),
	FACTOR(CLK_TOP_NETSYS_2X, "netsys_2x_ck",
			"netsys_2x_sel", 1, 1),
	FACTOR(CLK_TOP_NETSYS_WED_MCU, "netsys_wed_mcu_ck",
			"netsys_wed_mcu_sel", 1, 1),
	FACTOR(CLK_TOP_NETSYS_MED_MCU, "netsys_med_mcu_ck",
			"netsys_med_mcu_sel", 1, 1),
	FACTOR(CLK_TOP_SGMII_0, "sgmii_0_ck",
			"sgmii_0_sel", 1, 1),
	FACTOR(CLK_TOP_SGMII_SBUS_0, "sgmii_sbus_0_ck",
			"sgmii_sbus_0_sel", 1, 1),
	FACTOR(CLK_TOP_SGMII_1, "sgmii_1_ck",
			"sgmii_1_sel", 1, 1),
	FACTOR(CLK_TOP_SGMII_SBUS_1, "sgmii_sbus_1_ck",
			"sgmii_sbus_1_sel", 1, 1),
	FACTOR(CLK_TOP_USXGMII_SBUS_0, "usxgmii_sbus_0_ck",
			"usxgmii_sbus_0_sel", 1, 1),
	FACTOR(CLK_TOP_USXGMII_SBUS_1, "usxgmii_sbus_1_ck",
			"usxgmii_sbus_1_sel", 1, 1),
	FACTOR(CLK_TOP_AP2CONN_HOST, "ap2conn_host_ck",
			"ap2conn_host_sel", 1, 1),
	FACTOR(CLK_TOP_USB_TOP, "usb_ck",
			"usb_sel", 1, 1),
	FACTOR(CLK_TOP_USB_XHCI, "ssusb_xhci_ck",
			"ssusb_xhci_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_N, "emi_n_ck",
			"emi_n_sel", 1, 1),
	FACTOR(CLK_TOP_HSM_HSAH, "hsm_hsah_ck",
			"hsm_hsah_sel", 1, 1),
	FACTOR(CLK_TOP_RSA, "rsa_ck",
			"rsa_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_OCC_400, "msdc_occ_400_ck",
			"msdc_occ_400_sel", 1, 1),
	FACTOR(CLK_TOP_MSDC_OCC_200, "msdc_occ_200_ck",
			"msdc_occ_200_sel", 1, 1),
	FACTOR(CLK_TOP_DXCC, "dxcc_ck",
			"dxcc_sel", 1, 1),
	FACTOR(CLK_TOP_PCIE_250M, "pcie_250m_ck",
			"pcie_250m_sel", 1, 1),
	FACTOR(CLK_TOP_DSI_OCC, "dsi_occ_ck",
			"dsi_occ_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_INTERFACE_546, "emi_interface_546_ck",
			"emi_546_sel", 1, 1),
	FACTOR(CLK_TOP_EMI_INTERFACE_624, "emi_interface_624_ck",
			"emi_624_sel", 1, 1),
/*	FACTOR(CLK_TOP_AP2CONN_OSC, "ap2conn_osc_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_E_MCLK, "eint_e_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_S_MCLK, "eint_s_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_N_MCLK, "eint_n_mclk",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_EINT_W_MCLK, "eint_w_mclk",
			"tck_26m_mx9_ck", 1, 1),*/
	FACTOR(CLK_TOP_SRCK, "srck_ck",
			"osc_d10", 1, 1),
	FACTOR(CLK_TOP_XFI_PHY_0_REF_XTAL, "xfi_phy0_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_XFI_PHY_1_REF_XTAL, "xfi_phy1_ck",
			"tck_26m_mx9_ck", 1, 1),
/*	FACTOR(CLK_TOP_SGMII_REF_XTAL_0, "sgmii_ref_xtal_0_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_SGMII_REF_XTAL_1, "sgmii_ref_xtal_1_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_XFI_PHY_0_REF_XTAL, "xfi_phy0_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_XFI_PHY_1_REF_XTAL, "xfi_phy1_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_XFI_PLL_REF_XTAL, "xfi_pll_ref_xtal_ck",
			"tck_26m_mx9_ck", 1, 1),
	FACTOR(CLK_TOP_PMSRCK, "pmsrck_ck",
			"osc_d10", 1, 1),
	FACTOR(CLK_TOP_INFRA_PCCIF_SLOW, "infra_pccif_slow_ck",
			"clkrtc", 1, 1),
	FACTOR(CLK_TOP_U_SAP_CFG, "ufs_cfg_ck",
			"clk26m", 1, 1),
	FACTOR(CLK_TOP_U_TICK1US, "f_u_tick1us_ck",
			"clk26m", 1, 1),*/
};

static const char * const vlp_scp_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"univpll_d3",
	"mainpll_d3",
	"univpll_d6",
	"apll1_ck",
	"mainpll_d4",
	"mainpll_d7",
	"osc_d10",
	"tck_26m_mx9_ck"
};

static const char * const vlp_pwrap_ulposc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d7",
	"osc_d8",
	"osc_d16",
	"mainpll_d7_d8"
};

static const char * const vlp_gpt_bclk_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4",
	"mainpll_d4_d8",
	"osc_d10"
};

static const char * const vlp_spmi_p_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8"
};

static const char * const vlp_spmi_m_parents[] = {
	"tck_26m_mx9_ck",
	"f26m_d2",
	"osc_d8",
	"osc_d10",
	"osc_d16",
	"osc_d7",
	"clkrtc",
	"mainpll_d7_d8",
	"mainpll_d6_d8",
	"mainpll_d5_d8"
};

static const char * const vlp_dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_pwm_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d4",
	"clkrtc",
	"osc_d10",
	"mainpll_d4_d8"
};

static const char * const vlp_axi_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d2",
	"mainpll_d7_d4",
	"mainpll_d7_d2"
};

static const char * const vlp_dbgao_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_systimer_26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_pwrmcu_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d5_d2",
	"osc_ck",
	"mainpll_d7"
};

static const char * const vlp_sspm_f26m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_apeint_66m_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"osc_d4",
	"mainpll_d4_d8"
};

static const char * const vlp_srck_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_sramrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const vlp_tl_vlp_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"clkrtc",
	"mainpll_d6_d4",
	"mainpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2"
};

static const char * const vlp_hsm_crypto_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d4_d2",
	"mainpll_d6_d2",
	"mainpll_d7"
};

static const char * const vlp_hsm_arc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10",
	"mainpll_d4_d8",
	"mainpll_d4_d4",
	"mainpll_d6_d2"
};

static const struct mtk_mux vlp_ck_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_GPT_BCLK_SEL/* dts */, "vlp_gpt_bclk_sel",
		vlp_gpt_bclk_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 1/* width */,
		INV_OFS/* upd ofs */, INV_BIT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DXCC_SEL/* dts */, "vlp_dxcc_sel",
		vlp_dxcc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_DXCC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRMCU_SEL/* dts */, "vlp_pwrmcu_sel",
		vlp_pwrmcu_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRMCU_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APEINT_66M_SEL/* dts */, "vlp_apeint_66m_sel",
		vlp_apeint_66m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APEINT_66M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_TL_VLP_SEL/* dts */, "vlp_tl_vlp_sel",
		vlp_tl_vlp_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TL_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_HSM_CRYPTO_SEL/* dts */, "vlp_hsm_crypto_sel",
		vlp_hsm_crypto_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_HSM_CRYPTO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_HSM_ARC_SEL/* dts */, "vlp_hsm_arc_sel",
		vlp_hsm_arc_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_HSM_ARC_SHIFT/* upd shift */),
#else
	/* VLP_CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SCP_SEL/* dts */, "vlp_scp_sel",
		vlp_scp_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SCP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRAP_ULPOSC_SEL/* dts */, "vlp_pwrap_ulposc_sel",
		vlp_pwrap_ulposc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRAP_ULPOSC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_GPT_BCLK_SEL/* dts */, "vlp_gpt_bclk_sel",
		vlp_gpt_bclk_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 1/* width */,
		INV_OFS/* upd ofs */, INV_BIT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DXCC_SEL/* dts */, "vlp_dxcc_sel",
		vlp_dxcc_parents/* parent */, VLP_CLK_CFG_0, VLP_CLK_CFG_0_SET,
		VLP_CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_DXCC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_P_MST_SEL/* dts */, "vlp_spmi_p_sel",
		vlp_spmi_p_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_P_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SPMI_M_MST_SEL/* dts */, "vlp_spmi_m_sel",
		vlp_spmi_m_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 4/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPMI_M_MST_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DVFSRC_SEL/* dts */, "vlp_dvfsrc_sel",
		vlp_dvfsrc_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWM_VLP_SEL/* dts */, "vlp_pwm_vlp_sel",
		vlp_pwm_vlp_parents/* parent */, VLP_CLK_CFG_1, VLP_CLK_CFG_1_SET,
		VLP_CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWM_VLP_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_AXI_VLP_SEL/* dts */, "vlp_axi_vlp_sel",
		vlp_axi_vlp_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_DBGAO_26M_SEL/* dts */, "vlp_dbgao_26m_sel",
		vlp_dbgao_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBGAO_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SYSTIMER_26M_SEL/* dts */, "vlp_systimer_26m_sel",
		vlp_systimer_26m_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SYSTIMER_26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_PWRMCU_SEL/* dts */, "vlp_pwrmcu_sel",
		vlp_pwrmcu_parents/* parent */, VLP_CLK_CFG_2, VLP_CLK_CFG_2_SET,
		VLP_CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PWRMCU_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_SSPM_F26M_SEL/* dts */, "vlp_sspm_f26m_sel",
		vlp_sspm_f26m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SSPM_F26M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_APEINT_66M_SEL/* dts */, "vlp_apeint_66m_sel",
		vlp_apeint_66m_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 2/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_APEINT_66M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRCK_SEL/* dts */, "vlp_srck_sel",
		vlp_srck_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRCK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_SRAMRC_SEL/* dts */, "vlp_sramrc_sel",
		vlp_sramrc_parents/* parent */, VLP_CLK_CFG_3, VLP_CLK_CFG_3_SET,
		VLP_CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 1/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SRAMRC_SHIFT/* upd shift */),
	/* VLP_CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_VLP_CK_TL_VLP_SEL/* dts */, "vlp_tl_vlp_sel",
		vlp_tl_vlp_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TL_VLP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_HSM_CRYPTO_SEL/* dts */, "vlp_hsm_crypto_sel",
		vlp_hsm_crypto_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_HSM_CRYPTO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_VLP_CK_HSM_ARC_SEL/* dts */, "vlp_hsm_arc_sel",
		vlp_hsm_arc_parents/* parent */, VLP_CLK_CFG_5, VLP_CLK_CFG_5_SET,
		VLP_CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 3/* width */,
		VLP_CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_VLP_HSM_ARC_SHIFT/* upd shift */),
#endif
};

static const char * const axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4"
};

static const char * const peri_haxi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d7_d4",
	"mainpll_d4_d4",
	"mainpll_d5_d4",
	"mainpll_d6_d4",
	"osc_d4"
};

static const char * const peri_axi_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6_d2",
	"osc_d4"
};

static const char * const bus_aximem_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"mainpll_d4_d2",
	"mainpll_d5_d2",
	"mainpll_d6"
};

static const char * const mem_sub_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"mainpll_d5_d2",
	"mainpll_d4_d2",
	"mainpll_d6",
	"mmpll_d7",
	"mainpll_d5",
	"univpll_d5",
	"mainpll_d4",
	"univpll_d4"
};

static const char * const dvfsrc_parents[] = {
	"tck_26m_mx9_ck",
	"osc_d10"
};

static const char * const mm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"univpll_d7_d2",
	"mmpll_d6_d2",
	"mmpll_d4_d4",
	"mainpll_d6_d2",
	"mmpll_d5_d4",
	"univpll_d4_d4"
};

static const char * const dbi_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d4",
	"univpll_d4_d8",
	"mainpll_d6_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_d6_d8"
};

static const char * const disp_pwm_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"osc_d2",
	"osc_d4",
	"osc_d16",
	"univpll_d5_d4"
};

static const char * const mfg_ref_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d5_d2"
};

static const char * const mfg_parents[] = {
	"mfg_ref_sel",
	"mfgpll_ck"
};

static const char * const uart_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d6_d4"
};

static const char * const msdc5hclk_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2"
};

static const char * const msdc50_0_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d4_d4",
	"mainpll_d6_d2",
	"univpll_d4_d2"
};

static const char * const msdc30_1_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d2",
	"mainpll_d6_d2",
	"mainpll_d7_d2",
	"msdcpll_d2"
};

static const char * const msdc_macro_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck",
	"msdcpll_d2",
	"univpll_d6_d4"
};

static const char * const audio_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d5_d8",
	"mainpll_d7_d8",
	"mainpll_d4_d16"
};

static const char * const aud_intbus_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d7_d4"
};

static const char * const aud_engen1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_d2",
	"apll1_d4",
	"apll1_d8"
};

static const char * const aud_engen2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_d2",
	"apll2_d4",
	"apll2_d8"
};

static const char * const aud_1_parents[] = {
	"tck_26m_mx9_ck",
	"apll1_ck"
};

static const char * const aud_2_parents[] = {
	"tck_26m_mx9_ck",
	"apll2_ck"
};

static const char * const atb_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d5_d2"
};

static const char * const i2c_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"univpll_d5_d4"
};

static const char * const tl_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d6_d4"
};

static const char * const dpmaif_main_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4_d4",
	"mainpll_d6",
	"mainpll_d4_d2",
	"univpll_d6",
	"univpll_d5",
	"mainpll_d5",
	"univpll_d4_d2"
};

static const char * const mcupm_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d4",
	"mainpll_d6_d2"
};

static const char * const sflash_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d8",
	"univpll_d6_d8",
	"univpll_d5_d8"
};

static const char * const spi_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d4_d8",
	"univpll_d6_d4",
	"univpll_d5_d4",
	"univpll_d4_d4",
	"univpll_d7_d2",
	"univpll_d6_d2"
};

static const char * const spis_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d4_d8",
	"univpll_d6_d4",
	"univpll_d4_d4",
	"univpll_d6_d2",
	"univpll_d4_d2",
	"univpll_d6"
};

static const char * const nfi1x_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"mainpll_d7_d4",
	"mainpll_d6_d4",
	"univpll_d6_d4",
	"mainpll_d4_d4",
	"univpll_d4_d4",
	"mainpll_d6_d2"
};

static const char * const spinfi_bclk_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d8",
	"univpll_d5_d8",
	"mainpll_d4_d8",
	"univpll_d4_d8",
	"mainpll_d6_d4",
	"univpll_d6_d4",
	"univpll_d5_d4"
};

static const char * const gcpu_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6",
	"mainpll_d6",
	"univpll_d4_d2",
	"mainpll_d4_d2",
	"univpll_d6_d2"
};

static const char * const ecc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d9",
	"univpll_d4_d2",
	"mainpll_d6"
};

static const char * const hsm_crypto_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2",
	"mainpll_d7"
};

static const char * const hsm_arc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d8",
	"mainpll_d4_d4",
	"mainpll_d6_d2"
};

static const char * const eip97_parents[] = {
	"tck_26m_mx9_ck",
	"net2pll_ck",
	"mainpll_d3",
	"univpll_d4",
	"mainpll_d4",
	"univpll_d5",
	"mainpll_d6",
	"mainpll_d5_d2"
};

static const char * const eth_312p5m_parents[] = {
	"tck_26m_mx9_ck",
	"net1pll_d8"
};

static const char * const snps_eth_250m_parents[] = {
	"tck_26m_mx9_ck",
	"net1pll_d10"
};

static const char * const eth_62p4m_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d8",
	"apll1_d3",
	"apll2_d3"
};

static const char * const eth_50m_rmii_parents[] = {
	"tck_26m_mx9_ck",
	"net1pll_d50"
};

static const char * const netsys_500m_parents[] = {
	"tck_26m_mx9_ck",
	"net1pll_d5"
};

static const char * const netsys_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d9",
	"univpll_d4_d2",
	"univpll_d7"
};

static const char * const netsys_2x_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4",
	"mainpll_d3",
	"net2pll_ck"
};

static const char * const netsys_wed_mcu_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mainpll_d6",
	"mainpll_d5",
	"wedmcupll_ck"
};

static const char * const netsys_med_mcu_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d6_d4",
	"mainpll_d4_d2",
	"univpll_d7",
	"medmcupll_ck"
};

static const char * const sgmii_0_parents[] = {
	"tck_26m_mx9_ck",
	"sgmiipll_ck"
};

static const char * const sgmii_sbus_0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const sgmii_1_parents[] = {
	"tck_26m_mx9_ck",
	"sgmiipll_ck"
};

static const char * const sgmii_sbus_1_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const usxgmii_sbus_0_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const usxgmii_sbus_1_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const ap2conn_host_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d4"
};

static const char * const usb_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d2",
	"univpll_d5_d2"
};

static const char * const ssusb_xhci_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d5_d4",
	"univpll_d6_d2",
	"univpll_d5_d2"
};

static const char * const emi_n_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d6_d2",
	"mmpll_d4",
	"emipll_ck"
};

static const char * const hsm_hsah_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"mainpll_d9",
	"univpll_d4_d2",
	"mainpll_d6"
};

static const char * const rsa_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d6_d2",
	"mainpll_d7"
};

static const char * const msdc_occ_400_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_ck"
};

static const char * const msdc_occ_200_parents[] = {
	"tck_26m_mx9_ck",
	"msdcpll_d2"
};

static const char * const dxcc_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d2",
	"mainpll_d4_d4"
};

static const char * const pcie_250m_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d7_d2",
	"univpll_d5_d2"
};

static const char * const dsi_occ_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4_d4",
	"univpll_d6_d2",
	"mainpll_d5_d2",
	"univpll_d4_d4",
	"mainpll_d6_d2"
};

static const char * const emi_546_parents[] = {
	"tck_26m_mx9_ck",
	"mainpll_d4"
};

static const char * const emi_624_parents[] = {
	"tck_26m_mx9_ck",
	"univpll_d4"
};

static const char * const apll_i2s0_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s1_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s2_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s4_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s_tdmout_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s5_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const char * const apll_i2s6_mck_parents[] = {
	"aud_1_sel",
	"aud_2_sel"
};

static const struct mtk_mux top_muxes[] = {
#if MT_CCF_MUX_DISABLE
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_HAXI_SEL/* dts */, "peri_haxi_sel",
		peri_haxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_HD_HAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_AXI_SEL/* dts */, "peri_axi_sel",
		peri_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_HD_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DVFSRC_SEL/* dts */, "dvfsrc_sel",
		dvfsrc_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MM_SEL/* dts */, "mm_sel",
		mm_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DBI_SEL/* dts */, "dbi_sel",
		dbi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DBI_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_SEL/* dts */, "mfg_sel",
		mfg_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 10/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_SEL/* dts */, "msdc_macro_sel",
		msdc_macro_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MSDC_MACRO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUDIO_SEL/* dts */, "audio_sel",
		audio_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUDIO_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_1_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_TL_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SFLASH_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPI_SEL/* dts */, "spi_sel",
		spi_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPI_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_CLR_SET_UPD(CLK_TOP_SPIS_SEL/* dts */, "spis_sel",
		spis_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPIS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_NFI1X_SEL/* dts */, "nfi1x_sel",
		nfi1x_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_NFI1X_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SPINFI_BCLK_SEL/* dts */, "spinfi_bclk_sel",
		spinfi_bclk_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_SPINFI_BCLK_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_GCPU_SEL/* dts */, "gcpu_sel",
		gcpu_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_GCPU_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_CLR_SET_UPD(CLK_TOP_ECC_SEL/* dts */, "ecc_sel",
		ecc_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_ECC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_HSM_CRYPTO_SEL/* dts */, "hsm_crypto_sel",
		hsm_crypto_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_HSM_CRYPTO_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_HSM_ARC_SEL/* dts */, "hsm_arc_sel",
		hsm_arc_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_HSM_ARC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EIP97_SEL/* dts */, "eip97_sel",
		eip97_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EIP97_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_CLR_SET_UPD(CLK_TOP_ETH_312P5M_SEL/* dts */, "eth_312p5m_sel",
		eth_312p5m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SNPS_ETH_312P5M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SNPS_ETH_250M_SEL/* dts */, "snps_eth_250m_sel",
		snps_eth_250m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SNPS_ETH_250M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SNPS_ETH_62P4M_PTP_SEL/* dts */, "eth_62p4m_sel",
		eth_62p4m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SNPS_ETH_50M_RMII_SEL/* dts */, "eth_50m_rmii_sel",
		eth_50m_rmii_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SNPS_ETH_50M_RMII_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_CLR_SET_UPD(CLK_TOP_NETSYS_500M_SEL/* dts */, "netsys_500m_sel",
		netsys_500m_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NETSYS_500M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_NETSYS_SEL/* dts */, "netsys_sel",
		netsys_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NETSYS_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_NETSYS_2X_SEL/* dts */, "netsys_2x_sel",
		netsys_2x_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NETSYS_2X_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_NETSYS_WED_MCU_SEL/* dts */, "netsys_wed_mcu_sel",
		netsys_wed_mcu_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NETSYS_WED_MCU_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_CLR_SET_UPD(CLK_TOP_NETSYS_MED_MCU_SEL/* dts */, "netsys_med_mcu_sel",
		netsys_med_mcu_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_NETSYS_MED_MCU_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SGMII_0_SEL/* dts */, "sgmii_0_sel",
		sgmii_0_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SGMII_0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SGMII_SBUS_0_SEL/* dts */, "sgmii_sbus_0_sel",
		sgmii_sbus_0_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SGMII_SBUS_0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_SGMII_1_SEL/* dts */, "sgmii_1_sel",
		sgmii_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SGMII_1_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_CLR_SET_UPD(CLK_TOP_SGMII_SBUS_1_SEL/* dts */, "sgmii_sbus_1_sel",
		sgmii_sbus_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SGMII_SBUS_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USXGMII_SBUS_0_SEL/* dts */, "usxgmii_sbus_0_sel",
		usxgmii_sbus_0_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USXGMII_SBUS_0_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USXGMII_SBUS_1_SEL/* dts */, "usxgmii_sbus_1_sel",
		usxgmii_sbus_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USXGMII_SBUS_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_HSM_HSAH_SEL/* dts */, "hsm_hsah_sel",
		hsm_hsah_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_HSM_HSAH_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_CLR_SET_UPD(CLK_TOP_RSA_SEL/* dts */, "rsa_sel",
		rsa_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_RSA_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_OCC_400_SEL/* dts */, "msdc_occ_400_sel",
		msdc_occ_400_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_OCC_400_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MSDC_OCC_200_SEL/* dts */, "msdc_occ_200_sel",
		msdc_occ_200_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_MSDC_OCC_200_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_CLR_SET_UPD(CLK_TOP_PCIE_250M_SEL/* dts */, "pcie_250m_sel",
		pcie_250m_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_PCIE_250M_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "dsi_occ_sel",
		dsi_occ_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "emi_546_sel",
		emi_546_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_624_SEL/* dts */, "emi_624_sel",
		emi_624_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE2/* upd ofs */, TOP_MUX_EMI_INTERFACE_624_SHIFT/* upd shift */),
#else
	/* CLK_CFG_0 */
	MUX_CLR_SET_UPD(CLK_TOP_AXI_SEL/* dts */, "axi_sel",
		axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_AXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_HAXI_SEL/* dts */, "peri_haxi_sel",
		peri_haxi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 8/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_HD_HAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOPP_AXI_SEL/* dts */, "peri_axi_sel",
		peri_axi_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 16/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_PERI_HD_FAXI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_BUS_AXIMEM_SEL/* dts */, "bus_aximem_sel",
		bus_aximem_parents/* parent */, CLK_CFG_0, CLK_CFG_0_SET,
		CLK_CFG_0_CLR/* set parent */, 24/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_BUS_AXIMEM_SHIFT/* upd shift */),
	/* CLK_CFG_1 */
	MUX_CLR_SET_UPD(CLK_TOP_MEM_SUB_SEL/* dts */, "mem_sub_sel",
		mem_sub_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 0/* lsb */, 4/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MEM_SUB_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DVFSRC_SEL/* dts */, "dvfsrc_sel",
		dvfsrc_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DVFSRC_SHIFT/* upd shift */),
	MUX_HWV(CLK_TOP_MM_SEL/* dts */, "mm_sel",
		mm_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */,
		HWV_CLK_CFG_1_STA, HWV_CLK_CFG_1_SET, HWV_CLK_CFG_1_CLR/* set hwv */,
		16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DBI_SEL/* dts */, "dbi_sel",
		dbi_parents/* parent */, CLK_CFG_1, CLK_CFG_1_SET,
		CLK_CFG_1_CLR/* set parent */, 24/* lsb */, 4/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DBI_SHIFT/* upd shift */),
	/* CLK_CFG_2 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DISP_PWM_SEL/* dts */, "disp_pwm_sel",
		disp_pwm_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_DISP_PWM_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_REF_SEL/* dts */, "mfg_ref_sel",
		mfg_ref_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MFG_SEL/* dts */, "mfg_sel",
		mfg_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 10/* lsb */, 1/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MFG_REF_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_UART_SEL/* dts */, "uart_sel",
		uart_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_UART_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_HCLK_SEL/* dts */, "msdc5hclk_sel",
		msdc5hclk_parents/* parent */, CLK_CFG_2, CLK_CFG_2_SET,
		CLK_CFG_2_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_HCLK_SHIFT/* upd shift */),
	/* CLK_CFG_3 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC50_0_SEL/* dts */, "msdc50_0_sel",
		msdc50_0_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC50_0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC30_1_SEL/* dts */, "msdc30_1_sel",
		msdc30_1_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC30_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_MACRO_SEL/* dts */, "msdc_macro_sel",
		msdc_macro_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_MSDC_MACRO_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUDIO_SEL/* dts */, "audio_sel",
		audio_parents/* parent */, CLK_CFG_3, CLK_CFG_3_SET,
		CLK_CFG_3_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUDIO_SHIFT/* upd shift */),
	/* CLK_CFG_4 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_INTBUS_SEL/* dts */, "aud_intbus_sel",
		aud_intbus_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_INTBUS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN1_SEL/* dts */, "aud_engen1_sel",
		aud_engen1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_ENGEN2_SEL/* dts */, "aud_engen2_sel",
		aud_engen2_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_ENGEN2_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_1_SEL/* dts */, "aud_1_sel",
		aud_1_parents/* parent */, CLK_CFG_4, CLK_CFG_4_SET,
		CLK_CFG_4_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_1_SHIFT/* upd shift */),
	/* CLK_CFG_5 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_AUD_2_SEL/* dts */, "aud_2_sel",
		aud_2_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_AUD_2_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_ATB_SEL/* dts */, "atb_sel",
		atb_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_ATB_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_I2C_SEL/* dts */, "i2c_sel",
		i2c_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_I2C_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_TL_SEL/* dts */, "tl_sel",
		tl_parents/* parent */, CLK_CFG_5, CLK_CFG_5_SET,
		CLK_CFG_5_CLR/* set parent */, 24/* lsb */, 2/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_TL_SHIFT/* upd shift */),
	/* CLK_CFG_6 */
	MUX_CLR_SET_UPD(CLK_TOP_DPMAIF_MAIN_SEL/* dts */, "dpmaif_main_sel",
		dpmaif_main_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 0/* lsb */, 3/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_DPMAIF_MAIN_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_MCUPM_SEL/* dts */, "mcupm_sel",
		mcupm_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 8/* lsb */, 2/* width */,
		CLK_CFG_UPDATE/* upd ofs */, TOP_MUX_MCUPM_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SFLASH_SEL/* dts */, "sflash_sel",
		sflash_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SFLASH_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPI_SEL/* dts */, "spi_sel",
		spi_parents/* parent */, CLK_CFG_6, CLK_CFG_6_SET,
		CLK_CFG_6_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPI_SHIFT/* upd shift */),
	/* CLK_CFG_7 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPIS_SEL/* dts */, "spis_sel",
		spis_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPIS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NFI1X_SEL/* dts */, "nfi1x_sel",
		nfi1x_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_NFI1X_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SPINFI_BCLK_SEL/* dts */, "spinfi_bclk_sel",
		spinfi_bclk_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 16/* lsb */, 3/* width */,
		23/* pdn */, CLK_CFG_UPDATE/* upd ofs */,
		TOP_MUX_SPINFI_BCLK_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_GCPU_SEL/* dts */, "gcpu_sel",
		gcpu_parents/* parent */, CLK_CFG_7, CLK_CFG_7_SET,
		CLK_CFG_7_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_GCPU_SHIFT/* upd shift */),
	/* CLK_CFG_8 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ECC_SEL/* dts */, "ecc_sel",
		ecc_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_ECC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_HSM_CRYPTO_SEL/* dts */, "hsm_crypto_sel",
		hsm_crypto_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_HSM_CRYPTO_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_HSM_ARC_SEL/* dts */, "hsm_arc_sel",
		hsm_arc_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_HSM_ARC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_EIP97_SEL/* dts */, "eip97_sel",
		eip97_parents/* parent */, CLK_CFG_8, CLK_CFG_8_SET,
		CLK_CFG_8_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_EIP97_SHIFT/* upd shift */),
	/* CLK_CFG_9 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_ETH_312P5M_SEL/* dts */, "eth_312p5m_sel",
		eth_312p5m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SNPS_ETH_312P5M_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SNPS_ETH_250M_SEL/* dts */, "snps_eth_250m_sel",
		snps_eth_250m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SNPS_ETH_250M_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SNPS_ETH_62P4M_PTP_SEL/* dts */, "eth_62p4m_sel",
		eth_62p4m_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SNPS_ETH_62P4M_PTP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SNPS_ETH_50M_RMII_SEL/* dts */, "eth_50m_rmii_sel",
		eth_50m_rmii_parents/* parent */, CLK_CFG_9, CLK_CFG_9_SET,
		CLK_CFG_9_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SNPS_ETH_50M_RMII_SHIFT/* upd shift */),
	/* CLK_CFG_10 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NETSYS_500M_SEL/* dts */, "netsys_500m_sel",
		netsys_500m_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_NETSYS_500M_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NETSYS_SEL/* dts */, "netsys_sel",
		netsys_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_NETSYS_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NETSYS_2X_SEL/* dts */, "netsys_2x_sel",
		netsys_2x_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 16/* lsb */, 2/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_NETSYS_2X_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NETSYS_WED_MCU_SEL/* dts */, "netsys_wed_mcu_sel",
		netsys_wed_mcu_parents/* parent */, CLK_CFG_10, CLK_CFG_10_SET,
		CLK_CFG_10_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_NETSYS_WED_MCU_SHIFT/* upd shift */),
	/* CLK_CFG_11 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_NETSYS_MED_MCU_SEL/* dts */, "netsys_med_mcu_sel",
		netsys_med_mcu_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 0/* lsb */, 3/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_NETSYS_MED_MCU_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SGMII_0_SEL/* dts */, "sgmii_0_sel",
		sgmii_0_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SGMII_0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SGMII_SBUS_0_SEL/* dts */, "sgmii_sbus_0_sel",
		sgmii_sbus_0_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SGMII_SBUS_0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SGMII_1_SEL/* dts */, "sgmii_1_sel",
		sgmii_1_parents/* parent */, CLK_CFG_11, CLK_CFG_11_SET,
		CLK_CFG_11_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SGMII_1_SHIFT/* upd shift */),
	/* CLK_CFG_12 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_SGMII_SBUS_1_SEL/* dts */, "sgmii_sbus_1_sel",
		sgmii_sbus_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 0/* lsb */, 1/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SGMII_SBUS_1_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USXGMII_SBUS_0_SEL/* dts */, "usxgmii_sbus_0_sel",
		usxgmii_sbus_0_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USXGMII_SBUS_0_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USXGMII_SBUS_1_SEL/* dts */, "usxgmii_sbus_1_sel",
		usxgmii_sbus_1_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USXGMII_SBUS_1_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_AP2CONN_HOST_SEL/* dts */, "ap2conn_host_sel",
		ap2conn_host_parents/* parent */, CLK_CFG_12, CLK_CFG_12_SET,
		CLK_CFG_12_CLR/* set parent */, 24/* lsb */, 1/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_AP2CONN_HOST_SHIFT/* upd shift */),
	/* CLK_CFG_13 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_TOP_SEL/* dts */, "usb_sel",
		usb_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_USB_TOP_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_USB_XHCI_SEL/* dts */, "ssusb_xhci_sel",
		ssusb_xhci_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 8/* lsb */, 2/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_SSUSB_XHCI_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_EMI_N_SEL/* dts */, "emi_n_sel",
		emi_n_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 16/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_EMI_N_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_HSM_HSAH_SEL/* dts */, "hsm_hsah_sel",
		hsm_hsah_parents/* parent */, CLK_CFG_13, CLK_CFG_13_SET,
		CLK_CFG_13_CLR/* set parent */, 24/* lsb */, 3/* width */,
		31/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_HSM_HSAH_SHIFT/* upd shift */),
	/* CLK_CFG_14 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_RSA_SEL/* dts */, "rsa_sel",
		rsa_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_RSA_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_OCC_400_SEL/* dts */, "msdc_occ_400_sel",
		msdc_occ_400_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 8/* lsb */, 1/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC_OCC_400_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_MSDC_OCC_200_SEL/* dts */, "msdc_occ_200_sel",
		msdc_occ_200_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_MSDC_OCC_200_SHIFT/* upd shift */),
	MUX_CLR_SET_UPD(CLK_TOP_DXCC_SEL/* dts */, "dxcc_sel",
		dxcc_parents/* parent */, CLK_CFG_14, CLK_CFG_14_SET,
		CLK_CFG_14_CLR/* set parent */, 24/* lsb */, 2/* width */,
		CLK_CFG_UPDATE1/* upd ofs */, TOP_MUX_DXCC_SHIFT/* upd shift */),
	/* CLK_CFG_15 */
	MUX_GATE_CLR_SET_UPD(CLK_TOP_PCIE_250M_SEL/* dts */, "pcie_250m_sel",
		pcie_250m_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 0/* lsb */, 2/* width */,
		7/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_PCIE_250M_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_DSI_OCC_SEL/* dts */, "dsi_occ_sel",
		dsi_occ_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 8/* lsb */, 3/* width */,
		15/* pdn */, CLK_CFG_UPDATE1/* upd ofs */,
		TOP_MUX_DSI_OCC_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_546_SEL/* dts */, "emi_546_sel",
		emi_546_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 16/* lsb */, 1/* width */,
		23/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_EMI_INTERFACE_546_SHIFT/* upd shift */),
	MUX_GATE_CLR_SET_UPD(CLK_TOP_EMI_INTERFACE_624_SEL/* dts */, "emi_624_sel",
		emi_624_parents/* parent */, CLK_CFG_15, CLK_CFG_15_SET,
		CLK_CFG_15_CLR/* set parent */, 24/* lsb */, 1/* width */,
		31/* pdn */, CLK_CFG_UPDATE2/* upd ofs */,
		TOP_MUX_EMI_INTERFACE_624_SHIFT/* upd shift */),
#endif
};

static const struct mtk_composite top_composites[] = {
	/* CLK_AUDDIV_0 */
	MUX(CLK_TOP_APLL_I2S0_MCK_SEL/* dts */, "apll_i2s0_mck_sel",
		apll_i2s0_mck_parents/* parent */, 0x0320/* ofs */,
		16/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S1_MCK_SEL/* dts */, "apll_i2s1_mck_sel",
		apll_i2s1_mck_parents/* parent */, 0x0320/* ofs */,
		17/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S2_MCK_SEL/* dts */, "apll_i2s2_mck_sel",
		apll_i2s2_mck_parents/* parent */, 0x0320/* ofs */,
		18/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S4_MCK_SEL/* dts */, "apll_i2s4_mck_sel",
		apll_i2s4_mck_parents/* parent */, 0x0320/* ofs */,
		19/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S_TDMOUT_MCK_SEL/* dts */, "apll_i2s_tdmout_sel",
		apll_i2s_tdmout_parents/* parent */, 0x0320/* ofs */,
		20/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S5_MCK_SEL/* dts */, "apll_i2s5_mck_sel",
		apll_i2s5_mck_parents/* parent */, 0x0320/* ofs */,
		21/* lsb */, 1/* width */),
	MUX(CLK_TOP_APLL_I2S6_MCK_SEL/* dts */, "apll_i2s6_mck_sel",
		apll_i2s6_mck_parents/* parent */, 0x0320/* ofs */,
		22/* lsb */, 1/* width */),
	/* CLK_AUDDIV_2 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV0/* dts */, "apll12_div0"/* ccf */,
		"apll_i2s0_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		0/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV1/* dts */, "apll12_div1"/* ccf */,
		"apll_i2s1_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		1/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV2/* dts */, "apll12_div2"/* ccf */,
		"apll_i2s2_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		2/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV4/* dts */, "apll12_div4"/* ccf */,
		"apll_i2s3_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		3/* pdn bit */, CLK_AUDDIV_2/* ofs */, 8/* width */,
		24/* lsb */),
	/* CLK_AUDDIV_3 */
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_M/* dts */, "apll12_tdmout_m"/* ccf */,
		"apll_tdmout_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		4/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		0/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV_TDMOUT_B/* dts */, "apll12_tdmout_b"/* ccf */,
		"apll_tdmout_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		5/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		8/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV5/* dts */, "apll12_div5"/* ccf */,
		"apll_i2s5_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		6/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		16/* lsb */),
	DIV_GATE(CLK_TOP_APLL12_CK_DIV6/* dts */, "apll12_div6"/* ccf */,
		"apll_i2s6_mck_sel"/* parent */, 0x0320/* pdn ofs */,
		7/* pdn bit */, CLK_AUDDIV_3/* ofs */, 8/* width */,
		24/* lsb */),
};

#define MT6990_PLL_FMAX		(3800UL * MHZ)
#define MT6990_PLL_FMIN		(1500UL * MHZ)
#define MT6990_INTEGER_BITS	8

#if MT_CCF_PLL_DISABLE
#define PLL_CFLAGS		PLL_AO
#else
#define PLL_CFLAGS		(0)
#endif

#define PLL(_id, _name, _reg, _en_reg, _en_mask, _pll_en_bit,		\
			_pwr_reg, _flags, _rst_bar_mask,		\
			_pd_reg, _pd_shift, _tuner_reg,			\
			_tuner_en_reg, _tuner_en_bit,			\
			_pcw_reg, _pcw_shift, _pcwbits) {		\
		.id = _id,						\
		.name = _name,						\
		.reg = _reg,						\
		.en_reg = _en_reg,					\
		.en_mask = _en_mask,					\
		.pll_en_bit = _pll_en_bit,				\
		.pwr_reg = _pwr_reg,					\
		.flags = (_flags | PLL_CFLAGS),				\
		.rst_bar_mask = _rst_bar_mask,				\
		.fmax = MT6990_PLL_FMAX,				\
		.fmin = MT6990_PLL_FMIN,				\
		.pd_reg = _pd_reg,					\
		.pd_shift = _pd_shift,					\
		.tuner_reg = _tuner_reg,				\
		.tuner_en_reg = _tuner_en_reg,			\
		.tuner_en_bit = _tuner_en_bit,				\
		.pcw_reg = _pcw_reg,					\
		.pcw_shift = _pcw_shift,				\
		.pcwbits = _pcwbits,					\
		.pcwibits = MT6990_INTEGER_BITS,			\
	}

static const struct mtk_pll_data plls[] = {
	PLL(CLK_APMIXED_ARMPLL_LL, "armpll_ll", ARMPLL_LL_CON0/*base*/,
		ARMPLL_LL_CON0, 0x200, 9/*en*/,
		ARMPLL_LL_CON4/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		ARMPLL_LL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		ARMPLL_LL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_CCIPLL, "ccipll", CCIPLL_CON0/*base*/,
		CCIPLL_CON0, 0x200, 9/*en*/,
		CCIPLL_CON4/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		CCIPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		CCIPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MAINPLL, "mainpll", MAINPLL_CON0/*base*/,
		MAINPLL_CON0, 0x200, 9/*en*/,
		MAINPLL_CON4/*pwr*/, HAVE_RST_BAR|PLL_AO, BIT(23)/*rstb*/,
		MAINPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MAINPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_UNIVPLL, "univpll", UNIVPLL_CON0/*base*/,
		UNIVPLL_CON0, 0x200, 9/*en*/,
		UNIVPLL_CON4/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		UNIVPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		UNIVPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MSDCPLL, "msdcpll", MSDCPLL_CON0/*base*/,
		MSDCPLL_CON0, 0x200, 9/*en*/,
		MSDCPLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		MSDCPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MSDCPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MMPLL, "mmpll", MMPLL_CON0/*base*/,
		MMPLL_CON0, 0x200, 9/*en*/,
		MMPLL_CON4/*pwr*/, HAVE_RST_BAR, BIT(23)/*rstb*/,
		MMPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MMPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_APLL1, "apll1", APLL1_CON0/*base*/,
		APLL1_CON0, 0x200, 9/*en*/,
		APLL1_CON5/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL1_CON2, 24/*pd*/,
		APLL1_TUNER_CON0, AP_PLL_CON0, 12/*tuner*/,
		APLL1_CON3, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_APLL2, "apll2", APLL2_CON0/*base*/,
		APLL2_CON0, 0x200, 9/*en*/,
		APLL2_CON5/*pwr*/, 0, BIT(0)/*rstb*/,
		APLL2_CON2, 24/*pd*/,
		APLL2_TUNER_CON0, AP_PLL_CON0, 13/*tuner*/,
		APLL2_CON3, 0, 32/*pcw*/),
	PLL(CLK_APMIXED_MPLL, "mpll", MPLL_CON0/*base*/,
		MPLL_CON0, 0x200, 9/*en*/,
		MPLL_CON4/*pwr*/, PLL_AO, BIT(0)/*rstb*/,
		MPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MFGPLL, "mfgpll", MFGPLL_CON0/*base*/,
		MFGPLL_CON0, 0x200, 9/*en*/,
		MFGPLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		MFGPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MFGPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_NET1PLL, "net1pll", NET1PLL_CON0/*base*/,
		NET1PLL_CON0, 0x200, 9/*en*/,
		NET1PLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		NET1PLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		NET1PLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_NET2PLL, "net2pll", NET2PLL_CON0/*base*/,
		NET2PLL_CON0, 0x200, 9/*en*/,
		NET2PLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		NET2PLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		NET2PLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_WEDMCUPLL, "wedmcupll", WEDMCUPLL_CON0/*base*/,
		WEDMCUPLL_CON0, 0x200, 9/*en*/,
		WEDMCUPLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		WEDMCUPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		WEDMCUPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_MEDMCUPLL, "medmcupll", MEDMCUPLL_CON0/*base*/,
		MEDMCUPLL_CON0, 0x200, 9/*en*/,
		MEDMCUPLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		MEDMCUPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		MEDMCUPLL_CON2, 0, 22/*pcw*/),
	PLL(CLK_APMIXED_SGMIIPLL, "sgmiipll", SGMIIPLL_CON0/*base*/,
		SGMIIPLL_CON0, 0x200, 9/*en*/,
		SGMIIPLL_CON4/*pwr*/, 0, BIT(0)/*rstb*/,
		SGMIIPLL_CON2, 24/*pd*/,
		0, 0, 0/*tuner*/,
		SGMIIPLL_CON2, 0, 22/*pcw*/),
};

static int clk_mt6990_apmixed_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_APMIXED_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_plls(node, plls, ARRAY_SIZE(plls),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

	apmixed_base = base;

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_top_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_TOP_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(top_divs, ARRAY_SIZE(top_divs),
			clk_data);

	mtk_clk_register_muxes(&pdev->dev, top_muxes, ARRAY_SIZE(top_muxes), node,
			&mt6990_clk_lock, clk_data);

	mtk_clk_register_composites(&pdev->dev, top_composites, ARRAY_SIZE(top_composites),
			base, &mt6990_clk_lock, clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_vlp_ck_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

	void __iomem *base;
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(base)) {
		pr_err("%s(): ioremap failed\n", __func__);
		return PTR_ERR(base);
	}

	clk_data = mtk_alloc_clk_data(CLK_VLP_CK_NR_CLK);
	if (!clk_data)
		return -ENOMEM;

	mtk_clk_register_factors(vlp_ck_divs, ARRAY_SIZE(vlp_ck_divs),
			clk_data);

	mtk_clk_register_muxes(&pdev->dev, vlp_ck_muxes, ARRAY_SIZE(vlp_ck_muxes), node,
			&mt6990_clk_lock, clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

/* for suspend LDVT only */
static void pll_force_off(void)
{
	void __iomem *rst_reg, *en_reg, *pwr_reg;
	u32 i;

	for (i = 0; i < ARRAY_SIZE(plls); i++) {
		/* do not pwrdn the AO PLLs */
		if ((plls[i].flags & PLL_AO) == PLL_AO){
			pr_notice(" %s is AO pll \n",plls[i].name);
			continue;
		}
		if ((plls[i].flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			rst_reg = apmixed_base + plls[i].en_reg;
			writel(readl(rst_reg) & ~plls[i].rst_bar_mask,
				rst_reg);
		}

		en_reg = apmixed_base + plls[i].en_reg;

		pwr_reg = apmixed_base + plls[i].pwr_reg;

		writel(readl(en_reg) & ~plls[i].en_mask,
				en_reg);
		writel(readl(pwr_reg) | (0x2),
				pwr_reg);
		writel(readl(pwr_reg) & ~(0x1),
				pwr_reg);
		pr_notice(" force off %s \n",plls[i].name);
	}
}

static struct generic_pm_domain **get_all_genpd(void)
{
	static struct generic_pm_domain *pds[31];
	static int num_pds;
	const size_t maxpd = ARRAY_SIZE(pds);
	struct device_node *node;
	struct platform_device *pdev;
	int r;
	if (num_pds != 0)
		goto out;
	node = of_find_node_with_property(NULL, "#power-domain-cells");
	if (node == NULL)
		return NULL;
	pdev = platform_device_alloc("traverse", 0);
	if (pdev == NULL)
		return NULL;
	for (num_pds = 0; num_pds < maxpd; num_pds++) {
		struct of_phandle_args pa;
		pa.np = node;
		pa.args[0] = num_pds;
		pa.args_count = 1;
		r = of_genpd_add_device(&pa, &pdev->dev);
		if (r == -EINVAL)
			continue;
		else if (r != 0)
			pr_warn("%s(): of_genpd_add_device(%d)\n", __func__, r);
		pds[num_pds] = pd_to_genpd(pdev->dev.pm_domain);
		//r = pm_genpd_remove_device(pds[num_pds], &pdev->dev);
		r = pm_genpd_remove_device(&pdev->dev);
		if (r != 0)
			pr_warn("%s(): pm_genpd_remove_device(%d)\n",
					__func__, r);
		if (IS_ERR(pds[num_pds])) {
			pds[num_pds] = NULL;
			break;
		}
	}
	platform_device_put(pdev);
out:
	return pds;
}

static void subsys_force_off(void)
{
	struct generic_pm_domain *genpd;
	int (*gpd_op)(struct generic_pm_domain *);
	int r = 0;
	struct generic_pm_domain **pds = get_all_genpd();
	
	if(pds != NULL) {
		for (; *pds != NULL; pds++) {
			genpd = *pds;
			if (IS_ERR_OR_NULL(genpd))
				continue;
			if((genpd->flags & GENPD_FLAG_ALWAYS_ON)|(genpd->status == GENPD_STATE_OFF))
				continue;
			gpd_op = genpd->power_off;
			r |= gpd_op(genpd);
		}
	}
}

static void pll_if_on(void)
{
    void __iomem *en_reg;
    u32 i;
	for (i = 0; i < ARRAY_SIZE(plls); i++) {

        en_reg = apmixed_base + plls[i].en_reg;

        if (readl(en_reg) & plls[i].en_mask)
            pr_notice("suspend warning : %s is on !!!\n",plls[i].name);

	}
}

static void mtcmos_if_on(void)
{
    static const char * const pwr_names[] = {
		[0] = "MD1",
		[1] = "CONN",
		[3] = "IFR",
		[4] = "PERI",
		[5] = "AUDIO",
		[6] = "DIS0",
		[8] = "EIP97",
		[9] = "PEXTP_R_2LX1_MAC",
		[10] = "PEXTP_R_2LX2_MAC",
		[11] = "PEXTP_D_2LX1_MAC",
		[12] = "PEXTP_D_2LX1_PHY_AO",
		[13] = "PEXTP_D_2LX1_PHY_PD",
		[14] = "PEXTP_R_2LX1_PHY",
		[15] = "PEXTP_R_2LX2_PHY",
		[16] = "MSDC",
		[17] = "ETH",
		[18] = "SSUSB_TOP",
		[19] = "SSUSB_PHY",
		[20] = "MEDSYS",
		[21] = "NETSYS",
		[22] = "HSM_TOP",
		[23] = "XFI_PHY_0",
		[24] = "XFI_PHY_1",
		[25] = "XFI_TOP_0",
		[26] = "XFI_TOP_1",
		[27] = "SGMII_TOP_0",
		[28] = "SGMII_TOP_1",
	};
    u32 val = 0,i;
    static void __iomem *scpsys_base, *pwr_sta, *pwr_sta_2nd;
    scpsys_base = ioremap(0x12001000, PAGE_SIZE);
    pwr_sta = scpsys_base + 0xF38;
    pwr_sta_2nd = scpsys_base + 0xF3C;
    val = readl(pwr_sta) & readl(pwr_sta_2nd);
    for (i = 0; i < 29; i++) {
        if((val & BIT(i)) != 0U)
            pr_notice("suspend warning: %s is on!!\n",pwr_names[i]);
    }
}

static int pll_status_cmd(struct seq_file *s, void *v)
{
	seq_printf(s, "Call pll_if_on \n");
    pll_if_on();
	return 0;
}

static int mtcmos_status_cmd(struct seq_file *s, void *v)
{
	seq_printf(s, "Call mtcmos_if_on \n");
    mtcmos_if_on();
	return 0;
}

static int pll_off_cmd(struct seq_file *s, void *v)
{
	seq_printf(s, "Call pll_force_off \n");
    pll_force_off();
	return 0;
}

static int mtcmos_off_cmd(struct seq_file *s, void *v)
{
	seq_printf(s, "Call subsys_force_off \n");
    subsys_force_off();
	return 0;
}

static int all_off_cmd(struct seq_file *s, void *v)
{
	seq_printf(s, "Call pll/mtcmos off and status \n");
    pll_force_off();
    subsys_force_off();
    pll_if_on();
    mtcmos_if_on();
	return 0;
}

static const struct cmd_fn cmds[] = {
    CMDFN("pll_status", pll_status_cmd),
    CMDFN("mtcmos_status", mtcmos_status_cmd),
    CMDFN("pll_off", pll_off_cmd),
    CMDFN("mtcmos_off", mtcmos_off_cmd),
    CMDFN("all_off", all_off_cmd),
    {}
};


static const struct of_device_id of_match_clk_mt6990[] = {
	{
		.compatible = "mediatek,mt6990-apmixedsys",
		.data = clk_mt6990_apmixed_probe,
	}, {
		.compatible = "mediatek,mt6990-topckgen",
		.data = clk_mt6990_top_probe,
	}, {
		.compatible = "mediatek,mt6990-vlp_cksys",
		.data = clk_mt6990_vlp_ck_probe,
	}, {
		/* sentinel */
	}
};

static int clk_mt6990_probe(struct platform_device *pdev)
{
	int (*clk_probe)(struct platform_device *pd);
	int r;

	clk_probe = of_device_get_match_data(&pdev->dev);
	if (!clk_probe)
		return -EINVAL;

	r = clk_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#ifdef CONFIG_MTK_CLK_CHECK
	set_custom_cmds(cmds);
#endif

	return r;
}

static struct platform_driver clk_mt6990_drv = {
	.probe = clk_mt6990_probe,
	.driver = {
		.name = "clk-mt6990",
		.owner = THIS_MODULE,
		.of_match_table = of_match_clk_mt6990,
	},
};

static int __init clk_mt6990_init(void)
{
	return platform_driver_register(&clk_mt6990_drv);
}

static void __exit clk_mt6990_exit(void)
{
	platform_driver_unregister(&clk_mt6990_drv);
}

arch_initcall(clk_mt6990_init);
module_exit(clk_mt6990_exit);
MODULE_LICENSE("GPL");
