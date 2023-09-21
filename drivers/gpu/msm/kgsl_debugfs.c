/* Copyright (c) 2002,2008-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/debugfs.h>

#include "kgsl.h"
#include "kgsl_device.h"
#include "kgsl_sharedmem.h"
#include "kgsl_debugfs.h"

/*default log levels is error for everything*/
#define KGSL_LOG_LEVEL_MAX     7

struct dentry *kgsl_debugfs_dir;
static struct dentry *proc_d_debugfs;

static inline int kgsl_log_set(unsigned int *log_val, void *data, u64 val)
{
	*log_val = min((unsigned int)val, (unsigned int)KGSL_LOG_LEVEL_MAX);
	return 0;
}

#define KGSL_DEBUGFS_LOG(__log)                         \
static int __log ## _set(void *data, u64 val)           \
{                                                       \
	struct kgsl_device *device = data;              \
	return kgsl_log_set(&device->__log, data, val); \
}                                                       \
static int __log ## _get(void *data, u64 *val)	        \
{                                                       \
	struct kgsl_device *device = data;              \
	*val = device->__log;                           \
	return 0;                                       \
}                                                       \
DEFINE_SIMPLE_ATTRIBUTE(__log ## _fops,                 \
__log ## _get, __log ## _set, "%llu\n");                \

KGSL_DEBUGFS_LOG(drv_log);
KGSL_DEBUGFS_LOG(cmd_log);
KGSL_DEBUGFS_LOG(ctxt_log);
KGSL_DEBUGFS_LOG(mem_log);
KGSL_DEBUGFS_LOG(pwr_log);

static int _strict_set(void *data, u64 val)
{
	kgsl_sharedmem_set_noretry(val ? true : false);
	return 0;
}

static int _strict_get(void *data, u64 *val)
{
	*val = kgsl_sharedmem_get_noretry();
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_strict_fops, _strict_get, _strict_set, "%llu\n");

void kgsl_device_debugfs_init(struct kgsl_device *device)
{
	if (kgsl_debugfs_dir && !IS_ERR(kgsl_debugfs_dir))
		device->d_debugfs = debugfs_create_dir(device->name,
						       kgsl_debugfs_dir);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	debugfs_create_file("log_level_cmd", 0644, device->d_debugfs, device,
			    &cmd_log_fops);
	debugfs_create_file("log_level_ctxt", 0644, device->d_debugfs, device,
			    &ctxt_log_fops);
	debugfs_create_file("log_level_drv", 0644, device->d_debugfs, device,
			    &drv_log_fops);
	debugfs_create_file("log_level_mem", 0644, device->d_debugfs, device,
				&mem_log_fops);
	debugfs_create_file("log_level_pwr", 0644, device->d_debugfs, device,
				&pwr_log_fops);
}

void kgsl_device_debugfs_close(struct kgsl_device *device)
{
	debugfs_remove_recursive(device->d_debugfs);
}

struct type_entry {
	int type;
	const char *str;
};

static const struct type_entry memtypes[] = { KGSL_MEM_TYPES };

static const char *memtype_str(int memtype)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(memtypes); i++)
		if (memtypes[i].type == memtype)
			return memtypes[i].str;
	return "unknown";
}

static char get_alignflag(const struct kgsl_memdesc *m)
{
	int align = kgsl_memdesc_get_align(m);
	if (align >= ilog2(SZ_1M))
		return 'L';
	else if (align >= ilog2(SZ_64K))
		return 'l';
	return '-';
}

static char get_cacheflag(const struct kgsl_memdesc *m)
{
	static const char table[] = {
		[KGSL_CACHEMODE_WRITECOMBINE] = '-',
		[KGSL_CACHEMODE_UNCACHED] = 'u',
		[KGSL_CACHEMODE_WRITEBACK] = 'b',
		[KGSL_CACHEMODE_WRITETHROUGH] = 't',
	};
	return table[kgsl_memdesc_get_cachemode(m)];
}

