// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/timer.h>


#include "ccci_debug.h"
#include "ccci_fsm_cldma.h"
#include "ccci_config.h"

#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
/*Let mtk_ccci_common.h in the front of ccci_comm_config.h to fix ccci_header re-definition*/
#include <mt-plat/mtk_ccci_common.h>
#endif

#include "ccci_comm_config.h"
#include "ccci_msg_center.h"
#include "ccci_core.h"
#include "ccci_state_mgr.h"
#include "ccci_port_base.h"

#include "mt-plat/mtk_mhccif.h"


#define TAG "fsm"
#define HS_M3_MONITOR_TIMEOUT_VAL 40
#define AP_TEST_REQUEST_FEATURE_ID 15

typedef struct ccci_fsm_cldma_cmd {
	int    cmd_id;
	void  *cmd_data;

	struct ccci_fsm_cldma_cmd *next;

} ccci_fsm_cldma_cmd_t;

struct fsm_cmd_asyn_head {
	int count;

	ccci_fsm_cldma_cmd_t *first;
	ccci_fsm_cldma_cmd_t *last;

};

static struct fsm_cmd_asyn_head fsm_cmd_head;
static spinlock_t fsm_cmd_lock;
static wait_queue_head_t fsm_cmd_wq;
static struct task_struct *fsm_cmd_thread;
static struct ccci_kmem_data *postpone_processing_hs1_msg;
static struct timer_list hs_h3_moniter_timer;

#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
static unsigned int postpone_pending_remote_wakeup = 0;
#endif

#define HS2_AP_DATA_LEN 4000
static char ap_data[HS2_AP_DATA_LEN];

extern int port_dipc_get_ctrl_msg_data(char **data, int *data_len);

static int send_dipc_ctrl_msg(void)
{
	char *port_enum_msg_datas;
	int port_enum_msg_len;
	ccci_send_data_t send_data;
	struct ccci_header *ccci_h;
	int ret;
#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	struct ctrl_msg_h *ctrl_h;
#endif
	if (port_dipc_get_ctrl_msg_data(&port_enum_msg_datas, &port_enum_msg_len) == 0) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: dipc data not ready!\n",
			__func__);

		return -1;
	}

	ccci_h = (struct ccci_header *)port_enum_msg_datas;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	ctrl_h = (struct ctrl_msg_h *)((void*)ccci_h + sizeof(struct ccci_header));
	ccci_h->data[0] = 0;
	ccci_h->data[1] = port_enum_msg_len;
	ccci_h->channel = CCCI_PORT_MGR_TX;
	ccci_h->assert_bit = 0;
	ccci_h->reserved = 0;
	ctrl_h->ctrl_msg_id = MGR_MSG_ID_PORT_ENUM;
	ctrl_h->reserved = 0;
	ctrl_h->data_length =
		(port_enum_msg_len - sizeof(struct ccci_header) - sizeof(struct ctrl_msg_h));
#else
	ccci_h->data[0] = 0;
	ccci_h->data[1] = MGR_MSG_ID_PORT_ENUM;
	ccci_h->channel = CCCI_PORT_MGR_TX;
	ccci_h->assert_bit = 0;
	ccci_h->reserved = 0;
#endif

	send_data.data = ext_ccci_data_alloc(GFP_KERNEL, port_enum_msg_len + CCCI_ADDRESS_ALIGN_LEN);
	if (!send_data.data)
		return -ENOMEM;

	memcpy(send_data.data->off_data, port_enum_msg_datas, port_enum_msg_len);
	send_data.data->data_len = port_enum_msg_len;
	send_data.hif_id = HIF_ID_CLDMA;
	send_data.qno = MGR_TX_Q_NUM;
	send_data.blocking = 1;

	ret = ccci_msg_send_to_one(send_data.hif_id + CCCI_CLDMA_BASE_ID,
			1 << send_data.hif_id, &send_data);

	if (ret < 0) {
		ext_ccci_data_free(send_data.data);

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: %d\n",
			__func__, ret);
	}

	return ret;
}

static int fsm_set_port_config_runtime_data(
		struct cldma_feature_support *feature_support,
		cldma_runtime_feature_t **ap_runtime)
{
	cldma_runtime_feature_t *runtime = *ap_runtime;
	char *port_enum_msg_datas;
	int port_enum_msg_len;

	runtime->feature_id = CLDMA_PORT_CONFIG;
	runtime->support_info = *feature_support;
	//Step1: move ap_runtime ptr (+sizeof(cldma_runtime_feature_t))
	(*ap_runtime) = (void *)(*ap_runtime) + sizeof(cldma_runtime_feature_t);
	if (port_dipc_get_ctrl_msg_data(&port_enum_msg_datas, &port_enum_msg_len) == 0) {
		CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] dipc data not ready!\n",
			__func__);
		return -1;
	}

	// Support cldma driver private port config.
#ifdef CONFIG_MTK_HSAPIF_PRI_PORT
	ccci_append_pri_port_cfg(port_enum_msg_datas, &port_enum_msg_len);
