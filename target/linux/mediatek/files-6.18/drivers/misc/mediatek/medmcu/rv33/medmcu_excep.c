// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/vmalloc.h>      /* needed by vmalloc */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/workqueue.h>
#include <linux/io.h>
#include <linux/mutex.h>
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#endif
//#include <mt-plat/sync_write.h>
#include <linux/sched_clock.h>
#include <linux/ratelimit.h>
#include <linux/delay.h>
#include "medmcu_helper.h"
#include "medmcu_common.h"
#include "medmcu_feature_define.h"
#include "medmcu_ipi_pin.h"
#include "medmcu_excep.h"
#include "medmcu_l1c.h"


struct scp_dump_st {
	uint8_t *detail_buff;
	uint8_t *ramdump;
	uint32_t ramdump_length;
};

struct reg_save_st {
	uint32_t addr;
	uint32_t size;
};

struct reg_save_st reg_save_list[] = {
	/* size must 16 byte alignment */
	{(L1TCM_BASE + 0x21000), 0x120},
	{(L1TCM_BASE + 0x24000), 0x170},
	{(L1TCM_BASE + 0x30000), 0x120},
	{(L1TCM_BASE + 0x32000), 0x260},
	{(L1TCM_BASE + 0x33000), 0x120},
	{(L1TCM_BASE + 0x40000), 0x120},
	{(L1TCM_BASE + 0x42000), 0x260},
	{(L1TCM_BASE + 0x43000), 0x120},
	{(L1TCM_BASE + 0x50000), 0x330},
	{(L1TCM_BASE + 0x51000), 0x10},
	{(L1TCM_BASE + 0x51400), 0x70},
	{(L1TCM_BASE + 0x52000), 0x340},
	{(L1TCM_BASE + 0xA5000), 0x110},
};

//static unsigned char *scp_A_dump_buffer;
struct scp_dump_st scp_dump;

//static unsigned int scp_A_dump_length;
static unsigned int scp_A_task_context_addr;

struct scp_status_reg c0_m;
struct scp_status_reg c1_m;

static struct mutex scp_excep_mutex;
int scp_ee_enable;
int scp_reset_counts = 100000;

void scp_dump_last_regs(void)
{
	c0_m.status = readl(R_CORE0_STATUS);
	c0_m.pc = readl(R_CORE0_MON_PC);
	c0_m.lr = readl(R_CORE0_MON_LR);
	c0_m.sp = readl(R_CORE0_MON_SP);
	c0_m.pc_latch = readl(R_CORE0_MON_PC_LATCH);
	c0_m.lr_latch = readl(R_CORE0_MON_LR_LATCH);
	c0_m.sp_latch = readl(R_CORE0_MON_SP_LATCH);
	c1_m.status = readl(R_CORE1_STATUS);
	c1_m.pc = readl(R_CORE1_MON_PC);
	c1_m.lr = readl(R_CORE1_MON_LR);
	c1_m.sp = readl(R_CORE1_MON_SP);
	c1_m.pc_latch = readl(R_CORE1_MON_PC_LATCH);
	c1_m.lr_latch = readl(R_CORE1_MON_LR_LATCH);
	c1_m.sp_latch = readl(R_CORE1_MON_SP_LATCH);

	pr_debug("[SCP] c0_status = %08x\n", c0_m.status);
	pr_debug("[SCP] c0_pc = %08x\n", c0_m.pc);
	pr_debug("[SCP] c0_lr = %08x\n", c0_m.lr);
	pr_debug("[SCP] c0_sp = %08x\n", c0_m.sp);
	pr_debug("[SCP] c0_pc_latch = %08x\n", c0_m.pc_latch);
	pr_debug("[SCP] c0_lr_latch = %08x\n", c0_m.lr_latch);
	pr_debug("[SCP] c0_sp_latch = %08x\n", c0_m.sp_latch);
	pr_debug("[SCP] c1_status = %08x\n", c1_m.status);
	pr_debug("[SCP] c1_pc = %08x\n", c1_m.pc);
	pr_debug("[SCP] c1_lr = %08x\n", c1_m.lr);
	pr_debug("[SCP] c1_sp = %08x\n", c1_m.sp);
	pr_debug("[SCP] c1_pc_latch = %08x\n", c1_m.pc_latch);
	pr_debug("[SCP] c1_lr_latch = %08x\n", c1_m.lr_latch);
	pr_debug("[SCP] c1_sp_latch = %08x\n", c1_m.sp_latch);

	/* bus tracker reg dump */
	pr_notice("BUS DBG CON: %x\n", readl(SCP_BUS_DBG_CON));
	pr_notice("R %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(SCP_BUS_DBG_AR_TRACK0_L),
			readl(SCP_BUS_DBG_AR_TRACK1_L),
			readl(SCP_BUS_DBG_AR_TRACK2_L),
			readl(SCP_BUS_DBG_AR_TRACK3_L),
			readl(SCP_BUS_DBG_AR_TRACK4_L),
			readl(SCP_BUS_DBG_AR_TRACK5_L),
			readl(SCP_BUS_DBG_AR_TRACK6_L),
			readl(SCP_BUS_DBG_AR_TRACK7_L)
		   );
	pr_debug("W %08x %08x %08x %08x %08x %08x %08x %08x\n",
			readl(SCP_BUS_DBG_AW_TRACK0_L),
			readl(SCP_BUS_DBG_AW_TRACK1_L),
			readl(SCP_BUS_DBG_AW_TRACK2_L),
			readl(SCP_BUS_DBG_AW_TRACK3_L),
			readl(SCP_BUS_DBG_AW_TRACK4_L),
			readl(SCP_BUS_DBG_AW_TRACK5_L),
			readl(SCP_BUS_DBG_AW_TRACK6_L),
			readl(SCP_BUS_DBG_AW_TRACK7_L)
		   );
}

