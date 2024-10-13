#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/module.h>
#include <soc/samsung/cal-if.h>

#include "cmucal.h"
#include "vclk.h"
#include "ra.h"

/***        debugfs support        ***/
#ifdef CONFIG_DEBUG_FS
#define MAX_NAME_SIZE	50

static struct dentry *rootdir;
static struct cmucal_clk *clk_info;
static struct vclk *dvfs_domain;
static unsigned int margin;
static unsigned int debug_freq;

extern unsigned int dbg_offset;
static unsigned int cmu_top_base = 0x0;

/*
blk_hwacg_feature : It will print all the gate clocks of the specified block.
parameters:
addr : address of the block
*/
void blk_hwacg_feature(unsigned long addr)
{
	struct cmucal_clk *clk;
	int size, reg;
	int i;

	size = cmucal_get_list_size(GATE_TYPE);
	for (i = 0; i < size ; i++) {
		clk = cmucal_get_node(i | GATE_TYPE);
		if (clk &&((clk->paddr & 0xFFFF0000) == (addr & 0xFFFF0000)))
		{
			reg = readl(clk->offset + dbg_offset);
			if ((reg & 0x70) != 0x30)
				printk("name %s : [0x%x] active\n", clk->name, reg);
			else
				printk("name %s : [0x%x] idle\n", clk->name, reg);
		}
	}
}

void print_clk_on_blk(void)
{
	struct cmucal_clk *clk;
	int size, reg;
	int i;

	if (cmu_top_base == 0x0) {
		pr_info("cmu_top_base is NULL\n");
		return ;
	}

	size = cmucal_get_list_size(PLL_TYPE);
	for (i = 0; i < size ; i++) {
		clk = cmucal_get_node(i | PLL_TYPE);
		if (!clk || (clk->paddr & 0xFFFF0000) != cmu_top_base)
			continue;

		reg = readl(clk->pll_con0);
		if ((reg >> 29) & 0x1)
			printk("name %s : [0x%x] active\n", clk->name, reg);
		else
			printk("name %s : [0x%x] idle\n", clk->name, reg);

	}
	size = cmucal_get_list_size(GATE_TYPE);

	for (i = 0; i < size ; i++) {
		clk = cmucal_get_node(i | GATE_TYPE);
		if (!clk || (clk->paddr & 0xFFFF0000) != cmu_top_base)
			continue;

		reg = readl(clk->offset + dbg_offset);
		if ((reg & 0x70) != 0x30)
			printk("name %s : [0x%x] active\n", clk->name, reg);
		else
			printk("name %s : [0x%x] idle\n", clk->name, reg);

	}
}

static int vclk_table_dump(struct seq_file *s, void *p)
{
	struct vclk *vclk = s->private;
	struct cmucal_clk *clk;
	int i, j;

	seq_puts(s, "-----------------------------------------------------\n");
	seq_printf(s, "%s <%x> rate = %lu \n",
		   vclk->name, vclk->id, vclk_recalc_rate(vclk->id));
	for (i = 0; i < vclk->num_list; i++) {
		clk = cmucal_get_node(vclk->list[i]);
		if (!clk)
			continue;
		seq_printf(s, " [%s] value : %lu rate : %u\n",
			   clk->name,
			   ra_get_value(clk->id),
			   vclk_debug_clk_get_rate(clk->id));
	}

	if (!vclk->lut)
		return 0;

	for (i = 0; i < vclk->num_rates; i++) {
		seq_printf(s, "[%2d]%7d :", i + 1, vclk->lut[i].rate);
		for (j = 0; j < vclk->num_list; j++)
			seq_printf(s, "%7d ", vclk->lut[i].params[j]);
		seq_puts(s, "\n");
	}

	return 0;
}

static int vclk_table_open(struct inode *inode, struct file *file)
{
	return single_open(file, vclk_table_dump, inode->i_private);
}

static const struct file_operations vclk_table_fops = {
	.open		= vclk_table_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int vclk_clk_info(struct seq_file *s, void *p)
{
	return 0;
}

static int vclk_clk_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, vclk_clk_info, inode->i_private);
}

