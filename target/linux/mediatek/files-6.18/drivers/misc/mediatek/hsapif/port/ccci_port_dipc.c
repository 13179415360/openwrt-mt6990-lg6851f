// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/cdev.h>
#include <linux/ktime.h>

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_ioctrl_def.h"
#include "ccci_msg_center.h"
#include "ccci_state_mgr.h"
#include "ccci_comm_config.h"
#include "ccci_fsm_cldma.h"

#define TAG "dipc"

#define MAX_PORT_NUM 20


static spinlock_t port_dipc_data_lock;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
static char s_port_enum_msg_datas[sizeof(struct ccci_header) +
                                  sizeof(struct ctrl_msg_h) +
                                  sizeof(cldma_port_enum_msg_t) +
                                  MAX_PORT_NUM * sizeof(cldma_port_info_t)];
static cldma_port_enum_msg_t *s_port_enum_msg =
		(cldma_port_enum_msg_t *)(s_port_enum_msg_datas + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));

#else
static char s_port_enum_msg_datas[sizeof(struct ccci_header) +
                                  sizeof(cldma_port_enum_msg_t) +
                                  MAX_PORT_NUM * sizeof(cldma_port_info_t)];

static cldma_port_enum_msg_t *s_port_enum_msg =
		(cldma_port_enum_msg_t *)(s_port_enum_msg_datas + sizeof(struct ccci_header));

#endif


static int s_port_enum_msg_len = 0;
static int s_port_enum_msg_data_ready = 0;


int port_dipc_get_ctrl_msg_data(char **data, int *data_len)
{
	unsigned long irqflags;
	int ready = 0;

	spin_lock_irqsave(&port_dipc_data_lock, irqflags);

	*data = s_port_enum_msg_datas;
	*data_len = s_port_enum_msg_len;
	ready = s_port_enum_msg_data_ready;

	spin_unlock_irqrestore(&port_dipc_data_lock, irqflags);

	return ready;
}

static int port_dipc_handle_port_enum(
		struct port_t *port,
		ccci_ioctrl_data_t *ioctrl_data)
{
	int ret = 0;
	unsigned long irqflags;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	char port_enum_msg_datas[sizeof(struct ccci_header) +
									  sizeof(struct ctrl_msg_h) +
									  sizeof(cldma_port_enum_msg_t) +
									  MAX_PORT_NUM * sizeof(cldma_port_info_t)];
	cldma_port_enum_msg_t *port_enum_msg =
			(cldma_port_enum_msg_t *)(port_enum_msg_datas + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
	int port_enum_msg_len = (sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
#else
	char port_enum_msg_datas[sizeof(struct ccci_header) +
									  sizeof(cldma_port_enum_msg_t) +
									  MAX_PORT_NUM * sizeof(cldma_port_info_t)];
	cldma_port_enum_msg_t *port_enum_msg =
			(cldma_port_enum_msg_t *)(port_enum_msg_datas + sizeof(struct ccci_header));
	int port_enum_msg_len = sizeof(struct ccci_header);
#endif

	spin_lock_irqsave(&port_dipc_data_lock, irqflags);
	s_port_enum_msg_data_ready = 0;
	spin_unlock_irqrestore(&port_dipc_data_lock, irqflags);

	if (copy_from_user(port_enum_msg_datas + port_enum_msg_len,
			(void __user *)(ioctrl_data->user_arg),
			sizeof(cldma_port_enum_msg_t))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_from_user() is fail;\n", __func__);
		return -1;
	}

	port_enum_msg_len += sizeof(cldma_port_enum_msg_t);

	if (port_enum_msg->port_count > MAX_PORT_NUM) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: port count: %d is too many!\n",
			__func__, port_enum_msg->port_count);
		return -1;
	}

	if (copy_from_user(port_enum_msg_datas + port_enum_msg_len,
			(void __user *)(ioctrl_data->user_arg + sizeof(cldma_port_enum_msg_t)),
			port_enum_msg->port_count * sizeof(cldma_port_info_t))) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_from_user() is fail;\n", __func__);
		return -1;
	}

	port_enum_msg_len += (port_enum_msg->port_count * sizeof(cldma_port_info_t));

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] port_count: %u; port_enum_msg_len: %d/%lu\n",
		__func__,
		port_enum_msg->port_count,
		port_enum_msg_len,
		sizeof(port_enum_msg_datas));

	/* print port enum content */
	{
		int i = 0;

		CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] head: %X; count: %d; version: %d; tail: %X\n",
			__func__,
			port_enum_msg->head_pattern,
			port_enum_msg->port_count,
			port_enum_msg->version,
			port_enum_msg->tail_pattern);

		for (; i<port_enum_msg->port_count; i++) {
			CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] ch_id: %X; en_flag: %d; reserve: %X\n",
				__func__,
				port_enum_msg->ports[i].ch_id,
				port_enum_msg->ports[i].en_flag,
				port_enum_msg->ports[i].reserve);
		}
	}

	spin_lock_irqsave(&port_dipc_data_lock, irqflags);
	s_port_enum_msg_len = port_enum_msg_len;
	memcpy(s_port_enum_msg_datas, port_enum_msg_datas, sizeof(port_enum_msg_datas));
	s_port_enum_msg_data_ready = 1;
	spin_unlock_irqrestore(&port_dipc_data_lock, irqflags);

	if (ccci_msg_send_to_one(CCCI_DIPC_PORT_CFG_ID, CCCI_NONE_SUB_ID, port_enum_msg) < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] send error: msg: %d\n",
			__func__, CCCI_DIPC_PORT_CFG_ID);
	}

#ifdef __SUPPORT_ASYNC_HS_FLOW__
	ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_SEND_HS2_OR_CTRL_MSG, NULL);
#endif
	return ret;
}

static int port_dipc_ioctrl_handle(
		int           msg_id,
		unsigned int  sub_id,
		void         *msg_data,
		void         *my_data)
{
	struct port_t *port = (struct port_t *)my_data;
	ccci_ioctrl_data_t *ioctrl_data = (ccci_ioctrl_data_t *)msg_data;
	int ret = 0;

	if (sub_id == CLDMA_IOC_SET_PORT_ENUM_MSG)
		ret = port_dipc_handle_port_enum(port, ioctrl_data);



	return ret;
}

static const struct file_operations port_dipc_fops = {
	.owner          = THIS_MODULE,
	.open           = &ext_ccci_dev_open,
	.release        = &ext_ccci_dev_close,
	.unlocked_ioctl = &ext_ccci_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = &ext_ccci_dev_compat_ioctl,
#endif
};

static int port_dipc_init(struct port_t *port)
{
	int ret = 0;

	spin_lock_init(&port_dipc_data_lock);

	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->flags |= PORT_F_ADJUST_HEADER;

	if (port->flags & PORT_F_WITH_CHAR_NODE)
		ret = ext_ccci_dev_create(port, &port_dipc_fops);

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] ret: %d;\n", __func__, ret);

	if (ccci_msg_register(CCCI_IOCTRL_ID,
			CLDMA_IOC_SET_PORT_ENUM_MSG,
			port, &port_dipc_ioctrl_handle) < 0)
		return -1;

	return ret;
}

struct port_ops port_dipc_ops = {
	.init = &port_dipc_init,

};

