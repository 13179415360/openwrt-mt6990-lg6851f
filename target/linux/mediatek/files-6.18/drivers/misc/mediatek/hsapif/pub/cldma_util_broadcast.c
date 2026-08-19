// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <asm/ioctl.h>
#include <linux/compiler.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#include "ccci_debug.h"

#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
#include<mt-plat/mtk_ccci_common.h>
#include "ccci_msg_id.h"
#include "ccci_fsm_cldma.h"
#endif

#define TAG "cldma_bc"

/* Broadcast event defination */
struct host_pcie_status_event {
	struct timeval time_stamp;
	int event_type;
	char reason[32];
};

#ifdef CONFIG_COMPAT
struct host_pcie_status_compat_event {
	struct compat_timeval time_stamp;
	int event_type;
	char reason[32];
};
#endif

#define HOST_PCIE_BC_MAX_NUM	(1)
#define HOST_PCIE_BC_SUPPORT	(1<<0)

#define HOST_PCIE_EVENT_BUFF_SIZE (8)

static struct bc_host_ctl_block_t *s_bc_ctl_tbl[HOST_PCIE_BC_MAX_NUM];
static spinlock_t s_host_event_update_lock;
static int s_host_user_request_lock_cnt;
static spinlock_t s_host_user_lock_cnt_lock;


static struct class *s_ccci_bd_class;
static dev_t s_host_status_dev;
struct cdev s_host_bd_char_dev;

struct last_host_pcie_status_event {
	int has_value;
	struct timeval time_stamp;
	int event_type;
	char reason[32];
};

static struct last_host_pcie_status_event last_host_pcie_status[HOST_PCIE_BC_MAX_NUM];

#define CLDMA_UTIL_BC_MAGIC 'B'  /*magic */
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
#define CLDMA_IOC_REMOTE_WAKE_UP_HOST	_IO(CLDMA_UTIL_BC_MAGIC, 0)
#endif
#define CLDMA_IOC_HOLD_RST_LOCK		_IO(CLDMA_UTIL_BC_MAGIC, 1)
#define CLDMA_IOC_FREE_RST_LOCK		_IO(CLDMA_UTIL_BC_MAGIC, 2)
#define CLDMA_IOC_GET_HOLD_RST_CNT	_IO(CLDMA_UTIL_BC_MAGIC, 3)
#define CLDMA_IOC_SHOW_LOCK_USER		_IO(CLDMA_UTIL_BC_MAGIC, 4)

/* Internal used */
struct bc_host_ctl_block_t {
	struct list_head user_list;
	unsigned int event_bit_mask;
	wait_queue_head_t wait;
};

struct cldma_util_bc_user_ctlb {
	struct bc_host_ctl_block_t *bc_dev_ptr;
	int pending_event_cnt;
	int has_request_rst_lock;
	int curr_w;
	int curr_r;
	int buff_cnt;
	struct host_pcie_status_event event_buf[HOST_PCIE_EVENT_BUFF_SIZE];
	int exit;
	struct list_head node;
	char user_name[32];
};

/* The use of wakelock after broadcast state to userspace's app */
struct wakeup_source *resume_ack_wakelock;

static void inject_host_pcie_event_helper(struct cldma_util_bc_user_ctlb *user_ctlb,
	int md_id, struct timeval *ev_rtime, int event_type, char reason[])
{
	int count = 0;

	if (user_ctlb->pending_event_cnt == user_ctlb->buff_cnt) {
		/* Free one space */
		user_ctlb->curr_r++;
		user_ctlb->pending_event_cnt--;
		if (user_ctlb->curr_r >= user_ctlb->buff_cnt)
			user_ctlb->curr_r = 0;
	}

	if (user_ctlb->curr_w < 0 || user_ctlb->curr_w >= HOST_PCIE_EVENT_BUFF_SIZE) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] write index is out of bound\n", __func__);
		return;
	}

	user_ctlb->event_buf[user_ctlb->curr_w].time_stamp = *ev_rtime;
	user_ctlb->event_buf[user_ctlb->curr_w].event_type = event_type;
	if (reason != NULL) {
		count = snprintf(user_ctlb->event_buf[user_ctlb->curr_w].reason, 32,
			"%s", reason);
		if (count < 0) {
			CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format reason: %s\n",
					__func__, count, reason);
			return;
		}
	} else {
		count = snprintf(user_ctlb->event_buf[user_ctlb->curr_w].reason, 32,
			"%s", "----");
		if (count < 0) {
			CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format reason: ----\n",
					__func__, count);
			return;
		}
	}
	user_ctlb->curr_w++;
	if (user_ctlb->curr_w >= user_ctlb->buff_cnt)
		user_ctlb->curr_w = 0;
	user_ctlb->pending_event_cnt++;
}

