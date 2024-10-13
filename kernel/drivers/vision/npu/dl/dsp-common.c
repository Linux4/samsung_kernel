// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <linux/vmalloc.h>
#include <linux/sizes.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "dl/dsp-common.h"
#include "dl/dsp-list.h"

#define DL_LOG_BUF_MAX		(1024)
#define DSP_DL_MIN_ELEMENT	(8 * 1024)
#define SIZE_OF_ELEMENT		256
#define DSP_MAX_CHUNK_MEMPOOL	10

struct dsp_dl_mem {
	const char *msg;
	struct dsp_list_node node;
	char data[0];
};

struct dsp_dl_mem_pool {
	bool is_used;
	const char *msg;
	struct dsp_list_node node;
	char data[128];
};

typedef struct dsp_mempool_s {
	int num_of_elements;
	int cur_num_of_free_elements;
	void **elements;
	char **memory_pool;
	int num_of_chunk;
} dsp_mempool_t;

static char *dsp_dl_log_buf;
static struct dsp_list_head *dl_mem_list;
dsp_mempool_t *dsp_dl_mempool;

dsp_mempool_t *dsp_mempool_create(void) {
	void *element;
	dsp_mempool_t *pool;

	pool = vzalloc(sizeof(*pool));
	if (!pool) {
		dsp_err("malloc(%zu) is failed - pool\n", sizeof(*pool));
		return NULL;
	}
	pool->num_of_chunk = 0;
	pool->elements = vmalloc(DSP_DL_MIN_ELEMENT * sizeof(void*));
	if (!pool->elements) {
		dsp_err("malloc(%zu) is failed - pool->elements\n",
				DSP_DL_MIN_ELEMENT * sizeof(void*));
		vfree(pool);
		return NULL;
	}
	pool->memory_pool = vzalloc(sizeof(char *) * DSP_MAX_CHUNK_MEMPOOL);
	if (!pool->memory_pool) {
		dsp_err("malloc(%zu) is failed - pool->memory_pool\n",
				sizeof(char *) * DSP_MAX_CHUNK_MEMPOOL);
		vfree(pool->elements);
		vfree(pool);
		return NULL;
	}
	pool->memory_pool[pool->num_of_chunk] = vmalloc(DSP_DL_MIN_ELEMENT * SIZE_OF_ELEMENT);
	if (!pool->memory_pool[pool->num_of_chunk]) {
		dsp_err("malloc(%d) is failed - pool->memory_pool[0]\n",
				DSP_DL_MIN_ELEMENT * SIZE_OF_ELEMENT);
		vfree(pool->memory_pool);
		vfree(pool->elements);
		vfree(pool);
		return NULL;
	}
	pool->num_of_elements = DSP_DL_MIN_ELEMENT;

	while (pool->cur_num_of_free_elements < pool->num_of_elements) {
		element =
			pool->memory_pool[pool->num_of_chunk] +
			pool->cur_num_of_free_elements * SIZE_OF_ELEMENT;
		pool->elements[pool->cur_num_of_free_elements++] = element;
	}
	pool->num_of_chunk++;
	return pool;
}

static inline bool dsp_mempool_is_available(dsp_mempool_t *pool) {
	if (pool->cur_num_of_free_elements > 0) return true;

	return false;
}

static inline bool dsp_mempool_is_empty(dsp_mempool_t *pool) {
	if (pool->cur_num_of_free_elements <= 0) return true;

	return false;
}

#if 0
int dsp_mempool_resize(dsp_mempool_t *pool) {
	void *element;
	void **new_elements;
	int index = 0;
	int new_num_of_elements;

	if (pool->num_of_chunk >= DSP_MAX_CHUNK_MEMPOOL) {
		dsp_err("memory pool chunk is full(%d)", pool->num_of_chunk);
		return -ENOMEM;
	}
	new_num_of_elements = (pool->num_of_chunk + 1) * DSP_DL_MIN_ELEMENT;
	if (new_num_of_elements <= pool->num_of_elements) {
		dsp_err("new number of elements(%d) is not larger than current number of elements(%d).\n",
				new_num_of_elements, pool->num_of_elements);
		return -EINVAL;
	}
	new_elements = vmalloc(new_num_of_elements * sizeof(*new_elements));
	if (!new_elements) {
		dsp_err("malloc(%zu) is failed - new_elements\n",
				new_num_of_elements * sizeof(*new_elements));
		return -ENOMEM;
	}

	memcpy(new_elements, pool->elements,
			pool->cur_num_of_free_elements * sizeof(*new_elements));
	vfree(pool->elements);
	pool->elements = new_elements;
	pool->num_of_elements = new_num_of_elements;
	pool->memory_pool[pool->num_of_chunk] = vzalloc(DSP_DL_MIN_ELEMENT * SIZE_OF_ELEMENT);
	if (!pool->memory_pool[pool->num_of_chunk]) {
		dsp_err("malloc(%zu) is failed - pool->memory_pool[%d]\n",
				DSP_DL_MIN_ELEMENT * SIZE_OF_ELEMENT, pool->num_of_chunk);
		vfree(new_elements);
		return -ENOMEM;
	}

	while (index < DSP_DL_MIN_ELEMENT) {
		element = pool->memory_pool[pool->num_of_chunk] + (index++) * SIZE_OF_ELEMENT;
		pool->elements[pool->cur_num_of_free_elements++] = element;
	}
	pool->num_of_chunk++;

	dsp_dbg("dsp mempool was resized.\n");

	return 0;
}
#endif

