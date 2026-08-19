// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */



#include <linux/slab.h>
#include <linux/device.h>


#include "ccci_debug.h"
#include "ccci_kmem.h"
#include "ccci_hif.h"

#define TAG "kmem"


struct kmem_cache_info {
	__u16 index;
	__u16 reserve;

};


#define KMEM_CACHE_50_INDEX 0xFFFF
#define KMEM_GEAR_VALUE 500

#define KMEM_MAX_CACHE_SIZE (4000 - sizeof(struct kmem_cache_info))

#define KMEM_CACHE_COUNT ((KMEM_MAX_CACHE_SIZE + \
			sizeof(struct kmem_cache_info)) / KMEM_GEAR_VALUE)

static struct kmem_cache *kmem_cache_50 = NULL;
static struct kmem_cache *kmem_caches[KMEM_CACHE_COUNT] = {0};


int ext_ccci_kmem_init(void)
{
	int i = 0;
	char cache_name[30];

	kmem_cache_50 = kmem_cache_create_usercopy("ccci_kmem_cache_50",
			50, 0, 0, 0, 50, NULL);

	if (!kmem_cache_50) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kmem_cache_50 fail.\n", __func__);

		return -1;
	}

	while(i < KMEM_CACHE_COUNT) {
		sprintf(cache_name, "ccci_hif_kmem_%d", i);
		kmem_caches[i] = kmem_cache_create_usercopy(cache_name,
				(i+1)*KMEM_GEAR_VALUE, 0, 0, 0, (i+1)*KMEM_GEAR_VALUE, NULL);

		if (!kmem_caches[i]) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kmem_cache_create fail. size = %d\n",
				__func__, (i+1)*KMEM_GEAR_VALUE);

			return -1;
		}

		i++;
	}

	return 0;
}

static void *ccci_kmem_set_info(void *objp, __u16 index)
{
	((struct kmem_cache_info *)objp)->index = index;

	return (objp + sizeof(struct kmem_cache_info));
}

static void *ccci_kmem_get_info(void *objp, __u16 *index)
{
	objp -= (sizeof(struct kmem_cache_info));
	*index = ((struct kmem_cache_info *)objp)->index;

	return objp;
}

static __u16 ccci_kmem_get_index(unsigned int len)
{
	unsigned int index;

	index = len / KMEM_GEAR_VALUE;

	if ((len % KMEM_GEAR_VALUE) == 0)
		index --;

	return (__u16)index;
}

void *ext_ccci_kmem_alloc(gfp_t flags, unsigned int len)
{
	void *objp = NULL;
	__u16 index;

	if (len > KMEM_MAX_CACHE_SIZE) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: alloc mem len > %ld fail.\n",
			__func__, KMEM_MAX_CACHE_SIZE);

		return NULL;
	}

	len += sizeof(struct kmem_cache_info);

	// handle len <= 50 kmem cache alloc
	if (len <= 50) {
		objp = kmem_cache_zalloc(kmem_cache_50, flags);
		index = KMEM_CACHE_50_INDEX;

	} else {
		// handle len > 50 kmem cache alloc
		index = ccci_kmem_get_index(len);
		objp = kmem_cache_zalloc(kmem_caches[index], flags);
		if (!objp) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: kmem_cache_zalloc fail.(%d)\n",
				__func__, index);

			return NULL;
		}
	}

	if (!objp) {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: kmem_cache_zalloc fail. len: %ld\n",
			__func__, len - sizeof(struct kmem_cache_info));

		return NULL;
	}


	objp = ccci_kmem_set_info(objp, index);

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] len: %ld; alloc: %d; index: %d; objp: %p\n",
		__func__, len - sizeof(struct kmem_cache_info),
		index == KMEM_CACHE_50_INDEX ? 50 : KMEM_GEAR_VALUE * (index + 1),
		index, objp);

	return objp;
}

ccci_kmem_data_t *ext_ccci_data_alloc(gfp_t flags, unsigned int len)
{
	ccci_kmem_data_t *data = ext_ccci_kmem_alloc(flags, sizeof(ccci_kmem_data_t));


	if (data) {
		data->org_data = ext_ccci_kmem_alloc(flags, len);

		if (!data->org_data) {
			ext_ccci_kmem_free(data);

			return NULL;
		}

		// 64 bit align
		data->off_data = (void *)((unsigned long)(data->org_data + 63) & (unsigned long)(~63));
		data->alloc_len = len - (unsigned long)(data->off_data - data->org_data);
		if (data->alloc_len > MAX_GPD_RECV_DATA_LEN)
			data->alloc_len = MAX_GPD_RECV_DATA_LEN;

		data->data_len = 0;

		CCCI_DEBUG_LOG(-1, TAG,
			"[%s] data: %p; org_data: %p; off_data: %p; alloc_len: %d; len: %d\n",
			__func__, data, data->org_data, data->off_data, data->alloc_len, len);
	}

	return data;
}

void ext_ccci_kmem_free(void *objp)
{
	__u16 index;

	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] objp: %p\n", __func__, objp);

	objp = ccci_kmem_get_info(objp, &index);

	//CCCI_NORMAL_LOG(-1, TAG,
	//	"[%s] objp: %p; index = %d\n", __func__, objp, index);

	if (index == KMEM_CACHE_50_INDEX)
		kmem_cache_free(kmem_cache_50, objp);

	else
		kmem_cache_free(kmem_caches[index], objp);

}

void ext_ccci_data_free(ccci_kmem_data_t *data)
{
	CCCI_DEBUG_LOG(-1, TAG,
		"[%s] data: %p; org_data: %p; off_data: %p\n",
		__func__, data, data->org_data, data->off_data);

	ext_ccci_kmem_free(data->org_data);
	ext_ccci_data_clear(data);
	ext_ccci_kmem_free(data);
}

void ext_ccci_data_clear(ccci_kmem_data_t *data)
{
	data->org_data = NULL;
	data->off_data = NULL;
	data->alloc_len = 0;
	data->data_len = 0;
}

void ext_ccci_kmem_exit(void)
{
	int i = 0;

	kmem_cache_destroy(kmem_cache_50);
	kmem_cache_50 = NULL;

	while(i < KMEM_CACHE_COUNT) {
		kmem_cache_destroy(kmem_caches[i]);
		kmem_caches[i] = NULL;

		i ++;
	}
}

void *ext_ccci_kz_alloc(int md_id, size_t s, gfp_t f)
{
	void *pmem;

	pmem = kzalloc(s, f);
	if (pmem == NULL) {
		CCCI_ERROR_LOG(md_id, TAG,
			"[%s] error: kzalloc return NULL from %ps\n",
			__func__, __builtin_return_address(0));

	}

	if (pmem != NULL)
		memset(pmem, 0, s);

	return pmem;
}

