/*
 * Copyright (C) 2014 Marvell International Ltd.
 *		Lei Wen <leiwen@marvell.com>
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

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/debugfs.h>

struct sw_jtag_data {
	struct clk *clk;
	void __iomem *base;
};

static struct sw_jtag_data swjtag;
#define jtagw(x, y)	__raw_writel(x, swjtag.base + y)
#define jtagr(x)	__raw_readl(swjtag.base + x)

/* use for SW_JTAG control */
#define JTAGSW_EN		0x0
#define JTAGSW_DATA		0x4
#define JTAGSW_READ		0x8

#define TDI		0x3
#define TRST		0x2
#define TMS		0x6
#define TDITMS		0x7
#define TCK		0xa
#define TDITCK		0xb
#define TMSTCK		0xe
#define TDITMSTCK	0xf

#define  setbit1 \
	do { \
		jtagw(TDI, JTAGSW_DATA); \
		jtagw(TDITCK, JTAGSW_DATA); \
	} while (0)

#define  setbit0 \
	do { \
		jtagw(TRST, JTAGSW_DATA); \
		jtagw(TCK, JTAGSW_DATA); \
	} while (0)

void __weak enable_sw_jtag_power(void)
{
}

void __weak disable_sw_jtag_power(void)
{
}

static void jtag_rw(u32 address, u32 addr_len, unsigned int data[],
		u32 len, unsigned int read_array[])
{
	unsigned long addr_shift, data_shift, read_shift, read_tdo;
	int len_int;
	int i;
	int j;

	read_shift = 0;
	/* TAP goto RUTI */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);

	/* TAP goto SDRS */
	jtagw(TMS, JTAGSW_DATA);
	jtagw(TMSTCK, JTAGSW_DATA);

	/* TAP goto SIRS */
	jtagw(TMS, JTAGSW_DATA);
	jtagw(TMSTCK, JTAGSW_DATA);

	/* TAP goto CAIR */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);

	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);
	/* goto SHIR state, data will be latched in next negedge */

	addr_shift = address;

	for (i = 0; i < addr_len - 1; i++) {
		if ((addr_shift & 0x1) == 0x1)
			setbit1;
		else
			setbit0;
		addr_shift = addr_shift >> 1;
		/* bit0 is the first to shift out */
	}

	/* the last bit will be latched in next state */
	if (addr_shift & 0x1) {
		/* posedge latch bit[7]=1, and TAP goto Exit1_DR */
		jtagw(TDITMS, JTAGSW_DATA);
		jtagw(TDITMSTCK, JTAGSW_DATA);
	} else {
		jtagw(TMS, JTAGSW_DATA);
		jtagw(TMSTCK, JTAGSW_DATA);
	}

	/* TAP goto update IR */
	jtagw(TMS, JTAGSW_DATA);
	jtagw(TMSTCK, JTAGSW_DATA);

	/* IR done, goto DR */

	/* TAP goto RUTI */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);

	/* TAP goto SDRS */
	jtagw(TMS, JTAGSW_DATA);
	jtagw(TMSTCK, JTAGSW_DATA);

	/* TAP goto capture DR */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);

	/* TAP goto shift DR state, data will be latched in next neg edge */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);

	len_int = len;
	data_shift = data[0];

	j = 0;
	while (len_int != 0) {
		read_shift = 0;
		if (len_int <= 32) {
			/* less than 32-bit */
			for (i = 0; i < len_int - 1; i++) {
				if ((data_shift & 0x1) == 0x1)
					setbit1;
				else
					setbit0;
				data_shift = data_shift >> 1;
				/* bit0 is the first to shift out */
				read_tdo = jtagr(JTAGSW_READ);
				read_shift = read_shift | (read_tdo << i);
			}
			len_int = 0;
			read_array[j] = read_shift;
		} else {
			/* 32b-bit write */
			for (i = 0; i < 32; i++) {
				if ((data_shift & 0x1) == 0x1)
					setbit1;
				else
					setbit0;
				data_shift = data_shift >> 1;
				/* bit0 is the first to shift out. */
				read_tdo = jtagr(JTAGSW_READ);
				read_shift = read_shift | (read_tdo << i);
			}
			read_array[j] = read_shift;
			j = j + 1;
			data_shift = data[j];
			len_int = len_int - 32;
		}
	}

	/* the last bit will be latched in next state. */
	if (data_shift & 0x1) {
		/* posedge latch bit[7]=1, and TAP goto Exit1_DR */
		jtagw(TDITMS, JTAGSW_DATA);
		jtagw(TDITMSTCK, JTAGSW_DATA);
	} else {
		jtagw(TMS, JTAGSW_DATA);
		jtagw(TMSTCK, JTAGSW_DATA);
	}
	read_tdo = jtagr(JTAGSW_READ);
	read_shift = read_shift | (read_tdo << i);
	read_array[j] = read_shift;

	/* TAP goto update DR */
	jtagw(TMS, JTAGSW_DATA);
	jtagw(TMSTCK, JTAGSW_DATA);

	/* TAP goto run RUTI */
	jtagw(TRST, JTAGSW_DATA);
	jtagw(TCK, JTAGSW_DATA);
}

