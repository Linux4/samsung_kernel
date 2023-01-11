/*
 * MFP Init configuration driver for MMP related platform.
 *
 * This driver is aims to do the initialization on the MFP configuration
 * which is not handled by the component driver.
 * The driver user is required to add your "mfp init deivce" in your used
 * DTS file, whose complatiable is "mrvl,mmp-mfp", in which you can implement
 * pinctrl setting.
 *
 * Copyright:   (C) 2013 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/slab.h>

#ifdef CONFIG_SEC_GPIO_DVS
#include <linux/secgpio_dvs.h>
#include <linux/platform_data/gpio-mmp.h>
#include <linux/gpio.h>
#include <asm-generic/gpio.h>
#endif

#define MAX_STATE 8
#define MAX_STR_LEN 25

struct pinfunc_data {
	char pin_func_table[MAX_STATE][MAX_STR_LEN];
	char spec_name[MAX_STR_LEN];
	u32 pinfunc_mask;
} *mfp_dump_data;

static int pins_functbl_init(u32 tbl_length, struct device_node *node)
{
	int off = 0;
	struct pinfunc_data *data;
	struct device_node *pin_node;
	char str[MAX_STR_LEN];

	mfp_dump_data = kzalloc(sizeof(struct pinfunc_data) * (tbl_length / 4),
				GFP_ATOMIC);

	if (!mfp_dump_data)
		return -EINVAL;

	data = mfp_dump_data;

	for (off = 0; off < tbl_length; off += 0x4) {
		int tbl_idx = 0, list_idx = 0, bit = -EINVAL;

		snprintf(str, ARRAY_SIZE(str), "0x%x", off);
		pin_node = of_find_compatible_node(node, NULL, str);

		if (pin_node) {
			unsigned long mask;
			const char *spec_name = NULL;

			BUG_ON(of_property_read_u32(pin_node,
				"pin-mask", &(data->pinfunc_mask)));
			mask = data->pinfunc_mask;
			if (mask != 0)
				bit = find_first_bit(&(mask), 32);

			if (of_property_read_string(pin_node,
				"spec-name", &(spec_name))) {
				snprintf(str, ARRAY_SIZE(str),
					"MFP_%03d", (off / 4));
				strcpy(data->spec_name, str);
			} else
				strcpy(data->spec_name, (char *)spec_name);
		} else {
			snprintf(str, ARRAY_SIZE(str), "MFP_%03d", (off / 4));
			strcpy(data->spec_name, str);
		}

		while (1) {
			const char *func_name = NULL;

			BUG_ON(bit >= MAX_STATE);
			if (tbl_idx >= MAX_STATE)
				break;
			if (bit == tbl_idx) {
				unsigned long mask = data->pinfunc_mask;
				of_property_read_string_index(pin_node,
						"pin-func-list",
						list_idx, &func_name);
				if (func_name) {
					strcpy(data->pin_func_table[tbl_idx],
							(char *)func_name);
					bit = find_next_bit(&(mask),
							32, bit + 1);
					if (bit >= 32)
						break;
					list_idx++;
					tbl_idx++;
					continue;
				}
			}
			snprintf(str, ARRAY_SIZE(str),
					"func_%d", tbl_idx);
			strcpy(data->pin_func_table[tbl_idx], str);
			tbl_idx++;
		}
		data++;
	}
	return 0;
}
static int mfp_setting_dump(struct seq_file *m, void *unused)
{
	u32 pa_base = ((u32 *)(m->private))[0];
	u32 length = ((u32 *)(m->private))[1];
	int off = 0;
	void __iomem *va_base = ioremap(pa_base, length);
	struct pinfunc_data *data;

	if (!mfp_dump_data) {
		seq_puts(m, "MFP func table is not inited!!\n");
		iounmap(va_base);
		return 0;
	}

	data = mfp_dump_data;

	seq_printf(m, "%20s%8s %8s %20s %12s %12s %16s %19s\n",
		"Pin Name", "(Offset)", "MFPR", "AltFunc",
		"PullConf", "DRV.STR", "LPM Mode", "EDGE DET");

	for (off = 0; off < length; off += 0x4) {
		u32 val = readl_relaxed(va_base + off);

		seq_printf(m, "%20s(0x%04x) : 0x%04x%22s%12s%11s%22s%18s\n",
			data->spec_name, off, val,
			data->pin_func_table[val & 0x0007],
			((val & 0x7000) == 0x6000) ?
			"Wrong PULL" : (val & 0x8000) ?
			((val & 0x6000) == 0) ? "Pull Float" :
			((val & 0x4000) ? ((val & 0x2000) ?
			"Pull Both" : "Pull High") : "Pull Low") : "No Pull",
			((val & 0x1800) == 0x1800) ?
			"FAST" : ((val & 0x1800) == 0x1000) ?
			"MEDIUM" : ((val & 0x1800) == 0x0800) ?
			"SLOW" : "SLOW0",
			((val & 0x0208) == 0) ?
			"No Sleep control" :
			((val & 0x0208) == 0x0208) ? ((val & 0x0080) ?
			"Sleep Input" : (val & 0x0100) ?
			"Sleep Out High" : "Sleep Out Low") :
			"UNEX Sleep Control",
			((val & 0x0040) == 0x0040) ?
			"No EDGE Detect" : ((val & 0x0020) == 0) ?
			(((val & 0x0010) == 0) ?
			"EDGE Not Set" : "EDGE Fall EN") :
			(val & 0x0010) ? "EDGE Both EN" : "EDGE Rising EN");

		data++;
	}

	iounmap(va_base);

	return 0;
}

static int mfp_dump_open(struct inode *inode, struct file *file)
{
	return single_open(file, mfp_setting_dump, inode->i_private);
}

const struct file_operations mfp_dump_fops = {
	.owner	= THIS_MODULE,
	.open = mfp_dump_open,
	.read = seq_read,
	.llseek = seq_lseek,
};

#ifdef CONFIG_SEC_GPIO_DVS
/* MFPR register bit definitions */
#define MFPR_PULL_SEL		(0x1 << 15)
#define MFPR_PULLUP_EN		(0x1 << 14)
#define MFPR_PULLDOWN_EN	(0x1 << 13)
#define MFPR_SLEEP_SEL		(0x1 << 9)
#define MFPR_SLEEP_OE_N		(0x1 << 7)
#define MFPR_EDGE_CLEAR		(0x1 << 6)
#define MFPR_EDGE_FALL_EN	(0x1 << 5)
#define MFPR_EDGE_RISE_EN	(0x1 << 4)

