// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>       /* needed by device_* */
#include "medmcu_scpctl.h"
#include "medmcu_ipi_pin.h"
#include "medmcu_mbox_layout.h"

/*
 * A device node to send commands to scp wit unified interface
 * @magic:	should be 666
 * @type:	a class for different types of commands
 * @op:		the operation specified in a command type
 * @return:	0 if success, -EINVAL if wrong value of number
 *		of parameters
 */
static ssize_t scpctl_store(struct device *kobj
		, struct device_attribute *attr, const char *buf, size_t n)
{
	int ret;
	int magic, type, op;
	struct scpctl_cmd_s cmd;
	char *prompt = "[SCPCTL]:";

	if (sscanf(buf, "%d %d %d", &magic, &type, &op) != 3)
		return -EINVAL;
	pr_notice("%s %d %d %d\n", prompt, magic, type, op);

	if (magic != 666)
		return -EINVAL;

	cmd.type = type;
	cmd.op = op;

	ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_SCPCTL_1, 0, &cmd,
			   PIN_OUT_SIZE_SCPCTL_1, 0);

	if (ret != IPI_ACTION_DONE)
		goto _err;

	return n;

_err:
	pr_notice("%s failed, %d\n", prompt, ret);
	return -EIO;
}
DEVICE_ATTR_WO(scpctl);


