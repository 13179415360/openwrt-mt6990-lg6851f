// SPDX-License-Identifier: GPL-2.0-only
/* MediaTek MT6990 SGMII analog PHY */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/iopoll.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/regmap.h>

#define MT6990_SGMII_PLL_CON0	0x008
#define MT6990_SGMII_PLL_EN	BIT(31)
#define MT6990_PWR_STATUS	0xf38
#define MT6990_PWR_STATUS_2ND	0xf3c
#define MT6990_SGMII_TOP0_PWR_CON 0xe8c
#define MT6990_SGMII_TOP1_PWR_CON 0xe90
#define MT6990_PWR_RST_B	BIT(0)
#define MT6990_PWR_ISO		BIT(1)
#define MT6990_PWR_ON		BIT(2)
#define MT6990_PWR_ON_2ND	BIT(3)
#define MT6990_PWR_CLK_DIS	BIT(4)
#define MT6990_SRAM_PDN		BIT(8)

struct mt6990_sgmii_phy {
	void __iomem *base;
	struct regmap *pll;
	struct regmap *spm;
	struct device *top_pd;
	u32 top_ctl;
	u32 top_sta_mask;
	bool configured;
};

static int mt6990_sgmii_top_cycle(struct mt6990_sgmii_phy *priv)
{
	u32 val;
	int ret;

	/* Exact SDK SGMII_TOP MTCMOS off/on ordering. Bus-protect callbacks for
	 * these domains are empty in the MT6990 vendor implementation.
	 */
	regmap_update_bits(priv->spm, priv->top_ctl, MT6990_SRAM_PDN,
			   MT6990_SRAM_PDN);
	regmap_update_bits(priv->spm, priv->top_ctl,
			   MT6990_PWR_ISO | MT6990_PWR_CLK_DIS,
			   MT6990_PWR_ISO | MT6990_PWR_CLK_DIS);
	regmap_update_bits(priv->spm, priv->top_ctl,
			   MT6990_PWR_RST_B | MT6990_PWR_ON | MT6990_PWR_ON_2ND, 0);
	ret = regmap_read_poll_timeout(priv->spm, MT6990_PWR_STATUS, val,
				       !(val & priv->top_sta_mask), 10, 1000000);
	if (ret)
		return ret;
	ret = regmap_read_poll_timeout(priv->spm, MT6990_PWR_STATUS_2ND, val,
				       !(val & priv->top_sta_mask), 10, 1000000);
	if (ret)
		return ret;

	regmap_update_bits(priv->spm, priv->top_ctl,
			   MT6990_PWR_ON | MT6990_PWR_ON_2ND,
			   MT6990_PWR_ON | MT6990_PWR_ON_2ND);
	ret = regmap_read_poll_timeout(priv->spm, MT6990_PWR_STATUS, val,
				       (val & priv->top_sta_mask) == priv->top_sta_mask,
				       10, 1000000);
	if (ret)
		return ret;
	ret = regmap_read_poll_timeout(priv->spm, MT6990_PWR_STATUS_2ND, val,
				       (val & priv->top_sta_mask) == priv->top_sta_mask,
				       10, 1000000);
	if (ret)
		return ret;

	regmap_update_bits(priv->spm, priv->top_ctl,
			   MT6990_PWR_CLK_DIS | MT6990_PWR_ISO, 0);
	regmap_update_bits(priv->spm, priv->top_ctl, MT6990_PWR_RST_B,
			   MT6990_PWR_RST_B);
	regmap_update_bits(priv->spm, priv->top_ctl, MT6990_SRAM_PDN, 0);
	return 0;
}

static void mt6990_sgmii_runtime_disable(void *data)
{
	struct mt6990_sgmii_phy *priv = data;

	pm_runtime_put_sync(priv->top_pd);
	pm_runtime_disable(priv->top_pd);
}

static void mt6990_sgmii_rmw(struct mt6990_sgmii_phy *priv, u32 reg,
			     u32 mask, u32 val)
{
	u32 tmp = readl(priv->base + reg);

	tmp &= ~mask;
	tmp |= val & mask;
	writel(tmp, priv->base + reg);
}