static int print_mem_entry(void *data, void *ptr)
{
	struct seq_file *s = data;
	struct kgsl_mem_entry *entry = ptr;
	char flags[10];
	char usage[16];
	struct kgsl_memdesc *m = &entry->memdesc;
	unsigned int usermem_type = kgsl_memdesc_usermem_type(m);
	int egl_surface_count = 0, egl_image_count = 0;

	if (m->flags & KGSL_MEMFLAGS_SPARSE_VIRT)
		return 0;

	flags[0] = kgsl_memdesc_is_global(m) ?  'g' : '-';
	flags[1] = '-';
	flags[2] = !(m->flags & KGSL_MEMFLAGS_GPUREADONLY) ? 'w' : '-';
	flags[3] = get_alignflag(m);
	flags[4] = get_cacheflag(m);
	flags[5] = kgsl_memdesc_use_cpu_map(m) ? 'p' : '-';
	flags[6] = (m->useraddr) ? 'Y' : 'N';
	flags[7] = kgsl_memdesc_is_secured(m) ?  's' : '-';
	flags[8] = m->flags & KGSL_MEMFLAGS_SPARSE_PHYS ? 'P' : '-';
	flags[9] = '\0';

	kgsl_get_memory_usage(usage, sizeof(usage), m->flags);

	if (usermem_type == KGSL_MEM_ENTRY_ION)
		kgsl_get_egl_counts(entry, &egl_surface_count,
						&egl_image_count);

#if defined(CONFIG_FB_MSM_MDSS_SAMSUNG) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	seq_printf(s, "%p %p %16llu %5d %9s %10s %16s %5d %16llu %6d %6d",
			(uint64_t *)(uintptr_t) m->gpuaddr,
			(unsigned long *) m->useraddr,
			m->size, entry->id, flags,
			memtype_str(kgsl_memdesc_usermem_type(m)),
			usage, (m->sgt ? m->sgt->nents : 0), m->mapsize,
			egl_surface_count, egl_image_count);
#else
	seq_printf(s, "%pK %pK %16llu %5d %9s %10s %16s %5d %16llu %6d %6d",
			(uint64_t *)(uintptr_t) m->gpuaddr,
			(unsigned long *) m->useraddr,
			m->size, entry->id, flags,
			memtype_str(usermem_type),
			usage, (m->sgt ? m->sgt->nents : 0), m->mapsize,
			egl_surface_count, egl_image_count);
#endif

	if (entry->metadata[0] != 0)
		seq_printf(s, " %s", entry->metadata);

	seq_putc(s, '\n');

	return 0;
}

static struct kgsl_mem_entry *process_mem_seq_find(struct seq_file *s,
						void *ptr, loff_t pos)
{
	struct kgsl_mem_entry *entry = ptr;
	struct kgsl_process_private *private = s->private;
	int id = 0;
	loff_t temp_pos = 1;

	if (entry != SEQ_START_TOKEN)
		id = entry->id + 1;

	spin_lock(&private->mem_lock);
	for (entry = idr_get_next(&private->mem_idr, &id); entry;
		id++, entry = idr_get_next(&private->mem_idr, &id),
							temp_pos++) {
		if (temp_pos == pos && kgsl_mem_entry_get(entry)) {
			spin_unlock(&private->mem_lock);
			goto found;
		}
	}
	spin_unlock(&private->mem_lock);

	entry = NULL;
found:
	if (ptr != SEQ_START_TOKEN)
		kgsl_mem_entry_put(ptr);

	return entry;
}

static void *process_mem_seq_start(struct seq_file *s, loff_t *pos)
{
	loff_t seq_file_offset = *pos;

	if (seq_file_offset == 0)
		return SEQ_START_TOKEN;
	else
		return process_mem_seq_find(s, SEQ_START_TOKEN,
						seq_file_offset);
}

static void process_mem_seq_stop(struct seq_file *s, void *ptr)
{
	if (ptr && ptr != SEQ_START_TOKEN)
		kgsl_mem_entry_put(ptr);
}