static void save_last_host_pcie_status(int md_id,
		struct timeval *time_stamp, int event_type, char reason[])
{
	int count = 0;

	/* MD_STA_EV_HS1 = 9
	 * ignore events before MD_STA_EV_HS1
	 */
	if (event_type < 9)
		return;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] md_id = %d; event_type = %d\n",
			__func__, md_id, event_type);

	last_host_pcie_status[0].has_value = 1;
	last_host_pcie_status[0].time_stamp = *time_stamp;
	last_host_pcie_status[0].event_type = event_type;

	if (reason != NULL) {
		count = snprintf(last_host_pcie_status[0].reason, 32, "%s", reason);
		if (count < 0) {
			CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format reason: %s\n",
					__func__, count, reason);
			return;
		}
	} else {
		count = snprintf(last_host_pcie_status[0].reason, 32, "%s", "----");
		if (count < 0) {
			CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format reason: ----\n",
					__func__, count);
			return;
		}
	}
}

static void send_last_host_pcie_status_to_user(int md_id,
		struct cldma_util_bc_user_ctlb *user_ctlb)
{
	int i;

	CCCI_NORMAL_LOG(-1, TAG, "[%s] md_id = %d; user_name = %s\n",
			__func__, md_id, user_ctlb->user_name);

	/* md_id == -1, that means it's ccci_mdx_sta */
	for (i = 0; i < HOST_PCIE_BC_MAX_NUM; i++) {
		if (last_host_pcie_status[i].has_value == 1) {

			inject_host_pcie_event_helper(user_ctlb,
					0,
					&last_host_pcie_status[i].time_stamp,
					last_host_pcie_status[i].event_type,
					last_host_pcie_status[i].reason);
		}
	}
}

/* acquire wake lock for resume ack after broadcast state to userspace's app */
void cldma_bc_wakeup_event(void)
{
	CCCI_NORMAL_LOG(-1, TAG,"[%s] set wakelock after resume broadcasting \n",__func__);
	__pm_wakeup_event(resume_ack_wakelock, jiffies_to_msecs(HZ/2));
}

void inject_host_pcie_status_event(int cldma_id, int event_type, char reason[])
{
	struct timespec64 tv64;
	struct timeval time_stamp;
	struct cldma_util_bc_user_ctlb *user_ctlb;
	unsigned int event_mark = HOST_PCIE_BC_SUPPORT;
	int i;
	unsigned long flag;

	ktime_get_ts64(&tv64);
	time_stamp.tv_sec = tv64.tv_sec;
	time_stamp.tv_usec = tv64.tv_nsec/NSEC_PER_USEC;

	spin_lock_irqsave(&s_host_event_update_lock, flag);
	save_last_host_pcie_status(0, &time_stamp, event_type, reason);
	for (i = 0; i < HOST_PCIE_BC_MAX_NUM; i++) {
		if (s_bc_ctl_tbl[i]->event_bit_mask & event_mark) {
			list_for_each_entry(user_ctlb,
				&s_bc_ctl_tbl[i]->user_list, node)
				inject_host_pcie_event_helper(user_ctlb, 0,
					&time_stamp, event_type, reason);
			wake_up_interruptible(&s_bc_ctl_tbl[i]->wait);
		}
	}
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);
}

int get_host_lock_rst_user_cnt(int md_id)
{
	if (md_id == 0)
		return s_host_user_request_lock_cnt;
	return 0;
}

int get_host_lock_rst_user_list(int md_id, char list_buff[], int size)
{
	int cpy_size;
	int total_size = 0;
	struct cldma_util_bc_user_ctlb *user_ctlb;
	unsigned long flag;

	if (list_buff == NULL) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] NULL buffer for lock list\n", __func__);
		return 0;
	}

	if (md_id == 0) {
		spin_lock_irqsave(&s_host_event_update_lock, flag);
		list_for_each_entry(user_ctlb,
			&s_bc_ctl_tbl[0]->user_list, node) {
			if (user_ctlb->has_request_rst_lock) {
				cpy_size = snprintf(&list_buff[total_size],
						size - total_size,
						"%s,", user_ctlb->user_name);
				if (cpy_size > 0)
					total_size += cpy_size;
			}
		}
		spin_unlock_irqrestore(&s_host_event_update_lock, flag);
	}

	list_buff[total_size] = 0;

	return total_size;
}