#define MFPR_SLEEP_DATA(x)	((x) << 8)
#define MFPR_DRIVE(x)		(((x) & 0x7) << 10)
#define MFPR_AF_SEL(x)		(((x) & 0x7) << 0)

#define MFPR_EDGE_NONE		(0)
#define MFPR_EDGE_RISE		(MFPR_EDGE_RISE_EN)
#define MFPR_EDGE_FALL		(MFPR_EDGE_FALL_EN)
#define MFPR_EDGE_BOTH		(MFPR_EDGE_RISE | MFPR_EDGE_FALL)

/*
 * Table that determines the low power modes outputs, with actual settings
 * used in parentheses for don't-care values. Except for the float output,
 * the configured driven and pulled levels match, so if there is a need for
 * non-LPM pulled output, the same configuration could probably be used.
 *
 * Output value  sleep_oe_n  sleep_data  pullup_en  pulldown_en  pull_sel
 *                 (bit 7)    (bit 8)    (bit 14)     (bit 13)   (bit 15)
 *
 * Input            0          X(0)        X(0)        X(0)       0
 * Drive 0          0          0           0           X(1)       0
 * Drive 1          0          1           X(1)        0	  0
 * Pull hi (1)      1          X(1)        1           0	  0
 * Pull lo (0)      1          X(0)        0           1	  0
 * Z (float)        1          X(0)        0           0	  0
 */
#define MFPR_LPM_INPUT		(0x1 << 7)
#define MFPR_LPM_DRIVE_LOW	(MFPR_SLEEP_DATA(0) | MFPR_PULLDOWN_EN)
#define MFPR_LPM_DRIVE_HIGH    	(MFPR_SLEEP_DATA(1) | MFPR_PULLUP_EN)
#define MFPR_LPM_PULL_LOW      	(MFPR_LPM_DRIVE_LOW  | MFPR_SLEEP_OE_N)
#define MFPR_LPM_PULL_HIGH     	(MFPR_LPM_DRIVE_HIGH | MFPR_SLEEP_OE_N)
#define MFPR_LPM_FLOAT         	(MFPR_SLEEP_OE_N)
#define MFPR_LPM_MASK		(0xe080)

