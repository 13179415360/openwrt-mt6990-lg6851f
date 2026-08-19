// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/atomic.h>

#include "ccci_config.h"
#include "ccci_common_config.h"
#include "ccci_platform.h"
#include "ccci_core.h"
#include "ccci_bm.h"
#include "modem_sys.h"
#include "ccci_hif.h"
#include "md_sys1_platform.h"

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include "mt-plat/mtk_ccci_common.h"

#if defined(ENABLE_32K_CLK_LESS)
//#include <mt-plat/mtk_rtc.h>
#include "ccci_rtc.h"
#endif

#define TAG "md"

phys_addr_t amms_cma_allocate(unsigned long size);
int amms_cma_free(phys_addr_t addr, unsigned long size);
int md_cd_vcore_config(unsigned int md_id, unsigned int hold_req);

struct ccci_modem *modem_sys[MAX_MD_NUM];
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
unsigned int aee_off_fast_reboot_enable;
#endif

// colgin @{
u32 hs_timeout = 0;
// @}

/* flag for MD1_MD3_SMEM clear.
 * if it is been cleared by md1 bootup flow, set it to 1.
 * then it will not be cleared by md1 bootup flow
 */
static atomic_t md1_md3_smem_clear = ATOMIC_INIT(0);

#define DBM_S (CCCI_SMEM_SIZE_DBM + CCCI_SMEM_SIZE_DBM_GUARD * 2)
#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)

// AP Splunk @{
static const char *s_smem_user_names[SMEM_USER_MAX];

#define MD_SMEM_FLAG_NORMAL    0
#define MD_SMEM_FLAG_PADDING   1
#define MD_SMEM_FLAG_OVERLAP   2

#define MD_SMEM_FLAG_SIZE_ZERO 1000
#define MD_SMEM_FLAG_LAST_REGION 1001

#define MD_SMEM_BUF_SIZE 1024
static char g_md_smem_buf[MD_SMEM_BUF_SIZE];
static unsigned int g_md_smem_pos;

#define STR_SIZE 100

int ccci_get_md_smem_buf(char **pbuf, unsigned int *size)
{
	if ((!pbuf) || (!size))
		return -1;

	*pbuf = g_md_smem_buf;
	*size = g_md_smem_pos;

	return 0;
}
// @}

#ifdef CCCI_USE_DFD_OFFSET_0
struct ccci_smem_region md1_6297_noncacheable_fat[] = {
	{SMEM_USER_RAW_DFD,	        0,	0,		 0, },
	{SMEM_USER_RAW_UDC_DATA,	0,	0,		 0, },
	{SMEM_USER_MD_WIFI_PROXY,	0,	0,		 0,},
	{SMEM_USER_RAW_AMMS_POS,	0,	0,
		SMF_NCLR_FIRST, },

