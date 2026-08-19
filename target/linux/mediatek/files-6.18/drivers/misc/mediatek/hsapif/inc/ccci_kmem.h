// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */


#ifndef __CCCI_FUNC_H__
#define __CCCI_FUNC_H__


#include <linux/slab.h>


typedef struct ccci_kmem_data {
	void *org_data;
	void *off_data;
	int   alloc_len;
	int   data_len;

} ccci_kmem_data_t;



extern
int ext_ccci_kmem_init(void);

extern
void *ext_ccci_kmem_alloc(gfp_t flags, unsigned int len);

extern
void ext_ccci_kmem_free(void *objp);

extern
ccci_kmem_data_t *ext_ccci_data_alloc(gfp_t flags, unsigned int len);

extern
void ext_ccci_data_free(ccci_kmem_data_t *data);

extern
void ext_ccci_data_clear(ccci_kmem_data_t *data);

extern
void ext_ccci_kmem_exit(void);

extern
void *ext_ccci_kz_alloc(int md_id, size_t s, gfp_t f);


//extern
//ccci_kmem_data_t *ext_ccci_kmem_alloc_data(gfp_t flags, unsigned int len);

#endif  //__CCCI_FUNC_H__
