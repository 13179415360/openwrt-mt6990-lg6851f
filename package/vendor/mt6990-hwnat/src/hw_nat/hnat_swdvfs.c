/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2020 MediaTek Inc.
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
 * Copyright(C) 2020 MediaTek Inc. All rights reserved.
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


#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/timer.h>
#include <linux/version.h>

#include "foe_fdb.h"
#include "hnat_dbg_proc.h"
#include "frame_engine.h"
#include "util.h"
#include "hnat_config.h"

/* #define DD printk("%s %d\n", __FUNCTION__, __LINE__); */

u64 lower_tput[] = {10, 30, 50, ULLONG_MAX};		/* Tput threshold */
u64 upper_tput[] = {20, 40, 60, ULLONG_MAX};		/* in MBps */

u64 lower_pcnt[] = { 80,  800, 30000, ULLONG_MAX};	/* packet cnt threshold */
u64 upper_pcnt[] = {100, 1000, 40000, ULLONG_MAX};

#if defined(CONFIG_HNAT_V2)
int vcoreopp_to_dramopp[] = {0, 4, 5, 7};	/* vcore to dram opp mapping tbl*/
#else /* CONFIG_HNAT_V2 */
int vcoreopp_to_dramopp[] = {0, 2, 5, 6};	/* vcore to dram opp mapping tbl*/
#endif /* CONFIG_HNAT_V2 */

#ifndef CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP
#define CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP	3	/* the highest vcore */
#endif
#define RETRY_COUNT				100	/* in ms */

static bool			dvfs_en = false;	/* disable by default */
static int			force_opp = 0xFF;	/* invalid by default */

struct timer_list		dvfs_timer;
int				dvfs_current_opp;
int				dvfs_debug_level = 1;
int				dvfs_timeout = 1;	/* unite: HZ */

u64 lower_tput_force[sizeof(lower_tput)];		/* threshold. for debug */
u64 upper_tput_force[sizeof(upper_tput)];   		/* tput in MBps */
u64 lower_pcnt_force[sizeof(lower_pcnt)];		/* threshold. for debug */
u64 upper_pcnt_force[sizeof(upper_pcnt)];   		/* packet cnt */

static int dvfs_print(const char *fmt, ...)
{
	char buf[256];
	va_list args;
	int ret = 0;

	va_start(args, fmt);
	if (dvfs_debug_level) {
		ret = vsnprintf(buf, 256, fmt, args);
		if (ret < 0 || ret >= 256)
			pr_notice("%s(), err: %d\n", __func__, ret);
		printk("%s", buf);
	}
	va_end(args);
	return 0;
}

static int get_new_opp(u64 value, u64 *threshold, int count) /* Mbps*/
{
	int i;

	/* favor lower opp */
	for (i=0; i<count; i++) {
		if (threshold[i] >= value)
			return i;
	}
	return i;
}

static u64 get_bytes_count(void)
{
	int i;
	u64 bcnt = 0;
	int ag_idx[] = {
		GDMA1_PSE_PORT,
		GDMA2_PSE_PORT,
		WDMA0_PSE_PORT,
		WDMA1_PSE_PORT,
		MDMA_PSE_PORT,
		EDMA0_PSE_PORT,
		EDMA1_PSE_PORT
	#if defined(CONFIG_HNAT_V2)
		,WDMA2_PSE_PORT
	#endif
		};

	for (i=0; i<sizeof(ag_idx)/sizeof(int); i++) {

	#if defined(CONFIG_HNAT_V1)
		bcnt += reg_read(AC_BASE + ag_idx[i] * 16);
		bcnt += ((u64)reg_read(AC_BASE + ag_idx[i] * 16 + 4)) << 32;

	#elif defined(CONFIG_HNAT_V2)
		bcnt += reg_read(PPE0_AC_BASE + ag_idx[i] * 16);
		bcnt += ((u64)reg_read(PPE0_AC_BASE + ag_idx[i] * 16 + 4)) << 32;

		bcnt += reg_read(PPE1_AC_BASE + ag_idx[i] * 16);
		bcnt += ((u64)reg_read(PPE1_AC_BASE + ag_idx[i] * 16 + 4)) << 32;
	#endif /* CONFIG_HNAT_V2 */

	}

	/* in MBps */
	return (bcnt >> 20);
}