	{SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,	 0, },
	{SMEM_USER_RAW_MDSS_DBG,	0,	14*1024, 0, },
	{SMEM_USER_RAW_RESERVED,	0,	42*1024, 0, },
	{SMEM_USER_RAW_RUNTIME_DATA,	0,	4*1024,	 0, },
	{SMEM_USER_RAW_FORCE_ASSERT,	0,	1*1024,	 0, },
	{SMEM_USER_LOW_POWER,		0,	512,	 0, },
	{SMEM_USER_RAW_DBM,		0,	512,	 0, },
	{SMEM_USER_CCISM_SCP,		0,	32*1024, 0, },
	{SMEM_USER_RAW_CCB_CTRL,	0,	4*1024,
		SMF_NCLR_FIRST, },
	{SMEM_USER_RAW_NETD,		0,	8*1024,	 0, },
	{SMEM_USER_RAW_USB,	        0,	4*1024,	 0, },
	{SMEM_USER_RAW_AUDIO,		0,	52*1024,
		SMF_NCLR_FIRST, },
	{SMEM_USER_CCISM_MCU,	0, (720+1)*1024,	SMF_NCLR_FIRST, },
	{SMEM_USER_CCISM_MCU_EXP, 0, (120+1)*1024,	SMF_NCLR_FIRST, },
	{SMEM_USER_MAX, }, /* tail guard */
};
#else
struct ccci_smem_region md1_6297_noncacheable_fat[] = {
{SMEM_USER_SAP_DFD_DBG,	0, 4*1024*1024,	SMF_NCLR_FIRST | SMF_CLR_RESET, },
{SMEM_USER_RAW_MDCCCI_DBG,	4*1024*1024,	2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	(4*1024+2)*1024, 24*1024,	0, },
{SMEM_USER_32K_LOW_POWER,	(4*1024+26)*1024, 32*1024,	0, },
//{SMEM_USER_RAW_RESERVED,	58*1024,		0*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA,	(4*1024+58)*1024, 4*1024,	0, },
{SMEM_USER_RAW_FORCE_ASSERT,	(4*1024+62)*1024, 1*1024,	0, },
{SMEM_USER_LOW_POWER,		(4*1024+63)*1024, 512,	0, },
{SMEM_USER_RAW_DBM,		((4*1024+63)*1024 + 512), DBM_S, 0, }, //TBD
{SMEM_USER_RAW_CCB_CTRL,	(4*1024+64)*1024, 4*1024, SMF_NCLR_FIRST, },
{SMEM_USER_RAW_AUDIO,		(4*1024+68)*1024, 52*1024, SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU,		(4*1024+120)*1024, (720+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU_EXP,	(4*1024+841)*1024, (120+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_SAP_EX_DBG, 	(4*1024+962)*1024, 10*1024,
	SMF_NCLR_FIRST | SMF_CLR_RESET, },

/* for SIB */
{SMEM_USER_RAW_PHY_CAP, (4*1024+972)*1024, 0*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, }, /* tail guard */
};

struct ccci_smem_region md1_6298_noncacheable_fat[] = {
{SMEM_USER_SAP_DFD_DBG,	0,	0,	SMF_NCLR_FIRST | SMF_CLR_RESET, },
{SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	0,	24*1024,	0, },
{SMEM_USER_32K_LOW_POWER,	0,	32*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA,0,	4*1024,		0, },
{SMEM_USER_RAW_FORCE_ASSERT,0,	1*1024,		0, },
{SMEM_USER_LOW_POWER,		0,	512,		0, },
{SMEM_USER_RAW_DBM,			0,	512,		0, }, //TBD
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
{SMEM_USER_RAW_CCB_CTRL,	0,	4*1024,			SMF_NCLR_FIRST, },
{SMEM_USER_RAW_AUDIO,		0,	52*1024,		SMF_NCLR_FIRST, },
#endif
{SMEM_USER_CCISM_MCU,		0,	(720+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU_EXP,	0,	(120+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_SAP_EX_DBG,		0,	32*1024,
	SMF_NCLR_FIRST | SMF_CLR_RESET, },  // should be 32K Low Power(new)

/* for SIB */
{SMEM_USER_RESERVED,	0,	30*1024,	0, },  // 960K - 930K = 30K
{SMEM_USER_RAW_PHY_CAP,		0,	0*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, }, /* tail guard */
};

struct ccci_smem_region md1_6298_noncacheable_AMMS_v3_fat[] = {
{SMEM_USER_SAP_DFD_DBG,		0,	0,	SMF_NCLR_FIRST | SMF_CLR_RESET, },
{SMEM_USER_RAW_MDCCCI_DBG,	0,	2*1024,		0, },
{SMEM_USER_RAW_MDSS_DBG,	0,	24*1024,	0, },
{SMEM_USER_32K_LOW_POWER,	0,	32*1024,	0, },
{SMEM_USER_RAW_RUNTIME_DATA,0,	4*1024,		0, },
{SMEM_USER_RAW_FORCE_ASSERT,0,	1*1024,		0, },
{SMEM_USER_LOW_POWER,		0,	512,		0, },
{SMEM_USER_RAW_DBM,			0,	512,		0, }, //TBD
{SMEM_USER_RAW_CCB_CTRL,	0,	4*1024,			SMF_NCLR_FIRST, },
{SMEM_USER_RAW_AUDIO,		0,	52*1024,		SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU,		0,	(720+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_CCISM_MCU_EXP,	0,	(120+1)*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_SAP_EX_DBG,		0,	32*1024,
	SMF_NCLR_FIRST | SMF_CLR_RESET, },  // should be 32K Low Power(new)

/* for SIB */
{SMEM_USER_RAW_PHY_CAP,	0,	0*1024*1024,	SMF_NCLR_FIRST, },
{SMEM_USER_RESERVED,	0,	30*1024,	0, },  // 960K - 930K = 30K
{SMEM_USER_MD_DRDI,		0,	64*1024,	SMF_NCLR_FIRST, },  // AMMS_v3
{SMEM_USER_MAX, }, /* tail guard */
};

#endif
struct ccci_smem_region md1_6297_cacheable[] = {
{SMEM_USER_MD_CDMR,		0,	32*1024*1024,	0, },
{SMEM_USER_MD_NVRAM_CACHE,	0,	18*1024*1024,	0, },
/*
 * all CCB user should be put together, and the total size is set in the first one, all reset
 * CCB users' address, offset and size will be re-calculated during port initialization.
 * and please be aware of that CCB user's size will be aligned to 4KB.
 */
{SMEM_USER_CCB_DHL,		0,	2*1024*1024,	0, },
{SMEM_USER_CCB_MD_MONITOR,	0,	2*1024*1024,	0, },
{SMEM_USER_CCB_META,		0,	2*1024*1024,	0, },
{SMEM_USER_RAW_DHL,		0,	62*1024*1024,	0, },
{SMEM_USER_RAW_MDM,		0,	62*1024*1024,	0, },
{SMEM_USER_RAW_MD_CONSYS,		0, 256*1024, SMF_NCLR_FIRST, },
{SMEM_USER_RAW_USIP,		0, 128*1024, SMF_NCLR_FIRST, },
{SMEM_USER_USB_DATA,		0, 28*1024*1024, SMF_NCLR_FIRST, },
{SMEM_USER_MAX, },
};

struct ccci_smem_region md1_6298_cacheable[] = {
{SMEM_USER_MD_CDMR,		0,	0,	0, },
{SMEM_USER_MD_NVRAM_CACHE,	0,	5*1024*1024,	0, },
/*
 * all CCB user should be put together, and the total size is set in the first one, all reset
 * CCB users' address, offset and size will be re-calculated during port initialization.
 * and please be aware of that CCB user's size will be aligned to 4KB.
 */
{SMEM_USER_CCB_DHL,		0,	2*1024*1024,	0, },
{SMEM_USER_CCB_MD_MONITOR,	0,	2*1024*1024,	0, },
{SMEM_USER_CCB_META,		0,	2*1024*1024,	0, },
{SMEM_USER_RAW_DHL,		0,	62*1024*1024,	0, },
{SMEM_USER_RAW_MDM,		0,	62*1024*1024,	0, },
{SMEM_USER_RAW_MD_CONSYS,		0, 256*1024, SMF_NCLR_FIRST, },
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
{SMEM_USER_RAW_USIP,		0, 384*1024, SMF_NCLR_FIRST, },
#endif
{SMEM_USER_MAX, },
};

#define CCB_CACHE_MIN_SIZE    (2 * 1024 * 1024)

// AP Splunk @{
static void init_smem_user_name(void)
{
	s_smem_user_names[SMEM_USER_RAW_DBM] = "RAW_DBM";
	s_smem_user_names[SMEM_USER_CCB_DHL] = "CCB_DHL";
	s_smem_user_names[SMEM_USER_CCB_MD_MONITOR] = "CCB_MD_MONITOR";
	s_smem_user_names[SMEM_USER_CCB_META] = "CCB_META";
	s_smem_user_names[SMEM_USER_RAW_CCB_CTRL] = "RAW_CCB_CTRL";
	s_smem_user_names[SMEM_USER_RAW_DHL] = "RAW_DHL";
	s_smem_user_names[SMEM_USER_RAW_MDM] = "RAW_MDM";
	s_smem_user_names[SMEM_USER_RAW_NETD] = "RAW_NETD";
	s_smem_user_names[SMEM_USER_RAW_USB] = "RAW_USB";
	s_smem_user_names[SMEM_USER_RAW_AUDIO] = "RAW_AUDIO";
	s_smem_user_names[SMEM_USER_RAW_DFD] = "RAW_DFD";
	s_smem_user_names[SMEM_USER_RAW_LWA] = "RAW_LWA";
	s_smem_user_names[SMEM_USER_RAW_MDCCCI_DBG] = "RAW_MDCCCI_DBG";
	s_smem_user_names[SMEM_USER_RAW_MDSS_DBG] = "RAW_MDSS_DBG";
	s_smem_user_names[SMEM_USER_RAW_RUNTIME_DATA] = "RAW_RUNTIME_DATA";
	s_smem_user_names[SMEM_USER_RAW_FORCE_ASSERT] = "RAW_FORCE_ASSERT";
	s_smem_user_names[SMEM_USER_CCISM_SCP] = "CCISM_SCP";
	s_smem_user_names[SMEM_USER_RAW_MD2MD] = "RAW_MD2MD";
	s_smem_user_names[SMEM_USER_RAW_RESERVED] = "RAW_RESERVED";
	s_smem_user_names[SMEM_USER_CCISM_MCU] = "CCISM_MCU";
	s_smem_user_names[SMEM_USER_CCISM_MCU_EXP] = "CCISM_MCU_EXP";
	s_smem_user_names[SMEM_USER_SMART_LOGGING] = "SMART_LOGGING";
	s_smem_user_names[SMEM_USER_RAW_MD_CONSYS] = "RAW_MD_CONSYS";
	s_smem_user_names[SMEM_USER_RAW_PHY_CAP] = "RAW_PHY_CAP";
	s_smem_user_names[SMEM_USER_RAW_USIP] = "RAW_USIP";
	s_smem_user_names[SMEM_USER_RESV_0] = "RESV_0";
	s_smem_user_names[SMEM_USER_ALIGN_PADDING] = "ALIGN_PADDING";
	s_smem_user_names[SMEM_USER_RAW_UDC_DATA] = "RAW_UDC_DATA";
	s_smem_user_names[SMEM_USER_RAW_UDC_DESCTAB] = "RAW_UDC_DESCTAB";
	s_smem_user_names[SMEM_USER_RAW_AMMS_POS] = "RAW_AMMS_POS";
	s_smem_user_names[SMEM_USER_RAW_ALIGN_PADDING] = "RAW_ALIGN_PADDING";
	s_smem_user_names[SMEM_USER_MD_WIFI_PROXY] = "MD_WIFI_PROXY";
	s_smem_user_names[SMEM_USER_MD_NVRAM_CACHE] = "MD_NVRAM_CACHE";
	s_smem_user_names[SMEM_USER_LOW_POWER] = "LOW_POWER";
	s_smem_user_names[SMEM_USER_USB_DATA] = "USB_DATA";
	s_smem_user_names[SMEM_USER_MD_CDMR] = "MD_CDMR";
	s_smem_user_names[SMEM_USER_SAP_EX_DBG] = "SAP_EX_DBG";
	s_smem_user_names[SMEM_USER_SAP_DFD_DBG] = "SAP_DFD_DBG";
	s_smem_user_names[SMEM_USER_32K_LOW_POWER] = "32K_LOW_POWER";
	s_smem_user_names[SMEM_USER_RESERVED] = "RESERVED";
	s_smem_user_names[SMEM_USER_MD_DRDI] = "MD_DRDI";
}

static const char *get_smem_user_name(int user_id)
{
	if (user_id < 0 || user_id >= SMEM_USER_MAX)
		return "";

	return s_smem_user_names[user_id];
}
// @}

static struct ccci_smem_region *get_smem_by_user_id(
	struct ccci_smem_region *regions, enum SMEM_USER_ID user_id)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			return NULL;

		if (regions[i].id == user_id) {
			if (!get_modem_is_enabled(MD_SYS3) &&
				(regions[i].flag & SMF_MD3_RELATED))
				return NULL;
			else
				return regions + i;
		}
	}
	return NULL;
}


// AP Splunk @{
static void append_string_to_md_smem_buf(const char *str)
{
	int n;

	if (g_md_smem_pos >= (MD_SMEM_BUF_SIZE - 1))
		return;

	n = snprintf(g_md_smem_buf + g_md_smem_pos,
				 MD_SMEM_BUF_SIZE - g_md_smem_pos,
				 "%s", str);

	if (n <= 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] warning: snprintf() fail: %d\n",
			__func__, n);
		return;
	}

	if (n >= (MD_SMEM_BUF_SIZE - g_md_smem_pos)) {
		CCCI_ERROR_LOG(-1, TAG,
		"[%s] warning: g_md_smem_buf is too small: %u,%d\n",
		__func__, g_md_smem_pos, n);

		g_md_smem_pos = MD_SMEM_BUF_SIZE - 1;

	} else
		g_md_smem_pos += n;

	g_md_smem_buf[g_md_smem_pos] = '\0';
}

static void calc_smem_overlap_and_padding(
		struct ccci_smem_region *regions,
		int flag, int index, int *overlap_index)
{
	int i = 0, n = 0;
	char str[STR_SIZE] = {0};

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] flag: %d; index: %d; overlap_index: %d\n",
		__func__, flag, index, (*overlap_index));

	if ((flag != MD_SMEM_FLAG_SIZE_ZERO) &&
			((*overlap_index) != -1)) {  //overlap
		int s = 0, c = 0;
		unsigned int overlap_off = 0, overlap_size = 0;
		char lap[STR_SIZE] = {0};

		i = (*overlap_index);

		while (i < index) {
			if (regions[i].size == 0) {
				i++;
				continue;
			}

			if ((regions[i].offset < overlap_off) ||
					(overlap_off == 0))
				overlap_off = regions[i].offset;

			if ((regions[i].offset + regions[i].size)
					- overlap_off > overlap_size)
				overlap_size =
					(regions[i].offset + regions[i].size)
					- overlap_off;

			if (i == (*overlap_index))
				n = snprintf(lap + s, STR_SIZE - s,
						"%d", regions[i].id);
			else
				n = snprintf(lap + s, STR_SIZE - s,
						"|%d", regions[i].id);

			if (n >= (STR_SIZE - s))
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] warning: buf size too small: %d,%d\n",
					__func__, s, n);

			else if (n < 0) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] warning: snprintf() fail: %d,%d\n",
					__func__, s, n);
				break;
			}

			s += n;
			c++;
			i++;

			if (s >= STR_SIZE)
				break;
		}

		if (c > 1) {
			n = snprintf(str, STR_SIZE, "%d-%s-%X|%X\n",
					MD_SMEM_FLAG_OVERLAP,
					lap, overlap_off, overlap_size);

			if (n >= STR_SIZE)
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] warning: str buf size too small, %d\n",
					__func__, n);

			if (n > 0)
				append_string_to_md_smem_buf(str);
		}

		*overlap_index = -1;
	}

	if (flag == MD_SMEM_FLAG_SIZE_ZERO)
		flag = MD_SMEM_FLAG_NORMAL;

	if (flag == MD_SMEM_FLAG_NORMAL ||
			flag == MD_SMEM_FLAG_PADDING) {  //normal and padding

		if (flag == MD_SMEM_FLAG_PADDING) {
			int pad_off = regions[index-1].offset
						+ regions[index-1].size;

			n = snprintf(str, STR_SIZE, "%d-%d-%X|%X\n", flag,
					regions[index].id,
					pad_off,
					regions[index].offset - pad_off);

			if (n >= STR_SIZE)
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] warning: str buf size too small, %d\n",
					__func__, n);

			if (n > 0)
				append_string_to_md_smem_buf(str);

			flag = MD_SMEM_FLAG_NORMAL;
		}

		n = snprintf(str, STR_SIZE, "%d-%d-%X|%X\n", flag,
				regions[index].id,
				regions[index].offset, regions[index].size);

		if (n >= STR_SIZE)
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] warning: str buf size too small, %d\n",
				__func__, n);

		if (n > 0)
			append_string_to_md_smem_buf(str);
	}
}
// @}

