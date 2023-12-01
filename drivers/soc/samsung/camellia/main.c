/*
 * Copyright (C) 2021 Samsung Electronics
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/version.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/dma-mapping.h>
#include <linux/workqueue.h>

#include <drivers/soc/samsung/strong/strong_mailbox_common.h>
#include <drivers/soc/samsung/strong/strong_mailbox_ree.h>
#include <drivers/soc/samsung/strong/strong_error.h>
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/exynos/debug-snapshot.h>
#endif

/*
 * Note: if macro for converting physical address to page is not defined
 * in the kernel itself, it is defined hereby. This is to avoid build errors
 * which are reported during builds for some architectures.
 */
#ifndef phys_to_page
#define phys_to_page(phys) (pfn_to_page(__phys_to_pfn(phys)))
#endif

#include "camellia.h"
#include "log.h"
#include "utils.h"
#include "data_path.h"
#include "control_path.h"
#include "pm.h"
#include "dump.h"
#include "mailbox.h"
#include "logsink.h"

MODULE_LICENSE("Dual BSD/GPL");

#define RAMDUMP_OFFSET 0xFC0000
#define RAMDUMP_SIZE 0x40000

struct camellia_reserved_memory revmem = { 0, 0 };

static const char device_name[] = "camellia_iwc";
struct camellia_device camellia_dev;

static struct miscdevice iwc_dev;

static void camellia_pm_work(struct work_struct *work)
{
	LOG_INFO("PM Timer Work");

	mutex_lock(&camellia_dev.lock);
	if (camellia_dev.state == CAMELLIA_STATE_ONLINE) {
		if (atomic_read(&camellia_dev.refcnt) <= 0
			&& camellia_dev.pm_timer_set == CAMELLIA_PM_TIMER_ON) {
			camellia_dev.state = CAMELLIA_STATE_DOWN;
			if (strong_power_off()) {
				LOG_ERR("PM Timer Callback - Faild to turn off");
				camellia_dev.state = CAMELLIA_STATE_ONLINE;
				mutex_unlock(&camellia_dev.lock);
				return;
			}
			camellia_dev.state = CAMELLIA_STATE_OFFLINE;
			camellia_dev.pm_timer_set = CAMELLIA_PM_TIMER_OFF;
		} else {
			LOG_DBG("PM Timer Callback - Set Timer Again");
		}
	}
	mutex_unlock(&camellia_dev.lock);
}

static void pm_timer_callback(struct timer_list *timer)
{
	LOG_INFO("PM Timer Callback");

	queue_work(camellia_dev.pm_wq, &(camellia_dev.pm_work));
}

static ssize_t camellia_read(struct file *file, char __user *buffer,
							 size_t cnt,
							 loff_t *pos)
{
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;
	int is_empty;

	LOG_ENTRY;

	if (file->f_flags & O_NONBLOCK) {
		is_empty = camellia_is_data_msg_queue_empty(client->peer_id);
		if (is_empty) {
			LOG_INFO("No data for non-blocking read");
			return -EAGAIN;
		}
	}

	return camellia_read_data_msg_timeout(client->peer_id, buffer, cnt,
										  client->timeout_jiffies);
}

static ssize_t camellia_write(struct file *file, const char __user *buffer,
							  size_t cnt, loff_t *pos)
{
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;
	struct camellia_data_msg *msg = NULL;
	size_t msg_len = 0;
	size_t sent_msg_len = 0;
	int ret = 0;

	LOG_ENTRY;

	if (cnt < sizeof(struct camellia_data_msg_header))
		return -EINVAL;

	msg_len = sizeof(struct camellia_msg_flag) + cnt;

	msg = kmalloc(msg_len, GFP_KERNEL);
	if (!msg) {
		LOG_ERR("Unable to allocate message buffer");
		return -ENOMEM;
	}

	if (copy_from_user(&msg->header, buffer, cnt)) {
		LOG_ERR("Unable to copy request msg buffer");
		ret = -EAGAIN;
		goto free_memory;
	}

	if (msg_len != msg->header.length +
		sizeof(struct camellia_msg_flag) +
		sizeof(struct camellia_data_msg_header)) {
		ret = -EINVAL;
		goto free_memory;
	}

	sent_msg_len = camellia_send_data_msg(client->peer_id, msg);
	if (sent_msg_len != msg_len) {
		LOG_ERR("sent msg leng is not matched, sent len = (%d)", sent_msg_len);
		if (sent_msg_len > 0)
			ret = sent_msg_len - sizeof(struct camellia_msg_flag);
		else
			ret = -EAGAIN;
	} else {
		ret = cnt;
	}

free_memory:
	kfree(msg);

	return ret;
}

