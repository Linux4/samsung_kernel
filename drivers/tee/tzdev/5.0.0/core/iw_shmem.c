/*
 * Copyright (c) 2021 Samsung Electronics Co., Ltd All Rights Reserved
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

#include <linux/idr.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include "core/cred.h"
#include "core/iw_events.h"
#include "core/iw_mem.h"
#include "core/iw_shmem.h"
#include "core/log.h"
#include "core/smc_channel.h"
#include "core/sysdep.h"
#include "core/wait.h"

enum iw_shmem_state {
	CREATED,
	PUBLISHED,
	RELEASED
};

struct tzdev_iw_shmem_base {
	uint64_t id;
	uint64_t flags;
	uint64_t release_event;
	struct tz_cred cred;
} __packed;

struct tzdev_iw_shmem {
	struct tzdev_iw_shmem_base base;
	struct tzdev_iw_mem *mem;
	enum iw_shmem_state state;
	struct kref kref;
	struct work_struct unpublish_work;
};

static DEFINE_IDR(tzdev_iw_shmem_map);
static DEFINE_MUTEX(tzdev_iw_shmem_map_lock);
static DECLARE_WAIT_QUEUE_HEAD(tzdev_iw_shmem_wq);

static void tzdev_iw_shmem_get(struct tzdev_iw_shmem *shmem)
{
	kref_get(&shmem->kref);
}

static void tzdev_iw_shmem_destroy(struct kref *kref)
{
	struct tzdev_iw_shmem *shmem = container_of(kref, struct tzdev_iw_shmem, kref);

	tzdev_iw_mem_destroy(shmem->mem);
	kfree(shmem);
}

static void tzdev_iw_shmem_put(struct tzdev_iw_shmem *shmem)
{
	kref_put(&shmem->kref, tzdev_iw_shmem_destroy);
}

static int tzdev_iw_shmem_map_add(struct tzdev_iw_shmem *shmem)
{
	int ret;

	tzdev_iw_shmem_get(shmem);

	mutex_lock(&tzdev_iw_shmem_map_lock);
	ret = idr_alloc(&tzdev_iw_shmem_map, shmem, 1, 0, GFP_KERNEL);
	mutex_unlock(&tzdev_iw_shmem_map_lock);

	if (ret < 0)
		tzdev_iw_shmem_put(shmem);
	else
		shmem->base.id = ret;
	return ret;
}

static struct tzdev_iw_shmem *tzdev_iw_shmem_map_find(unsigned int id)
{
	struct tzdev_iw_shmem *shmem;

	mutex_lock(&tzdev_iw_shmem_map_lock);
	shmem = idr_find(&tzdev_iw_shmem_map, id);
	if (shmem)
		tzdev_iw_shmem_get(shmem);
	mutex_unlock(&tzdev_iw_shmem_map_lock);
	return shmem;
}

static void tzdev_iw_shmem_map_remove(struct tzdev_iw_shmem *shmem)
{
	mutex_lock(&tzdev_iw_shmem_map_lock);
	idr_remove(&tzdev_iw_shmem_map, shmem->base.id);
	mutex_unlock(&tzdev_iw_shmem_map_lock);

	tzdev_iw_shmem_put(shmem);
}

static void tzdev_iw_shmem_unpublish(struct tzdev_iw_shmem *shmem)
{
	tzdev_iw_shmem_map_remove(shmem);
	tzdev_iw_events_unregister(shmem->base.release_event);
	tzdev_iw_shmem_put(shmem);
}

static void tzdev_iw_shmem_unpublish_handler(struct work_struct *work)
{
	struct tzdev_iw_shmem *shmem = container_of(work, struct tzdev_iw_shmem, unpublish_work);

	tzdev_iw_shmem_unpublish(shmem);
}

static struct tzdev_iw_shmem *tzdev_iw_shmem_create_common(struct tzdev_iw_mem *mem, struct tz_cred *cred,
		unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;

	shmem = kmalloc(sizeof(struct tzdev_iw_shmem), GFP_KERNEL);
	if (!shmem)
		return ERR_PTR(-ENOMEM);

	kref_init(&shmem->kref);
	shmem->base.flags = flags;
	shmem->state = CREATED;
	shmem->mem = mem;
	memcpy(&shmem->base.cred, cred, sizeof(struct tz_cred));
	INIT_WORK(&shmem->unpublish_work, tzdev_iw_shmem_unpublish_handler);

	return shmem;
}

static struct tzdev_iw_shmem *tzdev_iw_shmem_create(size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	struct tzdev_iw_mem *mem;
	struct tz_cred cred;
	int ret;

	ret = tz_format_cred(&cred, 1);
	if (ret) {
		log_error(tzdev_iw_shmem, "Failed to format credentials (error %d)\n", ret);
		goto out;
	}

	mem = tzdev_iw_mem_create(size);
	if (IS_ERR(mem)) {
		ret = PTR_ERR(mem);
		log_error(tzdev_iw_shmem, "Failed to create iw memory (error %d)\n", ret);
		goto out;
	}

	if (!tzdev_iw_mem_map(mem)) {
		ret = -ENOMEM;
		log_error(tzdev_iw_shmem, "Failed to map iw memory\n");
		goto destroy_mem;
	}

	shmem = tzdev_iw_shmem_create_common(mem, &cred, flags);
	if (IS_ERR(shmem)) {
		ret = PTR_ERR(shmem);
		log_error(tzdev_iw_shmem, "Failed to create iw shmem (error %d)\n", ret);
		goto unmap_mem;
	}

	return shmem;

unmap_mem:
	tzdev_iw_mem_unmap(mem);
destroy_mem:
	tzdev_iw_mem_destroy(mem);
out:
	return ERR_PTR(ret);
}

static struct tzdev_iw_shmem *tzdev_iw_shmem_create_exist(void *ptr, size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	struct tzdev_iw_mem *mem;
	struct tz_cred cred;
	int ret;

	ret = tz_format_cred(&cred, 1);
	if (ret) {
		log_error(tzdev_iw_shmem, "Failed to format credentials (error %d)\n", ret);
		goto out;
	}

	mem = tzdev_iw_mem_create_exist(ptr, size);
	if (IS_ERR(mem)) {
		ret = PTR_ERR(mem);
		log_error(tzdev_iw_shmem, "Failed to create iw memory (error %d)\n", ret);
		goto out;
	}

	shmem = tzdev_iw_shmem_create_common(mem, &cred, flags);
	if (IS_ERR(shmem)) {
		ret = PTR_ERR(shmem);
		log_error(tzdev_iw_shmem, "Failed to create iw shmem (error %d)\n", ret);
		goto destroy_mem;
	}

	return shmem;

destroy_mem:
	tzdev_iw_mem_destroy(mem);
out:
	return ERR_PTR(ret);
}

static struct tzdev_iw_shmem *tzdev_iw_shmem_create_user(size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	struct tzdev_iw_mem *mem;
	struct tz_cred cred;
	int ret;

	ret = tz_format_cred(&cred, 0);
	if (ret) {
		log_error(tzdev_iw_shmem, "Failed to format credentials (error %d)\n", ret);
		goto out;
	}

	mem = tzdev_iw_mem_create(size);
	if (IS_ERR(mem)) {
		ret = PTR_ERR(mem);
		log_error(tzdev_iw_shmem, "Failed to create iw memory (error %d)\n", ret);
		goto out;
	}

	shmem = tzdev_iw_shmem_create_common(mem, &cred, flags);
	if (IS_ERR(shmem)) {
		ret = PTR_ERR(shmem);
		log_error(tzdev_iw_shmem, "Failed to create iw shmem (error %d)\n", ret);
		goto destroy_mem;
	}

	return shmem;

destroy_mem:
	tzdev_iw_mem_destroy(mem);
out:
	return ERR_PTR(ret);
}

static void tzdev_iw_shmem_release_event_cb(void *data)
{
	struct tzdev_iw_shmem *shmem = data;

	BUG_ON(shmem->state != PUBLISHED);

	shmem->state = RELEASED;
	smp_wmb();
	wake_up(&tzdev_iw_shmem_wq);

	schedule_work(&shmem->unpublish_work);
}

static int tzdev_iw_shmem_publish(struct tzdev_iw_shmem *shmem)
{
	int ret;

	ret = tzdev_iw_shmem_map_add(shmem);
	if (ret < 0)
		return ret;

	tzdev_iw_shmem_get(shmem);
	ret = tzdev_iw_events_register(tzdev_iw_shmem_release_event_cb, shmem);
	if (ret < 0)
		goto shmem_put;

	shmem->base.release_event = ret;
	return 0;

shmem_put:
	tzdev_iw_shmem_put(shmem);
	tzdev_iw_shmem_map_remove(shmem);
	return ret;
}

static int tzdev_iw_shmem_pack(struct tzdev_iw_shmem *shmem, struct tzdev_smc_channel *ch)
{
	int ret;

	ret = tzdev_smc_channel_write(ch, &shmem->base, sizeof(struct tzdev_iw_shmem_base));
	if (ret)
		goto out;

	ret = tzdev_iw_mem_pack(shmem->mem, ch);
	if (ret)
		goto out;

	shmem->state = PUBLISHED;
out:
	return ret;
}

static void tzdev_iw_shmem_pack_cleanup(struct tzdev_iw_shmem *shmem)
{
	shmem->state = CREATED;
}

static int tzdev_iw_shmem_register_common(struct tzdev_iw_shmem *shmem)
{
	struct tzdev_smc_channel *ch;
	char comm[sizeof(current->comm)];
	int ret;

	ret = tzdev_iw_shmem_publish(shmem);
	if (ret < 0) {
		log_error(tzdev_iw_shmem, "%s failed to publish iw shmem (error %d)\n",
				get_task_comm(comm, current), ret);
		goto out;
	}

	ch = tzdev_smc_channel_acquire();
	if (IS_ERR(ch)) {
		log_error(tzdev_iw_shmem, "%s failed to acquire smc channel (error %d)\n",
				get_task_comm(comm, current), ret);
		goto shmem_unpublish;
	}

	ret = tzdev_iw_shmem_pack(shmem, ch);
	if (ret) {
		log_error(tzdev_iw_shmem, "%s failed to pack iw shmem (error %d)\n",
				get_task_comm(comm, current), ret);
		goto channel_release;
	}

	ret = tzdev_smc_iw_shmem_reg();
	if (ret) {
		log_error(tzdev_iw_shmem, "%s failed to register iw shmem (error %d)\n",
				get_task_comm(comm, current), ret);
		tzdev_iw_shmem_pack_cleanup(shmem);
		goto channel_release;
	}

	tzdev_smc_channel_release(ch);

	log_debug(tzdev_iw_shmem, "%s[%d pid] registered iw shmem (%p) { %u id, %zu size, %u flags, %u release event }\n",
			get_task_comm(comm, current), task_pid_nr(current), shmem,
			(unsigned int)shmem->base.id, tzdev_iw_mem_get_size(shmem->mem),
			(unsigned int)shmem->base.flags, (unsigned int)shmem->base.release_event);

	return shmem->base.id;

channel_release:
	tzdev_smc_channel_release(ch);
shmem_unpublish:
	tzdev_iw_shmem_unpublish(shmem);
out:
	return ret;
}

/**
 * tzdev_iw_shmem_register() - Register iw shared memory.
 * @ptr: pointer where to save mapped iw memory address
 * @size: size
 * @flags: TZDEV_IW_SHMEM_FLAG_* flags
 *
 * Allocates @size iw memory, maps into kernel space and registers iw shared memory over it.
 * Mapped address will be written into @ptr on success.
 *
 * Return: id on success and error(<0) on failure.
 */
