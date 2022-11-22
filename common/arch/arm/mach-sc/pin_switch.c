//      #define DEBUG
//      #define pr_fmt(fmt) "---sprd pinswitch---"fmt

#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/printk.h>
#include <mach/hardware.h>
#include <mach/arch_lock.h>
#include <linux/of.h>
#include <mach/pin_switch.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_PROC_FS

#ifdef CONFIG_OF
#undef SPRD_PIN_BASE
static unsigned int SPRD_PIN_BASE;
#endif

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
#if defined(CONFIG_ARCH_SCX30G)	/*tshark add */
	{"", "bt_iis_con_eb", 0xc, 30, 1, 0},
	{"", "pad_oe_iis3_mck", 0xc, 27, 1, 0},
	{"", "pad_oe_iis2_mck", 0xc, 26, 1, 0},
	{"", "pad_oe_iis1_mck", 0xc, 25, 1, 0},
	{"", "pad_oe_iis0_mck", 0xc, 24, 1, 0},
#endif
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
	{"bt_iis_sys_sel", "rsv_d", 0x8, 28, 4, 13},
	{"bt_iis_sys_sel", "rsv_e", 0x8, 28, 4, 14},
	{"bt_iis_sys_sel", "rsv_f", 0x8, 28, 4, 15},
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

#if defined(CONFIG_PIN_POWER_DOMAIN_SWITCH)
struct pin_reg_desc {
	struct sprd_pin_switch_platform_data *platform_data;
	struct notifier_block nb;
	struct regulator_bulk_data supplies;
} pin_reg_desc_array[PD_CNT];
#endif

static struct sci_pin_switch_dir {
	struct sci_pin_switch *sci_pin_switch;
	u32 array_size;
} sci_pin_switch_dir_array[] = {
	{
	bt_iis_sys_sel_array, ARRAY_SIZE(bt_iis_sys_sel_array)}, {
	iis_0_array, ARRAY_SIZE(iis_0_array)}, {
	iis_1_array, ARRAY_SIZE(iis_1_array)},
#if !defined(CONFIG_ARCH_SCX15)
	{
	iis_2_array, ARRAY_SIZE(iis_1_array)}, {
	iis_3_array, ARRAY_SIZE(iis_1_array)},
#endif
};

u32 pinmap_get(u32 offset)
{
	return readl(SPRD_PIN_BASE + offset);
}

EXPORT_SYMBOL(pinmap_get);
int pinmap_set(u32 offset, u32 value)
{
	unsigned long flags;
	__arch_default_lock(HWLOCK_GLB, &flags);
	writel(value, SPRD_PIN_BASE + offset);
	__arch_default_unlock(HWLOCK_GLB, &flags);
	return 0;
}