static ssize_t
vclk_read_clk_info(struct file *filp, char __user *ubuf,
		       size_t cnt, loff_t *ppos)
{
	struct cmucal_clk *clk;
	struct cmucal_clk *parent;
	char buf[512];
	int r;

	clk = clk_info;
	if (clk == NULL) {
		r = sprintf(buf, "echo \"clk_name\" > clk_info\n");
	} else {
		r = sprintf(buf, "clk name : %s\n"
				 " id      : 0x%x\n"
				 " rate    : %u \n"
				 " value   : %lu\n"
				 " path    :\n", clk->name, clk->id,
				 vclk_debug_clk_get_rate(clk->id),
				 ra_get_value(clk->id));

		parent = ra_get_parent(clk->id);
		while (parent != NULL) {
			r += sprintf(buf + r, "<- %s ", parent->name);
			parent = ra_get_parent(parent->id);
		}
		r += sprintf(buf + r, "\n");
	}
	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t
vclk_write_clk_info(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	char buf[MAX_NAME_SIZE + 1];
	char *c_buf;
	unsigned int id;
	unsigned long c_addr;
	size_t ret;

	c_buf = buf;
	ret = cnt;

	if (cnt == 0)
		return cnt;

	if (cnt > MAX_NAME_SIZE)
		cnt = MAX_NAME_SIZE;

	if (copy_from_user(buf, ubuf, cnt))
		return -EVCLKFAULT;

	if (buf[cnt-1] == '\n')
		buf[cnt-1] = 0;
	else
		buf[cnt] = 0;

	if (!strcmp(buf, "hwacg")) {
		print_clk_on_blk();
	} else if(!strcmp(strsep(&c_buf," "),"blk_hwacg")) {
		if (kstrtol(strsep(&c_buf," "), 16, &c_addr) == 0)
			blk_hwacg_feature(c_addr);
	} else {
		id = cmucal_get_id(buf);
		clk_info = cmucal_get_node(id);
	}
	*ppos += ret;
	return cnt;
}

static ssize_t
vclk_read_dvfs_domain(struct file *filp, char __user *ubuf,
		       size_t cnt, loff_t *ppos)
{
	struct vclk *vclk;
	char buf[512];
	int i, size;
	int r;

	vclk = dvfs_domain;
	if (vclk == NULL)
		r = sprintf(buf, "echo id > dvfs_domain\n");
	else
		r = sprintf(buf, "%s : 0x%x\n", dvfs_domain->name, dvfs_domain->id);

	r += sprintf(buf + r, "- dvfs list\n");

	size = cmucal_get_list_size(ACPM_VCLK_TYPE);
	for (i = 0; i < size ; i++) {
		vclk = cmucal_get_node(ACPM_VCLK_TYPE | i);
		if (vclk == NULL)
			continue;
		r += sprintf(buf + r, "  %s : 0x%x\n", vclk->name, vclk->id);
	}

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t
vclk_write_dvfs_domain(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	char buf[16];
	ssize_t len;
	u32 id;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, cnt);
	if (len < 0)
		return len;

	buf[len] = '\0';

	if (!kstrtouint(buf, 0, &id)) {
		dvfs_domain = cmucal_get_node(id);
		if (!dvfs_domain || !IS_ACPM_VCLK(dvfs_domain->id))
			dvfs_domain = NULL;
	}

	return len;
}

static ssize_t
vclk_read_set_margin(struct file *filp, char __user *ubuf,
		       size_t cnt, loff_t *ppos)
{
	char buf[512];
	int r;

	r = sprintf(buf, "margin : %u\n", margin);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t
vclk_write_set_margin(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	char buf[16];
	ssize_t len;
	u32 volt;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, cnt);
	if (len < 0)
		return len;

	buf[len] = '\0';
	if (dvfs_domain && !kstrtoint(buf, 0, &volt)) {
		margin = volt;
		cal_dfs_set_volt_margin(dvfs_domain->id, volt);
	}

	return len;
}

static ssize_t
vclk_read_set_freq(struct file *filp, char __user *ubuf,
		       size_t cnt, loff_t *ppos)
{
	char buf[512];
	int r;

	r = sprintf(buf, "freq : %u\n", debug_freq);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static ssize_t
vclk_write_set_freq(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	char buf[16];
	ssize_t len;
	u32 freq;

	len = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, ubuf, cnt);
	if (len < 0)
		return len;

	buf[len] = '\0';
	if (dvfs_domain && !kstrtoint(buf, 0, &freq)) {
		debug_freq = freq;
		cal_dfs_set_rate(dvfs_domain->id, freq);
	}

	return len;
}

static const struct file_operations clk_info_fops = {
	.open		= vclk_clk_info_open,
	.read		= vclk_read_clk_info,
	.write		= vclk_write_clk_info,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static const struct file_operations dvfs_domain_fops = {
	.open		= simple_open,
	.read		= vclk_read_dvfs_domain,
	.write		= vclk_write_dvfs_domain,
	.llseek		= seq_lseek,
};

static const struct file_operations set_margin_fops = {
	.open		= simple_open,
	.read		= vclk_read_set_margin,
	.write		= vclk_write_set_margin,
	.llseek		= seq_lseek,
};

static const struct file_operations set_freq_fops = {
	.open		= simple_open,
	.read		= vclk_read_set_freq,
	.write		= vclk_write_set_freq,
	.llseek		= seq_lseek,
};

/* caller must hold prepare_lock */
static int vclk_debug_create_one(struct vclk *vclk, struct dentry *pdentry)
{
	struct dentry *d;
	int ret = -ENOMEM;

	if (!vclk || !pdentry) {
		ret = -EINVAL;
		goto out;
	}

	d = debugfs_create_dir(vclk->name, pdentry);
	if (!d)
		goto out;

	vclk->dentry = d;
#if 0
	d = debugfs_create_x32("vclk_id", S_IRUSR, vclk->dentry,
			(u32 *)&vclk->id);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("vclk_rate", S_IRUSR, vclk->dentry,
			(u32 *)&vclk->vrate);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("vclk_num_rates", S_IRUSR, vclk->dentry,
			(u32 *)&vclk->num_rates);
	if (!d)
		goto err_out;

	d = debugfs_create_u32("vclk_num_list", S_IRUSR, vclk->dentry,
			(u32 *)&vclk->num_list);
	if (!d)
		goto err_out;

	d = debugfs_create_file("vclk_table", S_IRUSR, vclk->dentry, vclk,
				&vclk_table_fops);
	if (!d)
		return -ENOMEM;

	ret = 0;
	goto out;

err_out:
	debugfs_remove_recursive(vclk->dentry);
	vclk->dentry = NULL;
#endif

        debugfs_create_x32("vclk_id", 0400, vclk->dentry, (u32 *)&vclk->id);

        debugfs_create_u32("vclk_rate", 0400, vclk->dentry, (u32 *)&vclk->vrate);

        debugfs_create_u32("vclk_num_rates", 0400, vclk->dentry, (u32 *)&vclk->num_rates);

        debugfs_create_u32("vclk_num_list", 0400, vclk->dentry, (u32 *)&vclk->num_list);

        d = debugfs_create_file("vclk_table", 0400, vclk->dentry, vclk,
                                &vclk_table_fops);
        if (!d)
                return -ENOMEM;

        ret = 0;
        goto out;

out:
	return ret;
}

unsigned int vclk_debug_clk_get_rate(unsigned int id)
{
	unsigned long rate;

	rate = ra_recalc_rate(id);

	return rate;
}
EXPORT_SYMBOL_GPL(vclk_debug_clk_get_rate);

unsigned long vclk_debug_clk_get_value(unsigned int id)
{
	unsigned long val;

	val = ra_get_value(id);

	return val;
}
EXPORT_SYMBOL_GPL(vclk_debug_clk_get_value);

int vclk_debug_clk_set_value(unsigned int id, unsigned int params)
{
	int ret;

	ret = ra_set_value(id, params);

	return ret;
}
EXPORT_SYMBOL_GPL(vclk_debug_clk_set_value);

void cmucal_dbg_set_cmu_top_base(u32 base_addr)
{
	cmu_top_base = base_addr;
	pr_info("cmu_top_base : 0x%x\n", base_addr);
}
EXPORT_SYMBOL_GPL(cmucal_dbg_set_cmu_top_base);
/**
 * vclk_debug_init - lazily create the debugfs clk tree visualization
 */
int vclk_debug_init(void)
{
	struct vclk *vclk;
	struct dentry *d;
	int i;

	rootdir = debugfs_create_dir("vclk", NULL);

	if (!rootdir)
		return -ENOMEM;

	for (i = 0; i < cmucal_get_list_size(VCLK_TYPE); i++) {
		vclk = cmucal_get_node(i | VCLK_TYPE);
		if (!vclk)
			continue;
		vclk_debug_create_one(vclk, rootdir);
	}

	for (i = 0; i < cmucal_get_list_size(ACPM_VCLK_TYPE); i++) {
		vclk = cmucal_get_node(i | ACPM_VCLK_TYPE);
		if (!vclk)
			continue;
		vclk_debug_create_one(vclk, rootdir);
	}

	d = debugfs_create_file("clk_info", 0600, rootdir, NULL,
				&clk_info_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("dvfs_domain", 0600, rootdir, NULL,
				&dvfs_domain_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("set_margin", 0600, rootdir, NULL,
				&set_margin_fops);
	if (!d)
		return -ENOMEM;

	d = debugfs_create_file("set_freq", 0600, rootdir, NULL,
				&set_freq_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(vclk_debug_init);
#endif
MODULE_LICENSE("GPL");