unsigned int camellia_poll(struct file *file,
						   struct poll_table_struct *poll_tab)
{
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;

	LOG_ENTRY;

	return camellia_poll_wait_data_msg(client->peer_id, file, poll_tab);
}

void camellia_vma_open(struct vm_area_struct *vma)
{
	/* Does Nothing */
}

void camellia_vma_close(struct vm_area_struct *vma)
{
	struct camellia_memory_entry *mem_entry = vma->vm_private_data;
	unsigned long flags;
	int value;

	LOG_ENTRY;
	if (mem_entry == NULL) {
		LOG_ERR("Fail to get memory information");
		return;
	}
	LOG_DBG("Release memory area %lx - %lx, handle = %lx", vma->vm_start,
			vma->vm_end, mem_entry->handle);
	if (mem_entry->client->peer_id != current->pid) {
		LOG_ERR("Invalid process try to free, owner = (%d), current = (%d)",
				mem_entry->client->peer_id, current->pid);
		return;
	}
	LOG_DBG("shm status, owner = (%d)", atomic_read(&camellia_dev.shm_user));
	value = mem_entry->client->peer_id;
	if (atomic_try_cmpxchg(&camellia_dev.shm_user, &value, SHM_USE_NO_ONE)) {
		LOG_INFO("Release shared memory. owner = (%d)", mem_entry->client->peer_id);
		wake_up_interruptible(&camellia_dev.shm_wait_queue);
	}
	spin_lock_irqsave(&mem_entry->client->shmem_list_lock, flags);
	list_del(&mem_entry->node);
	spin_unlock_irqrestore(&mem_entry->client->shmem_list_lock, flags);
	kfree(mem_entry);
	vma->vm_private_data = NULL;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
static vm_fault_t camellia_vma_fault(struct vm_fault *vmf)
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(4, 10, 0))
static int camellia_vma_fault(struct vm_fault *vmf)
#else
static int camellia_vma_fault(struct vm_area_struct *vma,
							  struct vm_fault *vmf)
#endif
{
	return VM_FAULT_SIGBUS;
}

static const struct vm_operations_struct camellia_vm_ops = {
	.open = camellia_vma_open,
	.close = camellia_vma_close,
	.fault = camellia_vma_fault
};

static bool check_shm_owner_exchange(unsigned int pid)
{
	unsigned int value = SHM_USE_NO_ONE;
	return atomic_try_cmpxchg(&camellia_dev.shm_user, &value, pid);
}

