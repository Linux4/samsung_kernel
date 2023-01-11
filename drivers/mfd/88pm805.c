/*
 * Base driver for Marvell 88PM805
 *
 * Copyright (C) 2014 Marvell International Ltd.
 * Haojian Zhuang <haojian.zhuang@marvell.com>
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
 * Qiao Zhou <zhouqiao@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/mfd/core.h>
#include <linux/mfd/88pm80x.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/debugfs.h>

#define PM80X_AUDIO_REG_NUM		0x9c
#define	PM805_PROC_FILE		"driver/pm805_reg"
static int reg_pm805 = 0xffff;

static const struct i2c_device_id pm80x_id_table[] = {
	{"88PM805", 0},
	{} /* NULL terminated */
};
MODULE_DEVICE_TABLE(i2c, pm80x_id_table);

static const struct of_device_id pm80x_dt_ids[] = {
	{ .compatible = "marvell,88pm805", },
	{},
};
MODULE_DEVICE_TABLE(of, pm80x_dt_ids);

/* Interrupt Number in 88PM805 */
enum {
	PM805_IRQ_HP1_SHRT,	/* 0 */
	PM805_IRQ_HP2_SHRT,
	PM805_IRQ_MIC_CONFLICT,
	PM805_IRQ_CLIP_FAULT,
	PM805_IRQ_SRC_DPLL_LOCK,
	PM805_IRQ_LDO_OFF,	/* 5 */

	PM805_IRQ_MIC_DET,	/* 6 */
	PM805_IRQ_SHRT_BTN_DET,
	PM805_IRQ_VOLM_BTN_DET,
	PM805_IRQ_VOLP_BTN_DET,
	PM805_IRQ_RAW_PLL_FAULT,
	PM805_IRQ_FINE_PLL_FAULT, /* 11 */

	PM805_MAX_IRQ,
};

static struct resource codec_resources[] = {
	{
	 /* Headset microphone insertion or removal */
	 .name = "micin",
	 .start = PM805_IRQ_MIC_DET,
	 .end = PM805_IRQ_MIC_DET,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 /* Audio short HP1 */
	 .name = "audio-short1",
	 .start = PM805_IRQ_HP1_SHRT,
	 .end = PM805_IRQ_HP1_SHRT,
	 .flags = IORESOURCE_IRQ,
	 },
	{
	 /* Audio short HP2 */
	 .name = "audio-short2",
	 .start = PM805_IRQ_HP2_SHRT,
	 .end = PM805_IRQ_HP2_SHRT,
	 .flags = IORESOURCE_IRQ,
	 },
};

static const struct mfd_cell codec_devs[] = {
	{
	 .name = "88pm80x-codec",
	 .num_resources = ARRAY_SIZE(codec_resources),
	 .resources = &codec_resources[0],
	 .id = -1,
	 },
};

static struct regmap_irq pm805_irqs[] = {
	/* INT0 */
	[PM805_IRQ_HP1_SHRT] = {
		.mask = PM805_INT1_HP1_SHRT,
	},
	[PM805_IRQ_HP2_SHRT] = {
		.mask = PM805_INT1_HP2_SHRT,
	},
	[PM805_IRQ_MIC_CONFLICT] = {
		.mask = PM805_INT1_MIC_CONFLICT,
	},
	[PM805_IRQ_CLIP_FAULT] = {
		.mask = PM805_INT1_CLIP_FAULT,
	},
	[PM805_IRQ_SRC_DPLL_LOCK] = {
		.mask = PM805_INT1_SRC_DPLL_LOCK,
	},
	[PM805_IRQ_LDO_OFF] = {
		.mask = PM805_INT1_LDO_OFF,
	},

	/* INT1 */
	[PM805_IRQ_MIC_DET] = {
		.reg_offset = 1,
		.mask = PM805_INT2_MIC_DET,
	},
	[PM805_IRQ_SHRT_BTN_DET] = {
		.reg_offset = 1,
		.mask = PM805_INT2_SHRT_BTN_DET,
	},
	[PM805_IRQ_VOLM_BTN_DET] = {
		.reg_offset = 1,
		.mask = PM805_INT2_VOLM_BTN_DET,
	},
	[PM805_IRQ_VOLP_BTN_DET] = {
		.reg_offset = 1,
		.mask = PM805_INT2_VOLP_BTN_DET,
	},
	[PM805_IRQ_RAW_PLL_FAULT] = {
		.reg_offset = 1,
		.mask = PM805_INT2_RAW_PLL_FAULT,
	},
	[PM805_IRQ_FINE_PLL_FAULT] = {
		.reg_offset = 1,
		.mask = PM805_INT2_FINE_PLL_FAULT,
	},
};

