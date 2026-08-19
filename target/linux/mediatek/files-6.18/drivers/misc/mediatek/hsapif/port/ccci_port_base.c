// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/cdev.h>
#include <linux/poll.h>


#include "ccci_comm_config.h"
#include "ccci_port_base.h"
//#include "ccci_metric.h"
#include "ccci_list.h"
#include "ccci_kmem.h"
#include "ccci_debug.h"
#include "ccci_msg_data.h"
#include "ccci_msg_center.h"
#include "ccci_msg_id.h"
#include "ccci_state_mgr.h"
#include "ccci_config.h"
#include "ccci_ioctrl_def.h"

#define TAG "base"
#define CCCI_DEV_NAME "ccci_base"




static port_base_info_t port_binfo;


port_base_info_t *ext_ccci_get_port_base_info()
{
	return &port_binfo;
}

void ext_ccci_port_base_init(void)
{
	port_binfo.port_count = 0;
	//port_binfo.major = 0;
	//port_binfo.minor_base = 0;
	//dev_class = class_create(THIS_MODULE, "ccci_base_node");
	//ccci_register_base_char_dev();

}

void ext_ccci_port_init(struct port_t *port)
{
	//port->major = port_binfo.major;
	//port->minor_base = port_binfo.minor_base;
	//port->minor = port_binfo.minor_base + port->rx_ch;

	port->tx_busy_count = 0;
	port->rx_busy_count = 0;
	atomic_set(&port->usage_cnt, 0);
	port->host_user_state = HOST_USER_READY;

	init_waitqueue_head(&port->rx_wq);
#if 0
	/*For kernel-4.14: rx_wakelock type is "strcut wakeup_source"*/
	wakeup_source_init(&port->rx_wakelock, port->name);
#endif
	port->rx_wakelock = wakeup_source_register(NULL, port->name);
	if (!port->rx_wakelock) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s %d: cldma init wakeup source fail",
			__func__, __LINE__);
	}

	if (port->ops->init)
		port->ops->init(port);


}
/*
int ext_ccci_dev_init(const struct file_operations *file_ops, char *name, int rx_ch)
{
	struct cdev   *scdev;
	struct device *sdevice;
	struct class  *sclass;
	int ret = 0;
	dev_t dev_n;
	static unsigned int major = 0;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] name: %s; rx_ch: %d; file_ops: %p\n",
		__func__, name, rx_ch, file_ops);

	if (major == 0) {
		ret = alloc_chrdev_region(&dev_n, 0, 1, name);
		major = MAJOR(dev_n);
		dev_n = MKDEV(major, rx_ch);

	} else {
		dev_n = MKDEV(major, rx_ch);
		ret = register_chrdev_region(dev_n, 1, name);

	}

	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc/register chrdev_region, ret=%d\n",
			__func__, ret);

		return ret;
	}

	CCCI_NORMAL_LOG(-1, TAG, "[%s] ret: %d; dev_n: %d; major: %d; minor: %d;\n",
			__func__, ret, dev_n, MAJOR(dev_n), MINOR(dev_n));

	scdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!scdev) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kmalloc cdev fail!!\n",
			__func__);

		return -1;
	}

	cdev_init(scdev, file_ops);
	scdev->owner = THIS_MODULE;

	ret = cdev_add(scdev, dev_n, 1);
	if (unlikely(ret)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: cdev_add, ret=%d\n",
			__func__, ret);

		return ret;
	}

	sclass = class_create(THIS_MODULE, name);
	if (!sclass) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: class_create fail.\n",
			__func__);

		return -1;
	}

	sdevice = device_create(sclass, NULL, dev_n, NULL, name);
	if (!sdevice) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: device_create fail\n",
			__func__);

		return ret;
	}
	return 0;
}
*/