static int camellia_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;
	struct camellia_memory_entry *mem_entry = NULL;
	unsigned long uaddr = vma->vm_start & PAGE_MASK;
	unsigned long usize = ((vma->vm_end - uaddr) + (PAGE_SIZE - 1)) &
						  PAGE_MASK;
	unsigned long pfn = (revmem.base >> PAGE_SHIFT) + vma->vm_pgoff;
	int ret = 0;
	int shm_setting_flag = SHM_SETTING_FLAG_FALSE;

	LOG_DBG("uaddr: 0x%lx, size: 0x%lx, vma->vm_start: 0x%lx, vma->vm_end: 0x%lx, vma->vm_pgoff: 0x%lx",
			uaddr, usize, vma->vm_start, vma->vm_end, vma->vm_pgoff << PAGE_SHIFT);

	/* TODO: separate operations into launch and log */
	if ((vma->vm_pgoff >= SHM_START_PAGE)
		&& (vma->vm_pgoff < (SHM_START_PAGE + (SHM_SIZE >> PAGE_SHIFT)))) {
		LOG_DBG("shm status, owner = (%d)", atomic_read(&camellia_dev.shm_user));
		ret = wait_event_interruptible_timeout(camellia_dev.shm_wait_queue,
							check_shm_owner_exchange(client->peer_id), LONG_MAX);
		if (ret == 0) {
			LOG_ERR("Peer %u's waiting for mmap was timeout", client->peer_id);
			return -ETIME;
		} else if (ret == -ERESTARTSYS) {
			LOG_ERR("Peer %u's waiting for mmap was canceled", client->peer_id);
			return -ERESTARTSYS;
		}
		shm_setting_flag = SHM_SETTING_FLAG_TRUE;
		LOG_INFO("shared memory success. id = (%d), owner = (%d)",
				client->peer_id, atomic_read(&camellia_dev.shm_user));
	}
	/* map as non-cacheable memory */
	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	if (remap_pfn_range(vma, vma->vm_start, pfn, usize,
						vma->vm_page_prot) != 0) {
		if (shm_setting_flag) {
			atomic_set(&camellia_dev.shm_user, SHM_USE_NO_ONE);
			wake_up_interruptible(&camellia_dev.shm_wait_queue);
		}
		LOG_ERR("Fail to remap pages");
		return -EINVAL;
	}

	LOG_DBG("allocated_memory's PA(mem_info) = 0x%lx", (pfn << PAGE_SHIFT));

	mem_entry = kmalloc(sizeof(struct camellia_memory_entry), GFP_KERNEL);
	if (!mem_entry) {
		if (shm_setting_flag) {
			atomic_set(&camellia_dev.shm_user, SHM_USE_NO_ONE);
			wake_up_interruptible(&camellia_dev.shm_wait_queue);
		}
		LOG_ERR("Unable to allocate memory list entry");
		return -ENOMEM;
	}

	mem_entry->size = usize;
	mem_entry->addr = vma->vm_start;
	mem_entry->handle = (pfn << PAGE_SHIFT);

	mem_entry->client = client;
	spin_lock(&client->shmem_list_lock);
	list_add_tail(&mem_entry->node, &client->shmem_list);
	spin_unlock(&client->shmem_list_lock);

	vma->vm_ops = &camellia_vm_ops;
	vma->vm_private_data = mem_entry;

	return 0;
}

static long camellia_memory_unwrap(struct camellia_client *client,
								   struct camellia_memory_pair *udata)
{
	struct camellia_memory_pair pair;
	struct camellia_memory_entry *elem;
	int found = 0;

	if (copy_from_user(&pair, udata, sizeof(struct camellia_memory_pair))) {
		LOG_ERR("Fail to get pair data from user.");
		return -EFAULT;
	}

	LOG_DBG("pair.address = %lx", pair.address);

	spin_lock(&client->shmem_list_lock);
	list_for_each_entry (elem, &client->shmem_list, node) {
		if (elem->addr == pair.address) {
			LOG_DBG("target address = %lx, handle = %lx",
					elem->addr, elem->handle);
			pair.handle = elem->handle;
			found = 1;
			break;
		}
	}
	spin_unlock(&client->shmem_list_lock);

	if (found == 0) {
		LOG_INFO("No memory found");
		pair.handle = 0;
	}

	if (copy_to_user(udata, &pair, sizeof(struct camellia_memory_pair))) {
		LOG_ERR("Fail to deliver result to user.");
		return -EFAULT;
	}

	return 0;
}

