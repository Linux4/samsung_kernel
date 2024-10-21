// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#define GZ_FFA_DEV_NAME	"gz_ffa_dev"
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/arm_ffa.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>
#include <linux/smp.h>
#include <linux/uaccess.h>
#include <linux/arm-smccc.h>
#include <linux/spinlock.h>
#include <linux/dma-buf.h>
#include <mtk_heap.h>

#include <tz_cross/trustzone.h>
#include "gz_ffa.h"

typedef u16 ffa_partition_id_t;
static spinlock_t ffa_mem_lock;

/* Use separate ID prefixes to distinguish for now. */
#define IS_LOGICAL_SP_ID(x) (((x) >> 14) & 0x11)
#define IS_PHYSICAL_SP_ID(x) (((x) >> 15) & 0x1)

/* Store the discovered partition data globally. */
static const struct ffa_ops *g_ffa_ops;
static struct ffa_device *g_ffa_dev;
static bool ffa_enabled;

static struct ffa_mem_list_head fm_head;
static struct ffa_mem_list_head *ffa_drv_manager;

/* Ensure we only initialise the device driver once. */
bool device_driver_initalized;

/**
 * [MTK] specific flag for ffa memory receive req
 * - FFA_MEM_LEND, this flag indicates do memory retrieve at the same time
 */
#define FFA_MEMORY_REGION_FLAG_MTK_RETRIEVE		(1<<31)

struct args {
	uint64_t arg[5];
};

#define VM_NUMBER_MAX 10
#define CONSTITUENT_BUFFER	SZ_4K

#define FFA_DEBUG(fmt...) pr_info("[FFA]" fmt)
#define FFA_INFO(fmt...) pr_info("[FFA]" fmt)
#define FFA_ERR(fmt...) pr_info("[FFA][ERR]" fmt)

static const int ffa_linux_errmap[] = {
	/* better than switch case as long as return value is continuous */
	0,		/* FFA_RET_SUCCESS */
	-EOPNOTSUPP,	/* FFA_RET_NOT_SUPPORTED */
	-EINVAL,	/* FFA_RET_INVALID_PARAMETERS */
	-ENOMEM,	/* FFA_RET_NO_MEMORY */
	-EBUSY,		/* FFA_RET_BUSY */
	-EINTR,		/* FFA_RET_INTERRUPTED */
	-EACCES,	/* FFA_RET_DENIED */
	-EAGAIN,	/* FFA_RET_RETRY */
	-ECANCELED,	/* FFA_RET_ABORTED */
};

static struct ffa_mem_list_head *ffa_mem_list_lock(void)
{
	spin_lock(&ffa_mem_lock);
	return ffa_drv_manager;
}

static void ffa_mem_list_unlock(void)
{
	spin_unlock(&ffa_mem_lock);
}

static inline int ffa_to_linux_errno(int errno)
{
	int err_idx = -errno;

	if (err_idx >= 0 && err_idx < ARRAY_SIZE(ffa_linux_errmap))
		return ffa_linux_errmap[err_idx];
	return -EINVAL;
}

static void ffa_mem_list_dump(void)
{
	struct ffa_mem_list_head *list_head;
	struct ffa_mem_list_head *iterator;
	struct ffa_mem_record_table *mem_record;

	list_head = ffa_mem_list_lock();
	for (iterator = list_head->next; iterator != list_head; iterator = iterator->next) {
		mem_record = list_entry(iterator, struct ffa_mem_record_table, list);
		/* FFA_DEBUG("[DUMP] handle=0x%lx mem_region=0x%lx, meta_data=0x%lx\n",
		 *				mem_record->handle,
		 *				mem_record->mem_region,
		 *				mem_record->meta_data);
		 */
	}
	ffa_mem_list_unlock();
}

enum message_t {
	/* Partition Only Messages. */
	FF_A_RELAY_MESSAGE = 0,

	/* Basic Functionality. */
	FF_A_ECHO_MESSAGE,
	FF_A_RELAY_MESSAGE_EL3,

	/* Memory Sharing. */
	FF_A_MEMORY_SHARE,
	FF_A_MEMORY_SHARE_FRAGMENTED,
	FF_A_MEMORY_LEND,
	FF_A_MEMORY_LEND_FRAGMENTED,