static void init_smem_regions(struct ccci_smem_region *regions,
	phys_addr_t base_ap_view_phy,
	void __iomem *base_ap_view_vir,
	phys_addr_t base_md_view_phy)
{
	int i;
	// AP Splunk @{
	int calc_offset = 0;
	int overlap_index = -1;
	// @}

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		if (!get_modem_is_enabled(MD_SYS3) &&
			(regions[i].flag & SMF_MD3_RELATED))
			continue;

		regions[i].base_ap_view_phy =
			base_ap_view_phy + regions[i].offset;
		regions[i].base_ap_view_vir =
			base_ap_view_vir + regions[i].offset;
		regions[i].base_md_view_phy =
			base_md_view_phy + regions[i].offset;

		// AP Splunk @{
		if ((i > 0) && (regions[i].size != 0) &&
				(calc_offset != regions[i].offset)) {

			if (regions[i].offset > calc_offset) { // padding
				CCCI_BOOTUP_LOG(-1, TAG,
					"[%s] <%d>(%s) padding size: %x\n",
					__func__, regions[i].id,
					get_smem_user_name(regions[i].id),
					regions[i].offset - calc_offset);

				calc_smem_overlap_and_padding(regions,
					MD_SMEM_FLAG_PADDING, i,
					&overlap_index);

				calc_offset = regions[i].offset + regions[i].size;

			} else {  //overlap
				CCCI_BOOTUP_LOG(-1, TAG,
					"[%s] (%s) and (%s) is overlap.\n",
					__func__,
					get_smem_user_name(regions[i-1].id),
					get_smem_user_name(regions[i].id));

				if (overlap_index == -1)
					overlap_index = i-1;

				if ((regions[i].offset + regions[i].size) >
						calc_offset)  //range is larger than before
					calc_offset = regions[i].offset +
							regions[i].size;
			}
		} else {
			if (regions[i].size != 0) {  //normal region
				calc_offset = regions[i].offset + regions[i].size;

				calc_smem_overlap_and_padding(regions,
					MD_SMEM_FLAG_NORMAL, i,
					&overlap_index);

			} else  // region size is 0
				calc_smem_overlap_and_padding(regions,
					MD_SMEM_FLAG_SIZE_ZERO, i,
					&overlap_index);
		}
		// @}

		CCCI_BOOTUP_LOG(-1, TAG,
			"%s: reg[%d](%s)<%d>(%lx %lx %lx)[%x]\n", __func__,
			i, get_smem_user_name(regions[i].id), regions[i].id,
			(unsigned long)regions[i].base_ap_view_phy,
			(unsigned long)regions[i].base_ap_view_vir,
			(unsigned long)regions[i].base_md_view_phy,
			regions[i].size);

#if defined(CONFIG_MTK_AEE_FEATURE)
		if (regions[i].id == SMEM_USER_RAW_MDSS_DBG)
			mrdump_mini_add_extra_file((unsigned long)regions[i].base_ap_view_vir,
					(unsigned long)regions[i].base_ap_view_phy, regions[i].size,
					"EXTRA_MDSS");
#endif
	}
	// AP Splunk @{
	calc_smem_overlap_and_padding(regions,
			MD_SMEM_FLAG_LAST_REGION, i,
			&overlap_index);
	// @}
}

static void clear_smem_region(struct ccci_smem_region *regions, int first_boot)
{
	int i;

	for (i = 0; ; i++) {
		if (!regions || regions[i].id == SMEM_USER_MAX)
			break;

		if (!get_modem_is_enabled(MD_SYS3) &&
			(regions[i].flag & SMF_MD3_RELATED))
			continue;
		if (first_boot) {
			if (!(regions[i].flag & SMF_NCLR_FIRST)) {
				if (regions[i].id == SMEM_USER_RAW_MD2MD) {
					if (atomic_add_unless(
						&md1_md3_smem_clear, 1, 1))
						memset_io(
						regions[i].base_ap_view_vir,
							0, regions[i].size);
				} else if (regions[i].size) {
					memset_io(regions[i].base_ap_view_vir,
						0, regions[i].size);
				}
			}
		} else {
			if (regions[i].flag & SMF_CLR_RESET && regions[i].size)
				memset_io(regions[i].base_ap_view_vir,
					0, regions[i].size);
		}
	}
}

/* setup function is only for data structure initialization */
struct ccci_modem *ccci_md_alloc(int private_size)
{
	struct ccci_modem *md = kzalloc(sizeof(struct ccci_modem), GFP_KERNEL);

	if (!md) {
		CCCI_ERROR_LOG(-1, TAG,
			"fail to allocate memory for modem structure\n");
		goto out;
	}
	if (private_size > 0)
		md->private_data = kzalloc(private_size, GFP_KERNEL);
	else
		md->private_data = NULL;
	md->per_md_data.config.setting |= MD_SETTING_FIRST_BOOT;
	md->per_md_data.is_in_ee_dump = 0;
	md->is_force_asserted = 0;
	md->per_md_data.md_dbg_dump_flag = MD_DBG_DUMP_AP_REG;

 out:
	return md;
}

static inline int log2_remain(unsigned int value)
{
	int x = 0;
	int y;

	if (value < 32)
		return -1;

	/* value = (2^x)*y */
	while (!(value & (1 << x)))
		x++;
	y = (value >> x);
	if ((1 << x) * y != value)
		WARN_ON(1);

	return y;
}

phys_addr_t __attribute__((weak)) amms_cma_allocate(unsigned long size)
{
	return 0;
}

int __attribute__((weak)) amms_cma_free(phys_addr_t addr, unsigned long size)
{
	return 0;
}

static inline int update_smem_region(struct ccci_smem_region *region)
{
	unsigned int offset, size;
	int ret = 0;

	if (get_nc_smem_region_info(region->id, &offset, NULL, &size)) {
		region->offset = offset;
		region->size = size;
		ret = 1;
		CCCI_BOOTUP_LOG(MD_SYS1, TAG, "Update <%d>:0x%x 0x%x\n",
			region->id, region->offset, region->size);
	}
	return ret;
}

static void ccci_6297_md_smem_layout_config(struct ccci_modem *md)
{
	struct ccci_mem_layout *mm_str = &md->mem_layout;
	unsigned int md_resv_mem_offset = 0, ccb_offset = 0;
	unsigned int md_resv_mem_size = 0, ccb_size = 0;
	unsigned int i;
	phys_addr_t md_resv_smem_addr = 0, smem_amms_pos_addr = 0;
	int size;

	// AMMS_v3 @{
	int noncachable_tbl_size = 0;
	int cachable_tbl_size = 0;
	struct ccci_smem_region *md1_noncachable_tbl = NULL;
	struct ccci_smem_region *md1_cachable_tbl = NULL;

	if (md->hw_info->plat_val->md_gen >= 6298) {
		if (get_md_drdi_ver_from_lk() != AMMS_V3) {
			noncachable_tbl_size =
				sizeof(md1_6298_noncacheable_fat)/sizeof(struct ccci_smem_region);
			md1_noncachable_tbl = md1_6298_noncacheable_fat;
		} else {
			noncachable_tbl_size =
				sizeof(md1_6298_noncacheable_AMMS_v3_fat)/sizeof(struct ccci_smem_region);
			md1_noncachable_tbl = md1_6298_noncacheable_AMMS_v3_fat;
		}
		cachable_tbl_size =
			sizeof(md1_6298_cacheable)/sizeof(struct ccci_smem_region);
		md1_cachable_tbl = md1_6298_cacheable;
	} else {
		noncachable_tbl_size =
			sizeof(md1_6297_noncacheable_fat)/sizeof(struct ccci_smem_region);
		cachable_tbl_size =
			sizeof(md1_6297_cacheable)/sizeof(struct ccci_smem_region);
		md1_noncachable_tbl = md1_6297_noncacheable_fat;
		md1_cachable_tbl = md1_6297_cacheable;
	}
	// @}

	/* non-cacheable start */
	get_md_resv_mem_info(md->index, NULL, NULL, &md_resv_smem_addr, NULL);

#ifdef CCCI_USE_DFD_OFFSET_0
	i = 0;
#else
	i = 1;
#endif
	for (; i < noncachable_tbl_size; i++) {

		update_smem_region(&md1_noncachable_tbl[i]);
#ifdef CCCI_USE_DFD_OFFSET_0
		if (i == 0)
			continue;
#endif
		if (md1_noncachable_tbl[i].offset == 0)
			/* update offset */
			md1_noncachable_tbl[i].offset =
				md1_noncachable_tbl[i-1].offset
				+ md1_noncachable_tbl[i-1].size;

		/* Special case */
		switch (md1_noncachable_tbl[i].id) {
		case SMEM_USER_RAW_AMMS_POS:
			size = get_smem_amms_pos_size(MD_SYS1);
			if (size >= 0) {
				/* free AMMS POS smem*/
				smem_amms_pos_addr = md_resv_smem_addr
					+ md1_noncachable_tbl[i].offset;
				amms_cma_free(smem_amms_pos_addr, size);
			}
			CCCI_BOOTUP_LOG(md->index, TAG,
			"smem amms pos size:%d\n",
			md1_noncachable_tbl[i].size);
			break;
		default:
			break;
		}
	}

	mm_str->md_bank4_noncacheable = md1_noncachable_tbl;
	get_md_resv_csmem_info(md->index,
		&mm_str->md_bank4_cacheable_total.base_ap_view_phy,
		&mm_str->md_bank4_cacheable_total.size);
	/* cacheable start */
	if (mm_str->md_bank4_cacheable_total.base_ap_view_phy &&
		mm_str->md_bank4_cacheable_total.size)
		mm_str->md_bank4_cacheable_total.base_ap_view_vir =
			ccci_map_phy_addr(
			mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			mm_str->md_bank4_cacheable_total.size);
	else
		CCCI_ERROR_LOG(md->index, TAG,
			"get cacheable info base:%lx size:%x\n",
			(unsigned long)
			mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			mm_str->md_bank4_cacheable_total.size);

	mm_str->md_bank4_cacheable_total.base_md_view_phy = 0x40000000
		+ get_md_smem_cachable_offset(MD_SYS1)
		+ mm_str->md_bank4_cacheable_total.base_ap_view_phy -
		round_down(mm_str->md_bank4_cacheable_total.base_ap_view_phy,
			0x00100000);

	/* specially, CCB size. */
	/* get_md_resv_ccb_info(md->index, &ccb_offset, &ccb_size); */
	get_md_cache_region_info(SMEM_USER_CCB_START,
				&ccb_offset,
				&ccb_size);
	CCCI_BOOTUP_LOG(md->index, TAG,
			"ccb totoal :offset = 0x%x, size = 0x%x\n",
			ccb_offset, ccb_size);

	for (i = 0; i < cachable_tbl_size; i++) {
		switch (md1_cachable_tbl[i].id) {
		case SMEM_USER_CCB_DHL:
		case SMEM_USER_CCB_MD_MONITOR:
		case SMEM_USER_CCB_META:
			md1_cachable_tbl[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				CCB_CACHE_MIN_SIZE:0;
			md1_cachable_tbl[i].offset =  ccb_offset;
			break;
		case SMEM_USER_RAW_DHL:
		case SMEM_USER_RAW_MDM:
			md1_cachable_tbl[i].size =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_size - CCB_CACHE_MIN_SIZE):0;
			md1_cachable_tbl[i].offset =
				(ccb_size > CCB_CACHE_MIN_SIZE) ?
				(ccb_offset + CCB_CACHE_MIN_SIZE):ccb_offset;
			CCCI_BOOTUP_LOG(md->index, TAG,
				"[%d]RAW size:%d\n", md1_cachable_tbl[i].id,
				md1_cachable_tbl[i].size);
			break;
		// colgin @ {
		case SMEM_USER_MD_CDMR:
		case SMEM_USER_USB_DATA:
		// @}
		case SMEM_USER_RAW_MD_CONSYS: /* go through */
		case SMEM_USER_MD_NVRAM_CACHE: /* go through */
		case SMEM_USER_RAW_UDC_DESCTAB: /* go through */
		case SMEM_USER_RAW_USIP:
			get_md_cache_region_info(md1_cachable_tbl[i].id,
				&md_resv_mem_offset,
				&md_resv_mem_size);

			md1_cachable_tbl[i].size = md_resv_mem_size;
			if (md_resv_mem_offset || md_resv_mem_size)
				md1_cachable_tbl[i].offset =
					md_resv_mem_offset; /* LK config */
			else if (i == 0)
				md1_cachable_tbl[i].offset = 0;
			else
				md1_cachable_tbl[i].offset =
					md1_cachable_tbl[i - 1].offset +
					md1_cachable_tbl[i - 1].size;
			break;
		default:
			md1_cachable_tbl[i].size = 0;
			md1_cachable_tbl[i].offset = 0;
			break;
		}
	}

	mm_str->md_bank4_cacheable = md1_cachable_tbl;
}