static void scp_do_regdump(uint32_t *out, uint32_t *out_end)
{
#if SCP_RECOVERY_SUPPORT
	int i = 0;
	void *from;
	uint32_t *buf = out;
	int size_limit = sizeof(reg_save_list) / sizeof(struct reg_save_st);

	for (i = 0; i < size_limit; i++) {
		if (((void *)buf + reg_save_list[i].size
			+ sizeof(struct reg_save_st)) > (void *)out_end) {
			pr_notice("[SCP] %s overflow\n", __func__);
			break;
		}

		*buf = reg_save_list[i].addr;
		buf++;
		*buf = reg_save_list[i].size;
		buf++;

		from = scp_regdump_virt + (reg_save_list[i].addr & 0xfffff);

		if ((reg_save_list[i].addr & 0xfff00000) < L1TCM_BASE)
			from = scpreg.scpsys + (reg_save_list[i].addr & 0xfff);

		memcpy_from_scp(buf, from, reg_save_list[i].size);

		buf += (reg_save_list[i].size / sizeof(uint32_t));
	}
#endif
}

static void scp_do_l1cdump(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp;
	struct arm_smccc_res ares;

	tmp = readl(R_SEC_CTRL);
	/* enable cache debug */
	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_SEC_CTRL,
		tmp | B_CORE0_CACHE_DBG_EN | B_CORE1_CACHE_DBG_EN, 0, 0, 0, 0, 0,
		&ares);

	if ((void *)buf + MDUMP_L1C_SIZE > (void *)out_end) {
		pr_notice("[SCP] %s overflow\n", __func__);
		return;
	}
	memcpy_from_scp(buf, R_CORE0_CACHE_RAM, MDUMP_L1C_SIZE);
	/* disable cache debug */
	arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
		MTK_SIP_MEDMCU_SEC_CTRL,
		tmp, 0, 0, 0, 0, 0,
		&ares);
}