static ssize_t pm805_dump_read(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	unsigned int reg_val = 0;
	struct pm80x_chip *chip = file->private_data;
	int i;
	int len = 0;
	char str[100];


	if (reg_pm805 == 0xffff) {
		len = snprintf(str, sizeof(str) - 1, "%s\n",
			       "pm805: register dump:");
		for (i = 0; i < PM80X_AUDIO_REG_NUM; i++) {
			regmap_read(chip->regmap, i, &reg_val);
			pr_info("[0x%02x]=0x%02x\n", i, reg_val);
		}
	} else {
		regmap_read(chip->regmap, reg_pm805, &reg_val);
		len = snprintf(str, sizeof(str), "reg_pm805=0x%x, val=0x%x\n",
			      reg_pm805, reg_val);
	}
	return simple_read_from_buffer(user_buf, count, ppos, str, len);

}

static ssize_t pm805_dump_write(struct file *file,
				const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	u8 reg_val;
	struct pm80x_chip *chip = file->private_data;
	int i = 0;
	int ret;

	char messages[20];
	memset(messages, '\0', 20);

	if (copy_from_user(messages, user_buf, count))
		return -EFAULT;

	if ('+' == messages[0]) {
		/* enable to get all the reg value */
		reg_pm805 = 0xffff;
		pr_info("read all reg enabled!\n");
	} else {
		if (messages[1] != 'x') {
			pr_err("Right format: 0x[addr]\n");
			return -EINVAL;
		}

		if (strlen(messages) > 5) {
			while (messages[i] != ' ')
				i++;
			messages[i] = '\0';
			if (kstrtouint(messages, 16, &reg_pm805) < 0)
				return -EINVAL;
			i++;
			if (kstrtou8(messages + i, 16, &reg_val) < 0)
				return -EINVAL;
			ret = regmap_write(chip->regmap, reg_pm805,
					   reg_val & 0xff);
			if (ret < 0) {
				pr_err("write reg error!\n");
				return -EINVAL;
			}
		} else {
			if (kstrtouint(messages, 16, &reg_pm805) < 0)
				return -EINVAL;
		}
	}

	return count;
}

static const struct file_operations pm805_dump_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm805_dump_read,
	.write		= pm805_dump_write,
};

static inline int pm805_dump_debugfs_init(struct pm80x_chip *chip)
{
	struct dentry *pm805_dump_reg;

	pm805_dump_reg = debugfs_create_file("pm805_reg", S_IRUGO | S_IFREG,
			    NULL, (void *)chip, &pm805_dump_ops);

	if (pm805_dump_reg == NULL) {
		pr_err("create pm805 debugfs error!\n");
		return -ENOENT;
	} else if (pm805_dump_reg == ERR_PTR(-ENODEV)) {
		pr_err("CONFIG_DEBUG_FS is not enabled!\n");
		return -ENOENT;
	}
	return 0;
}

static void pm805_dump_debugfs_remove(struct pm80x_chip *chip)
{
	debugfs_remove_recursive(chip->debugfs);
}

static int device_irq_init_805(struct pm80x_chip *chip)
{
	struct regmap *map = chip->regmap;
	unsigned long flags = IRQF_ONESHOT;
	int data, mask, ret = -EINVAL;

	if (!map || !chip->irq) {
		dev_err(chip->dev, "incorrect parameters\n");
		return -EINVAL;
	}

	/*
	 * irq_mode defines the way of clearing interrupt. it's read-clear by
	 * default.
	 */
	mask =
	    PM805_STATUS0_INT_CLEAR | PM805_STATUS0_INV_INT |
	    PM800_STATUS0_INT_MASK;

	data = (chip->irq_mode) ?  PM805_STATUS0_INT_WC : PM805_STATUS0_INT_RC;
	ret = regmap_update_bits(map, PM805_INT_STATUS0, mask, data);
	/*
	 * PM805_INT_STATUS is under 32K clock domain, so need to
	 * add proper delay before the next I2C register access.
	 */
	usleep_range(1000, 1500);

	if (ret < 0)
		goto out;

	ret =
	    regmap_add_irq_chip(chip->regmap, chip->irq, flags, -1,
				chip->regmap_irq_chip, &chip->irq_data);

out:
	return ret;
}