#endif

	//Note: port_enum_msg_len
	/*(= 16 bytes ccci_header + 12 bytes ctrl_msg_h + 12 bytes port_enum_msg + 4 bytes port_info_t * port_num)*/
	/*(= 40 bytes + 4 bytes port_info_t * port_num)*/
	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] port_enum_msg_len: %d\n",
		__func__, port_enum_msg_len);

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	port_enum_msg_datas += (sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
	port_enum_msg_len -= (sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
#else
	port_enum_msg_datas += sizeof(struct ccci_header);
	port_enum_msg_len -= sizeof(struct ccci_header);
#endif

	memcpy((void *)runtime + sizeof(cldma_runtime_feature_t),
			port_enum_msg_datas, port_enum_msg_len);

	runtime->data_len = port_enum_msg_len;
	//Step2: move ap_runtime ptr (+runtime->data_len)
	(*ap_runtime) = (void *)(*ap_runtime) + runtime->data_len;

	return 0;
}


static void fsm_set_non_supported_runtime_data(
		u8 feature_id,
		struct cldma_feature_support *feature_support,
		cldma_runtime_feature_t **ap_runtime)
{
	cldma_runtime_feature_t *runtime = *ap_runtime;
	runtime->feature_id = feature_id;
	runtime->support_info = *feature_support;
	runtime->support_info.support_mask = CCCI_FEATURE_NOT_EXIST; // or CCCI_FEATURE_NOT_SUPPORTED

	(*ap_runtime) = (void *)(*ap_runtime) + sizeof(cldma_runtime_feature_t);

	runtime->data_len = 0; //i.e., no any runtime data (size=0) due to not support this feature

	(*ap_runtime) = (void *)(*ap_runtime) + runtime->data_len;
}

static void fsm_hs_h3_monitor_func(struct timer_list *t)
{
	CLDMA_STATE_T cldma_state;
	cldma_state = ccci_cldma_state_get();

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s]  timeout! current cldma_state: %d\n",
		__func__, cldma_state);
	hsapif_release_wakelock();

	if (cldma_state == CLA_WAITING_HS3) {
		//It means that timeout but still not recv HS from Host!- Trigger assertion
#ifdef __CLDMA_UT__
		CCCI_BOOTUP_LOG(-1, TAG,
			"[%s]  UT Wait H3 timeout! back to cldma_state: %d\n",
			__func__, CLA_READY);
		ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_READY, NULL);
#endif
	}
}

static void dump_H2_message (void* ap_ccci_h)
{

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	/*Dump HS content for debugging purpose */
	//Step1: dump CCCI header
	//Step2: dump CTRL msg header
	//Step3: dump sAP feature query
	//Step4: dump runtime data

	struct ccci_header * h2_ccci_h;
	struct ctrl_msg_h * h2_ctrl_h;
	cldma_query_feature_t* query_feature;
	cldma_runtime_feature_t* runtime_data;
	int hs2_i = 0;
	int DUMP_TO_AP_FEATURE_INX = 2;

	h2_ccci_h = (struct ccci_header *)ap_ccci_h;
	h2_ctrl_h = (struct ctrl_msg_h *)((void*)h2_ccci_h + sizeof(struct ccci_header));
	query_feature = (cldma_query_feature_t*)((void*)h2_ctrl_h + sizeof(struct ctrl_msg_h));
	runtime_data = (cldma_runtime_feature_t*)((void*)query_feature + sizeof(cldma_query_feature_t));

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s]  dump H2 ccci header=0x%p len=%d, ch=%d\n",
		__func__, h2_ccci_h, h2_ccci_h->data[1], h2_ccci_h->channel);

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s]  dump H2 Ctrl=0x%p msg_id=%d, len=%d\n",
		__func__, h2_ctrl_h, h2_ctrl_h->ctrl_msg_id, h2_ctrl_h->data_length);

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] dump H2 feature query=0x%p: head_pattern=0x%x, tail_pattern=0x%x\n",
		__func__, query_feature, query_feature->head_pattern, query_feature->tail_pattern);

	for (hs2_i=0; hs2_i < CLDMA_FEATURE_COUNT; hs2_i++) {
		if (hs2_i < DUMP_TO_AP_FEATURE_INX) {
			CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] dump H2 feature query: id=%d, mask=%d, version=%d\n",
				__func__, hs2_i, query_feature->feature_set[hs2_i].support_mask,
				query_feature->feature_set[hs2_i].version);
		}
	}

	for (hs2_i=0; hs2_i < CLDMA_FEATURE_COUNT; hs2_i++) {
		if (hs2_i < DUMP_TO_AP_FEATURE_INX) {
			CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] dump H2 runtime data=0x%p: id=%d, mask=%d, version=%d, data_len=%d\n",
				__func__, runtime_data, runtime_data->feature_id,
				runtime_data->support_info.support_mask,
				runtime_data->support_info.version, runtime_data->data_len);
		}
		runtime_data = (void*)runtime_data +
			(sizeof(cldma_runtime_feature_t) + runtime_data->data_len);
	}

#endif

}

