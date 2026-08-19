// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/trace_seq.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>

#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/device.h>       /* needed by device_* */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "medmcu_helper.h"
#include "medmcu_common.h"
#include "medmcu_ipi_pin.h"
#include "medmcu_mbox_layout.h"
#include "medmcu_excep.h"

#define DRAM_BUF_LEN			(1 * 1024 * 1024)
#define SCP_TIMER_TIMEOUT	    (1 * HZ) /* 1 seconds*/
#define ROUNDUP(a, b)		    (((a) + ((b)-1)) & ~((b)-1))
#define PLT_LOG_ENABLE          0x504C5402 /*magic*/
#define SCP_IPI_RETRY_TIMES     (5000)

/*
 * Define macros.
 */
#define mt_reg_sync_writel(v, a) \
	do {    \
		__raw_writel((v), (void __force __iomem *)((a)));   \
		mb(); /* memory barrier */  \
	} while (0)

/* bit0 = 1, logger is on, else off*/
#define SCP_LOGGER_ON_BIT       (1<<0)
/* bit1 = 1, logger_dram_use is on, else off*/
#define SCP_LOGGER_DRAM_ON_BIT  (1<<1)
/* bit8 = 1, enable function (logger/logger dram use) */
#define SCP_LOGGER_ON_CTRL_BIT    (1<<8)
/* bit8 = 0, disable function */
#define SCP_LOGGER_OFF_CTRL_BIT		(0<<8)
/* let logger on */
#define SCP_LOGGER_ON       (SCP_LOGGER_ON_CTRL_BIT | SCP_LOGGER_ON_BIT)
/* let logger off */
#define SCP_LOGGER_OFF      (SCP_LOGGER_OFF_CTRL_BIT | SCP_LOGGER_ON_BIT)
/* let logger dram use on */
#define SCP_LOGGER_DRAM_ON  (SCP_LOGGER_ON_CTRL_BIT | SCP_LOGGER_DRAM_ON_BIT)
/* let logger dram use off */
#define SCP_LOGGER_DRAM_OFF (SCP_LOGGER_OFF_CTRL_BIT | SCP_LOGGER_DRAM_ON_BIT)
#define SCP_LOGGER_UT (1)

#define SCP_TCM_BASE         (0x15D00000)

static void __iomem *s_medhw_base;
#define MEDHW_BASE           s_medhw_base

#define BASE_NADDR_MEDHW_INT_MNG	(0x15B13000)
#define BASE_NADDR_MEDHW_BMP		(0x15B14000)
#define BASE_NADDR_MEDHW_SSR0		(0x15B28000)
#define BASE_NADDR_MEDHW_SSR1		(0x15B38000)
#define BASE_NADDR_MEDHW_SSR2		(0x15B48000)

#define MEDHW_INT_MNG_BASE          (MEDHW_BASE + 0x3000)
#define MEDHW_BMP_BASE              (MEDHW_BASE + 0x4000)
#define MEDHW_SSR0_BASE             (MEDHW_BASE + 0x18000)
#define MEDHW_SSR1_BASE             (MEDHW_BASE + 0x28000)
#define MEDHW_SSR2_BASE             (MEDHW_BASE + 0x38000)

#define MEDHW_BMP_PBAT_RB_BASE		(MEDHW_BMP_BASE  + 0x40)
#define MEDHW_BMP_PBAT_RB_SIZE		(MEDHW_BMP_BASE  + 0x48)
#define MEDHW_BMP_FBAT_RB_BASE		(MEDHW_BMP_BASE  + 0x50)
#define MEDHW_BMP_FBAT_RB_SIZE		(MEDHW_BMP_BASE  + 0x58)
#define MEDHW_BMP_FIFO0_SW_PUSH     (MEDHW_BMP_BASE  + 0x14)

#define MTK_DPMAIF_BASE             (0x1022D000)
#define MTK_DPMAIF_RANGE            (0xE00)

#define DPMAIF_BASE                 s_dpmaif_base
static void __iomem *s_dpmaif_base;

#define BASE_DPMAIF_INT             (0x1022D400)
#define BASE_DPMAIF_DL              (0x1022DC00)
#define BASE_DPMAIF_UL              (0x1022DD00)

#define DPMAIF_INT_BASE             (DPMAIF_BASE + 0x400)
#define DPMAIF_DL_BASE              (DPMAIF_BASE + 0xC00)
#define DPMAIF_UL_BASE              (DPMAIF_BASE + 0xD00)

#ifdef MED_V2
#define MTK_MDMA_BASE               (0x15105400)
#else
#define MTK_MDMA_BASE               (0x15104000)
#endif
#define MTK_MDMA_RANGE              (0x25C)

#define MDMA_BASE                   s_mdma_base
static void __iomem *s_mdma_base;

#ifdef MED_V2
#define BASE_MDMA_TX_DESC           (0x15105400)
#define BASE_MDMA_RX_DESC           (0x15105500)
#define BASE_MDMA_TXRX_DESC_INFO    (0x15105600)
#else
#define BASE_MDMA_TX_DESC           (0x15104000)
#define BASE_MDMA_RX_DESC           (0x15104100)
#define BASE_MDMA_TXRX_DESC_INFO    (0x15104200)
#endif

#define MDMA_TX_DESC_BASE           (MDMA_BASE)
#define MDMA_RX_DESC_BASE           (MDMA_BASE + 0x100)
#define MDMA_TXRX_DESC_INFO_BASE    (MDMA_BASE + 0x200)

#ifdef MED_V2
#define MTK_FDMA_BASE               (0x15105900)
#else
#define MTK_FDMA_BASE               (0x15104500)
#endif
#define MTK_FDMA_RANGE              (0x154)

#define FDMA_BASE                   s_fdma_base
static void __iomem *s_fdma_base;

#ifdef MED_V2
#define BASE_FDMA_HNAT_INFO         (0x15105900)
#define BASE_FDMA_HNAT_INFO_INFO    (0x15105A00)
#else
#define BASE_FDMA_HNAT_INFO         (0x15104500)
#define BASE_FDMA_HNAT_INFO_INFO    (0x15104600)
#endif

#define FDMA_HNAT_INFO_BASE         (FDMA_BASE)
#define FDMA_HNAT_INFO_INFO_BASE    (FDMA_BASE + 0x100)


static struct proc_dir_entry *proc_reg_dir;
static struct proc_dir_entry *proc_dram;
static struct proc_dir_entry *proc_cr_dump;
#define PROCREG_DRAM            "dram"
#define PROCREG_CR_DUMP         "cr_dump"
#define PROCREG_DIR             "medhw"

