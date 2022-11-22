/*
 * LP5562 LED driver
 *
 * Copyright (C) 2013 Texas Instruments
 *
 * Author: Milo(Woogyom) Kim <milo.kim@ti.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_data/leds-lp55xx.h>
#include <linux/slab.h>
#include <linux/sec_sysfs.h>

#include "leds-lp55xx-common.h"

#define LP5562_PROGRAM_LENGTH		32
#define LP5562_MAX_LEDS			4

/* ENABLE Register 00h */
#define LP5562_REG_ENABLE		0x00
#define LP5562_EXEC_ENG1_M		0x30
#define LP5562_EXEC_ENG2_M		0x0C
#define LP5562_EXEC_ENG3_M		0x03
#define LP5562_EXEC_M			0x3F
#define LP5562_MASTER_ENABLE		0x40	/* Chip master enable */
#define LP5562_LOGARITHMIC_PWM		0x80	/* Logarithmic PWM adjustment */
#define LP5562_EXEC_RUN			0x2A
#define LP5562_ENABLE_DEFAULT	\
	(LP5562_MASTER_ENABLE | LP5562_LOGARITHMIC_PWM)
#define LP5562_ENABLE_RUN_PROGRAM	\
	(LP5562_ENABLE_DEFAULT | LP5562_EXEC_RUN)

/* OPMODE Register 01h */
#define LP5562_REG_OP_MODE		0x01
#define LP5562_MODE_ENG1_M		0x30
#define LP5562_MODE_ENG2_M		0x0C
#define LP5562_MODE_ENG3_M		0x03
#define LP5562_LOAD_ENG1		0x10
#define LP5562_LOAD_ENG2		0x04
#define LP5562_LOAD_ENG3		0x01
#define LP5562_RUN_ENG1			0x20
#define LP5562_RUN_ENG2			0x08
#define LP5562_RUN_ENG3			0x02
#define LP5562_ENG1_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG1_M) == LP5562_LOAD_ENG1)
#define LP5562_ENG2_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG2_M) == LP5562_LOAD_ENG2)
#define LP5562_ENG3_IS_LOADING(mode)	\
	((mode & LP5562_MODE_ENG3_M) == LP5562_LOAD_ENG3)

/* BRIGHTNESS Registers */
#define LP5562_REG_R_PWM		0x04
#define LP5562_REG_G_PWM		0x03
#define LP5562_REG_B_PWM		0x02
#define LP5562_REG_W_PWM		0x0E

/* CURRENT Registers */
#define LP5562_REG_R_CURRENT		0x07
#define LP5562_REG_G_CURRENT		0x06
#define LP5562_REG_B_CURRENT		0x05
#define LP5562_REG_W_CURRENT		0x0F

/* CONFIG Register 08h */
#define LP5562_REG_CONFIG		0x08
#define LP5562_PWM_HF			0x40
#define LP5562_PWRSAVE_EN		0x20
#define LP5562_CLK_INT			0x01	/* Internal clock */
#define LP5562_DEFAULT_CFG		(LP5562_PWM_HF | LP5562_PWRSAVE_EN)

/* RESET Register 0Dh */
#define LP5562_REG_RESET		0x0D
#define LP5562_RESET			0xFF

/* PROGRAM ENGINE Registers */
#define LP5562_REG_PROG_MEM_ENG1	0x10
#define LP5562_REG_PROG_MEM_ENG2	0x30
#define LP5562_REG_PROG_MEM_ENG3	0x50

/* LEDMAP Register 70h */
#define LP5562_REG_ENG_SEL		0x70
#define LP5562_ENG_SEL_PWM		0
#define LP5562_ENG_FOR_RGB_M		0x3F
#define LP5562_ENG_SEL_RGB		0x1B	/* R:ENG1, G:ENG2, B:ENG3 */
#define LP5562_ENG_FOR_W_M		0xC0
#define LP5562_ENG1_FOR_W		0x40	/* W:ENG1 */
#define LP5562_ENG2_FOR_W		0x80	/* W:ENG2 */
#define LP5562_ENG3_FOR_W		0xC0	/* W:ENG3 */

/* Program Commands */
#define LP5562_CMD_DISABLE		0x00
#define LP5562_CMD_LOAD			0x15
#define LP5562_CMD_RUN			0x2A
#define LP5562_CMD_DIRECT		0x3F
#define LP5562_PATTERN_OFF		0

#define SEC_LED_SPECIFIC

#ifdef SEC_LED_SPECIFIC
/* Program Commands */
#define CMD_SET_PWM			0x40
#define CMD_WAIT_LSB			0x00
#define MAX_BLINK_TIME			60000	/* 60 sec */

extern struct class *sec_class;
static struct device *led_dev;
/*path : /sys/class/sec/led/led_pattern*/
/*path : /sys/class/sec/led/led_blink*/
/*path : /sys/class/sec/led/led_brightness*/
/*path : /sys/class/leds/led_r/brightness*/
/*path : /sys/class/leds/led_g/brightness*/
/*path : /sys/class/leds/led_b/brightness*/
struct lp55xx_chip *g_chip;