	/* split test memory sharing without using msg_send_direct */
	/* Integration test */
	FF_A_TEST_MEMORY_SHARE_ALL = 100,
	FF_A_TEST_MEMORY_SHARE_ALL_TA = 101,
	FF_A_TEST_MEMORY_LEND_ALL = 102,
	FF_A_TEST_MEMORY_LEND_ALL_TA = 103,

	/* unit test */
	/* issue memory send (lend or share) */
	FF_A_DRV_MEMORY_SEND = 121,
	/* issue memory retrieve request */
	FF_A_TEST_MEM_RETRIEVE_REQ = 122,
	FF_A_TEST_MEM_RELINQUISH = 123,
	FF_A_TEST_MEM_RECLAIM = 124,
	FF_A_DRV_READ_BACK = 125,

	/* debug utilities */
	FF_A_GZ_DUMP_MEM_SHARE = 201,
	FF_A_GZ_DUMP_PARTITION_INFO,
	FF_A_DUMP_LEND2PA_INFO,

	/* GZ_FFA_CMD_ */
	GZ_FFA_IS_ENABLED = 200,
	GZ_FF_A_MEMORY_SEND = 250,
	GZ_FF_A_MEM_RECLAIM,
	GZ_FF_A_PARTITION_INFO_GET,
	GZ_FF_A_MESSAGE_SEND,
	GZ_FF_A_GET_CHM_HANDLE,


	LAST = 65533,
	FF_A_RUN_ALL,
	FF_A_OP_MAX = 65535
};

int ffa_send_to_ha(struct ffa_send_direct_data *data, unsigned short vmid)
{
	int ret;
	struct ffa_device target_device = {0};

	if (!g_ffa_dev || !g_ffa_ops)
		return -ENODEV;

	target_device.vm_id = vmid;

	ret = g_ffa_ops->msg_ops->sync_send_receive(&target_device, data);
	while (ret == -EBUSY) {
		FFA_DEBUG("DIR_REQ: Busy - Retrying...\n");
		ret = g_ffa_ops->msg_ops->sync_send_receive(&target_device, data);
	}
	if (ret) {
		FFA_ERR("Failed to send msg to HA ret = %d\n", ret);
		return TZ_RESULT_ERROR_COMMUNICATION;
	}

	return 0;
}

/**
 * @brief allocate memory aligned to PAGE_SIZE
 *
 * @param mem_size
 * @retval 0:successful
 * @retval -ENOMEM: cannot allocate memory
 */
int kmalloc_aligned(size_t mem_size, void **origin_ptr, void **ptr)
{
	size_t alignment = PAGE_SIZE;
	size_t mask = alignment - 1;
	void *__ptr;

	FFA_ERR("%s %d\n", __func__, __LINE__);
	__ptr = kmalloc_large(mem_size + alignment, GFP_KERNEL);
	if (!IS_ERR_OR_NULL(__ptr)) {
		*origin_ptr = __ptr;
		*ptr = (void *) (((u64)__ptr + mask) & ~mask);
		// FFA_ERR("origin_ptr=0x%lx ptr=0x%lx\n", *origin_ptr, *ptr);
		return 0;
	} else
		return -ENOMEM;
}

/**
 * @brief memory send to ffa driver to issue memory send operation
 *
 * @param share True:share or false:lend
 * @param involve_sp True: add one secure world receiver, False: only HA
 * @param mtk_retrieve flag to indicate using mtk simple retrieve
 * @param mem_size memory size
 * @param page_entries
 * @param __test (output) buffer points to shared memory
 * @param handle (output) handle returned by ffa
 * @return long
 */
