#include <linux/io.h>
#include <soc/sprd/gpio.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/pinmap.h>
#include <linux/init.h>
#include <linux/module.h> /* Specifically, a modulei */
#include <linux/kernel.h> /* We're doing kernel work */
#include <linux/proc_fs.h> /* Necessary because we use the proc fs */
#include <linux/uaccess.h> /* for copy_from_user */
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/hwconfig_check.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/arch_misc.h>
#include <linux/regulator/consumer.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#define PROC_MAX_SIZE	64



struct hwconfig_check_t {
const char *name;
const int  size;
char *buffer;
bool ready;
};

enum reg_type {
normal_type = 0,
ana_type,
};



enum reg_type conti_reg_type;
enum reg_type sep_reg_type;

unsigned long conti_base_addr;
unsigned long sep_base_addr;
unsigned long ana_size;

enum idx {
idx_conti_reg = 0,
idx_sleep_conti_reg,
idx_resume_conti_reg,
idx_sep_reg,
idx_sleep_sep_reg,
idx_resume_sep_reg,
idx_regu_volt,
idx_sleep_regu_volt,
idx_resume_regu_volt,
idx_regu_volt_setting,
idx_conti_reg_setting,
idx_sep_reg_setting,
};



#define PROC_NAME        "sprd_hwconfig_check"
#define MAX_REGU_ARRAY_NUM  60
#define MAX_SEP_REG_NUM  128
static unsigned long reg_base_addr;
static unsigned long reg_num;


static bool hwconfig_sleep_flag;
static bool hwconfig_resume_flag;
static bool hwconfig_sep_sleep_flag;
static bool hwconfig_sep_resume_flag;
static bool hwconfig_regu_volt_sleep_flag;
static bool hwconfig_regu_volt_resume_flag;

static int regu_name_index;
static int sep_reg_index;


static char regu_name_array[MAX_REGU_ARRAY_NUM][64];
static int sep_reg_array[MAX_SEP_REG_NUM];
static struct hwconfig_check_t hwconfig_check_array[] = {
	[0] = {
		.name = "conti_config",
		.size = 4096,
	},
	[1] = {
		.name = "conti_sleep_config",
		.size = 4096,
	},
	[2] = {
		.name = "conti_resume_config",
		.size = 4096,
	},
	[3] = {
		.name = "sep_config",
		.size = 2048,
	},
	[4] = {
		.name = "sep_sleep_config",
		.size = 2048,
	},
	[5] = {
		.name = "sep_resume_config",
		.size = 2048,
	},
	[6] = {
		.name = "regu_volt",
		.size = 2048,
	},
	[7] = {
		.name = "sleep_regu_volt",
		.size = 2048,
	},
	[8] = {
		.name = "resume_regu_volt",
		.size = 2048,
	},

	[9] = {
		.name = "regu_volt_setting",
		.size = 512,
	},
	[10] = {
		.name = "conti_reg_setting",
		.size = 128,
	},
	[11] = {
		.name = "sep_reg_setting",
		.size = 1024,
	},
};



static  struct proc_dir_entry *hwconfig_entry;
/* The buffer used to store character for this module */
static int hwconfig_open(struct inode *inode, struct file *file);
static ssize_t hwconfig_read(struct file *file, char __user *buffer,
				size_t len, loff_t *offset);
static ssize_t hwconfig_write(struct file *file, const char __user *buffer,
				size_t len, loff_t *offset);

static const struct file_operations hwconfig_check_fops = {
	.owner		= THIS_MODULE,
	.read		= hwconfig_read,
	.open		= hwconfig_open,
	.write		= hwconfig_write,
	.release	= NULL,
};
/* This function is called when the module is loaded */


static int hwconfig_open(struct inode *inode, struct file *file)
{
	int i = 0;

	char *name = PDE_DATA(inode);
	for (i = 0; i < ARRAY_SIZE(hwconfig_check_array); i++) {
		if (!strcmp(name, hwconfig_check_array[i].name)) {
			file->private_data = (void *)i;
			return 0;
		}
	}
	return -1;
}

