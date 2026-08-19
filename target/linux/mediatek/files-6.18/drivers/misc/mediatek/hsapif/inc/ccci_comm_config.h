// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __MTK_CCCI_COMM_CONFIG_H__
#define __MTK_CCCI_COMM_CONFIG_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#if 0
enum SMEM_USER_ID {
	/* this should remain to be 0 for backward compatibility */
	SMEM_USER_RAW_DBM = 0,

	/* sequence in CCB users matters, must align with ccb_configs[]  */
	SMEM_USER_CCB_START,
	SMEM_USER_CCB_DHL = SMEM_USER_CCB_START,
	SMEM_USER_CCB_MD_MONITOR,
	SMEM_USER_CCB_META,
	SMEM_USER_CCB_END = SMEM_USER_CCB_META,

	/* squence of other users does not matter */
	SMEM_USER_RAW_CCB_CTRL,
	SMEM_USER_RAW_DHL,
	SMEM_USER_RAW_MDM,
	SMEM_USER_RAW_NETD,
	SMEM_USER_RAW_USB,
	SMEM_USER_RAW_AUDIO,
	SMEM_USER_RAW_DFD,
	SMEM_USER_RAW_LWA,
	SMEM_USER_RAW_MDCCCI_DBG,
	SMEM_USER_RAW_MDSS_DBG,
	SMEM_USER_RAW_RUNTIME_DATA,
	SMEM_USER_RAW_FORCE_ASSERT,
	SMEM_USER_CCISM_SCP,
	SMEM_USER_RAW_MD2MD,
	SMEM_USER_RAW_RESERVED,
	SMEM_USER_CCISM_MCU,
	SMEM_USER_CCISM_MCU_EXP,
	SMEM_USER_SMART_LOGGING,
	SMEM_USER_RAW_MD_CONSYS,
	SMEM_USER_RAW_PHY_CAP,
	SMEM_USER_RAW_USIP,
	SMEM_USER_RAW_UDC_DATA,
	SMEM_USER_RAW_UDC_DESCTAB,
	SMEM_USER_RAW_AMMS_POS,
	SMEM_USER_RAW_ALIGN_PADDING,
	SMEM_USER_MAX,
};

enum MD_SYS {
	/* MD SYS name counts from 1,
	 * but internal index counts from 0
	 */
	MD_SYS1 = 0,
	MD_SYS2,
	MD_SYS3,
	MD_SYS4,
	MD_SYS5 = 4,
	MAX_MD_NUM
};
#endif

#ifndef __MT_CCCI_COMMON_H__
/*If mtk_ccci_common.h is included, DO NOT define ccci_header in this file*/
struct ccci_header {
	/* do NOT assume data[1] is data length in Rx */
	u32 data[2];
	u16 channel:16;
	u16 seq_num:15;
	u16 assert_bit:1;
	u32 reserved;
} __packed;
#endif

//#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
struct ctrl_msg_h {
	u32 ctrl_msg_id;
	u32 reserved;
	u32 data_length;// data buffer size, not include ctrl header  size
	u8 data[0];
}__packed;
//#endif

/* ============================================================= */
/* CCCI HSAPIF Error number defination */
/* ============================================================= */
/* CCCI HSAPIF error number region */
#define CCCI_ERR_HSAPIF_REGION_START_ID	(1500)

/* ---- CCCI HSAPIF*/
#define CCCI_HSAPIF_ERR_INVALID_LOGIC_CHANNEL_ID \
	(CCCI_ERR_HSAPIF_REGION_START_ID+0)
#define CCCI_HSAPIF_ERR_PUSH_RX_DATA_TO_TX_CHANNEL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+1)
#define CCCI_HSAPIF_ERR_REG_CALL_BACK_FOR_TX_CHANNEL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+2)
#define CCCI_HSAPIF_ERR_LOGIC_CH_HAS_REGISTERED \
	(CCCI_ERR_HSAPIF_REGION_START_ID+3)
#define CCCI_HSAPIF_ERR_MD_NOT_READY \
	(CCCI_ERR_HSAPIF_REGION_START_ID+4)
#define CCCI_HSAPIF_ERR_ALLOCATE_MEMORY_FAIL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+5)
#define CCCI_HSAPIF_ERR_CREATE_CCIF_INSTANCE_FAIL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+6)
#define CCCI_HSAPIF_ERR_REPEAT_CHANNEL_ID \
	(CCCI_ERR_HSAPIF_REGION_START_ID+7)
#define CCCI_HSAPIF_ERR_KFIFO_IS_NOT_READY \
	(CCCI_ERR_HSAPIF_REGION_START_ID+8)
