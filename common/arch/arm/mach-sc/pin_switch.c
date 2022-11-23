#include <linux/io.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <mach/hardware.h>

#ifdef CONFIG_PROC_FS
struct sci_pin_switch {
	const char *dirname;
	const char *filename;
	u32 reg;
	u32 bit_offset;
	u32 bit_width;
	u32 func;
};
static struct sci_pin_switch sci_pin_switch_array[] = {
#if !defined(CONFIG_ARCH_SCX15)
	{"", "vbc_dac_iislrck_pin_in_sel", 0x4, 24, 1, 0},
	{"", "vbc_dac_iisclk_pin_in_sel", 0x4, 23, 1, 0},
	{"", "vbc_adc_iislrck_pin_in_sel", 0x4, 22, 1, 0},
	{"", "vbc_adc_iisdi_pin_in_sel", 0x4, 21, 1, 0},
	{"", "vbc_adc_iisclk_pin_in_sel", 0x4, 20, 1, 0},
	{"", "iis3mck_pin_in_sel", 0x4, 19, 1, 0},
	{"", "iis3lrck_pin_in_sel", 0x4, 18, 1, 0},
	{"", "iis3di_pin_in_sel", 0x4, 17, 1, 0},
	{"", "iis3clk_pin_in_sel", 0x4, 16, 1, 0},
	{"", "iis2mck_pin_in_sel", 0x4, 15, 1, 0},
	{"", "iis2lrck_pin_in_sel", 0x4, 14, 1, 0},
	{"", "iis2di_pin_in_sel", 0x4, 13, 1, 0},
	{"", "iis2clk_pin_in_sel", 0x4, 12, 1, 0},
#endif
	{"", "iis23_loop_sel", 0xc, 5, 1, 0},
	{"", "iis13_loop_sel", 0xc, 4, 1, 0},
	{"", "iis12_loop_sel", 0xc, 3, 1, 0},
	{"", "iis03_loop_sel", 0xc, 2, 1, 0},
	{"", "iis02_loop_sel", 0xc, 1, 1, 0},
	{"", "iis01_loop_sel", 0xc, 0, 1, 0},
};

static struct sci_pin_switch bt_iis_sys_sel_array[] = {
	{"bt_iis_sys_sel", "cp0_iis0", 0x8, 28, 4, 0},
	{"bt_iis_sys_sel", "cp0_iis1", 0x8, 28, 4, 1},
	{"bt_iis_sys_sel", "cp0_iis2", 0x8, 28, 4, 2},
	{"bt_iis_sys_sel", "cp0_iis3", 0x8, 28, 4, 3},
	{"bt_iis_sys_sel", "cp1_iis0", 0x8, 28, 4, 4},
	{"bt_iis_sys_sel", "cp1_iis1", 0x8, 28, 4, 5},
	{"bt_iis_sys_sel", "cp1_iis2", 0x8, 28, 4, 6},
	{"bt_iis_sys_sel", "cp1_iis3", 0x8, 28, 4, 7},
	{"bt_iis_sys_sel", "ap_iis0", 0x8, 28, 4, 8},
	{"bt_iis_sys_sel", "ap_iis1", 0x8, 28, 4, 9},
	{"bt_iis_sys_sel", "ap_iis2", 0x8, 28, 4, 10},
	{"bt_iis_sys_sel", "ap_iis3", 0x8, 28, 4, 11},
	{"bt_iis_sys_sel", "vbc_iis", 0x8, 28, 4, 12},
};

static struct sci_pin_switch iis_0_array[] = {
	{"iis0_sys_sel", "ap_iis0", 0xc, 6, 2, 0},
	{"iis0_sys_sel", "cp0_iis0", 0xc, 6, 2, 1},
	{"iis0_sys_sel", "cp1_iis0", 0xc, 6, 2, 2},
	{"iis0_sys_sel", "cp2_iis0", 0xc, 6, 2, 3},
};

