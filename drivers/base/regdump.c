/*
 *  regdump.c - framework for dump the system periperal registers.
 *
 *  Copyright (C) 2012 Lei Wen <leiwen@marvell.com>, Marvell Inc.
 *
 *  This file is released under the GPLv2.
 */

#include <linux/regdump_ops.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <asm/io.h>

#define REGDUMP_READLINE_NUM		80
#ifdef CONFIG_SMP
static DECLARE_RWSEM(all_cpu_access_lock);

static inline void dump_access_lock(void)
{
	down_write(&all_cpu_access_lock);
}

static inline void dump_access_unlock(void)
{
	up_write(&all_cpu_access_lock);
}

#else

static DEFINE_MUTEX(access_lock);

static inline void dump_access_lock(void)
{
	mutex_lock(&access_lock);
}

static inline void dump_access_unlock(void)
{
	mutex_unlock(&access_lock);
}
#endif

struct dentry *d_regdump;
struct dentry *regdump_init_dentry(void)
{
	static int once;

	if (d_regdump)
		return d_regdump;

	if (!debugfs_initialized()) {
		pr_warning("Debugfs not init yet!!!\n");
		return NULL;
	}

	d_regdump = debugfs_create_dir("regdump", NULL);
	if (!d_regdump && !once) {
		once = 1;
		pr_warning("Could not create debugfs directory 'regdump'\n");
		return NULL;
	}

	return d_regdump;
}

static LIST_HEAD(regdump_ops_list);
static DEFINE_MUTEX(regdump_ops_lock);

static unsigned long regdump_read_region(struct regdump_ops *ops,
					 struct regdump_region *region)
{
	unsigned long val, avail;

	avail = region->cond(ops);
	if (!avail)
		return 0;

	switch (region->size) {
	case 4:
		val = readl(ops->base + region->start);
		break;
	case 2:
		val = readw(ops->base + region->start);
		break;
	case 1:
		val = readb(ops->base + region->start);
		break;
	default:
		printk(KERN_ERR "not valid reg size %lx\n", region->size);
		return 0;
	}

	return val;
}

int dump_reg_to_mem(void)
{
	struct regdump_ops *ops;
	struct regdump_region *region;
	unsigned long val;
	void *buf;
	int i;

	dump_access_lock();
	list_for_each_entry_reverse(ops, &regdump_ops_list, node)
		if (ops->enable) {
			region = ops->regions;
			buf = ops->buffer;
			for (i = 0; i < ops->reg_nums; i ++, region ++) {
				val = regdump_read_region(ops, region);
				memcpy(buf, &val, region->size);
				buf += region->size;
			}
		}
	dump_access_unlock();

	return 0;
}
EXPORT_SYMBOL_GPL(dump_reg_to_mem);

DEFINE_MUTEX(dev_mutex);
static int regdump_generic_open(struct inode *inode, struct file *filp)
{
	filp->private_data = inode->i_private;
	return 0;
}

static ssize_t
dev_reg_enable_read(struct file *filp, char __user *ubuf, size_t cnt,
		    loff_t *ppos)
{
	struct regdump_ops *ops= filp->private_data;
	char buf[2];
	int ret;

	mutex_lock(&dev_mutex);
	buf[0] = ops->enable ? '1' : '0';
	mutex_unlock(&dev_mutex);
	buf[1] = '\n';

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, 2);

	return ret;
}

static ssize_t
dev_reg_enable_write(struct file *filp, const char __user *ubuf, size_t cnt,
		     loff_t *ppos)
{
	struct regdump_ops *ops= filp->private_data;
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return -EINVAL;

	*ppos += cnt;
	ops->enable = val;

	return cnt;
}

static const struct file_operations dev_enable_fops = {
	.open = regdump_generic_open,
	.read = dev_reg_enable_read,
	.write = dev_reg_enable_write,
};

