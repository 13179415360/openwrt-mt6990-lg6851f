/* SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2018-2019 MediaTek Inc.
 *
 * Author: Kurt Yang <kurt.yang@mediatek.com>
 *
 */
#ifndef MTK_FE_H
#define MTK_FE_H

#include <linux/list.h>
#include <linux/platform_device.h>

/*
 * struct mtk_fe_token - a token for user to represent a MTK frame engine
 *						 instance.
 *
 * @user: the name of the user, for debuging.
 * @list: list to link with all other tokens.
 * @dev:  pointer to the real frame engine device.
 * @priv: private structure for fe driver usage.
 */
struct mtk_fe_token {
	char user[10];
	struct list_head list;
	struct device *dev;
	void *priv;
};

/*
 * Acquire a frame engine instance.
 * It will cause the reference count adding on frame engine resource.
 *
 * @user: the name of the user, for debuging.
 * @fe:   the pointer to get struct mtk_fe_token object.
 * @return: 0 is ok.
 */
int acquire_FE(char *user, struct mtk_fe_token **fe);

/*
 * Release the frame engine instance.
 * It will decreasing the reference count on frame engine resource.
 *
 * @fe:     the token to be released.
 * @return: 0 is ok.
 */
int release_FE(struct mtk_fe_token *fe);

/*
 * Enable/disable the clocks controlled in frame engine.
 *
 * @fe:     the token to be released.
 * @suspend:	disable for suspend or enable for resume.
 * @return: 0 is ok.
 */
int mtk_fe_pm_clk(struct mtk_fe_token *fe, bool suspend);

extern int mtk_fe_dma_probe(struct platform_device *pdev);
extern int mtk_fe_dma_pm_handle(bool suspend);
extern int mtk_fe_dma_dbg(char *dma, char *argv[]);

#endif /* MTK_FE_H */