static u8 LED_DYNAMIC_CURRENT = 0x28;
static u8 LED_LOWPOWER_MODE = 0x0;

enum lp5562_wait_type {
	LP5562_CYCLE_INVALID,
	LP5562_CYCLE_50ms,
	LP5562_CYCLE_100ms,
	LP5562_CYCLE_200ms,
	LP5562_CYCLE_500ms,
	LP5562_CYCLE_700ms,
	LP5562_CYCLE_920ms,
	LP5562_CYCLE_982ms,
	LP5562_CYCLE_MAX,
};

struct lp5562_wait_param {
	unsigned cycle;
	unsigned limit;
	u8 cmd;
};

struct lp5562_pattern_data {
	u8 r[LP5562_PROGRAM_LENGTH];
	u8 g[LP5562_PROGRAM_LENGTH];
	u8 b[LP5562_PROGRAM_LENGTH];
	unsigned pc_r;
	unsigned pc_g;
	unsigned pc_b;
};

static const struct lp5562_wait_param lp5562_wait_cycle[LP5562_CYCLE_MAX] = {
	[LP5562_CYCLE_50ms] = {
		.cycle = 50,
		.limit = 3000,
		.cmd   = 0x43,
	},
	[LP5562_CYCLE_100ms] = {
		.cycle = 100,
		.limit = 6000,
		.cmd   = 0x46,
	},
	[LP5562_CYCLE_200ms] = {
		.cycle = 200,
		.limit = 10000,
		.cmd   = 0x4d,
	},
	[LP5562_CYCLE_500ms] = {
		.cycle = 500,
		.limit = 30000,
		.cmd   = 0x60,
	},
	[LP5562_CYCLE_700ms] = {
		.cycle = 700,
		.limit = 40000,
		.cmd   = 0x6d,
	},
	[LP5562_CYCLE_920ms] = {
		.cycle = 920,
		.limit = 50000,
		.cmd   = 0x7b,
	},
	[LP5562_CYCLE_982ms] = {
		.cycle = 982,
		.limit = 60000,
		.cmd   = 0x7f,
	},
};
#endif

static inline void lp5562_wait_opmode_done(void)
{
	/* operation mode change needs to be longer than 153 us */
	usleep_range(200, 300);
}

static inline void lp5562_wait_enable_done(void)
{
	/* it takes more 488 us to update ENABLE register */
	usleep_range(500, 600);
}

static void lp5562_set_led_current(struct lp55xx_led *led, u8 led_current)
{
	u8 addr[] = {
		LP5562_REG_R_CURRENT,
		LP5562_REG_G_CURRENT,
		LP5562_REG_B_CURRENT,
		LP5562_REG_W_CURRENT,
	};
	pr_info("%s\n", __func__);

	led->led_current = led_current;
	lp55xx_write(led->chip, addr[led->chan_nr], led_current);
}

static void lp5562_load_engine(struct lp55xx_chip *chip)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 mask[] = {
		[LP55XX_ENGINE_1] = LP5562_MODE_ENG1_M,
		[LP55XX_ENGINE_2] = LP5562_MODE_ENG2_M,
		[LP55XX_ENGINE_3] = LP5562_MODE_ENG3_M,
	};

	u8 val[] = {
		[LP55XX_ENGINE_1] = LP5562_LOAD_ENG1,
		[LP55XX_ENGINE_2] = LP5562_LOAD_ENG2,
		[LP55XX_ENGINE_3] = LP5562_LOAD_ENG3,
	};

	pr_info("%s\n", __func__);
	lp55xx_update_bits(chip, LP5562_REG_OP_MODE, mask[idx], val[idx]);

	lp5562_wait_opmode_done();
}

static void lp5562_stop_engine(struct lp55xx_chip *chip)
{
	pr_info("%s\n", __func__);
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DISABLE);
	lp5562_wait_opmode_done();
}

static void lp5562_run_engine(struct lp55xx_chip *chip, bool start)
{
	int ret;
	u8 mode;
	u8 exec;

	pr_info("%s %d\n", __func__, start);
	/* stop engine */
	if (!start) {
		lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
		lp5562_wait_enable_done();
		lp5562_stop_engine(chip);
		lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
		lp5562_wait_opmode_done();
		return;
	}

	/*
	 * To run the engine,
	 * operation mode and enable register should updated at the same time
	 */

	ret = lp55xx_read(chip, LP5562_REG_OP_MODE, &mode);
	if (ret)
		return;

	ret = lp55xx_read(chip, LP5562_REG_ENABLE, &exec);
	if (ret)
		return;

	/* change operation mode to RUN only when each engine is loading */
	if (LP5562_ENG1_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG1_M) | LP5562_RUN_ENG1;
		exec = (exec & ~LP5562_EXEC_ENG1_M) | LP5562_RUN_ENG1;
	}

	if (LP5562_ENG2_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG2_M) | LP5562_RUN_ENG2;
		exec = (exec & ~LP5562_EXEC_ENG2_M) | LP5562_RUN_ENG2;
	}

	if (LP5562_ENG3_IS_LOADING(mode)) {
		mode = (mode & ~LP5562_MODE_ENG3_M) | LP5562_RUN_ENG3;
		exec = (exec & ~LP5562_EXEC_ENG3_M) | LP5562_RUN_ENG3;
	}

	lp55xx_write(chip, LP5562_REG_OP_MODE, mode);
	lp5562_wait_opmode_done();

	lp55xx_update_bits(chip, LP5562_REG_ENABLE, LP5562_EXEC_M, exec);
	lp5562_wait_enable_done();
}

