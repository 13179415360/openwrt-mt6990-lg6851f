// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/cdev.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>   /* needed by miscdevice* */

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_ioctrl_def.h"
#include "ccci_msg_center.h"
#include "ccci_state_mgr.h"
#include "ccci_comm_config.h"
#include "ccci_fsm_cldma.h"
#include "../hif/ccci_hif_cldma.h"

#define TAG "sysfs"

extern void cldma_gpd_dump(unsigned int q);
extern void cldma_regs_dump(void);

/* default value of enable CLDMA log */
int cldma_log_enable = 0;

/*
 * Showing funciton once the sys dev file has been 'cat'
 *
 */
static ssize_t cldma_log_enable_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cldma_log_enable);
}

/*
 * Input funciton once the sys dev file has been 'echo'
 *
 */
static ssize_t cldma_log_enable_ctrl(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		cldma_log_enable = value;
		CCCI_HISTORY_TAG_LOG(-1, TAG,
		"[%s] 1:enable, 0:disable = %d\n",
		__func__, cldma_log_enable);
	}
	return n;
}

DEVICE_ATTR(cldma_log_enable, 0644, cldma_log_enable_show, cldma_log_enable_ctrl);

/* default value of dump CLDMA gpd for specific queue */
int cldma_gpd_dump_q = 0;

/*
 * Showing funciton once the sys dev file has been 'cat'
 *
 */
static ssize_t cldma_gpd_dump_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cldma_gpd_dump_q);
}

/*
 * Input funciton once the sys dev file has been 'echo'
 *
 */
static ssize_t cldma_gpd_dump_ctrl(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value < CLDMA_TXQ_NUM && value < CLDMA_RXQ_NUM) {
			cldma_gpd_dump_q = value;
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] dump cldma q%d gpd\n",
				__func__, cldma_gpd_dump_q);
			cldma_gpd_dump(value);
		} else {
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] invalid cldma q num:%d (max txq num:%d, max rxq num:%d)\n",
				__func__, value, CLDMA_TXQ_NUM, CLDMA_RXQ_NUM);
		}
	}
	return n;
}

DEVICE_ATTR(cldma_gpd_dump_q, 0644, cldma_gpd_dump_show, cldma_gpd_dump_ctrl);

/* default value of dump CLDMA gpd for all queue */
int cldma_gpd_dump_all = 0;

/*
 * Showing funciton once the sys dev file has been 'cat'
 *
 */
static ssize_t cldma_gpd_dump_all_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cldma_gpd_dump_all);
}

/*
 * Input funciton once the sys dev file has been 'echo'
 *
 */
static ssize_t cldma_gpd_dump_all_ctrl(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 0 || value == 1) {
			cldma_gpd_dump_all = value;
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] dump all cldma gpd : %d (1:yes, 0:no)\n",
				__func__, cldma_gpd_dump_all);
			if (value == 1) {
				cldma_gpd_dump(CLDMA_RXQ_NUM);  // note: currently RXQ equals TXQ.
			}
		} else {
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] invalid value : %d\n",
				__func__, value);
		}
	}
	return n;
}

DEVICE_ATTR(cldma_gpd_dump_all, 0644, cldma_gpd_dump_all_show, cldma_gpd_dump_all_ctrl);

/* default value of dump CLDMA registers */
int cldma_hw_regs_dump = 0;

/*
 * Showing funciton once the sys dev file has been 'cat'
 *
 */
static ssize_t cldma_hw_regs_dump_show(struct device *kobj
	, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", cldma_hw_regs_dump);
}

/*
 * Input funciton once the sys dev file has been 'echo'
 *
 */
static ssize_t cldma_hw_regs_dump_ctrl(struct device *kobj
	, struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int value = 0;

	if (kstrtouint(buf, 10, &value) == 0) {
		if (value == 0 || value == 1) {
			cldma_hw_regs_dump = value;
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] dump cldma hw regs : %d (1:yes, 0:no)\n",
				__func__, cldma_hw_regs_dump);
			if (value == 1) {
				cldma_regs_dump();
			}
		} else {
			CCCI_HISTORY_TAG_LOG(-1, TAG,
				"[%s] invalid value : %d\n",
				__func__, value);
		}
	}
	return n;
}

DEVICE_ATTR(cldma_hw_regs_dump, 0644, cldma_hw_regs_dump_show, cldma_hw_regs_dump_ctrl);

const struct file_operations cldma_log_ops = {
	.owner = THIS_MODULE,
	//.read = ,
	//.open =,
};

static struct miscdevice cldma_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "cldma",
	.fops = &cldma_log_ops
};

/*
 * register /dev and /sys files
 * @return:     0: success, otherwise: fail
 */
int create_cldma_files(void)
{
	int ret;

	ret = misc_register(&cldma_device);
	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] misc register failed\n", __func__);
		return ret;
	}

	ret = device_create_file(cldma_device.this_device
					, &dev_attr_cldma_log_enable);

	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] device create file failed - cldma_log_enable\n", __func__);
		return ret;
	}

	ret = device_create_file(cldma_device.this_device
					, &dev_attr_cldma_gpd_dump_q);

	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] device create file failed - cldma_gpd_dump\n", __func__);
		return ret;
	}

	ret = device_create_file(cldma_device.this_device
					, &dev_attr_cldma_gpd_dump_all);

	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] device create file failed - cldma_gpd_dump_all\n", __func__);
		return ret;
	}

	ret = device_create_file(cldma_device.this_device
					, &dev_attr_cldma_hw_regs_dump);

	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] device create file failed - cldma_hw_regs_dump\n", __func__);
		return ret;
	}

	return 0;
}