static void device_irq_exit_805(struct pm80x_chip *chip)
{
	regmap_del_irq_chip(chip->irq, chip->irq_data);
}

static struct regmap_irq_chip pm805_irq_chip = {
	.name = "88pm805",
	.irqs = pm805_irqs,
	.num_irqs = ARRAY_SIZE(pm805_irqs),

	.num_regs = 2,
	.status_base = PM805_INT_STATUS1,
	.mask_base = PM805_INT_MASK1,
	.ack_base = PM805_INT_STATUS1,
	.init_ack_masked = true,
	.mask_invert = 1,
};

static int device_805_init(struct pm80x_chip *chip)
{
	int ret = 0;
	struct regmap *map = chip->regmap;

	if (!map) {
		dev_err(chip->dev, "regmap is invalid\n");
		return -EINVAL;
	}

	chip->regmap_irq_chip = &pm805_irq_chip;

	ret = device_irq_init_805(chip);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to init pm805 irq!\n");
		goto out_irq_init;
	}

	ret = mfd_add_devices(chip->dev, 0, &codec_devs[0],
			      ARRAY_SIZE(codec_devs), &codec_resources[0],
			      regmap_irq_chip_get_base(chip->irq_data),
			      NULL);
	if (ret < 0) {
		dev_err(chip->dev, "Failed to add codec subdev\n");
		goto out_codec;
	} else
		dev_info(chip->dev, "[%s]:Added mfd codec_devs\n", __func__);

	return 0;

out_codec:
	device_irq_exit_805(chip);
out_irq_init:
	return ret;
}

static int pm805_dt_init(struct device_node *np,
			 struct device *dev,
			 struct pm80x_platform_data *pdata)
{
	pdata->irq_mode =
		!of_property_read_bool(np, "marvell,88pm805-irq-write-clear");

	return 0;
}