static void ccci_md_config(struct ccci_modem *md)
{
	phys_addr_t md_resv_mem_addr = 0,
		md_resv_smem_addr = 0, md1_md3_smem_phy = 0;

	unsigned int md_resv_mem_size = 0,
		md_resv_smem_size = 0, md1_md3_smem_size = 0;

	/* setup config */
	md->per_md_data.config.load_type = get_md_img_type(md->index);
	if (get_modem_is_enabled(md->index))
		md->per_md_data.config.setting |= MD_SETTING_ENABLE;
	else
		md->per_md_data.config.setting &= ~MD_SETTING_ENABLE;

	/* Get memory info */
	get_md_resv_mem_info(md->index, &md_resv_mem_addr,
		&md_resv_mem_size, &md_resv_smem_addr, &md_resv_smem_size);
	get_md1_md3_resv_smem_info(md->index, &md1_md3_smem_phy,
		&md1_md3_smem_size);
	/* setup memory layout */
	/* MD image */
	md->mem_layout.md_bank0.base_ap_view_phy = md_resv_mem_addr;
	md->mem_layout.md_bank0.size = md_resv_mem_size;
	/* do not remap whole region, consume too much vmalloc space */

	// AMMS_v3, DRDI MPU region is FORBIDDEN if drdi ver is AMMS_v3 @{
	if (get_md_drdi_ver_from_lk() != AMMS_V3) {
		md->mem_layout.md_bank0.base_ap_view_vir =
			ccci_map_phy_addr(
				md->mem_layout.md_bank0.base_ap_view_phy,
				MD_IMG_DUMP_SIZE);
	}
	// @}

	/* Share memory */
	/*
	 * MD bank4 is remap to nearest 32M aligned address
	 * assume share memoy layout is:
	 * |---AP/MD1--| <--MD1 bank4 0x0 (non-cacheable)
	 * |--MD1/MD3--| <--MD3 bank4 0x0 (non-cacheable)
	 * |---AP/MD3--|
	 * |--non-used_-|
	 * |--cacheable--| <-- MD1 bank4 0x8000000 (for 6292)
	 * this should align with LK's remap setting
	 */
	/* non-cacheable region */
	if (md->index == MD_SYS1)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy
			= md_resv_smem_addr;
	else if (md->index == MD_SYS3)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy
			= md1_md3_smem_phy;
	md->mem_layout.md_bank4_noncacheable_total.size
			= md_resv_smem_size + md1_md3_smem_size;
	md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir  =
		ccci_map_phy_addr(
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		md->mem_layout.md_bank4_noncacheable_total.size);
	md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy
		= MD_VIEW_BANK4;//0x40000000

	ccci_6297_md_smem_layout_config(md);

	CCCI_BOOTUP_LOG(md->index, TAG,
		"smem info: (%lx %lx %lx %d) (%lx %lx %lx %d)\n",
		(unsigned long)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		(unsigned long)
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy,
		(unsigned long)
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_noncacheable_total.size,
		(unsigned long)
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
		(unsigned long)
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy,
		(unsigned long)
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_cacheable_total.size);

	init_smem_regions(md->mem_layout.md_bank4_noncacheable,
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_phy,
		md->mem_layout.md_bank4_noncacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_noncacheable_total.base_md_view_phy);
	init_smem_regions(md->mem_layout.md_bank4_cacheable,
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_phy,
		md->mem_layout.md_bank4_cacheable_total.base_ap_view_vir,
		md->mem_layout.md_bank4_cacheable_total.base_md_view_phy);

	/* updae image info */
	md->per_md_data.img_info[IMG_MD].type = IMG_MD;
	md->per_md_data.img_info[IMG_MD].address =
		md->mem_layout.md_bank0.base_ap_view_phy;
	md->per_md_data.img_info[IMG_DSP].type = IMG_DSP;
	md->per_md_data.img_info[IMG_ARMV7].type = IMG_ARMV7;
}

static int boot_md_show(int md_id, char *buf, int size)
{
	int curr = 0;

	if (get_modem_is_enabled(md_id))
		curr += snprintf(&buf[curr], size, "md%d:%d",
			md_id + 1, ccci_fsm_get_md_state(md_id));
	return curr;
}

static int boot_md_store(int md_id)
{
	return -EACCES;
}

static void ccci_md_obj_release(struct kobject *kobj)
{
	CCCI_ERROR_LOG(-1, CORE, "md kobject release\n");
}

static ssize_t ccci_md_attr_show(struct kobject *kobj, struct attribute *attr,
	char *buf)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->show)
		len = a->show(a->modem, buf);

	return len;
}

static ssize_t ccci_md_attr_store(struct kobject *kobj, struct attribute *attr,
	const char *buf, size_t count)
{
	ssize_t len = 0;
	struct ccci_md_attribute *a = container_of(attr,
		struct ccci_md_attribute, attr);

	if (a->store)
		len = a->store(a->modem, buf, count);

	return len;
}

static const struct sysfs_ops ccci_md_sysfs_ops = {
	.show = ccci_md_attr_show,
	.store = ccci_md_attr_store
};

static struct attribute *ccci_md_default_attrs[] = {
	NULL
};


/* LG6851F Stage5D: Linux 6.18 kobj_type default_groups. */
static const struct attribute_group ccci_md_default_group = {
	.attrs = ccci_md_default_attrs,
};

static const struct attribute_group *ccci_md_default_groups[] = {
	&ccci_md_default_group,
	NULL,
};
static struct kobj_type ccci_md_ktype = {
	.release = ccci_md_obj_release,
	.sysfs_ops = &ccci_md_sysfs_ops,
	.default_groups = ccci_md_default_groups,};

static void ccci_sysfs_add_md(int md_id, void *kobj)
{
	ccci_sysfs_add_modem(md_id, (void *)kobj, (void *)&ccci_md_ktype,
		boot_md_show, boot_md_store);
}

int ccci_md_register(struct ccci_modem *md)
{
	int ret;

	/* init per-modem sub-system */
	CCCI_INIT_LOG(md->index, TAG, "register modem\n");

	init_smem_user_name();

	/* init modem */
	ret = md->ops->init(md);
	if (ret < 0)
		return ret;
	ccci_md_config(md);

	modem_sys[md->index] = md;
	ccci_sysfs_add_md(md->index, (void *)&md->kobj);
	ccci_platform_common_init(md);
	ccci_fsm_init(md->index);
	ccci_port_init(md->index);
	return 0;
}

int ccci_md_set_boot_data(unsigned char md_id, unsigned int data[], int len)
{
	int ret = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (len < 0 || data == NULL)
		return -1;

	md->mdlg_mode = data[MD_CFG_MDLOG_MODE];
	md->sbp_code  = data[MD_CFG_SBP_CODE];
	md->per_md_data.md_dbg_dump_flag =
		data[MD_CFG_DUMP_FLAG] == MD_DBG_DUMP_INVALID ?
		md->per_md_data.md_dbg_dump_flag : data[MD_CFG_DUMP_FLAG];
	md->boot_mode = data[MD_CFG_BOOT_MODE];

	return ret;
}

struct ccci_mem_layout *ccci_md_get_mem(int md_id)
{
	if (md_id >= MAX_MD_NUM || md_id < 0)
		return NULL;
	return &modem_sys[md_id]->mem_layout;
}

