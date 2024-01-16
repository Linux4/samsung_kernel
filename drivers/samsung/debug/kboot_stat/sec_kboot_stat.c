// SPDX-License-Identifier: GPL-2.0
/*
 * COPYRIGHT(C) 2022 Samsung Electronics Co., Ltd. All Right Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>

#include <linux/samsung/builder_pattern.h>
#include <linux/samsung/jump_table_target.h>

struct kboot_stat_node {
	struct list_head list;
	const char *name;
	int ret;
	ktime_t calltime;
	ktime_t rettime;
};

struct kboot_stat_logger {
	struct mutex lock;
	struct list_head head;
	struct proc_dir_entry *proc;
};

struct kboot_stat_drvdata {
	struct builder bd;
	struct kboot_stat_logger initdbg;
	struct kboot_stat_logger prbdbg;
};

static struct kboot_stat_drvdata __kboot_stat;
static struct kboot_stat_drvdata *kboot_stat = &__kboot_stat;

bool sec_kboot_stat_en __read_mostly;

static inline void __init __kboot_stat_init_logger(struct kboot_stat_logger *logger)
{
	INIT_LIST_HEAD(&logger->head);
	mutex_init(&logger->lock);
}

static int __init sec_kboot_stat_en_setup(char *str)
{
	sec_kboot_stat_en = true;

	/* NOTE: initialize mutex befor starting init calls */
	__kboot_stat_init_logger(&kboot_stat->initdbg);
	__kboot_stat_init_logger(&kboot_stat->prbdbg);

	return 1;
}
__setup("sec_kboot_stat", sec_kboot_stat_en_setup);

static inline struct kboot_stat_node *__kboot_stat_alloc_node(void)
{
	struct kboot_stat_node *node;

	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (node == NULL)
		return ERR_PTR(-ENOMEM);

	return node;
}

int notrace __sec_initcall_debug(initcall_t fn)
{
	struct kboot_stat_logger *logger = &kboot_stat->initdbg;
	struct kboot_stat_node *node;
	int ret;

	/* NOTE: do NOT use this feature if 'initcall_debug' is enabled.
	 * use a kernel internal implementaion.
	 */
	if (initcall_debug)
		return fn();

	node = __kboot_stat_alloc_node();
	BUG_ON(IS_ERR_OR_NULL(node));

	node->calltime = ktime_get();
	ret = fn();
	node->rettime = ktime_get();
	node->name = kasprintf(GFP_KERNEL, "%ps", jump_table_target(fn));
	node->ret = ret;

	mutex_lock(&logger->lock);
	list_add_tail(&node->list, &logger->head);
	mutex_unlock(&logger->lock);

	return ret;
}


int notrace __sec_probe_debug(int (*really_probe)(struct device *dev, struct device_driver *drv),
		struct device *dev, struct device_driver *drv)
{
	struct kboot_stat_logger *logger = &kboot_stat->prbdbg;
	struct kboot_stat_node *node;
	int ret;

	node = __kboot_stat_alloc_node();
	BUG_ON(IS_ERR_OR_NULL(node));

	node->calltime = ktime_get();
	ret = really_probe(dev, drv);
	node->rettime = ktime_get();
	node->ret = ret;
	node->name = kasprintf(GFP_KERNEL, "%s [%s]", dev_name(dev), drv->name);

	mutex_lock(&logger->lock);
	list_add_tail(&node->list, &logger->head);
	mutex_unlock(&logger->lock);

	return ret;
}

static int sec_kboot_stat_proc_show(struct seq_file *m, void *v)
{
	struct kboot_stat_logger *logger = m->private;
	struct list_head *head = &logger->head;
	struct kboot_stat_node *entry;

	mutex_lock(&logger->lock);

	list_for_each_entry(entry, head, list) {
		ktime_t delta = ktime_sub(entry->rettime, entry->calltime);

		seq_printf(m, "%s\t%d\t%lld\t%lld\t%lld\n",
				entry->name, entry->ret,
				(long long)ktime_to_us(entry->calltime),
				(long long)ktime_to_us(entry->rettime),
				(long long)ktime_to_us(delta));
	}

	mutex_unlock(&logger->lock);

	return 0;
}

static int sec_kboot_stat_proc_open(struct inode *inode, struct file *file)
{
	struct list_head *head = PDE_DATA(inode);

	return single_open(file, sec_kboot_stat_proc_show, head);
}

static const struct file_operations kboot_stat_fops = {
	.owner = THIS_MODULE,
	.open = sec_kboot_stat_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int __init __kboot_stat_init_proc_initdbg(struct builder *bd)
{
	struct kboot_stat_drvdata *drvdata =
	    container_of(bd, struct kboot_stat_drvdata, bd);
	struct kboot_stat_logger *logger = &drvdata->initdbg;
	const char *node_name = "sec_initcall_debug";
	struct proc_dir_entry *proc;

	proc = proc_create_data(node_name, 0440, NULL, &kboot_stat_fops,
			logger);
	if (!proc) {
		pr_warn("failed to create procfs node (%s)\n", node_name);
		return -ENODEV;
	}

	logger->proc = proc;

	return 0;
}

static int __init __kboot_stat_init_proc_prbdbg(struct builder *bd)
{
	struct kboot_stat_drvdata *drvdata =
	    container_of(bd, struct kboot_stat_drvdata, bd);
	struct kboot_stat_logger *logger = &drvdata->prbdbg;
	const char *node_name = "sec_probe_debug";
	struct proc_dir_entry *proc;

	proc = proc_create_data(node_name, 0440, NULL, &kboot_stat_fops,
			logger);
	if (!proc) {
		pr_warn("failed to create procfs node (%s)\n", node_name);
		return -ENODEV;
	}

	logger->proc = proc;

	return 0;
}

static const struct dev_builder __kboot_stat_dev_builder[] __initdata = {
	DEVICE_BUILDER(__kboot_stat_init_proc_initdbg, NULL),
	DEVICE_BUILDER(__kboot_stat_init_proc_prbdbg, NULL),
};

static int __init sec_kboot_stat_init(void)
{
	if (!sec_kboot_stat_en)
		return 0;

	return sec_director_probe_dev(&kboot_stat->bd,
			__kboot_stat_dev_builder, ARRAY_SIZE(__kboot_stat_dev_builder));
}
device_initcall(sec_kboot_stat_init);
