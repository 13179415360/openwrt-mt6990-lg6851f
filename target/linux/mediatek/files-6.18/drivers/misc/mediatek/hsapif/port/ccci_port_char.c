// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/cdev.h>

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"

#define TAG "char"





static const struct file_operations port_char_fops = {
	.owner          = THIS_MODULE,
	.open           = &ext_ccci_dev_open,
	.release        = &ext_ccci_dev_close,
	.read           = &ext_ccci_dev_read,
	.write          = &ext_ccci_dev_write,
	.poll           = &ext_ccci_dev_poll,
	.unlocked_ioctl = &ext_ccci_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = &ext_ccci_dev_compat_ioctl,
#endif
};

static int port_char_init(struct port_t *port)
{
	int ret = 0;


	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->flags |= PORT_F_ADJUST_HEADER;

	if (port->flags & PORT_F_WITH_CHAR_NODE)
		ret = ext_ccci_dev_create(port, &port_char_fops);

	CCCI_INIT_LOG(-1, TAG,
		"[%s] ret: %d;\n", __func__, ret);

	return ret;
}

struct port_ops port_char_ops = {
	.init = &port_char_init,

};