static DEFINE_MUTEX(medhw_dram_mutex);
static DEFINE_MUTEX(medhw_cr_dump_mutex);
static unsigned int medhw_dram_logged;
static unsigned int medhw_cr_dump_logged;

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned int w_pos;
};

struct SCP_LOG_INFO {
	uint32_t scp_log_dram_addr;
	uint32_t scp_log_buf_addr;
	uint32_t scp_log_start_addr;
	uint32_t scp_log_end_addr;
	uint32_t scp_log_buf_maxlen;
};


static unsigned int scp_A_logger_inited;
static unsigned int scp_A_logger_wakeup_ap;

static struct log_ctrl_s *SCP_A_log_ctl;
static struct buffer_info_s *SCP_A_buf_info;
/*static struct timer_list scp_log_timer;*/
static DEFINE_MUTEX(scp_A_log_mutex);
static DEFINE_SPINLOCK(scp_A_log_buf_spinlock);
static struct scp_work_struct scp_logger_notify_work[SCP_CORE_TOTAL];
static struct scp_work_struct scp_logger_to_kmesg_work[SCP_CORE_TOTAL];

/*scp last log info*/
#define LAST_LOG_BUF_SIZE  4095
static struct SCP_LOG_INFO last_log_info;

static char *scp_A_last_log;
static wait_queue_head_t scp_A_logwait;

static DEFINE_MUTEX(scp_logger_mutex);
static char *scp_last_logger;
/*global value*/
unsigned int r_pos_debug;
unsigned int log_ctl_debug;
static struct mutex scp_logger_mutex;

/* ipi message buffer */
struct SCP_LOG_INFO msg_logger_init;
char msg_logger_wk[4];

/*
 * get log from scp when received a buf full notify
 * @param id:   IPI id
 * @param prdata: IPI handler parameter
 * @param data: IPI data
 * @param len:  IPI data length
 */
static int scp_logger_wakeup_handler(unsigned int id, void *prdata, void *data,
				      unsigned int len)
{
	//pr_notice("[SCP] %s(): wakeup by SCP logger\n", __func__);
	/*set a wq to enable scp logger*/
	scp_logger_to_kmesg_work[SCP_A_ID].id = SCP_A_ID;
#if SCP_LOGGER_ENABLE
	scp_schedule_logger_work(&scp_logger_to_kmesg_work[SCP_A_ID]);
#endif

	return 0;
}

/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked and should not be called in interrupt
 *       context
 * @param ws:   work struct
 */
static void scp_logger_to_kmesg_ws(struct work_struct *ws)
{
	ssize_t datalen;

	datalen = scp_A_log_read_to_kmesg();
}

/*
 * get log from scp to last_log_buf
 * @param len:  data length
 * @return:     length of log
 */
static size_t scp_A_get_last_log(size_t b_len)
{
	size_t ret = 0;
	unsigned int log_start_idx;
	unsigned int log_end_idx;
	unsigned int update_start_idx;
	unsigned char *scp_last_log_buf =
		(unsigned char *)(SCP_TCM + last_log_info.scp_log_buf_addr);

	/*pr_debug("[SCP] %s\n", __func__);*/

	if (!scp_A_logger_inited) {
		pr_err("[SCP] %s(): logger has not been init\n", __func__);
		return 0;
	}

	mutex_lock(&scp_logger_mutex);

	log_start_idx = readl((void __iomem *)(SCP_TCM +
					last_log_info.scp_log_start_addr));
	log_end_idx = readl((void __iomem *)(SCP_TCM +
					last_log_info.scp_log_end_addr));

	if (b_len > last_log_info.scp_log_buf_maxlen) {
		pr_debug("[SCP] b_len %zu > scp_log_buf_maxlen %d\n",
			b_len, last_log_info.scp_log_buf_maxlen);
		b_len = last_log_info.scp_log_buf_maxlen;
	}
	/* handle sram error */
	if (log_end_idx >= last_log_info.scp_log_buf_maxlen)
		log_end_idx = 0;

	if (log_end_idx >= b_len)
		update_start_idx = log_end_idx - b_len;
	else
		update_start_idx = last_log_info.scp_log_buf_maxlen -
					(b_len - log_end_idx) + 1;

	/* read log from scp buffer */
	ret = 0;
	if (scp_A_last_log) {
		while ((update_start_idx != log_end_idx) && ret < b_len) {
			scp_A_last_log[ret] =
				scp_last_log_buf[update_start_idx];
			update_start_idx++;
			ret++;
			if (update_start_idx >=
				last_log_info.scp_log_buf_maxlen)
				update_start_idx = update_start_idx -
					last_log_info.scp_log_buf_maxlen;

			scp_A_last_log[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs*/
		update_start_idx = log_end_idx;
	}

	mutex_unlock(&scp_logger_mutex);
	return ret;
}

static ssize_t scp_A_log_read(char __user *data, size_t len)
{
	unsigned int w_pos, r_pos, datalen;
	char *buf;

	if (!scp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&scp_A_log_mutex);

	r_pos = SCP_A_buf_info->r_pos;
	w_pos = SCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	/*debug for logger pos fail*/
	r_pos_debug = r_pos;
	log_ctl_debug = SCP_A_log_ctl->buff_ofs;
	if (r_pos >= DRAM_BUF_LEN) {
		pr_err("[SCP] %s(): r_pos >= DRAM_BUF_LEN,%x,%x\n",
			__func__, r_pos_debug, log_ctl_debug);
		datalen = 0;
		goto error;
	}

	buf = ((char *) SCP_A_log_ctl) + SCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	if (copy_to_user(data, buf, len))
		pr_debug("[SCP]copy to user buf failed..\n");

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	SCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&scp_A_log_mutex);

	return datalen;
}

ssize_t scp_A_log_read_to_kmesg(void)
{
	unsigned int w_pos, r_pos, datalen;
	char *buf;

	if (!scp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&scp_A_log_mutex);

	r_pos = SCP_A_buf_info->r_pos;
	w_pos = SCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	/*debug for logger pos fail*/
	r_pos_debug = r_pos;
	log_ctl_debug = SCP_A_log_ctl->buff_ofs;
	if (r_pos >= DRAM_BUF_LEN) {
		pr_err("[SCP] %s(): r_pos >= DRAM_BUF_LEN,%x,%x\n",
			__func__, r_pos_debug, log_ctl_debug);
		datalen = 0;
		goto error;
	}

	buf = ((char *) SCP_A_log_ctl) + SCP_A_log_ctl->buff_ofs + r_pos;

	/*memory copy from dram to kmesg*/
	//pr_notice("[SCP] %s(): medmcu log start len=%d\n", __func__, datalen);
	pr_notice("[SCP] %.*s", datalen, buf);
	//pr_notice("[SCP] %s(): medmcu log end\n", __func__);

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	SCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&scp_A_log_mutex);

	return datalen;
}