void dsp_mempool_destroy(dsp_mempool_t *pool) {
	int i = 0;

	if (unlikely(!pool))
		return;

	for(i = 0; i < pool->num_of_chunk; i++) {
		if (pool->memory_pool[i])
			vfree(pool->memory_pool[i]);
	}
	if (pool->memory_pool)
		vfree(pool->memory_pool);

	if (pool->elements)
		vfree(pool->elements);

	vfree(pool);
}

static void *__dsp_vzalloc(size_t size)
{
	return vzalloc(size);
}

void dsp_dl_lib_file_reset(struct dsp_dl_lib_file *file)
{
	file->r_ptr = 0;
}

int dsp_dl_lib_file_read(char *buf, size_t size,
	struct dsp_dl_lib_file *file)
{
	if (file->r_ptr + size > file->size)
		return -1;

	memcpy(buf, (char *)(file->mem) + file->r_ptr, size);
	file->r_ptr += size;
	return size;
}

static void __dsp_dl_mem_print(void)
{
	struct dsp_list_node *node;

	dsp_dbg("Print dl mems\n");
	dsp_list_for_each(node, dl_mem_list)
	dsp_dbg("allocated memory(%s)\n",
		(container_of(node, struct dsp_dl_mem,
				node))->msg);
}

void dsp_dl_put_log_buf(const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	snprintf(dsp_dl_log_buf + strlen(dsp_dl_log_buf), DL_LOG_BUF_MAX,
			"%pV", &vaf);
	va_end(args);
}

