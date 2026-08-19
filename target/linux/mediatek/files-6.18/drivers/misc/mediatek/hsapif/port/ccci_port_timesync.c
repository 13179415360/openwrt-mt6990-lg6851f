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

#define TAG "timesync"

static inline int port_timesync_nonblock_read(struct port_t *port)
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

static inline int port_timesync_copy_data_to_user(struct port_t *port,
		char *buf, size_t len)
{
	int read_len = 0;
	ccci_kmem_data_t *data;

	if (port == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL port from user read\n",
			__func__, port->name);
		return -EINVAL;
	}

	if (buf == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL buffer from :%s\n",
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
		"[%s] data: %p; data->off_data: %p; org_data: %p; buf: %p; len: %ld, data_len: %d\n",
		__func__, data, data->off_data, data->org_data,
		buf, len, data->data_len);

	if (read_len > len)
		read_len = len;

	/* copy to user */
	memcpy(buf, data->off_data, read_len);
	data->off_data += read_len;
	data->data_len -= read_len;

	/* free request */
	if (data->data_len <= 0) {
		ext_ccci_data_free(data);
		ext_ccci_list_del_first(&port->rx_data_list);
	}

	return read_len;
}

int port_timesync_read_data(char *buf, size_t len)
{
	struct port_t *port;
	int ret = 0;

	port = ext_ccci_get_port_from_minor(CCCI_PORT_TIMESYNC_RX);

	if (buf == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL buffer from :%s\n",
			__func__, port->name);
		return -EINVAL;
	}

	/* get incoming request */
	ret = port_timesync_nonblock_read(port);
	if (ret)
		return ret;

	ret = port_timesync_copy_data_to_user(port, buf, len);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name = %s; ret = %d\n",
		__func__, port->name, ret);

	return ret;
}
EXPORT_SYMBOL(port_timesync_read_data);

int port_timesync_data_recv(struct port_t *port, void *data)
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

int port_timesync_write_data(const char *buf, size_t len)
{
	struct ccci_header *ccci_h = NULL;
	size_t actual_len = 0, alloc_size = 0;
	int ret = 0, header_len = 0;
	void *data = NULL;
	ccci_send_data_t send_data;

	if (buf == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL buffer from user\n",
			__func__);
		return -EINVAL;
	}

	header_len = sizeof(struct ccci_header);
	actual_len = len > CCCI_MTU ? CCCI_MTU : len;
	alloc_size = actual_len + header_len;

	send_data.data = ext_ccci_data_alloc(GFP_KERNEL, alloc_size + CCCI_ADDRESS_ALIGN_LEN);
	if (!send_data.data)
		return -ENOMEM;

	data = send_data.data->off_data;
	ccci_h = (struct ccci_header *)data;
	ccci_h->data[0] = 0;
	ccci_h->data[1] = alloc_size;
	ccci_h->channel = CCCI_PORT_TIMESYNC_TX;
	ccci_h->assert_bit = 0;
	ccci_h->reserved = 0;
	data += header_len;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data[1]: %d, actual_count: %ld; header_len: %d\n",
		__func__, ccci_h->data[1], actual_len, header_len);

	memcpy(data, buf, len);
	send_data.hif_id = HIF_ID_CLDMA;
	send_data.qno = TIMESYNC_TX_Q_NUM;
	send_data.data->data_len = alloc_size;
	send_data.blocking = 1;

	ret = ccci_msg_send_to_one(send_data.hif_id + CCCI_CLDMA_BASE_ID,
			1 << send_data.hif_id, &send_data);

	if (ret < 0) {
		ext_ccci_data_free(send_data.data);

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: %d\n",
			__func__, ret);
	}

	return ret ? ret : actual_len;
}
EXPORT_SYMBOL(port_timesync_write_data);

static int port_timesync_init(struct port_t *port)
{
	int ret = 0;

	port->rx_length_th = MAX_QUEUE_LENGTH;
	port->flags |= PORT_F_ADJUST_HEADER;

	//if (port->flags & PORT_F_WITH_CHAR_NODE)
	//	ret = ext_ccci_dev_create(port, &port_ctrl_fops);

	CCCI_INIT_LOG(-1, TAG,
		"[%s] ret: %d;\n", __func__, ret);

	return ret;
}

struct port_ops port_timesync_ops = {
	.init = &port_timesync_init,
	.recv_data = &port_timesync_data_recv,
};