static void pm805_register_reset(struct pm80x_chip *chip)
{
	int data;
	if (!chip->regmap) {
		dev_err(chip->dev, "Failed to reset pm805 register\n");
		return;
	}

	/* power up */
	regmap_read(chip->regmap, 0x01, &data);
	data |= 0x3;
	regmap_write(chip->regmap, 0x01, data);
	usleep_range(1000, 1500);
	/* reset 0x30 to 0x0 */
	regmap_write(chip->regmap, 0x30, 0x00);
	/* power off */
	data &= ~0x3;
	regmap_write(chip->regmap, 0x01, data);
	usleep_range(1000, 1500);
	return;
}
#ifdef CONFIG_MFD_88PM800
static int pm805_reconfig(struct pm80x_chip *chip)
{
	static  char buf[65];
	int val;
	/* 1 */
	regmap_write(chip->regmap, 0xfa, 0x0);
	regmap_write(chip->regmap, 0xfb, 0x0);
	/* 2 */
	regmap_read(chip->regmap, 0xd0, &val);
	buf[0] = val;
	regmap_write(chip->regmap, 0xd0, buf[0] | 0x80);
	regmap_write(chip->regmap, 0x0d, 0x80);
	regmap_write(chip->regmap, 0x1d, 0x01);
	regmap_write(chip->regmap, 0x21, 0x20);
	regmap_write(chip->regmap, 0xe2, 0x2a);

	/* 3 */
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	regmap_write(chip->regmap, 0xbd, 0x80);
	usleep_range(10000, 15000);
	regmap_write(chip->regmap, 0x01, 0x03);
	regmap_write(chip->regmap, 0x80, 0xad);
	usleep_range(20000, 25000);

	/* 4 */
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	regmap_write(chip->regmap, 0xc1, 0xe0);
	msleep_interruptible(200);
	regmap_write(chip->regmap, 0x29, 0x08);
	regmap_write(chip->regmap, 0x26, 0x0);
	usleep_range(20000, 25000);
	regmap_write(chip->regmap, 0x01, 0x02);
	regmap_write(chip->regmap, 0x80, 0x00);
	regmap_write(chip->regmap, 0x01, 0x03);
	usleep_range(20000, 25000);

	/* 5 */
	regmap_write(chip->regmap, 0x07, 0x00);
	regmap_write(chip->regmap, 0x08, 0x00);
	regmap_write(chip->regmap, 0x0a, 0x04);
	regmap_write(chip->regmap, 0x80, 0x00);
	regmap_write(chip->regmap, 0x82, 0x10);
	regmap_write(chip->regmap, 0x95, 0x44);
	regmap_write(chip->regmap, 0x96, 0x71);
	regmap_write(chip->regmap, 0x97, 0x12);
	regmap_write(chip->regmap, 0x99, 0xf0);
	regmap_write(chip->regmap, 0xbd, 0x00);
	regmap_write(chip->regmap, 0xbf, 0x00);
	regmap_write(chip->regmap, 0xcc, 0x00);
	regmap_write(chip->regmap, 0xcd, 0x00);
	regmap_write(chip->regmap, 0xce, 0x04);
	regmap_write(chip->regmap, 0xcf, 0x00);
	regmap_write(chip->regmap, 0xd0, 0x00);
	regmap_write(chip->regmap, 0xd1, 0x00);
	regmap_write(chip->regmap, 0xdc, 0x00);
	regmap_write(chip->regmap, 0xdd, 0x00);
	regmap_write(chip->regmap, 0xde, 0x00);
	regmap_write(chip->regmap, 0xe2, 0x00);
	regmap_write(chip->regmap, 0xe3, 0x58);

	/* 6 */
	regmap_write(chip->regmap, 0x02, 0x00);
	regmap_write(chip->regmap, 0x03, 0x00);
	regmap_write(chip->regmap, 0x04, 0x00);
	regmap_write(chip->regmap, 0x05, 0x00);
	regmap_write(chip->regmap, 0x06, 0x00);
	regmap_write(chip->regmap, 0x10, 0x14);
	regmap_write(chip->regmap, 0x11, 0x00);
	regmap_write(chip->regmap, 0x12, 0x00);
	regmap_write(chip->regmap, 0x13, 0x00);
	regmap_write(chip->regmap, 0x14, 0x00);
	regmap_write(chip->regmap, 0x15, 0x00);
	regmap_write(chip->regmap, 0x16, 0x00);
	regmap_write(chip->regmap, 0x20, 0x12);
	regmap_write(chip->regmap, 0x21, 0x38);
	regmap_write(chip->regmap, 0x22, 0x00);
	regmap_write(chip->regmap, 0x23, 0x1b);
	regmap_write(chip->regmap, 0x24, 0x00);
	regmap_write(chip->regmap, 0x25, 0x00);
	regmap_write(chip->regmap, 0x26, 0x00);
	regmap_write(chip->regmap, 0x27, 0x00);
	regmap_write(chip->regmap, 0x29, 0x08);
	regmap_write(chip->regmap, 0x2a, 0x00);
	regmap_write(chip->regmap, 0x30, 0x1f);
	regmap_write(chip->regmap, 0x31, 0x4c);
	regmap_write(chip->regmap, 0x32, 0x04);
	regmap_write(chip->regmap, 0x33, 0x00);
	regmap_write(chip->regmap, 0x34, 0x00);
	regmap_write(chip->regmap, 0x35, 0x14);
	regmap_write(chip->regmap, 0x36, 0x23);
	regmap_write(chip->regmap, 0x37, 0x04);
	regmap_write(chip->regmap, 0x38, 0x00);
	regmap_write(chip->regmap, 0x39, 0x14);
	regmap_write(chip->regmap, 0x3b, 0x00);
	regmap_write(chip->regmap, 0x3c, 0x23);
	regmap_write(chip->regmap, 0x3d, 0x01);
	regmap_write(chip->regmap, 0x3e, 0x00);
	regmap_write(chip->regmap, 0x3f, 0x00);
	regmap_write(chip->regmap, 0x40, 0x00);
	regmap_write(chip->regmap, 0x41, 0x00);
	regmap_write(chip->regmap, 0x42, 0x00);
	regmap_write(chip->regmap, 0x50, 0x00);
	regmap_write(chip->regmap, 0x51, 0x00);
	regmap_write(chip->regmap, 0x52, 0x01);
	regmap_write(chip->regmap, 0x53, 0x01);
	regmap_write(chip->regmap, 0x54, 0x55);
	regmap_write(chip->regmap, 0x55, 0x20);
	regmap_write(chip->regmap, 0x56, 0x00);
	regmap_write(chip->regmap, 0x57, 0x00);
	regmap_write(chip->regmap, 0x58, 0x00);
	regmap_write(chip->regmap, 0x59, 0x00);
	regmap_write(chip->regmap, 0x5a, 0x00);
	regmap_write(chip->regmap, 0x5b, 0x01);

	buf[0] = 0x5c;
	buf[1] = 0x0;
	buf[2] = 0x0;
	buf[3] = 0x0;
	buf[4] = 0x0;
	buf[5] = 0x0;
	buf[6] = 0x0;
	buf[7] = 0x0;
	buf[8] = 0x0;
	buf[9] = 0x0;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x04;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x08;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x0c;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x10;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x14;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x18;
	i2c_master_send(chip->client, &buf[0], 10);

	buf[1] = 0x1c;
	i2c_master_send(chip->client, &buf[0], 10);
	regmap_write(chip->regmap, 0x5b, 0x02);

	buf[0] = 0x5c;
	buf[1] = 0x0;
	buf[2] = 0x10;
	buf[3] = 0x00;
	buf[4] = 0x00;
	buf[5] = 0x00;
	buf[6] = 0x00;
	buf[7] = 0x00;
	buf[8] = 0x00;
	buf[9] = 0x00;
	buf[10] = 0x00;
	buf[11] = 0x00;
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x03);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x04);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x05);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x06);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x07);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x08);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x09);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0a);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0b);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0c);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0d);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0e);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x0f);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x10);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x11);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x12);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x13);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x14);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5b, 0x15);
	i2c_master_send(chip->client, &buf[0], 12);

	regmap_write(chip->regmap, 0x5d, 0x01);
	regmap_write(chip->regmap, 0x5d, 0x00);
	regmap_write(chip->regmap, 0x5b, 0x00);
	regmap_write(chip->regmap, 0x84, 0x88);
	regmap_write(chip->regmap, 0x85, 0x16);
	regmap_write(chip->regmap, 0x86, 0x16);
	regmap_write(chip->regmap, 0x87, 0x00);
	regmap_write(chip->regmap, 0x88, 0x40);
	regmap_write(chip->regmap, 0x8a, 0x08);
	regmap_write(chip->regmap, 0x8b, 0x05);
	regmap_write(chip->regmap, 0x8c, 0x00);
	regmap_write(chip->regmap, 0x8d, 0x00);
	regmap_write(chip->regmap, 0x8e, 0x09);
	regmap_write(chip->regmap, 0x8f, 0x00);
	regmap_write(chip->regmap, 0x90, 0x7e);
	regmap_write(chip->regmap, 0x91, 0x03);
	regmap_write(chip->regmap, 0x92, 0x55);
	regmap_write(chip->regmap, 0x93, 0x04);
	regmap_write(chip->regmap, 0x94, 0x02);
	regmap_write(chip->regmap, 0x99, 0xf0);
	regmap_write(chip->regmap, 0x9a, 0x12);

	regmap_write(chip->regmap, 0xa0, 0x00);
	regmap_write(chip->regmap, 0xa1, 0x00);
	regmap_write(chip->regmap, 0xa2, 0x00);
	regmap_write(chip->regmap, 0xa3, 0x00);
	regmap_write(chip->regmap, 0xa4, 0x00);
	regmap_write(chip->regmap, 0xa5, 0x00);
	regmap_write(chip->regmap, 0xa6, 0x00);
	regmap_write(chip->regmap, 0xa7, 0x00);
	regmap_write(chip->regmap, 0xa8, 0x00);
	regmap_write(chip->regmap, 0xaa, 0x00);
	regmap_write(chip->regmap, 0xac, 0x00);
	regmap_write(chip->regmap, 0xae, 0x00);

	regmap_write(chip->regmap, 0xb0, 0x00);
	regmap_write(chip->regmap, 0xb2, 0x00);
	regmap_write(chip->regmap, 0xb4, 0x00);
	regmap_write(chip->regmap, 0xb6, 0x00);
	regmap_write(chip->regmap, 0xb8, 0x00);
	regmap_write(chip->regmap, 0xba, 0x00);
	regmap_write(chip->regmap, 0xbc, 0x00);

	regmap_write(chip->regmap, 0xc1, 0x00);
	regmap_write(chip->regmap, 0xc2, 0x00);
	regmap_write(chip->regmap, 0xc3, 0x00);
	regmap_write(chip->regmap, 0xc4, 0x00);
	regmap_write(chip->regmap, 0xc9, 0x00);
	regmap_write(chip->regmap, 0xca, 0x00);

	regmap_write(chip->regmap, 0xd2, 0x00);
	regmap_write(chip->regmap, 0xd3, 0x00);
	regmap_write(chip->regmap, 0xd4, 0x00);
	regmap_write(chip->regmap, 0xd5, 0x00);
	regmap_write(chip->regmap, 0xd6, 0x00);
	regmap_write(chip->regmap, 0xd7, 0x00);
	regmap_write(chip->regmap, 0xd8, 0x00);
	regmap_write(chip->regmap, 0xd9, 0x00);
	regmap_write(chip->regmap, 0xda, 0x00);
	regmap_write(chip->regmap, 0xdb, 0x00);
	regmap_write(chip->regmap, 0x01, 0x02);

	usleep_range(10000, 15000);
	regmap_write(chip->regmap, 0xfc, 0x00);
	regmap_write(chip->regmap, 0xe2, 0x2a);
	usleep_range(1000, 1500);

	return 0;
}
#endif