static int lp5562_update_firmware(struct lp55xx_chip *chip,
					const u8 *data, size_t size)
{
	enum lp55xx_engine_index idx = chip->engine_idx;
	u8 pattern[LP5562_PROGRAM_LENGTH] = {0};
	u8 addr[] = {
		[LP55XX_ENGINE_1] = LP5562_REG_PROG_MEM_ENG1,
		[LP55XX_ENGINE_2] = LP5562_REG_PROG_MEM_ENG2,
		[LP55XX_ENGINE_3] = LP5562_REG_PROG_MEM_ENG3,
	};
	unsigned cmd;
	char c[3];
	int program_size;
	int nrchars;
	int offset = 0;
	int ret;
	int i;

	pr_info("%s\n", __func__);
	/* clear program memory before updating */
	for (i = 0; i < LP5562_PROGRAM_LENGTH; i++)
		lp55xx_write(chip, addr[idx] + i, 0);

	i = 0;
	while ((offset < size - 1) && (i < LP5562_PROGRAM_LENGTH)) {
		/* separate sscanfs because length is working only for %s */
		ret = sscanf(data + offset, "%2s%n ", c, &nrchars);
		if (ret != 1)
			goto err;

		ret = sscanf(c, "%2x", &cmd);
		if (ret != 1)
			goto err;

		pattern[i] = (u8)cmd;
		offset += nrchars;
		i++;
	}

	/* Each instruction is 16bit long. Check that length is even */
	if (i % 2)
		goto err;

	program_size = i;
	for (i = 0; i < program_size; i++)
		lp55xx_write(chip, addr[idx] + i, pattern[i]);

	return 0;

err:
	dev_err(&chip->cl->dev, "wrong pattern format\n");
	return -EINVAL;
}

static void lp5562_firmware_loaded(struct lp55xx_chip *chip)
{
	const struct firmware *fw = chip->fw;

	pr_info("%s\n", __func__);
	if (fw->size > LP5562_PROGRAM_LENGTH) {
		dev_err(&chip->cl->dev, "firmware data size overflow: %zu\n",
			fw->size);
		return;
	}

	/*
	 * Program momery sequence
	 *  1) set engine mode to "LOAD"
	 *  2) write firmware data into program memory
	 */

	lp5562_load_engine(chip);
	lp5562_update_firmware(chip, fw->data, fw->size);
}

static int lp5562_post_init_device(struct lp55xx_chip *chip)
{
	int ret;
	u8 cfg = LP5562_DEFAULT_CFG;

	pr_info("%s\n", __func__);
	/* Set all PWMs to direct control mode */
	ret = lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
	if (ret)
		return ret;

	lp5562_wait_opmode_done();

#ifdef MAINLINE_BUT_NOT_USE
	/* Update configuration for the clock setting */
	if (!lp55xx_is_extclk_used(chip))
		cfg |= LP5562_CLK_INT;
#endif

	ret = lp55xx_write(chip, LP5562_REG_CONFIG, cfg);
	if (ret)
		return ret;

	/* Initialize all channels PWM to zero -> leds off */
	lp55xx_write(chip, LP5562_REG_R_PWM, 0);
	lp55xx_write(chip, LP5562_REG_G_PWM, 0);
	lp55xx_write(chip, LP5562_REG_B_PWM, 0);
	lp55xx_write(chip, LP5562_REG_W_PWM, 0);

	/* Set LED map as register PWM by default */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);

	return 0;
}

static void lp5562_led_brightness_work(struct work_struct *work)
{
	struct lp55xx_led *led = container_of(work, struct lp55xx_led,
					      brightness_work);
	struct lp55xx_chip *chip = led->chip;
	u8 addr[] = {
		LP5562_REG_R_PWM,
		LP5562_REG_G_PWM,
		LP5562_REG_B_PWM,
		LP5562_REG_W_PWM,
	};

	pr_info("%s\n", __func__);
	mutex_lock(&chip->lock);
	lp55xx_write(chip, addr[led->chan_nr], led->brightness);
	mutex_unlock(&chip->lock);
}

