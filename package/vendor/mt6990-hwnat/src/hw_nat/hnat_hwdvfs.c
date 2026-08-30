/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2022 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2022 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
#include <linux/proc_fs.h>

#include "foe_fdb.h"
#include "hnat_dbg_proc.h"
#include "frame_engine.h"
#include "util.h"

/* disable by default (non-set means 0) */
static u8 hwdvfs_en;

/* byte_cnt and pkt_cnt accounting period, unit: 20 us */
/* Default: 50000 * 20us = 1 s*/
/* Max: 65535 * 20us = 1.310 s */
static u16 hwdvfs_acc_prd = 50000;

/* dvfs vcore value must be 0~15 */
static u8 dvfs_vcore_value[] = {0, 1, 2, 3};

/* dvfs bandwidth value must be 0~15 */
static u8 dvfs_bandwidth_value[] = {0, 4, 5, 7};

/* byte threshold (unit: MB) */
static u16 byte_threshold_low[] = {10, 30, 50};
static u16 byte_threshold_high[] = {20, 40, 60};

/* packet threshold (unit: 4k packets) */
static u16 pkt_threshold_low[] = {2, 6, 10};
static u16 pkt_threshold_high[] = {4, 8, 12};

/* PROCFS */
#define PROCREG_HWDVFS_USAGE		"hwdvfs_usage"
#define PROCREG_HWDVFS_EN		"hwdvfs_en"
#define PROCREG_HWDVFS_BYTE_THRESHOLD	"hwdvfs_byte_threshold"
#define PROCREG_HWDVFS_PKT_THRESHOLD	"hwdvfs_pkt_threshold"
#define PROCREG_HWDVFS_ACC_PERIOD	"hwdvfs_acc_period"
#define PROCREG_HWDVFS_CHECK_LEVEL	"hwdvfs_check_level"

static struct proc_dir_entry	*proc_hwdvfs_usage;
static struct proc_dir_entry	*proc_hwdvfs_en;
static struct proc_dir_entry	*proc_hwdvfs_byte_threshold;
static struct proc_dir_entry	*proc_hwdvfs_pkt_threshold;
static struct proc_dir_entry	*proc_hwdvfs_acc_period;
static struct proc_dir_entry	*proc_hwdvfs_check_level;

static void hwdvfs_byte_en_set(bool hwdvfs_byte_en);
static void hwdvfs_pkt_en_set(bool hwdvfs_pkt_en);

