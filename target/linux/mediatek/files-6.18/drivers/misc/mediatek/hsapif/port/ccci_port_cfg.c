// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



//#include "ccci_config.h"
//#include "ccci_hif.h"
//#include "ccci_port_cfg.h"
#include "ccci_debug.h"
//#include "ccif_hif_platform.h"

#include "ccci_port_t.h"
#include "ccci_core.h"
#include "ccci_hif.h"
#include "ccci_msg_data.h"
#include "ccci_port_base.h"
#include "ccci_msg_center.h"
#include "ccci_msg_id.h"
#include "ccci_ioctrl_def.h"

#define TAG "port_cfg"


extern int cldma_util_broadcast_init(void);
extern int create_cldma_files(void);

extern struct port_ops port_char_ops;
extern struct port_ops port_mgr_ops;
extern struct port_ops port_dipc_ops;
extern struct port_ops port_rback_ops;
extern struct port_ops port_timesync_ops;

static struct port_t *s_ports[CCCI_CH_COUNT] = {NULL};

static struct port_t ccci_ports_cldma[] = {

	{CCCI_PORT_MGR_TX, CCCI_PORT_MGR_RX, MGR_TX_Q_NUM, MGR_RX_Q_NUM,
		HIF_ID_CLDMA, 0,
		&port_mgr_ops, 0, "ccci_hsapif_mgr",},

	{CCCI_PORT_RBACK_TX, CCCI_PORT_RBACK_RX, 0, 0,
		HIF_ID_CLDMA, 0,
		&port_rback_ops, 0, "ccci_hsapif_rback",},

