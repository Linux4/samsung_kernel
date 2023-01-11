#include "88pm8xx-debugfs.h"

static int reg_pm800 = 0xffff;
static int pm_page_index;

ssize_t pm800_dump_read(struct file *file, char __user *buf,
			size_t count, loff_t *ppos)
{
	unsigned int reg_val = 0;
	struct pm80x_chip *chip = file->private_data;
	int i;

	if (reg_pm800 == 0xffff) {
		pr_info("pm800: base page:\n");
		for (i = 0; i < PM80X_BASE_REG_NUM; i++) {
			regmap_read(chip->regmap, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
		pr_info("pm80x: power page:\n");
		for (i = 0; i < PM80X_POWER_REG_NUM; i++) {
			regmap_read(chip->subchip->regmap_power, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
		pr_info("pm80x: gpadc page:\n");
		for (i = 0; i < PM80X_GPADC_REG_NUM; i++) {
			regmap_read(chip->subchip->regmap_gpadc, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
	} else {
		switch (pm_page_index) {
		case 0:
			regmap_read(chip->regmap, reg_pm800, &reg_val);
			break;
		case 1:
			regmap_read(chip->subchip->regmap_power, reg_pm800,
				    &reg_val);
			break;
		case 2:
			regmap_read(chip->subchip->regmap_gpadc, reg_pm800,
				    &reg_val);
			break;
#if 0
		case 7:
			regmap_read(chip->subchip->regmap_test, reg_pm800,
				    &reg_val);
			break;
#endif
		default:
			pr_err("pm_page_index error!\n");
			return 0;
		}

		pr_info("reg_pm800=0x%x, pm_page_index=0x%x, val=0x%x\n",
			reg_pm800, pm_page_index, reg_val);
	}
	return 0;
}

ssize_t pm800_dump_write(struct file *file, const char __user *buff,
			 size_t len, loff_t *ppos)
{
	u8 reg_val;
	struct pm80x_chip *chip = file->private_data;

	char messages[20], index[20];
	memset(messages, '\0', 20);
	memset(index, '\0', 20);

	if (copy_from_user(messages, buff, len))
		return -EFAULT;

	if ('-' == messages[0]) {
		if ((strlen(messages) != 10) &&
		    (strlen(messages) != 9)) {
			pr_err("Right format: -0x[page_addr] 0x[reg_addr]\n");
			return len;
		}
		/* set the register index */
		memcpy(index, messages + 1, 3);

		if (kstrtoint(index, 16, &pm_page_index) < 0)
			return -EINVAL;

		pr_info("pm_page_index = 0x%x\n", pm_page_index);

		memcpy(index, messages + 5, 4);
		if (kstrtoint(index, 16, &reg_pm800) < 0)
			return -EINVAL;
		pr_info("reg_pm800 = 0x%x\n", reg_pm800);
	} else if ('+' == messages[0]) {
		/* enable to get all the reg value */
		reg_pm800 = 0xffff;
		pr_info("read all reg enabled!\n");
	} else {
		if ((reg_pm800 == 0xffff) ||
		    ('0' != messages[0])) {
			pr_err("Right format: -0x[page_addr] 0x[reg_addr]\n");
			return len;
		}
		/* set the register value */
		if (kstrtou8(messages, 16, &reg_val) < 0)
			return -EINVAL;

		switch (pm_page_index) {
		case 0:
			regmap_write(chip->regmap, reg_pm800, reg_val & 0xff);
			break;
		case 1:
			regmap_write(chip->subchip->regmap_power,
				     reg_pm800, reg_val & 0xff);
			break;
		case 2:
			regmap_write(chip->subchip->regmap_gpadc,
				     reg_pm800, reg_val & 0xff);
			break;
#if 0
		case 7:
			regmap_write(chip->subchip->regmap_test,
				     reg_pm800, reg_val & 0xff);
			break;
#endif
		default:
			pr_err("pm_page_index error!\n");
			break;

		}
	}

	return len;
}

static struct file_operations pm800_dump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm800_dump_read,
	.write		= pm800_dump_write,
};

int pm800_dump_debugfs_init(struct pm80x_chip *chip)
{
	struct dentry *pm800_dump_reg;

	pm800_dump_reg = debugfs_create_file("pm800_reg", S_IRUGO | S_IFREG,
					     NULL, (void *)chip, &pm800_dump_ops);

	if (!pm800_dump_reg) {
		pr_err("faile to create pm800 debugfs!\n");
		return -ENOENT;
	} else if (pm800_dump_reg == ERR_PTR(-ENODEV)) {
		pr_err("pm800_dump_reg error!\n");
		return -ENOENT;
	}
	return 0;
}

void pm800_dump_debugfs_remove(struct pm80x_chip *chip)
{
	debugfs_remove_recursive(chip->debugfs);
}