void dsp_dl_print_log_buf(int level, const char *func)
{
	switch (level) {
	case DL_LOG_ERROR:
		dsp_dump("[%-30s]%s", func, dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	case DL_LOG_INFO:
		dsp_dump("%s", dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	case DL_LOG_DEBUG:
	default:
		dsp_dump("[%-30s]%s", func, dsp_dl_log_buf);
		dsp_dl_log_buf[0] = '\0';
		break;
	}
}

void *dsp_dl_mempool_alloc(dsp_mempool_t* dl_mempool)
{
	void *element;

	if (dsp_mempool_is_empty(dl_mempool)) return NULL;

	element = dl_mempool->elements[--dl_mempool->cur_num_of_free_elements];

	return element;
}

void dsp_dl_mempool_free(dsp_mempool_t* dl_mempool, void *element)
{
	if (dl_mempool->cur_num_of_free_elements >= dl_mempool->num_of_elements)
		return;

	dl_mempool->elements[dl_mempool->cur_num_of_free_elements++] = element;
}

int dsp_common_init(void)
{
	dsp_dl_log_buf = (char *)__dsp_vzalloc(DL_LOG_BUF_MAX);
	if (!dsp_dl_log_buf) {
		dsp_err("malloc(%d) is failed - dsp_dl_log_buf\n",
				DL_LOG_BUF_MAX);
		return -ENOMEM;
	}
	dsp_dl_log_buf[0] = '\0';

	dl_mem_list = (struct dsp_list_head *)__dsp_vzalloc(
			sizeof(struct dsp_list_head));
	if (!dl_mem_list) {
		dsp_err("malloc(%zu) is failed - dl_mem_list\n",
				sizeof(struct dsp_list_head));
		vfree(dsp_dl_log_buf);
		dsp_dl_log_buf = NULL;
		return -ENOMEM;
	}
	dsp_list_head_init(dl_mem_list);

	dsp_dl_mempool = dsp_mempool_create();
	if (!dsp_dl_mempool) {
		dsp_err("failed to create memory_pool in dsp\n");
		vfree(dl_mem_list);
		vfree(dsp_dl_log_buf);
		dl_mem_list = NULL;
		dsp_dl_log_buf = NULL;
		return -ENOMEM;
	}

	return 0;
}

void dsp_common_free(void)
{
	__dsp_dl_mem_print();

	if (dsp_dl_log_buf) vfree(dsp_dl_log_buf);

	if (dl_mem_list) vfree(dl_mem_list);

	if (dsp_dl_mempool) dsp_mempool_destroy(dsp_dl_mempool);

}

void *dsp_dl_malloc(size_t size, const char *msg)
{
	struct dsp_dl_mem *mem;
	struct dsp_dl_mem_pool *mempool;

	if (size <= SZ_128 && dsp_mempool_is_available(dsp_dl_mempool)) {
		mempool = (struct dsp_dl_mem_pool *)dsp_dl_mempool_alloc(dsp_dl_mempool);
		if (!mempool) {
			dsp_err("%s malloc(%zu) is failed - mempool\n", msg, size);
			return NULL;
		}
		memset(mempool, 0 ,sizeof(struct dsp_dl_mem_pool));
		mempool->is_used = true;
		mempool->msg = msg;
		dsp_list_node_init(&mempool->node);
		dsp_list_node_push_back(dl_mem_list, &mempool->node);

		return (void *)(mempool->data);
	}
	mem = (struct dsp_dl_mem *)__dsp_vzalloc(sizeof(*mem) + size);
	if (!mem) {
		dsp_err("%s malloc(%zu) is failed\n", msg, size);
		return NULL;
	}
	mem->msg = msg;
	dsp_list_node_init(&mem->node);
	dsp_list_node_push_back(dl_mem_list, &mem->node);

	return (void *)(mem->data);
}

bool dsp_mempool_is_in_mempool(dsp_mempool_t *pool, void *mem)
{
	int index = 0;
	void *lower_limit;
	void *upper_limit;

	for(index = 0; index < pool->num_of_chunk; index++) {
		lower_limit = pool->memory_pool[index];
		upper_limit = pool->memory_pool[index] +
			DSP_DL_MIN_ELEMENT * SIZE_OF_ELEMENT;

		if (mem >= lower_limit && mem < upper_limit) return true;
	}

	return false;
}

void dsp_dl_free(void *data)
{
	struct dsp_dl_mem *mem = NULL;
	struct dsp_dl_mem_pool *mempool = NULL;

	if (data == NULL)
		return;

	if (dsp_mempool_is_in_mempool(dsp_dl_mempool, data)) {
		mempool = container_of(data, struct dsp_dl_mem_pool, data);
		if (mempool->is_used == true) {
			dsp_list_node_remove(dl_mem_list, &mempool->node);
			dsp_dl_mempool_free(dsp_dl_mempool, mempool);
			mempool->is_used = false;
		}
	} else {
		mem = container_of(data, struct dsp_dl_mem, data);
		dsp_list_node_remove(dl_mem_list, &mem->node);
		vfree(mem);
	}
}

void add_string(struct string_manager* manager, char* string)
{
	char** backup_strings = manager->strings;
	manager->count++;
	if (manager->strings == NULL) {
		manager->strings = (char**)kmalloc(sizeof(char*), GFP_KERNEL);
	} else {
		manager->strings = (char**)krealloc(manager->strings,
				manager->count * sizeof(char*), GFP_KERNEL);
	}

	/* failed to alloc. */
	if (ZERO_OR_NULL_PTR(manager->strings))
		goto p_err;

	backup_strings = manager->strings;
	manager->strings[manager->count - 1] = kstrdup(string, GFP_KERNEL);

	/* failed to alloc. */
	if (ZERO_OR_NULL_PTR(manager->strings[manager->count - 1]))
		goto p_err;

	return;

p_err:
	dsp_err("add_string is failed on count(%d)\n", manager->count);
	manager->strings = backup_strings;
	manager->count--;
}

char* get_string(struct string_manager* manager, int index)
{
	if (index < manager->count) {
		return manager->strings[index];
	}
	return NULL;
}

void free_strings(struct string_manager* manager)
{
	for (int i = 0; i < manager->count; i++) {
		kfree(manager->strings[i]);
		manager->strings[i] = NULL;
	}
	kfree(manager->strings);
	manager->strings = NULL;
	manager->count = 0;
}