	{CCCI_PORT_GNSS_TX, CCCI_PORT_GNSS_RX, 0, 0,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_gnss",},

	{CCCI_PORT_META_TX, CCCI_PORT_META_RX, 1, 1,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_meta",},

	{CCCI_PORT_LOG_TX, CCCI_PORT_LOG_RX, 2, 2,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_log",},

	{CCCI_PORT_ADB_TX, CCCI_PORT_ADB_RX, 3, 3,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_adb",},

	{CCCI_PORT_MINI_TX, CCCI_PORT_MINI_RX, 4, 4,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_mini",},

	{CCCI_PORT_DICP_TX, CCCI_PORT_DICP_RX, MGR_TX_Q_NUM, MGR_RX_Q_NUM,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_dipc_ops, 0, "ccci_hsapif_dipc",},

	{CCCI_PORT_MINIDUMP_TX, CCCI_PORT_MINIDUMP_RX, 5, 5,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_minidump",},

	{CCCI_PORT_GCCCI0_TX, CCCI_PORT_GCCCI0_RX, 6, 6,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_gccci0",},

	{CCCI_PORT_GCCCI1_TX, CCCI_PORT_GCCCI1_RX, 6, 6,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_gccci1",},

	{CCCI_PORT_GCCCI2_TX, CCCI_PORT_GCCCI2_RX, 6, 6,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_gccci2",},

	{CCCI_PORT_TIMESYNC_TX, CCCI_PORT_TIMESYNC_RX, 6, 6,
		HIF_ID_CLDMA, 0,
		&port_timesync_ops, 0, "ccci_hsapif_timesync",},

	{CCCI_PORT_AGNSS_TX, CCCI_PORT_AGNSS_RX, 6, 6,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_agnss",},

	{CCCI_PORT_RFS_TX, CCCI_PORT_RFS_RX, 6, 6,
		HIF_ID_CLDMA, PORT_F_WITH_CHAR_NODE,
		&port_char_ops, 0, "ccci_hsapif_rfs",},
};

static struct pri_port_t ccci_pri_ports_cldma[] = {

	{"ccci_hsapif_rback", CCCI_PORT_RBACK_TX, 1, 0,},

};

static int ccci_port_get_cfg(int hif_id, struct port_t **ports)
{
	int port_number = 0;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] hif_id = %d\n",
		__func__, hif_id);

	*ports = NULL;

	switch (hif_id) {
	case HIF_ID_CLDMA:
		*ports = ccci_ports_cldma;
		port_number = ARRAY_SIZE(ccci_ports_cldma);

		break;

	default:
		port_number = 0;
		break;

	}

	if (!port_number)
		CCCI_ERROR_LOG(
			-1, TAG,
			"[%s] error hif_id: %d\n", __func__, hif_id);

	return port_number;
}

/*
 * cldma private port is for driver use only, e.g. rback.
 */
void ccci_append_pri_port_cfg(char *data, int *data_len)
{
	int i = 0;
	int pri_port_sz = 0;
	struct pri_port_t *pri_port;

	cldma_port_enum_msg_t *port_enum_msg =
			(cldma_port_enum_msg_t *)(data + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
	pri_port_sz = ARRAY_SIZE(ccci_pri_ports_cldma);

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] pri port count: %d; final port count: %d\n",
		__func__,
		pri_port_sz,
		port_enum_msg->port_count + pri_port_sz);

	for (i = 0; i < pri_port_sz; i++) {
		pri_port = ccci_pri_ports_cldma + i;
		port_enum_msg->port_count += 1;
		port_enum_msg->ports[port_enum_msg->port_count-1].ch_id = pri_port->ch_id;
		port_enum_msg->ports[port_enum_msg->port_count-1].en_flag = pri_port->en_flag;
		port_enum_msg->ports[port_enum_msg->port_count-1].reserve = pri_port->reserve;
		*data_len += sizeof(cldma_port_info_t);

		/* print pri port enum content */
		CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] ch_id: %X; en_flag: %d; reserve: %X\n",
				__func__,
				port_enum_msg->ports[port_enum_msg->port_count-1].ch_id,
				port_enum_msg->ports[port_enum_msg->port_count-1].en_flag,
				port_enum_msg->ports[port_enum_msg->port_count-1].reserve);
	}
}

struct port_t *ext_ccci_get_port_from_minor(int minor)
{
	unsigned int ch = minor;
	struct port_t *port = NULL;

	if (ch < CCCI_CH_MAX_NUM)
		port = s_ports[GET_CH_INDEX(ch)];

	if (!port)
		CCCI_ERROR_LOG(-1, TAG,
			"[%s]: error: ch: %d\n", __func__, ch);

	return port;
}

static int ccci_init_ports(struct port_t *ports, int count)
{
	int i;
	struct port_t *port;

	//mapping and init port
	for (i = 0; i < count; i++) {
		port = ports + i;

		/* dipc port cfg initial state */
		/*
		 * Disable all port cfg by default in the beginning phase except minidump and dipc,
		 * afterward dipc user can enable what they want to use.
		 */
		if ((strncmp(port->name, "ccci_hsapif_minidump", sizeof("ccci_hsapif_minidump")) == 0) ||
				(strncmp(port->name, "ccci_hsapif_dipc", sizeof("ccci_hsapif_dipc")) == 0)) {
			port->en_flag = DIPC_PORT_CFG_ENABLE;
		} else {
			port->en_flag = DIPC_PORT_CFG_DISABLE;
		}

		CCCI_INIT_LOG(-1, TAG,
			"[%s] port[%d/%d]: %s; tx_ch: %d; rx_ch: %d, en_flag:%d\n",
			__func__, i, count, port->name, port->tx_ch,
			port->rx_ch, port->en_flag);

		if ((port->tx_ch != 0xFFFF) && (port->rx_ch != 0xFFFF)) {
			s_ports[GET_CH_INDEX(port->tx_ch)] = port;
			s_ports[GET_CH_INDEX(port->rx_ch)] = port;

			if (ccci_msg_register(GET_CH_INDEX(port->rx_ch),
					CCCI_NONE_SUB_ID, port,
					&ext_ccci_data_recv) < 0) {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] msg reg fail port[%s]\n",
					__func__, port->name);
				return -1;
			}
		}

		ext_ccci_port_init(port);
	}

	if (ccci_msg_register(CCCI_DIPC_PORT_CFG_ID,
			CCCI_NONE_SUB_ID, NULL, &ext_ccci_dipc_port_cfg) < 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] dipc config reg error: msg: %d\n",
			__func__, CCCI_DIPC_PORT_CFG_ID);
		return -1;
	}

	return 0;
}

int ext_ccci_all_port_init(
		int           msg_id,
		unsigned int sub_id,
		void         *msg_data,
		void         *my_data)
{
	ccci_init_data_t *init_data = (ccci_init_data_t *)msg_data;
	struct port_t *ports;
	int count;

	int ret = 0;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] hif_ids = %X\n",
		__func__, init_data->hif_ids);

	if (init_data->hif_ids & (1 << HIF_ID_CLDMA)) {
		count = ccci_port_get_cfg(HIF_ID_CLDMA, &ports);
		if (ccci_init_ports(ports, count) < 0)
			return -1;
	}

	//Init CLDMA broacast device node for gettting Host PCIE states
	cldma_util_broadcast_init();

	// Init cldma sys device file for user input purpose at sys/class/misc/cldma
	ret = create_cldma_files();
	if (unlikely(ret != 0)) {
		CCCI_ERROR_LOG(-1, TAG, "[%s] create files failed\n\n", __func__);
		return -1;
	}

	return 0;
}
