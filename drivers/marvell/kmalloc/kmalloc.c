/*
 * Copyright (C) 2013 Marvell International Ltd.
 *		Jialing Fu <jlfu@marvell.com>
 *
 * Try to create a set of APIs to let some specail module/driver can
 * alloc/free objects seperatly
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

/*
 * Background:
 * As you know, kmalloc is called directlly and oftenly in the code of drivers
 * and modules.
 * In most case it works well, as it is a standard/recommend API provided
 * by linux kernel. Also this API is efficiently and benifit to save memory.
 *
 * Using the API, all driver and module would share same kmalloc-xxx for same
 * buffer size range. like kmalloc-64 is shared when kmalloc is called with size
 * from 1 to 64.
 *
 * As a result, it is hard to debug when kmem issue happens if kmalloc and
 * kfree is misused. As it has limitation:
 *  1) By default CONFIG_SLUB_DEBUB_ON is not set in kernel config, so that
 *  there is not Trace/Debug info for us to check when somebody misuse it.
 *  2) But if we enable CONFIG_SLUB_DEBUB_ON, all kmem_cache will enable the
 *  DEBUG feature, system performance would downgrade and memory space would be
 *  wasted too much.
 *
 *
 * So we may want to use kmalloc more advanced and flexible. Like
 * 1) The driver/module can still call kmalloc, but the buffer can be allocated
 * from other separate slub memory pools which different as kmalloc-xxx.
 * 2) The separate slub can have DEBUG feature when need.
 *
 *
 * So this file creates two more kmalloc kmem_cache, named as "supper kmalloc"
 *   #1: SKM0-xxx, with kernel's default debug flags
 *   #2: SKM1-xxx, force to enable all debug flags
 *
 *
 * User Guide:
 * 1) If "SUPPER_KMALLOC" is defined in driver/module, when kmalloc is called,
 * the compile tool would help to use SKMx-xxx instead.
 * 2) For each time, Only one Supper Kmalloc can be used, but you can change it
 * if echo 0 or 1 or 2 to /proc/supper_kmalloc
 *  #0: disable supper kmalloc, and use kmalloc-xxx
 *  #1: use SKM0-xxx
 *  #2: use SKM1-xxx
 *
 *  TODO: Enhance it if need
 */

#define SKM_DBG_LEVEL0 0
#define SKM_DBG_LEVEL1 1
#define SKM_DBG_LEVEL2 2
#define SKM_DBG_LEVEL_MAX	SKM_DBG_LEVEL2

#define SKM_DBG_LEVEL0_FLAGS 0
#define SKM_DBG_LEVEL1_FLAGS (SLAB_UNMERGE | SLAB_DEBUG_OBJECTS)
#define SKM_DBG_LEVEL2_FLAGS (SLAB_DEBUG_FREE | SLAB_RED_ZONE | \
				SLAB_POISON | SLAB_STORE_USER)

int skm_dbg_level = SKM_DBG_LEVEL2;

#define SKMALLOC_MAX_SIZE (8192)
static const size_t skmalloc_size[] = {
	64,	/* Alloc from 0~64 */
	128, /* Alloc from 65~128 */
	192,
	256,
	384,
	512,
	1024,
	2048,
	4096,
	SKMALLOC_MAX_SIZE, /* Alloc from 4097~8192 */
};

static const int dbg_level[] = {
	SKM_DBG_LEVEL1,
	SKM_DBG_LEVEL2,
};

static const int dbg_flags[] = {
	SKM_DBG_LEVEL1_FLAGS,
	SKM_DBG_LEVEL2_FLAGS,
};

#define MALLOC_CACHE_NUMS (sizeof(skmalloc_size)/sizeof(int))

#define NK_MAX_NAME 20
struct new_kmalloc {
	char name[NK_MAX_NAME];
	int	dbg_level;
	int	dbg_flags;
	struct kmem_cache *k_c[MALLOC_CACHE_NUMS];
};

struct supper_kmalloc {
	int	active_level;
	spinlock_t lock;
	struct new_kmalloc *active_n_k;
	struct new_kmalloc n_k[SKM_DBG_LEVEL_MAX];
};