static void *process_mem_seq_next(struct seq_file *s, void *ptr,
							loff_t *pos)
{
	++*pos;
	return process_mem_seq_find(s, ptr, 1);
}

static int process_mem_seq_show(struct seq_file *s, void *ptr)
{
	if (ptr == SEQ_START_TOKEN) {
		seq_printf(s, "%16s %16s %16s %5s %9s %10s %16s %5s %16s %6s %6s\n",
			"gpuaddr", "useraddr", "size", "id", "flags", "type",
			"usage", "sglen", "mapsize", "eglsrf", "eglimg");
		return 0;
	} else
		return print_mem_entry(s, ptr);
}

static const struct seq_operations process_mem_seq_fops = {
	.start = process_mem_seq_start,
	.stop = process_mem_seq_stop,
	.next = process_mem_seq_next,
	.show = process_mem_seq_show,
};

static int process_mem_open(struct inode *inode, struct file *file)
{
	int ret;
	pid_t pid = (pid_t) (unsigned long) inode->i_private;
	struct seq_file *s = NULL;
	struct kgsl_process_private *private = NULL;

	private = kgsl_process_private_find(pid);

	if (!private)
		return -ENODEV;

	ret = seq_open(file, &process_mem_seq_fops);
	if (ret)
		kgsl_process_private_put(private);
	else {
		s = file->private_data;
		s->private = private;
	}

	return ret;
}

static int process_mem_release(struct inode *inode, struct file *file)
{
	struct kgsl_process_private *private =
		((struct seq_file *)file->private_data)->private;

	if (private)
		kgsl_process_private_put(private);

	return seq_release(inode, file);
}

static const struct file_operations process_mem_fops = {
	.open = process_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = process_mem_release,
};

static int print_sparse_mem_entry(int id, void *ptr, void *data)
{
	struct seq_file *s = data;
	struct kgsl_mem_entry *entry = ptr;
	struct kgsl_memdesc *m = &entry->memdesc;
	struct rb_node *node;

	if (!(m->flags & KGSL_MEMFLAGS_SPARSE_VIRT))
		return 0;

	spin_lock(&entry->bind_lock);
	node = rb_first(&entry->bind_tree);

	while (node != NULL) {
		struct sparse_bind_object *obj = rb_entry(node,
				struct sparse_bind_object, node);
		seq_printf(s, "%5d %16llx %16llx %16llx %16llx\n",
				entry->id, entry->memdesc.gpuaddr,
				obj->v_off, obj->size, obj->p_off);
		node = rb_next(node);
	}
	spin_unlock(&entry->bind_lock);

	seq_putc(s, '\n');

	return 0;
}

static int process_sparse_mem_print(struct seq_file *s, void *unused)
{
	struct kgsl_process_private *private = s->private;

	seq_printf(s, "%5s %16s %16s %16s %16s\n",
		   "v_id", "gpuaddr", "v_offset", "v_size", "p_offset");

	spin_lock(&private->mem_lock);
	idr_for_each(&private->mem_idr, print_sparse_mem_entry, s);
	spin_unlock(&private->mem_lock);

	return 0;
}

static int process_sparse_mem_open(struct inode *inode, struct file *file)
{
	int ret;
	pid_t pid = (pid_t) (unsigned long) inode->i_private;
	struct kgsl_process_private *private = NULL;

	private = kgsl_process_private_find(pid);

	if (!private)
		return -ENODEV;

	ret = single_open(file, process_sparse_mem_print, private);
	if (ret)
		kgsl_process_private_put(private);

	return ret;
}

static const struct file_operations process_sparse_mem_fops = {
	.open = process_sparse_mem_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = process_mem_release,
};

static int globals_print(struct seq_file *s, void *unused)
{
	kgsl_print_global_pt_entries(s);
	return 0;
}

static int globals_open(struct inode *inode, struct file *file)
{
	return single_open(file, globals_print, NULL);
}

static int globals_release(struct inode *inode, struct file *file)
{
	return single_release(inode, file);
}

static const struct file_operations global_fops = {
	.open = globals_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = globals_release,
};