/*
 * The pullup and pulldown state of the MFP pin at run mode is by default
 * determined by the selected alternate function. In case that some buggy
 * devices need to override this default behavior,  the definitions below
 * indicates the setting of corresponding MFPR bits
 *
 * Definition       pull_sel  pullup_en  pulldown_en
 * MFPR_PULL_NONE       0         0        0
 * MFPR_PULL_LOW        1         0        1
 * MFPR_PULL_HIGH       1         1        0
 * MFPR_PULL_BOTH       1         1        1
 * MFPR_PULL_FLOAT	1         0        0
 */
#define MFPR_PULL_NONE		(0)
#define MFPR_PULL_LOW		(MFPR_PULL_SEL | MFPR_PULLDOWN_EN)
#define MFPR_PULL_BOTH		(MFPR_PULL_LOW | MFPR_PULLUP_EN)
#define MFPR_PULL_HIGH		(MFPR_PULL_SEL | MFPR_PULLUP_EN)
#define MFPR_PULL_FLOAT		(MFPR_PULL_SEL)

/* mfp_spin_lock is used to ensure that MFP register configuration
 * (most likely a read-modify-write operation) is atomic, and that
 * mfp_table[] is consistent
 */
static DEFINE_SPINLOCK(mfp_spin_lock);

static void __iomem *mfpr_mmio_base;
/*
 * perform a read-back of any valid MFPR register to make sure the
 * previous writings are finished
 */
static unsigned long mfpr_off_readback;
#define mfpr_sync()	(void)__raw_readl(mfpr_mmio_base + mfpr_off_readback)

struct mfp_pin {
	unsigned long	config;		/* -1 for not configured */
	unsigned long	mfpr_off;	/* MFPRxx Register offset */
	unsigned long	mfpr_run;	/* Run-Mode Register Value */
	unsigned long	mfpr_lpm;	/* Low Power Mode Register Value */
};

struct mfp_addr_map {
	unsigned int    start;
	unsigned int    end;
	unsigned long   offset;
};

#define MFP_PIN_MAX		199
#define DESCEND_OFFSET_FLAG (0x1 << 31)
void __init mfp_init_base(void __iomem *mfpr_base);
void __init mfp_init_addr(struct mfp_addr_map *map);
unsigned long mfp_read(int mfp);
void mfp_write(int mfp, unsigned long mfpr_val);

static struct mfp_pin mfp_table[MFP_PIN_MAX];

static struct mfp_addr_map pxa19xx_addr_map[] __initdata = {
/*
 *{ PXA1908/PXA1936 Model }
 *{start pin number, end pin number, offset address of start pin}
 */
	{0, 5, 0x260},
	{6, 7, 0x60 | DESCEND_OFFSET_FLAG},
	{8, 9, 0x44},
	{10, 25, 0x40 | DESCEND_OFFSET_FLAG},
	{26, 26, 0x68},
	{27, 27, 0x78},
	{28, 29, 0x58 | DESCEND_OFFSET_FLAG},
	{30, 30, 0x74},
	{31, 31, 0x7c},
	{32, 33, 0x6c},
	{34, 35, 0x4c},
	{36, 36, 0x80},
	{37, 37, 0x64},
	{38, 48, 0xac | DESCEND_OFFSET_FLAG},
	{49, 49, 0xb0},
	{50, 51, 0xcc},
	{52, 52, 0xbc},
	{53, 53, 0xb4},
	{54, 54, 0xc0},
	{55, 55, 0xb8},
	{56, 56, 0xc8},
	{57, 58, 0xd8 | DESCEND_OFFSET_FLAG},
	{59, 97, 0x11c},
	{98, 101, 0x10c},
	{102, 115, 0x1b8},
	{116, 127, 0xdc},
	{128, 145, 0x1f0},
//	{146, 149, 0x},
	{-1, 0},
};

struct dvs_mfpr_offset {
	unsigned long offset;
	int gpio;
	int func;
};

