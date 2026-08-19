/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __ECCCI_INTERNAL_OPTION__
#define __ECCCI_INTERNAL_OPTION__

// colgin @{
#define SAP_MEM_ADDR 0x44000000
#define SAP_MEM_SIZE 0x4000000
#define MD_VIEW_BANK4 0x40000000
// @}

/* platform info */
#define MD_GENERATION       (6297)
#define MD_PLATFORM_INFO    "6297"
#define AP_PLATFORM_INFO    "MT6880"
#define CCCI_DRIVER_VER     0x20110118

// data plane type: 0: modem (datacard)
//                  1: dpmaif (CPE)
//                  2: modem (mt6980 datacard)
//                  3: dpmaif (mt6990 CPE)
#ifdef CONFIG_MTK_ECCCI_DATA_PLANE_TYPE
#define DATA_PLANE_TYPE (CONFIG_MTK_ECCCI_DATA_PLANE_TYPE)
#else
#define DATA_PLANE_TYPE (0)
#endif

//#if (DATA_PLANE_TYPE == 0)  // fix mt6890 build fail(dpmaif_reg_v3.h not ready)
#define MT6297  // only define for mt6880
//#endif
#define _97_REORDER_BAT_PAGE_TABLE_

/* flag to tell WDT is triggered by EPON or not, in MD SS debug region */
#define MD_L2SRAM_SIZE (0x1800)
//#define CCCI_EE_OFFSET_EPON_MD1 (0x2844)
#define CCCI_EE_OFFSET_EPON_MD3 (0x464)

//#define _HW_REORDER_SW_WORKAROUND_  //not define for mt6800 and mt6890
//#define ENABLE_CPU_AFFINITY  　　　　//not define for mt6800 and mt6890

#define REFINE_BAT_OFFSET_REMOVE
#define PIT_USING_CACHE_MEM

//#define CCCI_USE_DFD_OFFSET_0   // new feature for DFD

//#define CCCI_LOG_LEVEL  CCCI_LOG_ALL_UART
#define USING_PM_RUNTIME

#if ((DATA_PLANE_TYPE == 1) || (DATA_PLANE_TYPE == 3))
/* @mt6298 }*/
#define _DPMAIF_GEN98_CODA_
#if (DATA_PLANE_TYPE == 3)
#define _DPMAIF_GEN98_CODA_V2
#define DPMAIF_MEDHW_RXIRQ
#endif
/* Data packets originate from or are destined for DPMAIF */
#define DATA_PLANE_DPMAIF
#ifdef CONFIG_MTK_TINYSYS_MEDMCU_SUPPORT
#define _DPMAIF_MED_SUPPORT_
#endif
//#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
//#define FEATURE_SCP_CCCI_SUPPORT  //　move to ccci_common_config.h on K54
//#endif
#define ENABLE_CPU_AFFINITY_WITH_MED
#define IRQ_AFFINITY_WITH_MED (0x02)
#define TASK_AFFINITY_WITH_MED (0x06)
#define CCCI_CPUNUM (4)
/* @mt6298 }*/

/* USIP share memory enable on CPE */
#define USIP_SHARE_MEMORY_FEATURE_ENABLE

#else
// enable fast reboot after EE when aee off(aee_enable = 0)
#define AEE_OFF_FAST_REBOOT_ENABLE
#endif

#if ((DATA_PLANE_TYPE == 2) || (DATA_PLANE_TYPE == 3))
#define _CTRL_PLANE_GEN98_
#endif

#endif