static u64 get_packet_count(void)
{
	int i;
	unsigned long long pcnt = 0;
	int ag_idx[] = {
		GDMA1_PSE_PORT,
		GDMA2_PSE_PORT,
		WDMA0_PSE_PORT,
		WDMA1_PSE_PORT,
		MDMA_PSE_PORT,
		EDMA0_PSE_PORT,
		EDMA1_PSE_PORT
	#if defined(CONFIG_HNAT_V2)
		,WDMA2_PSE_PORT
	#endif
		};

	for (i=0; i<sizeof(ag_idx)/sizeof(int); i++) {

	#if defined(CONFIG_HNAT_V1)
		pcnt += reg_read(AC_BASE + ag_idx[i] * 16 + 8);

	#elif defined(CONFIG_HNAT_V2)
		pcnt += reg_read(PPE0_AC_BASE + ag_idx[i] * 16 + 8);
		pcnt += reg_read(PPE1_AC_BASE + ag_idx[i] * 16 + 8);

	#endif /* CONFIG_HNAT_V2 */
	}

	/* in MBps */
	return pcnt;
}


static int update_opp_by_tput(int present_opp)
{
	u64 tput = get_bytes_count() / dvfs_timeout;

	dvfs_print("netsys dvfs: tput = %lld MBps\n", tput);

	/* favor low opp */
	if (tput < lower_tput[present_opp])
		return get_new_opp(tput, &lower_tput[0], sizeof(lower_tput)/sizeof(u64));

	if (tput > upper_tput[present_opp])
		return get_new_opp(tput, &upper_tput[0], sizeof(upper_tput)/sizeof(u64));

	return present_opp;
}

static int update_opp_by_pcnt(int present_opp)
{
	u64 pcnt = get_packet_count() / dvfs_timeout;

	dvfs_print("netsys dvfs: packet = %lld\n", pcnt);

	/* favor low opp */
	if (pcnt < lower_pcnt[present_opp])
		return get_new_opp(pcnt, &lower_pcnt[0], sizeof(lower_pcnt)/sizeof(u64));

	if (pcnt > upper_pcnt[present_opp])
		return get_new_opp(pcnt, &upper_pcnt[0], sizeof(upper_pcnt)/sizeof(u64));

	return present_opp;
}

static int change_to_opp(int opp)
{
	u32 val;

	int retry = 0;

	if (opp >= sizeof(vcoreopp_to_dramopp)/sizeof(int)) {
		pr_err("netsys swdvfs: sanity error\n");
		return -1;
	}

	val = reg_read(NETSYS_DVFS_CFG1);
	if (val & NETSYS_SW_VC_DVFS_ACK || val & NETSYS_SW_BW_DVFS_ACK) {
		pr_err("netsys swdvfs: ack error\n");
		return -1;
	}

	/* handle vcore */
	val &= 0xFF8FFFFF;
	val |= (((opp & 0x00000007) << NETSYS_SW_VC_DVFS_VAL_OFFSET)) | NETSYS_SW_VC_DVFS_REQ;

	/* handle dram bw */
	val &= 0x8FFFFFFF;
	val |= (((vcoreopp_to_dramopp[opp] & 0x00000007) << NETSYS_SW_BW_DVFS_VAL_OFFSET)) | NETSYS_SW_BW_DVFS_REQ;

	reg_write(NETSYS_DVFS_CFG1, val);

	/* wait for ack becomes 1 */
	val = reg_read(NETSYS_DVFS_CFG1);

	while ((val & NETSYS_SW_VC_DVFS_ACK) == 0 ||
		(val & NETSYS_SW_BW_DVFS_ACK) == 0) {
		if (retry++ >= RETRY_COUNT) {
			pr_err("netsys swdvfs: ack(1) timeout\n");
			return -1;
		}

		udelay(1000);
		val = reg_read(NETSYS_DVFS_CFG1);
	}

	/* clear req */
	val &= ~(NETSYS_SW_VC_DVFS_REQ | NETSYS_SW_BW_DVFS_REQ);

	/* clear vcore & dram bw */
	val &= 0x8F8FFFFF;

	reg_write(NETSYS_DVFS_CFG1, val);

	/* wait for ack becomes 0 */
	val = reg_read(NETSYS_DVFS_CFG1);

	while ((val & NETSYS_SW_VC_DVFS_ACK) ||
		(val & NETSYS_SW_BW_DVFS_ACK)) {
		if (retry++ >= RETRY_COUNT) {
			pr_info("%s, netsys swdvfs: ack(0) timeout\n", __func__);
			return -1;
		}

		udelay(1000);
		val = reg_read(NETSYS_DVFS_CFG1);
	}

	return 0;
}