static int hwdvfs_usage_read(struct seq_file *seq, void *v)
{
	/* START */
	pr_notice("\n");
	pr_notice("********************************************************\n");
	pr_notice("****************[hwdvfs commands usage:]****************\n");
	pr_notice("********************************************************\n");

	/* 1. /proc/hnat/hwdvfs_en */
	pr_notice("1. hwdvfs disable/enable\n");
	pr_notice("[Usage: check if hwdvfs disable/enable] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_EN);
	pr_notice("[Usage: set hwdvfs disable/enable] echo [0~3] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_EN);
	pr_notice("0: disable\n");
	pr_notice("1: enable hwdvfs byte\n");
	pr_notice("2: enable hwdvfs pkt\n");
	pr_notice("3: enable hwdvfs byte and pkt\n");
	pr_notice("\n");

	/* 2. /proc/hnat/hwdvfs_byte_threshold */
	pr_notice("2. hwdvfs_byte_threshold (unit: MB), Max: 65535\n");
	pr_notice("[Usage: check hwdvfs_byte_threshold] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_BYTE_THRESHOLD);
	pr_notice("[Usage: set hwdvfs_byte_threshold]\n");
	pr_notice("echo [byte_low_thrsh1] [byte_low_thrsh2] [byte_low_thrsh3]");
	pr_notice("[byte_high_thrsh1] [byte_high_thrsh2] [byte_high_thrsh3] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_BYTE_THRESHOLD);
	pr_notice("\n");

	/* 3. /proc/hnat/hwdvfs_pkt_threshold */
	pr_notice("3. hwdvfs_pkt_threshold (unit: 4k packets), Max: 65535\n");
	pr_notice("[Usage: check hwdvfs_pkt_threshold] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_PKT_THRESHOLD);
	pr_notice("[Usage: set hwdvfs_pkt_threshold]\n");
	pr_notice("echo [pkt_low_thrsh1] [pkt_low_thrsh2] [pkt_low_thrsh3]");
	pr_notice("[pkt_high_thrsh1] [pkt_high_thrsh2] [pkt_high_thrsh3] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_PKT_THRESHOLD);
	pr_notice("\n");

	/* 4. /proc/hnat/hwdvfs_acc_period */
	pr_notice("4. hwdvfs_acc_period (unit: 20us, Default: 50000(1s), Max: 65535(1.31s))\n");
	pr_notice("[Usage: check hwdvfs accouting period] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_ACC_PERIOD);
	pr_notice("[Usage: set hwdvfs accouting period] echo [0~65535] > /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_ACC_PERIOD);
	pr_notice("\n");

	/* 5. /proc/hnat/hwdvfs_check_level */
	pr_notice("5. /proc/hnat/hwdvfs_check_level\n");
	pr_notice("[Usage: check hwdvfs parking level] cat /proc/%s/%s\n",
		HNAT_PROCREG_DIR, PROCREG_HWDVFS_CHECK_LEVEL);
	pr_notice("\n");

	return 0;
}

static int hwdvfs_usage_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_usage_read, NULL);
}

static int hwdvfs_en_read(struct seq_file *seq, void *v)
{
	pr_info("hwdvfs_en = %d\n", hwdvfs_en);
	return 0;
}

static int hwdvfs_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_en_read, NULL);
}

static ssize_t hwdvfs_en_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[32];
	int len = count;
	long arg0 = 0;
	int ret;
	u32 val;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	ret = kstrtol(buf, 10, &arg0);

	if (arg0 <= 3 && arg0 >= 0) {
		/* Valid input */
		hwdvfs_en = arg0;
	} else {
		/* Invalid input */
		hwdvfs_en = 0;
	}

	pr_info("[%s] hwdvfs_en is set to %d\n", __func__, hwdvfs_en);

	/* When hwdvfs_en = 1,2,3 */
	if (hwdvfs_en) {
		/* Set NETSYS_DVFS 4 levels */
		val = 0;
		val |= (dvfs_vcore_value[0] << NETSYS_DVFS0_VC_VAL_OFFSET);
		val |= (dvfs_vcore_value[1] << NETSYS_DVFS1_VC_VAL_OFFSET);
		val |= (dvfs_vcore_value[2] << NETSYS_DVFS2_VC_VAL_OFFSET);
		val |= (dvfs_vcore_value[3] << NETSYS_DVFS3_VC_VAL_OFFSET);
		val |= (dvfs_bandwidth_value[0] << NETSYS_DVFS0_BW_VAL_OFFSET);
		val |= (dvfs_bandwidth_value[1] << NETSYS_DVFS1_BW_VAL_OFFSET);
		val |= (dvfs_bandwidth_value[2] << NETSYS_DVFS2_BW_VAL_OFFSET);
		val |= (dvfs_bandwidth_value[3] << NETSYS_DVFS3_BW_VAL_OFFSET);
		reg_write(NETSYS_DVFS_CFG2, val);

		/* Set NETSYS_DVFS_EN 1 */
		val = reg_read(NETSYS_DVFS_CFG0);
		val = val | NETSYS_DVFS_EN;
		reg_write(NETSYS_DVFS_CFG0, val);

		switch (hwdvfs_en) {
		case 1:
			hwdvfs_byte_en_set(true);
			hwdvfs_pkt_en_set(false);
			break;

		case 2:
			hwdvfs_byte_en_set(false);
			hwdvfs_pkt_en_set(true);
			break;

		case 3:
			hwdvfs_byte_en_set(true);
			hwdvfs_pkt_en_set(true);
			break;

		default:
			break;
		}
	}
	/* When hwdvfs_en = 0 */
	else {
		/* Set default opp */
#ifndef CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP
#define CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP	3	/* the highest vcore */
#endif
		if (CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP == 0) {
			/* mifi project, force to level 0 */
			reg_write(NETSYS_DVFS_BYTE_TH1, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_BYTE_TH2, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_BYTE_TH3, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH1, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH2, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH3, 0xFFFFFFFF);
		} else if (CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP == 1) {
			reg_write(NETSYS_DVFS_BYTE_TH1, 0);
			reg_write(NETSYS_DVFS_BYTE_TH2, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_BYTE_TH3, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH1, 0);
			reg_write(NETSYS_DVFS_PKT_TH2, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH3, 0xFFFFFFFF);
		} else if (CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP == 2) {
			reg_write(NETSYS_DVFS_BYTE_TH1, 0);
			reg_write(NETSYS_DVFS_BYTE_TH2, 0);
			reg_write(NETSYS_DVFS_BYTE_TH3, 0xFFFFFFFF);
			reg_write(NETSYS_DVFS_PKT_TH1, 0);
			reg_write(NETSYS_DVFS_PKT_TH2, 0);
			reg_write(NETSYS_DVFS_PKT_TH3, 0xFFFFFFFF);
		} else {
			/* cpe project, force to level 3 */
			reg_write(NETSYS_DVFS_BYTE_TH1, 0);
			reg_write(NETSYS_DVFS_BYTE_TH2, 0);
			reg_write(NETSYS_DVFS_BYTE_TH3, 0);
			reg_write(NETSYS_DVFS_PKT_TH1, 0);
			reg_write(NETSYS_DVFS_PKT_TH2, 0);
			reg_write(NETSYS_DVFS_PKT_TH3, 0);
		}

		/* Temporarily set accounting period to 1 (20us) */
		val = reg_read(NETSYS_DVFS_ACC_CFG);
		val &= (65535 << 16);
		val = val | (1 << NETSYS_DVFS_ACC_PERIOD_OFFSET);
		reg_write(NETSYS_DVFS_ACC_CFG, val);

		/* SWDVFS force to default opp */
		backto_default_opp();

		/* Set NETSYS_DVFS_EN 0 */
		val = reg_read(NETSYS_DVFS_CFG0);
		val = val & ~(NETSYS_DVFS_EN);
		reg_write(NETSYS_DVFS_CFG0, val);

		/* Set NETSYS_DVFS_ACC_BYTE_CNT_EN 0 */
		hwdvfs_byte_en_set(false);
		/* Set NETSYS_DVFS_ACC_PKT_CNT_EN 0 */
		hwdvfs_pkt_en_set(false);
	}

	return len;
}

static void hwdvfs_acc_period_set(void)
{
	u32 val;

	pr_info("[%s] set hwdvfs_acc_prd to %d\n", __func__, hwdvfs_acc_prd);
	/* Set byte_cnt and pkt_cnt accounting period */
	val = reg_read(NETSYS_DVFS_ACC_CFG);
	/* clear bit[15:0] */
	val &= (65535 << 16);
	val = val | (hwdvfs_acc_prd << NETSYS_DVFS_ACC_PERIOD_OFFSET);
	reg_write(NETSYS_DVFS_ACC_CFG, val);
}

static void hwdvfs_byte_en_threshold_set(void)
{
	u32 val;

	/* Set NETSYS_DVFS_BYTE_TH1 Low */
	val = byte_threshold_low[0] | (byte_threshold_high[0] << 16);
	reg_write(NETSYS_DVFS_BYTE_TH1, val);

	/* Set NETSYS_DVFS_BYTE_TH2 Low */
	val = byte_threshold_low[1] | (byte_threshold_high[1] << 16);
	reg_write(NETSYS_DVFS_BYTE_TH2, val);

	/* Set NETSYS_DVFS_BYTE_TH3 Low */
	val = byte_threshold_low[2] | (byte_threshold_high[2] << 16);
	reg_write(NETSYS_DVFS_BYTE_TH3, val);
}

static void hwdvfs_pkt_en_threshold_set(void)
{
	u32 val;

	pr_info("[%s]\n", __func__);
	/* Set NETSYS_DVFS_PKT_TH1 Low */
	val = pkt_threshold_low[0] | (pkt_threshold_high[0] << 16);
	reg_write(NETSYS_DVFS_PKT_TH1, val);

	/* Set NETSYS_DVFS_PKT_TH2 Low */
	val = pkt_threshold_low[1] | (pkt_threshold_high[1] << 16);
	reg_write(NETSYS_DVFS_PKT_TH2, val);

	/* Set NETSYS_DVFS_PKT_TH3 Low */
	val = pkt_threshold_low[2] | (pkt_threshold_high[2] << 16);
	reg_write(NETSYS_DVFS_PKT_TH3, val);
}

static void hwdvfs_byte_en_set(bool hwdvfs_byte_en)
{
	u32 val;

	pr_info("[%s] hwdvfs_byte_en = %d\n", __func__, hwdvfs_byte_en);

	if (hwdvfs_byte_en) {
		/* Set NETSYS_DVFS_ACC_BYTE_CNT_EN 1 */
		val = reg_read(NETSYS_DVFS_ACC_CFG);
		val = val | NETSYS_DVFS_ACC_BYTE_CNT_EN;
		reg_write(NETSYS_DVFS_ACC_CFG, val);

		/* Set byte threshold when enable */
		hwdvfs_byte_en_threshold_set();

		/* Set byte_cnt and pkt_cnt accounting period */
		hwdvfs_acc_period_set();
	} else {
		/* Set NETSYS_DVFS_ACC_BYTE_CNT_EN 0 */
		val = reg_read(NETSYS_DVFS_ACC_CFG);
		val = val & ~(NETSYS_DVFS_ACC_BYTE_CNT_EN);
		reg_write(NETSYS_DVFS_ACC_CFG, val);
	}
}

static void hwdvfs_pkt_en_set(bool hwdvfs_pkt_en)
{
	u32 val;

	pr_info("[%s] hwdvfs_pkt_en = %d\n", __func__, hwdvfs_pkt_en);

	if (hwdvfs_pkt_en) {
		/* Set NETSYS_DVFS_ACC_PKT_CNT_EN 1 */
		val = reg_read(NETSYS_DVFS_ACC_CFG);
		val = val | NETSYS_DVFS_ACC_PKT_CNT_EN;
		reg_write(NETSYS_DVFS_ACC_CFG, val);

		/* Set pkt threshold when enable */
		hwdvfs_pkt_en_threshold_set();

		/* Set byte_cnt and pkt_cnt accounting period */
		hwdvfs_acc_period_set();
	} else {
		/* Set NETSYS_DVFS_ACC_PKT_CNT_EN 0 */
		val = reg_read(NETSYS_DVFS_ACC_CFG);
		val = val & ~(NETSYS_DVFS_ACC_PKT_CNT_EN);
		reg_write(NETSYS_DVFS_ACC_CFG, val);
	}
}

static int hwdvfs_byte_threshold_read(struct seq_file *seq, void *v)
{
	int i;

	pr_info("[%s] (unit: MB)\n", __func__);
	/* Show 3 byte threshold */
	for (i = 0; i < 3; i++) {
		pr_info("byte_threshold_%d, Low %d, High %d\n",
			i + 1, byte_threshold_low[i], byte_threshold_high[i]);
	}

	return 0;
}

static int hwdvfs_byte_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_byte_threshold_read, NULL);
}

ssize_t hwdvfs_byte_threshold_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	int len = count;
	long arg0 = 0;
	int ret;
	u32 val;

	char * const delim = " ";
	char *token, *cur = buf;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_low[0] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH1 Low */
		val = reg_read(NETSYS_DVFS_BYTE_TH1);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_BYTE_TH1, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH1_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_low[1] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH2 Low */
		val = reg_read(NETSYS_DVFS_BYTE_TH2);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_BYTE_TH2, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH2_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_low[2] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH3 Low */
		val = reg_read(NETSYS_DVFS_BYTE_TH3);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_BYTE_TH3, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH3_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_high[0] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH1 High */
		val = reg_read(NETSYS_DVFS_BYTE_TH1);
		/* clear bit[31:16] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_BYTE_TH1, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH1_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;

	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_high[1] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH2 High */
		val = reg_read(NETSYS_DVFS_BYTE_TH2);
		/* clear bit[31:16] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_BYTE_TH2, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH2_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		byte_threshold_high[2] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH3 High */
		val = reg_read(NETSYS_DVFS_BYTE_TH3);
		/* clear bit[31:16] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_BYTE_TH3, val);
	} else {
		pr_err("NETSYS_DVFS_BYTE_TH3_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

failed:
	return len;
}

static int hwdvfs_pkt_threshold_read(struct seq_file *seq, void *v)
{
	int i;

	pr_info("[%s] (unit: 4k packets)\n", __func__);
	/* Show 3 pkt threshold */
	for (i = 0; i < 3; i++) {
		pr_info("pkt_threshold_%d, Low %d, High %d\n",
			i + 1, pkt_threshold_low[i], pkt_threshold_high[i]);
	}

	return 0;
}

