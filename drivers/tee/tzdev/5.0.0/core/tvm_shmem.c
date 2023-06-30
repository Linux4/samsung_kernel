/*
 * Copyright (C) 2022, Samsung Electronics Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/mm.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <soc/qcom/secure_buffer.h>

#include "tzdev_internal.h"
#include "core/log.h"
#include "core/tvm_shmem.h"

struct hv_operations {
	int (*get_pvm_vmid)(void);
	int (*get_tvm_vmid)(void);
	int (*notify_tvm_mem_share)(uint32_t mem_handle);
	int (*share_memory)(struct sg_table *sgt, uint32_t *mem_handle);
};

#ifdef CONFIG_TZDEV_GUNYAH

#include "core/gunyah_platform.h"
static const struct hv_operations hv_ops = {
	.get_pvm_vmid = gunyah_get_pvm_vmid,
	.get_tvm_vmid = gunyah_get_tvm_vmid,
	.notify_tvm_mem_share = gunyah_mem_notify,
	.share_memory = gunyah_share_memory,
};

#else /* !CONFIG_TZDEV_GUNYAH */

/* redefine struct hv_operations for another hypervisor if needed */

#endif /* CONFIG_TZDEV_GUNYAH */

/*
 * Memory regions are assigned to other VMs by batches.
 * One batch can contain max 32 SG entries or max size 2 Mb.
 * One batch - one memparcel - one mem handle.
 * See: get_batches_from_sgl in drivers/soc/qcom/secure_buffer.c for details.
 */
#define BATCH_MAX_SIZE SZ_1M
#define BATCH_MAX_SECTIONS 32

static RADIX_TREE(sharing_tree, GFP_KERNEL);
static RADIX_TREE(releasing_tree, GFP_KERNEL);
static DEFINE_SPINLOCK(shmem_lock);
/* only one memory notification at the same time to avoid race in TVM */
static DEFINE_MUTEX(notify_lock);

enum shmem_state {
	ERROR,
	INIT,
	SHARING,
	ACCEPTED,
	RELEASING,
	RELEASED
};

struct shmem_desc {
	uint32_t mem_handle;
	enum shmem_state state;
	struct sg_table *sgt;
	struct completion done;
};

static int shmem_desc_init(struct shmem_desc *desc, struct page **pages, unsigned int num_pages)
{
	int ret;

	desc->sgt = kzalloc(sizeof(*desc->sgt), GFP_KERNEL);
	if (!desc->sgt) {
		log_error(tzdev_platform, "Failed to allocate sg table\n");
		return -ENOMEM;
	}

	ret = sg_alloc_table_from_pages(desc->sgt, pages, num_pages, 0, num_pages << PAGE_SHIFT,
									GFP_KERNEL);
	if (ret) {
		log_error(tzdev_platform, "Failed to fill sg table\n");
		kfree(desc->sgt);
		desc->sgt = NULL;
		return ret;
	}

	desc->state = INIT;
	init_completion(&desc->done);

	return 0;
}

static int shmem_desc_wait_state(struct shmem_desc *desc, enum shmem_state state)
{
	int ret;
	long timeleft;

	timeleft = wait_for_completion_interruptible_timeout(&desc->done, HZ);

	spin_lock(&shmem_lock);
	ret = (timeleft > 0 && desc->state == state) ? 0 : -EINVAL;
	spin_unlock(&shmem_lock);

	return ret;
}

static int tvm_shmem_assign_mem(struct shmem_desc *desc)
{
	int ret = 0;
	u32 src_vmlist;
	int dst_vmlist[2];
	int dst_perms[2];
	struct scatterlist *s;
	int i;

	/* share memory between PVM and TVM */
	src_vmlist = hv_ops.get_pvm_vmid();
	dst_vmlist[0] = hv_ops.get_pvm_vmid();
	dst_vmlist[1] = hv_ops.get_tvm_vmid();
	dst_perms[0] = PERM_READ | PERM_WRITE;
	dst_perms[1] = PERM_READ | PERM_WRITE;

	ret = hyp_assign_table(desc->sgt, &src_vmlist, 1, dst_vmlist, dst_perms, 2);
	if (ret) {
		log_error(tzdev_platform, "Failed to assign memory\n");
		return ret;
	}

	log_debug(tzdev_platform, "mem handle: %u\n", desc->mem_handle);
	for_each_sg(desc->sgt->sgl, s, desc->sgt->nents, i) {
		log_debug(tzdev_platform, "sgl[%d]: phys: %#llx; len: %u\n",
				i, sg_phys(s), s->length);
	}

	return 0;
}