struct supper_kmalloc s_kmalloc;

static int skm_size_to_index(size_t size)
{
	int index = 0;

	/* TODO: Use more efficiently way if need  */
	if ((size > SKMALLOC_MAX_SIZE) || (size <= 0))
		return -1;

	while (1) {
		if (size <= skmalloc_size[index])
			return index;
		else
			index++;
	}
}

static size_t skm_index_to_size(int index)
{
	if ((index >= MALLOC_CACHE_NUMS) || (index < 0))
		return -1;

	return skmalloc_size[index];
}

static struct kmem_cache *skm_get_kmem_cache(size_t size, gfp_t flags)
{
	int index;
	unsigned long  spin_flags;
	struct new_kmalloc *p_nk;

	/* TODO: Use more efficiently way if need  */
	index =  skm_size_to_index(size);
	if (index >= 0) {
		spin_lock_irqsave(&s_kmalloc.lock, spin_flags);
		p_nk = s_kmalloc.active_n_k;
		spin_unlock_irqrestore(&s_kmalloc.lock, spin_flags);
		return p_nk->k_c[index];
	} else
		return NULL;
}

void *skmalloc(size_t size, gfp_t flags)
{
	struct kmem_cache *obj_cache;

	if (skm_dbg_level) {
		if (unlikely(size > SKMALLOC_MAX_SIZE)) {
			pr_warning("alloc 0x%x bytes by kmalloc\n", size);
			return __kmalloc(size, flags);
		}

		obj_cache = skm_get_kmem_cache(size, flags);
		if (obj_cache)
			return kmem_cache_alloc(obj_cache, flags);
		else {
			/* Fail to create during smalloc_create_obj_caches */
			return __kmalloc(size, flags);
		}
	} else
		return __kmalloc(size, flags);
}
EXPORT_SYMBOL(skmalloc);

static void skfree(void *x)
{
	kfree(x);
}

static int new_kmalloc_init(struct new_kmalloc *p_nk)
{
	int i;
	size_t size;
	char *s;

	for (i = 0; i < MALLOC_CACHE_NUMS; i++) {
		size = skm_index_to_size(i);

		/* TODO: can use scnprintf to not use kmalloc*/
		s = kasprintf(GFP_KERNEL, "%s-%d", p_nk->name, size);
		if (s) {
			p_nk->k_c[i] =
				kmem_cache_create(s, size, 0,
						p_nk->dbg_flags, NULL);
			kfree(s);
		} else {
			/* TODO: error handling for create error */
			p_nk->k_c[i] = NULL;
		}
	}

	return 0;
}

static int new_kmalloc_destory(struct new_kmalloc *p_nk)
{
	int i;

	if (!p_nk)
		return -1;

	for (i = 0; i < MALLOC_CACHE_NUMS; i++) {
		if (p_nk->k_c[i]) {
			kmem_cache_destroy(p_nk->k_c[i]);
			p_nk->k_c[i] = NULL;
		}
	}

	return 0;
}

static int proc_skmalloc_show(struct seq_file *m, void *v)
{
	seq_puts(m, ">>>>Supper Kmalloc Version 0.1========\n");
	seq_puts(m, " >>>There 2 supper kmalloc for using\n");
	seq_puts(m, "   #1: SKM0-xxx, with kernel's default debug flags\n");
	seq_puts(m, "   #2: SKM1-xxx, force to enable all debug flags\n");
	seq_puts(m, "   #0: Disable to use supper kmalloc\n");
	seq_puts(m, " >>It is enabled if SUPPER_KMALLOC is defined\n");
	seq_printf(m, " >>Only one Supper Kmalloc is enabled, Current is #%d\n",
					skm_dbg_level);
	seq_puts(m, "  >But you can echo 0 or 1 or 2 to change it\n");
	seq_puts(m, "<<<<<<===============================\n");
	return 0;
}

