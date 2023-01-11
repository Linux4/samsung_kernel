/*
 * Copyright (C) 2013~2014 Marvell International Ltd.
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

#include <linux/mm.h>
#include <linux/kexec.h>
#include <linux/reboot.h>

/*
 * Background:
 * As you know, kmalloc is called directlly and oftenly in the code of drivers
 * and modules.
 * In most case it works well, as it is a standard/recommend API provided
 * by linux kernel. Also this API is efficient and benifit to save memory.
 *
 * Using the API, all driver and module would share same kmalloc-xxx for same
 * buffer size range. like kmalloc-64 is shared, when size is from 1 to 64.
 *
 * As a result, it is hard to debug when kmem issue happens if kmalloc and
 * kfree is misused. As it has limitation:
 *  1) By default CONFIG_SLUB_DEBUB_ON is not set in kernel config, so that
 *  there is not Trace/Debug info for us to check, if somebody misuse it.
 *  2) But if we enable CONFIG_SLUB_DEBUB_ON, all kmem_cache will enable the
 *  DEBUG feature, system performance would downgrade and memory space would be
 *  wasted too much.
 *
 *
 * So we may want to use kmalloc more advanced and flexible. Like
 * 1) The driver/module can still call kmalloc, but the buffer can be allocated
 * from other separate kmem_cache which different as kmalloc-xxx.
 * 2) The new kmem_cache can have DEBUG feature when need.
 *
 *
 * So this file creates a list of kmem_cache to support the idea, named as
 * "supper kmalloc"
 *
 *
 * User Guide:
 * 1) If "SUPPER_KMALLOC" is defined in driver/module, when kmalloc is called,
 * the compile tool would help to use skmalloc-xxx instead.
 *
 * 2) For each kernel cycle, Only one Supper Kmalloc can be created.
 * And you can change its behavior by changing skm_dbg_level
 *  #0: disable supper kmalloc, and use kmalloc-xxx
 *  #1: Create skmalloc-xxx with kernel's default debug flags
 *  #2: Create skmalloc-xxx, force to enable all debug flags
 *
 *  TODO: Finish below TODO if need
 */

#define SKM_DBG_LEVEL0 0
#define SKM_DBG_LEVEL1 1
#define SKM_DBG_LEVEL2 2
#define SKM_DBG_LEVEL_MAX	SKM_DBG_LEVEL2

#define SKM_DBG_LEVEL0_FLAGS 0
#define SKM_DBG_LEVEL1_FLAGS (SLAB_UNMERGE | SLAB_DEBUG_OBJECTS)
#define SKM_DBG_LEVEL2_FLAGS (SLAB_DEBUG_FREE | SLAB_RED_ZONE | \
				SLAB_POISON | SLAB_STORE_USER)

static inline int check_skm_dbg_level(int dbg_level)
{
	return dbg_level >= 0 && dbg_level <= SKM_DBG_LEVEL_MAX;
}

/*
 * As "skm_dbg_level" is very important to Supper Kmalloc's behavior
 * There are 3 ways to set "skm_dbg_level" as below
 * Note: For way 1,2,3, the biggest Number has highest priority
 * Example #2 can overwrite #1's setting......
 */

/* #1 Default setting by Hard Code */
static int skm_dbg_level = SKM_DBG_LEVEL1;

/* #2 kernel cmd line, like "bootrags = skm_dbg_level=1" */
static int __init skm_dbg_level_setup(char *str)
{
	int n;
	if (!get_option(&str, &n))
		return 0;

	if (check_skm_dbg_level(n))
		skm_dbg_level = n;
	else
		pr_info("cmd line invalid skm_dgb_level:%d\n", n);

	return 1;
}
__setup("skm_dbg_level=", skm_dbg_level_setup);

/* #3 reboot cmd line, like "reboot skm_dbg_level=2" */
#ifdef CONFIG_SKMALLOC_REBOOT_NOTIFY

#define CRASH_PAGE_OFFSET_SKM 32
/* TODO: it is better to create a global file to save OFFSET */