static void dvfs_do(struct timer_list *t)
{
	int latest_opp;

	/* check /proc/force_opp. Opp range is between 0 to 3 */
	if (force_opp >= 0 && force_opp < sizeof(upper_tput)/sizeof(u64)) {
		latest_opp = force_opp;
	} else {
		int latest_tput_opp = update_opp_by_tput(dvfs_current_opp);
		int latest_pcnt_opp = update_opp_by_pcnt(dvfs_current_opp);

		dvfs_print("netsys dvfs: latest_tput_opp = %d\n", latest_tput_opp);
		dvfs_print("netsys dvfs: latest_pcnt_opp = %d\n", latest_pcnt_opp);

		latest_opp = max(latest_tput_opp, latest_pcnt_opp);
	}

	dvfs_print("netsys dvfs: latest_opp = %d\n", latest_opp);

	if (latest_opp != dvfs_current_opp) {
		dvfs_print("netsys dvfs: old_opp:%d new_opp:%d \n", dvfs_current_opp, latest_opp);

		dvfs_current_opp = latest_opp;

		change_to_opp(dvfs_current_opp);
	}

	if (dvfs_en)
		mod_timer(&dvfs_timer, jiffies + HZ * dvfs_timeout);	/* setup next timer */

	return;
}

static void dvfs_hw_enable(bool enable)
{
	u32 val;

	if (enable){
		val = reg_read(NETSYS_DVFS_CFG1);
		val = val | (NETSYS_SW_VC_DVFS_EN | NETSYS_SW_BW_DVFS_EN);
		reg_write(NETSYS_DVFS_CFG1, val);

		val = reg_read(NETSYS_DVFS_CFG0);
		val = val | NETSYS_DVFS_EN;
		reg_write(NETSYS_DVFS_CFG0, val);
	} else {
		val = reg_read(NETSYS_DVFS_CFG0);
		val = val & ~(NETSYS_DVFS_EN);
		reg_write(NETSYS_DVFS_CFG0, val);

		val = reg_read(NETSYS_DVFS_CFG1);
		val = val & ~(NETSYS_SW_VC_DVFS_EN | NETSYS_SW_BW_DVFS_EN);
		reg_write(NETSYS_DVFS_CFG1, val);
	}
}

static void change_threshold(void)
{
	if (upper_tput_force[2]) {
		lower_tput[0] = lower_tput_force[0];
		lower_tput[1] = lower_tput_force[1];
		lower_tput[2] = lower_tput_force[2];
		upper_tput[0] = upper_tput_force[0];
		upper_tput[1] = upper_tput_force[1];
		upper_tput[2] = upper_tput_force[2];
	}

	if (upper_pcnt_force[2]) {
		lower_pcnt[0] = lower_pcnt_force[0];
		lower_pcnt[1] = lower_pcnt_force[1];
		lower_pcnt[2] = lower_pcnt_force[2];
		upper_pcnt[0] = upper_pcnt_force[0];
		upper_pcnt[1] = upper_pcnt_force[1];
		upper_pcnt[2] = upper_pcnt_force[2];
	}
}