static struct dvs_mfpr_offset dvs_offset[] = {
/*
 * the following DVS gpios are from SM-J100F_GPIO.xls & SM-J700F_GPIO.xls
 * { offset in pxa1936_addr_map, gpio number, select PIN as GPIO function}
 */
	{0, 25, 6},
	{1, 26, 6},
	{2, 27, 6},
	{3, 28, 6},
	{4, 79, 6},
	{5, 80, 6},
	{6, 107, 0},
	{7, 12, 3},
	{8, 101, 1},
	{9, 102, 1},
	{10, 11, 2},
	{11, 10, 2},
	{12, 9, 2},
	{13, 8, 2},
	{14, 7, 2},
	{15, 6, 2},
	{16, 5, 2},
	{17, 4, 2},
	{18, 100, 1},
	{19, 66, 1},
	{20, 65, 1},
	{21, 64, 1},
	{22, 63, 1},
	{23, 62, 1},
	{24, 61, 1},
	{25, 60, 1},
	{26, 108, 1},
	{27, 1, 1},
	{28, 106, 0},
	{29, 105, 0},
	{30, 0, 1},
	{31, 2, 1},
	{32, 126, 0},
	{33, 127, 0},
	{34, 103, 1},
	{35, 104, 1},
	{36, 3, 1},
	{37, 13, 3},
	{38, 23, 1},
	{39, 18, 4},
	{40, 17, 4},
	{41, 16, 4},
	{42, 15, 4},
	{43, 14, 4},
	{44, -1, 7},
	{45, 22, 1},
	{46, 21, 1},
	{47, 20, 1},
	{48, 24, 1},
	{49, 99, 1},
	{50, 123, 1},
	{51, 124, 0},
	{52, 119, 1},
	{53, 117, 1},
	{54, 120, 1},
	{55, 118, 1},
	{56, 122, 1},
	{57, -1, 7},
	{58, 125, 1},
	{59, 16, 0},
	{60, 17, 0},
	{61, 18, 0},
	{62, 19, 0},
	{63, 20, 0},
	{64, 21, 0},
	{65, 22, 0},
	{66, 23, 0},
	{67, 24, 0},
	{68, 25, 0},
	{69, 26, 0},
	{70, 27, 0},
	{71, 28, 0},
	{72, 29, 0},
	{73, 30, 0},
	{74, 31, 0},
	{75, 32, 0},
	{76, 33, 0},
	{77, 34, 0},
	{78, 35, 0},
	{79, 36, 0},
	{80, 37, 0},
	{81, 38, 0},
	{82, 39, 0},
	{83, 40, 0},
	{84, 41, 0},
	{85, 42, 0},
	{86, 43, 0},
	{87, 44, 0},
	{88, 45, 0},
	{89, 46, 0},
	{90, 47, 0},
	{91, 48, 0},
	{92, 49, 0},
	{93, 50, 0},
	{94, 51, 0},
	{95, 52, 0},
	{96, 53, 0},
	{97, 54, 0},
	{98, 12, 0},
	{99, 13, 0},
	{100, 14, 0},
	{101, 15, 0},
	{102, 67, 0},
	{103, 68, 0},
	{104, 69, 0},
	{105, 70, 0},
	{106, 71, 0},
	{107, 72, 0},
	{108, 73, 0},
	{109, 74, 0},
	{110, 75, 0},
	{111, 76, 0},
	{112, 77, 0},
	{113, 78, 0},
	{114, 79, 0},
	{115, 80, 0},
	{116, 0, 0},
	{117, 1, 0},
	{118, 2, 0},
	{119, 3, 0},
	{120, 4, 0},
	{121, 5, 0},
	{122, 6, 0},
	{123, 7, 0},
	{124, 8, 0},
	{125, 9, 0},
	{126, 10, 0},
	{127, 11, 0},
	{128, 81, 0},
	{129, 82, 0},
	{130, 83, 0},
	{131, 84, 0},
	{132, 85, 0},
	{133, 86, 0},
	{134, 87, 0},
	{135, 88, 0},
	{136, 89, 0},
	{137, 90, 0},
	{138, 91, 0},
	{139, 92, 0},
	{140, 93, 0},
	{141, 94, 0},
	{142, 95, 0},
	{143, 96, 0},
	{144, 97, 0},
	{145, 98, 0},
//	{146, -1, 0},
//	{147, -1, 0},
//	{148, -1, 0},
//	{149, -1, 0},
};