static ssize_t format_reg(struct regdump_ops *ops, char *buf, size_t cnt)
{
	unsigned long val;
	int i, ret, n = 0, limit;
	struct regdump_region *region;

	n = snprintf(buf, REGDUMP_READLINE_NUM, "\nDev: %s\n", ops->dev_name);
	if (n < 0)
		return n;

	cnt -= n;
	region = ops->regions;
	for (i = 0; i < ops->reg_nums; i ++, region ++) {
		val = regdump_read_region(ops, region);
		limit = (cnt > REGDUMP_READLINE_NUM) ?
			REGDUMP_READLINE_NUM : cnt;
		ret = snprintf(buf + n, limit, "%s: 0x%08lx\n",
				region->name, val);
		if (ret < 0)
			return ret;

		n += ret;
		cnt -= ret;
	}

	return n;
}

static ssize_t
dev_reg_read(struct file *filp, char __user *ubuf, size_t cnt,
		    loff_t *ppos)
{
	struct regdump_ops *ops= filp->private_data;
	int ret;
	static void *buf = NULL;
	static int buf_sz = 0;

	if (!ops->enable) {
		pr_err("Dev %s regdump not enabled!\n", ops->dev_name);
		return 0;
	}

	if (!buf) {
		buf_sz = REGDUMP_READLINE_NUM * (ops->reg_nums + 1);
		buf = kmalloc(buf_sz, GFP_KERNEL);
		if (!buf)
			return -ENOMEM;

		buf_sz = format_reg(ops, buf, buf_sz);
		if (buf_sz < 0) {
			kfree(buf);
			return buf_sz;
		}
	}

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, buf_sz);

	if (ret <= 0) {
		kfree(buf);
		buf = NULL;
	}

	return ret;
}

static const struct file_operations dev_regdump_fops = {
	.open = regdump_generic_open,
	.read = dev_reg_read,
};

static void *
r_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct regdump_ops *ops = v;

	(*pos)++;

	list_for_each_entry_continue(ops, &regdump_ops_list, node)
		if (ops->enable)
			return ops;

	return NULL;
}

static void *r_start(struct seq_file *m, loff_t *pos)
{
	struct regdump_ops *ops;
	loff_t l;

	mutex_lock(&dev_mutex);
	ops = list_entry(&regdump_ops_list, struct regdump_ops, node);
	for (l = 0; l <= *pos; ) {
		ops = r_next(m, ops, &l);
		if (!ops)
			break;
	}

	return ops;
}

static void r_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&dev_mutex);
}

static int r_show(struct seq_file *m, void *v)
{
	struct regdump_ops *ops = v;

	if (!ops)
		return 0;

	seq_printf(m, "%s\n", ops->dev_name);

	return 0;
}

static const struct seq_operations show_regdump_seq_ops = {
	.start		= r_start,
	.next		= r_next,
	.stop		= r_stop,
	.show		= r_show,
};

static int regdump_seqfile_generic_open(struct inode *inode, struct file *file)
{
	const struct seq_operations *seq_ops;

	seq_ops = inode->i_private;
	return seq_open(file, seq_ops);
}

static const struct file_operations show_regdump_fops = {
	.open		= regdump_seqfile_generic_open,
	.read		= seq_read,
	.release	= seq_release,
	.llseek		= seq_lseek,
};

static int d_show(struct seq_file *m, void *v)
{
	struct regdump_ops *ops = v;
	int ret;

	if (!ops)
		return 0;

	ret = format_reg(ops, m->buf, m->size);
	if (ret < 0)
		return ret;

	m->count += ret;
	return 0;
}

static const struct seq_operations dump_all_regs_seq_ops = {
	.start		= r_start,
	.next		= r_next,
	.stop		= r_stop,
	.show		= d_show,
};

static const struct file_operations dump_allregs_fops = {
	.open		= regdump_seqfile_generic_open,
	.read		= seq_read,
	.release	= seq_release,
	.llseek		= seq_lseek,
};