struct ccci_smem_region *ccci_md_get_smem_by_user_id(int md_id,
	enum SMEM_USER_ID user_id)
{
	struct ccci_smem_region *curr = NULL;

	if (md_id >= MAX_MD_NUM || md_id < 0)
		return NULL;

	if (modem_sys[md_id] == NULL) {
		CCCI_ERROR_LOG(md_id, TAG,
			"md%d not enable/ before driver int, return NULL\n",
			md_id);
		return NULL;
	}

	curr = get_smem_by_user_id(
		modem_sys[md_id]->mem_layout.md_bank4_noncacheable, user_id);
	if (curr)
		return curr;
	curr = get_smem_by_user_id(
		modem_sys[md_id]->mem_layout.md_bank4_cacheable, user_id);
	return curr;
}
EXPORT_SYMBOL(ccci_md_get_smem_by_user_id);

void ccci_md_clear_smem(int md_id, int first_boot)
{
	struct ccci_smem_region *region = NULL;
	unsigned int size;

	if (md_id >= MAX_MD_NUM || md_id < 0)
		return;
	/* MD will clear share memory itself after the first boot */
	clear_smem_region(modem_sys[md_id]->mem_layout.md_bank4_noncacheable,
		first_boot);
	clear_smem_region(modem_sys[md_id]->mem_layout.md_bank4_cacheable,
		first_boot);
	if (!first_boot) {
		CCCI_NORMAL_LOG(-1, TAG, "clear buffer ! first_boot\n");
		region = ccci_md_get_smem_by_user_id(md_id, SMEM_USER_CCB_DHL);
		if (region && region->size) {
			/*clear ccb data smem*/
			memset_io(region->base_ap_view_vir, 0, region->size);
		}
		region = ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_DHL);
		if (region && region->size) {
			/* clear first 1k bytes in dsp log buffer */
			size = (region->size > (128 * sizeof(long long))) ?
			(128 * sizeof(long long))
			: region->size;
			memset_io(region->base_ap_view_vir, 0, size);
			CCCI_NORMAL_LOG(-1, TAG,
			"clear buffer user_id = SMEM_USER_RAW_DHL, szie = 0x%x\n",
			size);
		}
	}
}

int ccci_md_pre_stop(unsigned char md_id, unsigned int stop_type)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->pre_stop(md, stop_type);
}

int ccci_md_stop(unsigned char md_id, unsigned int stop_type)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->stop(md, stop_type);
}

int __weak md_cd_vcore_config(unsigned int md_id, unsigned int hold_req)
{
	pr_debug("[ccci/dummy] %s is not supported!\n", __func__);
	return 0;
}

int ccci_md_pre_start(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->hw_info->plat_ptr->vcore_config)
		return md->hw_info->plat_ptr->vcore_config(md_id, 1);
	return -1;
}
int ccci_md_start(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->start(md);
}
int ccci_md_post_start(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->hw_info->plat_ptr->vcore_config)
		return md->hw_info->plat_ptr->vcore_config(md_id, 0);
	return -1;
}
int ccci_md_soft_stop(unsigned char md_id, unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->soft_stop)
		return md->ops->soft_stop(md, sim_mode);
	return -1;
}
int ccci_md_soft_start(unsigned char md_id, unsigned int sim_mode)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->soft_start)
		return md->ops->soft_start(md, sim_mode);
	return -1;
}

int ccci_md_send_runtime_data(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->send_runtime_data(md, CCCI_CONTROL_TX, 0, 0);
}

int ccci_md_reset_pccif(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md->ops->reset_pccif)
		return md->ops->reset_pccif(md);
	return -1;
}

void ccci_md_dump_info(unsigned char md_id, enum MODEM_DUMP_FLAG flag,
	void *buff, int length)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md)
		md->ops->dump_info(md, flag, buff, length);
	else
		CCCI_ERROR_LOG(md_id, TAG, "invalid md_id %d!!\n", md_id);
}
EXPORT_SYMBOL(ccci_md_dump_info);

void ccci_md_exception_handshake(unsigned char md_id, int timeout)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	md->ops->ee_handshake(md, timeout);
}

int ccci_md_send_ccb_tx_notify(unsigned char md_id, int core_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	return md->ops->send_ccb_tx_notify(md, core_id);
}

int ccci_md_force_assert(unsigned char md_id, enum MD_FORCE_ASSERT_TYPE type,
	char *param, int len)
{
	int ret = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	struct ccci_force_assert_shm_fmt *ccci_fa_smem_ptr = NULL;
	struct ccci_smem_region *force_assert =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_FORCE_ASSERT);

	if (md->is_force_asserted != 0)
		return ret;
	mdee_set_ex_time_str(md_id, type, param);
	if (type == MD_FORCE_ASSERT_BY_AP_MPU) {
		ret = md->ops->force_assert(md, CCIF_MPU_INTR);
	} else {
		ccci_fa_smem_ptr = (struct ccci_force_assert_shm_fmt *)
			force_assert->base_ap_view_vir;
		if (ccci_fa_smem_ptr) {
			ccci_fa_smem_ptr->error_code = type;
			if (param != NULL && len > 0) {
				if (len > force_assert->size -
				sizeof(struct ccci_force_assert_shm_fmt))
					len = force_assert->size -
					sizeof(
					struct ccci_force_assert_shm_fmt);
				memcpy_toio(ccci_fa_smem_ptr->param,
					param, len);
			}
		}
		ret = md->ops->force_assert(md, CCIF_INTERRUPT);
	}
	md->is_force_asserted = 1;
	return ret;
}
EXPORT_SYMBOL(ccci_md_force_assert);

static void append_runtime_feature(char **p_rt_data,
	struct ccci_runtime_feature *rt_feature, void *data)
{
	CCCI_DEBUG_LOG(-1, TAG,
		"append rt_data %p, feature %u len %u\n",
		*p_rt_data, rt_feature->feature_id,
		rt_feature->data_len);
	memcpy_toio(*p_rt_data, rt_feature,
		sizeof(struct ccci_runtime_feature));
	*p_rt_data += sizeof(struct ccci_runtime_feature);
	if (data != NULL) {
		memcpy_toio(*p_rt_data, data, rt_feature->data_len);
		*p_rt_data += rt_feature->data_len;
	}
}


struct ccci_tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};

unsigned int get_boot_mode_from_dts(void)
{
	struct device_node *np_chosen = NULL;
	struct ccci_tag_bootmode *tag = NULL;
	u32 bootmode = NORMAL_BOOT_ID;
	static int ap_boot_mode = -1;

	if (ap_boot_mode >= 0) {
		CCCI_NORMAL_LOG(-1, TAG,
			"[%s] bootmode: 0x%x\n", __func__, ap_boot_mode);
		return ap_boot_mode;
	}

	np_chosen = of_find_node_by_path("/chosen");
	if (!np_chosen) {
		CCCI_ERROR_LOG(-1, TAG, "warning: not find node: '/chosen'\n");

		np_chosen = of_find_node_by_path("/chosen@0");
		if (!np_chosen) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: not find node: '/chosen@0'\n",
				__func__);
			return NORMAL_BOOT_ID;
		}
	}

	tag = (struct ccci_tag_bootmode *)
			of_get_property(np_chosen, "atag,boot", NULL);
	if (!tag) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: not find tag: 'atag,boot';\n", __func__);
		return NORMAL_BOOT_ID;
	}

	if (tag->bootmode == META_BOOT || tag->bootmode == ADVMETA_BOOT)
		bootmode = META_BOOT_ID;

	else if (tag->bootmode == FACTORY_BOOT ||
			tag->bootmode == ATE_FACTORY_BOOT)
		bootmode = FACTORY_BOOT_ID;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] bootmode: 0x%x boottype: 0x%x; return: 0x%x\n",
		__func__, tag->bootmode, tag->boottype, bootmode);
	ap_boot_mode = bootmode;

	return bootmode;
}

/*
 *booting_start_id bit mapping:
 * |31---------16|15-----------8|7---------0|
 * | mdwait_time | logging_mode | boot_mode |
 * mdwait_time: getting from property at user space
 * logging_mode: usb/sd/idl mode, setting at user space
 * boot_mode: factory/meta/normal mode
 */
static unsigned int get_booting_start_id(struct ccci_modem *md)
{
	enum LOGGING_MODE mdlog_flag = MODE_IDLE;
	enum MODEM_BOOT_MODE md_boot_mode = MODE_UNKNOWN_BOOT;
	u32 booting_start_id;

	mdlog_flag = md->mdlg_mode & 0x0000ffff;
	md_boot_mode = md->boot_mode;
	booting_start_id = (((char)mdlog_flag << 8)
				| get_boot_mode_from_dts());
	if (md_boot_mode == MODE_META_BOOT) {
		booting_start_id = ((char)mdlog_flag << 8
						| META_BOOT_ID);
	} else {
		booting_start_id = ((char)mdlog_flag << 8
						| NORMAL_BOOT_ID);
	}
	booting_start_id |= md->mdlg_mode & 0xffff0000;

	CCCI_BOOTUP_LOG(md->index, TAG,
		"%s 0x%x\n", __func__, booting_start_id);
	CCCI_NORMAL_LOG(md->index, TAG,
		"%s 0x%x, boot_mode=%d, mdlg_wait=%d\n", __func__, booting_start_id, md_boot_mode,
		booting_start_id >> 16);
	return booting_start_id;
}