static long camellia_ioctl(struct file *file, unsigned int cmd,
						   unsigned long arg)
{
	long res = 0;
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;

	switch (cmd) {
	case CAMELLIA_IOCTL_SET_TIMEOUT: {
		uint64_t time_value = 0;
		unsigned long flags;
		if (copy_from_user(&time_value, (void __user *)arg, sizeof(uint64_t))) {
			LOG_ERR("Fail to get time.");
			return -EFAULT;
		}
		LOG_DBG("timeout value is (%d)", time_value);
		spin_lock_irqsave(&client->timeout_set_lock, flags);
		if (time_value == 0)
			client->timeout_jiffies = LONG_MAX;

		else
			client->timeout_jiffies = msecs_to_jiffies(time_value);
		spin_unlock_irqrestore(&client->timeout_set_lock, flags);
		res = 0;
		break;
	}
	case CAMELLIA_IOCTL_GET_HANDLE:
		res = camellia_memory_unwrap(client, (void __user *)arg);
		break;
	case CAMELLIA_IOCTL_GET_VERSION: {
		struct camellia_version version_info;
		version_info.major = MAJOR_VERSION;
		version_info.minor = MINOR_VERSION;
		if (copy_to_user((void __user *)arg, &version_info,
			sizeof(struct camellia_version))) {
			LOG_ERR("Fail to deliver result to user.");
			return -EFAULT;
		}
		break;
	}
	case CAMELLIA_IOCTL_TURN_CAMELLIA: {
		if (arg == CAMELLIA_TURN_ON) {
			if (client->pm_cnt > 0) {
				LOG_ERR("It turned on already. id = (%d)", client->peer_id);
				return -EPERM;
			}
			mutex_lock(&camellia_dev.lock);
			if (camellia_dev.state == CAMELLIA_STATE_OFFLINE) {
				camellia_dev.state = CAMELLIA_STATE_UP;
				if (strong_power_on()) {
					camellia_dev.state = CAMELLIA_STATE_OFFLINE;
					mutex_unlock(&camellia_dev.lock);
					LOG_ERR("Turn On. Failed to turn on STRONG");
					return -EAGAIN;
				}
				camellia_dev.state = CAMELLIA_STATE_ONLINE;
			}
			atomic_inc(&camellia_dev.refcnt);
			LOG_DBG("Turn On. PM Ref Count = (%d)", atomic_read(&camellia_dev.refcnt));
			mutex_unlock(&camellia_dev.lock);
			client->pm_cnt = 1;
		} else if (arg == CAMELLIA_TURN_OFF) {
			if (client->pm_cnt == 0) {
				LOG_ERR("It didn't turn on before. id = (%d)", client->peer_id);
				return -EPERM;
			}
			atomic_dec(&camellia_dev.refcnt);
			LOG_DBG("Turn Off. PM Ref Count = (%d)", atomic_read(&camellia_dev.refcnt));
			if (atomic_read(&camellia_dev.refcnt) <= 0) {
				if (camellia_dev.pm_timer_set) {
					mod_timer(&camellia_dev.pm_timer, jiffies + (PM_WAIT_TIME * HZ));
				}
			}
			client->pm_cnt = 0;
		} else {
			LOG_ERR("Unknown PM command: %d", arg);
			return -EINVAL;
		}
		break;
	}
	default:
		LOG_ERR("Unknown ioctl command: %d", cmd);
		res = -EINVAL;
	}
	return res;
}

struct camellia_client *camellia_create_client(unsigned int peer_id)
{
	struct camellia_client *client = NULL;

	client = kzalloc(sizeof(struct camellia_client), GFP_KERNEL);
	if (!client) {
		LOG_ERR("Can't alloc client context (-ENOMEM)");
		return NULL;
	}

	INIT_LIST_HEAD(&client->shmem_list);

	spin_lock_init(&client->shmem_list_lock);
	spin_lock_init(&client->timeout_set_lock);

	client->peer_id = peer_id;
	client->timeout_jiffies = LONG_MAX;
	client->pm_cnt = 0;

	return client;
}