static unsigned int scp_A_log_poll(void)
{
	if (!scp_A_logger_inited)
		return 0;

	if (SCP_A_buf_info->r_pos != SCP_A_buf_info->w_pos)
		return POLLIN | POLLRDNORM;

	/*scp_log_timer_add();*/

	return 0;
}


static ssize_t medmcu_A_log_if_read(struct file *file,
		char __user *data, size_t len, loff_t *ppos)
{
	ssize_t ret;

	/*pr_debug("[SCP A] medmcu_A_log_if_read\n");*/

	ret = 0;

	/*if (access_ok(VERIFY_WRITE, data, len))*/
		ret = scp_A_log_read(data, len);

	return ret;
}

static int medmcu_A_log_if_open(struct inode *inode, struct file *file)
{
	/*pr_debug("[SCP A] medmcu_A_log_if_open\n");*/
	return nonseekable_open(inode, file);
}

static unsigned int medmcu_A_log_if_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;

	/* pr_debug("[SCP A] medmcu_A_log_if_poll\n"); */
	if (!scp_A_logger_inited)
		return 0;

	if (!(file->f_mode & FMODE_READ))
		return ret;

	/*poll_wait(file, &scp_A_logwait, wait);*/

	ret = scp_A_log_poll();

	return ret;
}
/*
 * ipi send to enable scp logger flag
 */
static unsigned int scp_A_log_enable_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (scp_A_logger_inited) {
		/*
		 *send ipi to invoke scp logger
		 */
		ret = 0;
		enable = (enable) ? SCP_LOGGER_ON : SCP_LOGGER_OFF;
		retrytimes = SCP_IPI_RETRY_TIMES;
		do {
			ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_LOGGER_ENABLE_1,
				0, &enable, PIN_OUT_SIZE_LOGGER_ENABLE_1, 0);

			if (ret == IPI_ACTION_DONE)
				break;

			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == IPI_ACTION_DONE) && (enable == SCP_LOGGER_ON))
			SCP_A_log_ctl->enable = 1;
		else if ((ret == IPI_ACTION_DONE) && (enable == SCP_LOGGER_OFF))
			SCP_A_log_ctl->enable = 0;

		if (ret != IPI_ACTION_DONE) {
			pr_err("[SCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 *ipi send enable scp logger wake up flag
 */
static unsigned int scp_A_log_wakeup_set(unsigned int enable)
{
	int ret;
	unsigned int retrytimes;

	if (scp_A_logger_inited) {
		/*
		 *send ipi to invoke scp logger
		 */
		ret = 0;
		enable = (enable) ? 1 : 0;
		retrytimes = SCP_IPI_RETRY_TIMES;
		do {
			ret = mtk_ipi_send(&scp_ipidev, IPI_OUT_LOGGER_WAKEUP_1,
				0, &enable, PIN_OUT_SIZE_LOGGER_WAKEUP_1, 0);

			if (ret == IPI_ACTION_DONE)
				break;

			retrytimes--;
			udelay(100);
		} while (retrytimes > 0);
		/*
		 *disable/enable logger flag
		 */
		if ((ret == IPI_ACTION_DONE) && (enable == 1))
			scp_A_logger_wakeup_ap = 1;
		else if ((ret == IPI_ACTION_DONE) && (enable == 0))
			scp_A_logger_wakeup_ap = 0;

		if (ret != IPI_ACTION_DONE) {
			pr_err("[SCP] %s: fail ret=%d\n", __func__, ret);
			goto error;
		}

	}

error:
	return 0;
}

/*
 * create device sysfs, scp logger status
 */
static ssize_t scp_A_mobile_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;
	int ret;
	size_t buf_size = PAGE_SIZE;

	stat = (scp_A_logger_inited && SCP_A_log_ctl->enable) ? 1 : 0;

	ret = snprintf(buf, buf_size, "[SCP A] mobile log is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");
	if (ret < 0 || ret >= buf_size)
		pr_err("[SCP A] %s: fail to write to buf, %d\n", __func__, ret);

	return ret;
}

static ssize_t scp_A_mobile_log_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_enable_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_A_mobile_log);


/*
 * create device sysfs, scp ADB cmd to set SCP wakeup AP flag
 */
static ssize_t scp_A_logger_wakeup_AP_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int stat;
	int ret;
	size_t buf_size = PAGE_SIZE;

	stat = (scp_A_logger_inited && scp_A_logger_wakeup_ap) ? 1 : 0;

	ret = snprintf(buf, buf_size, "[SCP A] logger wakeup AP is %s\n",
			(stat == 0x1) ? "enabled" : "disabled");

	if (ret < 0 || ret >= buf_size)
		pr_err("[SCP A] %s: fail to write to buf:%d\n", __func__, ret);

	return ret;
}

static ssize_t scp_A_logger_wakeup_AP_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_wakeup_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_A_logger_wakeup_AP);

/*
 * create device sysfs, scp last log show
 */
static ssize_t scp_A_get_last_log_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	int ret;
	size_t buf_size = PAGE_SIZE;

	scp_A_get_last_log(last_log_info.scp_log_buf_maxlen);
	ret = snprintf(buf, buf_size, "scp_log_buf_maxlen=%u, log=%s\n",
				last_log_info.scp_log_buf_maxlen,
				scp_A_last_log ? scp_A_last_log : "");

	if (ret < 0 || ret >= buf_size)
		pr_err("[SCP A] %s: fail to write to buf:%d\n", __func__, ret);
	return ret;
}

DEVICE_ATTR_RO(scp_A_get_last_log);

/*
 * logger UT test
 *
 */
#if SCP_LOGGER_UT
static ssize_t scp_A_mobile_log_UT_show(struct device *kobj,
		struct device_attribute *attr, char *buf)
{
	unsigned int w_pos, r_pos, datalen;
	char *logger_buf;
	size_t len = 1024;

	if (!scp_A_logger_inited)
		return 0;

	datalen = 0;

	mutex_lock(&scp_A_log_mutex);

	r_pos = SCP_A_buf_info->r_pos;
	w_pos = SCP_A_buf_info->w_pos;

	if (r_pos == w_pos)
		goto error;

	if (r_pos > w_pos)
		datalen = DRAM_BUF_LEN - r_pos; /* not wrap */
	else
		datalen = w_pos - r_pos;

	if (datalen > len)
		datalen = len;

	logger_buf = ((char *) SCP_A_log_ctl) +
			SCP_A_log_ctl->buff_ofs + r_pos;

	len = datalen;
	/*memory copy from log buf*/
	memcpy_fromio(buf, logger_buf, len);