/* only used in fini/init */
void backto_default_opp(void)
{
	pr_info("%s, %d\n", __func__, CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP);
	dvfs_hw_enable(true);
	dvfs_current_opp = CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP;
	change_to_opp(CONFIG_HW_NAT_SW_DVFS_DEFAULT_OPP);
	dvfs_hw_enable(false);
}

/* PROCFS */
#define PROCREG_DVFS_EN			"dvfs_en"
#define PROCREG_FORCE_OPP		"dvfs_force_opp"
#define PROCREG_FORCE_TPUT_THRESHOLD	"dvfs_force_tput_thrsh"
#define PROCREG_FORCE_PCNT_THRESHOLD	"dvfs_force_pcnt_thrsh"
#define PROCREG_DVFS_TIMEOUT		"dvfs_timeout"
#define PROCREG_DVFS_DEBUG_LEVEL	"dvfs_debug_level"

static struct proc_dir_entry 	*proc_force_opp;
static struct proc_dir_entry 	*proc_force_tput_threshold;
static struct proc_dir_entry 	*proc_force_pcnt_threshold;
static struct proc_dir_entry 	*proc_dvfs_en;
static struct proc_dir_entry 	*proc_dvfs_timeout;
static struct proc_dir_entry 	*proc_dvfs_debug_level;

static int dvfs_en_read(struct seq_file *seq, void *v)
{
	pr_info("dvfs_en=%d\n", dvfs_en);
	return 0;
}

static int dvfs_en_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_en_read, NULL);
}

static ssize_t dvfs_en_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
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

	dvfs_en = arg0 ? true : false;

	if (dvfs_en) {
		change_threshold();	/* change threshold if possible */

		dvfs_hw_enable(true);

		/* start the timer for dvfs activity */
		mod_timer(&dvfs_timer, jiffies + HZ * dvfs_timeout);
	} else {
		timer_delete_sync(&dvfs_timer);

		backto_default_opp();

		dvfs_hw_enable(false);
	}

	return len;
}

static ssize_t dvfs_timeout_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
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
	dvfs_timeout = arg0;

	/* MAX 10 secs. min 1 secs */
	if (dvfs_timeout > 10)
		dvfs_timeout = 10;
	if (dvfs_timeout <= 0)
		dvfs_timeout = 1;

	return len;
}

static ssize_t dvfs_debug_level_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
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
	dvfs_debug_level = arg0 ? 1 : 0;

	return len;
}

static int force_opp_read(struct seq_file *seq, void *v)
{
	pr_info("force_opp=%d\n", force_opp);	/* just simply printk */
	return 0;
}

static int dvfs_debug_level_read(struct seq_file *seq, void *v)
{
	pr_info("dvfs_debug_level=%d\n", dvfs_debug_level);
	return 0;
}

static int dvfs_timeout_read(struct seq_file *seq, void *v)
{
	pr_info("dvfs_timeout=%d\n", dvfs_timeout);
	return 0;
}

static int force_tput_threshold_read(struct seq_file *seq, void *v)
{
	pr_info("as-is:\n");
	pr_info("lower_tput:%lld %lld %lld\n", lower_tput[0], lower_tput[1], lower_tput[2]);
	pr_info("upper_tput:%lld %lld %lld\n", upper_tput[0], upper_tput[1], upper_tput[2]);
	pr_info("to-be:\n");
	pr_info("lower_tput:%lld %lld %lld\n", lower_tput_force[0], lower_tput_force[1], lower_tput_force[2]);
	pr_info("upper_tput:%lld %lld %lld\n", upper_tput_force[0], upper_tput_force[1], upper_tput_force[2]);

	return 0;
}


