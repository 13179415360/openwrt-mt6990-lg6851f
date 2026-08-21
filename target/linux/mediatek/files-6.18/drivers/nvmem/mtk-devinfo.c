// SPDX-License-Identifier: GPL-2.0
/*
 * MediaTek bootloader devinfo NVMEM provider
 *
 * MT6990 firmware passes efuse calibration words through
 * /chosen/atag,devinfo instead of exposing the raw efuse MMIO contents.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

struct mtk_devinfo_priv {
	u32 *data;
	size_t size;
};

struct mtk_devinfo_tag {
	u32 data_size;
	u32 data[];
};

static int mtk_devinfo_read(void *context, unsigned int offset,
			    void *val, size_t bytes)
{
	struct mtk_devinfo_priv *priv = context;

	if (offset > priv->size || bytes > priv->size - offset)
		return -EINVAL;

	memcpy(val, (u8 *)priv->data + offset, bytes);
	return 0;
}

static int mtk_devinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *chosen;
	const struct mtk_devinfo_tag *tag;
	struct mtk_devinfo_priv *priv;
	struct nvmem_config config = {};
	struct nvmem_device *nvmem;
	size_t size;
	int len;

	chosen = of_find_node_by_path("/chosen");
	if (!chosen)
		chosen = of_find_node_by_path("/chosen@0");
	if (!chosen)
		return dev_err_probe(dev, -ENODEV, "chosen node not found\n");

	tag = of_get_property(chosen, "atag,devinfo", &len);
	if (!tag || len < sizeof(tag->data_size)) {
		of_node_put(chosen);
		return dev_err_probe(dev, -ENODATA,
				     "atag,devinfo not found\n");
	}

	/* The vendor ATAG is native little-endian data supplied by LK. */
	if (!tag->data_size || tag->data_size > 300 ||
	    tag->data_size > (len - sizeof(tag->data_size)) / sizeof(u32)) {
		of_node_put(chosen);
		return dev_err_probe(dev, -EINVAL,
				     "invalid atag,devinfo size %u (property %d)\n",
				     tag->data_size, len);
	}

	size = tag->data_size * sizeof(u32);
	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		of_node_put(chosen);
		return -ENOMEM;
	}

	priv->data = devm_kmemdup(dev, tag->data, size, GFP_KERNEL);
	of_node_put(chosen);
	if (!priv->data)
		return -ENOMEM;
	priv->size = size;

	config.name = "mtk-devinfo";
	config.dev = dev;
	config.owner = THIS_MODULE;
	config.read_only = true;
	config.root_only = true;
	config.stride = sizeof(u32);
	config.word_size = sizeof(u32);
	config.size = size;
	config.reg_read = mtk_devinfo_read;
	config.priv = priv;
	config.add_legacy_fixed_of_cells = true;

	nvmem = devm_nvmem_register(dev, &config);
	if (IS_ERR(nvmem))
		return dev_err_probe(dev, PTR_ERR(nvmem),
				     "failed to register NVMEM provider\n");

	dev_info(dev, "registered %zu-byte bootloader devinfo NVMEM\n", size);
	return 0;
}

static const struct of_device_id mtk_devinfo_of_match[] = {
	{ .compatible = "mediatek,devinfo" },
	{ }
};
MODULE_DEVICE_TABLE(of, mtk_devinfo_of_match);

static struct platform_driver mtk_devinfo_driver = {
	.probe = mtk_devinfo_probe,
	.driver = {
		.name = "mediatek-devinfo",
		.of_match_table = mtk_devinfo_of_match,
	},
};
module_platform_driver(mtk_devinfo_driver);

MODULE_AUTHOR("MediaTek Inc.; OpenWrt MT6990 port");
MODULE_DESCRIPTION("MediaTek bootloader devinfo NVMEM provider");
MODULE_LICENSE("GPL");