EXPORT_SYMBOL(pinmap_set);

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
	u32 mask = (1 << (p->bit_width)) - 1;
	if ((shift > 31))
		BUG_ON(1);
	if (v > mask)
		printk("v:0x%x overflow bitwidth:%ud, mask:0x%x it\n",
		       v, p->bit_width, mask);
	val = pinmap_get(p->reg);
	if (is_read) {
		val >>= shift;
		val &= mask;
		return val;
	} else {
		val &= ~(mask << shift);
		val |= (v & mask) << shift;
		pinmap_set(p->reg, val);
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
	long val = 0;
	int ret = 0;
	struct sci_pin_switch *p =
	    (struct sci_pin_switch *)(PDE_DATA(file_inode(file)));
	ret = kstrtol_from_user(buffer, count, 0, &val);
	if (ret) {
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
	int val = 0;
	int ret = 0;
	struct sci_pin_switch *p =
	    (struct sci_pin_switch *)(PDE_DATA(file_inode(file)));
	ret = kstrtol_from_user(buffer, count, 0, &val);
	if (ret) {
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
	    proc_create_data(pin_switch->filename, S_IRWXUGO,
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
				     S_IRWXUGO, tmp_proc_dir,
				     &pin_switch_dir_fops,
				     &pin_switch_dir->sci_pin_switch[i]);
		if (!tmp_proc)
			return -EINVAL;
	}
	return 0;
}

#if defined(CONFIG_PIN_POWER_DOMAIN_SWITCH)
static int sc271x_regulator_event(struct notifier_block *regu_nb,
				  unsigned long event, void *data)
{
	unsigned long flags;
	struct pin_reg_desc *p_pin_reg_desc =
	    container_of(regu_nb, struct pin_reg_desc, nb);
	unsigned long best_data = (unsigned long *)data;
	const char *regu_name;
	u32 bit;
	u32 reg;

	if (!p_pin_reg_desc) {
		return -EINVAL;
	}
	regu_name = p_pin_reg_desc->supplies.supply;
	bit = p_pin_reg_desc->platform_data->bit_offset;
	reg = p_pin_reg_desc->platform_data->reg;
	pr_info("event:0x%x, best_data:0x%x\n", event, best_data);

	if (event & REGULATOR_EVENT_VOLTAGE_CHANGE) {
		if (p_pin_reg_desc) {
			if (best_data > VOL_THRESHOLD) {
				pinmap_set(reg, (pinmap_get(reg) & ~(1 << bit)));
				pr_info
				    ("%s()->Line:%d, --REGULATOR_EVENT_VOLTAGE_CHANGE--> %s event %ld, data %ld\n",
				     __func__, __LINE__, regu_name, event,
				     best_data);
			} else {
				pinmap_set(reg, (pinmap_get(reg) | (1 << bit)));
				pr_info
				    ("%s()->Line:%d, --REGULATOR_EVENT_VOLTAGE_CHANGE--> %s event %ld, data %ld\n",
				     __func__, __LINE__, regu_name, event,
				     best_data);
			}
		}
	} else if (event & REGULATOR_EVENT_DISABLE) {
		pr_info
		    ("%s()->Line:%d, --REGULATOR_EVENT_DISABLE--> %s event %ld\n",
		     __func__, __LINE__, regu_name, event);
	}

	return NOTIFY_OK;
}

static int pin_regulator_notifier_register(struct pin_reg_desc *table)
{
	int i, ret;
	struct regulator *regu;

	for (i = 0; i < PD_CNT; i++) {
		if (IS_ERR_OR_NULL(table[i].platform_data->power_domain)) {
			pr_err("invalid platform data: 0x%p\n",
			       table[i].platform_data->power_domain);
			return -EINVAL;
		}
		regu =
		    regulator_get(NULL, table[i].platform_data->power_domain);
		pr_info("%s, %d, regu_name:%s\n", __FUNCTION__, __LINE__,
			table[i].platform_data->power_domain);
		if (IS_ERR_OR_NULL(regu)) {
			pr_err("no regu %s !\n",
			       table[i].platform_data->power_domain);
			return -ENXIO;;
		}
		table[i].supplies.consumer = regu;
		table[i].supplies.supply = table[i].platform_data->power_domain;
		table[i].nb.notifier_call = sc271x_regulator_event;
		ret = regulator_register_notifier(regu, &(table[i].nb));
		if (ret) {
			pr_err("alert: regu %s reg notifier failed\n",
			       table[i].platform_data->power_domain);
		}
	}
	return 0;
}

static int sprd_pin_switch_probe(struct platform_device *pdev)
{
	int i, ret;
	struct sprd_pin_switch_platform_data *p_pin_switch_data;

#ifdef CONFIG_OF
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;

	p_pin_switch_data =
	    devm_kzalloc(&pdev->dev, sizeof(*p_pin_switch_data) * PD_CNT,
			 GFP_KERNEL);
	if (!p_pin_switch_data) {
		pr_err("pin_switch alloc err\n");
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "No reg of property specified\n");
		return -ENODEV;
	}

	SPRD_PIN_BASE = res->start;
	pr_info("-----SPRD_PIN_BASE:0x%x-----\n", SPRD_PIN_BASE);

	for (i = 0; i < PD_CNT; i++) {
		/*1. get pin power domain vddname */
		ret =
		    of_property_read_string_index(np, "pwr_domain", i,
						  &p_pin_switch_data[i].
						  power_domain);
		if (ret) {
			pr_err("fail to get pwr_domain\n");
			return ret;
		}
		pr_info("pwr_domain[%d]:%s\n", i,
			p_pin_switch_data[i].power_domain);
		/*2. get pin power domain reg ctrl description info */
		ret =
		    of_property_read_u32_index(np, "ctrl_desc", i * 3,
					       &p_pin_switch_data[i].reg);
		if (ret) {
			pr_err("fail to get ctrl_desc reg\n");
			return ret;
		}
		pr_info("reg[%d]:0x%x\n", i, p_pin_switch_data[i].reg);

		ret =
		    of_property_read_u32_index(np, "ctrl_desc", i * 3 + 1,
					       &p_pin_switch_data[i].
					       bit_offset);
		if (ret) {
			pr_err("fail to get ctrl_desc bit_offset\n");
			return ret;
		}
		pr_info("bit_offset[%d]:0x%x\n", i,
			p_pin_switch_data[i].bit_offset);
		ret =
		    of_property_read_u32_index(np, "ctrl_desc", i * 3 + 2,
					       &p_pin_switch_data[i].bit_width);
		if (ret) {
			pr_err("fail to get ctrl_desc bit_width\n");
			return ret;
		}
		pr_info("bit_width[%d]:0x%x\n", i,
			p_pin_switch_data[i].bit_width);
	}
#else
	p_pin_switch_data = dev_get_platdata(&pdev->dev);
	if (IS_ERR_OR_NULL(p_pin_switch_data)) {
		pr_err("invalid platform data: 0x%p\n", p_pin_switch_data);
		return -EINVAL;
	}
#endif
	for (i = 0; i < PD_CNT; i++) {
		pin_reg_desc_array[i].platform_data = &p_pin_switch_data[i];	// + i*sizeof(struct sprd_pin_switch_platform_data);
	}
	ret = pin_regulator_notifier_register(pin_reg_desc_array);
	if (ret) {
		pr_err("register notifier err\n");
	}
	return ret;
}

static struct of_device_id sprd_pinctrl_match_table[] = {
	{.compatible = "sprd,pinctrl",},
	{},
};

static struct platform_driver pin_switch_driver = {
	.probe = sprd_pin_switch_probe,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "pin_switch",
		   .of_match_table = of_match_ptr(sprd_pinctrl_match_table),
		   },
};
#endif

int __init pin_switch_proc_init(void)
{
	int i;
	int ret = 0;
	pin_switch_proc_base = proc_mkdir("pin_switch", NULL);
	if (!pin_switch_proc_base)
		return -ENOMEM;
	for (i = 0; i < ARRAY_SIZE(sci_pin_switch_array); ++i) {
		pin_switch_proc_add(&sci_pin_switch_array[i]);
	}
	for (i = 0; i < ARRAY_SIZE(sci_pin_switch_dir_array); ++i) {
		pin_switch_proc_add_dir(&sci_pin_switch_dir_array[i]);
	}
#if defined(CONFIG_PIN_POWER_DOMAIN_SWITCH)
	ret = platform_driver_register(&pin_switch_driver);
#endif
	return ret;
}

subsys_initcall_sync(pin_switch_proc_init);
#else
#error "CONFIG_PROC_FS needed by mach-sc/pin_switch"
#endif /* CONFIG_PROC_FS */