static int ccci_util_bc_open(struct inode *inode, struct file *filp)
{
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;
	int minor;
	unsigned long flag;
	int count = 0;

	minor = iminor(inode);
	if (minor < 0 || minor >= HOST_PCIE_BC_MAX_NUM) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] minor is out of bound\n", __func__);
		return -ENODEV;
	}
	bc_dev = s_bc_ctl_tbl[minor];

	user_ctlb = kzalloc(sizeof(struct cldma_util_bc_user_ctlb),
					GFP_KERNEL);
	if (user_ctlb == NULL)
		return -ENOMEM;

	user_ctlb->bc_dev_ptr = s_bc_ctl_tbl[minor];
	user_ctlb->pending_event_cnt = 0;
	user_ctlb->has_request_rst_lock = 0;
	user_ctlb->curr_w = 0;
	user_ctlb->curr_r = 0;
	user_ctlb->buff_cnt = HOST_PCIE_EVENT_BUFF_SIZE;
	user_ctlb->exit = 0;
	INIT_LIST_HEAD(&user_ctlb->node);
	count = snprintf(user_ctlb->user_name, 32, "%s", current->comm);
	if (count < 0) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] %d failed to format user_name\n",
				__func__, count);
		kfree(user_ctlb);
		user_ctlb = NULL;
		return -ENOMEM;
	}
	filp->private_data = user_ctlb;
	nonseekable_open(inode, filp);

	spin_lock_irqsave(&s_host_event_update_lock, flag);
	list_add_tail(&user_ctlb->node, &bc_dev->user_list);
	send_last_host_pcie_status_to_user(minor - 1, user_ctlb);
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);

	return 0;
}

static int ccci_util_bc_release(struct inode *inode, struct file *filp)
{
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;
	unsigned long flag;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;

	user_ctlb->exit = 1;
	wake_up_interruptible(&bc_dev->wait);
	spin_lock_irqsave(&s_host_event_update_lock, flag);
	user_ctlb->pending_event_cnt = 0;
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);
	if (bc_dev->event_bit_mask & HOST_PCIE_BC_SUPPORT) {
		spin_lock(&s_host_user_lock_cnt_lock);
		if (user_ctlb->has_request_rst_lock == 1) {
			user_ctlb->has_request_rst_lock = 0;
			s_host_user_request_lock_cnt--;
		}
		spin_unlock(&s_host_user_lock_cnt_lock);
	}
	spin_lock_irqsave(&s_host_event_update_lock, flag);
	list_del(&user_ctlb->node);
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);
	kfree(user_ctlb);

	return 0;
}

static ssize_t ccci_util_bc_write(struct file *filp, const char __user *buf,
	size_t size, loff_t *ppos)
{
	return 0;
}

static int read_out_event(struct cldma_util_bc_user_ctlb *user_ctlb,
	struct host_pcie_status_event *event)
{
	int ret;
	struct host_pcie_status_event *src_event;
	unsigned long flag;

	spin_lock_irqsave(&s_host_event_update_lock, flag);
	if (user_ctlb->pending_event_cnt) {
		src_event = &user_ctlb->event_buf[user_ctlb->curr_r];
		memcpy(event, src_event, sizeof(struct host_pcie_status_event));
		user_ctlb->pending_event_cnt--;
		ret = 1;
		user_ctlb->curr_r++;
		if (user_ctlb->curr_r >= user_ctlb->buff_cnt)
			user_ctlb->curr_r = 0;
	} else
		ret = 0;
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);

	return ret;
}

static int cpy_compat_event_to_user(char __user *buf, size_t size,
	const struct host_pcie_status_event *event)
{
	unsigned int event_size;
#ifdef CONFIG_COMPAT
	struct host_pcie_status_compat_event compat_event;
#endif

	/*check parameter from user is legal */
	if (buf == NULL)
		return -EFAULT;

#ifdef CONFIG_COMPAT
	if (test_thread_flag(TIF_32BIT) && !COMPAT_USE_64BIT_TIME) {
		event_size = sizeof(struct host_pcie_status_compat_event);
		if (size < event_size)
			return -EINVAL;
		compat_event.time_stamp.tv_sec = event->time_stamp.tv_sec;
		compat_event.time_stamp.tv_usec = event->time_stamp.tv_usec;
		compat_event.event_type = event->event_type;
		memcpy(compat_event.reason, event->reason, 32);
		if (copy_to_user(buf, &compat_event, event_size))
			return -EFAULT;
		else
			return (ssize_t)event_size;
	} else {
#endif
		event_size = sizeof(struct host_pcie_status_event);
		if (copy_to_user(buf, event, event_size))
			return -EFAULT;
		else
			return (ssize_t)event_size;
#ifdef CONFIG_COMPAT
	}
#endif
}