static int hwdvfs_pkt_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_pkt_threshold_read, NULL);
}

ssize_t hwdvfs_pkt_threshold_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	int len = count;
	long arg0 = 0;
	int ret;
	u32 val;

	char * const delim = " ";
	char *token, *cur = buf;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_low[0] = arg0;

		/* Set NETSYS_DVFS_PKT_TH1 Low */
		val = reg_read(NETSYS_DVFS_PKT_TH1);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_PKT_TH1, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH1_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_low[1] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH2 Low */
		val = reg_read(NETSYS_DVFS_PKT_TH2);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_PKT_TH2, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH2_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_low[2] = arg0;

		/* Set NETSYS_DVFS_BYTE_TH3 Low */
		val = reg_read(NETSYS_DVFS_PKT_TH3);
		/* clear bit[15:0] */
		val &= (65535 << 16);
		val = val | arg0;
		reg_write(NETSYS_DVFS_PKT_TH3, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH3_LOW is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_high[0] = arg0;

		/* Set NETSYS_DVFS_PKT_TH1 High */
		val = reg_read(NETSYS_DVFS_PKT_TH1);
		/* clear bit[15:0] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_PKT_TH1, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH1_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;

	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_high[1] = arg0;

		/* Set NETSYS_DVFS_PKT_TH2 High */
		val = reg_read(NETSYS_DVFS_PKT_TH2);
		/* clear bit[31:16] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_PKT_TH2, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH2_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	if (arg0 >= 0 && arg0 <= 65535) {
		pkt_threshold_high[2] = arg0;

		/* Set NETSYS_DVFS_PKT_TH3 High */
		val = reg_read(NETSYS_DVFS_PKT_TH3);
		/* clear bit[31:16] */
		val &= 65535;
		val = val | (arg0 << 16);
		reg_write(NETSYS_DVFS_PKT_TH3, val);
	} else {
		pr_err("NETSYS_DVFS_PKT_TH3_HIGH is invalid, should be 0~65535 !!\n");
		return -EFAULT;
	}