static void lp5562_write_program_memory(struct lp55xx_chip *chip,
					u8 base, const u8 *rgb, int size)
{
	int i;

	pr_info("%s\n", __func__);
	if (!rgb || size <= 0)
		return;

	for (i = 0; i < size; i++)
		lp55xx_write(chip, base + i, *(rgb + i));

	lp55xx_write(chip, base + i, 0);
	lp55xx_write(chip, base + i + 1, 0);
}

/* Used in J, to enable led_blink */
static enum lp5562_wait_type lp5562_lookup_cycle(unsigned int ms)
{
	int i;

	pr_info("%s\n", __func__);

	for (i = LP5562_CYCLE_50ms; i < LP5562_CYCLE_MAX; i++) {
		if (ms > lp5562_wait_cycle[i-1].limit &&
		    ms <= lp5562_wait_cycle[i].limit)
			return i;
	}

	return LP5562_CYCLE_INVALID;
}

static void lp5562_set_wait_cmd(struct lp5562_pattern_data *ptn,
				unsigned int ms, u8 jump)
{
	enum lp5562_wait_type type = lp5562_lookup_cycle(ms);
	unsigned int loop = ms / lp5562_wait_cycle[type].cycle;
	u8 cmd_msb = lp5562_wait_cycle[type].cmd;
	u8 msb;
	u8 lsb;
	u16 branch;

	pr_info("%s\n", __func__);
	WARN_ON(!cmd_msb);
	WARN_ON(loop > 64);

	/* wait command */
	ptn->r[ptn->pc_r++] = cmd_msb;
	ptn->r[ptn->pc_r++] = CMD_WAIT_LSB;
	ptn->g[ptn->pc_g++] = cmd_msb;
	ptn->g[ptn->pc_g++] = CMD_WAIT_LSB;
	ptn->b[ptn->pc_b++] = cmd_msb;
	ptn->b[ptn->pc_b++] = CMD_WAIT_LSB;

	/* branch command : if wait time is bigger than cycle msec,
			branch is used for command looping */
	if (loop > 1) {
		branch = (5 << 13) | ((loop - 1) << 7) | jump;
		msb = (branch >> 8) & 0xFF;
		lsb = branch & 0xFF;

		ptn->r[ptn->pc_r++] = msb;
		ptn->r[ptn->pc_r++] = lsb;
		ptn->g[ptn->pc_g++] = msb;
		ptn->g[ptn->pc_g++] = lsb;
		ptn->b[ptn->pc_b++] = msb;
		ptn->b[ptn->pc_b++] = lsb;
	}
}

static void lp5562_set_pwm_cmd(struct lp5562_pattern_data *ptn,
			unsigned int color)
{
	u8 r = (color >> 16) & 0xFF;
	u8 g = (color >> 8) & 0xFF;
	u8 b = color & 0xFF;

	pr_info("%s\n", __func__);
	ptn->r[ptn->pc_r++] = CMD_SET_PWM;
	ptn->r[ptn->pc_r++] = r;
	ptn->g[ptn->pc_g++] = CMD_SET_PWM;
	ptn->g[ptn->pc_g++] = g;
	ptn->b[ptn->pc_b++] = CMD_SET_PWM;
	ptn->b[ptn->pc_b++] = b;
}

static void lp5562_clear_program_memory(struct lp55xx_chip *chip)
{
	int i;

	u8 rgb_mem[] = {
		LP5562_REG_PROG_MEM_ENG1,
		LP5562_REG_PROG_MEM_ENG2,
		LP5562_REG_PROG_MEM_ENG3,
	};

	pr_info("%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(rgb_mem); i++) {
		lp55xx_write(chip, rgb_mem[i], 0);
		lp55xx_write(chip, rgb_mem[i] + 1, 0);
	}
}

/* have a same name function in 3.10 but parameter is different */
static inline bool _is_pc_overflow_old(struct lp5562_pattern_data *ptn)
{
	return (ptn->pc_r >= LP5562_PROGRAM_LENGTH ||
		ptn->pc_g >= LP5562_PROGRAM_LENGTH ||
		ptn->pc_b >= LP5562_PROGRAM_LENGTH);
}

static void lp5562_run_led_pattern(struct lp55xx_chip *chip,
				struct lp5562_pattern_data *ptn)
{
	pr_info("%s\n", __func__);
	WARN_ON(_is_pc_overflow_old(ptn));

	/* Set LED map as RGB */
	lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_RGB);

	/* OP mode : LOAD */
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_LOAD);
	lp5562_wait_opmode_done();

	/* copy pattern commands into the program memory */
	lp5562_clear_program_memory(chip);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG1,
				ptn->r, ptn->pc_r);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG2,
				ptn->g, ptn->pc_g);
	lp5562_write_program_memory(chip, LP5562_REG_PROG_MEM_ENG3,
				ptn->b, ptn->pc_b);

	/* OP mode : RUN */
	lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_RUN);
	lp5562_wait_opmode_done();
	lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_RUN_PROGRAM);
}


/* check the size of program count */
static inline bool _is_pc_overflow(struct lp55xx_predef_pattern *ptn)
{
	return (ptn->size_r >= LP5562_PROGRAM_LENGTH ||
		ptn->size_g >= LP5562_PROGRAM_LENGTH ||
		ptn->size_b >= LP5562_PROGRAM_LENGTH);
}

