// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#include <linux/slab.h>
#include <linux/spinlock.h>

#include "ccci_list.h"
#include "ccci_debug.h"
#include "ccci_kmem.h"

#define TAG "list"





void ext_ccci_list_init(ext_ccci_data_list_t *list)
{
	spin_lock_init(&list->lock);
	list->qlen = 0;
	list->next = NULL;
	list->last = NULL;
}

static ext_ccci_node_t *ccci_list_node_alloc(void *data, gfp_t flags)
{
	ext_ccci_node_t *pnode;

	pnode = (ext_ccci_node_t *)ext_ccci_kmem_alloc(flags,
							sizeof(ext_ccci_node_t));
	if (!pnode)
		return NULL;

	pnode->data   = data;
	//pnode->len    = len;
	//pnode->offset = 0;
	pnode->next   = NULL;

	return pnode;
}

ext_ccci_node_t *ext_ccci_list_add_node(ext_ccci_data_list_t *list,
		void *data, gfp_t flags)
{
	unsigned long lock_flags;
	ext_ccci_node_t *pnode = ccci_list_node_alloc(data, flags);

	if (!pnode)
		return NULL;

	spin_lock_irqsave(&list->lock, lock_flags);

	//add to list's tail
	if (list->last)
		list->last->next = pnode;
	else
		list->next = pnode;

	list->last = pnode;

	list->qlen ++;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] list->qlen = %d\n",
		__func__, list->qlen);

	spin_unlock_irqrestore(&list->lock, lock_flags);

	return pnode;
}

void *ext_ccci_list_get_first(ext_ccci_data_list_t *list)
{
	unsigned long flags;
	ext_ccci_node_t *ret = NULL;

	spin_lock_irqsave(&list->lock, flags);

	//get from list's header
	ret = list->next;

	spin_unlock_irqrestore(&list->lock, flags);

	return ret->data;
}

void ext_ccci_list_del_first(ext_ccci_data_list_t *list)
{
	unsigned long flags;
	ext_ccci_node_t *pnode;

	spin_lock_irqsave(&list->lock, flags);

	//del list's header
	if (list->next) {
		pnode = list->next;
		list->next = pnode->next;

		if (list->next == NULL)
			list->last = NULL;

		list->qlen --;

		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] list->qlen = %d\n",
			__func__, list->qlen);

		ext_ccci_kmem_free(pnode);

	};

	spin_unlock_irqrestore(&list->lock, flags);
}

int ext_ccci_list_clear_node(ext_ccci_data_list_t *list, ccci_list_clear_cb_t clear)
{
	unsigned long flags;
	ext_ccci_node_t *pnode, *next;
	int count = 0;

	spin_lock_irqsave(&list->lock, flags);

	count = list->qlen;
	pnode = list->next;

	while (pnode) {
		next = pnode->next;

		clear(pnode->data);
		ext_ccci_kmem_free(pnode);

		pnode = next;
	}

	list->next = NULL;
	list->last = NULL;
	list->qlen = 0;

	spin_unlock_irqrestore(&list->lock, flags);

	return count;
}

int ext_ccci_get_list_node_count(ext_ccci_data_list_t *list)
{
	unsigned long flags;
	int count = 0;

	spin_lock_irqsave(&list->lock, flags);

	count = list->qlen;

	spin_unlock_irqrestore(&list->lock, flags);

	return count;
}

int ext_ccci_list_empty(ext_ccci_data_list_t *list)
{
	unsigned long flags;
	int isempty = 1;

	spin_lock_irqsave(&list->lock, flags);

	isempty = (list->qlen == 0);

	spin_unlock_irqrestore(&list->lock, flags);

	return isempty;
}