static int tvm_shmem_unassign_mem(struct shmem_desc *desc)
{
	int ret = 0;
	u32 src_vmlist[2];
	int dst_vmlist;
	int dst_perms;
	struct scatterlist *s;
	int i;

	/* Hyp-assign from TVM back */
	src_vmlist[0] = hv_ops.get_pvm_vmid();
	src_vmlist[1] = hv_ops.get_tvm_vmid();
	dst_vmlist = hv_ops.get_pvm_vmid();
	dst_perms = PERM_READ | PERM_WRITE | PERM_EXEC;

	ret = gh_rm_mem_reclaim(desc->mem_handle, 0);
	if (ret) {
		log_error(tzdev_platform, "Gunyah mem reclaim failed: %d\n", ret);
		return ret;
	}

	ret = hyp_assign_table(desc->sgt, src_vmlist, 2, &dst_vmlist, &dst_perms, 1);
	if (ret) {
		log_error(tzdev_platform, "Failed to unassign memory\n");
		return ret;
	}

	log_debug(tzdev_platform, "mem handle: %u\n", desc->mem_handle);
	for_each_sg(desc->sgt->sgl, s, desc->sgt->nents, i) {
		log_debug(tzdev_platform, "sgl[%d]: phys: %#llx; len: %u\n",
				i, sg_phys(s), s->length);
	}

	return 0;
}

int tvm_shmem_release_batch(uint32_t mem_handle)
{
	int ret;
	struct shmem_desc *desc;

	spin_lock(&shmem_lock);
	desc = radix_tree_lookup(&sharing_tree, mem_handle);
	if (!desc) {
		log_error(tzdev_platform, "Descriptor for shmem was not found for handle: %u\n",
				mem_handle);
		spin_unlock(&shmem_lock);
		return -EINVAL;
	}
	log_debug(tzdev_platform, "mem handle: %u\n", desc->mem_handle);

	if (desc->state == ACCEPTED) {
		desc->state = RELEASING;
	} else {
		log_error(tzdev_platform, "Shared mem descriptor should be in ACCEPTED state\n");
		spin_unlock(&shmem_lock);
		return -EINVAL;
	}

	radix_tree_delete(&sharing_tree, desc->mem_handle);

	if (!radix_tree_lookup(&releasing_tree, desc->mem_handle)) {
		radix_tree_insert(&releasing_tree, desc->mem_handle, desc);
	} else {
		log_error(tzdev_platform, "Releasing tree is not empty for %u\n", desc->mem_handle);
		spin_unlock(&shmem_lock);
		return -EBUSY;
	}
	spin_unlock(&shmem_lock);

	mutex_lock(&notify_lock);
	ret = tzdev_smc_tvm_shmem_rls(desc->mem_handle);
	if (ret) {
		mutex_unlock(&notify_lock);
		log_error(tzdev_platform, "tvm shmem release SMC error: %d\n", ret);
		return ret;
	}

	ret = shmem_desc_wait_state(desc, RELEASED);
	if (ret) {
		mutex_unlock(&notify_lock);
		log_error(tzdev_platform, "Not correct state. Should be RELEASED\n");
		return ret;
	}
	mutex_unlock(&notify_lock);

	spin_lock(&shmem_lock);
	radix_tree_delete(&releasing_tree, desc->mem_handle);
	spin_unlock(&shmem_lock);

	ret = tvm_shmem_unassign_mem(desc);
	if (ret) {
		log_error(tzdev_platform, "Failed to un-assign memory\n");
	}

	sg_free_table(desc->sgt);
	kfree(desc);

	return ret;
}

int tvm_shmem_release(struct tvm_shmem_handles *handles)
{
	unsigned int i;
	int ret = 0;

	if (!handles->ids) {
		log_error(tzdev_platform, "handles->ids is empty\n");
		return -EINVAL;
	}

	log_debug(tzdev_platform, "releasing handles->ids = %p", handles->ids);

	for (i = 0; i < handles->count; i++) {
		if (!handles->ids[i]) {
			log_error(tzdev_platform, "No handles->ids[%u]\n", i);
			ret = -EINVAL;
			break;
		}

		/* We can't use memory anymore in the case of error */
		if (tvm_shmem_release_batch(handles->ids[i])) {
			ret = -EADDRNOTAVAIL;
			break;
		}
	}

	kfree(handles->ids);
	handles->ids = NULL;

	return ret;
}

