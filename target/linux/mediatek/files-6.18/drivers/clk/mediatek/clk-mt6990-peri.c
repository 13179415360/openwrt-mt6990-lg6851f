// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Pei-hsuan Cheng <pei-hsuan.cheng@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6990-clk.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs imp_iic_top_wrap0_cg_regs = {
	.set_ofs = 0xD44E00,
	.clr_ofs = 0xD44E00,
	.sta_ofs = 0xD44E00,
};

static const struct mtk_gate_regs imp_iic_top_wrap1_cg_regs = {
	.set_ofs = 0xED2E00,
	.clr_ofs = 0xED2E00,
	.sta_ofs = 0xED2E00,
};

#define GATE_IMP_IIC_TOP_WRAP0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_iic_top_wrap0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_IMP_IIC_TOP_WRAP1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &imp_iic_top_wrap1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate imp_iic_top_wrap_clks[] = {
	/* IMP_IIC_TOP_WRAP0 */
	GATE_IMP_IIC_TOP_WRAP0(CLK_IMP_IIC_TOP_WRAP_I2C0, "imp_iic_wrap_i2c0",
			"i2c_ck"/* parent */, 0),
	GATE_IMP_IIC_TOP_WRAP0(CLK_IMP_IIC_TOP_WRAP_I2C1, "imp_iic_wrap_i2c1",
			"i2c_ck"/* parent */, 1),
	GATE_IMP_IIC_TOP_WRAP0(CLK_IMP_IIC_TOP_WRAP_I2C4, "imp_iic_wrap_i2c4",
			"i2c_ck"/* parent */, 2),
	GATE_IMP_IIC_TOP_WRAP0(CLK_IMP_IIC_TOP_WRAP_I2C5, "imp_iic_wrap_i2c5",
			"i2c_ck"/* parent */, 3),
	/* IMP_IIC_TOP_WRAP1 */
	GATE_IMP_IIC_TOP_WRAP1(CLK_IMP_IIC_TOP_WRAP_I2C2, "imp_iic_wrap_i2c2",
			"i2c_ck"/* parent */, 0),
	GATE_IMP_IIC_TOP_WRAP1(CLK_IMP_IIC_TOP_WRAP_I2C3, "imp_iic_wrap_i2c3",
			"i2c_ck"/* parent */, 1),
};

//static const struct mtk_clk_desc imp_iic_top_wrap_mcd = {
//	.clks = imp_iic_top_wrap_clks,
//	.num_clks = CLK_IMP_IIC_TOP_WRAP_NR_CLK,
//};

static const struct mtk_gate_regs msdc00_cg_regs = {
	.set_ofs = 0x68,
	.clr_ofs = 0x68,
	.sta_ofs = 0x68,
};

static const struct mtk_gate_regs msdc01_cg_regs = {
	.set_ofs = 0xB4,
	.clr_ofs = 0xB4,
	.sta_ofs = 0xB4,
};

#define GATE_MSDC00(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &msdc00_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

#define GATE_MSDC01(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &msdc01_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate msdc0_clks[] = {
	/* MSDC00 */
	GATE_MSDC00(CLK_MSDC0_MSDC_NEW_RX_PATH_SEL, "msdc0_msdc_rx",
			"msdc0_new_rx_ck"/* parent */, 0),
	/* MSDC01 */
	GATE_MSDC01(CLK_MSDC0_AXI_WRAP_CKEN, "msdc0_axi_wrap_cken",
			"axi_ck"/* parent */, 22),
};

//static const struct mtk_clk_desc msdc0_mcd = {
//	.clks = msdc0_clks,
//	.num_clks = CLK_MSDC0_NR_CLK,
//};

static const struct mtk_gate_regs msdc1_cg_regs = {
	.set_ofs = 0x68,
	.clr_ofs = 0x68,
	.sta_ofs = 0x68,
};

#define GATE_MSDC1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &msdc1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate msdc1_clks[] = {
	GATE_MSDC1(CLK_MSDC1_MSDC_NEW_RX_PATH_SEL, "msdc1_msdc_rx",
			"msdc30_1_ck"/* parent */, 0),
};