/**
 * kgsl_process_init_debugfs() - Initialize debugfs for a process
 * @private: Pointer to process private structure created for the process
 *
 * kgsl_process_init_debugfs() is called at the time of creating the
 * process struct when a process opens kgsl device for the first time.
 * This function is not fatal - all we do is print a warning message if
 * the files can't be created
 */
void kgsl_process_init_debugfs(struct kgsl_process_private *private)
{
	unsigned char name[16];
	struct dentry *dentry;

	snprintf(name, sizeof(name), "%d", private->pid);

	private->debug_root = debugfs_create_dir(name, proc_d_debugfs);

	/*
	 * Both debugfs_create_dir() and debugfs_create_file() return
	 * ERR_PTR(-ENODEV) if debugfs is disabled in the kernel but return
	 * NULL on error when it is enabled. For both usages we need to check
	 * for ERROR or NULL and only print a warning on an actual failure
	 * (i.e. - when the return value is NULL)
	 */

	if (IS_ERR_OR_NULL(private->debug_root)) {
		WARN((private->debug_root == NULL),
			"Unable to create debugfs dir for %s\n", name);
		private->debug_root = NULL;
		return;
	}

	dentry = debugfs_create_file("mem", 0444, private->debug_root,
		(void *) ((unsigned long) private->pid), &process_mem_fops);

	if (IS_ERR_OR_NULL(dentry))
		WARN((dentry == NULL),
			"Unable to create 'mem' file for %s\n", name);

	dentry = debugfs_create_file("sparse_mem", 0444, private->debug_root,
		(void *) ((unsigned long) private->pid),
		&process_sparse_mem_fops);

	if (IS_ERR_OR_NULL(dentry))
		WARN((dentry == NULL),
			"Unable to create 'sparse_mem' file for %s\n", name);

}

/* TODO: Enable */
#if 0
/* #if defined(CONFIG_FB_MSM_MDSS_SAMSUNG) */
/* #define CONFIG_SAMSUNG_KGSL_DEBUG */
extern pid_t kgsl_mem_dump_pid;

static int kgsl_mem_debugfs_show(struct seq_file *s, void *unused)
{
	struct task_struct *task;
	struct mm_struct *mm;

	struct kgsl_process_private *private = NULL;
	struct kgsl_process_private *dump_private = NULL;
	unsigned long total_vm = 0;

#if defined(CONFIG_SAMSUNG_KGSL_DEBUG) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	struct kgsl_iommu_pt *pt = NULL;
	struct vm_area_struct *vma = NULL;
	unsigned long vma_size = 0;
	unsigned long last_vm_end = 0;
	unsigned long biggest_hole = 0, total_hole = 0, hole_size = 0;

	unsigned long kgsl_mem_dump_svm_start = KGSL_IOMMU_SVM_BASE32;
	unsigned long kgsl_mem_dump_svm_end = KGSL_IOMMU_SVM_END32;
#endif


	mutex_lock(&kgsl_driver.process_mutex);

	list_for_each_entry(private, &kgsl_driver.process_list, list) {
		if (private->pid == kgsl_mem_dump_pid)
			dump_private = private;
	}

	if (kgsl_mem_dump_pid != -EPERM) {
		task = find_task_by_vpid(kgsl_mem_dump_pid);

		if (!IS_ERR_OR_NULL(task)) {
			mm = task->mm;
			if (!IS_ERR_OR_NULL(mm))
				total_vm = mm->total_vm;
			else
				total_vm = 0;

			seq_printf(s, "kgsl_dump_pid : %d pid : %d name : %16s total_vm : 0x%lx\n",
				kgsl_mem_dump_pid, task->pid, task->comm, total_vm);

#if defined(CONFIG_SAMSUNG_KGSL_DEBUG) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			if (!IS_ERR_OR_NULL(dump_private)) {
				if (!IS_ERR_OR_NULL(dump_private->pagetable)) {
					pt = (struct kgsl_iommu_pt *)(dump_private->pagetable->priv);
					if (!IS_ERR_OR_NULL(pt)) {
						kgsl_mem_dump_svm_start = pt->svm_start; 
						kgsl_mem_dump_svm_end = pt->svm_end;
					}
				}
			}
#endif
			/* gpu memory dump */
			if (!IS_ERR_OR_NULL(dump_private)) {
				s->private = dump_private;
				process_mem_print(s, NULL);
			} else
				seq_printf(s, "pid %d is not attached to process list\n", task->pid);
		} else
			seq_printf(s, "kgsl_dump_pid : %d task was killed at gpu dump\n", kgsl_mem_dump_pid);
	} else
		seq_printf(s, "kgsl dump pid is not exist(normal operation)\n");

	mutex_unlock(&kgsl_driver.process_mutex);

