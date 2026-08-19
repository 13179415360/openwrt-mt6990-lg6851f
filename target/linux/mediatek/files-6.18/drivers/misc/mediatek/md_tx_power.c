// SPDX-License-Identifier: GPL-2.0
/*
 * LG6851F modem TX-power control contract.
 *
 * The stock netagent writes "<pa_id> <power>" to ql_md_tx_pwr.  Keep this
 * driver deliberately small: the board only needs the two DT-described PA
 * controls and the existing CCCI throttling transport, not the complete
 * vendor thermal framework.
 */

#include <linux/device.h>
#include <linux/bits.h>
#include <linux/limits.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>

#include "include/mt-plat/mtk_ccci_common.h"

#define TMC_CTRL_CMD_TX_POWER			10U
#define TMC_TX_PWR_REDUCE_OTHER_MAX_TX_EVENT	3U
#define TMC_TX_PWR_REDUCE_NR_MAX_TX_EVENT	4U
#define LG6851F_PA_COUNT			2U

static ssize_t ql_md_tx_pwr_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned int pa_id, power, event, msg;
	int ret;

	if (sscanf(buf, "%u %u", &pa_id, &power) != 2)
		return -EINVAL;
	if (pa_id >= LG6851F_PA_COUNT || power > U8_MAX)
		return -ERANGE;

	event = pa_id ? TMC_TX_PWR_REDUCE_NR_MAX_TX_EVENT :
			TMC_TX_PWR_REDUCE_OTHER_MAX_TX_EVENT;
	msg = TMC_CTRL_CMD_TX_POWER | (event << 16) | (power << 24);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1, ID_THROTTLING_CFG,
					     (char *)&msg, sizeof(msg));
	if (ret)
		return ret < 0 ? ret : -EAGAIN;

	dev_info(dev, "pa_id:%u pwr:%u done\n", pa_id, power);
	return count;
}
static DEVICE_ATTR_WO(ql_md_tx_pwr);

static struct attribute *lg6851f_md_tx_power_attrs[] = {
	&dev_attr_ql_md_tx_pwr.attr,
	NULL,
};

static const struct attribute_group lg6851f_md_tx_power_group = {
	.attrs = lg6851f_md_tx_power_attrs,
};

static int lg6851f_md_tx_power_probe(struct platform_device *pdev)
{
	struct device_node *child;
	unsigned int id;
	unsigned int found = 0;
	int ret;

	for_each_available_child_of_node(pdev->dev.of_node, child) {
		if (of_property_read_u32(child, "id", &id) ||
		    id >= LG6851F_PA_COUNT)
			continue;
		found |= BIT(id);
		dev_info(&pdev->dev, "register %pOFn done, id=%u\n", child, id);
	}

	if (found != GENMASK(LG6851F_PA_COUNT - 1, 0)) {
		dev_err(&pdev->dev, "missing PA controls (mask %#x)\n", found);
		return -EINVAL;
	}

	ret = devm_device_add_group(&pdev->dev, &lg6851f_md_tx_power_group);
	if (ret)
		dev_err(&pdev->dev, "failed to create ql_md_tx_pwr: %d\n", ret);

	return ret;
}

static const struct of_device_id lg6851f_md_tx_power_of_match[] = {
	{ .compatible = "mediatek,md-cooler-tx-pwr" },
	{ }
};
MODULE_DEVICE_TABLE(of, lg6851f_md_tx_power_of_match);

static struct platform_driver lg6851f_md_tx_power_driver = {
	.probe = lg6851f_md_tx_power_probe,
	.driver = {
		.name = "mtk-md-cooling-tx-pwr",
		.of_match_table = lg6851f_md_tx_power_of_match,
	},
};
module_platform_driver(lg6851f_md_tx_power_driver);

MODULE_DESCRIPTION("LG6851F modem TX-power control interface");
MODULE_LICENSE("GPL");