static ssize_t chip_watcher_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	unsigned int ir_val, ir_len, dr_len;
	char buf[128];
	unsigned int datax[5], readx[5];
	int buf_size, ret, i;

	buf_size = min(cnt, (sizeof(buf)-1));
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;
	memset(datax, 0, 5 * sizeof(u32));
	ret = sscanf(buf, "%x-%x_%x-%x-%x-%x-%x-%x",
			&ir_len, &ir_val, &dr_len, &datax[0],
			&datax[1], &datax[2], &datax[3], &datax[4]);

	if (ret == 8) {
		/* release SW_JTAG reset */
		/* enable JTAG SW mode, hardware JTAG will be invalid now */
		jtagw(0x1, JTAGSW_EN);

		/* force TAP to reset state */
		for (i = 0; i < 30; i++) {
			jtagw(TMS, JTAGSW_DATA);
			jtagw(TMSTCK, JTAGSW_DATA);
		}
		memset(readx, 0, 5 * sizeof(u32));
		jtag_rw(ir_val, ir_len, datax, dr_len, readx);
		pr_info("JTAG old value is 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			readx[0], readx[1], readx[2], readx[3], readx[4]);
		jtag_rw(ir_val, ir_len, datax, dr_len, readx);
		pr_info("JTAG new value is 0x%x 0x%x 0x%x 0x%x 0x%x\n",
			readx[0], readx[1], readx[2], readx[3], readx[4]);

		jtagw(0x0, JTAGSW_EN);
	} else
		pr_err("You need use pattern like %%x-%%x_%%x-%%x-%%x-%%x-%%x-%%x!!!\n");

	return cnt;
}
static const struct file_operations chip_watcher_fops = {
	.owner		= THIS_MODULE,
	.write		= chip_watcher_write,
};

static ssize_t swjtag_power_write(struct file *filp, const char __user *ubuf,
		size_t cnt, loff_t *ppos)
{
	char buf[128];
	unsigned int on_status;
	int ret, buf_size;

	buf_size = min(cnt, (sizeof(buf)-1));
	if (copy_from_user(buf, ubuf, buf_size))
		return -EFAULT;
	ret = sscanf(buf, "%x", &on_status);

	if (ret == 1) {
		if (on_status) {
			enable_sw_jtag_power();
			clk_prepare_enable(swjtag.clk);
		} else {
			clk_disable_unprepare(swjtag.clk);
			disable_sw_jtag_power();
		}
	} else
		pr_err("Please input 0 or 1\n");

	return cnt;
}
static const struct file_operations swjtag_power_fops = {
	.owner		= THIS_MODULE,
	.write		= swjtag_power_write,
};

static int sw_jtag_probe(struct platform_device *pdev)
{
	struct resource *mem;
	struct dentry *dentry, *root;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	swjtag.base = devm_ioremap_resource(&pdev->dev, mem);
	swjtag.clk = clk_get(NULL, "swjtag");

	root = debugfs_create_dir("chip_watcher", NULL);
	dentry = debugfs_create_file("val", S_IRUGO,
			root, NULL, &chip_watcher_fops);
	if (!dentry) {
		dev_err(&pdev->dev, "Cannot create debugfs!\n");
		return -ENOMEM;
	}

	dentry = debugfs_create_file("power", S_IRUGO,
			root, NULL, &swjtag_power_fops);
	if (!dentry) {
		dev_err(&pdev->dev, "Cannot create debugfs!\n");
		return -ENOMEM;
	}

	return 0;
}

static int sw_jtag_remove(struct platform_device *pdev)
{
	clk_put(swjtag.clk);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id sw_jtag_match[] = {
	{
		.compatible = "marvell,sw-jtag", .data = NULL},
	{},
};
MODULE_DEVICE_TABLE(of, sw_jtag_match);
#endif

static struct platform_driver mv_sw_jtag = {
	.driver = {
		.name   = "sw-jtag",
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(sw_jtag_match),
#endif
	},
	.probe = sw_jtag_probe,
	.remove	= sw_jtag_remove,
};

module_platform_driver(mv_sw_jtag);