int ext_ccci_dev_create(struct port_t *port,
		const struct file_operations *file_ops)
{
	struct cdev   *scdev;
	struct device *sdevice;
	struct class  *sclass;
	int ret = 0;
	dev_t dev_n = 0;
	static unsigned int major = 0;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] name: %s; tx_ch: %d; rx_ch: %d; file_ops: %p\n",
		__func__, port->name, port->tx_ch, port->rx_ch, file_ops);

	if (major == 0) {
		ret = alloc_chrdev_region(&dev_n, 0, 1, port->name);
		major = MAJOR(dev_n);
		dev_n = MKDEV(major, port->rx_ch);

	} else {
		dev_n = MKDEV(major, port->rx_ch);
		ret = register_chrdev_region(dev_n, 1, port->name);

	}

	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc/register chrdev_region, ret=%d\n",
			__func__, ret);

		return ret;
	}

	CCCI_INIT_LOG(-1, TAG,
		"[%s] ret: %d; dev_n: %d; major: %d; minor: %d;\n",
		__func__, ret, dev_n, MAJOR(dev_n), MINOR(dev_n));

	scdev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
	if (!scdev) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kmalloc cdev fail!!\n",
			__func__);

		return -1;
	}

	cdev_init(scdev, file_ops);
	scdev->owner = THIS_MODULE;

	ret = cdev_add(scdev, dev_n, 1);
	if (unlikely(ret)) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: cdev_add, ret=%d\n",
			__func__, ret);

		return ret;
	}

	sclass = class_create(THIS_MODULE, port->name);
	if (!sclass) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: class_create fail.\n",
			__func__);

		return -1;
	}

	sdevice = device_create(sclass, NULL, dev_n, NULL, port->name);
	if (!sdevice) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: device_create fail\n",
			__func__);

		return ret;
	}
	return 0;
}

int ext_ccci_comm_dev_open(struct port_t *port,
		struct inode *inode, struct file *file)
{
	// check dipc port cfg state
	if (port->en_flag == DIPC_PORT_CFG_DISABLE) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] port %s error with EACCES; dipc port cfg: DISABLE\n",
			__func__, port->name);
		return -EACCES;
	}

	if (atomic_add_return(1, &port->usage_cnt) > 1) {

		atomic_dec(&port->usage_cnt);

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] port %s open fail with EBUSY\n",
			__func__, port->name);

		return -EBUSY;
	}

	file->private_data = port;

	nonseekable_open(inode, file);

	//port_user_register(port);

	return 0;
}

int ext_ccci_dev_open(struct inode *inode,
		struct file *file)
{
	struct port_t *port;

	port = ext_ccci_get_port_from_minor(iminor(inode));
	if (!port)
		return -ENXIO;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] port %s open with flag 0x%X by %s; iminor(inode): %d\n",
		__func__, port->name, file->f_flags,
		current->comm,	iminor(inode));

	if (port->port_dev_open)
		return port->port_dev_open(port, inode, file);

	else
		return ext_ccci_comm_dev_open(port, inode, file);
}

static void ccci_list_clear_node_cb(void *data)
{
	ext_ccci_data_free((ccci_kmem_data_t *)data);
}

int ext_ccci_comm_dev_close(struct port_t *port)
{
	int clear_cnt = 0;

	if (!port) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s]: error: port == NULL\n", __func__);
		return -ENXIO;
	}

	atomic_dec(&port->usage_cnt);

	/* 1. purge Rx request list */
	clear_cnt = ext_ccci_list_clear_node(&port->rx_data_list, ccci_list_clear_node_cb);
	port->rx_drop_cnt += clear_cnt;

	/*  flush Rx */
	//port_ask_more_req_to_md(port);

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] clear_cnt=%d, drop=%d\n",
		__func__, clear_cnt, port->rx_drop_cnt);

	//ccci_event_log(
	//	"md%d: port %s close close by %s rx_len=%d clear_cnt=%d, drop=%d, usagecnt=%d\n",
	//	md_id, port->name, current->comm,
	//	ext_ccci_list_node_count(&port->rx_data_list),
	//	clear_cnt, port->rx_drop_cnt, atomic_read(&port->usage_cnt));


	//port_user_unregister(port);

	return 0;
}