//static const struct mtk_clk_desc msdc1_mcd = {
//	.clks = msdc1_clks,
//	.num_clks = CLK_MSDC1_NR_CLK,
//};

static const struct mtk_gate_regs perao0_cg_regs = {
	.set_ofs = 0x3C,
	.clr_ofs = 0x3C,
	.sta_ofs = 0x3C,
};

static const struct mtk_gate_regs perao1_cg_regs = {
	.set_ofs = 0x40,
	.clr_ofs = 0x40,
	.sta_ofs = 0x40,
};

static const struct mtk_gate_regs perao2_cg_regs = {
	.set_ofs = 0x44,
	.clr_ofs = 0x44,
	.sta_ofs = 0x44,
};

#define GATE_PERAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_PERAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_PERAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &perao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate perao_clks[] = {
	/* PERAO0 */
	GATE_PERAO0(CLK_PERAO_UART0, "perao_uart0",
			"peri_axi_ck"/* parent */, 0),
	GATE_PERAO0(CLK_PERAO_UART1, "perao_uart1",
			"peri_axi_ck"/* parent */, 1),
	GATE_PERAO0(CLK_PERAO_UART2, "perao_uart2",
			"peri_axi_ck"/* parent */, 2),
	GATE_PERAO0(CLK_PERAO_UART3, "perao_uart3",
			"peri_axi_ck"/* parent */, 3),
	GATE_PERAO0(CLK_PERAO_UARTHUB_PCLK, "perao_uarthub_pclk",
			"peri_axi_ck"/* parent */, 5),
	GATE_PERAO0(CLK_PERAO_UARTHUB, "perao_uarthub",
			"peri_axi_ck"/* parent */, 6),
	GATE_PERAO0(CLK_PERAO_SPI0_BCLK, "perao_spi0_bclk",
			"spi_ck"/* parent */, 8),
	GATE_PERAO0(CLK_PERAO_SPI1_BCLK, "perao_spi1_bclk",
			"spi_ck"/* parent */, 9),
	GATE_PERAO0(CLK_PERAO_SPI2_BCLK, "perao_spi2_bclk",
			"spi_ck"/* parent */, 10),
	GATE_PERAO0(CLK_PERAO_SPI3_BCLK, "perao_spi3_bclk",
			"spi_ck"/* parent */, 11),
	GATE_PERAO0(CLK_PERAO_SPIS_BCLK, "perao_spis_bclk",
			"spis_ck"/* parent */, 12),
	GATE_PERAO0(CLK_PERAO_SPI0_HCLK, "perao_spi0_hclk",
			"peri_axi_ck"/* parent */, 13),
	GATE_PERAO0(CLK_PERAO_SPI1_HCLK, "perao_spi1_hclk",
			"peri_axi_ck"/* parent */, 14),
	GATE_PERAO0(CLK_PERAO_SPI2_HCLK, "perao_spi2_hclk",
			"peri_axi_ck"/* parent */, 15),
	GATE_PERAO0(CLK_PERAO_SPI3_HCLK, "perao_spi3_hclk",
			"peri_axi_ck"/* parent */, 16),
	GATE_PERAO0(CLK_PERAO_SPIS_HCLK, "perao_spis_hclk",
			"peri_axi_ck"/* parent */, 17),
	GATE_PERAO0(CLK_PERAO_I2C_SLAVE, "perao_i2c_slave",
			"i2c_ck"/* parent */, 18),
	GATE_PERAO0(CLK_PERAO_DISP_PWM_BCLK, "perao_disp_pwm_bclk",
			"disp_pwm_ck"/* parent */, 20),
	GATE_PERAO0(CLK_PERAO_FLASHIFLASH_CLK, "perao_flash_clk",
			"sflash_ck"/* parent */, 22),
	GATE_PERAO0(CLK_PERAO_FLASHIF_BCLK, "perao_flashif_bclk",
			"peri_axi_ck"/* parent */, 23),
	GATE_PERAO0(CLK_PERAO_FLASHIF_AXI_CLK, "perao_flash_axi_clk",
			"peri_axi_ck"/* parent */, 24),
	GATE_PERAO0(CLK_PERAO_FLASHIF_DRAM_CLK, "perao_flash_dram_clk",
			"peri_axi_ck"/* parent */, 25),
	GATE_PERAO0(CLK_PERAO_MSDC1_SRC, "perao_msdc1_src",
			"msdc30_1_ck"/* parent */, 26),
	GATE_PERAO0(CLK_PERAO_MSDC1_HCLK, "perao_msdc1_hclk",
			"peri_axi_ck"/* parent */, 27),
	GATE_PERAO0(CLK_PERAO_MSDC50_SCAN, "perao_msdc50_scan",
			"msdc50_0_ck"/* parent */, 29),
	GATE_PERAO0(CLK_PERAO_MSDC50_HCLK, "perao_msdc50_hclk",
			"msdc5hclk_ck"/* parent */, 30),
	GATE_PERAO0(CLK_PERAO_MSDC0_AHB_SLAVE, "perao_msdc0_slave",
			"peri_axi_ck"/* parent */, 31),
	/* PERAO1 */
	GATE_PERAO1(CLK_PERAO_MSDC0_AXI_MASTER, "perao_msdc0_master",
			"peri_axi_ck"/* parent */, 0),
	GATE_PERAO1(CLK_PERAO_AP_DMA_HCLK, "perao_ap_dma_hclk",
			"peri_axi_ck"/* parent */, 4),
	GATE_PERAO1(CLK_PERAO_NFI_BCLK, "perao_nfi_bclk",
			"nfi1x_ck"/* parent */, 5),
	GATE_PERAO1(CLK_PERAO_NFI_CFCLK, "perao_nfi_cfclk",
			"spinfi_bclk_ck"/* parent */, 6),
	GATE_PERAO1(CLK_PERAO_NFI_HCLK, "perao_nfi_hclk",
			"peri_axi_ck"/* parent */, 7),
	GATE_PERAO1(CLK_PERAO_PTP_THERM_MCU_BCLK, "perao_therm_bclk",
			"peri_axi_ck"/* parent */, 8),
	GATE_PERAO1(CLK_PERAO_PTP_THERM_MCU_SYS, "perao_therm_sys",
			"f26m_ck"/* parent */, 9),
	GATE_PERAO1(CLK_PERAO_PTP_THERM_BCLK, "perao_ptp_therm_bclk",
			"peri_axi_ck"/* parent */, 10),
	GATE_PERAO1(CLK_PERAO_PTP_THERM_SYS, "perao_ptp_therm_sys",
			"f26m_ck"/* parent */, 11),
	GATE_PERAO1(CLK_PERAO_USB_FRMCNT, "perao_usb_frmcnt",
			"f_fssusn_fmcnt_ck"/* parent */, 14),
	GATE_PERAO1(CLK_PERAO_USB_SYS, "perao_usb_sys",
			"usb_ck"/* parent */, 15),
	GATE_PERAO1(CLK_PERAO_USB_XCHI, "perao_usb_xchi",
			"ssusb_xhci_ck"/* parent */, 16),
	GATE_PERAO1(CLK_PERAO_USB_DMA, "perao_usb_dma",
			"peri_axi_ck"/* parent */, 20),
	GATE_PERAO1(CLK_PERAO_USB_MCU, "perao_usb_mcu",
			"peri_axi_ck"/* parent */, 21),
	GATE_PERAO1(CLK_PERAO_USB_26M, "perao_usb_26m",
			"f26m_ck"/* parent */, 22),
	GATE_PERAO1(CLK_PERAO_DWC_RMII, "perao_dwc_rmii",
			"snps_eth_50m_rmii_ck"/* parent */, 23),
	GATE_PERAO1(CLK_PERAO_DWC_250, "perao_dwc_250",
			"snps_eth_250m_ck"/* parent */, 24),
	GATE_PERAO1(CLK_PERAO_DWC_312P5, "perao_dwc_312p5",
			"snps_eth_312p5m_ck"/* parent */, 25),
	GATE_PERAO1(CLK_PERAO_DWC_PTP_REF, "perao_dwc_ptp_ref",
			"eth_62p4m_ck"/* parent */, 26),
	GATE_PERAO1(CLK_PERAO_DWC_PTP_ACLK, "perao_dwc_ptp_aclk",
			"peri_axi_ck"/* parent */, 27),
	GATE_PERAO1(CLK_PERAO_PCIE_2LX1_TL, "perao_pcie_2lx1_tl",
			"tl_ck"/* parent */, 28),
	GATE_PERAO1(CLK_PERAO_PCIE_2LX1_REF, "perao_pcie_2lx1_ref",
			"f26m_ck"/* parent */, 30),
	/* PERAO2 */
	GATE_PERAO2(CLK_PERAO_PCIE_2LX1_AXI, "perao_pcie_2lx1_axi",
			"peri_axi_ck"/* parent */, 0),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX1_AHB, "perao_pcie_2lx1_ahb",
			"peri_axi_ck"/* parent */, 1),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX1_26M, "perao_pcie_2lx1_26m",
			"f26m_ck"/* parent */, 2),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX2_TL, "perao_pcie_2lx2_tl",
			"tl_ck"/* parent */, 3),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX2_REF, "perao_pcie_2lx2_ref",
			"f26m_ck"/* parent */, 5),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX2_AXI, "perao_pcie_2lx2_axi",
			"peri_axi_ck"/* parent */, 7),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX2_AHB, "perao_pcie_2lx2_ahb",
			"peri_axi_ck"/* parent */, 8),
	GATE_PERAO2(CLK_PERAO_PCIE_2LX2_26M, "perao_pcie_2lx2_26m",
			"f26m_ck"/* parent */, 9),
	GATE_PERAO2(CLK_PERAO_AUDIO_QAXI, "perao_audio_qaxi",
			"peri_axi_ck"/* parent */, 10),
	GATE_PERAO2(CLK_PERAO_AUDIO_26M, "perao_audio_26m",
			"f26m_ck"/* parent */, 11),
	GATE_PERAO2(CLK_PERAO_MED_MAIN, "perao_med_main",
			"netsys_med_mcu_ck"/* parent */, 12),
	GATE_PERAO2(CLK_PERAO_MED_26M, "perao_med_26m",
			"f26m_ck"/* parent */, 14),
	GATE_PERAO2(CLK_PERAO_DWC_CSR, "perao_dwc_csr",
			"peri_axi_ck"/* parent */, 15),
	GATE_PERAO2(CLK_PERAO_MDAP_BUS, "perao_mdap_bus",
			"peri_axi_ck"/* parent */, 16),
	GATE_PERAO2(CLK_PERAO_MDAP_26M, "perao_mdap_26m",
			"f26m_ck"/* parent */, 17),
	GATE_PERAO2(CLK_PERAO_P2P_EAST_TX, "perao_p2p_east_tx",
			"peri_axi_ck"/* parent */, 18),
	GATE_PERAO2(CLK_PERAO_P2P_WEST_TX, "perao_p2p_west_tx",
			"peri_axi_ck"/* parent */, 19),
	GATE_PERAO2(CLK_PERAO_P2P_SOUTH_TX, "perao_p2p_south_tx",
			"peri_axi_ck"/* parent */, 20),
	GATE_PERAO2(CLK_PERAO_P2P_NORTH_TX, "perao_p2p_north_tx",
			"peri_axi_ck"/* parent */, 21),
	GATE_PERAO2(CLK_PERAO_P2P_WEST_NORTH_TX, "perao_p2p_wn_tx",
			"peri_axi_ck"/* parent */, 22),
	GATE_PERAO2(CLK_PERAO_PERI2PCIE0__TX, "perao_peri2pcie0__tx",
			"peri_axi_ck"/* parent */, 23),
};

