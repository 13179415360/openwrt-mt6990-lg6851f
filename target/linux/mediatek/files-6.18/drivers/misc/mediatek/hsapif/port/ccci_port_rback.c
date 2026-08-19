// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */


#include <linux/cdev.h>

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_kmem.h"
#include "ccci_core.h"
#include "ccci_comm_config.h"
#include "ccci_msg_id.h"
#include "ccci_msg_data.h"
#include "ccci_msg_center.h"

#define TAG "rback"

static struct task_struct *port_rback_thread;

static inline int port_rback_nonblock_read(struct port_t *port)
{
	int ret = 0;

	if (ext_ccci_get_list_node_count(&port->rx_data_list) == 0) {
		CCCI_DEBUG_LOG(-1, TAG,
				"[%s] name: %s, qlen=%d\n",
				__func__, port->name,
				ext_ccci_get_list_node_count(&port->rx_data_list));

		spin_lock_irq(&port->rx_wq.lock);

		ret = wait_event_interruptible_locked_irq(port->rx_wq,
					ext_ccci_get_list_node_count(&port->rx_data_list));

		spin_unlock_irq(&port->rx_wq.lock);

		CCCI_DEBUG_LOG(-1, TAG,
				"[%s] name: %s, qlen=%d, ret=%d\n",
				__func__, port->name,
				ext_ccci_get_list_node_count(&port->rx_data_list),
				ret);

		if (ret == -ERESTARTSYS)
			return -EINTR;
		else if (ret)
			CCCI_ERROR_LOG(-1, TAG,
					"[%s] error, port: %s; ret: %d\n",
					__func__, port->name, ret);
	}

	return ret;
}

int port_rback_write_data_back(struct port_t *port)
{
	int ret = 0;
	int read_len = 0;
	ccci_kmem_data_t *data;
	ccci_send_data_t send_data;
	struct ccci_header *ccci_h = NULL;

	if (port == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL port from user read\n",
			__func__, port->name);
		return -EINVAL;
	}

	data = ext_ccci_list_get_first(&port->rx_data_list);
	if (data == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL first node from :%s\n",
			__func__, port->name);
		return -EFAULT;
	}

	read_len = data->data_len;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data: %p; data->off_data: %p; org_data: %p; data_len: %d\n",
		__func__, data, data->off_data, data->org_data,
		data->data_len);

	send_data.data = data;
	ccci_h = (struct ccci_header *)(send_data.data->off_data);
	ccci_h->channel = port->tx_ch;

	send_data.hif_id = port->hif_id;
	send_data.qno = port->txq_index;
	send_data.blocking = 1;

	ret = ccci_msg_send_to_one(send_data.hif_id + CCCI_CLDMA_BASE_ID,
			1 << send_data.hif_id, &send_data);

	/* free request */
	if (ret < 0) {
		ext_ccci_data_free(data);
	}
	ext_ccci_list_del_first(&port->rx_data_list);

	return ret ? ret : read_len;
}

int port_rback_read_data(void)
{
	struct port_t *port;
	int ret = 0;

	port = ext_ccci_get_port_from_minor(CCCI_PORT_RBACK_RX);

	/* get incoming request */
	ret = port_rback_nonblock_read(port);
	if (ret)
		return ret;

	ret = port_rback_write_data_back(port);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name = %s; ret = %d\n",
		__func__, port->name, ret);

	return ret;
}

int port_rback_data_recv(struct port_t *port, void *data)
{
	int ret = 0;
	ext_ccci_node_t *pnode;

	if (port == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL port from data recv\n",
			__func__);
		return -EINVAL;
	}

	if (data == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL data from data recv\n",
			__func__);
		return -EINVAL;
	}

	pnode = ext_ccci_list_add_node(&port->rx_data_list,
					data, GFP_ATOMIC);
	if (!pnode)
		return -ENOMEM;

	if (port->flags & PORT_F_ADJUST_HEADER) {
		((ccci_kmem_data_t *)data)->data_len -= sizeof(struct ccci_header);
		((ccci_kmem_data_t *)data)->off_data += sizeof(struct ccci_header);
	}

	port->rx_pkg_cnt++;

	ext_ccci_port_recv_wakeup(port);

	return 0;
}

static int port_rback_thread_func(
		void *arg)
{
	int ret;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] start running.\n",
		__func__);

	while (1) {
		ret = port_rback_read_data();

		if (ret < 0) {
			CCCI_ERROR_LOG(-1, TAG,
					"[%s] error, ret: %d\n",
					__func__, ret);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int port_rback_create_thread(void)
{
	port_rback_thread = kthread_run(
			port_rback_thread_func,
			NULL,
			"cldma_rback_t");

	if (port_rback_thread == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kthread_run() fail!\n",
			__func__);

		return -1;
	}

	return 0;
}

static int port_rback_init(struct port_t *port)
{
	int ret = 0;

	port->rx_length_th = MAX_QUEUE_LENGTH;
	//port->flags |= PORT_F_ADJUST_HEADER;

	//if (port->flags & PORT_F_WITH_CHAR_NODE)
	//	ret = ext_ccci_dev_create(port, &port_ctrl_fops);

	if (strncmp(port->name, "ccci_hsapif_rback", sizeof("ccci_hsapif_rback")) == 0) {
		CCCI_INIT_LOG(-1, TAG, "[%s] %s create thread\n", __func__, port->name);
		ret = port_rback_create_thread();
	}

	CCCI_INIT_LOG(-1, TAG,
		"[%s] %s ret: %d\n", __func__, port->name, ret);

	return ret;
}

struct port_ops port_rback_ops = {
	.init = &port_rback_init,
	.recv_data = &port_rback_data_recv,
};