static void config_ap_side_feature(struct ccci_modem *md,
	struct md_query_ap_feature *md_feature)
{
	md->runtime_version = AP_MD_HS_V2;
	md_feature->feature_set[BOOT_INFO].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[EXCEPTION_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[CCIF_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

	md_feature->feature_set[CCISM_SHARE_MEMORY_EXP].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	if ((md->index == MD_SYS1) && ((get_md_resv_phy_cap_size(MD_SYS1) > 0)
		|| (get_md_resv_sib_size(MD_SYS1) > 0)))
		md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	else
		md_feature->feature_set[MD_PHY_CAPTURE].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[MD_CONSYS_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	/* notice: CCB_SHARE_MEMORY should be set to support
	 * when at least one CCB region exists
	 */
	md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[MD_PHY_CAPTURE].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[MD_CONSYS_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[CCB_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[DHL_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[AUDIO_RAW_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
    md_feature->feature_set[MISC_INFO_HIF_32K_LOW_POWER].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#ifdef ENABLE_32K_CLK_LESS
	if (mtk_crystal_exist_status()) {
		CCCI_DEBUG_LOG(md->index, TAG,
			"MISC_32K_LESS no support, mtk_crystal_exist_status 1\n");
		md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask
			= CCCI_FEATURE_NOT_SUPPORT;
	} else {
		CCCI_DEBUG_LOG(md->index, TAG, "MISC_32K_LESS support\n");
		md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask
			= CCCI_FEATURE_MUST_SUPPORT;
	}
#else
	CCCI_DEBUG_LOG(md->index, TAG, "ENABLE_32K_CLK_LESS disabled\n");
	md_feature->feature_set[MISC_INFO_RTC_32K_LESS].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	md_feature->feature_set[MISC_INFO_RANDOM_SEED_NUM].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MISC_INFO_SBP_ID].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[MISC_INFO_CCCI].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

	md_feature->feature_set[MISC_INFO_CLIB_TIME].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;

#if defined(USIP_SHARE_MEMORY_FEATURE_ENABLE) && !defined(CONFIG_MTK_ECCCI_FLASHLESS)
		md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_OPTIONAL_SUPPORT;
#else
		md_feature->feature_set[MD_USIP_SHARE_MEMORY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
#endif

	md_feature->feature_set[NVRAM_CACHE_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;

	/* This item is reserved */
	md_feature->feature_set[SECURITY_SHARE_MEMORY].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;

// AMMS_v3 @{
	if (md->hw_info->plat_val->md_gen >= 6298)
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
		md_feature->feature_set[AMMS_DRDI_COPY].support_mask =
			CCCI_FEATURE_MUST_SUPPORT;
#else
		md_feature->feature_set[AMMS_DRDI_COPY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
#endif
	else
		md_feature->feature_set[AMMS_DRDI_COPY].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
// @}

#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	md_feature->feature_set[MD_MEM_AP_VIEW_INF].support_mask =
		CCCI_FEATURE_OPTIONAL_SUPPORT;
#else
	md_feature->feature_set[MD_MEM_AP_VIEW_INF].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
#endif

// colgin @{
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	md_feature->feature_set[SAP_MEMDUMP_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[SAP_EXCEPTION_RECORD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[MD_PHY_CAPTURE].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[SAP_MEMDUMP_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[SAP_EXCEPTION_RECORD_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[MD_PHY_CAPTURE].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
	md_feature->feature_set[MISC_INFO_EAP_FEATURE].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
	md_feature->feature_set[DPMF_USB_PATH_SHARE_MEMORY].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	md_feature->feature_set[RUNTIME_SHARE_MEMORY_MIDR].support_mask
		= CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[RUNTIME_SHARE_MEMORY_MIDR].support_mask
		= CCCI_FEATURE_NOT_SUPPORT;
#endif
#if defined(CONFIG_MTK_AEE_FEATURE)
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	md_feature->feature_set[CCCI_DHL_MISC_INFO].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;
#else
	md_feature->feature_set[CCCI_DHL_MISC_INFO].support_mask =
		CCCI_FEATURE_NOT_SUPPORT;
	md_feature->feature_set[CCCI_DHL_MISC_INFO_V2].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;
#endif
	if (md->hw_info->plat_val->md_gen >= 6298)
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
		md_feature->feature_set[AEE_OFF_FAST_REBOOT].support_mask =
			CCCI_FEATURE_MUST_SUPPORT;
#else
		md_feature->feature_set[AEE_OFF_FAST_REBOOT].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
#endif
	else
		md_feature->feature_set[AEE_OFF_FAST_REBOOT].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
#endif
#if defined(CONFIG_MTK_ECCCI_FLASHLESS)
	md_feature->feature_set[SAR_IDX].support_mask =
		CCCI_FEATURE_MUST_SUPPORT;
	struct ThinMdDrdiRuntimeData drdi = get_drdi_rt_data();
	if (drdi.version == AMMS_V4) {
		md_feature->feature_set[DRDI_RT_DATA].support_mask =
			CCCI_FEATURE_MUST_SUPPORT;
	} else {
		md_feature->feature_set[DRDI_RT_DATA].support_mask =
			CCCI_FEATURE_NOT_SUPPORT;
	}
#endif
// @}
}

static void ccci_sib_region_set_runtime(struct ccci_runtime_feature *rt_feature,
	struct ccci_runtime_share_memory *rt_shm)
{
	phys_addr_t md_sib_mem_addr;
	unsigned int md_sib_mem_size;

	get_md_sib_mem_info(&md_sib_mem_addr, &md_sib_mem_size);
	rt_feature->data_len =
		sizeof(struct ccci_runtime_share_memory);
	rt_shm->addr = 0;
	if (md_sib_mem_addr)
		rt_shm->size = md_sib_mem_size;
	else
		rt_shm->size = 0;
}

static void ccci_md_mem_inf_prepare(int md_id,
		struct ccci_runtime_feature *rt_ft,
		struct ccci_runtime_md_mem_ap_addr *tbl, unsigned int num)
{
	unsigned int add_num = 0;
	phys_addr_t ro_rw_base, ncrw_base, crw_base;
	u32 ro_rw_size, ncrw_size, crw_size;
	int ret;

	ret = get_md_resv_mem_info(md_id, &ro_rw_base, &ro_rw_size,
					&ncrw_base, &ncrw_size);
	if (ret < 0) {
		CCCI_REPEAT_LOG(md_id, TAG, "%s get mdrorw and srw fail\n",
			__func__);
		return;
	}
	ret = get_md_resv_csmem_info(md_id, &crw_base, &crw_size);
	if (ret < 0) {
		CCCI_REPEAT_LOG(md_id, TAG, "%s get cache smem info fail\n",
			__func__);
		return;
	}

	/* Add bank 0 and bank 1 */
	if (add_num < num) {
		tbl[add_num].md_view_phy = 0;
		tbl[add_num].size = ro_rw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)ro_rw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(ro_rw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(md_id, TAG, "%s add bank0/1 fail(%d)\n",
			__func__, add_num);

	if (add_num < num) {
		tbl[add_num].md_view_phy = 0x40000000;
		tbl[add_num].size = ncrw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)ncrw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(ncrw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(md_id, TAG, "%s add bank4 nc fail(%d)\n",
			__func__, add_num);

	if (add_num < num) {
		tbl[add_num].md_view_phy = 0x40000000 +
				get_md_smem_cachable_offset(md_id);
		tbl[add_num].size = crw_size;
		tbl[add_num].ap_view_phy_lo32 = (u32)crw_base;
		tbl[add_num].ap_view_phy_hi32 = (u32)(crw_base >> 32);
		add_num++;
	} else
		CCCI_REPEAT_LOG(md_id, TAG, "%s add bank4 c fail(%d)\n",
			__func__, add_num);
	rt_ft->feature_id = MD_MEM_AP_VIEW_INF;
	rt_ft->data_len =
		(sizeof(struct ccci_runtime_md_mem_ap_addr)) * add_num;
}

static void ccci_smem_region_set_runtime(unsigned char md_id, unsigned int id,
	struct ccci_runtime_feature *rt_feature,
	struct ccci_runtime_share_memory *rt_shm)
{
	struct ccci_smem_region *region = ccci_md_get_smem_by_user_id(md_id,
		id);

	if (region) {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = region->base_md_view_phy;
		rt_shm->size = region->size;
	} else {
		rt_feature->data_len =
			sizeof(struct ccci_runtime_share_memory);
		rt_shm->addr = 0;
		rt_shm->size = 0;
	}
}

static unsigned int __maybe_unused
ccci_md_get_dhl_dump_config(unsigned char md_id)
{
	struct device_node *node;
	const char *aee_enable;
	unsigned int ret = 0;

	node = of_find_node_by_path("/chosen");
	if (node) {
		if (of_property_read_string(node, "aee,enable", &aee_enable) == 0) {
			if (strnstr(aee_enable, "mini", 4))
				ret = 1;
			else if (strnstr(aee_enable, "full", 4))
				ret = 2;
		}
		// CCCI_ERROR_LOG(md_id, TAG, "aee_enable=%d\n", ret);
	} else {
		CCCI_NORMAL_LOG(md_id, TAG, "Can't find chosen node\n");
	}
	of_node_put(node);
	return ret;
}

int ccci_md_prepare_runtime_data(unsigned char md_id, unsigned char *data,
	int length)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	u8 i = 0;
	u32 total_len;
	int j;
	/*runtime data buffer */
	struct ccci_smem_region *region;
	struct ccci_smem_region *rt_data_region =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_RUNTIME_DATA);
	char *rt_data = (char *)rt_data_region->base_ap_view_vir;

	struct ccci_runtime_feature rt_feature;
	/*runtime feature type */
	struct ccci_runtime_share_memory rt_shm;
	struct ccci_misc_info_element rt_f_element;
	struct ccci_runtime_md_mem_ap_addr rt_mem_view[4];

	struct md_query_ap_feature *md_feature = NULL;
	struct md_query_ap_feature md_feature_ap;
	struct ccci_runtime_boot_info boot_info;
	unsigned int random_seed = 0;
	struct timespec64 t;
#if defined(CONFIG_MTK_AEE_FEATURE)
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
	unsigned int aee_mode = 0;
	struct aee_dhl_dump_config aee_config;
	struct aee_off_fast_reboot_config fast_reboot_config;
#endif
#endif

	CCCI_BOOTUP_LOG(md->index, TAG,
		"prepare_runtime_data  AP total %u features\n",
		MD_RUNTIME_FEATURE_ID_MAX);

	memset(&md_feature_ap, 0, sizeof(struct md_query_ap_feature));
	config_ap_side_feature(md, &md_feature_ap);

	md_feature = (struct md_query_ap_feature *)(data +
				sizeof(struct ccci_header));

	if (md_feature->head_pattern != MD_FEATURE_QUERY_PATTERN ||
	    md_feature->tail_pattern != MD_FEATURE_QUERY_PATTERN) {
		CCCI_BOOTUP_LOG(md->index, TAG,
			"md_feature pattern is wrong: head 0x%x, tail 0x%x\n",
			md_feature->head_pattern, md_feature->tail_pattern);
		if (md->index == MD_SYS3)
			md->ops->dump_info(md, DUMP_FLAG_CCIF, NULL, 0);
		return -1;
	}

	for (i = BOOT_INFO; i < FEATURE_COUNT; i++) {
		memset(&rt_feature, 0, sizeof(struct ccci_runtime_feature));
		memset(&rt_shm, 0, sizeof(struct ccci_runtime_share_memory));
		memset(&rt_f_element, 0, sizeof(struct ccci_misc_info_element));
		rt_feature.feature_id = i;
		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT &&
		    md_feature_ap.feature_set[i].support_mask <
			CCCI_FEATURE_MUST_SUPPORT) {
			CCCI_BOOTUP_LOG(md->index, TAG,
				"feature %u not support for AP\n",
				rt_feature.feature_id);
			return -1;
		}

		if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_NOT_EXIST) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_MUST_SUPPORT) {
			rt_feature.support_info =
				md_feature->feature_set[i];
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_OPTIONAL_SUPPORT) {
			if (md_feature->feature_set[i].version ==
			md_feature_ap.feature_set[i].version &&
			md_feature_ap.feature_set[i].support_mask >=
			CCCI_FEATURE_MUST_SUPPORT) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		} else if (md_feature->feature_set[i].support_mask ==
			CCCI_FEATURE_SUPPORT_BACKWARD_COMPAT) {
			if (md_feature->feature_set[i].version >=
				md_feature_ap.feature_set[i].version) {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_MUST_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			} else {
				rt_feature.support_info.support_mask =
					CCCI_FEATURE_NOT_SUPPORT;
				rt_feature.support_info.version =
					md_feature_ap.feature_set[i].version;
			}
		}

		CCCI_DEBUG_LOG(md->index, TAG,
			"ftr %u md_mask %u, ver %u, ap_mask %u rt %u\n",
			rt_feature.feature_id,
			md_feature->feature_set[i].support_mask,
			md_feature->feature_set[i].version,
			md_feature_ap.feature_set[i].support_mask,
			rt_feature.support_info.support_mask);

		if (rt_feature.support_info.support_mask ==
		CCCI_FEATURE_MUST_SUPPORT) {
			switch (rt_feature.feature_id) {
			case BOOT_INFO:
				memset(&boot_info, 0, sizeof(boot_info));
				rt_feature.data_len = sizeof(boot_info);
				boot_info.boot_channel = CCCI_CONTROL_RX;
				boot_info.booting_start_id =
					get_booting_start_id(md);
				append_runtime_feature(&rt_data,
					&rt_feature, &boot_info);
				break;
			case EXCEPTION_SHARE_MEMORY:
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_MDCCCI_DBG);
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = region->base_md_view_phy;
				rt_shm.size = CCCI_EE_SMEM_TOTAL_SIZE;
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCIF_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_CCISM_MCU,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case CCB_SHARE_MEMORY:
				/* notice: we should add up
				 * all CCB region size here
				 */
				/* ctrl control first */
				region = ccci_md_get_smem_by_user_id(md_id,
					SMEM_USER_RAW_CCB_CTRL);
				if (region) {
					rt_feature.data_len =
					sizeof(struct ccci_misc_info_element);
					rt_f_element.feature[0] =
					region->base_md_view_phy;
					rt_f_element.feature[1] =
					region->size;
				}
				/* ccb data second */
				for (j = SMEM_USER_CCB_START;
					j <= SMEM_USER_CCB_END; j++) {
					region = ccci_md_get_smem_by_user_id(
						md_id, j);
					if (j == SMEM_USER_CCB_START
						&& region) {
						rt_f_element.feature[2] =
						region->base_md_view_phy;
						rt_f_element.feature[3] = 0;
					} else if (j == SMEM_USER_CCB_START
							&& region == NULL)
						break;
					if (region)
						rt_f_element.feature[3] +=
						region->size;
				}
				CCCI_BOOTUP_LOG(md->index, TAG,
					"ccb data size (include dsp raw): %X\n",
					rt_f_element.feature[3]);

				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case DHL_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_DHL,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
					&rt_feature, &rt_shm);
				break;
			case MISC_INFO_RTC_32K_LESS:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_RANDOM_SEED_NUM:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				get_random_bytes(&random_seed, sizeof(int));
				rt_f_element.feature[0] = random_seed;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_SBP_ID:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = md->sbp_code;
				if (md->per_md_data.config.load_type
					< modem_ultg)
					rt_f_element.feature[1] = 0;
				else
					rt_f_element.feature[1] =
					get_wm_bitmap_for_ubin();
				CCCI_BOOTUP_LOG(md->index, TAG,
					"sbp=0x%x,wmid[%d]\n",
					rt_f_element.feature[0],
					rt_f_element.feature[1]);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CCCI:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				/* sequence check */
				rt_f_element.feature[0] |= (1 << 0);
				/* polling MD status */
				rt_f_element.feature[0] |= (1 << 1);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case MISC_INFO_CLIB_TIME:
				rt_feature.data_len =
				sizeof(struct ccci_misc_info_element);
				ktime_get_real_ts64(&t);
				/*set seconds information */
				rt_f_element.feature[0] =
				((unsigned int *)&t.tv_sec)[0];
				CCCI_NORMAL_LOG(md->index, TAG, "MISC_INFO_CLIB_TIME t.tv_sec[0]: %d\n",
						rt_f_element.feature[0]);
				rt_f_element.feature[1] =
				((unsigned int *)&t.tv_sec)[1];
				/*sys_tz.tz_minuteswest; */
				rt_f_element.feature[2] = current_time_zone;
				/*not used for now */
				rt_f_element.feature[3] = sys_tz.tz_dsttime;
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_f_element);
				break;
			case AUDIO_RAW_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_AUDIO,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data,
				&rt_feature, &rt_shm);
				break;
			case CCISM_SHARE_MEMORY_EXP:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_CCISM_MCU_EXP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_PHY_CAPTURE:
				ccci_sib_region_set_runtime(&rt_feature,
					&rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_CONSYS_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_MD_CONSYS,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_USIP_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_RAW_USIP,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case NVRAM_CACHE_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_MD_NVRAM_CACHE,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			case MD_MEM_AP_VIEW_INF:
				ccci_md_mem_inf_prepare(md_id, &rt_feature,
					rt_mem_view, 4);
				append_runtime_feature(&rt_data, &rt_feature,
				rt_mem_view);
				break;
			case MISC_INFO_HIF_32K_LOW_POWER:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_32K_LOW_POWER,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			case SAP_MEMDUMP_SHARE_MEMORY:
				rt_feature.data_len =
				sizeof(struct ccci_runtime_share_memory);
				rt_shm.addr = SAP_MEM_ADDR;
				rt_shm.size = SAP_MEM_SIZE;
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			// colgin @{
			case SAP_EXCEPTION_RECORD_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_SAP_EX_DBG,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			case MISC_INFO_EAP_FEATURE:
				rt_feature.data_len =
					sizeof(struct ccci_misc_info_element);
				//hs_timeout should align with BOOT_TIMEOUT
				hs_timeout = 30 * 1000;//ms
				rt_f_element.feature[0] = hs_timeout;
				CCCI_BOOTUP_LOG(md->index, TAG,
					"HS TIMEOUT change to %dS\n",
					hs_timeout/1000);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_f_element);
				break;
			case DPMF_USB_PATH_SHARE_MEMORY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_USB_DATA,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			case RUNTIME_SHARE_MEMORY_MIDR:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_MD_CDMR,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_shm);
				break;
			// @}
#if defined(CONFIG_MTK_AEE_FEATURE)
#if !defined(CONFIG_MTK_ECCCI_FLASHLESS)
			case CCCI_DHL_MISC_INFO:
				rt_feature.data_len =
					sizeof(struct aee_dhl_dump_config);
				memset(&aee_config, 0, sizeof(struct aee_dhl_dump_config));
				/* use the aee_mode as temp_u32 value */
				aee_mode = ccci_md_get_dhl_dump_config(md_id);
				if (aee_mode == 1) {  // aee mini
					aee_config.mini_dump_flag = 1;
					aee_config.midr_dump_flag = 0;
				} else if (aee_mode == 2) {  // aee full
					aee_config.mini_dump_flag = 0;
					aee_config.midr_dump_flag = 1;
				} else {  // aee off
					aee_config.mini_dump_flag = 0;
					aee_config.midr_dump_flag = 0;
				}
				append_runtime_feature(&rt_data, &rt_feature,
					&aee_config);
				CCCI_NORMAL_LOG(md_id, TAG,
					"sync DHL config done, aee_mode=%d\n",
					aee_mode);
				break;
			case AEE_OFF_FAST_REBOOT:
				rt_feature.data_len =
					sizeof(struct aee_off_fast_reboot_config);
				memset(&fast_reboot_config, 0,
					sizeof(struct aee_off_fast_reboot_config));
				fast_reboot_config.fast_reboot_flag = 0;

#if defined(AEE_OFF_FAST_REBOOT_ENABLE)
				aee_mode =
					ccci_md_get_dhl_dump_config(md_id);
				if (aee_off_fast_reboot_enable && aee_mode == 0)
					fast_reboot_config.fast_reboot_flag = 1;
#endif
				append_runtime_feature(&rt_data, &rt_feature,
					&fast_reboot_config);
				CCCI_NORMAL_LOG(md_id, TAG,
					"sync fast_reboot_flag=%d\n",
					fast_reboot_config.fast_reboot_flag);
				break;
#else
			case CCCI_DHL_MISC_INFO_V2:
				rt_feature.data_len = sizeof(struct ThinMdDhl);
				struct ThinMdDhl dhl = get_dhl_misc_info();
				append_runtime_feature(&rt_data, &rt_feature, &dhl);
				CCCI_NORMAL_LOG(md_id, TAG,
					"ver=0x%x, aee_mini=0x%x, midr=0x%x, memory_dump=0x%x\n",
					dhl.version, dhl.aee_mini_dump_flag, dhl.midr_dump_flag,
					dhl.memory_dump_flag);
				break;
#endif
#endif
#if defined(CONFIG_MTK_ECCCI_FLASHLESS)
			case SAR_IDX:
				rt_feature.data_len =
					sizeof(struct ccci_misc_info_element);
				rt_f_element.feature[0] = get_sar_idx();
				append_runtime_feature(&rt_data, &rt_feature,
					&rt_f_element);
				CCCI_NORMAL_LOG(md_id, TAG, "sar_idx=%d\n",
					rt_f_element.feature[0]);
				break;
			case DRDI_RT_DATA:
				rt_feature.data_len = sizeof(struct ThinMdDrdiRuntimeData);
				struct ThinMdDrdiRuntimeData drdi = get_drdi_rt_data();
				append_runtime_feature(&rt_data, &rt_feature, &drdi);
				CCCI_NORMAL_LOG(md_id, TAG,
					"drdi ver=0x%x, error_code=0x%x\n",
					drdi.version, drdi.error_code);
				break;

#endif
			case AMMS_DRDI_COPY:
				ccci_smem_region_set_runtime(md_id,
					SMEM_USER_MD_DRDI,
					&rt_feature, &rt_shm);
				append_runtime_feature(&rt_data, &rt_feature,
				&rt_shm);
				break;
			default:
				break;
			};
		} else {
			rt_feature.data_len = 0;
			append_runtime_feature(&rt_data, &rt_feature, NULL);
		}

	}
	md->multi_md_mpu_support = 1;
	total_len = rt_data - (char *)rt_data_region->base_ap_view_vir;
	CCCI_BOOTUP_DUMP_LOG(md->index, TAG, "AP runtime data\n");
	ccci_util_mem_dump(md->index, CCCI_DUMP_BOOTUP,
		rt_data_region->base_ap_view_vir, total_len);

	return 0;
}

struct ccci_runtime_feature *ccci_md_get_rt_feature_by_id(unsigned char md_id,
	u8 feature_id, u8 ap_query_md)
{
	struct ccci_runtime_feature *rt_feature = NULL;
	u8 i = 0;
	u8 max_id = 0;
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);
	struct ccci_smem_region *rt_data_region =
		ccci_md_get_smem_by_user_id(md_id, SMEM_USER_RAW_RUNTIME_DATA);

	if (ap_query_md) {
		rt_feature = (struct ccci_runtime_feature *)
		(rt_data_region->base_ap_view_vir +
			CCCI_SMEM_SIZE_RUNTIME_AP);
		max_id = AP_RUNTIME_FEATURE_ID_MAX;
	} else {
		rt_feature = (struct ccci_runtime_feature *)
		(rt_data_region->base_ap_view_vir);
		max_id = MD_RUNTIME_FEATURE_ID_MAX;
	}
	while (i < max_id) {
		if (feature_id == rt_feature->feature_id)
			return rt_feature;
		if (rt_feature->data_len >
			sizeof(struct ccci_misc_info_element)) {
			CCCI_ERROR_LOG(md->index, TAG,
				"get invalid feature, id %u\n", i);
			return NULL;
		}
		rt_feature = (struct ccci_runtime_feature *)
		((char *)rt_feature->data + rt_feature->data_len);
		i++;
	}

	return NULL;
}

int ccci_md_parse_rt_feature(unsigned char md_id,
	struct ccci_runtime_feature *rt_feature, void *data, u32 data_len)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (unlikely(!rt_feature)) {
		CCCI_ERROR_LOG(md->index, TAG,
			"parse_md_rt_feature: rt_feature == NULL\n");
		return -EFAULT;
	}
	if (unlikely(rt_feature->data_len > data_len ||
		rt_feature->data_len == 0)) {
		CCCI_ERROR_LOG(md->index, TAG,
			"rt_feature %u data_len = %u, expected data_len %u\n",
			rt_feature->feature_id, rt_feature->data_len, data_len);
		return -EFAULT;
	}

	memcpy(data, (const void *)((char *)rt_feature->data),
		rt_feature->data_len);

	return 0;
}

struct ccci_per_md *ccci_get_per_md_data(unsigned char md_id)
{
	struct ccci_modem *md = ccci_md_get_modem_by_id(md_id);

	if (md)
		return &md->per_md_data;
	else
		return NULL;
}
EXPORT_SYMBOL(ccci_get_per_md_data);

static void receive_wakeup_src_notify(int md_id, char *buf, unsigned int len)
{
	int tmp_data = 0;

	if (len == 0) {
		/* before spm add MD_WAKEUP_SOURCE parameter. */
		if (md_id == MD_SYS1) {
			ccci_hif_set_wakeup_src(MD1_NET_HIF, 1);
			ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		}
		if (md_id == MD_SYS3)
			ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		return;
	}

	/* after spm add MD_WAKEUP_SOURCE parameter. */
	if (len > sizeof(tmp_data))
		len = sizeof(tmp_data);
	memcpy((void *)&tmp_data, buf, len);
	switch (tmp_data) {
	case WAKE_SRC_HIF_CCIF0:
		ccci_hif_set_wakeup_src(CCIF_HIF_ID, 1);
		break;
	case WAKE_SRC_HIF_DPMAIF:
		ccci_hif_set_wakeup_src(MD1_NET_HIF, 1);
		break;
	default:
		break;
	};
}

int exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf,
	unsigned int len)
{
	int ret = 0;
	int tmp_data;

	if (!get_modem_is_enabled(md_id)) {
		CCCI_ERROR_LOG(md_id, CORE,
			"wrong MD ID from %ps for %d\n",
			__builtin_return_address(0), id);
		return -CCCI_ERR_MD_INDEX_NOT_FOUND;
	}

	CCCI_DEBUG_LOG(md_id, CORE, "%ps execute function %d\n",
		__builtin_return_address(0), id);
	switch (id) {
	case ID_GET_MD_WAKEUP_SRC:
		receive_wakeup_src_notify(md_id, buf, len);
		break;
	case ID_FORCE_MD_ASSERT:
		CCCI_NORMAL_LOG(md_id, CORE, "Force MD assert called by %s\n",
			current->comm);
		ret = ccci_md_force_assert(md_id,
			MD_FORCE_ASSERT_BY_USER_TRIGGER,
			NULL, 0);
		break;
	case ID_MD_MPU_ASSERT:
		if (md_id == MD_SYS1) {
			if (buf != NULL && strlen(buf)) {
				CCCI_NORMAL_LOG(md_id, CORE,
					"Force MD assert(MPU) called by %s\n",
					current->comm);
				ret = ccci_md_force_assert(md_id,
					MD_FORCE_ASSERT_BY_AP_MPU,
					buf, len);
			} else {
				CCCI_NORMAL_LOG(md_id, CORE,
					"ACK (MPU violation) called by %s\n",
					current->comm);
				ret = ccci_port_send_msg_to_md(md_id,
					CCCI_SYSTEM_TX,
					MD_AP_MPU_ACK_MD, 0, 0);
			}
		} else
			CCCI_NORMAL_LOG(md_id, CORE,
				"MD%d MPU API called by %s\n",
				md_id, current->comm);
		break;
	case ID_PAUSE_LTE:
		/*
		 * MD booting/flight mode/exception mode: return >0 to DVFS.
		 * MD ready: return 0 if message delivered,
		 * return <0 if get error.
		 * DVFS will call this API with IRQ disabled.
		 */
		if (ccci_fsm_get_md_state(md_id) != READY)
			ret = 1;
		else {
			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
					MD_PAUSE_LTE, *((int *)buf), 1);
			if (ret == -CCCI_ERR_MD_NOT_READY ||
				ret == -CCCI_ERR_HIF_NOT_POWER_ON)
				ret = 1;
		}
		break;
	case ID_GET_MD_STATE:
		ret = ccci_fsm_get_md_state_for_user(md_id);
		break;
		/* used for throttling feature - start */
	case ID_THROTTLING_CFG:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				MD_THROTTLING,
				*((int *)buf), 1);
		break;
		/* used for throttling feature - end */
	case ID_UPDATE_TX_POWER:
		{
			unsigned int msg_id = (md_id == 0) ?
				MD_SW_MD1_TX_POWER :
				MD_SW_MD2_TX_POWER;
			unsigned int mode = *((unsigned int *)buf);

			ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				msg_id, mode, 0);
		}
		break;
	case ID_DUMP_MD_SLEEP_MODE:
		ccci_md_dump_info(md_id, DUMP_FLAG_SMEM_MDSLP, NULL, 0);
		break;
	case ID_PMIC_INTR:
		ret = ccci_port_send_msg_to_md(md_id,
				CCCI_SYSTEM_TX, PMIC_INTR_MODEM_BUCK_OC,
				*((int *)buf), 1);
		break;
	case ID_LWA_CONTROL_MSG:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			LWA_CONTROL_MSG, *((int *)buf), 1);
		break;
	case MD_TX_POWER:
	case MD_RF_MAX_TEMPERATURE_SUB6:
	case MD_RF_ALL_TEMPERATURE_MMW:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				id, 0, 0);
		break;
	case MD_DISPLAY_DYNAMIC_MIPI:
		tmp_data = 0;
		memcpy((void *)&tmp_data, buf, len);
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			id, tmp_data, 0);
		break;
	case ID_AP2MD_LOWPWR:
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
			CCMSG_ID_SYSMSGSVC_LOWPWR_APSTS_NOTIFY,
			*((int *)buf), 1);
		break;
	case SYSMSGSV_PCIE_PM_NOTIFY:
		tmp_data = 0;
		len = (len > sizeof(int)) ? sizeof(int) : len;
		memcpy((void *)&tmp_data, buf, len);
		ret = ccci_port_send_msg_to_md(md_id, CCCI_SYSTEM_TX,
				id, tmp_data, 0);
		break;
	default:
		ret = -CCCI_ERR_FUNC_ID_ERROR;
		break;
	};
	return ret;
}
EXPORT_SYMBOL(exec_ccci_kern_func_by_md_id);