//static const struct mtk_clk_desc perao_mcd = {
//	.clks = perao_clks,
//	.num_clks = CLK_PERAO_NR_CLK,
//};

static const struct mtk_gate_regs usb_sif0_cg_regs = {
	.set_ofs = 0x30,
	.clr_ofs = 0x30,
	.sta_ofs = 0x30,
};

static const struct mtk_gate_regs usb_sif1_cg_regs = {
	.set_ofs = 0x50,
	.clr_ofs = 0x50,
	.sta_ofs = 0x50,
};

static const struct mtk_gate_regs usb_sif2_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_USB_SIF0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_SIF1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

#define GATE_USB_SIF2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_sif2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr,	\
	}

static const struct mtk_gate usb_sif_clks[] = {
	/* USB_SIF0 */
	GATE_USB_SIF0(CLK_USB_SIF_USB_U3_P, "usb_sif_usb_u3_p",
			"usb_ck"/* parent */, 0),
	/* USB_SIF1 */
	GATE_USB_SIF1(CLK_USB_SIF_USB_U2_P, "usb_sif_usb_u2_p",
			"usb_ck"/* parent */, 0),
	/* USB_SIF2 */
	GATE_USB_SIF2(CLK_USB_SIF_USB_IP_DMA_B, "usb_sif_usb_dma",
			"axi_ck"/* parent */, 0),
};