	r_pos += datalen;
	if (r_pos >= DRAM_BUF_LEN)
		r_pos -= DRAM_BUF_LEN;

	SCP_A_buf_info->r_pos = r_pos;

error:
	mutex_unlock(&scp_A_log_mutex);

	return len;
}

static ssize_t scp_A_mobile_log_UT_store(struct device *kobj,
		struct device_attribute *attr, const char *buf, size_t n)
{
	unsigned int enable;

	if (kstrtouint(buf, 0, &enable) != 0)
		return -EINVAL;

	mutex_lock(&scp_A_log_mutex);
	scp_A_log_enable_set(enable);
	mutex_unlock(&scp_A_log_mutex);

	return n;
}

DEVICE_ATTR_RW(scp_A_mobile_log_UT);
#endif

/*
 * IPI for logger init
 * @param id:   IPI id
 * @param prdata: callback function parameter
 * @param data:  IPI data
 * @param len: IPI data length
 */
static int scp_logger_init_handler(unsigned int id, void *prdata, void *data,
				    unsigned int len)
{
	unsigned long flags;
	struct SCP_LOG_INFO *log_info = (struct SCP_LOG_INFO *)data;
	struct arm_smccc_res ares;

	pr_debug("[SCP]scp_get_reserve_mem_phys=%llx\n",
		(uint64_t)scp_get_reserve_mem_phys(SCP_A_LOGGER_MEM_ID));

	spin_lock_irqsave(&scp_A_log_buf_spinlock, flags);

	/* sync scp last log information*/
	last_log_info.scp_log_dram_addr = log_info->scp_log_dram_addr;
	last_log_info.scp_log_buf_addr = log_info->scp_log_buf_addr;
	last_log_info.scp_log_start_addr = log_info->scp_log_start_addr;
	last_log_info.scp_log_end_addr = log_info->scp_log_end_addr;
	last_log_info.scp_log_buf_maxlen = log_info->scp_log_buf_maxlen;

	/* setting dram ctrl config to scp*/
	/* scp side get wakelock, AP to write info to scp sram*/
	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_LOG_DRAM,
		scp_get_reserve_mem_phys(SCP_A_LOGGER_MEM_ID),
		last_log_info.scp_log_dram_addr, 0, 0, 0, 0,
		&ares);

	/* set init flag here*/
	scp_A_logger_inited = 1;

	spin_unlock_irqrestore(&scp_A_log_buf_spinlock, flags);

	/*set a wq to enable scp logger*/
	scp_logger_notify_work[SCP_A_ID].id = SCP_A_ID;
#if SCP_LOGGER_ENABLE
	scp_schedule_logger_work(&scp_logger_notify_work[SCP_A_ID]);
#endif

	return 0;
}

/*
 * callback function for work struct
 * notify apps to start their tasks or generate an exception according to flag
 * NOTE: this function may be blocked and should not be called in interrupt
 *       context
 * @param ws:   work struct
 */
static void scp_logger_notify_ws(struct work_struct *ws)
{
	unsigned int retrytimes;
	int ret = 0;
	unsigned int scp_ipi_id;
	unsigned int dram_info[2];

	scp_ipi_id = IPI_OUT_LOGGER_INIT_1;
	dram_info[0] = scp_get_reserve_mem_phys(SCP_A_LOGGER_MEM_ID);
	dram_info[1] = scp_get_reserve_mem_size(SCP_A_LOGGER_MEM_ID);

	pr_notice("[SCP] %s: id=%u\n", __func__, scp_ipi_id);
	/*
	 *send ipi to invoke scp logger
	 */
	retrytimes = SCP_IPI_RETRY_TIMES;
	do {
		ret = mtk_ipi_send(&scp_ipidev, scp_ipi_id, 0, dram_info,
				   PIN_OUT_SIZE_LOGGER_INIT_1, 0);

		if ((retrytimes % 500) == 0)
			pr_debug("[SCP] %s: ipi ret=%d\n", __func__, ret);

		if (ret == IPI_ACTION_DONE)
			break;

		retrytimes--;
		udelay(2000);
	} while (retrytimes > 0);

	/*enable logger flag*/
	if (ret == IPI_ACTION_DONE)
		SCP_A_log_ctl->enable = 1;
	else {
		/*scp logger ipi init fail but still let logger dump*/
		SCP_A_log_ctl->enable = 1;
		pr_err("[SCP]logger initial fail, ipi ret=%d\n", ret);
	}

}