static int lp5562_lookup_pattern(struct lp55xx_chip *chip, u8 offset,
				struct lp5562_pattern_data *ptn)
{
	struct lp55xx_predef_pattern *predef;

	if(offset < 1 || offset > chip->pdata->num_patterns)
		return -EINVAL;

	pr_info("%s\n", __func__);
	predef = chip->pdata->patterns + (offset - 1);

	/* copy predefined pattern to internal data */
	memcpy(ptn->r, predef->r, predef->size_r);
	memcpy(ptn->g, predef->g, predef->size_g);
	memcpy(ptn->b, predef->b, predef->size_b);
	ptn->pc_r = predef->size_r;
	ptn->pc_g = predef->size_g;
	ptn->pc_b = predef->size_b;

	return 0;
}

static int lp5562_run_predef_led_pattern(struct lp55xx_chip *chip, int mode)
{
	int num_patterns = chip->pdata->num_patterns;
	int ret = -EINVAL;

	pr_info("%s: mode %d, lpmode %d\n", __func__, mode, LED_LOWPOWER_MODE);
	/* invalid pattern data */
	if (mode > num_patterns || !(chip->pdata->patterns))
		return ret;

	if (mode == LP5562_PATTERN_OFF) {
		lp55xx_write(chip, LP5562_REG_ENABLE, LP5562_ENABLE_DEFAULT);
		lp5562_wait_enable_done();
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DISABLE);
		lp5562_wait_opmode_done();
		lp55xx_write(chip, LP5562_REG_ENG_SEL, LP5562_ENG_SEL_PWM);
		lp55xx_write(chip, LP5562_REG_OP_MODE, LP5562_CMD_DIRECT);
		lp5562_wait_opmode_done();
	} else {
		struct lp5562_pattern_data ptn = { };

		ret = lp5562_lookup_pattern(chip, mode, &ptn);
		if (ret)
			return ret;

		if (LED_LOWPOWER_MODE == 1)
			LED_DYNAMIC_CURRENT = 0x5;
		else
			LED_DYNAMIC_CURRENT = 0x28;

		chip->pdata->led_config[0].led_current = LED_DYNAMIC_CURRENT;
		chip->pdata->led_config[1].led_current = LED_DYNAMIC_CURRENT;
		chip->pdata->led_config[2].led_current = LED_DYNAMIC_CURRENT;

		lp55xx_write(chip, LP5562_REG_R_CURRENT, LED_DYNAMIC_CURRENT);
		lp55xx_write(chip, LP5562_REG_G_CURRENT, LED_DYNAMIC_CURRENT);
		lp55xx_write(chip, LP5562_REG_B_CURRENT, LED_DYNAMIC_CURRENT);

		lp5562_run_led_pattern(chip, &ptn);
	}

	return 0;

}

static ssize_t lp5562_store_pattern(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t len) {
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_predef_pattern *ptn = chip->pdata->patterns;
	int num_patterns = chip->pdata->num_patterns;
	unsigned long mode;
	int ret;

	ret = kstrtoul(buf, 0, &mode);
	if (ret)
		return ret;

	if (mode > num_patterns || !ptn)
		return -EINVAL;

	pr_info("%s: mode %lu, lpmode %d\n", __func__, mode, LED_LOWPOWER_MODE);
	mutex_lock(&chip->lock);
	ret = lp5562_run_predef_led_pattern(chip, mode);
	mutex_unlock(&chip->lock);

	if (ret)
		return ret;

	return len;
}

static ssize_t lp5562_show_blink(struct device *dev,
				struct device_attribute *attr,
				char *unused)
{
	return 0;
}

/* LED blink with the internal program engine */
static ssize_t lp5562_store_blink(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	unsigned int rgb = 0;
	unsigned int on = 0;
	unsigned int off = 0;
	struct lp5562_pattern_data ptn = { };
	u8 jump_pc = 0;

	sscanf(buf, "0x%08x %d %d", &rgb, &on, &off);
	pr_info("%s: 0x%08x %d %d\n", __func__, rgb, on, off);

	if (on == 0) {
        pr_info("%s: wrong on/off time. on: %d, off: %d\n",
            __func__, on, off);
        return len;
    }
    if (off == 0) {
        pr_info("%s: off time. on: %d, off: %d\n",
            __func__, on, off);

    }

	lp5562_stop_engine(chip);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on)
		return len;

	mutex_lock(&chip->lock);

	/* make on-time pattern */
	lp5562_set_pwm_cmd(&ptn, rgb);
	lp5562_set_wait_cmd(&ptn, on, jump_pc);
	jump_pc = ptn.pc_r / 2; /* 16bit size program counter */

	/* make off-time pattern */
	lp5562_set_pwm_cmd(&ptn, 0);
	lp5562_set_wait_cmd(&ptn, off, jump_pc);

	/* run the pattern */
	lp5562_run_led_pattern(chip, &ptn);

	printk(KERN_ERR "led_blink is called, Color:0x%X Brightness:%i\n",
			rgb, LED_DYNAMIC_CURRENT);

	mutex_unlock(&chip->lock);

	return len;
}