static int force_pcnt_threshold_read(struct seq_file *seq, void *v)
{
	pr_info("as-is:\n");
	pr_info("lower_pcnt:%lld %lld %lld\n", lower_pcnt[0], lower_pcnt[1], lower_pcnt[2]);
	pr_info("upper_pcnt:%lld %lld %lld\n", upper_pcnt[0], upper_pcnt[1], upper_pcnt[2]);
	pr_info("to-be:\n");
	pr_info("lower_pcnt:%lld %lld %lld\n", lower_pcnt_force[0], lower_pcnt_force[1], lower_pcnt_force[2]);
	pr_info("upper_pcnt:%lld %lld %lld\n", upper_pcnt_force[0], upper_pcnt_force[1], upper_pcnt_force[2]);

	return 0;
}

ssize_t force_tput_threshold_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	int len = count;
	long arg0 = 0;
	int ret;

	char * const delim = " ";
	char *token, *cur = buf;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';

	/* ugly actually. only for debug. */
	upper_tput_force[2] = 0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_tput_force[0] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_tput_force[1] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_tput_force[2] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	upper_tput_force[0] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;

	ret = kstrtol(token, 10, &arg0);
	upper_tput_force[1] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	upper_tput_force[2] = arg0;

failed:
	return len;
}

ssize_t force_pcnt_threshold_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char buf[64];
	int len = count;
	long arg0 = 0;
	int ret;

	char * const delim = " ";
	char *token, *cur = buf;

	if (len >= sizeof(buf) || len <= 0)
		return -EFAULT;

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';

	/* ugly actually. only for debug. */
	upper_pcnt_force[2] = 0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_pcnt_force[0] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_pcnt_force[1] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	lower_pcnt_force[2] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	upper_pcnt_force[0] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;

	ret = kstrtol(token, 10, &arg0);
	upper_pcnt_force[1] = arg0;

	token = strsep(&cur, delim);
	if (!token)
		goto failed;
	ret = kstrtol(token, 10, &arg0);
	upper_pcnt_force[2] = arg0;

failed:
	return len;
}


static int force_opp_open(struct inode *inode, struct file *file)
{
	return single_open(file, force_opp_read, NULL);
}

static int force_tput_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, force_tput_threshold_read, NULL);
}

static int force_pcnt_threshold_open(struct inode *inode, struct file *file)
{
	return single_open(file, force_pcnt_threshold_read, NULL);
}

static int dvfs_timeout_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_timeout_read, NULL);
}

static int dvfs_debug_level_open(struct inode *inode, struct file *file)
{
	return single_open(file, dvfs_debug_level_read, NULL);
}

ssize_t force_opp_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
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

	force_opp = arg0;

	return len;
}



