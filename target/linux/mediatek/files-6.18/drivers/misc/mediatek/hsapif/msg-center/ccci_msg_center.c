// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/sched.h>
//#include <linux/rwlock.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include "ccci_msg_center.h"
#include "ccci_debug.h"


#define TAG "msg-center"



typedef struct register_head{
	int           msg_id;
	unsigned int  sub_id;
	void         *my_data;

	ccci_msg_cb_t callback;
	//struct register_head *next;
	struct list_head list;

} register_head_t;

typedef struct msg_wq_data{
	int completed;
	int result;
	wait_queue_head_t wq;

} msg_wq_data_t;

typedef struct message_head{
	int           msg_id;
	unsigned int  sub_id;
	void         *msg_data;
	int           free_msg_data;

	msg_wq_data_t *wq_data;
	struct message_head *next;

} ccci_msg_head_t;

#ifdef SUPPORT_ASYNC_SEDN_MSG
typedef struct {
	int count;

	ccci_msg_head_t *first;
	ccci_msg_head_t *last;

} msg_asyn_head_t;

static msg_asyn_head_t msg_asyn_head;
static spinlock_t msg_asyn_head_lock;
static struct task_struct *msg_asyn_thread;

static wait_queue_head_t msg_asyn_wq;

static inline ccci_msg_head_t *get_msg_asyn_head(void);
#endif

static struct list_head *msg_reg_array[CCCI_MAX_MSG_ID] = {0};
#ifdef USE_LOCK_TO_REG_ARRAY
static spinlock_t msg_reg_array_mutex;
#endif

static inline int handle_msg_data(ccci_msg_head_t *msg_head);
static inline int msg_id_check(int msg_id);

static inline ccci_msg_head_t *alloc_msg_head(
		int msg_id,
		unsigned int sub_id,
		void *msg_data,
		int free_msg_data)
{
	ccci_msg_head_t *msg_head = NULL;

	msg_head = kzalloc(sizeof(ccci_msg_head_t), GFP_KERNEL);
	if (msg_head == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc ccci_msg_head_t fail! "\
			"msg_id = %d; sub_id = 0x%08X\n",
			__func__, msg_id, sub_id);

		return msg_head;
	}
	msg_head->msg_id   = msg_id;
	msg_head->sub_id   = sub_id;
	msg_head->msg_data = msg_data;
	msg_head->free_msg_data = free_msg_data;

	msg_head->wq_data  = NULL;

	return msg_head;
}

#ifdef SUPPORT_ASYNC_SEDN_MSG
static inline int msg_asyn_thread_func(
		void *arg)
{
	ccci_msg_head_t *msg_head = NULL;
	int ret;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] start running.\n",
		__func__);

	while (1) {
		ret = wait_event_interruptible(
				msg_asyn_wq,
				msg_asyn_head.count);

		if (ret == -ERESTARTSYS)
			continue;

		while ((msg_head = get_msg_asyn_head()) != NULL) {

			handle_msg_data(msg_head);

			if (msg_head->free_msg_data && msg_head->msg_data)
				kfree(msg_head->msg_data);

			kfree(msg_head);
		}

		if (kthread_should_stop())
			break;
	}

	return 0;
}

static int msg_asyn_init(void)
{
	spin_lock_init(&msg_asyn_head_lock);

	msg_asyn_head.count = 0;
	msg_asyn_head.first = NULL;
	msg_asyn_head.last = NULL;

	init_waitqueue_head(&msg_asyn_wq);

	msg_asyn_thread = kthread_run(
			msg_asyn_thread_func,
			NULL,
			"msg_asyn_thread");

	if (msg_asyn_thread == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kthread_run() == NULL\n",
			__func__);
		return -1;
	}

	return 0;
}

static inline int enqueue_msg_asyn_data(
		ccci_msg_head_t *msg_head)
{
	if (msg_asyn_head.count) {
		msg_asyn_head.last->next = msg_head;
		msg_asyn_head.last = msg_head;

	} else {
		msg_asyn_head.first = msg_head;
		msg_asyn_head.last  = msg_head;

	}

	msg_asyn_head.count ++;

	return 0;
}

static inline ccci_msg_head_t *dequeue_msg_asyn_data(
		void)
{
	ccci_msg_head_t *msg_head = NULL;

	if (msg_asyn_head.count) {
		msg_head = msg_asyn_head.first;
		msg_asyn_head.first = msg_head->next;

		if (msg_asyn_head.count == 1)
			msg_asyn_head.last = NULL;

		msg_asyn_head.count --;
	}

	return msg_head;
}
#endif