static void dump_H3_message(void* host_ccci_h)
{
	struct ccci_header * h3_ccci_h;
	struct ctrl_msg_h * h3_ctrl_h;
	cldma_runtime_feature_t* runtime_data;
	int h3_i = 0;
	int accumulated_len = 0;
	int DUMP_TO_AP_FEATURE_INX = 2;

	h3_ccci_h = (struct ccci_header *)host_ccci_h;
	h3_ctrl_h = (struct ctrl_msg_h *)((void*)h3_ccci_h + sizeof(struct ccci_header));
	runtime_data = (cldma_runtime_feature_t*)((void*)h3_ctrl_h + sizeof(struct ctrl_msg_h));

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s]  dump H3 ccci header=0x%p, len=%d, ch=%d\n",
		__func__, h3_ccci_h, h3_ccci_h->data[1], h3_ccci_h->channel);

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s]  dump H3 Ctrl=0x%p msg_id=%d, len=%d\n",
		__func__, h3_ctrl_h, h3_ctrl_h->ctrl_msg_id, h3_ctrl_h->data_length);

	//Check if the response is recevided for sAP requested feature id
	for (h3_i=0; h3_i < CLDMA_FEATURE_COUNT; h3_i++) {
		if (h3_i < DUMP_TO_AP_FEATURE_INX) {
			CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] Recv HS3=0x%p:feature_id=%d,mask=%d, version=%d, data_len=%d\n",
				__func__, runtime_data, runtime_data->feature_id,
				runtime_data->support_info.support_mask,
				runtime_data->support_info.version, runtime_data->data_len);
		}
		//Continue to next one runtime_data item
		accumulated_len += (sizeof(cldma_runtime_feature_t) + runtime_data->data_len);
		runtime_data = (cldma_runtime_feature_t*)
			((void*)runtime_data + (sizeof(cldma_runtime_feature_t) + runtime_data->data_len));

	}

}


static int fsm_send_runtime_data_for_HS2(struct ccci_header *ap_ch, u32 len)
{
	ccci_send_data_t send_data;
	int ret = 0;
#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	struct ctrl_msg_h* ctrl_h = (void*)ap_ch + sizeof(struct ccci_header);
#endif

	ap_ch->data[0] = 0;
	ap_ch->data[1] = len;
	ap_ch->channel = CCCI_PORT_MGR_TX;
	ap_ch->assert_bit = 0;
	ap_ch->reserved = 0;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	ctrl_h->ctrl_msg_id = MGR_MSG_ID_HS2;
	ctrl_h->reserved = 0;
	ctrl_h->data_length = (len - sizeof(struct ccci_header) - sizeof(struct ctrl_msg_h));
#endif


	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] len: %d\n",
		__func__, len);

	send_data.data = ext_ccci_data_alloc(GFP_KERNEL, len + CCCI_ADDRESS_ALIGN_LEN);
	if (!send_data.data)
		return -ENOMEM;

	memcpy(send_data.data->off_data, ap_ch, len);
	send_data.data->data_len = len;
	send_data.hif_id = HIF_ID_CLDMA;
	send_data.qno = MGR_TX_Q_NUM;
	send_data.blocking = 1;

	//for debug purpose: dump H2 message
	dump_H2_message(send_data.data->off_data);

	ret = ccci_msg_send_to_one(send_data.hif_id + CCCI_CLDMA_BASE_ID,
			1 << send_data.hif_id, &send_data);

	if (ret < 0) {
		ext_ccci_data_free(send_data.data);

		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: %d\n",
			__func__, ret);
		return ret;
	}

	ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_WAITING_HS3, NULL);
	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] current cldma_state: %d\n",
		__func__, ccci_cldma_state_get());

	//Start HS_H3 monitor timer to wait for H3 sent back from Host side
	mod_timer(&hs_h3_moniter_timer, jiffies + HS_M3_MONITOR_TIMEOUT_VAL*HZ);
	return ret;
}

static void configure_ap_feature_query(cldma_query_feature_t *feature_p)
{
	int test_feature_id = AP_TEST_REQUEST_FEATURE_ID;
	feature_p->feature_set[test_feature_id].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	feature_p->feature_set[test_feature_id].version= 0;
}