int tzdev_iw_shmem_register(void **ptr, size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	int ret;

	shmem = tzdev_iw_shmem_create(size, flags);
	if (IS_ERR(shmem))
		return PTR_ERR(shmem);

	ret = tzdev_iw_shmem_register_common(shmem);
	if (ret >= 0)
		*ptr = tzdev_iw_mem_get_map_address(shmem->mem);

	tzdev_iw_shmem_put(shmem);
	return ret;
}

/**
 * tzdev_iw_shmem_register_exist() - Register iw shared memory over existing memory.
 * @ptr: address
 * @size: size
 * @flags: TZDEV_IW_SHMEM_FLAG_* flags
 *
 * Registers iw shared memory over kernel pages that cover [@ptr; @ptr+@size) kernel virtual memory.
 *
 * Return: id on success and error(<0) on failure.
 */
int tzdev_iw_shmem_register_exist(void *ptr, size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	int ret;

	shmem = tzdev_iw_shmem_create_exist(ptr, size, flags);
	if (IS_ERR(shmem))
		return PTR_ERR(shmem);

	ret = tzdev_iw_shmem_register_common(shmem);

	tzdev_iw_shmem_put(shmem);
	return ret;
}

/**
 * tzdev_iw_shmem_register_user() - Register iw shared memory.
 * @size: size
 * @flags: TZDEV_IW_SHMEM_FLAG_* flags
 *
 * Allocates @size iw memory and registers iw shared memory over it.
 *
 * Return: id on success and error(<0) on failure.
 */