#define mfpr_readl(off)			\
	__raw_readl(mfpr_mmio_base + (off))

#define mfpr_writel(off, val)		\
	__raw_writel(val, mfpr_mmio_base + (off))

#define mfp_configured(p)	((p)->config != -1)

unsigned long mfp_read(int mfp)
{
	unsigned long val, flags;

	BUG_ON(mfp < 0 || mfp >= MFP_PIN_MAX);

	spin_lock_irqsave(&mfp_spin_lock, flags);
	val = mfpr_readl(mfp_table[mfp].mfpr_off);
	spin_unlock_irqrestore(&mfp_spin_lock, flags);

	return val;
}

void mfp_write(int mfp, unsigned long val)
{
	unsigned long flags;

	BUG_ON(mfp < 0 || mfp >= MFP_PIN_MAX);

	spin_lock_irqsave(&mfp_spin_lock, flags);
	mfpr_writel(mfp_table[mfp].mfpr_off, val);
	mfpr_sync();
	spin_unlock_irqrestore(&mfp_spin_lock, flags);
}

void __init mfp_init_base(void __iomem *mfpr_base)
{
	int i;
	/* initialize the table with default - unconfigured */
	for (i = 0; i < ARRAY_SIZE(mfp_table); i++)
		mfp_table[i].config = -1;

	mfpr_mmio_base = mfpr_base;
}
#endif