int ext_ccci_dev_close(struct inode *inode,
		struct file *file)
{
	struct port_t *port = file->private_data;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] port %s, close by %s, rx_qlen=%d, usagecnt=%d\n",
		__func__, port->name, current->comm,
		ext_ccci_get_list_node_count(&port->rx_data_list),
		atomic_read(&port->usage_cnt));

	if (port->port_dev_close)
		return port->port_dev_close(port);

	else
		return ext_ccci_comm_dev_close(port);
}

static inline int ccci_handle_nonblock_read(struct file *file,
		struct port_t *port)
{
	int ret = 0;

	if (ext_ccci_get_list_node_count(&port->rx_data_list) == 0) {
		if (!(file->f_flags & O_NONBLOCK)) {

			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] name: %s, qlen=%d\n",
				__func__, port->name,
				ext_ccci_get_list_node_count(
						&port->rx_data_list));

			spin_lock_irq(&port->rx_wq.lock);

			ret = wait_event_interruptible_locked_irq(port->rx_wq,
					ext_ccci_get_list_node_count(
							&port->rx_data_list));

			spin_unlock_irq(&port->rx_wq.lock);

			CCCI_DEBUG_LOG(-1, TAG,
				"[%s] name: %s, qlen=%d, ret=%d\n",
				__func__, port->name,
				ext_ccci_get_list_node_count(
					&port->rx_data_list), ret);

			if (ret == -ERESTARTSYS)
				return -EINTR;

			else if (ret)
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error, port: %s; ret: %d\n",
					__func__, port->name, ret);

		} else
			return -EAGAIN;

	}

	return ret;
}

static inline void ccci_normal_data_adjust(struct port_t *port,
		ccci_kmem_data_t *data)
{
	struct ccci_header *ccci_h = NULL;

	/* header provide by user */
	if (port->flags & PORT_F_USER_HEADER) {
		/* CCCI_MON_CH should fall in here,
		 * as header must be send to md_init
		 */
		ccci_h = (struct ccci_header *)data->off_data;

		if (ccci_h->data[0] == CCCI_MAGIC_NUM) {
			if (unlikely(data->data_len > sizeof(struct ccci_header))) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] recv unexpected data for %s, len=%d\n",
					__func__, port->name, data->data_len);

				data->data_len = sizeof(struct ccci_header);
			}
		}

	} else {
		data->data_len -= sizeof(struct ccci_header);
		data->off_data += sizeof(struct ccci_header);

	}
}

static inline int ccci_handle_copy_data_to_user(struct port_t *port,
		char *buf, size_t count)
{
	int read_len = 0;
	ccci_kmem_data_t *data;


	data = ext_ccci_list_get_first(&port->rx_data_list);
	if (data == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] get NULL first node from :%s\n",
			__func__, port->name);
		return -EFAULT;
	}

	read_len = data->data_len;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data: %p; data->off_data: %p; org_data: %p; buf: %p; count: %ld, data_len: %d\n",
		__func__, data, data->off_data, data->org_data,
		buf, count, data->data_len);

	if (read_len > count)
		read_len = count;

	//if (port->flags & PORT_F_CH_TRAFFIC)
	//	port_ch_dump(port, 0, ext_ccci_get_node_data(pnode), read_len);

	/* 3. copy to user */
	if (copy_to_user(buf, data->off_data, read_len)) {
		CCCI_ERROR_LOG(-1, TAG,
			"read on %s, copy to user failed, %d/%zu\n",
			port->name, read_len, count);

		return -EFAULT;
	}

	data->off_data += read_len;
	data->data_len -= read_len;

	/* 4. free request */
	if (data->data_len <= 0) {
		ext_ccci_data_free(data);
		ext_ccci_list_del_first(&port->rx_data_list);
	}

	return read_len;
}