/* Two cmd state will invoke this API: process_hs1_msg() */
/* C#1: In CLA_WAITING_HS1: Recv HS1 msg (to check if Port Enum info is available or not) */
/* C#2: In CLA_WAITING_PORT_ENUM_SEND_HS2: Once DIPCd provides Port Enum info., process H1 to send HS2 */
static int process_hs1_msg(struct ccci_kmem_data *msg_data)
{
	struct ccci_kmem_data *keme_data = msg_data;
	cldma_query_feature_t *host_feature;
	//char ap_data[HS2_AP_DATA_LEN];
	struct ccci_header *ap_ch = (struct ccci_header *)ap_data;
	int ret = 0;
	u8 i = 0;
	int DUMP_TO_AP_FEATURE_INX = 2;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	struct ctrl_msg_h *ap_ctrl_msg =
		(struct ctrl_msg_h *)((void *)ap_ch + sizeof(struct ccci_header));
	cldma_query_feature_t *ap_feature =
		(cldma_query_feature_t *)((void *)ap_ctrl_msg + sizeof(struct ctrl_msg_h));
#else
	cldma_query_feature_t *ap_feature =
		(cldma_query_feature_t *)((void *)ap_ch + sizeof(struct ccci_header));
#endif

	cldma_runtime_feature_t *ap_runtime =
		(cldma_runtime_feature_t *)((void *)ap_feature + sizeof(cldma_query_feature_t));

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	host_feature =
		(cldma_query_feature_t *)(keme_data->off_data + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
#else
	host_feature =
		(cldma_query_feature_t *)(keme_data->off_data + sizeof(struct ccci_header));
#endif

	//HS1 message =
	/*(16 bytes [ccci_header] + 12 bytes [ctrl_msg_h] + 72 bytes [query_feature(4+1*64+4)] = 100 bytes)*/
	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] HS1 data_len: %d\n",
		__func__, keme_data->data_len);

	/* set ap feature query data:*/
	/* "PartC Feature query" -Memset all zero means that: each feature's support_mask is 0 (i.e., CCCI_FEATURE_NOT_EXIST) */
	memset(ap_data, 0, HS2_AP_DATA_LEN);
	ap_feature->head_pattern = AP_FEATURE_QUERY_PATTERN;
	ap_feature->tail_pattern = AP_FEATURE_QUERY_PATTERN;
	/* set feature query which sAP wants to ask Host: e.g., ap_feature->feature_set[i].support_mask */
	configure_ap_feature_query(ap_feature);

	/* check host feature if available */
	if (host_feature->head_pattern != HOST_FEATURE_QUERY_PATTERN ||
		host_feature->tail_pattern != HOST_FEATURE_QUERY_PATTERN) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: host query feature pattern is wrong: head: 0x%X, tail: 0x%X\n",
			__func__, host_feature->head_pattern, host_feature->tail_pattern);

		goto is_fail;
	}

	/* "PartD Runtime Data for each feature id": set response for host queried feature runtime data */
	for (i=0; i<CLDMA_FEATURE_COUNT; i++) {
		if (i < DUMP_TO_AP_FEATURE_INX) {
			CCCI_BOOTUP_LOG(-1, TAG,
				"[%s] feature_id=%d, mask=%d\n",
				__func__, i, host_feature->feature_set[i].support_mask);
		}
		if (host_feature->feature_set[i].support_mask == CCCI_FEATURE_MUST_SUPPORT) {
			if (i == CLDMA_PORT_CONFIG) {
				//Currently, sAP only supports feature_id=1 (i.e., CLDMA_PORT_CONFIG)
				ret = fsm_set_port_config_runtime_data(&host_feature->feature_set[i], &ap_runtime);
			} else {
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: Host wants sAP support but sAP no existed/supported feature: %d\n",
					__func__, i);
				fsm_set_non_supported_runtime_data(i, &host_feature->feature_set[i], &ap_runtime);
			}
		} else {
			//For other non "CCCI_FEATURE_MUST_SUPPORT" feature id queried by Host, repond with NON_EXISTED
			fsm_set_non_supported_runtime_data(i, &host_feature->feature_set[i], &ap_runtime);
		}
	}

	if (ret == 0){
		/* In this case, it means that get Port Enum info successfully: */
		/* "PartA CCCI_HEADER" & "PartB CTRL_MSG_HEADER" will be configured in this function */
		fsm_send_runtime_data_for_HS2(ap_ch, (u32)((char *)ap_runtime - (char *)ap_ch));

		ext_ccci_data_free(keme_data);
	} else {
		/* Fail to get DIPC Port Enum Config info: 1) Can't send HS2 here 2) Keep HS1 msg for later processing */
		postpone_processing_hs1_msg = keme_data;
		ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_WAITING_PORT_ENUM_SEND_HS2, NULL);
		CCCI_BOOTUP_LOG(-1, TAG, "[%s] current cldma_state: %d\n",
			__func__, ccci_cldma_state_get());
	}

	return 0;

is_fail:
	ext_ccci_data_free(keme_data);
	return -1;
}

#ifdef __CLDMA_UT__
static void ut_receive_hs1_msg (void)
{
	int size =
		sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h) + sizeof(cldma_query_feature_t);
	struct ccci_kmem_data *keme_data =
		ext_ccci_data_alloc(GFP_KERNEL, size + CCCI_ADDRESS_ALIGN_LEN);
	cldma_query_feature_t * host_feature;

#ifdef __SUPPORT_CCCI_CTRL_MSG_HEADER__
	host_feature = (cldma_query_feature_t *)
		(keme_data->off_data + sizeof(struct ccci_header) + sizeof(struct ctrl_msg_h));
#else
	host_feature = (cldma_query_feature_t *)(keme_data->off_data + sizeof(struct ccci_header));
#endif

	keme_data->data_len = size;
	memset(host_feature, 0, sizeof(cldma_query_feature_t));
	host_feature->head_pattern = HOST_FEATURE_QUERY_PATTERN;
	host_feature->tail_pattern = HOST_FEATURE_QUERY_PATTERN;
	//ID=0 is for MD
	host_feature->feature_set[MD_PORT_CONFIG].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	host_feature->feature_set[MD_PORT_CONFIG].version = 1;
	//ID=1 is for sAP
	host_feature->feature_set[CLDMA_PORT_CONFIG].support_mask = CCCI_FEATURE_MUST_SUPPORT;
	host_feature->feature_set[CLDMA_PORT_CONFIG].version = 1;
	process_hs1_msg(keme_data);

}
#endif