static int mt6990_sgmii_phy_configure(struct phy *phy)
{
	struct mt6990_sgmii_phy *priv = phy_get_drvdata(phy);

	/* The SDK requires the shared PLL enable to remain asserted for the
	 * complete SGMII active lifetime.  Reassert it even when the analog
	 * table is already configured: another consumer of the shared AO
	 * register bank must not leave a live port without its 3.125G clock.
	 */
	regmap_update_bits(priv->pll, MT6990_SGMII_PLL_CON0,
			   MT6990_SGMII_PLL_EN, MT6990_SGMII_PLL_EN);

	if (priv->configured)
		return 0;

	dev_info(&phy->dev, "applying MT6990 SGMII Gen2 SerDes settings\n");

	/* The analog XFI windows discard accesses until the shared PLL is on. */
	regmap_update_bits(priv->pll, MT6990_SGMII_PLL_CON0,
			   MT6990_SGMII_PLL_EN, MT6990_SGMII_PLL_EN);

	/* Exact SDK mtk_sgmii_phy_gen2() sequence selected by the MT6990
	 * CONFIG_MEDIATEK_NETSYS_V2=y kernel configuration.
	 */
	mt6990_sgmii_rmw(priv, 0x00f4, BIT(0) | BIT(5), BIT(0) | BIT(5));
	mt6990_sgmii_rmw(priv, 0x0030, GENMASK(31, 10), 3 << 10);
	mt6990_sgmii_rmw(priv, 0x00f8, GENMASK(8, 0), 0x09c);
	mt6990_sgmii_rmw(priv, 0x00f8, GENMASK(25, 16), 0x09c << 16);
	mt6990_sgmii_rmw(priv, 0x0070, GENMASK(26, 24), 2 << 24);
	mt6990_sgmii_rmw(priv, 0x3080, GENMASK(5, 4), 2 << 4);
	mt6990_sgmii_rmw(priv, 0x3028, GENMASK(17, 0), 0x08a01);
	mt6990_sgmii_rmw(priv, 0x302c, GENMASK(17, 0), 0x0a884);
	mt6990_sgmii_rmw(priv, 0x3024, GENMASK(20, 8), 0x0830 << 8);
	mt6990_sgmii_rmw(priv, 0x3014, GENMASK(15, 8), 0x50 << 8);
	mt6990_sgmii_rmw(priv, 0x3018, GENMASK(15, 8), 0x28 << 8);
	mt6990_sgmii_rmw(priv, 0x301c, GENMASK(15, 8), 0x50 << 8);
	mt6990_sgmii_rmw(priv, 0x00ec, GENMASK(28, 17), 0x249 << 17);
	mt6990_sgmii_rmw(priv, 0x2230, GENMASK(23, 20), 0);
	writel(0x14000000, priv->base + 0x223c);
	writel(0x14000000, priv->base + 0x2240);
	writel(0x14000000, priv->base + 0x2244);
	writel(0x14000000, priv->base + 0x2248);
	mt6990_sgmii_rmw(priv, 0x50f8, GENMASK(31, 16), 0x0055 << 16);
	mt6990_sgmii_rmw(priv, 0x30f8, GENMASK(31, 20), 0xfff << 20);
	mt6990_sgmii_rmw(priv, 0x90d0, GENMASK(3, 0), 7);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(15) | BIT(14), BIT(15) | BIT(14));
	udelay(150);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(0) | BIT(8), BIT(0) | BIT(8));
	mt6990_sgmii_rmw(priv, 0x0070, GENMASK(13, 10), 0);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(4), BIT(4));
	udelay(10);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(4), 0);
	udelay(10);
	mt6990_sgmii_rmw(priv, 0x0070, GENMASK(17, 16), 1 << 16);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(4), BIT(4));
	udelay(10);
	mt6990_sgmii_rmw(priv, 0x0070, BIT(4), 0);
	udelay(1000);
	mt6990_sgmii_rmw(priv, 0x3080, GENMASK(5, 4), 3 << 4);
	mt6990_sgmii_rmw(priv, 0x00f4, BIT(5), 0);
	mt6990_sgmii_rmw(priv, 0x3040, GENMASK(29, 28), 3 << 28);

	dev_info(&phy->dev,
		 "MT6990 SDK NETSYS_V2 Gen2 configured: 070=%08x 3028=%08x 3080=%08x 3040=%08x\n",
		 readl(priv->base + 0x0070), readl(priv->base + 0x3028),
		 readl(priv->base + 0x3080), readl(priv->base + 0x3040));
	priv->configured = true;

	return 0;
}

static int mt6990_sgmii_phy_power_on(struct phy *phy)
{
	struct mt6990_sgmii_phy *priv = phy_get_drvdata(phy);

	/* SDK mtk_sgmii_setup_mode_force() enables the shared PLL before any
	 * per-port TOP/PCS operation and keeps it enabled while the link is up.
	 */
	regmap_update_bits(priv->pll, MT6990_SGMII_PLL_CON0,
			   MT6990_SGMII_PLL_EN, MT6990_SGMII_PLL_EN);

	/* The PCS releases SGMSYS_QPHY_PWR_STATE_CTRL later in .pcs_config(). */
	return 0;
}