ssize_t ext_ccci_comm_dev_read(struct file *file,
		struct port_t *port, char *buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	// check dipc port cfg state
	if (port->en_flag == DIPC_PORT_CFG_DISABLE) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] port %s error with EACCES; dipc port cfg: DISABLE\n",
			__func__, port->name);
		return -EACCES;
	}

	/* 1. get incoming request */
	ret = ccci_handle_nonblock_read(file, port);
	if (ret)
		return ret;

	ret = ccci_handle_copy_data_to_user(port, buf, count);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name = %s; ret = %d\n",
		__func__, port->name, ret);

	return ret;
}

ssize_t ext_ccci_dev_read(struct file *file,
		char *buf, size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name = %s; count = %ld\n",
		__func__, port->name, count);

	if (port->port_dev_read)
		return port->port_dev_read(port, buf, count, ppos);

	else
		return ext_ccci_comm_dev_read(file, port, buf, count, ppos);

}

unsigned int ccci_comm_dev_poll(struct port_t *port,
		struct file *fp,
		struct poll_table_struct *poll)
{
	unsigned int mask = 0;

	poll_wait(fp, &port->rx_wq, poll);

	/* TODO: lack of poll wait for Tx */
	if (!ext_ccci_list_empty(&port->rx_data_list))
		mask |= POLLIN | POLLRDNORM;

	//if (port_write_room_to_md(port) > 0)
	//	mask |= POLLOUT | POLLWRNORM;

	return mask;
}

unsigned int ext_ccci_dev_poll(struct file *fp,
		struct poll_table_struct *poll)
{
	struct port_t *port = fp->private_data;

	CCCI_DEBUG_LOG(-1, TAG, "poll on %s\n", port->name);

	if (port->port_dev_poll)
		return port->port_dev_poll(port, fp, poll);

	else
		return ccci_comm_dev_poll(port, fp, poll);


//	if (port->rx_ch == CCCI_UART1_RX &&
//	    md_state != READY &&
//		md_state != EXCEPTION) {
		/* notify MD logger to save its log
		 * before md_init kills it
		 */
//		mask |= POLLERR;
//		CCCI_NORMAL_LOG(md_id, CHAR,
//			"poll error for MD logger at state %d,mask=%d\n",
//			md_state, mask);
//	}
}

static inline int ccci_handle_write_data(
		struct file *file, struct port_t *port,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct ccci_header *ccci_h = NULL;
	size_t actual_count = 0, alloc_size = 0;
	int ret = 0, header_len = 0;
	void *data = NULL;
	ccci_send_data_t send_data;

	send_data.blocking = !(file->f_flags & O_NONBLOCK);

	header_len = sizeof(struct ccci_header);

	if (port->flags & PORT_F_USER_HEADER) {
		if (count > (CCCI_MTU + header_len)) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error:count=%ld,larger than MTU on %s\n",
				__func__, count, port->name);

			return -ENOMEM;
		}
		actual_count = count;
		alloc_size = actual_count;

	} else {
		actual_count = count > CCCI_MTU ? CCCI_MTU : count;
		alloc_size = actual_count + header_len;

	}

	//data = ext_ccci_kmem_alloc(GFP_KERNEL, alloc_size);

	send_data.data = ext_ccci_data_alloc(GFP_KERNEL, alloc_size + CCCI_ADDRESS_ALIGN_LEN);
	if (!send_data.data)
		return -ENOMEM;

	data = send_data.data->off_data;
	ccci_h = (struct ccci_header *)data;
	if (!(port->flags & PORT_F_USER_HEADER)) {
		ccci_h->data[0] = 0;
		ccci_h->data[1] = actual_count + header_len;
		ccci_h->reserved = 0;
		data += header_len;

	}
	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data[1]: %d, actual_count: %ld; header_len: %d\n",
		__func__, ccci_h->data[1], actual_count, header_len);

	ret = copy_from_user(data, buf, actual_count);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: copy_from_user fail: %s, l=%zu r=%d\n",
			__func__, port->name, actual_count, ret);

		ext_ccci_data_free(send_data.data);

		return ret;
	}

	if (port->flags & PORT_F_USER_HEADER) {
		if (actual_count == sizeof(struct ccci_header))
			ccci_h->data[0] = CCCI_MAGIC_NUM;

		else
			ccci_h->data[1] = actual_count;

	}

	ccci_h->channel = port->tx_ch;

	//if (port->flags & PORT_F_CH_TRAFFIC) {
	//	port_ch_dump(port, 1, ccci_h, alloc_size);
	//}

	//ret = ext_ccci_send_data(CCCI_NORMAL_DATA, ccci_h, alloc_size);
	send_data.hif_id = port->hif_id;
	send_data.qno = port->txq_index;
	send_data.data->data_len = alloc_size;
	ret = ccci_msg_send_to_one(port->hif_id + CCCI_CLDMA_BASE_ID,
			1 << port->hif_id, &send_data);

	if (ret < 0)
		ext_ccci_data_free(send_data.data);

	return ret ? ret : actual_count;
}

