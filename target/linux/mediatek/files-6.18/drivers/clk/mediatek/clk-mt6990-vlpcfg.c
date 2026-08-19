// SPDX-License-Identifier: GPL-2.0
/* MT6990 VLP clock gates, ported from the vendor SDK. */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "clk-mtk.h"
#include "clk-gate.h"

#include <dt-bindings/clock/mt6990-clk.h>

static const struct mtk_gate_regs vlpcfg0_cg_regs = {
	.set_ofs = 0x118,
	.clr_ofs = 0x118,
	.sta_ofs = 0x118,
};

static const struct mtk_gate_regs vlpcfg1_cg_regs = {
	.set_ofs = 0x8,
	.clr_ofs = 0x8,
	.sta_ofs = 0x8,
};

#define GATE_VLPCFG0(_id, _name, _parent, _shift) { \
	.id = _id, .name = _name, .parent_name = _parent, \
	.regs = &vlpcfg0_cg_regs, .shift = _shift, \
	.ops = &mtk_clk_gate_ops_no_setclr, \
}

#define GATE_VLPCFG1_I(_id, _name, _parent, _shift) { \
	.id = _id, .name = _name, .parent_name = _parent, \
	.regs = &vlpcfg1_cg_regs, .shift = _shift, \
	.ops = &mtk_clk_gate_ops_no_setclr_inv, \
}

static const struct mtk_gate vlpcfg_clks[] = {
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_TL_32K, "vlpcfg_pcie_tl_32k",
		       "rtc_ck", 0),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_TL_26M, "vlpcfg_pcie_tl_26m",
		       "f26m_ck", 1),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_TL_96M, "vlpcfg_pcie_tl_96m",
		       "vlp_tl_vlp_ck", 2),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_TL_ULPOSC, "vlpcfg_PCIE_ULPOSC",
		       "vlp_pcie_26m_ck", 3),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_REF_CK_26M, "vlpcfg_pcie_ref_26m",
		       "f26m_ck", 5),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_AXI250, "vlpcfg_pcie_axi250",
		       "pcie_250m_ck", 6),
	GATE_VLPCFG0(CLK_VLPCFG_PCIE_SRAM_26M, "vlpcfg_pcie_sram_26m",
		       "f26m_ck", 7),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_1, "vlpcfg_pwm_cgen_1",
			 "vlp_pwm_vlp_ck", 0),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_2, "vlpcfg_pwm_cgen_2",
			 "vlp_pwm_vlp_ck", 1),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_3, "vlpcfg_pwm_cgen_3",
			 "vlp_pwm_vlp_ck", 2),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_4, "vlpcfg_pwm_cgen_4",
			 "vlp_pwm_vlp_ck", 3),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_5, "vlpcfg_pwm_cgen_5",
			 "vlp_pwm_vlp_ck", 4),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_6, "vlpcfg_pwm_cgen_6",
			 "vlp_pwm_vlp_ck", 5),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_7, "vlpcfg_pwm_cgen_7",
			 "vlp_pwm_vlp_ck", 6),
	GATE_VLPCFG1_I(CLK_VLPCFG_PWM_CK_CGEN_8, "vlpcfg_pwm_cgen_8",
			 "vlp_pwm_vlp_ck", 7),
};

static int clk_mt6990_vlpcfg_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk_hw_onecell_data *clk_data;
	int ret;

	clk_data = mtk_alloc_clk_data(CLK_VLPCFG_NR_CLK);
	mtk_clk_register_gates(&pdev->dev, node, vlpcfg_clks,
			       ARRAY_SIZE(vlpcfg_clks), clk_data);
	ret = of_clk_add_hw_provider(node, of_clk_hw_onecell_get, clk_data);
	if (ret)
		dev_err(&pdev->dev, "could not register clock provider: %d\n", ret);
	return ret;
}

static const struct of_device_id clk_mt6990_vlpcfg_of_match[] = {
	{ .compatible = "mediatek,mt6990-vlpcfg" },
	{ }
};

static struct platform_driver clk_mt6990_vlpcfg_driver = {
	.probe = clk_mt6990_vlpcfg_probe,
	.driver = {
		.name = "clk-mt6990-vlpcfg",
		.of_match_table = clk_mt6990_vlpcfg_of_match,
	},
};

static int __init clk_mt6990_vlpcfg_init(void)
{
	return platform_driver_register(&clk_mt6990_vlpcfg_driver);
}
arch_initcall(clk_mt6990_vlpcfg_init);

MODULE_LICENSE("GPL");
