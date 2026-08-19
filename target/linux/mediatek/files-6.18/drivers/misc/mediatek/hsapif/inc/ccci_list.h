// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef __CCCI_LIST_H__
#define __CCCI_LIST_H__




typedef struct ext_ccci_node {
	//int   len;
	//int   offset;
	void *data;
	struct ext_ccci_node *next;

} ext_ccci_node_t;


typedef struct ext_ccci_data_list {
	spinlock_t lock;
	int qlen;

	ext_ccci_node_t *next;
	ext_ccci_node_t *last;

} ext_ccci_data_list_t;


typedef void (* ccci_list_clear_cb_t)(void *data);


extern
void ext_ccci_list_init(ext_ccci_data_list_t *list);

extern
ext_ccci_node_t *ext_ccci_list_add_node(ext_ccci_data_list_t *list,
		void *data, gfp_t flags);

extern
void *ext_ccci_list_get_first(ext_ccci_data_list_t *list);

extern
void ext_ccci_list_del_first(ext_ccci_data_list_t *list);

extern
int ext_ccci_list_clear_node(ext_ccci_data_list_t *list, ccci_list_clear_cb_t clear);

extern
int ext_ccci_get_list_node_count(ext_ccci_data_list_t *list);

extern
int ext_ccci_list_empty(ext_ccci_data_list_t *list);

#endif  //__CCCI_LIST_H__