static ssize_t hwconfig_read(struct file *file
		, char __user *buffer, size_t len, loff_t *offset)
{

	int idx = (int)file->private_data;
	/*printk("chip_id 0x%x, ana_chip_id 0x%x",sci_get_chip_id(),
		sci_get_ana_chip_id());*/
	if (idx >= ARRAY_SIZE(hwconfig_check_array))
		pr_debug("[hwconfig] hwconfig_read idx error %d\n", idx);

	if (!hwconfig_check_array[idx].ready) {
		/*pr_debug("[hwconfig] hwconfig_read
			idx %ddata not ready\n", idx);*/
		return 0;
	}
	return simple_read_from_buffer(buffer, len, offset,
		hwconfig_check_array[idx].buffer,
		hwconfig_check_array[idx].size);
}

unsigned long hwconfig_read_reg(unsigned long addr)
{
	pr_debug("[hwconfig], hwconfig_read_reg 0x%x\n", addr);
	return readl_relaxed((void __iomem *)addr);
}

unsigned long hwconfig_read_anareg(unsigned long addr)
{
	unsigned long real_addr = (u64)SPRD_ADI_BASE+addr;
	return sci_adi_read((void __iomem *)real_addr);
}

static int trigger_conti_reg_dump(int mode)
{
	int i = 0;
	long r = 0;
	unsigned int reg = 0;
	int index = mode;

	if (index >= ARRAY_SIZE(hwconfig_check_array))
		pr_debug("trigger_pinmap_dump index error %d\n", index);

	memset(hwconfig_check_array[index].buffer, 0x0,
		hwconfig_check_array[index].size);

	for (i = 0; i < reg_num; i++) {
		char temp[20] = {0};
		reg = reg_base_addr+(i<<2);

		if (conti_reg_type == normal_type)
			r = hwconfig_read_reg(conti_base_addr+reg);
		else
			r = hwconfig_read_anareg(reg);
		/*printk("[hwconfig] trigger_conti_reg_dump
			conti_base_addr 0x%0x, reg0x%0x
			r 0x%0x\n ", conti_base_addr, reg, r);*/
		sprintf(temp, "0x%08x,", r);

		strcat(hwconfig_check_array[index].buffer, temp);
	}
	hwconfig_check_array[index].ready = true;

	return 0;

}



bool get_hwconfig_sleep_flag(void)
{
	return hwconfig_sleep_flag;
}

bool get_hwconfig_resume_flag(void)
{
	return hwconfig_resume_flag;
}

int hwconfig_check_sleep_conti_reg(void)
{
	/*pr_debug("[hwconfig] hwconfig_check_sleep_
		conti_reg hwconfig_sleep_flag %d\n", hwconfig_sleep_flag);*/
	if (hwconfig_sleep_flag) {
		hwconfig_sleep_flag = false;
		trigger_conti_reg_dump(idx_sleep_conti_reg);
	}
	return 0;
}

int hwconfig_check_resume_conti_reg(void)
{
	if (hwconfig_resume_flag) {
		hwconfig_resume_flag = false;
		trigger_conti_reg_dump(idx_resume_conti_reg);
	}
	return 0;

}