static long memory_send(bool share, bool involve_sp,
		bool mtk_retrieve, uint32_t mem_size, uint32_t page_entries,
		char **__test, unsigned long *handle, unsigned short *vmids, uint32_t number_of_vm)
{
	int retval, i;
	void *origin_mem_region;
	void *mem_region;
	char *buf;
	struct sg_table sgt;
	struct page **pages;
	struct ffa_mem_ops_args args;
	u32 index;
	struct ffa_mem_region_attributes mem_region_attributes[VM_NUMBER_MAX+1];
	struct scatterlist *sg;
	struct ffa_mem_list_head *list_head;
	struct ffa_mem_list_head *prev_node;
	struct ffa_mem_list_head *current_node;

	struct ffa_mem_record_table *new_node;

	new_node = kzalloc(sizeof(struct ffa_mem_record_table), GFP_KERNEL);
	if (unlikely(!new_node)) {
		FFA_ERR("Out of memory. %s:%d\n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	if (kmalloc_aligned(mem_size, &origin_mem_region, &mem_region)) {
		FFA_ERR("Out of memory. %s:%d\n", __FILE__, __LINE__);
		retval = -ENOMEM;
		goto free_node_ret;
	}

	buf = (char *)mem_region;
	*__test = buf;
	FFA_DEBUG("Address: 0x%p Physical: %llx\n", buf, virt_to_phys(buf));

	/* sharing only even pages to produce fragmentations */
	page_entries /= 2;
	pages = kcalloc(page_entries, sizeof(void *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(pages)) {
		FFA_ERR("Out of memory. %s:%d\n", __FILE__, __LINE__);
		retval = -ENOMEM;
		goto free_mem_ret;
	}

	for (index = 0; index < page_entries; index++) {
		/* We want to keep even pages. This ensures the constituents
		 * have a single page and consume the maximum space possible in the
		 * memory region descriptor.
		 */
		__u64 va = (__u64)mem_region + ((__u64)index * 2 * 4096);

		pages[index] = virt_to_page(va);
		FFA_DEBUG("Page: %d at 0x%p (0x%llx)\n", index,
			 virt_to_page(va), page_to_phys(virt_to_page(va)));
	}
	retval = sg_alloc_table_from_pages(&sgt, pages,
				  page_entries, 0,
				  page_entries * 4096, GFP_KERNEL);
	if (retval) {
		FFA_ERR("sg_alloc_table_from_pages failed, ret=%d %s:%d\n", retval,
			   __FILE__, __LINE__);
		goto free_mem_ret;
	}

	FFA_DEBUG("Start mem share %s:%d\n", __FILE__, __LINE__);

	sg = sgt.sgl;
	do {
		FFA_DEBUG("sg addr=%llx size=%d\n", sg_phys(sg), sg->length);
	} while ((sg = sg_next(sg)));

	for (i = 0; i < number_of_vm && i < VM_NUMBER_MAX; i++) {
		mem_region_attributes[i] = (struct ffa_mem_region_attributes) {
			.receiver = vmids[i],
			.attrs = FFA_MEM_RW
		};
	}
	if (involve_sp) {
		mem_region_attributes[i] = (struct ffa_mem_region_attributes) {
			/* SPMC will check if this is valid partition */
			.receiver = 0x8001,
			.attrs = FFA_MEM_RW
		};
		args.nattrs = i+1;
	} else {
		args.nattrs = i;
	}

	if (mtk_retrieve)
		args.flags = FFA_MEMORY_REGION_FLAG_MTK_RETRIEVE;
	else
		args.flags = 0;

	args.use_txbuf = true;
	args.tag = 0;
	args.g_handle = 0;
	args.sg = sgt.sgl;
	args.attrs = mem_region_attributes;

	/* Share or lend the memory. */
	if (share)
		retval = g_ffa_ops->mem_ops->memory_share(&args);
	else
		retval = g_ffa_ops->mem_ops->memory_lend(&args);

	if (retval) {
		FFA_ERR("Failed to send the memory region ret=%d %s:%d\n", retval,
			   __FILE__, __LINE__);
		goto free_mem_ret;
	}

	*handle = args.g_handle;

	new_node->origin_mem_region = origin_mem_region;
	new_node->handle = args.g_handle;
	new_node->mem_region = mem_region;
	new_node->mem_size = mem_size;
	new_node->meta_data = (void *)new_node;

	list_head = ffa_mem_list_lock();
	prev_node = list_head->prev;
	current_node = &new_node->list;
	current_node->prev = prev_node;
	current_node->next = list_head;
	prev_node->next = current_node;
	list_head->prev = current_node;
	ffa_mem_list_unlock();

	for (i = 0; i < number_of_vm; i++) {
		FFA_INFO("Sharing memory with SP %#X with handle=%lx %s:%d\n",
			 *(vmids + i), new_node->handle, __FILE__, __LINE__);
	}

	ffa_mem_list_dump();

	kfree(pages);
	return 0;

free_mem_ret:
	kfree(pages);
	kfree(origin_mem_region);
free_node_ret:
	kfree(new_node);

	return retval;
}

/**
 * @brief
 *
 * @return int
 */
static int ffa_memory_send_write(struct args *args, void * __user user_args)
{
	uint32_t mem_size, nr_pages, number_of_vm;
	int ret, i;
	unsigned short *vmids;
	bool share, involve_sp, mtk_retrieve;
	unsigned long ffa_memory_handle;
	void *ffa_memory_ptr = NULL;
	unsigned short vm_arr[VM_NUMBER_MAX] = {0};
	unsigned char *ptr;

	share = !!args->arg[1];
	involve_sp = !!args->arg[2];
	mtk_retrieve = !!args->arg[3];
	FFA_INFO("%s %d share=%d sp=%d retrieve=%d size=%llu\n", __func__, __LINE__, share,
			involve_sp, mtk_retrieve, args->arg[4]);

	vmids = (unsigned short *)args->arg[0];
	number_of_vm = (args->arg[4] >> 32) & 0x0FFFFFFFF;
	mem_size = args->arg[4] & 0x0FFFFFFFF;

	if ((number_of_vm > VM_NUMBER_MAX) || !number_of_vm || !vmids)
		return -EINVAL;
	if (mem_size) {
		mem_size = PAGE_ALIGN(mem_size);
		nr_pages = mem_size / PAGE_SIZE;
	} else {
		FFA_ERR("memory size is invalid : %d\n", mem_size);
		return -EINVAL;
	}

	ret = copy_from_user(vm_arr, vmids, number_of_vm * sizeof(unsigned short));
	if (ret) {
		FFA_ERR("copy_from_user error with ret=%d\n", ret);
		return ret;
	}

	for (i = 0; i < number_of_vm; i++)
		FFA_INFO("%s %d vmid=%d mem_size=%d\n", __func__, __LINE__, vm_arr[i], mem_size);

	ret = memory_send(share, involve_sp, mtk_retrieve, mem_size,
						nr_pages, (char **)&ffa_memory_ptr,
						&ffa_memory_handle, vm_arr, number_of_vm);
	if (ret) {
		FFA_ERR("memory_send error with ret=%d\n", ret);
		return ret;
	}

	ptr = ffa_memory_ptr;
	for (i = 0; i < mem_size; i++)
		ptr[i] = i / 4096;

	args->arg[4] = ffa_memory_handle;
	ret = copy_to_user(user_args, args, sizeof(struct args));
	if (ret) {
		FFA_ERR("copy_to_user error with ret=%d\n", ret);
		return ret;
	}

	return 0;
}

/**
 * @brief Test memory share via echo TeeServiceCall
 *
 * @return int
 */
static int ffa_memory_share_read(struct args *args)
{
	uint32_t i;
	unsigned char *ptr;

	int found = 0;
	uint32_t mem_size;
	unsigned long long handle;
	struct ffa_mem_list_head *list_head;
	struct ffa_mem_list_head *iterator;
	struct ffa_mem_record_table *mem_record;

	handle = args->arg[1];

	list_head = ffa_mem_list_lock();

	for (iterator = list_head->next; iterator != list_head; iterator = iterator->next) {
		mem_record = list_entry(iterator, struct ffa_mem_record_table, list);
		if (mem_record->handle == handle) {
			found = 1;
			break;
		}
	}
	ffa_mem_list_unlock();

	if (found) {
		ptr = (unsigned char *)mem_record->mem_region;
		mem_size = mem_record->mem_size;
	} else {
		return -1;
	}

	if (!ptr) {
		FFA_ERR("share memory is not yet configured\n");
		return -EFAULT;
	}

	for (i = 0; i < mem_size; i++) {
		if (ptr[i] != 'B') {
			FFA_ERR("%s %d Test failed on ptr[%u], expect=%x, real=%x\n",
				   __func__, __LINE__, i, 'B', ptr[i]);
			return -EFAULT;
		}
	}
	FFA_INFO("%s test passed!\n", __func__);
	return 0;
}

/**
 * @brief Test memory share via echo TeeServiceCall
 *
 * @param arg  output: userspace address for memory handle
 * @return int
 */
static int gz_ffa_memory_share(struct args *args, void * __user user_args)
{
	uint32_t mem_size, nr_pages, number_of_vm;
	int ret, i;
	unsigned short *vmids;
	bool share, involve_sp, mtk_retrieve;
	unsigned long ffa_memory_handle;
	void *ffa_memory_ptr = NULL;
	unsigned short vm_arr[VM_NUMBER_MAX] = {0};

	share = !!args->arg[1];
	involve_sp = !!args->arg[2];
	mtk_retrieve = !!args->arg[3];
	FFA_INFO("%s %d share=%d sp=%d retrieve=%d\n", __func__, __LINE__, share,
			involve_sp, mtk_retrieve);

	vmids = (unsigned short *)args->arg[0];
	number_of_vm = (args->arg[4] >> 32) & 0x0FFFFFFFF;
	mem_size = args->arg[4] & 0x0FFFFFFFF;

	if ((number_of_vm > VM_NUMBER_MAX) || !number_of_vm || !vmids)
		return -EINVAL;
	if (mem_size) {
		mem_size = PAGE_ALIGN(mem_size);
		nr_pages = mem_size / PAGE_SIZE;
	} else {
		FFA_ERR("memory size is invalid : %d\n", mem_size);
		return -EINVAL;
	}

	ret = copy_from_user(vm_arr, vmids, number_of_vm * sizeof(unsigned short));
	if (ret) {
		FFA_ERR("copy_from_user error with ret=%d\n", ret);
		return ret;
	}

	for (i = 0; i < number_of_vm; i++)
		FFA_INFO("%s %d vmid=%d mem_size=%d\n", __func__, __LINE__, vm_arr[i], mem_size);

	ret = memory_send(share, involve_sp, mtk_retrieve, mem_size,
						nr_pages, (char **)&ffa_memory_ptr,
						&ffa_memory_handle, vm_arr, number_of_vm);

	if (ret) {
		FFA_ERR("memory_send error with ret=%d\n", ret);
		return ret;
	}
	args->arg[4] = ffa_memory_handle;

	ret = copy_to_user(user_args, args, sizeof(struct args));

	if (ret) {
		FFA_ERR("copy_to_user error with ret=%d\n", ret);
		return ret;
	}
	return 0;
}

static int gz_ffa_memory_reclaim(struct args *args)
{
	int ret, found = 0;
	uint32_t flags;
	unsigned long long handle;
	struct ffa_mem_list_head *list_head;
	struct ffa_mem_list_head *iterator;
	struct ffa_mem_record_table *mem_record;
	struct ffa_mem_list_head *prev_node, *next_node;

	flags = args->arg[1];
	handle = args->arg[2];

	if (flags & ~(0x80000001)) {
		FFA_ERR("Error specified flags=%x\n", flags);
		return -EINVAL;
	}

	ret = g_ffa_ops->mem_ops->memory_reclaim(handle, flags);
	if (ret) {
		FFA_ERR("memory_reclaim failed ret=%d\n", ret);
		return ret;
	}
	list_head = ffa_mem_list_lock();

	for (iterator = list_head->next; iterator != list_head; iterator = iterator->next) {
		mem_record = list_entry(iterator, struct ffa_mem_record_table, list);
		if (mem_record->handle == handle) {
			found = 1;
			break;
		}
	}
	ffa_mem_list_unlock();

	if (found) {
		kfree(mem_record->origin_mem_region);

		prev_node = iterator->prev;
		next_node = iterator->next;

		prev_node->next = next_node;
		next_node->prev = prev_node;

		kfree(mem_record->meta_data);

		ffa_mem_list_dump();
		return 0;
	}

	ffa_mem_list_dump();
	return -10;
	//TODO: free the allocated memory which is specified by handle.


	/*
	 *	if (flags & FFA_MEM_CLEAR) { * verify data only if clear flag is specified *
	 *		for (i = 0; i < share_memory_size; i++) {
	 *			* Only even pages are set to share, so only even pages are cleared,
	 *			 * while the odd pages is kept the value of CA's *
	 *			if (i / 4096 % 2 == 0)
	 *				v = 0x00;
	 *			else
	 *				v = i / 4096;
	 *			if (ptr[i] != v) {
	 *				FFA_ERR("%s %d Test failed on ptr[%u],
	 *						expect=%x, real=%x\n", __func__,
	 *						__LINE__, i, v, ptr[i]);
	 *				ret = -EFAULT;
	 *				break;
	 *			}
	 *		}
	 *	}
	 */
}

#define VM_UUID_LENGTH 36
static int gz_ffa_partition_info(struct args *args)
{
	int ret = 0;
	char target_uuid[VM_UUID_LENGTH + 1] = {0};
	struct ffa_partition_info *buffer = kzalloc(PAGE_SIZE, GFP_KERNEL);

	memcpy(target_uuid, args, VM_UUID_LENGTH);

	ret = g_ffa_ops->info_ops->partition_info_get((const char *)target_uuid, buffer);

//FIXME: make this part more logical
	if (ret != 0) {
		ret = -1;
		FFA_ERR("No partition found\n");
	} else {
		ret = buffer->id;
		FFA_INFO("return partition id: %d\n", ret);
	}

	kfree(buffer);
	return ret; // error return negative
}

static int gz_ffa_message_send(struct args *args, void * __user user_args)
{
	int ret, i;
	struct ffa_send_direct_data data = {};
	uint32_t number_of_vm;
	unsigned short vmids[VM_NUMBER_MAX] = {0};

	if (!g_ffa_dev)
		return -ENODEV;

	number_of_vm = (args->arg[0] >> 32) & 0x0FFFFFFFF;
	if (number_of_vm > VM_NUMBER_MAX || !number_of_vm)
		return -EINVAL;
	ret = copy_from_user(vmids, (void *)args->arg[4], number_of_vm * sizeof(unsigned short));
	if (ret) {
		FFA_ERR("copy_from_user error with ret=%d\n", ret);
		return ret;
	}

	data.data0 = (args->arg[0] & 0x0FFFFFFFF); // request command
	data.data1 = args->arg[1]; // data
	data.data2 = args->arg[2];
	data.data3 = args->arg[3];

	for (i = 0; i < number_of_vm; i++) {
		ret = ffa_send_to_ha(&data, vmids[i]);
		if (ret) {
			FFA_ERR("ffa_send_to_ha error with ret=%d\n", ret);
			return ret;
		}
	}

	FFA_INFO("Responsed data: 0x%lx 0x%lx 0x%lx 0x%lx 0x%lx\n", data.data0, data.data1,
			 data.data2, data.data3, data.data4);

	ret = copy_to_user(user_args, &data, sizeof(struct args));
	if (ret) {
		FFA_ERR("copy_to_user error with ret=%d\n", ret);
		return ret;
	}

	return 0;
}

static TZ_RESULT DMAFd2MemHandle(int buf_fd,
	unsigned long long *mem_handle)
{
	struct dma_buf *dbuf = NULL;
	uint64_t secure_handle = 0;

	dbuf = dma_buf_get(buf_fd);
	if (!dbuf || IS_ERR(dbuf)) {
		FFA_INFO("dma_buf_get error\n");
		return TZ_RESULT_ERROR_ITEM_NOT_FOUND;
	}

	secure_handle = dmabuf_to_secure_handle(dbuf);
	if (!secure_handle) {
		FFA_INFO("dmabuf_to_secure_handle failed!\n");
		*mem_handle = 0;
		return TZ_RESULT_ERROR_GENERIC;
	}

	dma_buf_put(dbuf);
	*mem_handle = secure_handle;
	return TZ_RESULT_SUCCESS;
}

static int gz_ffa_get_chm_handle(struct args *args, void * __user user_args)
{
	int ret;
	int buf_fd;
	uint64_t handle;

	buf_fd = (int)args->arg[0];

	ret = DMAFd2MemHandle(buf_fd, &handle);
	if (ret != TZ_RESULT_SUCCESS) {
		FFA_INFO("[%s]DMAFd2MemHandle fail(0x%x)\n", __func__, ret);
		return ret;
	}

	args->arg[1] = handle;
	ret = copy_to_user(user_args, args, sizeof(struct args));
	if (ret)
		FFA_INFO("[%s]copy_to_user fail(0x%x)\n", __func__, ret);

	return ret;
}

static long ff_a_gz_ioctl(struct file *fd, unsigned int cmd, unsigned long user_args)
{
	long ret;
	struct args args = {};
	void * __user user_arg = (void *)user_args;

	FFA_INFO("cmd = %d\n", cmd);

	if (user_arg != NULL) {
		if (copy_from_user(&args, user_arg, sizeof(args)) != 0)
			return -EFAULT;
	}

	switch (cmd) {
	case GZ_FFA_IS_ENABLED:
		ret = ffa_enabled;
		break;
	case GZ_FF_A_MEMORY_SEND:
		ret = gz_ffa_memory_share(&args, user_arg);
		break;
	case GZ_FF_A_MEM_RECLAIM:
		ret = gz_ffa_memory_reclaim(&args);
		break;
	case GZ_FF_A_PARTITION_INFO_GET:
		ret = gz_ffa_partition_info(&args);
		break;
	case GZ_FF_A_MESSAGE_SEND:
		ret = gz_ffa_message_send(&args, user_arg);
		break;
	case GZ_FF_A_GET_CHM_HANDLE:
		ret = gz_ffa_get_chm_handle(&args, user_arg);
		break;

	case FF_A_DRV_MEMORY_SEND:
		ret = ffa_memory_send_write(&args, user_arg);
		break;
	case FF_A_DRV_READ_BACK:
		ret = ffa_memory_share_read(&args);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	if (ret == -EINVAL)
		FFA_ERR("Invalid command for %s: %ld\n", __func__, ret);

	return ret;
}

const struct file_operations fops = {
	.unlocked_ioctl = ff_a_gz_ioctl,
};

static int device_driver_init(void)
{
	struct class *cl;
	int rc;

	cl = class_create(THIS_MODULE, GZ_FFA_DEV_NAME);
	if (IS_ERR(cl))
		return PTR_ERR(cl);

	/* Create char device. */
	rc = register_chrdev(0, GZ_FFA_DEV_NAME, &fops);

	/* Create device file in the /dev directory. */
	device_create(cl, NULL, MKDEV(rc, 0), NULL, GZ_FFA_DEV_NAME);

	// comment this line to avoid the compile error
	// FFA_INFO("%s %s %s %s\n", __FILE__, __func__, __DATE__, __TIME__);

	return rc;
}

static int gz_ffa_driver_probe(struct ffa_device *ffa_dev)
{
	FFA_INFO("Initialising driver for Partition: 0x%x\n", ffa_dev->vm_id);

	/* Check if we've discovered the Logical SP ID. */
	FFA_INFO("Got ffa_dev vmid = %d\n", ffa_dev->vm_id);
	g_ffa_dev = ffa_dev;

	if (g_ffa_dev) {
		g_ffa_ops = g_ffa_dev->ops;
		if (IS_ERR_OR_NULL(g_ffa_ops)) {
			FFA_ERR("Failed to obtain FFA ops %s:%d\n", __FILE__, __LINE__);
			return -1;
		}
	} else {
		FFA_ERR("Failed to obtain FFA device %s:%d\n", __FILE__, __LINE__);
		return -1;
	}


	/* Only do this setup once */
	if (!device_driver_initalized) {
		device_driver_init();
		device_driver_initalized = true;
	}

	ffa_drv_manager = &fm_head;

	ffa_drv_manager->next = ffa_drv_manager;
	ffa_drv_manager->prev = ffa_drv_manager;

	spin_lock_init(&ffa_mem_lock);

	ffa_enabled = true;
	FFA_INFO("FF-A test module init finalized\n");
	return 0;
}

static const struct ffa_device_id gz_ffa_device_id[] = {
	/* Echo server UUID  SP UUID - BE Format */
	{ UUID_INIT(0x11001100, 0x1100, 0x3322, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77) },
	{ }
};

struct ffa_driver gz_ffa_driver = {
	.name = GZ_FFA_DEV_NAME,
	.probe = gz_ffa_driver_probe,
	.id_table = gz_ffa_device_id,
};

struct ffa_driver *gz_get_ffa_dev(void)
{
	return &gz_ffa_driver;
}

MODULE_LICENSE("GPL");