void camellia_destroy_client(struct camellia_client *client)
{
	unsigned long flags;
	struct camellia_memory_entry *mem_entry = NULL;
	spin_lock_irqsave(&client->shmem_list_lock, flags);
	while (!list_empty(&client->shmem_list)) {
		mem_entry = list_first_entry(&client->shmem_list,
					struct camellia_memory_entry, node);
		list_del(&mem_entry->node);
		kfree(mem_entry);
	}
	spin_unlock_irqrestore(&client->shmem_list_lock, flags);
	kfree(client);
}

static int camellia_open(struct inode *inode, struct file *file)
{
	struct camellia_client *client;
	unsigned int peer_id;
	int ret;

	peer_id = task_pid_nr(current);
	ret = camellia_data_peer_create(peer_id);
	if (ret) {
		LOG_ERR("Failed to create peer %u", peer_id);
		return -EINVAL;
	}

	client = camellia_create_client(peer_id);
	if (client == NULL) {
		camellia_data_peer_destroy(peer_id);
		LOG_ERR("Failed to create client");
		return -EINVAL;
	}

	file->private_data = client;

	return 0;
}

static int camellia_release(struct inode *inode, struct file *file)
{
	struct camellia_client *client =
		(struct camellia_client *)file->private_data;
	int value = client->peer_id;

	LOG_ENTRY;

	if (client->pm_cnt > 0) {
		atomic_dec(&camellia_dev.refcnt);
		LOG_DBG("Release. PM Ref Count = (%d)", atomic_read(&camellia_dev.refcnt));
		if (atomic_read(&camellia_dev.refcnt) <= 0) {
			if (camellia_dev.pm_timer_set) {
				mod_timer(&camellia_dev.pm_timer, jiffies + (PM_WAIT_TIME * HZ));
			}
		}
	}
	LOG_DBG("shm status, owner = (%d)", atomic_read(&camellia_dev.shm_user));
	if (atomic_try_cmpxchg(&camellia_dev.shm_user, &value, SHM_USE_NO_ONE)) {
		LOG_INFO("Release shm at close. owner = (%d)", client->peer_id);
		wake_up_interruptible(&camellia_dev.shm_wait_queue);
	}

	camellia_data_peer_destroy(client->peer_id);
	camellia_destroy_client(client);

	LOG_EXIT;
	return 0;
}

void *camellia_request_region(unsigned long addr, unsigned int size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages = NULL;
	void *v_addr = NULL;

	if (!addr)
		return NULL;

	pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_ATOMIC);
	if (!pages)
		return NULL;

	for (i = 0; i < num_pages; i++) {
		pages[i] = phys_to_page(addr);
		addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	kfree(pages);

	return v_addr;
}

static const struct of_device_id camellia_of_match_table[] = {
	{
		.compatible = "exynos,strong",
	},
	{},
};

static int camellia_memory_setup(void)
{
	struct reserved_mem *rmem;
	struct device_node *dn;

	dn = of_find_matching_node(NULL, camellia_of_match_table);
	if (!dn || !of_device_is_available(dn)) {
		LOG_ERR("camellia node is not available");
		return -EINVAL;
	}

	rmem = of_reserved_mem_lookup(dn);
	if (!rmem) {
		LOG_ERR("failed to acquire memory region");
		return -EINVAL;
	}

	revmem.base = rmem->base;
	revmem.size = rmem->size;
	LOG_DBG("Found reserved memory: %lx - %lx", revmem.base, revmem.size);

	return 0;
}

static int camellia_ram_dump_setup(void)
{
#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
	unsigned long log_base = revmem.base + RAMDUMP_OFFSET;
	unsigned long log_size = RAMDUMP_SIZE;
	int result = -1;

	result = dbg_snapshot_add_bl_item_info(LOG_CAMELLIA, log_base, log_size);
	if (result)
		return result;
#endif
	return 0;
}

static struct file_operations camellia_fops = {
	.owner = THIS_MODULE,
	.read = camellia_read,
	.write = camellia_write,
	.poll = camellia_poll,
	.mmap = camellia_mmap,
	.unlocked_ioctl = camellia_ioctl,
	.open = camellia_open,
	.release = camellia_release,
};