static int trigger_conti_reg_check(char *buffer)
{

	char *ptr;
	unsigned long phy_addr;
	unsigned long size;
	unsigned long base_addr;

	hwconfig_check_array[idx_conti_reg].ready = false;
	hwconfig_check_array[idx_sleep_conti_reg].ready = false;
	hwconfig_check_array[idx_resume_conti_reg].ready = false;

	ptr = strsep(&buffer, ",");
	if (ptr) {
		conti_reg_type = simple_strtoul(ptr, NULL, 10);
		pr_debug("[hwconfig] conti_reg_type %d \n", conti_reg_type);
	}
	ptr = strsep(&buffer, ",");
	if (ptr) {
		phy_addr = simple_strtoull(ptr, NULL, 16);
		pr_debug("[hwconfig]conti phy 0x%x\n", phy_addr);
	}
	ptr = strsep(&buffer, ",");
	if (ptr) {
		size = simple_strtoul(ptr, NULL, 16);
		pr_debug("[hwconfig]conti size 0x%x\n", size);
	}
	if (conti_reg_type == normal_type) {
		conti_base_addr = ioremap_nocache(phy_addr, size);
	} else {
		conti_base_addr = phy_addr;
		ana_size = size;
	}
	pr_debug("[hwconfig]conti addr 0x%x\n", conti_base_addr);

	ptr = strsep(&buffer, ",");
	if (ptr) {
		reg_base_addr = simple_strtoull(ptr, NULL, 16);
		/*pr_debug("[hwconfig] trigger_
			conti_reg_check reg_base_addr 0x%xbuffer %s\n"
			, reg_base_addr, buffer);*/
	}
	ptr = strsep(&buffer, ",");
	if (ptr) {
		reg_num = simple_strtoul(ptr, NULL, 10);
		pr_debug("[hwconfig] conti reg_num %d\n", reg_num);
	}

	trigger_conti_reg_dump(idx_conti_reg);
	hwconfig_sleep_flag = true;
	hwconfig_resume_flag = true;

	return 0;
}




static int trigger_regu_volt_dump(int mode)
{

	struct regulator *vdd_dev = NULL;
	unsigned long volt;
	int i = 0;
	int r = 0;
	int index = mode;

	if (index >= ARRAY_SIZE(hwconfig_check_array))
		pr_debug("trigger_sep_reg_dump index error %d\n", index);
	memset(hwconfig_check_array[index].buffer,
		0x0, hwconfig_check_array[index].size);

	for (i = 0; i < regu_name_index; i++) {
		char temp[20] = {0};

		vdd_dev = regulator_get(NULL, regu_name_array[i]);
		if (IS_ERR(vdd_dev)) {
			/*pr_debug("[hwconfig] trigger_reg_volt_
				dump can't find the regu dev %s\n"
				, regu_name_array[i]);*/
			sprintf(temp, "%d,", 0);
		} else {
			volt = regulator_get_voltage(vdd_dev);
			/*pr_debug("[hwconfig] trigger_regu_du
				mp %s, volt %lu\n", regu_name_array[i], volt);*/
			sprintf(temp, "%d,", volt);
		}
		strcat(hwconfig_check_array[index].buffer, temp);
	}

	hwconfig_check_array[index].ready = true;
}

int hwconfig_check_sleep_regu_volt(void)
{
	if (hwconfig_regu_volt_sleep_flag)
		trigger_regu_volt_dump(idx_sleep_regu_volt);
	return 0;
}

int hwconfig_check_resume_regu_volt(void)
{
	if (hwconfig_regu_volt_resume_flag)
		trigger_regu_volt_dump(idx_resume_regu_volt);
	return 0;
}


static int trigger_regu_volt_check(char *buffer)
{
	char *ptr;
	char *ptr1;
	unsigned long  regu_num;
	regu_name_index = 0;

	hwconfig_check_array[idx_regu_volt].ready = false;
	hwconfig_check_array[idx_sleep_regu_volt].ready = false;
	hwconfig_check_array[idx_resume_regu_volt].ready = false;

	while ((ptr = strsep(&buffer, ",")) != NULL) {
	    if (regu_name_index < MAX_REGU_ARRAY_NUM) {
			/*pr_debug("[hwconfig] trigger_regu_volt_
				check regu_name %s\n", ptr);*/
			strcpy(regu_name_array[regu_name_index], ptr);
			regu_name_index++;
		} else {
			pr_debug("[hwconfig]  trigger_hwconfig_sep_reg_chec!!!\n");
			break;
		}
	}
	trigger_regu_volt_dump(idx_regu_volt);
	hwconfig_regu_volt_sleep_flag = true;
	hwconfig_regu_volt_resume_flag = true;
}