static int fsm_cmd_handle_hs1_msg(ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	struct ccci_kmem_data *keme_data = (struct ccci_kmem_data *)fsm_cmd->cmd_data;
	int ret = 0;
	ret = process_hs1_msg(keme_data);
	return ret;
}

static int fsm_cmd_handle_hs3_msg(ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	struct ccci_kmem_data *keme_data = (struct ccci_kmem_data *)fsm_cmd->cmd_data;
	struct ccci_header *ccci_h = (struct ccci_header *)keme_data->off_data;
	struct ctrl_msg_h *ctrl_h  = (struct ctrl_msg_h *)((void*)ccci_h + sizeof(struct ccci_header));
	cldma_runtime_feature_t* runtime_data =
		(cldma_runtime_feature_t*)((void*)ctrl_h + sizeof(struct ctrl_msg_h));
	int h3_i = 0;
	int accumulated_len = 0;

	dump_H3_message(ccci_h);

	//Check if the response is recevided for sAP requested feature id
	for (h3_i=0; h3_i < CLDMA_FEATURE_COUNT; h3_i++) {

		if (runtime_data->feature_id == AP_TEST_REQUEST_FEATURE_ID) {
			if (runtime_data->support_info.support_mask == CCCI_FEATURE_MUST_SUPPORT) {
				//Recv feature response from Host
				CCCI_BOOTUP_LOG(-1, TAG,
					"[%s] Recv HS3: support feature_id=%d\n", __func__, runtime_data->feature_id);

			} else if ((runtime_data->support_info.support_mask == CCCI_FEATURE_NOT_EXIST)
						|| (runtime_data->support_info.support_mask == CCCI_FEATURE_NOT_SUPPORT)) {
				//Something wrong and fail to get response of sAP's requested feature id!
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] Recv HS3: not support feature_id=%d\n",
					__func__, runtime_data->feature_id);
			} else {
				CCCI_BOOTUP_LOG(-1, TAG,
					"[%s] Recv HS3: partial support feature_id=%d, with mask=%d\n",
					__func__, runtime_data->feature_id, runtime_data->support_info.support_mask);
			}

		}

		//Continue to next one runtime_data item
		accumulated_len += (sizeof(cldma_runtime_feature_t) + runtime_data->data_len);
		runtime_data = (cldma_runtime_feature_t*)
			((void*)runtime_data + (sizeof(cldma_runtime_feature_t) + runtime_data->data_len));

	}

	if (ctrl_h->data_length != accumulated_len) {
		CCCI_ERROR_LOG(-1, TAG,
				"[%s] Recv HS3: ctrl_datalen=%d, accumulated_len=%d\n",
				__func__, ctrl_h->data_length, accumulated_len);
	}

	ext_ccci_data_free(keme_data);

	if (timer_pending(&hs_h3_moniter_timer) == 1) {
		CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] Recv HS3, stop timer\n", __func__);
		del_timer(&hs_h3_moniter_timer);
	}
	ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_READY, NULL);
	CCCI_BOOTUP_LOG(-1, TAG, "[%s] current cldma_state: %d \n",
			__func__, ccci_cldma_state_get());
	hsapif_release_wakelock();

	return 0;
}

static int fsm_cmd_call_mhccif_irq(ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	int ret;
#if (defined(__SUPPORT_ASYNC_HS_FLOW__) && (!defined(__CLDMA_UT__)))

#ifdef CONFIG_MTK_MHCCIF
	int check_val = mtk_mhccif_irq_to_host(MHCCIF_D2H_INT_ASYNC);
	ret = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_WAITING_HS1, NULL);
	CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] D2H INT via MHCCIF, ret_val=%d, current cldma_state: %d\n",
			__func__, check_val, ccci_cldma_state_get());
#else
	CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] Enable HS but not build MHCCIF!, set cldma_state to %d\n",
			__func__, CLA_READY);
	ret = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_READY, NULL);
#endif /*End of CONFIG_MTK_MHCCIF*/

#else
	CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] Set cldma_state to %d\n", __func__, CLA_READY);
	ret = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID, CLA_READY, NULL);
#endif
	return ret;
}

static int fsm_cmd_send_hs2_or_ctrl_msg(ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	CLDMA_STATE_T cldma_state;

	cldma_state = ccci_cldma_state_get();

	CCCI_BOOTUP_LOG(-1, TAG,
		"[%s] current cldma_state: %d \n",
		__func__, cldma_state);

#ifndef __CLDMA_UT__
	if (cldma_state == CLA_READY) {
		//need to send the ctrol msg now
		return send_dipc_ctrl_msg();

	} else if (cldma_state == CLA_WAITING_PORT_ENUM_SEND_HS2) {
		/*Send HS2 msg back to Host*/
		/*In this state, it means that CLDMA driver already handled HS1 but no Port Enum info at that time*/
		/*Once getting Port Enum info. from DIPCD, it's time to process "kept HS1 msg + Port Enum" to send HS*/
		/*ccci_fsm_cldma_add_cmd(CCCI_FSM_CMD_RECV_HS1,fsm_hs1_msg);*/
		if (postpone_processing_hs1_msg != NULL){
			process_hs1_msg(postpone_processing_hs1_msg);
			postpone_processing_hs1_msg = NULL;
		} else {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] postpone_processing_hs1_msg is NULL fail to HS2.\n",
				__func__);
		}

	} else if (cldma_state == CLA_WAITING_HS3) {
		//No need to send HS2/Ctrl message due to Port Enum info is already sent to Host in HS2 msg
		CCCI_BOOTUP_LOG(-1, TAG,
			"[%s] no need to send the HS2 or CTRL msg in CLA_WAITING_HS3 state.\n",
			__func__);
	}

	return -1;