static inline register_head_t *alloc_reg_head(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		ccci_msg_cb_t callback)
{
	register_head_t *new_reg = NULL;

	new_reg = kzalloc(sizeof(register_head_t), GFP_KERNEL);
	if (new_reg) {
		new_reg->msg_id   = msg_id;
		new_reg->sub_id   = sub_id;
		new_reg->my_data  = my_data;
		new_reg->callback = callback;

		INIT_LIST_HEAD(&new_reg->list);

		//new_reg->next     = NULL;

	} else
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc register_head_t fail!\n",
			__func__);

	return new_reg;
}

#ifdef SUPPORT_ASYNC_SEDN_MSG
static int add_msg_asyn_data(
		ccci_msg_head_t *msg_head)
{
	int ret = -1;
	unsigned long irqflags;

	if (in_interrupt()) {
		spin_lock(&msg_asyn_head_lock);
		ret = enqueue_msg_asyn_data(msg_head);
		spin_unlock(&msg_asyn_head_lock);

	} else {
		spin_lock_irqsave(&msg_asyn_head_lock, irqflags);
		ret = enqueue_msg_asyn_data(msg_head);
		spin_unlock_irqrestore(&msg_asyn_head_lock, irqflags);

	}

	return ret;
}

static inline ccci_msg_head_t *get_msg_asyn_head(
		void)
{
	ccci_msg_head_t *msg_head = NULL;
	unsigned long irqflags;

	if (in_interrupt()) {
		spin_lock(&msg_asyn_head_lock);
		msg_head = dequeue_msg_asyn_data();
		spin_unlock(&msg_asyn_head_lock);

	} else {
		spin_lock_irqsave(&msg_asyn_head_lock, irqflags);
		msg_head = dequeue_msg_asyn_data();
		spin_unlock_irqrestore(&msg_asyn_head_lock, irqflags);

	}

	return msg_head;
}
#endif

static inline int msg_reg_array_check(
		int msg_id)
{
	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	if (msg_reg_array[msg_id] == NULL) {
		if (msg_id != CCCI_CLDMA_STATE_CHANGE_ID) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: msg_reg_array[%d] == NULL\n",
				__func__, msg_id);
		}

		return -1;
	}

	return 0;
}

static inline int handle_msg_data(
		ccci_msg_head_t *msg_head)
{
	int ret = -1;
	int count = -1;
	register_head_t *reg;

	//CCCI_NORMAL_LOG(-1, TAG,
	//	"[%s] msg_id = %d; msg_reg_array[%d] = %p\n",
	//	__func__, msg_head->msg_id, msg_head->msg_id,
	//	msg_reg_array[msg_head->msg_id]);

	if (msg_head == NULL)
		return -CCCI_HSAPIF_ERR_GET_NULL_POINTER;

	if (msg_id_check(msg_head->msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

#ifdef USE_LOCK_TO_REG_ARRAY
	rcu_read_lock();
#endif

	if (!msg_reg_array_check(msg_head->msg_id)) {
		count = 0;
#ifdef USE_LOCK_TO_REG_ARRAY
		list_for_each_entry_rcu(reg,
				msg_reg_array[msg_head->msg_id], list) {
#else
		list_for_each_entry(reg,
				msg_reg_array[msg_head->msg_id], list) {
#endif
			if (reg->sub_id == CCCI_NONE_SUB_ID ||
				(reg->sub_id == msg_head->sub_id)) {

				ret = reg->callback(
							msg_head->msg_id,
							msg_head->sub_id,
							msg_head->msg_data,
							reg->my_data);

				if (msg_head->wq_data) {
					msg_head->wq_data->result = ret;
					msg_head->wq_data->completed = 1;
					wake_up(&msg_head->wq_data->wq);
				}

				count ++;
			}
		}
	}

#ifdef USE_LOCK_TO_REG_ARRAY
	rcu_read_unlock();
#endif
	if (count > -1) {
		if (count == 0)
			CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: no user receive: "\
			"msg_id(%d), sub_id(0x%08X).\n",
			__func__, msg_head->msg_id, msg_head->sub_id);

		else
			CCCI_DEBUG_LOG(-1, TAG,
			"[%s] count = %d; ret = %d\n",
			__func__, count, ret);

	}

	return ret;
}

static inline int msg_id_check(
		int msg_id)
{
	if (msg_id < 0 || msg_id >= CCCI_MAX_MSG_ID) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: msg_id(%d) is invalid\n",
			__func__, msg_id);

		return -1;
	}

	return 0;
}