static int mt6990_sgmii_phy_set_mode(struct phy *phy, enum phy_mode mode,
				     int submode)
{
	if (mode != PHY_MODE_ETHERNET)
		return -EINVAL;

	/* LynxI calls this after clearing SGMII_PHYA_PWD, matching the SDK. */
	return mt6990_sgmii_phy_configure(phy);
}

static int mt6990_sgmii_phy_reset(struct phy *phy)
{
	struct mt6990_sgmii_phy *priv = phy_get_drvdata(phy);
	int ret;

	/* The generic power-domain core owns this device's runtime-PM reference.
	 * Cycling it here underflows the usage count during phylink configure and
	 * races the two ports through their shared parent domain.  The TOP domain
	 * is already on at probe time; invalidate the analog state so set_mode()
	 * reapplies the complete SDK Gen2 table after the PCS force-mode writes.
	 */
	regmap_update_bits(priv->pll, MT6990_SGMII_PLL_CON0,
			   MT6990_SGMII_PLL_EN, MT6990_SGMII_PLL_EN);
	ret = mt6990_sgmii_top_cycle(priv);
	if (ret)
		return ret;

	priv->configured = false;
	return 0;
}

static int mt6990_sgmii_phy_power_off(struct phy *phy)
{
	/* The vendor sequence has no symmetric, validated analog shutdown. */
	return 0;
}

static const struct phy_ops mt6990_sgmii_phy_ops = {
	.power_on = mt6990_sgmii_phy_power_on,
	.power_off = mt6990_sgmii_phy_power_off,
	.reset = mt6990_sgmii_phy_reset,
	.set_mode = mt6990_sgmii_phy_set_mode,
	.owner = THIS_MODULE,
};

static int mt6990_sgmii_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *provider;
	struct mt6990_sgmii_phy *priv;
	struct phy *phy;
	u32 id;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	/* The platform core has already attached the single power-domains
	 * entry to this device before probe. Attaching it again by name returns
	 * -EEXIST and prevents both PHYs, and therefore Ethernet, from probing.
	 * Operate the already attached SGMII TOP domain through this device's
	 * runtime-PM handle.
	 */
	priv->top_pd = &pdev->dev;
	pm_runtime_enable(priv->top_pd);

	ret = pm_runtime_resume_and_get(priv->top_pd);
	if (ret < 0)
		goto err_disable_runtime;

	ret = devm_add_action_or_reset(&pdev->dev, mt6990_sgmii_runtime_disable,
				       priv);
	if (ret)
		return ret;

	priv->pll = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						   "mediatek,sgmii-pll");
	if (IS_ERR(priv->pll))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->pll),
				     "failed to get shared SGMII PLL\n");

	priv->spm = syscon_regmap_lookup_by_phandle(pdev->dev.of_node,
						   "mediatek,spm");
	if (IS_ERR(priv->spm))
		return dev_err_probe(&pdev->dev, PTR_ERR(priv->spm),
				     "failed to get SPM syscon\n");
	if (of_property_read_u32(pdev->dev.of_node, "mediatek,sgmii-id", &id))
		return dev_err_probe(&pdev->dev, -EINVAL, "missing SGMII id\n");
	if (id == 0) {
		priv->top_ctl = MT6990_SGMII_TOP0_PWR_CON;
		priv->top_sta_mask = BIT(27);
	} else if (id == 1) {
		priv->top_ctl = MT6990_SGMII_TOP1_PWR_CON;
		priv->top_sta_mask = BIT(28);
	} else {
		return dev_err_probe(&pdev->dev, -EINVAL, "invalid SGMII id\n");
	}

	phy = devm_phy_create(&pdev->dev, NULL, &mt6990_sgmii_phy_ops);
	if (IS_ERR(phy))
		return PTR_ERR(phy);

	phy_set_drvdata(phy, priv);
	provider = devm_of_phy_provider_register(&pdev->dev,
						 of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(provider);

err_disable_runtime:
	pm_runtime_disable(priv->top_pd);
	return dev_err_probe(&pdev->dev, ret,
			     "failed to enable SGMII TOP power domain\n");
}

static const struct of_device_id mt6990_sgmii_phy_of_match[] = {
	{ .compatible = "mediatek,mt6990-sgmii-phy" },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6990_sgmii_phy_of_match);

static struct platform_driver mt6990_sgmii_phy_driver = {
	.probe = mt6990_sgmii_phy_probe,
	.driver = {
		.name = "mt6990-sgmii-phy",
		.of_match_table = mt6990_sgmii_phy_of_match,
	},
};
module_platform_driver(mt6990_sgmii_phy_driver);

MODULE_DESCRIPTION("MediaTek MT6990 SGMII analog PHY driver");
MODULE_LICENSE("GPL");