static int trigger_sep_reg_dump(int mode)
{
	int index = mode;
	int i = 0;

	if (index >= ARRAY_SIZE(hwconfig_check_array))
		pr_debug("trigger_sep_reg_dump index error %d\n", index);
	memset(hwconfig_check_array[index].buffer
		, 0x0, hwconfig_check_array[index].size);

	for (i = 0; i < sep_reg_index; i++) {
		char temp[20] = {0};
		long r;
		if (sep_reg_type == normal_type)
			r = hwconfig_read_reg(sep_base_addr+sep_reg_array[i]);
		else
			r = hwconfig_read_anareg(sep_reg_array[i]);

		sprintf(temp, "0x%x,", r);
		strcat(hwconfig_check_array[index].buffer, temp);
	}
	pr_debug("[hwconfig] sep %s\n", hwconfig_check_array[index].buffer);
	hwconfig_check_array[index].ready = true;

	return 0;

}

int hwconfig_check_sleep_sep_reg(void)
{
	if (hwconfig_sep_sleep_flag)
		trigger_sep_reg_dump(idx_sleep_sep_reg);
	return 0;
}

int hwconfig_check_resume_sep_reg(void)
{
	if (hwconfig_sep_resume_flag)
		trigger_sep_reg_dump(idx_resume_sep_reg);
	return 0;
}




static int trigger_sep_reg_check(char *buffer)
{
	char *ptr;
	char *ptr_addr;
	char sep_reg[256] = {0};
	long reg_addr = 0;
	unsigned long phy_addr;
	unsigned long size;
	unsigned long base_addr;

	sep_reg_index = 0;
	hwconfig_check_array[idx_sep_reg].ready = false;
	hwconfig_check_array[idx_sleep_sep_reg].ready = false;
	hwconfig_check_array[idx_resume_sep_reg].ready = false;

	ptr = strsep(&buffer, ",");/*reg type 0:normal;1:ana reg*/
	if (ptr) {
		sep_reg_type = simple_strtoul(ptr, NULL, 10);
		pr_debug("[hwconfig] sep type %d\n", sep_reg_type);
	}
	ptr = strsep(&buffer, ",");
	if (ptr) {
		phy_addr = simple_strtoull(ptr, NULL, 16);
		pr_debug("[hwconfig]sep phy_addr 0x%x\n", phy_addr);
	}
	ptr = strsep(&buffer, ",");
	if (ptr) {
		size = simple_strtoul(ptr, NULL, 16);
		pr_debug("[hwconfig]sep size 0x%x\n", size);

	}
	if (sep_reg_type == normal_type) {
		sep_base_addr = ioremap_nocache(phy_addr, size);
	} else {
		sep_base_addr = phy_addr;
		ana_size = size;
	}
	pr_debug("[hwconfig]sep base 0x%08x\n", sep_base_addr);
	while ((ptr = strsep(&buffer, ",")) != NULL) {
		if (sep_reg_index < MAX_SEP_REG_NUM) {
			reg_addr = simple_strtoul(ptr, NULL, 16);
			sep_reg_array[sep_reg_index] = reg_addr;
			sep_reg_index++;
		} else {
			pr_debug("[hwconfig]sep too many!!!\n");
			break;
		}

	}
	trigger_sep_reg_dump(idx_sep_reg);
	hwconfig_sep_sleep_flag = true;
	hwconfig_sep_resume_flag = true;

	return 0;
}

static ssize_t hwconfig_write(struct file *file,
		const char __user *buffer, size_t len, loff_t *offset)
{
	int idx = (int)file->private_data;
	int ret = 0;

	pr_debug("[hwconfig] hwconfig_write idx %d\n", idx);
	if (idx < idx_regu_volt_setting)
		return 0;

	if (len > hwconfig_check_array[idx].size) {
		pr_debug("[hwconfig] hwconfig_write len %d\n", len);
		return 0;
	}

	memset(hwconfig_check_array[idx].buffer
		, '\0', hwconfig_check_array[idx].size);
	ret = simple_write_to_buffer(hwconfig_check_array[idx].buffer,
			hwconfig_check_array[idx].size, offset, buffer, len);
	pr_debug("[hwconfig] write idx %d, %s\n", idx,hwconfig_check_array[idx].buffer);
	if (idx == idx_conti_reg_setting)
		trigger_conti_reg_check(hwconfig_check_array[idx].buffer);
	else if (idx == idx_sep_reg_setting)
		trigger_sep_reg_check(hwconfig_check_array[idx].buffer);
	else if (idx == idx_regu_volt_setting)
		trigger_regu_volt_check(hwconfig_check_array[idx].buffer);

	return ret;
}