static ssize_t
regdump_all_status(struct file *filp, char __user *ubuf, size_t cnt,
		    loff_t *ppos)
{
	struct regdump_ops *ops;
	char buf[2];
	int enable = 1, ret;

	mutex_lock(&dev_mutex);
	list_for_each_entry_reverse(ops, &regdump_ops_list, node)
		if (!ops->enable) {
			enable = 0;
			break;
		}
	mutex_unlock(&dev_mutex);

	buf[0] = enable ? '1' : '0';
	buf[1] = '\n';

	ret = simple_read_from_buffer(ubuf, cnt, ppos, buf, 2);

	return ret;
}

static ssize_t
regdump_all_enable(struct file *filp, const char __user *ubuf, size_t cnt,
		     loff_t *ppos)
{
	struct regdump_ops *ops;
	char buf[64];
	unsigned long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtoul(buf, 10, &val);
	if (ret < 0)
		return ret;

	if (val != 0 && val != 1)
		return -EINVAL;

	*ppos += cnt;
	mutex_lock(&dev_mutex);
	list_for_each_entry_reverse(ops, &regdump_ops_list, node)
		ops->enable = val;
	mutex_unlock(&dev_mutex);

	return cnt;
}

static const struct file_operations enable_regdump_fops = {
	.read = regdump_all_status,
	.write = regdump_all_enable,
};

/**
 * register_regdump_ops - Register a set of system core operations.
 * @ops: System core operations to register.
 */
int register_regdump_ops(struct regdump_ops *ops)
{
	int i, total_sz = 0;
	struct dentry *d_regdump, *d_dev;

	mutex_lock(&regdump_ops_lock);
	for (i = 0; i < ops->reg_nums; i ++)
		total_sz += ops->regions[i].size;

	if (!total_sz) {
		ops->buffer = kmalloc(total_sz, GFP_KERNEL);
		if (!ops->buffer) {
			printk(KERN_ERR "REGDUMP: fail to alloc mem for %s\n",
					ops->dev_name);
			mutex_unlock(&regdump_ops_lock);
			return -ENOMEM;
		}
	} else
		ops->buffer = NULL;
	ops->enable = 1;

	list_add_tail(&ops->node, &regdump_ops_list);
	mutex_unlock(&regdump_ops_lock);

	d_regdump = regdump_init_dentry();
	if (!d_regdump)
		return -ENODEV;

	if (!ops->dev_name) {
		pr_warning("Regdump: need specify name\n");
		return -EINVAL;
	}

	d_dev = debugfs_create_dir(ops->dev_name, d_regdump);
	if (!d_dev) {
		pr_warning("Could not create debugfs directory \'%s\'",
				ops->dev_name);
		return -EINVAL;
	}
	debugfs_create_file("enable", 0644, d_dev,
			ops, &dev_enable_fops);
	debugfs_create_file("dump", 0444, d_dev,
			ops, &dev_regdump_fops);

	return 0;
}
EXPORT_SYMBOL_GPL(register_regdump_ops);

/**
 * unregister_regdump_ops - Unregister a set of system core operations.
 * @ops: System core operations to unregister.
 */
int unregister_regdump_ops(struct regdump_ops *ops)
{
	mutex_lock(&regdump_ops_lock);
	if (ops->buffer)
		kfree(ops->buffer);

	list_del(&ops->node);
	mutex_unlock(&regdump_ops_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_regdump_ops);

static __init int regdump_init_debugfs(void)
{
	struct dentry *d_regdump;

	d_regdump = regdump_init_dentry();
	debugfs_create_file("current_enabled_devices", 0444, d_regdump,
			(void *)&show_regdump_seq_ops, &show_regdump_fops);
	debugfs_create_file("dump_all", 0444, d_regdump,
			(void *)&dump_all_regs_seq_ops, &dump_allregs_fops);
	debugfs_create_file("enable_all", 0644, d_regdump,
			NULL, &enable_regdump_fops);

	return 0;
}

fs_initcall(regdump_init_debugfs);