#if defined(CONFIG_SAMSUNG_KGSL_DEBUG) && !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
	/* userspace vmlist dump */
	if (kgsl_mem_dump_pid != -EPERM) {
		task = find_task_by_vpid(kgsl_mem_dump_pid);

		if (!IS_ERR_OR_NULL(task)) {
			seq_printf(s, "\n%-21s %-18s %-11s %s\n",
				"vm_start", "vm_end", "vm_flags", "size");

			for (vma = task->mm->mmap; vma; vma = vma->vm_next) {

				/* To check iommu address range */
				if ((vma->vm_start > kgsl_mem_dump_svm_end) || (vma->vm_end > kgsl_mem_dump_svm_end))
					break;

				vma_size = vma->vm_end - vma->vm_start;

				if (last_vm_end && (last_vm_end != vma->vm_start)) {
					hole_size = (vma->vm_start - last_vm_end) >> 10;
					seq_printf(s, "(hole) (0x%lx %lu KB)\n", hole_size, hole_size);

					if (hole_size > biggest_hole)
						biggest_hole = hole_size;

					total_hole += hole_size;
				} else
					seq_printf(s, "0x%-16lx -- 0x%-16lx 0x%-8lx (%7lu KB)\n",
						vma->vm_start, vma->vm_end, vma->vm_flags, vma_size >> 10);

				last_vm_end = vma->vm_end;

				if (vma_size <= 0)
					break;
			}

			seq_printf(s, "Biggest hole(from 0x%lx to 0x%lx) is 0x%lx (%lu KB)\n"
					"Total hole is 0x%lx (%lu KB)\n", 
					kgsl_mem_dump_svm_start, kgsl_mem_dump_svm_end,
					biggest_hole, biggest_hole,
					total_hole, total_hole);
		} else
			seq_printf(s, "kgsl_dump_pid : %d task was killed at vmlist dump\n", kgsl_mem_dump_pid);
	}
#endif

	return 0;
}

static int kgsl_mem_debugfs_open(struct inode *inode, struct file *file)
{	
	return single_open(file, kgsl_mem_debugfs_show, file);
}

static const struct file_operations kgsl_mem_debugfs_fops = {
	.open           = kgsl_mem_debugfs_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif

void kgsl_core_debugfs_init(void)
{
	struct dentry *debug_dir;

	kgsl_debugfs_dir = debugfs_create_dir("kgsl", NULL);

	debugfs_create_file("globals", 0444, kgsl_debugfs_dir, NULL,
		&global_fops);

	debug_dir = debugfs_create_dir("debug", kgsl_debugfs_dir);

	debugfs_create_file("strict_memory", 0644, debug_dir, NULL,
		&_strict_fops);

	proc_d_debugfs = debugfs_create_dir("proc", kgsl_debugfs_dir);
/* TODO: Enalbe */
#if 0
/* #if defined(CONFIG_FB_MSM_MDSS_SAMSUNG) */
	debugfs_create_file("kgsl_mem", S_IRUGO, kgsl_debugfs_dir, NULL, &kgsl_mem_debugfs_fops);
#endif
}

void kgsl_core_debugfs_close(void)
{
	debugfs_remove_recursive(kgsl_debugfs_dir);
}