#else
	ut_receive_hs1_msg();
	return 0;
#endif

}


static int fsm_cmd_handle_host_pm_suspend_req
	(ccci_fsm_cldma_cmd_t *fsm_cmd) {

	int ret_1, ret_2;
	/*Change state to block userspace's app to continue to TX*/
	ret_1 = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
		CLA_RECV_HOST_SUSPEND_REQ,
		NULL);

	/*Broadcast state to userspace's app*/
	inject_host_pcie_status_event(0, HOST_STA_EV_SUSPEND_REQ, "PM_SR");

	/*Start monitor HW TXQ mechanism*/
	ret_2 = ccci_msg_send_to_one(CCCI_CLDMA_MONITOR_HW_TXQ_ID,
		CCCI_NONE_SUB_ID,
		NULL);

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] change cldma_state to %d (ret=%d), request_schedule_ret=%d\n",
		__func__, ccci_cldma_state_get(), ret_1, ret_2);
	return ret_2;
}

static int fsm_cmd_prepare_host_pm_suspend_ack
	(ccci_fsm_cldma_cmd_t *fsm_cmd) {

	int ret_1, ret_2;
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
	int ret_3, err;
	/*msg_cmd format of exec_ccci_kern_func_by_md_id():*/
	/*Byte [0] -> cmd : 1 -> wake, others -> reserve*/
	/*Byte [1] -> reserve*/
	/*Byte [2] -> reserve*/
	/*Byte [3] -> reserve*/
	unsigned int msg_cmd = 1;
#endif
	/*Send PCIE_SUSPEND_ACK back to Host*/
	ret_1 = mtk_mhccif_irq_to_host(MHCCIF_D2H_INT_PCIE_PM_SUSPEND_ACK_AP);

	/*Change state to allow userspace's app to request Remote Wakeup command*/
	ret_2 = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
		CLA_SEND_HOST_SUSPEND_ACK,
		NULL);

	/*Broadcast state to userspace's app*/
	inject_host_pcie_status_event(0, HOST_STA_EV_SUSPEND_ACK, "PM_SA");
/*PCIe will support to provide IOCTL command to execute remote wakeup command instead of CLDMA driver*/
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
#ifdef __SIMULATE_HOST_PM_PROCEDURE__
	postpone_pending_remote_wakeup = SYSMSGSV_PCIE_PM_NOTIFY;
#endif

	/*Check if pending remote_wakeup_command is waiting to execute*/
	if (postpone_pending_remote_wakeup == SYSMSGSV_PCIE_PM_NOTIFY) {
		/*Send remote_wakeup_command here via CCCI Ctrl Path*/
		/*Use this API & its parameters - need mtk_ccci_common.h*/
		err = exec_ccci_kern_func_by_md_id(MD_SYS1, SYSMSGSV_PCIE_PM_NOTIFY,
			(char *)&msg_cmd, sizeof(unsigned int));

		CCCI_NORMAL_LOG(-1, TAG,
			"[%s]exec_ccci_kern_func_by_md_id() err=%d\n", __func__,err);

		postpone_pending_remote_wakeup = 0;

		ret_3 = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
			CLA_SEND_HOST_REMOTE_WAKEUP,
			NULL);
	}
#endif
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] send ack ret=%d, change cldma_state to %d (ret_2=%d, ret_3=%d)\n",
		__func__, ret_1, ccci_cldma_state_get(), ret_2, ret_3);
#else
	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] send ack ret=%d, change cldma_state to %d (ret=%d)\n",
		__func__, ret_1, ccci_cldma_state_get(), ret_2);

#endif


	return ret_2;
}

static int fsm_cmd_handle_host_pm_resume_req
	(ccci_fsm_cldma_cmd_t *fsm_cmd) {

	int ret_1, ret_2, ret_3;

	/*Change state to allow userspace's app to request Remote Wakeup command*/
	ret_1 = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
		CLA_RECV_HOST_RESUME_REQ,
		NULL);

	/*Send PCIE_RESUME_ACK back to Host*/
	ret_2 = mtk_mhccif_irq_to_host(MHCCIF_D2H_INT_PCIE_PM_RESUME_ACK_AP);

	/*Change to CLA_READY to allow userspace's app to continue TX again*/
	ret_3 = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
		CLA_READY,
		NULL);

	/*Broadcast state to userspace's app*/
	CCCI_NORMAL_LOG(-1, TAG,"[%s] inject_host_pcie_status_event \n",__func__);
	inject_host_pcie_status_event(0, HOST_STA_EV_READY, "PM_RR");

	/*Acquire wake lock for resume ack*/
	cldma_bc_wakeup_event();

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] send ack ret=%d, change cldma_state to %d (ret_1=%d, ret_3=%d)\n",
		__func__, ret_2, ccci_cldma_state_get(), ret_1, ret_3);
	return ret_2;
}

