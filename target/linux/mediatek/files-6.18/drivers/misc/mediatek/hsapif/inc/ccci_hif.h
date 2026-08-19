// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_HIF_H__
#define __CCCI_HIF_H__

#include <linux/skbuff.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>

#include "ccci_config.h"

typedef enum {
	HIF_ID_CLDMA = 0,
	HIF_ID_CLDMA0_PCIE,
	HIF_ID_CLDMA_PCIE,

	HIF_ID_MAX,
} hif_id_type;



typedef enum {
	HIF_FLAG_CTRL = 0x01, // 1 << HIF_PATH_CTRL
	HIF_FLAG_CTRL_1 = 0x02,// 1 << HIF_PATH_CTRL_1
	HIF_FLAG_DATA = 0x04,
	HIF_FLAG_CTRL_DATA = 0x07,
	HIF_FLAG_EX = 0x08,
	HIF_FLAG_ALL = 0x0F,
	HIF_FLAG_MAX,
} hif_path_flag;


typedef enum {
	HIF_PATH_CTRL = 0,
	HIF_PATH_CTRL_1,
	HIF_PATH_DATA,
	HIF_PATH_EX,
	HIF_PATH_MAX,
} hif_path_type;


/*
 * the actually allocated skb's buffer is much bigger than what we request,
 * so when we judge which pool it belongs, the comparision is quite tricky...
 * another trikcy part is, ccci_fsd use CCCI_MTU as its payload size,
 * but it treat both ccci_header and op_id as header, so the total packet size
 * will be CCCI_MTU+sizeof(ccci_header)+sizeof(unsigned int).
 * check port_char's write() and ccci_fsd for detail.
 *
 * beaware, these macros are also used by CLDMA
 */
/* user MTU+CCCI_H+extra(ex. ccci_fsd's OP_ID), for general packet */
#define SKB_4K (CCCI_MTU+192)
/* net MTU+CCCI_H, for network packet */
#define SKB_1_5K (CCCI_NET_MTU+16)
#define SKB_16 16			/* for struct ccci_header */
#define NET_RX_BUF SKB_4K
#define MAX_GPD_RECV_DATA_LEN 3584

extern struct ccci_md_cd_ctrl *ccci_md_ctrls[HIF_ID_MAX];

extern struct ccci_cldma_hw_ctrl *ccci_hw_ctrls[HIF_ID_MAX];

#endif