static inline struct list_head *alloc_list_head(
		void)
{
	struct list_head *list = NULL;

	list = kzalloc(sizeof(struct list_head), GFP_KERNEL);
	if (list == NULL) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kzalloc struct list_head fail\n",
			__func__);

		return NULL;
	}

	INIT_LIST_HEAD(list);
	return list;
}

static inline int add_reg_to_array(
		register_head_t *new_reg)
{
	struct list_head *list = NULL;

	if (new_reg == NULL)
		return -1;

	if (msg_id_check(new_reg->msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

#ifdef USE_LOCK_TO_REG_ARRAY
	spin_lock(&msg_reg_array_mutex);
#endif
	list = msg_reg_array[new_reg->msg_id];
	if (list == NULL) {
		list = alloc_list_head();
		if (list == NULL) {
			kfree(new_reg);
#ifdef USE_LOCK_TO_REG_ARRAY
			spin_unlock(&msg_reg_array_mutex);
#endif
			return -1;
		}

		msg_reg_array[new_reg->msg_id] = list;
		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] msg_reg_array[%d] = %p\n",
			__func__, new_reg->msg_id, list);
	}

#ifdef USE_LOCK_TO_REG_ARRAY
	list_add_tail_rcu(&new_reg->list, list);
	spin_unlock(&msg_reg_array_mutex);
#else
	list_add_tail(&new_reg->list, list);
#endif
	//synchronize_rcu();
	return 0;
}

static inline int del_reg_from_array(
		int msg_id,
		unsigned int sub_id,
		ccci_msg_cb_t callback)
{
	register_head_t *reg;
	struct list_head *free_head = NULL;

	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

#ifdef USE_LOCK_TO_REG_ARRAY
	spin_lock(&msg_reg_array_mutex);
#endif
	if (msg_reg_array[msg_id]) {
		list_for_each_entry(reg, msg_reg_array[msg_id], list) {

			if (reg->sub_id == sub_id &&
				reg->callback == callback) {

#ifdef USE_LOCK_TO_REG_ARRAY
				list_del_rcu(&reg->list);
#else
				list_del(&reg->list);
#endif
				if (list_empty(msg_reg_array[msg_id])) {
					free_head = msg_reg_array[msg_id];
					msg_reg_array[msg_id] = NULL;
				}

#ifdef USE_LOCK_TO_REG_ARRAY
				spin_unlock(&msg_reg_array_mutex);
				synchronize_rcu();
#endif
				kfree(reg);

				if (free_head)
					kfree(free_head);

				CCCI_NORMAL_LOG(-1, TAG,
					"[%s] del reg succ.\n", __func__);

				return 0;
			}
		}
	}

#ifdef USE_LOCK_TO_REG_ARRAY
	spin_unlock(&msg_reg_array_mutex);
#endif

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] error: reg not exist(" \
		"msg_id = %d; sub_id = 0x%08X)\n",
		__func__, msg_id, sub_id);

	return -1;
}

#ifdef SUPPORT_ASYNC_SEDN_MSG
static msg_wq_data_t *alloc_msg_wq_data(void)
{
	msg_wq_data_t *wq_data = NULL;

	wq_data = kmalloc(sizeof(msg_wq_data_t), GFP_KERNEL);
	if (wq_data) {
		init_waitqueue_head(&wq_data->wq);
		wq_data->result = -1;
		wq_data->completed = 0;

		return wq_data;
	}

	CCCI_ERROR_LOG(-1, TAG,
		"[%s] error: kmalloc(msg_wq_data_t) fail.\n",
		__func__);

	return NULL;
}
#endif

int ccci_msg_center_init(void)
{
	CCCI_INIT_LOG(-1, TAG, "[%s]\n", __func__);
#ifdef USE_LOCK_TO_REG_ARRAY
	spin_lock_init(&msg_reg_array_mutex);
#endif

#ifdef SUPPORT_ASYNC_SEDN_MSG
	return msg_asyn_init();
#else
	return 0;
#endif

}