#define SKM_RSV_MAGIC1 0x2D4D4B53
#define SKM_RSV_MAGIC2 0x55464C4A
struct skm_rsvbuf {
	unsigned int magic1;
	int dbg_level;
	unsigned int magic2;
};

void *get_skm_rsvbuf(void)
{
	struct page *page;
	void *addr;

	/* make sure crash rsv page is defined in cmd line */
	if (crashk_res.start == 0 || crashk_res.end == 0)
		return NULL;

	page = pfn_to_page(crashk_res.end >> PAGE_SHIFT);
	addr = page_address(page);

	return (void *)(addr + CRASH_PAGE_OFFSET_SKM);
}

static int save_skm_dbg_level(unsigned int value)
{
	struct skm_rsvbuf *p_rsvbuf;

	p_rsvbuf = get_skm_rsvbuf();
	if (!p_rsvbuf)
		return -1;

	memset(p_rsvbuf, 0, sizeof(struct skm_rsvbuf));
	if (check_skm_dbg_level(value)) {
		p_rsvbuf->magic1 = SKM_RSV_MAGIC1;
		p_rsvbuf->magic2 = SKM_RSV_MAGIC2;
		p_rsvbuf->dbg_level = value;
		return 0;
	} else
		pr_info("reboot invalid skm_dgb_level:%d\n", value);

	return -1;
}

static int restore_skm_dbg_level(void)
{
	struct skm_rsvbuf *p_rsvbuf;

	p_rsvbuf = get_skm_rsvbuf();
	if (unlikely(!p_rsvbuf))
		return -1;

	if ((p_rsvbuf->magic1 == SKM_RSV_MAGIC1) &&
		(p_rsvbuf->magic2 == SKM_RSV_MAGIC2)) {
		/* make sure it is reserved for SKM indeed */
		if (check_skm_dbg_level(p_rsvbuf->dbg_level)) {
			skm_dbg_level = p_rsvbuf->dbg_level;
			return 0;
		}
	}
	return -1;
}

static int skm_notifier_bootup_handle(void)
{
	/*
	 * check whether skm_dbg_level was saved in last reboot
	 * If yes, restore skm_dbg_level and clear the saved information
	 */
	if (0 == restore_skm_dbg_level())
		save_skm_dbg_level(-1);

	return 0;
}

static int skm_notifier_shutdown_handle(struct notifier_block *this,
		unsigned long code, void *cmd)
{
	unsigned int tmp;

	if (cmd && sscanf(cmd, "skm_dbg_level=%d", &tmp) == 1)
		save_skm_dbg_level(tmp);

	return 0;
}

static struct notifier_block skm_reboot_notifier = {
	.notifier_call = skm_notifier_shutdown_handle,
};
#endif

/* End to init skm_dbg_level */


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

#define MALLOC_CACHE_NUMS ARRAY_SIZE(skmalloc_size)
struct supper_kmalloc {
	int	dbg_level;
	int	dbg_flags;
	char *name;
	struct kmem_cache *k_c[MALLOC_CACHE_NUMS];
};
struct supper_kmalloc s_kmalloc;

static inline void *skm_get_supper_kmalloc(void)
{
	return &s_kmalloc;
}

static int skm_size_to_index(size_t size)
{
	int index = 0;

	/* TODO: Use more efficiently way if need  */

	if (unlikely((size > SKMALLOC_MAX_SIZE) || (size <= 0)))
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
	if (unlikely((index >= MALLOC_CACHE_NUMS) || (index < 0)))
		return 0;

	return skmalloc_size[index];
}

static struct kmem_cache *skm_get_kmem_cache(size_t size, gfp_t flags)
{
	int index;
	struct supper_kmalloc *active_sk;

	active_sk = skm_get_supper_kmalloc();

	index =  skm_size_to_index(size);
	if (likely(index >= 0))
		return active_sk->k_c[index];
	else
		return NULL;
}