static const struct PROC_STRUCT dvfs_en_fops = {
	PROC_OWNER
	.PROC_OPEN = dvfs_en_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = dvfs_en_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT force_opp_fops = {
	PROC_OWNER
	.PROC_OPEN = force_opp_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = force_opp_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT force_tput_threshold_fops = {
	PROC_OWNER
	.PROC_OPEN = force_tput_threshold_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = force_tput_threshold_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT force_pcnt_threshold_fops = {
	PROC_OWNER
	.PROC_OPEN = force_pcnt_threshold_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = force_pcnt_threshold_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT dvfs_timeout_fops = {
	PROC_OWNER
	.PROC_OPEN = dvfs_timeout_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = dvfs_timeout_write,
	.PROC_RELEASE = single_release
};

static const struct PROC_STRUCT dvfs_debug_level_fops = {
	PROC_OWNER
	.PROC_OPEN = dvfs_debug_level_open,
	.PROC_READ = seq_read,
	.PROC_SEEK = seq_lseek,
	.PROC_WRITE = dvfs_debug_level_write,
	.PROC_RELEASE = single_release
};

static void create_procfs(void)
{
        proc_dvfs_en = proc_create(PROCREG_DVFS_EN, 0400,
                                      hnat_proc_reg_dir, &dvfs_en_fops);
        if (!proc_dvfs_en)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_DVFS_EN);

        proc_force_opp = proc_create(PROCREG_FORCE_OPP, 0400,
                                      hnat_proc_reg_dir, &force_opp_fops);
        if (!proc_force_opp)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_FORCE_OPP);

        proc_force_tput_threshold = proc_create(PROCREG_FORCE_TPUT_THRESHOLD, 0400,
                                      hnat_proc_reg_dir, &force_tput_threshold_fops);
        if (!proc_force_tput_threshold)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_FORCE_TPUT_THRESHOLD);

        proc_force_pcnt_threshold = proc_create(PROCREG_FORCE_PCNT_THRESHOLD, 0400,
                                      hnat_proc_reg_dir, &force_pcnt_threshold_fops);
        if (!proc_force_pcnt_threshold)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_FORCE_PCNT_THRESHOLD);

	proc_dvfs_timeout = proc_create(PROCREG_DVFS_TIMEOUT, 0400,
                                      hnat_proc_reg_dir, &dvfs_timeout_fops);
        if (!proc_dvfs_timeout)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_DVFS_TIMEOUT);

	proc_dvfs_debug_level = proc_create(PROCREG_DVFS_DEBUG_LEVEL, 0400,
                                      hnat_proc_reg_dir, &dvfs_debug_level_fops);
        if (!proc_dvfs_debug_level)
                pr_err("!! FAIL to create %s PROC !!\n", PROCREG_DVFS_DEBUG_LEVEL);

}

static void fini_procfs(void)
{
	if (proc_dvfs_en)
                remove_proc_entry(PROCREG_DVFS_EN, hnat_proc_reg_dir);
	if (proc_force_opp)
                remove_proc_entry(PROCREG_FORCE_OPP, hnat_proc_reg_dir);
	if (proc_force_tput_threshold)
                remove_proc_entry(PROCREG_FORCE_TPUT_THRESHOLD, hnat_proc_reg_dir);
	if (proc_force_pcnt_threshold)
                remove_proc_entry(PROCREG_FORCE_PCNT_THRESHOLD, hnat_proc_reg_dir);
	if (proc_dvfs_timeout)
                remove_proc_entry(PROCREG_DVFS_TIMEOUT, hnat_proc_reg_dir);
	if (proc_dvfs_debug_level)
                remove_proc_entry(PROCREG_DVFS_DEBUG_LEVEL, hnat_proc_reg_dir);
}

/* init/fini */
void sw_dvfs_init(void)
{
	create_procfs();

	backto_default_opp();

	/* setup dvfs timer */
	timer_setup(&dvfs_timer, dvfs_do, 0);
}

void sw_dvfs_fini(void)
{
	/* remove timer */
	timer_delete_sync(&dvfs_timer);

	/* back to default opp */
	backto_default_opp();

	fini_procfs();
}

/** Command guide:
 * 1. hnat_dvfs
 *   echo 1 > /proc/hnat/dvfs_en
 *   echo 3 > /proc/hnat/dvfs_force_opp
 *   echo 0 > /proc/hnat/dvfs_en
 *
 * 2. query opp level
 *   cat /sys/devices/platform/1200f000.dvfsrc/helio-dvfsrc/dvfsrc_dump
 *
 * 3. force opp level
 *   echo 21 > /sys/devices/platform/1200f000.dvfsrc/helio-dvfsrc/dvfsrc_force_vcore_dvfs_opp
 *
 * 4. release opp level
 *   echo 32 > /sys/devices/platform/1200f000.dvfsrc/helio-dvfsrc/dvfsrc_force_vcore_dvfs_opp
 */
