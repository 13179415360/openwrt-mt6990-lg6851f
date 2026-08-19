/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef __PORT_NET_H__
#define __PORT_NET_H__
#include "ccmni.h"
#include "port_t.h"
extern struct ccmni_dev_ops ccmni_ops;

int ccci_get_ccmni_channel(int md_id, int ccmni_idx,
			   struct ccmni_ch *channel);

extern int mbim_start_xmit(struct sk_buff *skb, int ifid);
#endif /*__PORT_NET_H__*/