static struct sci_pin_switch iis_1_array[] = {
	{"iis1_sys_sel", "ap_iis1", 0xc, 9, 2, 0},
	{"iis1_sys_sel", "cp0_iis1", 0xc, 9, 2, 1},
	{"iis1_sys_sel", "cp1_iis1", 0xc, 9, 2, 2},
	{"iis1_sys_sel", "cp2_iis1", 0xc, 9, 2, 3},
};

#if !defined(CONFIG_ARCH_SCX15)
static struct sci_pin_switch iis_2_array[] = {
	{"iis2_sys_sel", "ap_iis2", 0xc, 12, 2, 0},
	{"iis2_sys_sel", "cp0_iis2", 0xc, 12, 2, 1},
	{"iis2_sys_sel", "cp1_iis2", 0xc, 12, 2, 2},
	{"iis2_sys_sel", "cp2_iis2", 0xc, 12, 2, 3},
};

static struct sci_pin_switch iis_3_array[] = {
	{"iis3_sys_sel", "ap_iis3", 0xc, 15, 2, 0},
	{"iis3_sys_sel", "cp0_iis3", 0xc, 15, 2, 1},
	{"iis3_sys_sel", "cp1_iis3", 0xc, 15, 2, 2},
	{"iis3_sys_sel", "cp2_iis3", 0xc, 15, 2, 3},
};
#endif

static struct sci_pin_switch_dir {
	struct sci_pin_switch *sci_pin_switch;
	u32 array_size;
} sci_pin_switch_dir_array[] = {
	{bt_iis_sys_sel_array, ARRAY_SIZE(bt_iis_sys_sel_array)},
	{iis_0_array, ARRAY_SIZE(iis_0_array)},
	{iis_1_array, ARRAY_SIZE(iis_1_array)},
#if !defined(CONFIG_ARCH_SCX15)
	{iis_2_array, ARRAY_SIZE(iis_1_array)},
	{iis_3_array, ARRAY_SIZE(iis_1_array)},
#endif
	};

/*
*	#define IIS_TO_AP		(0)
*	#define IIS_TO_CP0		(1)
*	#define IIS_TO_CP1		(2)
*	#define IIS_TO_CP2		(3)
*	#define PIN_CTL_REG3 (SPRD_PIN_BASE + 0xc)
*/
static int read_write_pin_switch(int is_read, int v, struct sci_pin_switch *p)
{
	int val = 0;
	u32 shift = p->bit_offset;
	u32 pin_ctl_reg = SPRD_PIN_BASE + p->reg;
	u32 mask = (1 << (p->bit_width)) - 1;
	if ((shift > 31))
		BUG_ON(1);
	if (v > mask)
		printk("v:0x%x overflow bitwidth:%d, mask:0x%x it\n",
		       v, p->bit_width, mask);
	val = __raw_readl(pin_ctl_reg);
	if (is_read) {
		val >>= shift;
		val &= mask;
		return val;
	} else {
		val &= ~(mask << shift);
		val |= (v & mask) << shift;
		__raw_writel(val, pin_ctl_reg);
	}
	return val;
}

static int pin_switch_proc_show(struct seq_file *m, void *v)
{
	int val = 0;
	struct sci_pin_switch *p = (struct sci_pin_switch *)(m->private);
	val = read_write_pin_switch(1, (int)val, p);
	seq_printf(m, "%d\n", val);
	return 0;
}

static int pin_switch_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pin_switch_proc_show, PDE_DATA(inode));
}

static ssize_t pin_switch_proc_write(struct file *file,
				     const char __user * buffer,
				     size_t count, loff_t * pos)
{
	char lbuf[32];
	long val = 0;
	int ret = 0;
	struct sci_pin_switch *p =
	    (struct sci_pin_switch *)(PDE_DATA(file_inode(file)));
	ret = kstrtol_from_user(buffer, count, 0, &val);
	if(ret){
		pr_err("input err\n");
		return -EINVAL;
	}
	read_write_pin_switch(0, val, p);
	return count;
}