#define CCCI_HSAPIF_ERR_GET_NULL_POINTER \
	(CCCI_ERR_HSAPIF_REGION_START_ID+9)
#define CCCI_HSAPIF_ERR_GET_RX_DATA_FROM_TX_CHANNEL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+10)
#define CCCI_HSAPIF_ERR_CHANNEL_NUM_MIS_MATCH \
	(CCCI_ERR_HSAPIF_REGION_START_ID+11)
#define CCCI_HSAPIF_ERR_START_ADDR_NOT_4BYTES_ALIGN \
	(CCCI_ERR_HSAPIF_REGION_START_ID+12)
#define CCCI_HSAPIF_ERR_NOT_DIVISIBLE_BY_4 \
	(CCCI_ERR_HSAPIF_REGION_START_ID+13)
#define CCCI_HSAPIF_ERR_MD_AT_EXCEPTION \
	(CCCI_ERR_HSAPIF_REGION_START_ID+14)
#define CCCI_HSAPIF_ERR_MD_CB_HAS_REGISTER \
	(CCCI_ERR_HSAPIF_REGION_START_ID+15)
#define CCCI_HSAPIF_ERR_MD_INDEX_NOT_FOUND \
	(CCCI_ERR_HSAPIF_REGION_START_ID+16)
#define CCCI_HSAPIF_ERR_DROP_PACKET \
	(CCCI_ERR_HSAPIF_REGION_START_ID+17)
#define CCCI_HSAPIF_ERR_PORT_RX_FULL \
	(CCCI_ERR_HSAPIF_REGION_START_ID+18)
#define CCCI_HSAPIF_ERR_SYSFS_NOT_READY \
	(CCCI_ERR_HSAPIF_REGION_START_ID+19)
#define CCCI_HSAPIF_ERR_IPC_ID_ERROR \
	(CCCI_ERR_HSAPIF_REGION_START_ID+20)
#define CCCI_HSAPIF_ERR_FUNC_ID_ERROR \
	(CCCI_ERR_HSAPIF_REGION_START_ID+21)
#define CCCI_HSAPIF_ERR_INVALID_QUEUE_INDEX \
	(CCCI_ERR_HSAPIF_REGION_START_ID+22)
#define CCCI_HSAPIF_ERR_HIF_NOT_POWER_ON \
	(CCCI_ERR_HSAPIF_REGION_START_ID+23)
#define CCCI_HSAPIF_ERR_DIPC_PORT_CFG_DISABLE \
	(CCCI_ERR_HSAPIF_REGION_START_ID+24)
#define CCCI_HSAPIF_ERR_INVALID_MSG_ID \
	(CCCI_ERR_HSAPIF_REGION_START_ID+25)

#if 0
struct lhif_header {
	u16 pdcp_count;
	u8 flow:4;
	u8 f:3;
	u8 reserved:1;
	u8 netif:5;
	u8 nw_type:3;
} __packed;


struct md_check_header {
	unsigned char check_header[12];	/* magic number is "CHECK_HEADER"*/
	unsigned int  header_verno;	/* header structure version number */
	/* 0x0:invalid; 0x1:debug version; 0x2:release version */
	unsigned int  product_ver;

	/* 0x0:invalid; 0x1:2G modem; 0x2: 3G modem */
	unsigned int  image_type;
	unsigned char platform[16];	/* MT6573_S01 or MT6573_S02 */
	unsigned char build_time[64];	/* build time string */
	unsigned char build_ver[64];	/* project version, ex:11A_MD.W11.28 */

	/* bind to md sys id, MD SYS1: 1, MD SYS2: 2 */
	unsigned char bind_sys_id;
	unsigned char ext_attr;		/* no shrink: 0, shrink: 1*/
	unsigned char reserved[2];	/* for reserved */

	/* md ROM/RAM image size requested by md */
	unsigned int  mem_size;
	unsigned int  md_img_size;	/* md image size, exclude head size*/
	unsigned int  reserved_info;	/* for reserved */
	unsigned int  size;		/* the size of this structure */
} __packed;


/* MPU setting */
struct _mpu_cfg {
	unsigned int start;
	unsigned int end;
	int region;
	unsigned int permission;
	int relate_region; /* Using same behavior and setting */
};

/* ============================================================= */
/* CCCI Error number defination */
/* ============================================================= */
/* CCCI error number region */
#define CCCI_ERR_MODULE_INIT_START_ID   (0)
#define CCCI_ERR_COMMON_REGION_START_ID	(100)
#define CCCI_ERR_CCIF_REGION_START_ID	(200)
#define CCCI_ERR_CCCI_REGION_START_ID	(300)
#define CCCI_ERR_LOAD_IMG_START_ID	(400)