failed:
	return len;
}

static int hwdvfs_acc_period_read(struct seq_file *seq, void *v)
{
	pr_info("hwdvfs_acc_prd = %d (unit: 20us)\n", hwdvfs_acc_prd);
	return 0;
}

static int hwdvfs_acc_period_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_acc_period_read, NULL);
}

static ssize_t hwdvfs_acc_period_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[32];
	int len = count;
	long arg0 = 0;
	int ret;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	ret = kstrtol(buf, 10, &arg0);

	hwdvfs_acc_prd = arg0;

	/* Correct invalid value */
	if (hwdvfs_acc_prd > 65535) {
		pr_info("[%s] Input hwdvfs_acc_prd is more than Max 65535, set to 65535\n", __func__);
		hwdvfs_acc_prd = 65535;
	} else if (hwdvfs_acc_prd <= 0) {
		pr_info("[%s] Input hwdvfs_acc_prd is less than Min 1, set to 1\n", __func__);
		hwdvfs_acc_prd = 1;
	}

	/* Set value to register */
	pr_info("[%s] set hwdvfs_acc_prd to %d\n", __func__, hwdvfs_acc_prd);
	hwdvfs_acc_period_set();

	return len;
}

static int hwdvfs_check_level_read(struct seq_file *seq, void *v)
{
	u32 val;

	pr_info("[%s]\n", __func__);
	val = reg_read(NETSYS_DVFS_LEVEL_DBG);
	pr_info("hwdvfs Final level %d\n", (val >> NETSYS_DVFS_ALL_LEVEL_OFFSET) & 3);
	pr_info("hwdvfs pkt level %d\n", (val >> NETSYS_DVFS_PKT_LEVEL_OFFSET) & 3);
	pr_info("hwdvfs byte level %d\n", (val >> NETSYS_DVFS_BYTE_LEVEL_OFFSET) & 3);

	return 0;
}