static void scp_do_tbufdump(uint32_t *out, uint32_t *out_end)
{
	uint32_t *buf = out;
	uint32_t tmp, index, offset, wbuf_ptr;
	int i;
	struct arm_smccc_res ares;

	wbuf_ptr = readl(R_CORE0_TBUF_WPTR);
	tmp = readl(R_CORE0_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_DBG_CTRL,
			0, tmp | (index << S_CORE_TBUF_DBG_SEL), 0, 0, 0, 0,
			&ares);
		*(buf) = readl(R_CORE0_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE0_TBUF_DATA63_32 + offset);
		buf += 2;
	}

	wbuf_ptr = readl(R_CORE1_TBUF_WPTR);
	tmp = readl(R_CORE1_DBG_CTRL) & (~M_CORE_TBUF_DBG_SEL);
	for (i = 0; i < 16; i++) {
		index = (wbuf_ptr + i) / 2;
		offset = ((wbuf_ptr + i) % 2) * 0x8;
		arm_smccc_smc(MTK_SIP_KERNEL_MEDMCU_CONTROL,
			MTK_SIP_MEDMCU_DBG_CTRL,
			1, tmp | (index << S_CORE_TBUF_DBG_SEL), 0, 0, 0, 0,
			&ares);
		*(buf) = readl(R_CORE1_TBUF_DATA31_0 + offset);
		*(buf + 1) = readl(R_CORE1_TBUF_DATA63_32 + offset);
		buf += 2;
	}

	for (i = 0; i < 16; i++) {
		pr_notice("[SCP] C0:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2), *(out + i * 2 + 1));
	}
	for (i = 0; i < 16; i++) {
		pr_notice("[SCP] C1:%02d:0x%08x::0x%08x\n",
			i, *(out + i * 2 + 16), *(out + i * 2 + 17));
	}
}

/*
 * scp_crash_dump: dump scp tcm info.
 * @param MemoryDump:   scp dump struct
 * @param scp_core_id:  core id
 * @return:             scp dump size
 */
static unsigned int scp_crash_dump(struct MemoryDump *pMemoryDump,
		enum scp_core_id id)
{
	unsigned int scp_dump_size;

	memcpy_from_scp((void *)&(pMemoryDump->l2tcm),
		(void *)(SCP_TCM),
		(SCP_A_TCM_SIZE));

	scp_do_l1cdump((void *)&(pMemoryDump->l1c),
		(void *)&(pMemoryDump->regdump));

	/* dump sys registers */
	scp_do_regdump((void *)&(pMemoryDump->regdump),
		(void *)&(pMemoryDump->tbuf));

	scp_do_tbufdump((void *)&(pMemoryDump->tbuf),
		(void *)&(pMemoryDump->dram));

	scp_dump_size = MDUMP_L2TCM_SIZE + MDUMP_L1C_SIZE
		+ MDUMP_REGDUMP_SIZE + MDUMP_TBUF_SIZE;

	dsb(SY); /* may take lot of time */

	return scp_dump_size;
}

/*
 * generate aee argument with scp register dump
 * @param aed_str:  exception description
 * @param id:       identify scp core id
 */
static void scp_prepare_aed_dump(char *aed_str,
		enum scp_core_id id)
{
	struct MemoryDump *md = (struct MemoryDump *) scp_dump.ramdump;
	char *scp_A_log = NULL;
	int ret;

	pr_notice("[SCP] %s begins:%s\n", __func__, aed_str);
	scp_dump_last_regs();

	scp_A_log = scp_pickup_log_for_aee();

	if (scp_dump.detail_buff == NULL) {
		pr_err("[SCP AEE]detail buf is null\n");
	} else {
		/* prepare scp aee detail information*/
		memset(scp_dump.detail_buff, 0, SCP_AED_STR_LEN);

		ret = snprintf(scp_dump.detail_buff, SCP_AED_STR_LEN,
				"%s\ncore0 pc=0x%08x, lr=0x%08x, sp=0x%08x\n"
				"core1 pc=0x%08x, lr=0x%08x, sp=0x%08x\n"
				"last log:\n%s",
				aed_str, c0_m.pc, c0_m.lr, c0_m.sp,
				c1_m.pc, c1_m.lr, c1_m.sp, scp_A_log);
		if (ret < 0 || ret >= SCP_AED_STR_LEN)
			pr_err("scp_dump detail buff write error\n");

		scp_dump.detail_buff[SCP_AED_STR_LEN - 1] = '\0';
	}

	/* prepare scp A db file */
	scp_dump.ramdump_length = 0;
	memset(md, 0x0, sizeof(*md));

	scp_dump.ramdump_length = scp_crash_dump(md, SCP_A_ID);