#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
static int fsm_cmd_handle_send_remote_wakeup
	(ccci_fsm_cldma_cmd_t *fsm_cmd) {

	int err=0, ret=0;
	/*msg_cmd format of exec_ccci_kern_func_by_md_id():*/
	/*Byte [0] -> cmd : 1 -> wake, others -> reserve*/
	/*Byte [1] -> reserve*/
	/*Byte [2] -> reserve*/
	/*Byte [3] -> reserve*/
	unsigned int msg_cmd = 1;

	CLDMA_STATE_T cldma_state;
	cldma_state = ccci_cldma_state_get();

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] cldma_state: %d, pending_rwp_cmd=%d\n",
		__func__, cldma_state, postpone_pending_remote_wakeup);

	if (cldma_state == CLA_RECV_HOST_SUSPEND_REQ) {
		/*Record it to pending_remote_wakeup_command*/
		postpone_pending_remote_wakeup = SYSMSGSV_PCIE_PM_NOTIFY;

	} else if (cldma_state == CLA_SEND_HOST_SUSPEND_ACK) {
		if (postpone_pending_remote_wakeup == 0) {
			err = exec_ccci_kern_func_by_md_id(MD_SYS1, SYSMSGSV_PCIE_PM_NOTIFY,
				(char *)&msg_cmd, sizeof(unsigned int));

			CCCI_NORMAL_LOG(-1, TAG,
				"[%s]exec_ccci_kern_func_by_md_id() err=%d\n", __func__,err);

			ret = ccci_msg_send(CCCI_CLDMA_STATE_SET_ID,
				CLA_SEND_HOST_REMOTE_WAKEUP,
				NULL);
			CCCI_NORMAL_LOG(-1, TAG,
				"[%s] change cldma_state to %d (ret=%d)\n",
				__func__, ccci_cldma_state_get(), ret);
		}

	} else if (cldma_state == CLA_SEND_HOST_REMOTE_WAKEUP) {
		/*Already sent remote_wakeup_command, do nothing here!*/
	} else
		ret = -1;
	return ret;

}
#endif

static inline int fsm_cmd_handle(
		ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	int ret = 0;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] process cmd id: %d\n",
		__func__, fsm_cmd->cmd_id);

	switch (fsm_cmd->cmd_id) {
	case CCCI_FSM_CMD_CALL_MHCCIF_IRQ:
		ret = fsm_cmd_call_mhccif_irq(fsm_cmd);
		break;

	case CCCI_FSM_CMD_SEND_HS2_OR_CTRL_MSG:
		ret = fsm_cmd_send_hs2_or_ctrl_msg(fsm_cmd);
		break;

	case CCCI_FSM_CMD_RECV_HS1:
		ret = fsm_cmd_handle_hs1_msg(fsm_cmd);
		break;

	case CCCI_FSM_CMD_RECV_HS3:
		ret = fsm_cmd_handle_hs3_msg(fsm_cmd);
		break;

	case CCCI_FSM_CMD_RECV_HOST_PM_SUSPEND_REQ:
		ret = fsm_cmd_handle_host_pm_suspend_req(fsm_cmd);
		break;

	case CCCI_FSM_CMD_SEND_HOST_PM_SUSPEND_ACK:
		ret = fsm_cmd_prepare_host_pm_suspend_ack(fsm_cmd);
		break;

	case CCCI_FSM_CMD_RECV_HOST_PM_RESUME_REQ:
		ret = fsm_cmd_handle_host_pm_resume_req(fsm_cmd);
		break;
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
	case CCCI_FSM_CMD_SEND_HOST_REMOTE_WAKEUP:
		ret = fsm_cmd_handle_send_remote_wakeup(fsm_cmd);
		break;
#endif
	default:
		break;
	}

	return ret;
}

static inline int fsm_cmd_count(void)
{
	int count = 0;
	unsigned long irqflags;

	if (in_interrupt()) {
		spin_lock(&fsm_cmd_lock);
		count = fsm_cmd_head.count;
		spin_unlock(&fsm_cmd_lock);

	} else {
		spin_lock_irqsave(&fsm_cmd_lock, irqflags);
		count = fsm_cmd_head.count;
		spin_unlock_irqrestore(&fsm_cmd_lock, irqflags);

	}

	return count;
}

#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
static inline int fsm_cmd_check_existing_one(
		ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	int i;
	ccci_fsm_cldma_cmd_t* node = fsm_cmd_head.first;
	if (fsm_cmd_head.count == 0)
		return 0;

	for (i=0; i < fsm_cmd_head.count; i++)
	{
		if ((node->cmd_id == fsm_cmd->cmd_id)
			&& (node->cmd_id == CCCI_FSM_CMD_SEND_HOST_REMOTE_WAKEUP))
				return 1;
		else
			node = node->next;
	}
	return 0;
}
#endif