static int chipid_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t chipid_read(struct file *file, char __user *buffer,
				size_t len, loff_t *offset)
{
	char buf[64] = {0};
	sprintf(buf, "0x%x,0x%x,", sci_get_chip_id(),
			sci_get_ana_chip_id());
	pr_debug("[hwconfig], %s\n", buf);
	return simple_read_from_buffer(buffer, len, offset, buf, 64);
}

static ssize_t chipid_write(struct file *file, const char __user *buffer,
			size_t len, loff_t *offset)
{
	return 0;
}

static int battery_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
        switch (mode) {
        case PM_SUSPEND_PREPARE:
		{
			hwconfig_check_sleep_conti_reg();
			hwconfig_check_sleep_sep_reg();
			hwconfig_check_sleep_regu_volt();
		}
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		{
			hwconfig_check_resume_conti_reg();
			hwconfig_check_resume_sep_reg();
			hwconfig_check_resume_regu_volt();
		}
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static const struct file_operations chipid_fops = {
	.owner          = THIS_MODULE,
	.read           = chipid_read,
	.open           = chipid_open,
	.write          = chipid_write,
	.release        = NULL,
};

static struct notifier_block hwconfig_nb;
static int __init sprd_hwconfig_check_init(void)
{
	int i;
	struct proc_dir_entry *entry;

	hwconfig_entry = proc_mkdir(PROC_NAME, NULL);

	if (!hwconfig_entry) {
		/*pr_debug(KERN_ALERT "Error: Could not initialize
			/proc/%s\n", PROC_NAME);*/
		return -ENOMEM;
	}
	for (i = 0; i < ARRAY_SIZE(hwconfig_check_array); i++) {
		entry = proc_create_data(hwconfig_check_array[i].name,
			S_IRUGO|S_IWUGO, hwconfig_entry,
			&hwconfig_check_fops, hwconfig_check_array[i].name);
		hwconfig_check_array[i].buffer = kmalloc(
			hwconfig_check_array[i].size, GFP_KERNEL);

		if (NULL == hwconfig_check_array[i].buffer) {
			pr_debug("[hwconfig] sprd_hwconfig_check_init malloc error\n");
			return -1;
		}
		memset(hwconfig_check_array[i].buffer
			, '\0', hwconfig_check_array[i].size);
		hwconfig_check_array[i].ready = false;
	}
	proc_create_data("chipid", S_IRUGO|S_IWUGO, hwconfig_entry,
			&chipid_fops, "chipid");
	hwconfig_nb.notifier_call = battery_notify;
	register_pm_notifier(&hwconfig_nb);
	hwconfig_sleep_flag = false;
	hwconfig_resume_flag = false;
	hwconfig_sep_sleep_flag = false;
	hwconfig_sep_resume_flag = false;
	hwconfig_regu_volt_sleep_flag = false;
	hwconfig_regu_volt_resume_flag = false;
	regu_name_index = 0;
	sep_reg_index = 0;
	return 0;
}
late_initcall(sprd_hwconfig_check_init);

static void sprd_hwconfig_check_exit(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(hwconfig_check_array); i++) {
		remove_proc_entry(hwconfig_check_array[i].name, hwconfig_entry);
		kfree(hwconfig_check_array[i].buffer);
	}
	remove_proc_entry(PROC_NAME, NULL);
	unregister_pm_notifier(&hwconfig_nb);
	pr_debug(KERN_INFO "/proc/%s removed\n", PROC_NAME);

}

module_exit(sprd_hwconfig_check_exit);
MODULE_DESCRIPTION("SPREADTRUM HARDWARE CONFIGURATION CHECK");
MODULE_LICENSE("GPL");