void *skmalloc(size_t size, gfp_t flags)
{
	struct kmem_cache *p_kc;

	if (skm_dbg_level) {
		if (unlikely(size > SKMALLOC_MAX_SIZE)) {
			pr_warn("alloc 0x%zx bytes by kmalloc\n", size);
			return __kmalloc(size, flags);
		}

		p_kc = skm_get_kmem_cache(size, flags);
		if (likely(p_kc))
			return kmem_cache_alloc(p_kc, flags);
		else {
			/* supper_kmalloc_create failed to create kmem_cache */
			return __kmalloc(size, flags);
		}
	} else
		return __kmalloc(size, flags);
}
EXPORT_SYMBOL(skmalloc);

void skfree(void *x)
{
	kfree(x);
}
EXPORT_SYMBOL(skfree);

static int supper_kmalloc_create(struct supper_kmalloc *p_sk)
{
	int i;
	size_t size;
	char *s;

	for (i = 0; i < MALLOC_CACHE_NUMS; i++) {
		size = skm_index_to_size(i);

		if (likely(size)) {
			/* TODO: scnprintf not to use kmalloc as kasprintf */
			s = kasprintf(GFP_KERNEL, "%s-%zu", p_sk->name, size);
			if (s) {
				p_sk->k_c[i] = kmem_cache_create(s, size, 0,
						p_sk->dbg_flags, NULL);
				kfree(s);
			} else
				p_sk->k_c[i] = NULL;
		} else {
			/* it is very unlikely */
			p_sk->k_c[i] = NULL;
		}
	}

	return 0;
}

static int supper_kmalloc_destory(struct supper_kmalloc *p_sk)
{
	int i;

	for (i = 0; i < MALLOC_CACHE_NUMS; i++) {
		if (p_sk->k_c[i]) {
			kmem_cache_destroy(p_sk->k_c[i]);
			p_sk->k_c[i] = NULL;
		}
	}

	return 0;
}

static int __init skmalloc_init(void)
{
	struct supper_kmalloc *p_sk;

	pr_info(">>>Create Supper Kmalloc start\n");

#ifdef CONFIG_SKMALLOC_REBOOT_NOTIFY
	skm_notifier_bootup_handle();
	register_reboot_notifier(&skm_reboot_notifier);
#endif

	pr_info("Supper Kmalloc skm_dbg_level=%d\n", skm_dbg_level);
	if (skm_dbg_level > 0 && skm_dbg_level <= SKM_DBG_LEVEL_MAX) {
		p_sk = skm_get_supper_kmalloc();
		p_sk->name = "skmalloc";
		p_sk->dbg_level = dbg_level[skm_dbg_level-1];
		p_sk->dbg_flags = dbg_flags[skm_dbg_level-1];
		supper_kmalloc_create(p_sk);
		pr_info("skm_dbg_flags=0x%x\n", p_sk->dbg_flags);
	} else {
		/* In fact, skm_dbg_level is 0 here */
		pr_info("use normal kmalloc instead\n");
	}

	pr_info("<<<Create Supper Kmalloc end\n");
	return 0;
}
/*
 * Make sure skmalloc_init is called at:
 * after SLUB system has been finished && before devices code is ready to run
 */
rootfs_initcall(skmalloc_init);

static void __exit skmalloc_exit(void)
{
	struct supper_kmalloc *p_sk;

	pr_info(">>>Destory Supper Kmalloc Start\n");

	if (skm_dbg_level > 0 && skm_dbg_level <= SKM_DBG_LEVEL_MAX) {
		p_sk = skm_get_supper_kmalloc();
		supper_kmalloc_destory(p_sk);
	}

#ifdef CONFIG_SKMALLOC_REBOOT_NOTIFY
	unregister_reboot_notifier(&skm_reboot_notifier);
#endif

	pr_info("<<<Destory Supper kmalloc end\n");
}
module_exit(skmalloc_exit);


MODULE_AUTHOR("Jialing Fu");
MODULE_DESCRIPTION("Supper Kmalloc APIs");
MODULE_LICENSE("GPL");

module_param(skm_dbg_level, int, 0);
MODULE_PARM_DESC(skm_dbg_level,
		 "0: Use kmalloc 1: Use skmalloc; 2: Use skmalloc with highest dbg level");