static int pm805_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	int ret = 0;
	struct pm80x_chip *chip;
	struct pm80x_platform_data *pdata = dev_get_platdata(&client->dev);
	struct device_node *node = client->dev.of_node;
	unsigned char version;
	unsigned int val;

	if (IS_ENABLED(CONFIG_OF)) {
		if (!pdata) {
			pdata = devm_kzalloc(&client->dev,
					     sizeof(*pdata), GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
		}
		ret = pm805_dt_init(node, &client->dev, pdata);
		if (ret)
			return ret;
	} else if (!pdata)
		return -EINVAL;

	ret = pm80x_init(client);
	if (ret) {
		dev_err(&client->dev, "pm805_init fail!\n");
		goto out_init;
	}

	chip = i2c_get_clientdata(client);
	if (pdata)
		chip->irq_mode = pdata->irq_mode;

	ret = device_805_init(chip);
	if (ret) {
		dev_err(chip->dev, "Failed to initialize 88pm805 devices\n");
		goto err_805_init;
	}

	if (pdata && pdata->plat_config)
		pdata->plat_config(chip, pdata);
#ifdef CONFIG_MFD_88PM800
	/* reconfig */
	version = regmap_read(chip->regmap, 0x0, &val);
	version = val & 0xff;
	if (!(version == 0x06))
		pm805_reconfig(chip);
#endif

	pm805_register_reset(chip);

	ret = pm805_dump_debugfs_init(chip);
	if (!ret)
		dev_info(chip->dev, "88pm805 debugfs created!\n");

	return 0;
err_805_init:
	pm80x_deinit();
out_init:
	return ret;
}

static int pm805_remove(struct i2c_client *client)
{
	struct pm80x_chip *chip = i2c_get_clientdata(client);

	mfd_remove_devices(chip->dev);
	device_irq_exit_805(chip);

	if (chip->debugfs)
		pm805_dump_debugfs_remove(chip);

	pm80x_deinit();

	return 0;
}

static struct i2c_driver pm805_driver = {
	.driver = {
		.name = "88PM805",
		.owner = THIS_MODULE,
		.pm = &pm80x_pm_ops,
		.of_match_table	= of_match_ptr(pm80x_dt_ids),
		},
	.probe = pm805_probe,
	.remove = pm805_remove,
	.id_table = pm80x_id_table,
};

static int __init pm805_i2c_init(void)
{
	return i2c_add_driver(&pm805_driver);
}
subsys_initcall(pm805_i2c_init);

static void __exit pm805_i2c_exit(void)
{
	i2c_del_driver(&pm805_driver);
}
module_exit(pm805_i2c_exit);

MODULE_DESCRIPTION("PMIC Driver for Marvell 88PM805");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_LICENSE("GPL");