static int mfp_probe(struct platform_device *dev)
{
	static u32 mfpreg_data[2] = {0};
	struct device_node *node;
#ifdef CONFIG_SEC_GPIO_DVS
	void __iomem *va_base;
#endif
	node = of_find_compatible_node(NULL, NULL, "marvell,mmp-mfp-leftover");

	if (!node) {
		pr_err("MFP DTS is not found and no init on MFP\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(node, "reg", mfpreg_data,
		ARRAY_SIZE(mfpreg_data))) {
		pr_err("Cannot get MFP Reg Base and Length\n");
		return -EINVAL;
	}

	pr_debug("MFPReg Base : 0x%x\n", mfpreg_data[0]);
	pr_debug("MFPReg Length: 0x%x\n", mfpreg_data[1]);

#ifdef CONFIG_SEC_GPIO_DVS
	va_base = ioremap(mfpreg_data[0], mfpreg_data[1]);
	mfp_init_base(va_base);
#endif
	debugfs_create_file("mfp_setting_dump",
			S_IRUGO, NULL, mfpreg_data, &mfp_dump_fops);

	if (!mfp_dump_data)
		if (pins_functbl_init(mfpreg_data[1], node))
			pr_info("HW Pin func table Initialization fails!\n");

	pr_info("MFP Configuration is inited in MFP init driver\n");
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id mfp_dt_ids[] = {
	{ .compatible = "marvell,mmp-mfp-leftover", },
	{}
};
MODULE_DEVICE_TABLE(of, mfp_dt_ids);
#endif

static struct platform_driver mfp_driver = {
	.probe		= mfp_probe,
	.driver		= {
		.name	= "mmp-mfp-leftover",
		.owner	= THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(mfp_dt_ids),
#endif
	},
};

#ifdef CONFIG_SEC_GPIO_DVS
/****************************************************************/
/* Define value in accordance with
    the specification of each BB vendor. */
#define AP_GPIO_COUNT	199
#define GET_RESULT_GPIO(a, b, c)	\
	((a<<4 & 0xF0) | (b<<1 & 0xE) | (c & 0x1))
/****************************************************************/

static void plat_gpio_input_config(unsigned gpio, int value)
{
	unsigned int mfpr = mfp_read(gpio);
	pr_info("plat_gpio_input_config() mfpr %x\n", mfpr);

	switch (value)
	{
		case 0: //NP
			mfpr = (mfpr & (~(MFPR_PULL_SEL | MFPR_PULLDOWN_EN | MFPR_PULLUP_EN))) | MFPR_PULL_FLOAT;
			break;
		case 1: //PD
			mfpr = (mfpr & (~(MFPR_PULL_SEL | MFPR_PULLDOWN_EN | MFPR_PULLUP_EN))) | MFPR_PULL_LOW;
			break;
		case 2: //PU
			mfpr = (mfpr & (~(MFPR_PULL_SEL | MFPR_PULLDOWN_EN | MFPR_PULLUP_EN))) | MFPR_PULL_HIGH;
			break;
		default:
			break;
	}

	if (mfpr & MFPR_SLEEP_SEL)
	{
		mfpr = (mfpr & (~(MFPR_SLEEP_OE_N|MFPR_SLEEP_DATA(1)))) | MFPR_LPM_INPUT;
	}

	mfp_write(gpio, mfpr);

	pr_info("plat_gpio_input_config() mfpr %x\n", mfpr);

	gpio_request(gpio, "gpio_input_test_on");
	gpio_direction_input(gpio);
	gpio_free(gpio);
}

static void plat_gpio_output_config(unsigned gpio, int value)
{
	unsigned int mfpr = mfp_read(gpio);
	pr_info("plat_gpio_output_config() mfpr %x\n", mfpr);

	if (mfpr & MFPR_SLEEP_SEL)
	{
		if (value == 1)
		{
			mfpr = (mfpr & (~(MFPR_SLEEP_OE_N|MFPR_SLEEP_DATA(1)))) | MFPR_LPM_DRIVE_HIGH;
		}
		else
		{
			mfpr = (mfpr & (~(MFPR_SLEEP_OE_N|MFPR_SLEEP_DATA(1)))) | MFPR_LPM_DRIVE_LOW;
		}
	}

	mfp_write(gpio, mfpr);

	pr_info("plat_gpio_output_config() mfpr %x\n", mfpr);

	gpio_request(gpio, "gpio_output_test_on");
	gpio_direction_output(gpio, 1);
	gpio_free(gpio);

	if (value == 1)
	{
		gpio_request(gpio, "gpio_output_test_on");
		gpio_set_value(gpio, 1);
		gpio_free(gpio);
	} else if (value == 0) {
		gpio_request(gpio, "gpio_output_test_off");
		gpio_set_value(gpio, 0);
		gpio_free(gpio);
	}
}

/****************************************************************/
/* Pre-defined variables. (DO NOT CHANGE THIS!!) */
static unsigned char checkgpiomap_result[GDVS_PHONE_STATUS_MAX][AP_GPIO_COUNT];
static struct gpiomap_result gpiomap_result = {
	.init = checkgpiomap_result[PHONE_INIT],
	.sleep = checkgpiomap_result[PHONE_SLEEP]
};
/****************************************************************/

static unsigned __check_init_direction(int gpio)
{
	int index, offset;
	unsigned int gpdr;

	if (gpio < 0)
		return 0; /* IS function PIN */

	index = gpio / 32;
	offset = gpio % 32;

	pxa_direction_get(&gpdr, index);

	return (gpdr & (0x1 << offset)) > 0 ? 2 : 1;
}

static unsigned __check_mfp_direction(unsigned mfpr_val, int func)
{
	u32 ret = 0;
	if (MFPR_AF_SEL(mfpr_val) != func) ret = 0x0; /* FUNC */
	else {
		if (mfpr_val & MFPR_SLEEP_OE_N) ret = 0x01; /* INPUT */
		else ret = 0x2; /*OUTPUT */
	}
	return ret;
}

static unsigned __check_mfp_pull_status(unsigned mfpr_val)
{
	u32 ret = 0;
	if (mfpr_val & MFPR_PULL_SEL) {
		if (mfpr_val & MFPR_PULLUP_EN) ret = 0x2; /* PULL_UP */
		else if (mfpr_val & MFPR_PULLDOWN_EN) ret = 0x01; /* PULL_DOWN */
		else ret = 0x0; /* NO_PULL */
	} else
		ret = 0x0; /* NO_PULL */
	return ret;
}

static unsigned __check_mfp_data_status(unsigned mfpr_val, int gpio, unsigned dir)
{
	u32 ret = 0;

	switch (dir) {
		case 0:
			ret = 0x0; /* FUNC */
			break;
		case 1:
			ret = gpio_get_value(gpio);
			break;
		case 2:
			if (mfpr_val & (0x1 << 8)) ret = 0x1; /* HIGH */
			else ret = 0x0; /* LOW */
			break;
		default:
			break;
	}

	return ret;
}

static void pxa_check_mfp_configuration(unsigned char phonestate)
{
	u8 temp_io = 0, temp_pdpu = 0, temp_lh = 0;
	int i;

	pr_info("[secgpio_dvs][%s] state : %s\n", __func__,
		(phonestate == PHONE_INIT) ? "init" : "sleep");

	for (i = 0; i < ARRAY_SIZE(dvs_offset); i++) {
		unsigned long mfpr_val, offset;
		int gpio, sleep_mode;

		offset = dvs_offset[i].offset;
		gpio = dvs_offset[i].gpio;
		mfpr_val = mfp_read(offset);

		sleep_mode = mfpr_val & ((0x1 << 9) | (0x1 << 3));

		if ((phonestate == PHONE_INIT) || (sleep_mode == 0)) {
			temp_io = __check_mfp_direction(mfpr_val, dvs_offset[i].func);
			if (temp_io == 0) {
				temp_lh = 0;
			} else {
				temp_io = __check_init_direction(gpio);
				temp_lh = gpio_get_value(gpio);
			}
		} else {
			temp_io = __check_mfp_direction(mfpr_val, dvs_offset[i].func);
			temp_lh = __check_mfp_data_status(mfpr_val, gpio, temp_io);
		}
		temp_pdpu = __check_mfp_pull_status(mfpr_val);

		checkgpiomap_result[phonestate][i] =
			GET_RESULT_GPIO(temp_io, temp_pdpu, temp_lh);
	}

	pr_info("[secgpio_dvs][%s]-\n", __func__);

	return;
}
/****************************************************************/
/* Define appropriate variable in accordance with
    the specification of each BB vendor */
static struct gpio_dvs pxa_gpio_dvs = {
	.result = &gpiomap_result,
	.check_gpio_status = pxa_check_mfp_configuration,
	.gpio_input_config = plat_gpio_input_config,
	.gpio_output_config = plat_gpio_output_config,
	.count = AP_GPIO_COUNT,
};
/****************************************************************/

void __init mfp_init_addr(struct mfp_addr_map *map)
{
	struct mfp_addr_map *p;
	unsigned long offset, flags;
	int i;
	int descend_offset = 0;

	spin_lock_irqsave(&mfp_spin_lock, flags);

	pxa_gpio_dvs.count = ARRAY_SIZE(dvs_offset);

	/* mfp offset for readback */
	mfpr_off_readback = map[0].offset;

	for (p = map; p->start != -1; p++) {
		offset = p->offset;
		i = p->start;

		if (unlikely(offset & (DESCEND_OFFSET_FLAG))) {
			offset &= ~DESCEND_OFFSET_FLAG;
			descend_offset = 1;
		}

		do {
			mfp_table[i].mfpr_off = offset;
			mfp_table[i].mfpr_run = 0;
			mfp_table[i].mfpr_lpm = 0;
			if (descend_offset)
				offset -= 4;
			else
				offset += 4;
			i++;
		} while ((i <= p->end) && (p->end != -1));
		descend_offset = 0;
	}

	spin_unlock_irqrestore(&mfp_spin_lock, flags);
}

static struct platform_device secgpio_dvs_device = {
	.name	= "secgpio_dvs",
	.id		= -1,
/****************************************************************/
/* Designate appropriate variable pointer
    in accordance with the specification of each BB vendor. */
	.dev.platform_data = &pxa_gpio_dvs,
/****************************************************************/
};

static struct platform_device *secgpio_dvs_devices[] __initdata = {
		&secgpio_dvs_device,
};
#endif

int __init mfp_init(void)
{
#ifdef CONFIG_SEC_GPIO_DVS
	mfp_init_addr(pxa19xx_addr_map);
	platform_add_devices(secgpio_dvs_devices, ARRAY_SIZE(secgpio_dvs_devices));
#endif
	return platform_driver_register(&mfp_driver);
}

subsys_initcall(mfp_init);

MODULE_AUTHOR("Fan Wu<fwu@marvell.com>");
MODULE_DESCRIPTION("MFP Initialization Driver for MMP");
MODULE_LICENSE("GPL v2");
