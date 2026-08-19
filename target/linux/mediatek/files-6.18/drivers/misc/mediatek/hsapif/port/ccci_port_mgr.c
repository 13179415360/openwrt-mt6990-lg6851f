// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/cdev.h>

#include "ccci_port_t.h"
#include "ccci_port_base.h"
#include "ccci_debug.h"
#include "ccci_kmem.h"
#include "ccci_core.h"
#include "ccci_comm_config.h"
#include "ccci_fsm_cldma.h"
#include "ccci_msg_id.h"


#define TAG "mgr"


static void handle_dicp_ctrl_msg_ack(
		struct ccci_kmem_data *keme_data,
		struct ccci_header *ccci_h)
{
	if (ccci_h->reserved == 0x00657272)
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] ctrl msg version not match;\n", __func__);

	ext_ccci_data_free(keme_data);
}

int port_mgr_data_recv(
		struct port_t *port,
		void *data)
{
	struct ccci_kmem_data *keme_data = (struct ccci_kmem_data *)data;
	struct ccci_header *ccci_h = (struct ccci_header *)keme_data->off_data;
	int ret = 0;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	struct ctrl_msg_h *ctrl_h = (struct ctrl_msg_h *) ((void*)ccci_h + sizeof(struct ccci_header));

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] ccci_h=0x%p, ccci_channel=%d, ctrl_msg=0x%p, ctrl_msg_id=%d\n",
		__func__, ccci_h, ccci_h->channel, ctrl_h, ctrl_h->ctrl_msg_id);
#endif


#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	if (ctrl_h->ctrl_msg_id == MGR_MSG_ID_PORT_ENUM)
		handle_dicp_ctrl_msg_ack(keme_data, ccci_h);

	else if (ctrl_h->ctrl_msg_id == MGR_MSG_ID_HS1) {
		hsapif_obtain_wakelock();
		ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_RECV_HS1, keme_data);
	}

	else if (ctrl_h->ctrl_msg_id == MGR_MSG_ID_HS3)
		ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_RECV_HS3, keme_data);
#else
	if (ccci_h->data[1] == MGR_MSG_ID_PORT_ENUM)
		handle_dicp_ctrl_msg_ack(keme_data, ccci_h);

	else if (ccci_h->data[1] == MGR_MSG_ID_PHASE1_HS1)
		ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_RECV_HS1, keme_data);

	else if (ccci_h->data[1] == MGR_MSG_ID_PHASE1_HS3)
		ret = ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_RECV_HS3, keme_data);


#endif


	return ret;
}


static int port_mgr_init(struct port_t *port)
{
	int ret = 0;

	port->rx_length_th = MAX_QUEUE_LENGTH;
	//port->flags |= PORT_F_ADJUST_HEADER;

	//if (port->flags & PORT_F_WITH_CHAR_NODE)
	//	ret = ext_ccci_dev_create(port, &port_ctrl_fops);

	CCCI_INIT_LOG(-1, TAG,
		"[%s] ret: %d;\n", __func__, ret);



	return ret;
}

struct port_ops port_mgr_ops = {
	.init = &port_mgr_init,

	.recv_data = &port_mgr_data_recv,

};