void lp5562_blink(int rgb, int on, int off)
{
	struct lp55xx_chip *chip = g_chip;
	struct lp5562_pattern_data ptn = { };
	u8 jump_pc = 0;

	lp5562_stop_engine(chip);

	on = min_t(unsigned int, on, MAX_BLINK_TIME);
	off = min_t(unsigned int, off, MAX_BLINK_TIME);

	if (!rgb || !on || !off)
		return;

	pr_info("%s\n", __func__);
	mutex_lock(&chip->lock);

	/* make on-time pattern */
	lp5562_set_pwm_cmd(&ptn, rgb);
	lp5562_set_wait_cmd(&ptn, on, jump_pc);
	jump_pc = ptn.pc_r / 2; /* 16bit size program counter */

	/* make off-time pattern */
	lp5562_set_pwm_cmd(&ptn, 0);
	lp5562_set_wait_cmd(&ptn, off, jump_pc);

	/* run the pattern */
	lp5562_run_led_pattern(chip, &ptn);

	printk(KERN_ERR "led_blink is called, color:0x%X brightness:%i\n",
			rgb, LED_DYNAMIC_CURRENT);

	mutex_unlock(&chip->lock);
}
EXPORT_SYMBOL(lp5562_blink);
static DEVICE_ATTR(led_blink, 0664,
		lp5562_show_blink, lp5562_store_blink);
/* Used in J, to enable led_blink */

#ifdef SEC_LED_SPECIFIC
static ssize_t store_led_r(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	pr_info("%s %d\n", __func__, brightness);
	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_R_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_R_PWM, brightness);
	mutex_unlock(&chip->lock);

out:
	return count;
}

static ssize_t store_led_g(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	pr_info("%s %d\n", __func__, brightness);
	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_G_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_G_PWM, brightness);
	mutex_unlock(&chip->lock);
out:
	return count;
}

static ssize_t store_led_b(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	char buff[10] = {0,};
	int cnt, ret;
	u8 brightness;

	cnt = count;
	cnt = (buf[cnt-1] == '\n') ? cnt-1 : cnt;
	memcpy(buff, buf, cnt);
	buff[cnt] = '\0';

	ret = kstrtou8(buff, 0, &brightness);
	if (ret != 0) {
		dev_err(&chip->cl->dev, "fail to get brightness.\n");
		goto out;
	}

	brightness = min((u8)40,brightness);
	brightness = brightness * 255 / 40;

	pr_info("%s %d\n", __func__, brightness);
	mutex_lock(&chip->lock);
	lp55xx_write(chip, LP5562_REG_B_CURRENT, 0x78);
	lp55xx_write(chip, LP5562_REG_B_PWM, brightness);
	mutex_unlock(&chip->lock);

out:
	return count;

}

static ssize_t lp5562_store_led_lowpower(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	u8 led_lowpower;
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;

	retval = kstrtou8(buf, 0, &led_lowpower);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_lowpower.\n");
		return count;
	}

	LED_LOWPOWER_MODE = led_lowpower;

	printk(KERN_DEBUG "led_lowpower mode set to %i\n", led_lowpower);

	return count;
}

static ssize_t lp5562_store_led_brightness(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	u8 brightness;
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;

	retval = kstrtou8(buf, 0, &brightness);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_brightness.\n");
		return count;
	}
/*
	if (brightness > LED_MAX_CURRENT)
		brightness = LED_MAX_CURRENT;
*/
	LED_DYNAMIC_CURRENT = brightness;

	printk(KERN_DEBUG "led brightness set to %i\n", brightness);

	return count;
}

static ssize_t lp5562_store_led_br_lev(struct device *dev,
					struct device_attribute *devattr,
					const char *buf, size_t count)
{
	int retval;
	unsigned long brightness_lev;
	/* struct i2c_client *client; */
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	/*client = b_client;*/

	pr_info("%s\n", __func__);
	retval = kstrtoul(buf, 16, &brightness_lev);
	if (retval != 0) {
		dev_err(&chip->cl->dev, "fail to get led_br_lev.\n");
		return count;
	}

	/*leds_set_imax(client, brightness_lev);*/

	return count;
}

#endif

#ifdef SEC_LED_SPECIFIC
/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(led_r, 0664, NULL, store_led_r);
static DEVICE_ATTR(led_g, 0664, NULL, store_led_g);
static DEVICE_ATTR(led_b, 0664, NULL, store_led_b);
/* led_pattern node permission is 664 */
/* To access sysfs node from other groups */
static DEVICE_ATTR(led_br_lev, 0664, NULL, \
					lp5562_store_led_br_lev);
static DEVICE_ATTR(led_brightness, 0664, NULL, \
					lp5562_store_led_brightness);