static int tvm_shmem_notify_and_wait(struct shmem_desc *desc)
{
	int ret;

	mutex_lock(&notify_lock);
	ret = hv_ops.notify_tvm_mem_share(desc->mem_handle);
	if (ret < 0) {
		log_error(tzdev_platform, "Failed to notify TVM about memory sharing: %d\n", ret);
		mutex_unlock(&notify_lock);
		return ret;
	}

	ret = shmem_desc_wait_state(desc, ACCEPTED);
	if (ret) {
		log_error(tzdev_platform, "Not correct state. Should be ACCEPTED\n");
	}
	mutex_unlock(&notify_lock);

	return ret;
}

int tvm_shmem_share_batch(struct page **pages, unsigned int num_pages, uint32_t *mem_handle)
{
	int ret;
	unsigned int i;
	struct shmem_desc *desc;

	for (i = 0; i < num_pages; i++)
		log_debug(tzdev_platform, "pfn[%d]: %#lx\n", i, page_to_pfn(pages[i]));

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc) {
		log_error(tzdev_platform, "Failed to allocate shmem desc\n");
		return -ENOMEM;
	}

	ret = shmem_desc_init(desc, pages, num_pages);
	if (ret) {
		log_error(tzdev_platform, "Failed to fill sg table\n");
		goto free_desc;
	}

	ret = tvm_shmem_assign_mem(desc);
	if (ret) {
		log_error(tzdev_platform, "Failed to assign memory\n");
		goto free_desc;
	}

	ret = hv_ops.share_memory(desc->sgt, &desc->mem_handle);
	if (ret) {
		log_error(tzdev_platform, "Failed to get mem handle\n");
		goto unassign_mem;
	}

	spin_lock(&shmem_lock);
	if (!radix_tree_lookup(&sharing_tree, desc->mem_handle)) {
		radix_tree_insert(&sharing_tree, desc->mem_handle, desc);
	} else {
		log_error(tzdev_platform, "Sharing tree is not empty for %u\n", desc->mem_handle);
		spin_unlock(&shmem_lock);
		ret = -EBUSY;
		goto unassign_mem;
	}
	desc->state = SHARING;
	spin_unlock(&shmem_lock);

	ret = tvm_shmem_notify_and_wait(desc);
	if (ret) {
		log_error(tzdev_platform, "Failed to notify TVM\n");
		goto delete_from_tree;
	}

	*mem_handle = desc->mem_handle;
	log_debug(tzdev_platform, "mem handle: %u\n", desc->mem_handle);

	return 0;

delete_from_tree:
	spin_lock(&shmem_lock);
	radix_tree_delete(&sharing_tree, desc->mem_handle);
	spin_unlock(&shmem_lock);
unassign_mem:
	tvm_shmem_unassign_mem(desc);
free_desc:
	if (desc->sgt)
		sg_free_table(desc->sgt);
	kfree(desc);

	return ret;
}

static unsigned int get_chunks(struct page **pages, unsigned int num_pages,
		unsigned int *chunks_len)
{
	unsigned int i;
	unsigned int chunks_cnt, len;

	len = 0;
	chunks_cnt = 1;
	for (i = 1; i < num_pages; i++) {
		len += PAGE_SIZE;
		if (len == BATCH_MAX_SIZE ||
				page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1) {
			if (chunks_len)
				chunks_len[chunks_cnt - 1] = len;
			len = 0;
			chunks_cnt++;
		}
	}
	if (chunks_len)
		chunks_len[chunks_cnt - 1] = len + PAGE_SIZE;

	return chunks_cnt;
}

static unsigned int get_batches(unsigned int chunks_cnt, unsigned int *chunks_len, unsigned int *batches_len)
{
	unsigned int i;
	unsigned int batches_cnt;
	unsigned int chunks_in_batch;
	unsigned int batch_size;

	i = 0;
	batches_cnt = 0;
	while (i < chunks_cnt) {
		batch_size = 0;
		chunks_in_batch = 0;
		do {
			batch_size += chunks_len[i];
			chunks_in_batch++;
			i++;
		} while (i < chunks_cnt && chunks_in_batch < BATCH_MAX_SECTIONS &&
				chunks_len[i] + batch_size < BATCH_MAX_SIZE);

		if (batches_len)
			batches_len[batches_cnt] = batch_size;

		batches_cnt++;
	}

	return batches_cnt;
}