static inline int fsm_cmd_enqueue(
		ccci_fsm_cldma_cmd_t *fsm_cmd)
{
	if (fsm_cmd_head.count) {
		fsm_cmd_head.last->next = fsm_cmd;
		fsm_cmd_head.last = fsm_cmd;

	} else {
		fsm_cmd_head.first = fsm_cmd;
		fsm_cmd_head.last  = fsm_cmd;

	}

	fsm_cmd_head.count ++;

	return 0;
}

static inline ccci_fsm_cldma_cmd_t *fsm_cmd_dequeue(void)
{
	ccci_fsm_cldma_cmd_t *fsm_cmd = NULL;

	if (fsm_cmd_head.count) {
		fsm_cmd = fsm_cmd_head.first;
		fsm_cmd_head.first = fsm_cmd->next;

		if (fsm_cmd_head.count == 1)
			fsm_cmd_head.last = NULL;

		fsm_cmd_head.count --;
	}

	return fsm_cmd;
}

int ccci_fsm_cldma_add_cmd(
		int cmd_id, void *cmd_data)
{
	int ret = 0;
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
	int cmd_existed = 0;
#endif
	unsigned long irqflags;
	ccci_fsm_cldma_cmd_t *fsm_cmd;

	fsm_cmd = kmalloc(sizeof(ccci_fsm_cldma_cmd_t), GFP_ATOMIC);
	if (!fsm_cmd) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kmalloc() fail\n",
			__func__);
		return -1;
	}

	fsm_cmd->next = NULL;
	fsm_cmd->cmd_id = cmd_id;
	fsm_cmd->cmd_data = cmd_data;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] add cmd id: %d\n",
		__func__, fsm_cmd->cmd_id);

	if (in_interrupt()) {
		spin_lock(&fsm_cmd_lock);
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
		cmd_existed = fsm_cmd_check_existing_one(fsm_cmd);
		if (cmd_existed == 0)
#endif
		ret = fsm_cmd_enqueue(fsm_cmd);
		spin_unlock(&fsm_cmd_lock);

	} else {
		spin_lock_irqsave(&fsm_cmd_lock, irqflags);
#ifdef __CLDMA_SUPPORT_REMOTE_WAKEUP__
		cmd_existed = fsm_cmd_check_existing_one(fsm_cmd);
		if (cmd_existed == 0)
#endif
		ret = fsm_cmd_enqueue(fsm_cmd);
		spin_unlock_irqrestore(&fsm_cmd_lock, irqflags);

	}

	if (ret < 0)
		kfree(fsm_cmd);
	else
		wake_up_all(&fsm_cmd_wq);

	return ret;
}

static inline ccci_fsm_cldma_cmd_t *fsm_cmd_get(void)
{
	ccci_fsm_cldma_cmd_t *fms_cmd = NULL;
	unsigned long irqflags;

	if (in_interrupt()) {
		spin_lock(&fsm_cmd_lock);
		fms_cmd = fsm_cmd_dequeue();
		spin_unlock(&fsm_cmd_lock);

	} else {
		spin_lock_irqsave(&fsm_cmd_lock, irqflags);
		fms_cmd = fsm_cmd_dequeue();
		spin_unlock_irqrestore(&fsm_cmd_lock, irqflags);

	}

	return fms_cmd;
}


static int fsm_cmd_thread_func(
		void *arg)
{
	ccci_fsm_cldma_cmd_t *fsm_cmd;
	int ret;

	CCCI_INIT_LOG(-1, TAG,
		"[%s] start running.\n",
		__func__);

	while (1) {
		ret = wait_event_interruptible(
				fsm_cmd_wq,
				fsm_cmd_count());

		if (ret == -ERESTARTSYS)
			continue;

		while ((fsm_cmd = fsm_cmd_get()) != NULL) {
			fsm_cmd_handle(fsm_cmd);

			kfree(fsm_cmd);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}


static int ccci_fsm_create_thread(void)
{
	fsm_cmd_thread = kthread_run(
			fsm_cmd_thread_func,
			NULL,
			"cldma_fsm_thrd");

	if (fsm_cmd_thread == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kthread_run() fail!\n",
			__func__);

		return -1;
	}

	return 0;
}

int ccci_fsm_cldma_init()
{
	int ret = 0;

	CCCI_INIT_LOG(-1, TAG, "[%s]\n", __func__);

	spin_lock_init(&fsm_cmd_lock);

	fsm_cmd_head.count = 0;
	fsm_cmd_head.first = NULL;
	fsm_cmd_head.last = NULL;


	init_waitqueue_head(&fsm_cmd_wq);
#if 0
	/*kernel-4.14 timer init method not suitable in 4.19*/
	init_timer(&hs_h3_moniter_timer);
	hs_h3_moniter_timer.function = fsm_hs_h3_monitor_func;
	hs_h3_moniter_timer.data = NULL;
#endif
	timer_setup(&hs_h3_moniter_timer, fsm_hs_h3_monitor_func, 0);

	ret = ccci_fsm_create_thread();

	return ret;
}