static DEVICE_ATTR(led_lowpower, 0664, NULL, \
					lp5562_store_led_lowpower);
#endif

static ssize_t lp5562_store_engine_mux(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	struct lp55xx_led *led = i2c_get_clientdata(to_i2c_client(dev));
	struct lp55xx_chip *chip = led->chip;
	u8 mask;
	u8 val;

	pr_info("%s\n", __func__);
	/* LED map
	 * R ... Engine 1 (fixed)
	 * G ... Engine 2 (fixed)
	 * B ... Engine 3 (fixed)
	 * W ... Engine 1 or 2 or 3
	 */

	if (sysfs_streq(buf, "RGB")) {
		mask = LP5562_ENG_FOR_RGB_M;
		val = LP5562_ENG_SEL_RGB;
	} else if (sysfs_streq(buf, "W")) {
		enum lp55xx_engine_index idx = chip->engine_idx;

		mask = LP5562_ENG_FOR_W_M;
		switch (idx) {
		case LP55XX_ENGINE_1:
			val = LP5562_ENG1_FOR_W;
			break;
		case LP55XX_ENGINE_2:
			val = LP5562_ENG2_FOR_W;
			break;
		case LP55XX_ENGINE_3:
			val = LP5562_ENG3_FOR_W;
			break;
		default:
			return -EINVAL;
		}

	} else {
		dev_err(dev, "choose RGB or W\n");
		return -EINVAL;
	}

	mutex_lock(&chip->lock);
	lp55xx_update_bits(chip, LP5562_REG_ENG_SEL, mask, val);
	mutex_unlock(&chip->lock);

	return len;
}

static DEVICE_ATTR(led_pattern, 0664,
		NULL, lp5562_store_pattern);
static LP55XX_DEV_ATTR_WO(engine_mux, lp5562_store_engine_mux);

static struct attribute *lp5562_attributes[] = {
#ifndef SEC_LED_SPECIFIC
	&dev_attr_engine_mux.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_blink.attr,
#endif
	NULL,
};

static const struct attribute_group lp5562_group = {
	.attrs = lp5562_attributes,
};

#ifdef SEC_LED_SPECIFIC
static struct attribute *sec_led_attributes[] = {
	&dev_attr_led_r.attr,
	&dev_attr_led_g.attr,
	&dev_attr_led_b.attr,
	&dev_attr_led_pattern.attr,
	&dev_attr_led_br_lev.attr,
	&dev_attr_led_brightness.attr,
	&dev_attr_led_lowpower.attr,
	&dev_attr_engine_mux.attr,
	&dev_attr_led_blink.attr,
	NULL,
};

static struct attribute_group sec_led_attr_group = {
	.attrs = sec_led_attributes,
};
#endif

/* Chip specific configurations */
static struct lp55xx_device_config lp5562_cfg = {
	.max_channel  = LP5562_MAX_LEDS,
	.reset = {
		.addr = LP5562_REG_RESET,
		.val  = LP5562_RESET,
	},
	.enable = {
		.addr = LP5562_REG_ENABLE,
		.val  = LP5562_ENABLE_DEFAULT,
	},
	.post_init_device   = lp5562_post_init_device,
	.set_led_current    = lp5562_set_led_current,
	.brightness_work_fn = lp5562_led_brightness_work,
	.run_engine         = lp5562_run_engine,
	.firmware_cb        = lp5562_firmware_loaded,
	.dev_attr_group     = &lp5562_group,
};
#ifdef CONFIG_OF
#ifdef SEC_LED_SPECIFIC
static int lp5562_fill_pattern_data(struct lp55xx_predef_pattern *p_data,
			struct device_node *np)
{
	int err, ret;
	int sz, nr;
	char const *type;
	u8 *ptr;

	err = of_property_read_u32(np, "num", &nr);
	ret = err;
	err = of_property_read_u32(np, "size", &sz);
	ret += err;
	err = of_property_read_string(np, "type", &type);
	ret += err;
	ptr = kzalloc(sz * sizeof(u8), GFP_KERNEL);
	if (!ptr) {
		pr_info("%s failed to alloc the array\n", __func__);
		return -1;
	}
	err = of_property_read_u8_array(np, "data", ptr, sz);
	ret += err;

	if (type[0] == 'r') {
		p_data[nr].r = ptr;
		p_data[nr].size_r = sz;
	} else if (type[0] == 'g') {
		p_data[nr].g = ptr;
		p_data[nr].size_g = sz;
	} else if (type[0] == 'b') {
		p_data[nr].b = ptr;
		p_data[nr].size_b = sz;
	} else {
		pr_info("%s failed to match the type\n", __func__);
		kfree(ptr);
		return -1;
	}

	return ret;
}