static int hwdvfs_check_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwdvfs_check_level_read, NULL);
}

static const struct PROC_STRUCT hwdvfs_usage_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_usage_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hwdvfs_en_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_en_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = hwdvfs_en_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hwdvfs_byte_threshold_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_byte_threshold_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = hwdvfs_byte_threshold_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hwdvfs_pkt_threshold_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_pkt_threshold_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = hwdvfs_pkt_threshold_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hwdvfs_acc_period_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_acc_period_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = hwdvfs_acc_period_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT hwdvfs_check_level_fops = {
	PROC_OWNER
	.PROC_OPEN = hwdvfs_check_level_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_RELEASE = single_release
};

static void create_hwdvfs_procfs(void)
{
	proc_hwdvfs_usage = proc_create(PROCREG_HWDVFS_USAGE, 0400,
		hnat_proc_reg_dir, &hwdvfs_usage_fops);
	if (!proc_hwdvfs_usage)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_USAGE);

	proc_hwdvfs_en = proc_create(PROCREG_HWDVFS_EN, 0400,
		hnat_proc_reg_dir, &hwdvfs_en_fops);
	if (!proc_hwdvfs_en)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_EN);

	proc_hwdvfs_byte_threshold = proc_create(PROCREG_HWDVFS_BYTE_THRESHOLD, 0400,
		hnat_proc_reg_dir, &hwdvfs_byte_threshold_fops);
	if (!proc_hwdvfs_byte_threshold)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_BYTE_THRESHOLD);

	proc_hwdvfs_pkt_threshold = proc_create(PROCREG_HWDVFS_PKT_THRESHOLD, 0400,
		hnat_proc_reg_dir, &hwdvfs_pkt_threshold_fops);
	if (!proc_hwdvfs_pkt_threshold)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_PKT_THRESHOLD);

	proc_hwdvfs_acc_period = proc_create(PROCREG_HWDVFS_ACC_PERIOD, 0400,
		hnat_proc_reg_dir, &hwdvfs_acc_period_fops);
	if (!proc_hwdvfs_acc_period)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_ACC_PERIOD);

	proc_hwdvfs_check_level = proc_create(PROCREG_HWDVFS_CHECK_LEVEL, 0400,
		hnat_proc_reg_dir, &hwdvfs_check_level_fops);
	if (!proc_hwdvfs_check_level)
		pr_err("!! FAIL to create %s PROC !!\n", PROCREG_HWDVFS_CHECK_LEVEL);
}

void hw_dvfs_init(void)
{
	create_hwdvfs_procfs();
}

void hw_dvfs_fini(void) {

	if (proc_hwdvfs_usage)
                remove_proc_entry(PROCREG_HWDVFS_USAGE, hnat_proc_reg_dir);
	if (proc_hwdvfs_en)
                remove_proc_entry(PROCREG_HWDVFS_EN, hnat_proc_reg_dir);
	if (proc_hwdvfs_byte_threshold)
                remove_proc_entry(PROCREG_HWDVFS_BYTE_THRESHOLD, hnat_proc_reg_dir);
	if (proc_hwdvfs_pkt_threshold)
                remove_proc_entry(PROCREG_HWDVFS_PKT_THRESHOLD, hnat_proc_reg_dir);
	if (proc_hwdvfs_acc_period)
                remove_proc_entry(PROCREG_HWDVFS_ACC_PERIOD, hnat_proc_reg_dir);
	if (proc_hwdvfs_check_level)
                remove_proc_entry(PROCREG_HWDVFS_CHECK_LEVEL, hnat_proc_reg_dir);
}