int tzdev_iw_shmem_register_user(size_t size, unsigned int flags)
{
	struct tzdev_iw_shmem *shmem;
	int ret;

	shmem = tzdev_iw_shmem_create_user(size, flags);
	if (IS_ERR(shmem))
		return PTR_ERR(shmem);

	ret = tzdev_iw_shmem_register_common(shmem);

	tzdev_iw_shmem_put(shmem);
	return ret;
}

/**
 * tzdev_iw_shmem_release() - Release iw shared memory.
 * @id: iw shared memory id
 *
 * Requests to release iw shared memory with @id id. If shared memory is created with
 * TZDEV_IW_SHMEM_FLAG_SYNC flag, the function will wait until SWd is released it.
 *
 * Before calling this function be sure that there are no mappings created via
 * tzdev_iw_shmem_map_user().
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_iw_shmem_release(unsigned int id)
{
	struct tzdev_iw_shmem *shmem;
	char comm[sizeof(current->comm)];
	int ret;

	shmem = tzdev_iw_shmem_map_find(id);
	if (!shmem) {
		log_error(tzdev_iw_shmem, "%s failed to find iw shmem { %u } (error %d)",
				get_task_comm(comm, current), id, (int)PTR_ERR(shmem));
		return -ENOENT;
	}

	log_debug(tzdev_iw_shmem, "%s requested to release iw shmem (%p) { %u id, %u sync }\n",
			get_task_comm(comm, current), shmem,
			(unsigned int)shmem->base.id, (unsigned int)shmem->base.flags & TZDEV_IW_SHMEM_FLAG_SYNC);

	BUG_ON(shmem->base.id != id);

	ret = tzdev_smc_iw_shmem_rls(id);
	if (ret) {
		log_error(tzdev_iw_shmem, "%s failed to release iw shmem (%p) { %u id }\n",
				get_task_comm(comm, current), shmem, (unsigned int)shmem->base.id);
		goto shmem_put;
	}

	if (shmem->base.flags & TZDEV_IW_SHMEM_FLAG_SYNC)
		wait_event_interruptible_nested(tzdev_iw_shmem_wq,
				shmem->state == RELEASED);

shmem_put:
	tzdev_iw_shmem_put(shmem);
	return ret;
}

/**
 * tzdev_iw_shmem_map_user() - Map iw shared memory into user vma.
 * @id: iw shared memory id
 * @vma: user vma
 *
 * Gets iw shared memory with @id id and inserts its pages into @vma user vma.
 *
 * Return: 0 on success and error on failure.
 */
int tzdev_iw_shmem_map_user(unsigned int id, struct vm_area_struct *vma)
{
	struct tzdev_iw_shmem *shmem;
	int ret;

	shmem = tzdev_iw_shmem_map_find(id);
	if (!shmem)
		return -ENOENT;

	ret = tzdev_iw_mem_map_user(shmem->mem, vma);

	tzdev_iw_shmem_put(shmem);
	return ret;
}