static int of_lp5562_parsing_dt(struct lp55xx_platform_data *pdata)
{
	struct device_node *np;
	struct device_node *c_np;
	struct lp55xx_predef_pattern *patterns;
	int ret = 0, err;
	int p_nr, p_arr_sz;

	pr_info("%s\n",__func__);

	np = of_find_node_by_path("/lp5562_pattern_data");
	if (!np) {
		pr_info("%s failed to get device node\n", __func__);
		return -EINVAL;
	}

	p_nr = of_get_child_count(np);

	err = of_property_read_u32(np, "pattern_arr_size", &p_arr_sz);
	ret += err;
	if (ret) {
		pr_info("%s failed to parse dt info %d\n", __func__, ret);
		return ret;
	}
	pdata->num_patterns = p_arr_sz;

	patterns = kzalloc(p_arr_sz * sizeof(struct lp55xx_predef_pattern),
			GFP_KERNEL);
	if (!patterns) {
		pr_info("%s failed to alloc the data\n", __func__);
		return -1;
	}

	for_each_child_of_node(np, c_np) {
		ret = lp5562_fill_pattern_data(patterns, c_np);
		if (ret) {
			pr_info("%s failed to parse dt info %d\n", __func__, ret);
			kfree(patterns);
			return ret;
		}
	}
	pdata->patterns = patterns;

	return ret;
}
#endif
#endif

static int lp5562_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	struct lp55xx_chip *chip;
	struct lp55xx_led *led;
	struct lp55xx_platform_data *pdata = NULL;
	struct device_node *np = client->dev.of_node;

	if (!dev_get_platdata(&client->dev)) {
		if (np) {
			ret = lp55xx_of_populate_pdata(&client->dev, np);
			if (ret < 0)
				return ret;
		} else {
			dev_err(&client->dev, "no platform data\n");
			return -EINVAL;
		}
	}
	pdata = dev_get_platdata(&client->dev);

#ifdef SEC_LED_SPECIFIC
	/* Get the pre-defined patterns for LED */
	ret = of_lp5562_parsing_dt(pdata);
	if (ret < 0)
		return ret;
#endif

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	led = devm_kzalloc(&client->dev,
			sizeof(*led) * pdata->num_channels, GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	chip->cl = client;
	chip->pdata = pdata;
	chip->cfg = &lp5562_cfg;

	mutex_init(&chip->lock);

	i2c_set_clientdata(client, led);

	ret = lp55xx_init_device(chip);
	if (ret)
		goto err_init;

	ret = lp55xx_register_leds(led, chip);
	if (ret)
		goto err_register_leds;

	ret = lp55xx_register_sysfs(chip);
	if (ret) {
		dev_err(&client->dev, "registering sysfs failed\n");
		goto err_register_sysfs;
	}

#ifdef SEC_LED_SPECIFIC
	led_dev = sec_device_create(led, "led");
	if (IS_ERR(led_dev)) {
		dev_err(&client->dev,
			"Failed to create device for samsung specific led\n");
		ret = -ENODEV;
		goto err_register_sysfs;
	}
	ret = sysfs_create_group(&led_dev->kobj, &sec_led_attr_group);
	if (ret) {
		dev_err(&client->dev,
			"Failed to create sysfs group for samsung specific led\n");
		goto err_register_sysfs;
	}
#endif

	g_chip = chip;

	pr_info("%s: done\n", __func__);

	return 0;

err_register_sysfs:
	lp55xx_unregister_leds(led, chip);
err_register_leds:
	lp55xx_deinit_device(chip);
err_init:
	return ret;
}

static int lp5562_remove(struct i2c_client *client)
{
	struct lp55xx_led *led = i2c_get_clientdata(client);
	struct lp55xx_chip *chip = led->chip;
	struct lp55xx_platform_data *pdata = chip->pdata;
	int i;

	lp5562_stop_engine(chip);

	lp55xx_unregister_sysfs(chip);
	lp55xx_unregister_leds(led, chip);
	lp55xx_deinit_device(chip);

	if (pdata) {
		for (i = 0 ; i < pdata->num_patterns ; i++) {
			if (pdata->patterns->r)
				kfree(pdata->patterns->r);
			else if (pdata->patterns->g)
				kfree(pdata->patterns->g);
			else if (pdata->patterns->b)
				kfree(pdata->patterns->b);
		}
		kfree(pdata->patterns);
	}

	return 0;
}

static const struct i2c_device_id lp5562_id[] = {
	{ "lp5562", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, lp5562_id);

#ifdef CONFIG_OF
static const struct of_device_id of_lp5562_leds_match[] = {
	{ .compatible = "ti,lp5562", },
	{},
};

MODULE_DEVICE_TABLE(of, of_lp5562_leds_match);
#endif

static struct i2c_driver lp5562_driver = {
	.driver = {
		.name	= "lp5562",
		.of_match_table = of_match_ptr(of_lp5562_leds_match),
	},
	.probe		= lp5562_probe,
	.remove		= lp5562_remove,
	.id_table	= lp5562_id,
};

module_i2c_driver(lp5562_driver);

MODULE_DESCRIPTION("Texas Instruments LP5562 LED Driver");
MODULE_AUTHOR("Milo Kim");
MODULE_LICENSE("GPL");