static ssize_t ccci_util_bc_read(struct file *filp, char __user *buf,
	size_t size, loff_t *ppos)
{
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;
	struct host_pcie_status_event event;
	int ret;
	int has_data;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;
	has_data = read_out_event(user_ctlb, &event);

	/* For Non-block read */
	if (filp->f_flags & O_NONBLOCK) {
		if (has_data == 0)
			return -EAGAIN;
		else
			return cpy_compat_event_to_user(buf, size, &event);
	}

	/* For block read */
	while (1) {
		if (has_data)
			return cpy_compat_event_to_user(buf, size, &event);

		ret = wait_event_interruptible(bc_dev->wait,
				user_ctlb->pending_event_cnt ||
				user_ctlb->exit);
		if (ret)
			return ret;

		has_data = read_out_event(user_ctlb, &event);
	}

	return 0;
}

static long ccci_util_bc_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	int err = 0;
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;
	int lock_cnt, cpy_size;
	char *buf;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;

	/* check device type and device number */
	if (_IOC_TYPE(cmd) != CLDMA_UTIL_BC_MAGIC)
		return -EINVAL;

	switch (cmd) {
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
	case CLDMA_IOC_REMOTE_WAKE_UP_HOST:
		/*If already existing one pending remote_wakeup to be executed, return directly*/
		err = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_SEND_HOST_REMOTE_WAKEUP, NULL);
		CCCI_NORMAL_LOG(-1, TAG, "Recv Remote Wake Up Host Cmd\n");
		break;
#endif
	case CLDMA_IOC_HOLD_RST_LOCK:
		if (bc_dev->event_bit_mask & HOST_PCIE_BC_SUPPORT) {
			spin_lock(&s_host_user_lock_cnt_lock);
			if (user_ctlb->has_request_rst_lock == 0) {
				user_ctlb->has_request_rst_lock = 1;
				s_host_user_request_lock_cnt++;
			}
			spin_unlock(&s_host_user_lock_cnt_lock);
		} else
			err = -1;
		break;
	case CLDMA_IOC_FREE_RST_LOCK:
		if (bc_dev->event_bit_mask & HOST_PCIE_BC_SUPPORT) {
			spin_lock(&s_host_user_lock_cnt_lock);
			if (user_ctlb->has_request_rst_lock == 1) {
				user_ctlb->has_request_rst_lock = 0;
				s_host_user_request_lock_cnt--;
			}
			spin_unlock(&s_host_user_lock_cnt_lock);
		} else
			err = -1;
		break;
	case CLDMA_IOC_GET_HOLD_RST_CNT:
		if (bc_dev->event_bit_mask & HOST_PCIE_BC_SUPPORT)
			lock_cnt = get_host_lock_rst_user_cnt(0);
		else
			lock_cnt = 0;
		err = put_user((unsigned int)lock_cnt,
				(unsigned int __user *)arg);
		break;
	case CLDMA_IOC_SHOW_LOCK_USER:
		buf = kmalloc(1024, GFP_KERNEL);
		if (buf == NULL) {
			err = -1;
			break;
		}
		if (bc_dev->event_bit_mask & HOST_PCIE_BC_SUPPORT){
			cpy_size = get_host_lock_rst_user_list(0, buf, 1024);
			get_host_lock_rst_user_list(2, &buf[cpy_size],
			1024 - cpy_size);
		} else {
			buf[0] = 0;
		}
		CCCI_NORMAL_LOG(-1, TAG, "[%s] Lock reset user list: %s\n", __func__, buf);
		kfree(buf);

		break;
	default:
		err = -ENOIOCTLCMD;
		break;
	}
	return err;
}

