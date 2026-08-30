/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
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
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
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

#include <linux/netdevice.h>

#define EDMA_NUM (2)
#define EDMA_QUEUE_NUM (2)

typedef int (*ppe_usb_tx_func)(struct net_device *net, char *buff, int len,
			       int dma_id, int queue_id);

typedef void (*ppe_usb_rx_done_func)(void *context);

struct mtk_hnat_fast {
	struct mtk_fe_token *fe_token;
	struct mtk_fe_dma *edma0;
	struct mtk_fe_dma *edma1;

	ppe_usb_tx_func usb_tx;
	ppe_usb_rx_done_func usb_rx_done;

	bool in_suspend[EDMA_NUM][EDMA_QUEUE_NUM];
	bool init_done;
};

void ppe_fe_dma_init(void);
void ppe_fe_dma_deinit(void);
void ppe_fe_dma_probe(void);
void ppe_fe_dma_cleanup(void);
void ppe_fe_dma_pm_resume(void);
void ppe_fe_dma_pm_suspend(void);
int ppe_usb_set_cb(ppe_usb_tx_func tx_send, ppe_usb_rx_done_func rx_done);
void ppe_usb_tx_done(char *buff);
int ppe_usb_rx_send(struct net_device *net, void *context, char *buff, int len);
int ppe_hitbind_force_to_rndis(struct sk_buff *skb);
int ppe_usb_tx_link_num(void);
void ppe_usb_resume_queue(struct net_device *net, int dma_id, int ring_no);
void ppe_usb_suspend_queue(struct net_device *net, int dma_id, int ring_no);
void ppe_hnat_fast_dump(void);
bool ppe_hnat_fast_init_done(void);
uint32_t ppe_hnat_fast_send_skb(struct sk_buff *skb);