static ssize_t camellia_dbg_show(struct device *dev,
       struct device_attribute *attr, char *buf)
{
	int index = 0;

	index += sprintf(buf, "%s: free cache cnt:%d \n",
		__func__, camellia_cache_cnt(1));
	return index;
}

static ssize_t camellia_dbg_store(struct device *dev,
                              struct device_attribute *attr,
                              const char *buf, size_t count)
{
	return count;
}

static struct device_attribute attributes[] = {
	__ATTR(cam_dbg, 0664, camellia_dbg_show, camellia_dbg_store),
};

static int camellia_init(void)
{
	int result = 0;
	int i;

	result = camellia_data_path_init();
	if (result < 0) {
		LOG_ERR("Failed to setup camelllia data path");
		return result;
	}

	result = camellia_pm_init();
	if (result < 0) {
		LOG_ERR("Failed to setup camelllia power-management");
		return result;
	}

	result = camellia_memory_setup();
	if (result < 0) {
		LOG_ERR("Failed to setup camellia memory");
		return result;
	}

	mutex_init(&camellia_dev.lock);
	mutex_init(&camellia_dev.logsink_lock);
	mutex_init(&camellia_dev.coredump_lock);
	camellia_dev.state = CAMELLIA_STATE_OFFLINE;
	camellia_dev.pm_timer_set = CAMELLIA_PM_TIMER_OFF;
	atomic_set(&camellia_dev.refcnt, 0);
	timer_setup(&camellia_dev.pm_timer, pm_timer_callback, 0);
	atomic_set(&camellia_dev.shm_user, SHM_USE_NO_ONE);
	init_waitqueue_head(&camellia_dev.shm_wait_queue);

	iwc_dev.minor = MISC_DYNAMIC_MINOR;
	iwc_dev.fops = &camellia_fops;
	iwc_dev.name = device_name;

	result = misc_register(&iwc_dev);
	if (result < 0) {
		LOG_ERR("Failed to register camellia device: %i", result);
		return result;
	}
	LOG_DBG("Registered camellia communication device %d", iwc_dev.minor);

	result = camellia_dump_init();
	if (result < 0) {
		LOG_ERR("Failed to register camllia dump device: %i", result);
		return result;
	}

	result = camellia_mailbox_init();
	if (result < 0) {
		LOG_ERR("Failed to initialize mailbox: %i", result);
		return result;
	}

	result = camellia_ram_dump_setup();
	if (result != 0) {
		LOG_ERR("Failed to add ram dump: %i", result);
		return result;
	}

	result = camellia_memlog_register();
	if (result != 0) {
		LOG_ERR("Failed to register memlog: %i", result);
		return result;
	}

	result = camellia_coredump_memlog_register();
	if (result != 0) {
		LOG_ERR("Failed to register coredump memlog: %i", result);
		return result;
	}

	camellia_dev.pm_wq = alloc_workqueue("camellia_pm", WQ_UNBOUND | WQ_SYSFS, 0);
	if (!camellia_dev.pm_wq) {
		LOG_ERR("Failed to allocate pm workqueue");
		return result;
	}
	INIT_WORK(&(camellia_dev.pm_work), camellia_pm_work);

	/* Add attributes */
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		pr_info("Add attribute: %s\n", attributes[i].attr.name);
		result = device_create_file(iwc_dev.this_device, &attributes[i]);
		if (result)
			pr_err("Failed to create file: %s\n", attributes[i].attr.name);
	}
	pr_info("Add attribute Done: %s\n", attributes[0].attr.name);

	return 0;
}

static void camellia_exit(void)
{
	LOG_DBG("Deregistered camellia communication device %d", iwc_dev.minor);

	del_timer(&camellia_dev.pm_timer);

	misc_deregister(&iwc_dev);

	camellia_mailbox_deinit();
	camellia_dump_deinit();
	camellia_pm_deinit();
	camellia_data_path_deinit();
	destroy_workqueue(camellia_dev.pm_wq);
}

module_init(camellia_init);
module_exit(camellia_exit);