/* ---- CCCI */
#define CCCI_ERR_INVALID_LOGIC_CHANNEL_ID \
	(CCCI_ERR_CCCI_REGION_START_ID+0)
#define CCCI_ERR_PUSH_RX_DATA_TO_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+1)
#define CCCI_ERR_REG_CALL_BACK_FOR_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+2)
#define CCCI_ERR_LOGIC_CH_HAS_REGISTERED \
	(CCCI_ERR_CCCI_REGION_START_ID+3)
#define CCCI_ERR_MD_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+4)
#define CCCI_ERR_ALLOCATE_MEMORY_FAIL \
	(CCCI_ERR_CCCI_REGION_START_ID+5)
#define CCCI_ERR_CREATE_CCIF_INSTANCE_FAIL \
	(CCCI_ERR_CCCI_REGION_START_ID+6)
#define CCCI_ERR_REPEAT_CHANNEL_ID \
	(CCCI_ERR_CCCI_REGION_START_ID+7)
#define CCCI_ERR_KFIFO_IS_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+8)
#define CCCI_ERR_GET_NULL_POINTER \
	(CCCI_ERR_CCCI_REGION_START_ID+9)
#define CCCI_ERR_GET_RX_DATA_FROM_TX_CHANNEL \
	(CCCI_ERR_CCCI_REGION_START_ID+10)
#define CCCI_ERR_CHANNEL_NUM_MIS_MATCH \
	(CCCI_ERR_CCCI_REGION_START_ID+11)
#define CCCI_ERR_START_ADDR_NOT_4BYTES_ALIGN \
	(CCCI_ERR_CCCI_REGION_START_ID+12)
#define CCCI_ERR_NOT_DIVISIBLE_BY_4 \
	(CCCI_ERR_CCCI_REGION_START_ID+13)
#define CCCI_ERR_MD_AT_EXCEPTION \
	(CCCI_ERR_CCCI_REGION_START_ID+14)
#define CCCI_ERR_MD_CB_HAS_REGISTER \
	(CCCI_ERR_CCCI_REGION_START_ID+15)
#define CCCI_ERR_MD_INDEX_NOT_FOUND \
	(CCCI_ERR_CCCI_REGION_START_ID+16)
#define CCCI_ERR_DROP_PACKET \
	(CCCI_ERR_CCCI_REGION_START_ID+17)
#define CCCI_ERR_PORT_RX_FULL \
	(CCCI_ERR_CCCI_REGION_START_ID+18)
#define CCCI_ERR_SYSFS_NOT_READY \
	(CCCI_ERR_CCCI_REGION_START_ID+19)
#define CCCI_ERR_IPC_ID_ERROR \
	(CCCI_ERR_CCCI_REGION_START_ID+20)
#define CCCI_ERR_FUNC_ID_ERROR \
	(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_INVALID_QUEUE_INDEX \
	(CCCI_ERR_CCCI_REGION_START_ID+21)
#define CCCI_ERR_HIF_NOT_POWER_ON \
	(CCCI_ERR_CCCI_REGION_START_ID+22)


#define IMG_NAME_LEN 32
#define IMG_POSTFIX_LEN 16
#define IMG_PATH_LEN 64

/* ============================================================== */
/* Image type and header defination part */
/* ============================================================== */
enum MD_IMG_TYPE {
	IMG_MD = 0,
	IMG_DSP,
	IMG_ARMV7,
	IMG_NUM,
};

enum PRODUCT_VER_TYPE {
	INVALID_VARSION = 0,
	DEBUG_VERSION,
	RELEASE_VERSION
};

enum MD_BOOT_MODE {
	MD_BOOT_MODE_INVALID = 0,
	MD_BOOT_MODE_NORMAL,
	MD_BOOT_MODE_META,
	MD_BOOT_MODE_MAX,
};

/* MD type defination */
enum MD_LOAD_TYPE {
	md_type_invalid = 0,
	modem_2g = 1,
	modem_3g,
	modem_wg,
	modem_tg,
	modem_lwg,
	modem_ltg,
	modem_sglte,
	modem_ultg,
	modem_ulwg,
	modem_ulwtg,
	modem_ulwcg,
	modem_ulwctg,
	modem_ulttg,
	modem_ulfwg,
	modem_ulfwcg,
	modem_ulctg,
	modem_ultctg,
	modem_ultwg,
	modem_ultwcg,
	modem_ulftg,
	modem_ulfctg,
	MAX_IMG_NUM = modem_ulfctg /* this enum starts from 1 */
};
#endif



#endif	/* __MTK_CCCI_COMM_CONFIG_H__ */