	pr_notice("[SCP] %s ends, @0x%llx, size = %x\n", __func__,
			  (uint64_t)md, scp_dump.ramdump_length);
}

/*
 * generate an exception according to exception type
 * NOTE: this function may be blocked and
 * should not be called in interrupt context
 * @param type: exception type
 */
void scp_aed(enum SCP_RESET_TYPE type, enum scp_core_id id)
{
	char *scp_aed_title = NULL;

	if (!scp_ee_enable) {
		pr_debug("[SCP]ee disable value=%d\n", scp_ee_enable);
		return;
	}

	mutex_lock(&scp_excep_mutex);

	/* get scp title and exception type*/
	switch (type) {
	case RESET_TYPE_WDT:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A wdt reset";
		else
			scp_aed_title = "SCP_B wdt reset";
		break;
	case RESET_TYPE_AWAKE:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A awake reset";
		else
			scp_aed_title = "SCP_B awake reset";
		break;
	case RESET_TYPE_CMD:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A cmd reset";
		else
			scp_aed_title = "SCP_B cmd reset";
		break;
	case RESET_TYPE_TIMEOUT:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A timeout reset";
		else
			scp_aed_title = "SCP_B timeout reset";
		break;
	default:
		if (id == SCP_A_ID)
			scp_aed_title = "SCP_A other reset";
		else
			scp_aed_title = "SCP_B other reset";
		break;
	}
	scp_get_log(id);

	/*print scp message*/
	pr_notice("scp_aed_title=%s\n", scp_aed_title);

	scp_prepare_aed_dump(scp_aed_title, id);

#ifdef CONFIG_MTK_AEE_FEATURE
	/* scp aed api, only detail information available*/
	aed_common_exception_api("scp", NULL, 0, NULL, 0,
			scp_dump.detail_buff, DB_OPT_DEFAULT);
	pr_notice("[SCP] scp exception dump is done\n");
#endif

	mutex_unlock(&scp_excep_mutex);
}



static ssize_t scp_A_dump_show(struct file *filep,
		struct kobject *kobj, const struct bin_attribute *attr,
		char *buf, loff_t offset, size_t size)
{
	unsigned int length = 0;

	mutex_lock(&scp_excep_mutex);

	if (offset >= 0 && offset < scp_dump.ramdump_length) {
		if ((offset + size) > scp_dump.ramdump_length)
			size = scp_dump.ramdump_length - offset;

		memcpy(buf, scp_dump.ramdump + offset, size);
		length = size;
	}

	mutex_unlock(&scp_excep_mutex);

	return length;
}


struct bin_attribute bin_attr_scp_dump = {
	.attr = {
		.name = "scp_dump",
		.mode = 0444,
	},
	.size = 0,
	.read = scp_A_dump_show,
};



/*
 * init a work struct
 */
int scp_excep_init(void)
{

	mutex_init(&scp_excep_mutex);

	/* alloc dump memory */
	scp_dump.detail_buff = vmalloc(SCP_AED_STR_LEN);
	if (!scp_dump.detail_buff)
		return -1;

	scp_dump.ramdump = vmalloc(sizeof(struct MemoryDump));
	if (!scp_dump.ramdump)
		return -1;

	/* init global values */
	scp_dump.ramdump_length = 0;
	/* 1: ee on, 0: ee disable */
	scp_ee_enable = 1;

	return 0;
}


/******************************************************************************
 * This function is called in the interrupt context. Note that scp_region_info
 * was initialized in scp_region_info_init() which must be called before this
 * function is called.
 *****************************************************************************/
void scp_ram_dump_init(void)
{
#if SCP_RECOVERY_SUPPORT
	scp_A_task_context_addr = scp_region_info->TaskContext_ptr;
	pr_debug("[SCP] get scp_A_task_context_addr: 0x%x\n",
		scp_A_task_context_addr);
#endif
}


/*
 * cleanup scp exception
 */
void scp_excep_cleanup(void)
{
	vfree(scp_dump.detail_buff);
	vfree(scp_dump.ramdump);

	scp_A_task_context_addr = 0;

	pr_debug("[SCP] %s ends\n", __func__);
}