static const struct file_operations pin_switch_fops = {
	.open = pin_switch_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = pin_switch_proc_write,
};

static int pin_switch_dir_proc_show(struct seq_file *m, void *v)
{
	int val = 0;
	int func_tmp = 0;
	struct sci_pin_switch *p = (struct sci_pin_switch *)m->private;
	func_tmp = read_write_pin_switch(1, val, p);
	if (p->func == func_tmp)
		val = 1;
	else
		val = 0;
	seq_printf(m, "%d\n", val);
	return 0;
}

static int pin_switch_dir_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pin_switch_dir_proc_show, PDE_DATA(inode));
}

static ssize_t pin_switch_dir_proc_write(struct file *file,
					 const char __user * buffer,
					 size_t count, loff_t * pos)
{
	char lbuf[32];
	int val = 0;
	int ret =0;
	struct sci_pin_switch *p =
	    (struct sci_pin_switch *)(PDE_DATA(file_inode(file)));
	ret = kstrtol_from_user(buffer, count, 0, &val);
	if(ret){
		pr_err("input err\n");
		return -EINVAL;
	}
	if (val == 1)
		val = p->func;
	else			/*if val = 0 or other value, just ignore it */
		return 0;
	read_write_pin_switch(0, val, p);
	return count;
}

static const struct file_operations pin_switch_dir_fops = {
	.open = pin_switch_dir_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
	.write = pin_switch_dir_proc_write,
};

static struct proc_dir_entry *pin_switch_proc_base;
static int __init pin_switch_proc_add(struct sci_pin_switch *pin_switch)
{
	struct proc_dir_entry *tmp_proc;
	tmp_proc =
	    proc_create_data(pin_switch->filename, S_IALLUGO,
			     pin_switch_proc_base, &pin_switch_fops,
			     pin_switch);
	if (!tmp_proc)
		return -ENOMEM;
	return 0;
}

static int __init pin_switch_proc_add_dir(struct sci_pin_switch_dir
					  *pin_switch_dir)
{
	struct proc_dir_entry *tmp_proc_dir;
	struct proc_dir_entry *tmp_proc;
	int i;
	if (pin_switch_dir->sci_pin_switch->dirname == NULL)
		return EINVAL;
	/* has dir name, first create parent dir */
	tmp_proc_dir =
	    proc_mkdir(pin_switch_dir->sci_pin_switch->dirname,
		       pin_switch_proc_base);
	if (!tmp_proc_dir)
		return -ENOMEM;

	for (i = 0; i < pin_switch_dir->array_size; ++i) {
		if (pin_switch_dir->sci_pin_switch[i].filename == NULL)
			return EINVAL;
		tmp_proc =
		    proc_create_data(pin_switch_dir->sci_pin_switch[i].filename,
				     S_IALLUGO, tmp_proc_dir,
				     &pin_switch_dir_fops,
				     &pin_switch_dir->sci_pin_switch[i]);
		if (!tmp_proc)
			return -EINVAL;
	}
	return 0;
}

int __init pin_switch_proc_init(void)
{
	int i;
	pin_switch_proc_base = proc_mkdir("pin_switch", NULL);
	if (!pin_switch_proc_base)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(sci_pin_switch_array); ++i) {
		pin_switch_proc_add(&sci_pin_switch_array[i]);
	}
	for (i = 0; i < ARRAY_SIZE(sci_pin_switch_dir_array); ++i) {
		pin_switch_proc_add_dir(&sci_pin_switch_dir_array[i]);
	}
	return 0;
}

late_initcall(pin_switch_proc_init);
#else
#error "CONFIG_PROC_FS needed by mach-sc/pin_switch"
#endif /* CONFIG_PROC_FS */