#define MEM_HANDLES_PER_PAGE	(PAGE_SIZE / sizeof(uint32_t) - 1)

int tvm_shmem_share(struct page **pages, unsigned int num_pages, struct tvm_shmem_handles *handles)
{
	int ret = 0;
	int rel_ret = 0;
	unsigned int i;

	unsigned int batches_cnt;
	unsigned int *batches_len;
	unsigned int chunks_cnt;
	unsigned int *chunks_len;
	unsigned int cur_page;
	unsigned int pages_in_batch;

	if (!pages || !handles || !num_pages)
		return -EINVAL;

	/* get count of contiguous chunks */
	chunks_cnt = get_chunks(pages, num_pages, NULL);

	chunks_len = kcalloc(chunks_cnt, sizeof(*chunks_len), GFP_KERNEL);
	if (!chunks_len)
		return -ENOMEM;

	/* fill lengths of contiguous chunks */
	get_chunks(pages, num_pages, chunks_len);

	/* get count of batches (32 sg entries or max 1 Mb in set of sg entries) */
	batches_cnt = get_batches(chunks_cnt, chunks_len, NULL);
	if (batches_cnt > MEM_HANDLES_PER_PAGE) {
		log_error(tzdev_platform, "Wrong count of batches\n");
		ret = -EINVAL;
		goto free_chunks;
	}

	batches_len = kcalloc(batches_cnt, sizeof(*batches_len), GFP_KERNEL);
	if (!batches_len) {
		ret = -ENOMEM;
		goto free_chunks;
	}

	/* fill lengths of batches */
	get_batches(chunks_cnt, chunks_len, batches_len);

	handles->count = batches_cnt;

	handles->ids = kcalloc(handles->count, sizeof(uint32_t), GFP_KERNEL);
	if (!handles->ids) {
		ret = -ENOMEM;
		goto free_batches;
	}

	log_debug(tzdev_platform, "sharing handles->ids = %p", handles->ids);

	cur_page = 0;
	for (i = 0; i < handles->count; i++) {
		pages_in_batch = batches_len[i] / PAGE_SIZE;
		ret = tvm_shmem_share_batch(&pages[cur_page], pages_in_batch, &handles->ids[i]);
		if (ret) {
			log_error(tzdev_platform, "Failed to share mem\n");
			rel_ret = tvm_shmem_release(handles);
			break;
		}
		cur_page += pages_in_batch;
	}

free_batches:
	kfree(batches_len);
free_chunks:
	kfree(chunks_len);

	/*
	 * We can't use the memory anymore if return value -EADDRNOTAVAIL.
	 * See comment for hyp_assign_table function
	 */
	return (rel_ret == -EADDRNOTAVAIL) ? rel_ret : ret;
}

void tvm_handle_accepted(uint32_t mem_handle)
{
	struct shmem_desc *desc;

	log_debug(tzdev_platform, "Shared memory was accepted for handle: %u\n", mem_handle);

	spin_lock(&shmem_lock);
	desc = radix_tree_lookup(&sharing_tree, mem_handle);
	if (!desc) {
		log_error(tzdev_platform, "Descriptor for shmem was not found for handle: %u\n", mem_handle);
		spin_unlock(&shmem_lock);
		return;
	}

	if (desc->state == ACCEPTED) {
		log_error(tzdev_platform, "Memory handle %u is in ACCEPTED state\n", mem_handle);
		spin_unlock(&shmem_lock);
		return;
	}

	if (desc->state == SHARING)
		desc->state = ACCEPTED;
	else
		desc->state = ERROR;

	complete(&desc->done);

	spin_unlock(&shmem_lock);
}

void tvm_handle_released(uint32_t mem_handle)
{
	struct shmem_desc *desc;

	log_debug(tzdev_platform, "Shared memory was released for handle: %u\n", mem_handle);

	spin_lock(&shmem_lock);
	desc = radix_tree_lookup(&releasing_tree, mem_handle);
	if (!desc) {
		log_error(tzdev_platform, "Descriptor for shmem was not found for handle: %u\n", mem_handle);
		spin_unlock(&shmem_lock);
		return;
	}

	if (desc->state == RELEASED) {
		log_error(tzdev_platform, "Memory handle %u is in RELEASED state\n", mem_handle);
		spin_unlock(&shmem_lock);
		return;
	}

	if (desc->state == RELEASING)
		desc->state = RELEASED;
	else
		desc->state = ERROR;

	complete(&desc->done);

	spin_unlock(&shmem_lock);
}