static ssize_t proc_skmalloc_write(struct file *file, const char __user *buf,
			    size_t count, loff_t *offs)
{
	char *kbuf;
	int debug_level;
	unsigned long flags;
	struct supper_kmalloc *p_sk = &s_kmalloc;

	if (count > PAGE_SIZE || *offs)
		return -EINVAL;

	kbuf = __kmalloc(count, GFP_KERNEL);
	if (!kbuf)
		return -EFAULT;

	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;

	sscanf(kbuf, "%d", &debug_level);

	if (debug_level >= 0 && debug_level <= SKM_DBG_LEVEL_MAX) {
		spin_lock_irqsave(&s_kmalloc.lock, flags);
		skm_dbg_level = debug_level;
		if (skm_dbg_level == SKM_DBG_LEVEL1) {
			p_sk->active_level = SKM_DBG_LEVEL1;
			p_sk->active_n_k = &p_sk->n_k[0];

		} else if (skm_dbg_level == SKM_DBG_LEVEL2) {
			p_sk->active_level = SKM_DBG_LEVEL2;
			p_sk->active_n_k = &p_sk->n_k[1];
		} else {
			p_sk->active_level = SKM_DBG_LEVEL0;
			p_sk->active_n_k = NULL;
		}
		spin_unlock_irqrestore(&s_kmalloc.lock, flags);
	}
	kfree(kbuf);

	return count;
}

static int proc_skmalloc_open(struct inode *inode, struct file *filp)
{
	return single_open(filp, proc_skmalloc_show, NULL);
}

static const struct file_operations proc_skmalloc_operations = {
	.open		= proc_skmalloc_open,
	.read		= seq_read,
	.write		= proc_skmalloc_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init skmalloc_init(void)
{
	int i;
	int status = 0;
	struct supper_kmalloc *p_sk = &s_kmalloc;
	struct new_kmalloc *p_nk;

	printk(KERN_INFO "Create Supper Kmalloc for some special driver or module\n");

	spin_lock_init(&s_kmalloc.lock);
	for (i = 0; i < SKM_DBG_LEVEL_MAX; i++) {
		p_nk = &p_sk->n_k[i];
		p_nk->dbg_level = dbg_level[i];
		p_nk->dbg_flags = dbg_flags[i];
		snprintf(&p_nk->name[0], NK_MAX_NAME, "SKM%d", i);

		new_kmalloc_init(p_nk);
	}

	/* Set default active setting for supper_kmalloc */
	if (skm_dbg_level == SKM_DBG_LEVEL1) {
		p_sk->active_level = SKM_DBG_LEVEL1;
		p_sk->active_n_k = &p_sk->n_k[0];
	} else if (skm_dbg_level == SKM_DBG_LEVEL2) {
		p_sk->active_level = SKM_DBG_LEVEL2;
		p_sk->active_n_k = &p_sk->n_k[1];
	} else {
		skm_dbg_level = SKM_DBG_LEVEL0;
		p_sk->active_level = SKM_DBG_LEVEL0;
		p_sk->active_n_k = NULL;
	}
	proc_create("supper_kmalloc", S_IRUGO | S_IWUSR, NULL,
			&proc_skmalloc_operations);

	return status;
}
/*
 * Make sure skmalloc_init is called at:
 * after SLUB system has been finished && before devices code is ready to run
 */
rootfs_initcall(skmalloc_init);

static void __exit skmalloc_exit(void)
{
	int i;

	struct supper_kmalloc *p_sk = &s_kmalloc;
	struct new_kmalloc *p_nk;

	printk(KERN_INFO "Destory Supper Kmalloc Objects\n");
	remove_proc_entry("supper_kmalloc", NULL);

	for (i = 0; i < SKM_DBG_LEVEL_MAX; i++) {
		p_nk = &p_sk->n_k[i];
		new_kmalloc_destory(p_nk);
	}

	p_sk->active_level = SKM_DBG_LEVEL1;
	p_sk->active_n_k = &p_sk->n_k[0];

	printk(KERN_INFO "------kmalloc exit end\n");
}
module_exit(skmalloc_exit);


MODULE_AUTHOR("Jialing Fu");
MODULE_DESCRIPTION("Kmalloc APIs");
MODULE_LICENSE("GPL");

module_param(skm_dbg_level, int, 0);
MODULE_PARM_DESC(skm_dbg_level,
		 "0: Use kmalloc-xxx 1: Use SKM0-xxx; 2: Use SKM1-xxx");
