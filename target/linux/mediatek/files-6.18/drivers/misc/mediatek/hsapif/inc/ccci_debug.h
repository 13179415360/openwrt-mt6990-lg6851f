// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_DEBUG_H__
#define __CCCI_DEBUG_H__

#include <linux/sched.h>

enum {
	HSAPIF_CCCI_LOG_ALL_UART = 1,
	HSAPIF_CCCI_LOG_ALL_MOBILE,
	HSAPIF_CCCI_LOG_CRITICAL_UART,
	HSAPIF_CCCI_LOG_CRITICAL_MOBILE,
	HSAPIF_CCCI_LOG_ALL_OFF,
};

/* CCCI dump */
#define CCCI_DUMP_TIME_FLAG		(1<<0)
#define CCCI_DUMP_CLR_BUF_FLAG	(1<<1)
#define CCCI_DUMP_CURR_FLAG		(1<<2)

enum HSAPIF_MD_SYS {
	/* MD SYS name counts from 1,
	 * but internal index counts from 0
	 */
	HSAPIF_MD_SYS1 = 0,
	HSAPIF_MD_SYS2,
	HSAPIF_MD_SYS3,
	HSAPIF_MD_SYS4,
	HSAPIF_MD_SYS5 = 4,
	HSAPIF_MAX_MD_NUM
};


enum {
	HSAPIF_CCCI_DUMP_INIT = 0,
	HSAPIF_CCCI_DUMP_BOOTUP,
	HSAPIF_CCCI_DUMP_NORMAL,
	HSAPIF_CCCI_DUMP_REPEAT,
	HSAPIF_CCCI_DUMP_MEM_DUMP,
	HSAPIF_CCCI_DUMP_HISTORY,
	HSAPIF_CCCI_DUMP_REGISTER,
	HSAPIF_CCCI_DUMP_MAX,
};


extern unsigned int ext_ccci_debug_enable; /* Exported by CCCI core */
extern int ext_ccci_log_write(const char *fmt, ...); /* Exported by CCCI Util */
extern int ext_ccci_dump_write(int md_id, int buf_type,
	unsigned int flag, const char *fmt, ...);
extern int cldma_log_enable;

/*****************************************************************************
 ** CCCI dump log define start ****************
 ****************************************************************************/
/*--------------------------------------------------------------------------*/
/* This is used for log to mobile log or uart log */
#if 1
#define CCCI_LEGACY_DBG_LOG(idx, tag, fmt, args...) \
do { \
	pr_debug("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args); \
} while (0)
#endif

#if 0
#define CCCI_LEGACY_DBG_LOG(idx, tag, fmt, args...) \
do { \
	if (ext_ccci_debug_enable == CCCI_LOG_ALL_MOBILE) \
		pr_debug("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args); \
	else if (ext_ccci_debug_enable == HSAPIF_CCCI_LOG_ALL_UART) \
		pr_info("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args); \
} while (0)
#endif

#define CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, args...) \
do { \
	if (ext_ccci_debug_enable == HSAPIF_CCCI_LOG_ALL_MOBILE \
		|| ext_ccci_debug_enable == HSAPIF_CCCI_LOG_CRITICAL_MOBILE) \
		pr_debug("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args); \
	else if (ext_ccci_debug_enable == HSAPIF_CCCI_LOG_ALL_UART \
			|| ext_ccci_debug_enable == HSAPIF_CCCI_LOG_CRITICAL_UART) \
		pr_info("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args); \
} while (0)

#define CCCI_LEGACY_ERR_LOG(idx, tag, fmt, args...) \
		pr_notice("[cldma-%s-%d-%d/" tag "]" fmt, current->comm, current->pid, current->tgid, ##args)

/*--------------------------------------------------------------------------*/
/* This log is used for driver init and part of first boot up log */
#define CCCI_INIT_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_INIT, \
		CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
		"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for save runtime data */
/* The first line with time stamp */
#define CCCI_BOOTUP_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_BOOTUP, \
		CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_BOOTUP_DUMP_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_BOOTUP, 0, \
		"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for modem boot up log and event */
/* The cldma_log_enable is set by user via sys dev nodes,
 *  to runtime enable/disable CLDMA related logs.
 */
#define CCCI_NORMAL_LOG(idx, tag, fmt, args...) \
do { \
	if ( cldma_log_enable >= 1) { \
		ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_NORMAL, \
			CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
				"[%d]" fmt, (idx+1), ##args); \
		CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
	} \
} while (0)

#define CCCI_NOTICE_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_NORMAL, \
		CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_ALWAYS_LOG(idx, tag, fmt, ##args); \
} while (0)

/* The cldma_log_enable is set by user via sys dev nodes,
 *  to runtime enable/disable CLDMA related logs.
 */
#define CCCI_ERROR_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_REPEAT, \
		CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_DEBUG_LOG(idx, tag, fmt, args...) \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args)

/* This log is used for periodic log */
#define CCCI_REPEAT_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_REPEAT,\
		CCCI_DUMP_CURR_FLAG|CCCI_DUMP_TIME_FLAG, \
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for memory dump */
#define CCCI_MEM_LOG_TAG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_MEM_DUMP, \
		CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG,\
			"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

#define CCCI_MEM_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_MEM_DUMP, 0, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/* This log is used for history dump */
#define CCCI_HISTORY_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_HISTORY, 0, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)
#define CCCI_HISTORY_TAG_LOG(idx, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, HSAPIF_CCCI_DUMP_HISTORY, \
		CCCI_DUMP_TIME_FLAG, fmt, ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)
#define CCCI_BUF_LOG_TAG(idx, buf_type, tag, fmt, args...) \
do { \
	ext_ccci_dump_write(idx, buf_type, \
		CCCI_DUMP_TIME_FLAG|CCCI_DUMP_CURR_FLAG,\
		"[%d]" fmt, (idx+1), ##args); \
	CCCI_LEGACY_DBG_LOG(idx, tag, fmt, ##args); \
} while (0)

/****************************************************************************
 ** CCCI dump log define end ****************
 ****************************************************************************/

/* #define CLDMA_TRACE */
/* #define PORT_NET_TRACE */
#define CCCI_SKB_TRACE
/* #define CCCI_BM_TRACE */

#endif				/* __CCCI_DEBUG_H__ */