int ccci_msg_register(
		int msg_id,
		unsigned int sub_id,
		void *my_data,
		ccci_msg_cb_t callback)
{
	register_head_t *new_reg = NULL;

	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	new_reg = alloc_reg_head(msg_id, sub_id, my_data, callback);

	CCCI_INIT_LOG(-1, TAG,
		"[%s] msg_id = %d; sub_id = 0x%08X; " \
		"my_data = %p; callback = %p\n",
		__func__, msg_id, sub_id, my_data, callback);

	return add_reg_to_array(new_reg);
}

#ifdef USE_LOCK_TO_REG_ARRAY
int	ccci_msg_unregister(
		int msg_id,
		unsigned int sub_id,
		ccci_msg_cb_t callback)
{
	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] msg_id = %d; sub_id = 0x%08X; callback = %p\n",
		__func__, msg_id, sub_id, callback);

	return del_reg_from_array(msg_id, sub_id, callback);
}
#endif

/* the func help to free the msg_data */
int ccci_msg_send(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	ccci_msg_head_t msg_head;

	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	msg_head.msg_id   = msg_id;
	msg_head.sub_id   = sub_id;
	msg_head.msg_data = msg_data;
	msg_head.wq_data  = NULL;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] msg_id = %d; sub_id = 0x%08X\n",
		__func__, msg_id, sub_id);

	return handle_msg_data(&msg_head);
}

/* no compare sub_id, think the msg_id is one-to-one */
int ccci_msg_send_to_one(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	register_head_t *reg;

	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	if (msg_reg_array[msg_id] &&
		!list_empty(msg_reg_array[msg_id])) {

		reg = list_first_entry(msg_reg_array[msg_id],
				register_head_t, list);

		//CCCI_DEBUG_LOG(-1, TAG,
		CCCI_NORMAL_LOG(-1, TAG,
				"[%s] msg_id = %d; sub_id = 0x%08X\n",
				__func__, msg_id, sub_id);

		return reg->callback(
					msg_id,
					sub_id,
					msg_data,
					reg->my_data);
	}

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] error: no register msg; msg_id: %d; sub_id: %d\n",
		__func__, msg_id, sub_id);

	return -1;
}

#ifdef SUPPORT_ASYNC_SEDN_MSG
/* the func help to free the msg_data */
int ccci_msg_send_asyn(
		int msg_id,
		unsigned int sub_id,
		void *msg_data,
		int free_msg_data)
{
	ccci_msg_head_t *msg_head = NULL;

	if (msg_id_check(msg_id))
		goto msg_error;

	msg_head = alloc_msg_head(msg_id, sub_id, msg_data, free_msg_data);
	if (msg_head == NULL)
		goto msg_error;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] msg_id = %d; sub_id = 0x%08X\n",
		__func__, msg_id, sub_id);

	if (add_msg_asyn_data(msg_head))
		goto add_msg_error;

	wake_up_all(&msg_asyn_wq);
	return 0;

add_msg_error:
	kfree(msg_head);

msg_error:
	if (free_msg_data && msg_data)
		kfree(msg_data);

	return -1;
}

/* the func help to free the msg_data */
int ccci_msg_send_wait(
		int msg_id,
		unsigned int sub_id,
		void *msg_data)
{
	msg_wq_data_t *wq_data = NULL;
	ccci_msg_head_t *msg_head = NULL;
	int ret = -1;

	if (msg_id_check(msg_id))
		return -CCCI_HSAPIF_ERR_INVALID_MSG_ID;

	msg_head = alloc_msg_head(msg_id, sub_id, msg_data, 0);
	if (msg_head == NULL)
		return ret;

	wq_data = alloc_msg_wq_data();
	if (wq_data == NULL)
		goto alloc_wq_data_error;

	msg_head->wq_data = wq_data;
	if (add_msg_asyn_data(msg_head))
		goto add_msg_error;

	CCCI_NORMAL_LOG(-1, TAG,
		"[%s] msg_id = %d; sub_id = 0x%08X\n",
		__func__, msg_id, sub_id);

	wake_up_all(&msg_asyn_wq);

	wait_event(wq_data->wq, wq_data->completed);
	ret = wq_data->result;
	kfree(wq_data);

	return ret;

add_msg_error:
	kfree(wq_data);

alloc_wq_data_error:
	kfree(msg_head);

	return ret;
}

#endif

