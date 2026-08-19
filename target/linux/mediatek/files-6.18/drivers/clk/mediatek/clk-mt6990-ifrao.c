// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.
// Author: Pei-hsuan Cheng <pei-hsuan.cheng@mediatek.com>

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6990-clk.h>

#define MT_CCF_BRINGUP		0

/* Regular Number Definition */
#define INV_OFS			-1
#define INV_BIT			-1

static const struct mtk_gate_regs ifrao0_cg_regs = {
	.set_ofs = 0x80,
	.clr_ofs = 0x84,
	.sta_ofs = 0x90,
};

static const struct mtk_gate_regs ifrao1_cg_regs = {
	.set_ofs = 0xC0,
	.clr_ofs = 0xC4,
	.sta_ofs = 0xC8,
};

static const struct mtk_gate_regs ifrao2_cg_regs = {
	.set_ofs = 0xE0,
	.clr_ofs = 0xE4,
	.sta_ofs = 0xE8,
};

#define GATE_IFRAO0(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao0_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO1(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao1_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr,	\
	}

#define GATE_IFRAO2_I(_id, _name, _parent, _shift) {	\
		.id = _id,				\
		.name = _name,				\
		.parent_name = _parent,			\
		.regs = &ifrao2_cg_regs,			\
		.shift = _shift,			\
		.ops = &mtk_clk_gate_ops_setclr_inv,	\
	}

static const struct mtk_gate ifrao_clks[] = {
	/* IFRAO0 */
	GATE_IFRAO0(CLK_IFRAO_GCE, "ifrao_gce",
			"axi_ck"/* parent */, 8),
	/* IFRAO1 */
	GATE_IFRAO1(CLK_IFRAO_BIST2FPC, "ifrao_bist2fpc",
			"msdc30_1_ck"/* parent */, 24),
	GATE_IFRAO1(CLK_IFRAO_DPMAIF, "ifrao_dpmaif",
			"dpmaif_main_ck"/* parent */, 26),
	/* IFRAO2 */
	GATE_IFRAO2_I(CLK_IFRAO_EIP97, "ifrao_eip97",
			"eip97_ck"/* parent */, 0),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA0, "ifrao_cldma0",
			"axi_ck"/* parent */, 1),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA1, "ifrao_cldma1",
			"axi_ck"/* parent */, 2),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA2, "ifrao_cldma2",
			"axi_ck"/* parent */, 3),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA3, "ifrao_cldma3",
			"axi_ck"/* parent */, 4),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA4, "ifrao_cldma4",
			"axi_ck"/* parent */, 5),
	GATE_IFRAO2_I(CLK_IFRAO_CLDMA5, "ifrao_cldma5",
			"axi_ck"/* parent */, 6),
	GATE_IFRAO2_I(CLK_IFRAO_GCE_AXI, "ifrao_gce_axi",
			"axi_ck"/* parent */, 7),
	GATE_IFRAO2_I(CLK_IFRAO_CQ_DMA, "ifrao_cq_dma",
			"axi_ck"/* parent */, 8),
	GATE_IFRAO2_I(CLK_IFRAO_SRAMROM, "ifrao_sramrom",
			"axi_ck"/* parent */, 10),
	GATE_IFRAO2_I(CLK_IFRAO_CCIF0, "ifrao_ccif0",
			"axi_ck"/* parent */, 12),
	GATE_IFRAO2_I(CLK_IFRAO_CCIF1, "ifrao_ccif1",
			"axi_ck"/* parent */, 13),
	GATE_IFRAO2_I(CLK_IFRAO_CCIF3, "ifrao_ccif3",
			"axi_ck"/* parent */, 14),
	GATE_IFRAO2(CLK_IFRAO_RG_MMW_DPMAIF26M_CK, "ifrao_dpmaif_26m",
			"f26m_ck"/* parent */, 17),
};

//static const struct mtk_clk_desc ifrao_mcd = {
//	.clks = ifrao_clks,
//	.num_clks = CLK_IFRAO_NR_CLK,
//};

static int clk_mt6990_ifrao_probe(struct platform_device *pdev)
{
	struct clk_hw_onecell_data *clk_data;
	int r;
	struct device_node *node = pdev->dev.of_node;

#if MT_CCF_BRINGUP
	pr_notice("%s init begin\n", __func__);
#endif

	clk_data = mtk_alloc_clk_data(CLK_IFRAO_NR_CLK);

	mtk_clk_register_gates(&pdev->dev, node, ifrao_clks,
				ARRAY_SIZE(ifrao_clks), clk_data);

	r = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);

//	r = mtk_clk_simple_probe(pdev);
	if (r)
		dev_err(&pdev->dev,
			"could not register clock provider: %s: %d\n",
			pdev->name, r);

#if MT_CCF_BRINGUP
	pr_notice("%s init end\n", __func__);
#endif

	return r;
}

static const struct of_device_id of_match_clk_mt6990_ifrao[] = {
	{
		.compatible = "mediatek,mt6990-infracfg_ao",
//		.data = &ifrao_mcd,
	},
	{
		.compatible = "mediatek,mt6990-infracfg-ao",
//		.data = &ifrao_mcd,
	},
	{}
};

static struct platform_driver clk_mt6990_ifrao_drv = {
	.probe = clk_mt6990_ifrao_probe,
	.driver = {
		.name = "clk-mt6990-ifrao",
		.of_match_table = of_match_clk_mt6990_ifrao,
	},
};

static int __init clk_mt6990_ifrao_init(void)
{
	return platform_driver_register(&clk_mt6990_ifrao_drv);
}

static void __exit clk_mt6990_ifrao_exit(void)
{
	platform_driver_unregister(&clk_mt6990_ifrao_drv);
}

arch_initcall(clk_mt6990_ifrao_init);
module_exit(clk_mt6990_ifrao_exit);
MODULE_LICENSE("GPL");