#ifdef CONFIG_COMPAT
static long ccci_util_bc_compat_ioctl(struct file *filp, unsigned int cmd,
	unsigned long arg)
{
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;

	user_ctlb = filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		CCCI_ERROR_LOG(-1, TAG,
			"%s(!filp->f_op || !filp->f_op->unlocked_ioctl)\n", __func__);
		return -ENOTTY;
	}

	return filp->f_op->unlocked_ioctl(filp, cmd,
			(unsigned long)compat_ptr(arg));
}
#endif

static unsigned int ccci_util_bc_poll(struct file *filp,
	struct poll_table_struct *wait)
{
	unsigned int mask = 0;
	struct cldma_util_bc_user_ctlb *user_ctlb;
	struct bc_host_ctl_block_t *bc_dev;
	unsigned long flag;

	user_ctlb = (struct cldma_util_bc_user_ctlb *)filp->private_data;
	bc_dev = user_ctlb->bc_dev_ptr;
	poll_wait(filp, &bc_dev->wait, wait);

	spin_lock_irqsave(&s_host_event_update_lock, flag);
	if (user_ctlb->pending_event_cnt)
		mask |= POLLIN|POLLRDNORM;
	spin_unlock_irqrestore(&s_host_event_update_lock, flag);

	return mask;
}

static const struct file_operations broad_cast_fops = {
	.owner = THIS_MODULE,
	.open = ccci_util_bc_open,
	.read = ccci_util_bc_read,
	.write = ccci_util_bc_write,
	.unlocked_ioctl = ccci_util_bc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ccci_util_bc_compat_ioctl,
#endif
	.poll = ccci_util_bc_poll,
	.release = ccci_util_bc_release,
};

int cldma_util_broadcast_init(void)
{
	int ret;
	int i;
	dev_t dev_n;

	memset(last_host_pcie_status, 0, sizeof(last_host_pcie_status));

	for (i = 0; i < HOST_PCIE_BC_MAX_NUM; i++) {
		s_bc_ctl_tbl[i] = kmalloc(sizeof(struct bc_host_ctl_block_t),
			GFP_KERNEL);
		if (s_bc_ctl_tbl[i] == NULL)
			goto _exit;
		INIT_LIST_HEAD(&s_bc_ctl_tbl[i]->user_list);
		init_waitqueue_head(&s_bc_ctl_tbl[i]->wait);
#if 0
		if (i == 0)
			s_bc_ctl_tbl[i]->event_bit_mask = 0x7;
		else
			s_bc_ctl_tbl[i]->event_bit_mask = (1<<(i-1));
#endif
		/*For each device: its event_mask will set as HOST_PCIE_BC_SUPPORT(i.e.,1) by default*/
		s_bc_ctl_tbl[i]->event_bit_mask = 0x7;
	}

	/* regiser wakeup soruce for resume ack */
	CCCI_INIT_LOG(-1, TAG,"[%s] set resume wakelock \n",__func__);
	resume_ack_wakelock = wakeup_source_register(NULL, "cldma_resume_ack_wakelock");

	spin_lock_init(&s_host_event_update_lock);
	spin_lock_init(&s_host_user_lock_cnt_lock);

	s_ccci_bd_class = class_create(THIS_MODULE, "ccci_host_pcie_sta");
	s_host_user_request_lock_cnt = 0;


	ret = alloc_chrdev_region(&s_host_status_dev, 0, 1, "ccci_host_pcie_sta");
	if (ret != 0) {
		CCCI_ERROR_LOG(-1, TAG, "alloc chrdev fail for ccci_md_sta(%d)\n",
			ret);
		goto _exit_1;
	}
	cdev_init(&s_host_bd_char_dev, &broad_cast_fops);
	s_host_bd_char_dev.owner = THIS_MODULE;
	ret = cdev_add(&s_host_bd_char_dev, s_host_status_dev, 1);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "cdev_add failed\n");
		goto _exit_2;
	}

	/* create a device node in directory /dev/ */
	for (i = 0; i < HOST_PCIE_BC_MAX_NUM; i++) {
		dev_n = MKDEV(MAJOR(s_host_status_dev), i);
		if (i == 0) {
			device_create(s_ccci_bd_class, NULL, dev_n,
				NULL, "ccci_host_pcie_sta");
		}
	}

	return 0;

_exit_2:
	cdev_del(&s_host_bd_char_dev);

_exit_1:
	unregister_chrdev_region(s_host_status_dev, 1);

_exit:
	for (i = 0; i < HOST_PCIE_BC_MAX_NUM; i++)
		kfree(s_bc_ctl_tbl[i]);

	return -1;
}