static int ccci_check_can_write(struct port_t *port)
{
	CLDMA_STATE_T cla_state;

	// check dipc port cfg state
	if (port->en_flag == DIPC_PORT_CFG_DISABLE) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] port %s error with EACCES; dipc port cfg: DISABLE\n",
			__func__, port->name);
		return -EACCES;
	}

	// check driver state
	cla_state = ccci_cldma_state_get();
	if (cla_state != CLA_READY) {
		if ((cla_state == CLA_RECV_HOST_SUSPEND_REQ)
			|| (cla_state == CLA_SEND_HOST_SUSPEND_ACK)
			|| (cla_state == CLA_SEND_HOST_REMOTE_WAKEUP)
			|| (cla_state == CLA_RECV_HOST_RESUME_REQ)) {
			return -(CLA_RECV_HOST_SUSPEND_REQ + ERR_DRV_BASE_ID);
		} else
			return -(cla_state + ERR_DRV_BASE_ID);
	}
	// check host user state
	if (port->host_user_state != HOST_USER_READY)
		return -(port->host_user_state + ERR_HOST_USER_BASE_ID);

	return 0;
}

ssize_t ext_ccci_comm_dev_write(
		struct file *file, struct port_t *port,
		const char __user *buf, size_t count, loff_t *ppos)
{
	int ret = 0;

	ret = ccci_check_can_write(port);
	if (ret < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: (%s); can not send data: ret: %d\n",
			__func__, port->name, ret);
		return ret;
	}

	ret = ccci_handle_write_data(file, port, buf, count, ppos);
	if (ret < 0) {
		if (ret == -EBUSY || ret == -ENOMEM)
			ret = -EAGAIN;

	}

	return ret;
}

ssize_t ext_ccci_dev_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct port_t *port = file->private_data;
	int ret;

	if (count == 0)
		return -EINVAL;

	if (port->port_dev_write)
		ret = port->port_dev_write(file, port, buf, count, ppos);

	else
		ret = ext_ccci_comm_dev_write(file, port, buf, count, ppos);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name = %s; hif_id = %d; count = %ld; return: %d\n",
		__func__, port->name, port->hif_id, count, ret);

	return ret;
}

int ext_ccci_port_recv_check(struct port_t *port,
		void *data)
{
	int qlen;

	// check dipc port cfg state
	if (port->en_flag == DIPC_PORT_CFG_DISABLE) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] port %s error with EACCES; dipc port cfg: DISABLE\n",
			__func__, port->name);
		return -CCCI_HSAPIF_ERR_DIPC_PORT_CFG_DISABLE;
	}

	qlen = ext_ccci_get_list_node_count(&port->rx_data_list);
	if (qlen >= port->rx_length_th) {

		if (!(port->flags & PORT_F_RX_FULLED)) {
			CCCI_NORMAL_LOG(-1, TAG,
				"port %s Rx full start, qlen = %d\n",
				port->name, qlen);

			port->flags |= PORT_F_RX_FULLED;
		}

		if (port->flags & PORT_F_ALLOW_DROP) {
			CCCI_NORMAL_LOG(-1, TAG,
				"drop on %s, qlen = %d\n", port->name, qlen);

			ext_ccci_data_free(data);
			port->rx_drop_cnt ++;
			return -CCCI_HSAPIF_ERR_DROP_PACKET;

		} else {
			port->rx_busy_count ++;
			return -CCCI_HSAPIF_ERR_PORT_RX_FULL;

		}
	}

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data = %p; recv on %s, qlen=%d\n",
		__func__, data, port->name, qlen);

	if (port->flags & PORT_F_RX_FULLED) {
		CCCI_NORMAL_LOG(-1, TAG,
			"port %s Rx full end, qlen = %d; data: %p\n",
			port->name, qlen, data);

		port->flags &= ~PORT_F_RX_FULLED;
	}

	return 0;
}