//static const struct mtk_clk_desc usb_sif_mcd = {
//	.clks = usb_sif_clks,
//	.num_clks = CLK_USB_SIF_NR_CLK,
//};

static const struct mtk_gate_regs usb_t5ff_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x80,
	.sta_ofs = 0x80,
};

#define GATE_USB_T5FF(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &usb_t5ff_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_no_setclr_inv,	\
	}

static const struct mtk_gate usb_t5ff_clks[] = {
	GATE_USB_T5FF(CLK_USB_T5FF_RG_CHGDT_EN, "usb_t5ff_chgdt_en",
			"hd_faxi_west_ck"/* parent */, 0),
};

//static const struct mtk_clk_desc usb_t5ff_mcd = {
//	.clks = usb_t5ff_clks,
//	.num_clks = CLK_USB_T5FF_NR_CLK,
//};

static int clk_mt6990_peri_imp_i2c_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IMP_IIC_TOP_WRAP_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, imp_iic_top_wrap_clks, ARRAY_SIZE(imp_iic_top_wrap_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_peri_msdc0_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MSDC0_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, msdc0_clks, ARRAY_SIZE(msdc0_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_peri_msdc1_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_MSDC1_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, msdc1_clks, ARRAY_SIZE(msdc1_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_peri_perao_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_PERAO_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, perao_clks, ARRAY_SIZE(perao_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_peri_ssusb_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_USB_SIF_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, usb_sif_clks, ARRAY_SIZE(usb_sif_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static int clk_mt6990_peri_ssusb_phy_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_USB_T5FF_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, usb_t5ff_clks, ARRAY_SIZE(usb_t5ff_clks),
			clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

	if (r)
		pr_err("%s(): could not register clock provider: %d\n",
			__func__, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6990_peri[] = {
	{
		.compatible = "mediatek,mt6990-imp_iic_top_wrap",
		.data = clk_mt6990_peri_imp_i2c_probe,
	}, {
		.compatible = "mediatek,mt6990-msdc0",
		.data = clk_mt6990_peri_msdc0_probe,
	}, {
		.compatible = "mediatek,mt6990-msdc1",
		.data = clk_mt6990_peri_msdc1_probe,
	}, {
		.compatible = "mediatek,mt6990-pericfg_ao",
		.data = clk_mt6990_peri_perao_probe,
	},
	{
		.compatible = "mediatek,mt6990-pericfg-ao",
		.data = clk_mt6990_peri_perao_probe,
	}, {
		.compatible = "mediatek,mt6990-ssusb",
		.data = clk_mt6990_peri_ssusb_probe,
	}, {
		.compatible = "mediatek,mt6990-ssusb_phy",
		.data = clk_mt6990_peri_ssusb_phy_probe,
	}, {
		/* sentinel */
	}
};


static int clk_mt6990_peri_grp_probe(struct platform_device *pdev)
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

	return r;
}

static struct platform_driver clk_mt6990_peri_drv = {
	.probe = clk_mt6990_peri_grp_probe,
	.driver = {
		.name = "clk-mt6990-peri",
		.of_match_table = of_match_clk_mt6990_peri,
	},
};

static int __init clk_mt6990_peri_init(void)
{
	return platform_driver_register(&clk_mt6990_peri_drv);
}

static void __exit clk_mt6990_peri_exit(void)
{
	platform_driver_unregister(&clk_mt6990_peri_drv);
}

arch_initcall(clk_mt6990_peri_init);
module_exit(clk_mt6990_peri_exit);
MODULE_LICENSE("GPL");
