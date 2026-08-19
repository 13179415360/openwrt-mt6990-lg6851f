// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_IOCTRL_DEF_H__
#define __CCCI_IOCTRL_DEF_H__

#include <linux/types.h>


/* ======================================================================= */
/* IOCTL definations */
/* ======================================================================= */

#define CLDMA_IOC_MAGIC 'C'


typedef struct cldma_port_info {
	u32 ch_id:15;
	u32 en_flag:1;  //0: disable; 1: enable;
	u32 reserve:16;

} cldma_port_info_t;


typedef struct cldma_port_enum_msg {
	u32 head_pattern; //head:0X5A5A5A5A
	u32 port_count:16;
	u32 version:16;
	u32 tail_pattern; //tail:0XA5A5A5A5

	cldma_port_info_t ports[0];

} cldma_port_enum_msg_t;



#define CLDMA_IOC_SET_PORT_ENUM_MSG \
	_IOW(CLDMA_IOC_MAGIC, 1, cldma_port_enum_msg_t)  /* dipc */




#endif	/* __CCCI_IOCTRL_DEF_H__ */