void ext_ccci_port_recv_wakeup(struct port_t *port)
{
	unsigned long flags;

	__pm_wakeup_event(port->rx_wakelock, jiffies_to_msecs(HZ/2));

	spin_lock_irqsave(&port->rx_wq.lock, flags);
	wake_up_all_locked(&port->rx_wq);
	spin_unlock_irqrestore(&port->rx_wq.lock, flags);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] name: %s, qlen=%d\n",
		__func__, port->name,
		ext_ccci_get_list_node_count(&port->rx_data_list));

}

int ext_ccci_normal_data_recv(struct port_t *port,
		void *data)
{
	int ret = 0;
	ext_ccci_node_t *pnode;


	ret = ext_ccci_port_recv_check(port, data);
	if (ret)
		return ret;

	//if (ccci_h->channel == CCCI_STATUS_RX)
	//	port->skb_handler(port, skb);
	//else
	//	__skb_queue_tail(&port->rx_skb_list, skb);

	pnode = ext_ccci_list_add_node(&port->rx_data_list,
					data, GFP_ATOMIC);
	if (!pnode)
		return -ENOMEM;

	if (port->flags & PORT_F_ADJUST_HEADER)
		ccci_normal_data_adjust(port, (ccci_kmem_data_t *)data);

	port->rx_pkg_cnt ++;

	ext_ccci_port_recv_wakeup(port);

	return 0;
}

int ext_ccci_data_recv(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	struct port_t *port = (struct port_t *)my_data;
	struct ccci_kmem_data *data = (struct ccci_kmem_data *)msg_data;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] port=0x%p\n",__func__, port);

	if (port != NULL) {
		if (port->ops != NULL) {
			/*Only port_mgr_ops.recv_data != NULL*/
			/*port_mgr_data_recv is usded to handle Handshake messages*/
			if (port->ops->recv_data != NULL)
				return port->ops->recv_data(port, data);
		}
	}

	/*For other ports data handling, will be processed by this API*/
	if (port != NULL) {
		return ext_ccci_normal_data_recv(port, data);
	} else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] Fail to process recv data\n",__func__);
		return -1;
	}
}

int ext_ccci_dipc_port_cfg(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	struct cldma_port_enum_msg *data =
		(struct cldma_port_enum_msg *)msg_data;
	int i = 0;
	struct port_t *port;

	if (data != NULL) {
		for (; i < data->port_count; i++) {
			port = ext_ccci_get_port_from_minor(
				data->ports[i].ch_id);
			if (!port) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s]: port is NULL (ch_id: %d)\n",
					__func__, data->ports[i].ch_id);
				return -1;
			}
			port->en_flag = data->ports[i].en_flag;
		}
		return 0;
	}

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] Fail to process dipc cfg\n", __func__);
	return -1;
}

long ext_ccci_dev_ioctl(
		struct file *file,
		unsigned int cmd,
		unsigned long arg)
{
	ccci_ioctrl_data_t ioctrl_data;

	ioctrl_data.user_arg = arg;

	return ccci_msg_send(CCCI_IOCTRL_ID, cmd, &ioctrl_data);
}

#ifdef CONFIG_COMPAT
long ext_ccci_dev_compat_ioctl(
		struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	ccci_ioctrl_data_t ioctrl_data;

	ioctrl_data.user_arg = arg;

	return ccci_msg_send(CCCI_IOCTRL_ID, cmd, &ioctrl_data);
}
#endif