int print_medhw_dram(struct seq_file *seq)
{
	u32 i;
	u32 *p;
	char buf[100];
	size_t buf_size = sizeof(buf);
	struct medmcu_desc_info_t *desc = get_desc_info();
	int ret;

	pr_notice("Dump TX_DESC...\n");
	p = (u32 *)desc[TX_DESC].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<TX_DESC DUMP>>\n");
	else
		pr_notice("        <<TX_DESC DUMP>>\n");
	for (i = 0; i < desc[TX_DESC].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
			    desc[TX_DESC].phy_base + i*4,
			    *(p + i), *(p + i + 1),
			    *(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[TX_DESC DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump RX_DESC...\n");
	p = (u32 *)desc[RX_DESC].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<RX_DESC DUMP>>\n");
	else
		pr_notice("        <<RX_DESC DUMP>>\n");
	for (i = 0; i < desc[RX_DESC].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[RX_DESC].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[RX_DESC DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

#ifdef MED_V2
	pr_notice("Dump RX_DESC1...\n");
	p = (u32 *)desc[RX_DESC_1].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<RX_DESC_1 DUMP>>\n");
	else
		pr_notice("        <<RX_DESC_1 DUMP>>\n");
	for (i = 0; i < desc[RX_DESC_1].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[RX_DESC_1].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[RX_DESC_1 DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}
#endif

	pr_notice("Dump HNAT_INFO...\n");
	p = (u32 *)desc[HNAT_INFO].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<HNAT_INFO DUMP>>\n");
	else
		pr_notice("        <<HNAT_INFO DUMP>>\n");
	for (i = 0; i < desc[HNAT_INFO].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[HNAT_INFO].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[HNAT_INFO DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump HNAT_INFO_HOST...\n");
	p = (u32 *)desc[HNAT_INFO_HOST].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<HNAT_INFO_HOST DUMP>>\n");
	else
		pr_notice("        <<HNAT_INFO_HOST DUMP>>\n");
	for (i = 0; i < desc[HNAT_INFO_HOST].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[HNAT_INFO_HOST].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[HNAT_INFO_HOST DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump PIT_NAT...\n");
	p = (u32 *)desc[PIT_NAT].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<PIT_NAT DUMP>>\n");
	else
		pr_notice("        <<PIT_NAT DUMP>>\n");
	for (i = 0; i < desc[PIT_NAT].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[PIT_NAT].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[PIT_NAT DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump PIT...\n");
	p = (u32 *)desc[PIT].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<PIT DUMP>>\n");
	else
		pr_notice("        <<PIT DUMP>>\n");
	for (i = 0; i < desc[PIT].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[PIT].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[PIT DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump DRB0...\n");
	p = (u32 *)desc[DRB0].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<DRB0 DUMP>>\n");
	else
		pr_notice("        <<DRB0 DUMP>>\n");
	for (i = 0; i < desc[DRB0].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[DRB0].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DRB0 DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump DRB1...\n");
	p = (u32 *)desc[DRB1].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<DRB1 DUMP>>\n");
	else
		pr_notice("        <<DRB1 DUMP>>\n");
	for (i = 0; i < desc[DRB1].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[DRB1].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DRB1 DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

#ifdef MED_V2
	pr_notice("Dump DRB2...\n");
	p = (u32 *)desc[DRB2].base_virt;
	if (seq != NULL)
		seq_puts(seq, "        <<DRB2 DUMP>>\n");
	else
		pr_notice("        <<DRB2 DUMP>>\n");
	for (i = 0; i < desc[DRB2].sz / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08llx : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				desc[DRB2].phy_base + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DRB2 DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}
#endif

	medhw_dram_logged = 1;
	pr_notice("[MEDHW] read dram end\n");

	return 0;
}

static int sw_map_flush(bool frag)
{
	unsigned int cmd;
	unsigned char frag_val;
	int count = 0;

	frag_val = (frag) ? 1 : 0;
	cmd = (0 & 0xffff) | ((0 & 0x3ff) << 16) | (frag_val << 27) | (1 << 30) | (1 << 31);
	reg_write(MEDHW_BMP_FIFO0_SW_PUSH, cmd);
	while (1) {
		if ((reg_read(MEDHW_BMP_FIFO0_SW_PUSH) & (1<<31)) == 0)
			break;
		if (++count >= 1600000) {
			pr_err("[SCP] %s(): flush fail\n", __func__);
			return 0;
		}
	}
	return 1;
}

int print_medhw_cr_dump(struct seq_file *seq)
{
	u32 i;
	u32 *p;
	char buf[100];
	size_t buf_size = sizeof(buf);
	int ret;

	uint32_t bat_addr = reg_read(MEDHW_BMP_PBAT_RB_BASE);
	uint32_t fbat_addr = reg_read(MEDHW_BMP_FBAT_RB_BASE);
	uint32_t bat_size = reg_read(MEDHW_BMP_PBAT_RB_SIZE);
	uint32_t fbat_size = reg_read(MEDHW_BMP_FBAT_RB_SIZE);

	unsigned char *medhw_bat_sw_map_buf = (unsigned char *)(SCP_TCM + bat_addr);
	unsigned char *medhw_fbat_sw_map_buf = (unsigned char *)(SCP_TCM + fbat_addr);

	pr_notice("Dump MEDHW_INT_MNG...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MEDHW_INT_MNG CR DUMP>>\n");
	else
		pr_notice("       <<MEDHW_INT_MNG CR DUMP>>\n");
#ifdef MED_V2
	for (i = 0; i < 0x98; i = i + 0x10) {
#else
	for (i = 0; i < 0x64; i = i + 0x10) {
#endif
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_NADDR_MEDHW_INT_MNG + i,
				reg_read(MEDHW_INT_MNG_BASE + i),
				reg_read(MEDHW_INT_MNG_BASE + i + 4),
				reg_read(MEDHW_INT_MNG_BASE + i + 8),
				reg_read(MEDHW_INT_MNG_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MEDHW_INT_MNG CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MEDHW_BMP...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MEDHW_BMP CR DUMP>>\n");
	else
		pr_notice("       <<MEDHW_BMP CR DUMP>>\n");
	for (i = 0; i < 0x80; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_NADDR_MEDHW_BMP + i,
				reg_read(MEDHW_BMP_BASE + i),
				reg_read(MEDHW_BMP_BASE + i + 4),
				reg_read(MEDHW_BMP_BASE + i + 8),
				reg_read(MEDHW_BMP_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MEDHW_BMP CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MEDHW_SSR0...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MEDHW_SSR0 CR DUMP>>\n");
	else
		pr_notice("       <<MEDHW_SSR0 CR DUMP>>\n");
	for (i = 0; i < 0x230; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_NADDR_MEDHW_SSR0 + i,
				reg_read(MEDHW_SSR0_BASE + i),
				reg_read(MEDHW_SSR0_BASE + i + 4),
				reg_read(MEDHW_SSR0_BASE + i + 8),
				reg_read(MEDHW_SSR0_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MEDHW_SSR0 CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MEDHW_SSR1...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MEDHW_SSR1 CR DUMP>>\n");
	else
		pr_notice("       <<MEDHW_SSR1 CR DUMP>>\n");
	for (i = 0; i < 0x230; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_NADDR_MEDHW_SSR1 + i,
				reg_read(MEDHW_SSR1_BASE + i),
				reg_read(MEDHW_SSR1_BASE + i + 4),
				reg_read(MEDHW_SSR1_BASE + i + 8),
				reg_read(MEDHW_SSR1_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MEDHW_SSR1 CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MEDHW_SSR2...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MEDHW_SSR2 CR DUMP>>\n");
	else
		pr_notice("       <<MEDHW_SSR2 CR DUMP>>\n");
#ifdef MED_V2
	for (i = 0; i < 0x278; i = i + 0x10) {
#else
	for (i = 0; i < 0x230; i = i + 0x10) {
#endif
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_NADDR_MEDHW_SSR2 + i,
				reg_read(MEDHW_SSR2_BASE + i),
				reg_read(MEDHW_SSR2_BASE + i + 4),
				reg_read(MEDHW_SSR2_BASE + i + 8),
				reg_read(MEDHW_SSR2_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MEDHW_SSR2 CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump DPMAIF_INT...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<DPMAIF_INT CR DUMP>>\n");
	else
		pr_notice("       <<DPMAIF_INT CR DUMP>>\n");
	for (i = 0; i < 0x100; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_DPMAIF_INT + i,
				reg_read(DPMAIF_INT_BASE + i),
				reg_read(DPMAIF_INT_BASE + i + 4),
				reg_read(DPMAIF_INT_BASE + i + 8),
				reg_read(DPMAIF_INT_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DPMAIF_INT CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump DPMAIF_DL...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<DPMAIF_DL CR DUMP>>\n");
	else
		pr_notice("       <<DPMAIF_DL CR DUMP>>\n");
	for (i = 0; i < 0x100; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_DPMAIF_DL + i,
				reg_read(DPMAIF_DL_BASE + i),
				reg_read(DPMAIF_DL_BASE + i + 4),
				reg_read(DPMAIF_DL_BASE + i + 8),
				reg_read(DPMAIF_DL_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DPMAIF_DL CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump DPMAIF_UL...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<DPMAIF_UL CR DUMP>>\n");
	else
		pr_notice("       <<DPMAIF_UL CR DUMP>>\n");
	for (i = 0; i < 0x100; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_DPMAIF_UL + i,
				reg_read(DPMAIF_UL_BASE + i),
				reg_read(DPMAIF_UL_BASE + i + 4),
				reg_read(DPMAIF_UL_BASE + i + 8),
				reg_read(DPMAIF_UL_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[DPMAIF_UL CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MDMA_TX_DESC...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MDMA_TX_DESC CR DUMP>>\n");
	else
		pr_notice("       <<MDMA_TX_DESC CR DUMP>>\n");
	for (i = 0; i < 0x10; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_MDMA_TX_DESC + i,
				reg_read(MDMA_TX_DESC_BASE + i),
				reg_read(MDMA_TX_DESC_BASE + i + 4),
				reg_read(MDMA_TX_DESC_BASE + i + 8),
				reg_read(MDMA_TX_DESC_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MDMA_TX_DESC CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MDMA_RX_DESC...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MDMA_RX_DESC CR DUMP>>\n");
	else
		pr_notice("       <<MDMA_RX_DESC CR DUMP>>\n");
#ifdef MED_V2
	for (i = 0; i < 0x20; i = i + 0x10) {
#else
	for (i = 0; i < 0x10; i = i + 0x10) {
#endif
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_MDMA_RX_DESC + i,
				reg_read(MDMA_RX_DESC_BASE + i),
				reg_read(MDMA_RX_DESC_BASE + i + 4),
				reg_read(MDMA_RX_DESC_BASE + i + 8),
				reg_read(MDMA_RX_DESC_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MDMA_RX_DESC CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump MDMA_TXRX_DESC_INFO...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<MDMA_TXRX_DESC_INFO CR DUMP>>\n");
	else
		pr_notice("       <<MDMA_TXRX_DESC_INFO CR DUMP>>\n");
	for (i = 0; i < 0x50; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_MDMA_TXRX_DESC_INFO + i,
				reg_read(MDMA_TXRX_DESC_INFO_BASE + i),
				reg_read(MDMA_TXRX_DESC_INFO_BASE + i + 4),
				reg_read(MDMA_TXRX_DESC_INFO_BASE + i + 8),
				reg_read(MDMA_TXRX_DESC_INFO_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[MDMA_TXRX_DESC CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump FDMA_HNAT_INFO...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<FDMA_HNAT_INFO CR DUMP>>\n");
	else
		pr_notice("       <<FDMA_HNAT_INFO CR DUMP>>\n");
	for (i = 0; i < 0x10; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_FDMA_HNAT_INFO + i,
				reg_read(FDMA_HNAT_INFO_BASE + i),
				reg_read(FDMA_HNAT_INFO_BASE + i + 4),
				reg_read(FDMA_HNAT_INFO_BASE + i + 8),
				reg_read(FDMA_HNAT_INFO_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[FDMA_HNAT_INFO CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	pr_notice("Dump FDMA_HNAT_INFO...\n");
	if (seq != NULL)
		seq_puts(seq, "       <<FDMA_HNAT_INFO_INFO CR DUMP>>\n");
	else
		pr_notice("       <<FDMA_HNAT_INFO_INFO CR DUMP>>\n");
	for (i = 0; i < 0x50; i = i + 0x10) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				BASE_FDMA_HNAT_INFO_INFO + i,
				reg_read(FDMA_HNAT_INFO_INFO_BASE + i),
				reg_read(FDMA_HNAT_INFO_INFO_BASE + i + 4),
				reg_read(FDMA_HNAT_INFO_INFO_BASE + i + 8),
				reg_read(FDMA_HNAT_INFO_INFO_BASE + i + 0xc));
		if (ret < 0 || ret >= buf_size)
			pr_err("[FDMA_HNAT_INFO_INFO CR DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}
	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}


	if (!sw_map_flush(false)) {
		seq_puts(seq, "BAT_SW_MAP flush error\n");
		return 0;
	}
	if (!sw_map_flush(true)) {
		seq_puts(seq, "FBAT_SW_MAP flush error\n");
		return 0;
	}

	pr_notice("Dump BAT_SW_MAP...\n");
	p = (u32 *)medhw_bat_sw_map_buf;
	if (seq != NULL)
		seq_puts(seq, "       <<BAT_SW_MAP DUMP>>\n");
	else
		pr_notice("       <<BAT_SW_MAP DUMP>>\n");
	for (i = 0; i < (bat_size >> 2) / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				SCP_TCM_BASE + bat_addr + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[BAT_SW_MAP DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}

	pr_notice("Dump FBAT_SW_MAP...\n");
	p = (u32 *)medhw_fbat_sw_map_buf;
	if (seq != NULL)
		seq_puts(seq, "       <<FBAT_SW_MAP DUMP>>\n");
	else
		pr_notice("       <<FBAT_SW_MAP DUMP>>\n");
	for (i = 0; i < (fbat_size >> 2) / 4; i = i + 4) {
		ret = snprintf(buf, buf_size, "0x%08x : 0x%08x 0x%08x 0x%08x 0x%08x\n",
				SCP_TCM_BASE + fbat_addr + i*4,
				*(p + i), *(p + i + 1),
				*(p + i + 2), *(p + i + 3));
		if (ret < 0 || ret >= buf_size)
			pr_err("[FBAT_SW_MAP DUMP]: fail to write to buf\n");
		if (seq != NULL)
			seq_printf(seq, "%s", buf);
		else
			pr_notice("[MEDHW] %s", buf);
	}

	if (seq != NULL) {
		seq_puts(seq, "+-----------------------------------------------+\n");
		seq_puts(seq, "\n");
	} else {
		pr_notice("+-----------------------------------------------+\n");
	}

	medhw_cr_dump_logged = 1;
	pr_notice("[MEDHW] read cr_dump end\n");

	return 0;
}

static int dram_seq_show(struct seq_file *seq, void *v)
{
    return print_medhw_dram(seq);
}

static int dram_seq_open(struct inode *inode, struct file *file)
{
	pr_notice("[MEDHW] read dram start\n");
	return single_open_size(file, dram_seq_show, NULL, (PAGE_SIZE << 11));
}

static const struct proc_ops dram_fops = {
	.proc_open	 = dram_seq_open,
	.proc_read	 = seq_read,
	.proc_lseek	 = seq_lseek,
	.proc_release = single_release,
};

static int cr_dump_seq_show(struct seq_file *seq, void *v)
{
	return print_medhw_cr_dump(seq);
}

static int cr_dump_seq_open(struct inode *inode, struct file *file)
{
	pr_notice("[MEDHW] read cr_dump start\n");
	return single_open_size(file, cr_dump_seq_show, NULL, (PAGE_SIZE << 4));
}

static const struct proc_ops cr_dump_fops = {
	.proc_open	 = cr_dump_seq_open,
	.proc_read	 = seq_read,
	.proc_lseek	 = seq_lseek,
	.proc_release = single_release,
};

/******************************************************************************
 * init scp logger dram ctrl structure
 * @return:     -1: fail, otherwise: end of buffer
 *****************************************************************************/
int scp_logger_init(phys_addr_t start, phys_addr_t limit)
{
	int last_ofs;

    /* init hw base */
	s_medhw_base = get_medhw_base();

	/*init wait queue*/
	init_waitqueue_head(&scp_A_logwait);
	scp_A_logger_wakeup_ap = 0;
	mutex_init(&scp_logger_mutex);

	/*init work queue*/
	INIT_WORK(&scp_logger_notify_work[SCP_A_ID].work, scp_logger_notify_ws);
	INIT_WORK(&scp_logger_to_kmesg_work[SCP_A_ID].work, scp_logger_to_kmesg_ws);

	/*init dram ctrl table*/
	last_ofs = 0;
#ifdef CONFIG_ARM64
	SCP_A_log_ctl = (struct log_ctrl_s *) start;
#else
	/* plz fix origial ptr to phys_addr flow */
	SCP_A_log_ctl = (struct log_ctrl_s *) (u32) start;
#endif
	SCP_A_log_ctl->base = PLT_LOG_ENABLE; /* magic */
	SCP_A_log_ctl->enable = 0;
	SCP_A_log_ctl->size = sizeof(*SCP_A_log_ctl);

	last_ofs += SCP_A_log_ctl->size;
	SCP_A_log_ctl->info_ofs = last_ofs;

	last_ofs += sizeof(*SCP_A_buf_info);
	last_ofs = ROUNDUP(last_ofs, 4);
	SCP_A_log_ctl->buff_ofs = last_ofs;
	SCP_A_log_ctl->buff_size = DRAM_BUF_LEN;

	SCP_A_buf_info = (struct buffer_info_s *)
		(((unsigned char *) SCP_A_log_ctl) + SCP_A_log_ctl->info_ofs);
	SCP_A_buf_info->r_pos = 0;
	SCP_A_buf_info->w_pos = 0;

	last_ofs += SCP_A_log_ctl->buff_size;

	if (last_ofs >= limit) {
		pr_err("[SCP]:%s() initial fail, last_ofs=%u, limit=%u\n",
			__func__, last_ofs, (unsigned int) limit);
		goto error;
	}

	/* init last log buffer*/
	last_log_info.scp_log_buf_maxlen = LAST_LOG_BUF_SIZE;
	if (!scp_A_last_log) {
		/* Allocate one more byte for the NULL character. */
		scp_A_last_log = vmalloc(last_log_info.scp_log_buf_maxlen + 1);
	}

	/* register logger ini IPI */
	mtk_ipi_register(&scp_ipidev, IPI_IN_LOGGER_INIT_1,
			(void *)scp_logger_init_handler, NULL,
			&msg_logger_init);

	/* register log wakeup IPI */
	mtk_ipi_register(&scp_ipidev, IPI_IN_LOGGER_WAKEUP_1,
			(void *)scp_logger_wakeup_handler, NULL,
			msg_logger_wk);

	scp_A_logger_inited = 1;

	s_dpmaif_base = ioremap(MTK_DPMAIF_BASE, MTK_DPMAIF_RANGE);
	s_mdma_base = ioremap(MTK_MDMA_BASE, MTK_MDMA_RANGE);
	s_fdma_base = ioremap(MTK_FDMA_BASE, MTK_FDMA_RANGE);

	medhw_dram_logged = 0;
	medhw_cr_dump_logged = 0;

	if (!proc_reg_dir)
		proc_reg_dir = proc_mkdir(PROCREG_DIR, NULL);

	proc_dram =
		proc_create(PROCREG_DRAM, 0, proc_reg_dir, &dram_fops);
	if (!proc_dram)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_DRAM);

	proc_cr_dump =
	    proc_create(PROCREG_CR_DUMP, 0, proc_reg_dir, &cr_dump_fops);
	if (!proc_cr_dump)
		pr_notice("!! FAIL to create %s PROC !!\n", PROCREG_CR_DUMP);

	return last_ofs;

error:
	medhw_dram_logged = 0;
	medhw_cr_dump_logged = 0;
	scp_A_logger_inited = 0;
	SCP_A_log_ctl = NULL;
	SCP_A_buf_info = NULL;
	return -1;
}

void scp_logger_uninit(void)
{
	char *tmp = scp_A_last_log;

	medhw_dram_logged = 0;
	medhw_cr_dump_logged = 0;

	scp_A_logger_inited = 0;
	scp_A_last_log = NULL;
	if (tmp)
		vfree(tmp);
	iounmap(s_dpmaif_base);
	iounmap(s_mdma_base);
	iounmap(s_fdma_base);
}

const struct file_operations medmcu_A_log_file_ops = {
	.owner = THIS_MODULE,
	.read = medmcu_A_log_if_read,
	.open = medmcu_A_log_if_open,
	.poll = medmcu_A_log_if_poll,
};

/*
 * move scp last log from sram to dram
 * NOTE: this function may be blocked
 * @param scp_core_id:  fill scp id to get last log
 */
static void scp_crash_log_move_to_buf(enum scp_core_id scp_id)
{
	int pos;
	unsigned int ret;
	unsigned int length;
	unsigned int log_buf_idx;    /* SCP log buf pointer */
	unsigned int log_start_idx;  /* SCP log start pointer */
	unsigned int log_end_idx;    /* SCP log end pointer */
	unsigned int w_pos;          /* buf write pointer */
	char *pre_scp_logger_buf;
	char *dram_logger_buf;       /* dram buffer */

	char *crash_message = "****SCP EE LOG DUMP****\n";
	unsigned char *scp_logger_buf = (unsigned char *)(SCP_TCM +
					last_log_info.scp_log_buf_addr);

	if (!scp_A_logger_inited && scp_id == SCP_A_ID) {
		pr_err("[SCP] %s(): logger has not been init\n", __func__);
		return;
	}

	mutex_lock(&scp_logger_mutex);

	log_buf_idx = readl((void __iomem *)(SCP_TCM +
				last_log_info.scp_log_buf_addr));
	log_start_idx = readl((void __iomem *)(SCP_TCM +
				last_log_info.scp_log_start_addr));
	log_end_idx = readl((void __iomem *)(SCP_TCM +
				last_log_info.scp_log_end_addr));

	/* if loggger_r/w_pos was messed up, dump all message in logger_buf */
	if (((log_start_idx < log_buf_idx + last_log_info.scp_log_buf_maxlen)
	    || (log_start_idx >= log_buf_idx))
	    && ((log_end_idx < log_buf_idx + last_log_info.scp_log_buf_maxlen)
	    || (log_end_idx >= log_buf_idx))) {

		if (log_end_idx >= log_start_idx)
			length = log_end_idx - log_start_idx;
		else
			length = last_log_info.scp_log_buf_maxlen -
				(log_start_idx - log_end_idx);
	} else {
		length = last_log_info.scp_log_buf_maxlen;
		log_start_idx = log_buf_idx;
		log_end_idx = log_buf_idx + length - 1;
	}

	if (length >= last_log_info.scp_log_buf_maxlen) {
		pr_err("[SCP] %s: length >= max\n", __func__);
		length = last_log_info.scp_log_buf_maxlen;
	}

	pre_scp_logger_buf = scp_last_logger;
	scp_last_logger = vmalloc(length + strlen(crash_message) + 1);

	/* read log from scp buffer */
	ret = 0;
	if (scp_last_logger) {
		ret += snprintf(scp_last_logger, strlen(crash_message),
			crash_message);
		ret--;
		while ((log_start_idx != log_end_idx) &&
			ret <= (length + strlen(crash_message))) {
			scp_last_logger[ret] = scp_logger_buf[log_start_idx];
			log_start_idx++;
			ret++;
			if (log_start_idx >= last_log_info.scp_log_buf_maxlen)
				log_start_idx = log_start_idx -
					last_log_info.scp_log_buf_maxlen;

			scp_last_logger[ret] = '\0';
		}
	} else {
		/* no buffer, just skip logs */
		log_start_idx = log_end_idx;
	}

	if (ret != 0) {
		/* get buffer w pos */
		w_pos = SCP_A_buf_info->w_pos;

		if (w_pos >= DRAM_BUF_LEN) {
			pr_err("[SCP] %s(): w_pos >= DRAM_BUF_LEN, w_pos=%u",
				__func__, w_pos);
			return;
		}

		/* copy to dram buffer */
		dram_logger_buf = ((char *) SCP_A_log_ctl) +
		    SCP_A_log_ctl->buff_ofs + w_pos;

		/* memory copy from log buf */
		pos = 0;
		while ((pos != ret) && pos <= ret) {
			*dram_logger_buf = scp_last_logger[pos];
			pos++;
			w_pos++;
			dram_logger_buf++;
			if (w_pos >= DRAM_BUF_LEN) {
				/* warp */
				pr_err("[SCP] %s: dram warp\n", __func__);
				w_pos = 0;

				dram_logger_buf = ((char *) SCP_A_log_ctl) +
					SCP_A_log_ctl->buff_ofs;
			}
		}
		/* update write pointer */
		SCP_A_buf_info->w_pos = w_pos;
	}

	mutex_unlock(&scp_logger_mutex);
	vfree(pre_scp_logger_buf);
}



/*
 * get log from scp and optionally save it
 * NOTE: this function may be blocked
 * @param scp_core_id:  fill scp id to get last log
 */
void scp_get_log(enum scp_core_id scp_id)
{
	pr_debug("[SCP] %s\n", __func__);
#if SCP_LOGGER_ENABLE
	scp_A_get_last_log(last_log_info.scp_log_buf_maxlen);
	/*move last log to dram*/
	scp_crash_log_move_to_buf(scp_id);
#endif
}

/*
 * return useful log for aee issue dispatch
 */
#define CMP_SAFT_RANGE	176
#define DEFAULT_IDX (last_log_info.scp_log_buf_maxlen/3)
char *scp_pickup_log_for_aee(void)
{
	char *last_log;
	int i;
	char keyword1[] = "coredump";
	char keyword2[] = "exception";

	if (scp_A_last_log == NULL)
		return NULL;
	last_log = &scp_A_last_log[DEFAULT_IDX]; /* default value */

	for (i = last_log_info.scp_log_buf_maxlen; i >= CMP_SAFT_RANGE; i--) {
		if (scp_A_last_log[i-0] != keyword1[7])
			continue;
		if (scp_A_last_log[i-1] != keyword1[6])
			continue;
		if (scp_A_last_log[i-2] != keyword1[5])
			continue;
		if (scp_A_last_log[i-3] != keyword1[4])
			continue;
		if (scp_A_last_log[i-4] != keyword1[3])
			continue;
		if (scp_A_last_log[i-5] != keyword1[2])
			continue;
		if (scp_A_last_log[i-6] != keyword1[1])
			continue;
		if (scp_A_last_log[i-7] != keyword1[0])
			continue;
		last_log = &scp_A_last_log[i-CMP_SAFT_RANGE];
		return last_log;
	}

	for (i = last_log_info.scp_log_buf_maxlen; i >= CMP_SAFT_RANGE; i--) {
		if (scp_A_last_log[i-0] != keyword2[8])
			continue;
		if (scp_A_last_log[i-1] != keyword2[7])
			continue;
		if (scp_A_last_log[i-2] != keyword2[6])
			continue;
		if (scp_A_last_log[i-3] != keyword2[5])
			continue;
		if (scp_A_last_log[i-4] != keyword2[4])
			continue;
		if (scp_A_last_log[i-5] != keyword2[3])
			continue;
		if (scp_A_last_log[i-6] != keyword2[2])
			continue;
		if (scp_A_last_log[i-7] != keyword2[1])
			continue;
		if (scp_A_last_log[i-8] != keyword2[0])
			continue;
		last_log = &scp_A_last_log[i-CMP_SAFT_RANGE];
		return last_log;
	}
	return last_log;
}

/*
 * clear medhw_dram_logged and medhw_cr_dump_logged
 */
void clear_medhw_logged(void)
{
	medhw_dram_logged = 0;
	medhw_cr_dump_logged = 0;
}

/*
 * check if medhw_dram_logged and medhw_cr_dump_logged
 */
bool is_medhw_logged_ready(void)
{
	return ((medhw_dram_logged == 1) && (medhw_cr_dump_logged == 1));
}

/*
 * set scp_A_logger_inited
 */
void scp_logger_init_set(unsigned int value)
{
	/*scp_A_logger_inited
	 *  0: logger not init
	 *  1: logger inited
	 */
	scp_A_logger_inited = value;
}
