/* drivers/staging/taos/tsl277x.c
 *
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * Author: Aleksej Makarov <aleksej.makarov@sonyericsson.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/i2c/tsl2772.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/irq.h>
#include <linux/regulator/consumer.h>

enum tsl277x_regs {
	TSL277X_ENABLE,
	TSL277X_ALS_TIME,
	TSL277X_PRX_TIME,
	TSL277X_WAIT_TIME,
	TSL277X_ALS_MINTHRESHLO,
	TSL277X_ALS_MINTHRESHHI,
	TSL277X_ALS_MAXTHRESHLO,
	TSL277X_ALS_MAXTHRESHHI,
	TSL277X_PRX_MINTHRESHLO,
	TSL277X_PRX_MINTHRESHHI,
	TSL277X_PRX_MAXTHRESHLO,
	TSL277X_PRX_MAXTHRESHHI,
	TSL277X_PERSISTENCE,
	TSL277X_CONFIG,
	TSL277X_PRX_PULSE_COUNT,
	TSL277X_CONTROL,

	TSL277X_REVID = 0x11,
	TSL277X_CHIPID,
	TSL277X_STATUS,
	TSL277X_ALS_CHAN0LO,
	TSL277X_ALS_CHAN0HI,
	TSL277X_ALS_CHAN1LO,
	TSL277X_ALS_CHAN1HI,
	TSL277X_PRX_LO,
	TSL277X_PRX_HI,

	TSL277X_REG_PRX_OFFS = 0x1e,
	TSL277X_REG_MAX,
};

enum tsl277x_cmd_reg {
	TSL277X_CMD_REG           = (1 << 7),
	TSL277X_CMD_INCR          = (0x1 << 5),
	TSL277X_CMD_SPL_FN        = (0x3 << 5),
	TSL277X_CMD_PROX_INT_CLR  = (0x5 << 0),
	TSL277X_CMD_ALS_INT_CLR   = (0x6 << 0),
};

enum tsl277x_en_reg {
	TSL277X_EN_PWR_ON   = (1 << 0),
	TSL277X_EN_ALS      = (1 << 1),
	TSL277X_EN_PRX      = (1 << 2),
	TSL277X_EN_WAIT     = (1 << 3),
	TSL277X_EN_ALS_IRQ  = (1 << 4),
	TSL277X_EN_PRX_IRQ  = (1 << 5),
	TSL277X_EN_SAI      = (1 << 6),
};

enum tsl277x_status {
	TSL277X_ST_ALS_VALID  = (1 << 0),
	TSL277X_ST_PRX_VALID  = (1 << 1),
	TSL277X_ST_ALS_IRQ    = (1 << 4),
	TSL277X_ST_PRX_IRQ    = (1 << 5),
	TSL277X_ST_PRX_SAT    = (1 << 6),
};

enum {
	TSL277X_ALS_GAIN_MASK = (3 << 0),
	TSL277X_ALS_AGL_MASK  = (1 << 2),
	TSL277X_ALS_AGL_SHIFT = 2,
	TSL277X_ATIME_PER_100 = 273,
	TSL277X_ATIME_DEFAULT_MS = 50,
	SCALE_SHIFT = 11,
	RATIO_SHIFT = 10,
	MAX_ALS_VALUE = 0xffff,
	MIN_ALS_VALUE = 10,
	GAIN_SWITCH_LEVEL = 100,
	GAIN_AUTO_INIT_VALUE = 16,
};

static u8 const tsl277x_ids[] = {
	0x39,
	0x30,
};

static char const *tsl277x_names[] = {
	"tsl27721 / tsl27725",
	"tsl27723 / tsl2777",
};

static u8 const restorable_regs[] = {
	TSL277X_ALS_TIME,
	TSL277X_PRX_TIME,
	TSL277X_WAIT_TIME,
	TSL277X_PERSISTENCE,
	TSL277X_CONFIG,
	TSL277X_PRX_PULSE_COUNT,
	TSL277X_CONTROL,
	TSL277X_REG_PRX_OFFS,
};

static u8 const als_gains[] = {
	1,
	8,
	16,
	120
};

struct taos_als_info {
	int ch0;
	int ch1;
	u32 cpl;
	u32 saturation;
	int lux;
};

struct taos_prox_info {
	int raw;
	int detected;
};

static struct lux_segment segment_default[] = {
	{
		.ratio = (435 << RATIO_SHIFT) / 1000,
		.k0 = (46516 << SCALE_SHIFT) / 1000,
		.k1 = (95381 << SCALE_SHIFT) / 1000,
	},
	{
		.ratio = (551 << RATIO_SHIFT) / 1000,
		.k0 = (23740 << SCALE_SHIFT) / 1000,
		.k1 = (43044 << SCALE_SHIFT) / 1000,
	},
};

struct tsl2772_chip {
	struct mutex lock;
	struct i2c_client *client;
	struct taos_prox_info prx_inf;
	struct taos_als_info als_inf;
	struct taos_parameters params;
	struct tsl2772_i2c_platform_data *pdata;
	u8 shadow[TSL277X_REG_MAX];
	struct input_dev *p_idev;
	struct input_dev *a_idev;
	int in_suspend;
	int wake_irq;
	int irq_pending;
	bool powered;
	bool als_status;
	bool prox_status;
	bool als_enabled;
	bool prox_enabled;
	struct lux_segment *segment;
	int segment_num;
	int seg_num_max;
	bool als_gain_auto;
	struct work_struct irq_work;
};

static int taos_i2c_read(struct tsl2772_chip *chip, u8 reg, u8 *val)
{
	int ret;
	s32 read;
	struct i2c_client *client = chip->client;

	ret = i2c_smbus_write_byte(client, (TSL277X_CMD_REG | reg));
	if (ret < 0) {
		dev_err(&client->dev, "%s: failed to write register %x\n",
				__func__, reg);
		return ret;
	}
	read = i2c_smbus_read_byte(client);
	if (read < 0) {
		dev_err(&client->dev, "%s: failed to read from register %x\n",
				__func__, reg);
		return ret;
	}
	*val = read;
	return 0;
}

static int taos_i2c_blk_read(struct tsl2772_chip *chip,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = chip->client;

	ret =  i2c_smbus_read_i2c_block_data(client,
			TSL277X_CMD_REG | TSL277X_CMD_INCR | reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}

static int taos_i2c_write(struct tsl2772_chip *chip, u8 reg, u8 val)
{
	int ret;
	struct i2c_client *client = chip->client;

	ret = i2c_smbus_write_byte_data(client, TSL277X_CMD_REG | reg, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed to write register %x\n",
				__func__, reg);
	return ret;
}

static int taos_i2c_blk_write(struct tsl2772_chip *chip,
		u8 reg, u8 *val, int size)
{
	s32 ret;
	struct i2c_client *client = chip->client;

	ret =  i2c_smbus_write_i2c_block_data(client,
			TSL277X_CMD_REG | TSL277X_CMD_INCR | reg, size, val);
	if (ret < 0)
		dev_err(&client->dev, "%s: failed at address %x (%d bytes)\n",
				__func__, reg, size);
	return ret;
}

static int set_segment_table(struct tsl2772_chip *chip,
		struct lux_segment *segment, int seg_num)
{
	int i;
	struct device *dev = &chip->client->dev;

	chip->seg_num_max = chip->pdata->segment_num ?
			chip->pdata->segment_num : ARRAY_SIZE(segment_default);

	if (!chip->segment) {
		dev_dbg(dev, "%s: allocating segment table\n", __func__);
		chip->segment = kzalloc(sizeof(*chip->segment) *
				chip->seg_num_max, GFP_KERNEL);
		if (!chip->segment) {
			dev_err(dev, "%s: no memory!\n", __func__);
			return -ENOMEM;
		}
	}
	if (seg_num > chip->seg_num_max) {
		dev_warn(dev, "%s: %d segment requested, %d applied\n",
				__func__, seg_num, chip->seg_num_max);
		chip->segment_num = chip->seg_num_max;
	} else {
		chip->segment_num = seg_num;
	}
	memcpy(chip->segment, segment,
			chip->segment_num * sizeof(*chip->segment));
	dev_dbg(dev, "%s: %d segment requested, %d applied\n", __func__,
			seg_num, chip->seg_num_max);
	for (i = 0; i < chip->segment_num; i++)
		dev_dbg(dev, "segment %d: ratio %6u, k0 %6u, k1 %6u\n",
				i, chip->segment[i].ratio,
				chip->segment[i].k0, chip->segment[i].k1);
	return 0;
}

static void taos_calc_cpl(struct tsl2772_chip *chip)
{
	u32 cpl;
	u32 sat;
	u8 atime = chip->shadow[TSL277X_ALS_TIME];
	u8 agl = (chip->shadow[TSL277X_CONFIG] & TSL277X_ALS_AGL_MASK)
			>> TSL277X_ALS_AGL_SHIFT;
	u32 time_scale = ((256 - atime) << SCALE_SHIFT) *
		TSL277X_ATIME_PER_100 / (TSL277X_ATIME_DEFAULT_MS * 100);

	cpl = time_scale * chip->params.als_gain;
	if (agl)
		cpl = cpl * 16 / 1000;
	sat = min_t(u32, MAX_ALS_VALUE, (u32)(256 - atime) << 10);
	sat = sat * 8 / 10;
	dev_dbg(&chip->client->dev,
	"%s: cpl = %u [time_scale %u, gain %u, agl %u], saturation %u\n",
	__func__, cpl, time_scale, chip->params.als_gain, agl, sat);
	chip->als_inf.cpl = cpl;
	chip->als_inf.saturation = sat;
}

static int set_als_gain(struct tsl2772_chip *chip, int gain)
{
	int rc;
	u8 ctrl_reg  = chip->shadow[TSL277X_CONTROL] & ~TSL277X_ALS_GAIN_MASK;

	switch (gain) {
	case 1:
		ctrl_reg |= AGAIN_1;
		break;
	case 8:
		ctrl_reg |= AGAIN_8;
		break;
	case 16:
		ctrl_reg |= AGAIN_16;
		break;
	case 120:
		ctrl_reg |= AGAIN_120;
		break;
	default:
		dev_err(&chip->client->dev, "%s: wrong als gain %d\n",
				__func__, gain);
		return -EINVAL;
	}
	rc = taos_i2c_write(chip, TSL277X_CONTROL, ctrl_reg);
	if (!rc) {
		chip->shadow[TSL277X_CONTROL] = ctrl_reg;
		chip->params.als_gain = gain;
		dev_dbg(&chip->client->dev, "%s: new gain %d\n",
				__func__, gain);
	}
	return rc;
}

static int taos_get_lux(struct tsl2772_chip *chip)
{
	unsigned i;
	int ret = 0;
	struct device *dev = &chip->client->dev;
	struct lux_segment *s = chip->segment;
	u32 c0 = chip->als_inf.ch0;
	u32 c1 = chip->als_inf.ch1;
	u32 sat = chip->als_inf.saturation;
	u32 ratio;
	u64 lux_0, lux_1;
	u32 cpl = chip->als_inf.cpl;
	u32 lux, k0 = 0, k1 = 0;

	if (!chip->als_gain_auto) {
		if (c0 <= MIN_ALS_VALUE) {
			dev_dbg(dev, "%s: darkness\n", __func__);
			lux = 0;
			goto exit;
		} else if (c0 >= sat) {
			dev_dbg(dev, "%s: saturation, keep lux\n", __func__);
			lux = chip->als_inf.lux;
			goto exit;
		}
	} else {
		u8 gain = chip->params.als_gain;
		int rc = -EIO;

		if (gain == 16 && c0 >= sat) {
			rc = set_als_gain(chip, 1);
		} else if (gain == 16 && c0 < GAIN_SWITCH_LEVEL) {
			rc = set_als_gain(chip, 120);
		} else if ((gain == 120 && c0 >= sat) ||
				(gain == 1 && c0 < GAIN_SWITCH_LEVEL)) {
			rc = set_als_gain(chip, 16);
		}
		if (!rc) {
			dev_dbg(dev, "%s: gain adjusted, skip\n", __func__);
			taos_calc_cpl(chip);
			ret = -EAGAIN;
			lux = chip->als_inf.lux;
			goto exit;
		}

		if (c0 <= MIN_ALS_VALUE) {
			dev_dbg(dev, "%s: darkness\n", __func__);
			lux = 0;
			goto exit;
		} else if (c0 >= sat) {
			dev_dbg(dev, "%s: saturation, keep lux\n", __func__);
			lux = chip->als_inf.lux;
			goto exit;
		}
	}

	ratio = (c1 << RATIO_SHIFT) / c0;
	for (i = 0; i < chip->segment_num; i++, s++) {
		if (ratio <= s->ratio) {
			dev_dbg(&chip->client->dev,
			"%s: ratio %u segment %u [r %u, k0 %u, k1 %u]\n",
			__func__, ratio, i, s->ratio, s->k0, s->k1);
			k0 = s->k0;
			k1 = s->k1;
			break;
		}
	}
	if (i >= chip->segment_num) {
		dev_dbg(&chip->client->dev, "%s: ratio %u - darkness\n",
				__func__, ratio);
		lux = 0;
		goto exit;
	}
	lux_0 = k0 * (s64)c0;
	lux_1 = k1 * (s64)c1;
	if (lux_1 >= lux_0) {
		dev_dbg(&chip->client->dev, "%s: negative result - darkness\n",
				__func__);
		lux = 0;
		goto exit;
	}
	lux_0 -= lux_1;
	while (lux_0 & ((u64)0xffffffff << 32)) {
		dev_dbg(&chip->client->dev, "%s: overwlow lux64 = 0x%16llx",
				__func__, lux_0);
		lux_0 >>= 1;
		cpl >>= 1;
	}
	if (!cpl) {
		dev_dbg(&chip->client->dev, "%s: zero cpl - darkness\n",
				__func__);
		lux = 0;
		goto exit;
	}
	lux = lux_0;
	lux = lux / cpl;
exit:
	dev_dbg(&chip->client->dev, "%s: lux %u (%u x %u - %u x %u) / %u\n",
		__func__, lux, k0, c0, k1, c1, cpl);
	chip->als_inf.lux = lux;
	return ret;
}

static int pltf_power_on(struct tsl2772_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_ON);
		usleep_range(10000, 10005);
	}
	if (rc == 0)
		chip->powered = true;

	return rc;
}

static int pltf_power_off(struct tsl2772_chip *chip)
{
	int rc = 0;
	if (chip->pdata->platform_power) {
		rc = chip->pdata->platform_power(&chip->client->dev,
			POWER_OFF);
		if (rc == 0)
			chip->powered = false;
	} else {
		chip->powered = true;
	}
	return rc;
}

static int taos_irq_clr(struct tsl2772_chip *chip, u8 bits)
{
	int ret = i2c_smbus_write_byte(chip->client, TSL277X_CMD_REG |
			TSL277X_CMD_SPL_FN | bits);
	if (ret < 0)
		dev_err(&chip->client->dev, "%s: failed, bits %x\n",
				__func__, bits);
	return ret;
}

static void taos_get_als(struct tsl2772_chip *chip)
{
	u32 ch0, ch1;
	u8 *buf = &chip->shadow[TSL277X_ALS_CHAN0LO];

	ch0 = le16_to_cpup((const __le16 *)&buf[0]);
	ch1 = le16_to_cpup((const __le16 *)&buf[2]);
	chip->als_inf.ch0 = ch0;
	chip->als_inf.ch1 = ch1;
	dev_dbg(&chip->client->dev, "%s: ch0 %u, ch1 %u\n",
			__func__, ch0, ch1);
}

static void taos_get_prox(struct tsl2772_chip *chip)
{
	u8 *buf = &chip->shadow[TSL277X_PRX_LO];
	bool d = chip->prx_inf.detected;

	chip->prx_inf.raw = (buf[1] << 8) | buf[0];
	chip->prx_inf.detected =
		(d && (chip->prx_inf.raw > chip->params.prox_th_min)) ||
		(!d && (chip->prx_inf.raw > chip->params.prox_th_max));
	dev_dbg(&chip->client->dev, "%s: raw %d, detected %d\n", __func__,
			chip->prx_inf.raw, chip->prx_inf.detected);
}

static int taos_read_all(struct tsl2772_chip *chip)
{
	struct i2c_client *client = chip->client;
	s32 ret;

	dev_dbg(&client->dev, "%s\n", __func__);
	ret = taos_i2c_blk_read(chip, TSL277X_STATUS,
			&chip->shadow[TSL277X_STATUS],
			TSL277X_PRX_HI - TSL277X_STATUS + 1);
	return (ret < 0) ? ret : 0;
}

static int update_prox_thresh(struct tsl2772_chip *chip, bool on_enable)
{
	s32 ret;
	u8 *buf = &chip->shadow[TSL277X_PRX_MINTHRESHLO];
	u16 from, to;

	if (on_enable) {
		/* zero gate to force irq */
		from = to = 0;
	} else {
		if (chip->prx_inf.detected) {
			from = chip->params.prox_th_min;
			to = 0xffff;
		} else {
			from = 0;
			to = chip->params.prox_th_max;
		}
	}
	dev_dbg(&chip->client->dev, "%s: %u - %u\n", __func__, from, to);
	*buf++ = from & 0xff;
	*buf++ = from >> 8;
	*buf++ = to & 0xff;
	*buf++ = to >> 8;
	ret = taos_i2c_blk_write(chip, TSL277X_PRX_MINTHRESHLO,
			&chip->shadow[TSL277X_PRX_MINTHRESHLO],
			TSL277X_PRX_MAXTHRESHHI - TSL277X_PRX_MINTHRESHLO + 1);
	return (ret < 0) ? ret : 0;
}

static int update_als_thres(struct tsl2772_chip *chip, bool on_enable)
{
	s32 ret;
	u8 *buf = &chip->shadow[TSL277X_ALS_MINTHRESHLO];
	u16 gate = chip->params.als_gate;
	u16 from, to, cur;

	cur = chip->als_inf.ch0;
	if (on_enable) {
		/* zero gate far away form current position to force an irq */
		from = to = cur > 0xffff / 2 ? 0 : 0xffff;
	} else {
		gate = cur * gate / 100;
		if (!gate)
			gate = 1;
		if (cur > gate)
			from = cur - gate;
		else
			from = 0;

		if (cur < (0xffff - gate))
			to = cur + gate;
		else
			to = 0xffff;
	}
	dev_dbg(&chip->client->dev, "%s: [%u - %u]\n", __func__, from, to);
	*buf++ = from & 0xff;
	*buf++ = from >> 8;
	*buf++ = to & 0xff;
	*buf++ = to >> 8;
	ret = taos_i2c_blk_write(chip, TSL277X_ALS_MINTHRESHLO,
			&chip->shadow[TSL277X_ALS_MINTHRESHLO],
			TSL277X_ALS_MAXTHRESHHI - TSL277X_ALS_MINTHRESHLO + 1);
	return (ret < 0) ? ret : 0;
}

static void report_prox(struct tsl2772_chip *chip)
{
	if (chip->p_idev) {
		input_report_abs(chip->p_idev, ABS_DISTANCE,
				chip->prx_inf.detected ? 0 : 1);
		input_sync(chip->p_idev);
	}
}

static void report_als(struct tsl2772_chip *chip)
{
	if (chip->a_idev) {
		int rc = taos_get_lux(chip);
		if (!rc) {
			int lux = chip->als_inf.lux;
			input_report_abs(chip->a_idev, ABS_MISC, lux);
			input_sync(chip->a_idev);
			update_als_thres(chip, 0);
		} else {
			update_als_thres(chip, 1);
		}
	}
}

static int taos_check_and_report(struct tsl2772_chip *chip)
{
	u8 status;

	int ret = taos_read_all(chip);
	if (ret)
		goto exit_clr;

	status = chip->shadow[TSL277X_STATUS];
	dev_dbg(&chip->client->dev, "%s: status 0x%02x\n", __func__, status);

	if ((status & (TSL277X_ST_PRX_VALID | TSL277X_ST_PRX_IRQ)) ==
			(TSL277X_ST_PRX_VALID | TSL277X_ST_PRX_IRQ)) {
		taos_get_prox(chip);
		report_prox(chip);
		update_prox_thresh(chip, 0);
	}

	if ((status & (TSL277X_ST_ALS_VALID | TSL277X_ST_ALS_IRQ)) ==
			(TSL277X_ST_ALS_VALID | TSL277X_ST_ALS_IRQ)) {
		taos_get_als(chip);
		report_als(chip);
	}
exit_clr:
	taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR | TSL277X_CMD_ALS_INT_CLR);
	return ret;
}

static void irq_work_func(struct work_struct *work)
{
	struct tsl2772_chip *chip = container_of((struct work_struct *)work,
			struct tsl2772_chip, irq_work);
	dev_dbg(&chip->client->dev, "%s: irq worker!\n", __func__);
	mutex_lock(&chip->lock);
	if (chip->in_suspend) {
		dev_dbg(&chip->client->dev, "%s: in suspend\n", __func__);
		chip->irq_pending = 1;
		disable_irq_nosync(chip->client->irq);
		goto bypass;
	}
	(void)taos_check_and_report(chip);
bypass:
	mutex_unlock(&chip->lock);
}

static irqreturn_t taos_irq(int irq, void *handle)
{
	struct tsl2772_chip *chip = handle;
	struct device *dev = &chip->client->dev;

	dev_dbg(dev, "%s: handle irq!\n", __func__);

	dev_dbg(dev, "%s\n", __func__);
	schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF

int simulate_of_property_read_u8(const struct device_node *np,
		const char *propname, u8 *out_value)
{
	int nRet;
	u32 code;
	u8 value;

	nRet = of_property_read_u32(np, propname, &code);
	if (nRet == 0) {
		value = code & 0xFF;
		*out_value = value;
		nRet = 0;
	} else
		nRet = -ENODATA;

	return nRet;
}

int simulate_of_property_read_u16(const struct device_node *np,
		const char *propname, u16 *out_value)
{
	int nRet;
	u32 code;
	u16 value;

	nRet = of_property_read_u32(np, propname, &code);
	if (nRet == 0) {
		value = code & 0xFFFF;
		*out_value = value;
		nRet = 0;
	} else
		nRet = -ENODATA;

	return nRet;
}

static void tsl2772_parse_dt(struct tsl2772_chip *chip)
{
	struct device_node *dNode = chip->client->dev.of_node;
	u8 *sh = chip->shadow;
	struct device *dev = &chip->client->dev;
	int ret;

	if (dNode == NULL)
		dev_err(dev, "%s: failed to find the of data!\n", __func__);

	dev_dbg(dev, "%s: begin parse of data!\n", __func__);

	chip->pdata->prox_name = kzalloc((256*sizeof(char)), GFP_KERNEL);
	if (chip->pdata->prox_name != NULL) {
		ret = of_property_read_string(dNode, "prox_name",
				&chip->pdata->prox_name);
		if (ret)
			dev_err(dev, "fail to get prox_name\n");
		else
			dev_info(dev, "prox_name:%s\n",
					chip->pdata->prox_name);
	}

	chip->pdata->als_name = kzalloc((256*sizeof(char)), GFP_KERNEL);
	if (chip->pdata->als_name != NULL) {
		ret = of_property_read_string(dNode, "als_name",
				&chip->pdata->als_name);
		if (ret)
			dev_err(dev, "fail to get prox_name\n");
		else
			dev_info(dev, "als_name:%s\n", chip->pdata->als_name);
	}

	ret = simulate_of_property_read_u8(dNode, "als_time",
			&sh[TSL277X_ALS_TIME]);
	if (ret)
		dev_err(dev, "fail to get als_time\n");
	else
		dev_info(dev, "als_time:%d\n", (u8)sh[TSL277X_ALS_TIME]);

	ret = simulate_of_property_read_u8(dNode, "prx_time",
			&sh[TSL277X_PRX_TIME]);
	if (ret)
		dev_err(dev, "fail to get prx_time\n");
	else
		dev_info(dev, "prx_time:%d\n", (u8)sh[TSL277X_PRX_TIME]);

	ret = simulate_of_property_read_u8(dNode, "wait_time",
			&sh[TSL277X_WAIT_TIME]);
	if (ret)
		dev_err(dev, "fail to get wait_time\n");
	else
		dev_info(dev, "wait_time:%d\n", (u8)sh[TSL277X_WAIT_TIME]);

	ret = simulate_of_property_read_u8(dNode, "persist",
			&sh[TSL277X_PERSISTENCE]);
	if (ret)
		dev_err(dev, "fail to get persist\n");
	else {
		dev_info(dev, "persist:0x%x\n", (u8)sh[TSL277X_PERSISTENCE]);
		dev_info(dev, "default persist:0x%x\n",
				PRX_PERSIST(1) | ALS_PERSIST(3));
	}

	ret = simulate_of_property_read_u8(dNode, "cfg_reg",
			&sh[TSL277X_CONFIG]);
	if (ret)
		dev_err(dev, "fail to get cfg_reg\n");
	else
		dev_info(dev, "cfg_reg:0x%x\n", (u8)sh[TSL277X_CONFIG]);

	ret = simulate_of_property_read_u8(dNode, "prox_pulse_cnt",
			&sh[TSL277X_PRX_PULSE_COUNT]);
	if (ret)
		dev_err(dev, "fail to get prox_pulse_cnt\n");
	else
		dev_info(dev, "prox_pulse_cnt:%d\n",
				(u8)sh[TSL277X_PRX_PULSE_COUNT]);

	ret = simulate_of_property_read_u8(dNode, "ctrl_reg",
			&sh[TSL277X_CONTROL]);
	if (ret)
		dev_err(dev, "fail to get ctrl_reg\n");
	else {
		dev_info(dev, "ctrl_reg:0x%x\n", (u8)sh[TSL277X_CONTROL]);
		dev_info(dev, "default ctrl reg:0x%x\n",
			(AGAIN_8 | PGAIN_4 | PDIOD_CH0 | PDRIVE_120MA));
	}

	ret = simulate_of_property_read_u8(dNode, "prox_offs",
			&sh[TSL277X_REG_PRX_OFFS]);
	if (ret)
		dev_err(dev, "fail to get prox_offs\n");
	else
		dev_info(dev, "prox_offs:%d\n", (u8)sh[TSL277X_REG_PRX_OFFS]);

	ret = simulate_of_property_read_u16(dNode, "parameters,als_gate",
			(u16 *)&chip->pdata->parameters.als_gate);
	if (ret)
		dev_err(dev, "fail to get parameters.als_gate\n");
	else
		dev_info(dev, "parameters,als_gate:%d\n",
				(u16)chip->pdata->parameters.als_gate);

	ret = simulate_of_property_read_u16(dNode, "parameters,prox_th_max",
			(u16 *)&chip->pdata->parameters.prox_th_max);
	if (ret)
		dev_err(dev, "fail to get parameters.prox_th_max\n");
	else
		dev_info(dev, "parameters,prox_th_max:%d\n",
				(u16)chip->pdata->parameters.prox_th_max);

	ret = simulate_of_property_read_u16(dNode, "parameters,prox_th_min",
			(u16 *)&chip->pdata->parameters.prox_th_min);
	if (ret)
		dev_err(dev, "fail to get parameters.prox_th_min\n");
	else
		dev_info(dev, "parameters,prox_th_min:%d\n",
				(u16)chip->pdata->parameters.prox_th_min);

	ret = simulate_of_property_read_u16(dNode, "parameters,als_gain",
			(u16 *)&chip->pdata->parameters.als_gain);
	if (ret) {
		dev_err(dev, "fail to get parameters.als_gain\n");
		chip->pdata->parameters.als_gain = 0;
	} else
		dev_info(dev, "parameters,als_gain:%d\n",
				(u16)chip->pdata->parameters.als_gain);

	chip->pdata->als_can_wake = of_property_read_bool(dNode,
			"als_can_wake");
	dev_info(dev, "als_can_wake:%s\n",
			(chip->pdata->als_can_wake ? "true" : "false"));
	chip->pdata->proximity_can_wake = of_property_read_bool(dNode,
			"proximity_can_wake");
	dev_info(dev, "proximity_can_wake:%s\n",
			(chip->pdata->proximity_can_wake ? "true" : "false"));
}
#endif

static void set_pltf_settings(struct tsl2772_chip *chip)
{
	struct device *dev = &chip->client->dev;

#ifndef CONFIG_OF
	struct taos_raw_settings const *s = chip->pdata->raw_settings;
	u8 *sh = chip->shadow;
	if (s) {
		dev_dbg(dev, "%s: form pltf data\n", __func__);
		sh[TSL277X_ALS_TIME] = s->als_time;
		sh[TSL277X_PRX_TIME] = s->prx_time;
		sh[TSL277X_WAIT_TIME] = s->wait_time;
		sh[TSL277X_PERSISTENCE] = s->persist;
		sh[TSL277X_CONFIG] = s->cfg_reg;
		sh[TSL277X_PRX_PULSE_COUNT] = s->prox_pulse_cnt;
		sh[TSL277X_CONTROL] = s->ctrl_reg;
		sh[TSL277X_REG_PRX_OFFS] = s->prox_offs;
	} else {
		dev_dbg(dev, "%s: use defaults\n", __func__);
		sh[TSL277X_ALS_TIME] = 238; /* ~50 ms */
		sh[TSL277X_PRX_TIME] = 255;
		sh[TSL277X_WAIT_TIME] = 0;
		sh[TSL277X_PERSISTENCE] = PRX_PERSIST(1) | ALS_PERSIST(3);
		sh[TSL277X_CONFIG] = 0;
		sh[TSL277X_PRX_PULSE_COUNT] = 10;
		sh[TSL277X_CONTROL] = AGAIN_8 | PGAIN_4 |
				PDIOD_CH0 | PDRIVE_120MA;
		sh[TSL277X_REG_PRX_OFFS] = 0;
	}
#endif

	chip->params.als_gate = chip->pdata->parameters.als_gate;
	chip->params.prox_th_max = chip->pdata->parameters.prox_th_max;
	chip->params.prox_th_min = chip->pdata->parameters.prox_th_min;
	chip->params.als_gain = chip->pdata->parameters.als_gain;
	if (chip->pdata->parameters.als_gain) {
		chip->params.als_gain = chip->pdata->parameters.als_gain;
	} else {
		chip->als_gain_auto = true;
		chip->params.als_gain = GAIN_AUTO_INIT_VALUE;
		dev_dbg(dev, "%s: auto als gain.\n", __func__);
	}

	(void)set_als_gain(chip, chip->params.als_gain);
	taos_calc_cpl(chip);
}


static int flush_regs(struct tsl2772_chip *chip)
{
	unsigned i;
	int rc;
	u8 reg;

	dev_dbg(&chip->client->dev, "%s\n", __func__);
	for (i = 0; i < ARRAY_SIZE(restorable_regs); i++) {
		reg = restorable_regs[i];
		rc = taos_i2c_write(chip, reg, chip->shadow[reg]);
		if (rc) {
			dev_err(&chip->client->dev, "%s: err on reg 0x%02x\n",
					__func__, reg);
			break;
		}
	}
	return rc;
}

static int update_enable_reg(struct tsl2772_chip *chip)
{
	dev_dbg(&chip->client->dev, "%s: %02x\n", __func__,
			chip->shadow[TSL277X_ENABLE]);
	return taos_i2c_write(chip, TSL277X_ENABLE,
			chip->shadow[TSL277X_ENABLE]);
}

static int taos_prox_enable(struct tsl2772_chip *chip, int on)
{
	int rc;

	dev_dbg(&chip->client->dev, "%s: on = %d\n", __func__, on);
	if (on) {
		taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR);
		update_prox_thresh(chip, 1);
		chip->shadow[TSL277X_ENABLE] |=
				(TSL277X_EN_PWR_ON | TSL277X_EN_PRX |
				TSL277X_EN_PRX_IRQ);
		rc = update_enable_reg(chip);
		if (rc)
			return rc;
		usleep_range(3000, 3005);
	} else {
		chip->shadow[TSL277X_ENABLE] &=
				~(TSL277X_EN_PRX_IRQ | TSL277X_EN_PRX);
		if (!(chip->shadow[TSL277X_ENABLE] & TSL277X_EN_ALS))
			chip->shadow[TSL277X_ENABLE] &= ~TSL277X_EN_PWR_ON;
		rc = update_enable_reg(chip);
		if (rc)
			return rc;
		taos_irq_clr(chip, TSL277X_CMD_PROX_INT_CLR);
	}
	if (!rc)
		chip->prox_status = on;
	return rc;
}

static int taos_als_enable(struct tsl2772_chip *chip, int on)
{
	int rc;

	dev_dbg(&chip->client->dev, "%s: on = %d\n", __func__, on);
	if (on) {
		taos_irq_clr(chip, TSL277X_CMD_ALS_INT_CLR);
		update_als_thres(chip, 1);
		chip->shadow[TSL277X_ENABLE] |=
				(TSL277X_EN_PWR_ON | TSL277X_EN_ALS |
				TSL277X_EN_ALS_IRQ);
		rc = update_enable_reg(chip);
		if (rc)
			return rc;
		usleep_range(3000, 3005);
	} else {
		chip->shadow[TSL277X_ENABLE] &=
				~(TSL277X_EN_ALS_IRQ | TSL277X_EN_ALS);
		if (!(chip->shadow[TSL277X_ENABLE] & TSL277X_EN_PRX))
			chip->shadow[TSL277X_ENABLE] &= ~TSL277X_EN_PWR_ON;
		rc = update_enable_reg(chip);
		if (rc)
			return rc;
		taos_irq_clr(chip, TSL277X_CMD_ALS_INT_CLR);
	}
	if (!rc)
		chip->als_status = on;
	return rc;
}

static ssize_t taos_device_als_ch0(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ch0);
}

static ssize_t taos_device_als_ch1(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.ch1);
}

static ssize_t taos_device_als_cpl(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.cpl);
}

static ssize_t taos_device_als_lux(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_inf.lux);
}

static ssize_t taos_lux_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	struct lux_segment *s = chip->segment;
	int i, k;

	for (i = k = 0; i < chip->segment_num; i++)
		k += snprintf(buf + k, PAGE_SIZE - k, "%d:%u,%u,%u\n", i,
				(s[i].ratio * 1000) >> RATIO_SHIFT,
				(s[i].k0 * 1000) >> SCALE_SHIFT,
				(s[i].k1 * 1000) >> SCALE_SHIFT);
	return k;
}

static ssize_t taos_lux_table_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	int i;
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	u32 ratio, k0, k1;

	if (4 != sscanf(buf, "%10d:%10u,%10u,%10u", &i, &ratio, &k0, &k1))
		return -EINVAL;
	if (i >= chip->segment_num)
		return -EINVAL;
	mutex_lock(&chip->lock);
	chip->segment[i].ratio = (ratio << RATIO_SHIFT) / 1000;
	chip->segment[i].k0 = (k0 << SCALE_SHIFT) / 1000;
	chip->segment[i].k1 = (k1 << SCALE_SHIFT) / 1000;
	mutex_unlock(&chip->lock);
	return size;
}

static ssize_t tsl2772_als_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t tsl2772_als_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value) {
		taos_als_enable(chip, 1);
		dev_dbg(&chip->client->dev, "%s: on!\n", __func__);
	} else {
		taos_als_enable(chip, 0);
		dev_dbg(&chip->client->dev, "%s: off!\n", __func__);
	}

	chip->als_enabled = value;

	return size;
}

static ssize_t tsl2772_prox_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prox_enabled);
}

static ssize_t tsl2772_prox_enable(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool value;

	if (strtobool(buf, &value))
		return -EINVAL;

	if (value) {
		dev_dbg(&chip->client->dev, "%s: on!\n", __func__);
		taos_prox_enable(chip, 1);
	} else {
		dev_dbg(&chip->client->dev, "%s: off!\n", __func__);
		taos_prox_enable(chip, 0);
	}

	chip->prox_enabled = value;

	return size;
}

static ssize_t taos_als_gain_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d (%s)\n", chip->params.als_gain,
			chip->als_gain_auto ? "auto" : "manual");
}

static ssize_t taos_als_gain_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long gain;
	int rc;
	struct tsl2772_chip *chip = dev_get_drvdata(dev);

	rc = strict_strtoul(buf, 10, &gain);
	if (rc)
		return -EINVAL;
	if (gain != 0 && gain != 1 && gain != 8 && gain != 16 && gain != 120)
		return -EINVAL;
	mutex_lock(&chip->lock);
	if (gain) {
		chip->als_gain_auto = false;
		rc = set_als_gain(chip, gain);
		if (!rc)
			taos_calc_cpl(chip);
	} else {
		chip->als_gain_auto = true;
	}
	mutex_unlock(&chip->lock);
	return rc ? rc : size;
}

static ssize_t taos_als_gate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d (in %%)\n", chip->params.als_gate);
}

static ssize_t taos_als_gate_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	unsigned long gate;
	int rc;
	struct tsl2772_chip *chip = dev_get_drvdata(dev);

	rc = strict_strtoul(buf, 10, &gate);
	if (rc || gate > 100)
		return -EINVAL;
	mutex_lock(&chip->lock);
	chip->params.als_gate = gate;
	mutex_unlock(&chip->lock);
	return size;
}

static ssize_t taos_device_prx_raw(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prx_inf.raw);
}

static ssize_t taos_device_prx_detected(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", chip->prx_inf.detected);
}

static struct device_attribute prox_attrs[] = {
	__ATTR(prx_raw, 0444, taos_device_prx_raw, NULL),
	__ATTR(prx_detect, 0444, taos_device_prx_detected, NULL),
	__ATTR(prox_power_state, 0666, tsl2772_prox_enable_show,
			tsl2772_prox_enable),
};

static struct device_attribute als_attrs[] = {
	__ATTR(als_ch0, 0444, taos_device_als_ch0, NULL),
	__ATTR(als_ch1, 0444, taos_device_als_ch1, NULL),
	__ATTR(als_cpl, 0444, taos_device_als_cpl, NULL),
	__ATTR(als_lux, 0444, taos_device_als_lux, NULL),
	__ATTR(als_gain, 0644, taos_als_gain_show, taos_als_gain_store),
	__ATTR(als_gate, 0644, taos_als_gate_show, taos_als_gate_store),
	__ATTR(lux_table, 0644, taos_lux_table_show, taos_lux_table_store),
	__ATTR(als_power_state, 0666, tsl2772_als_enable_show,
			tsl2772_als_enable),
};

static int add_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		if (device_create_file(dev, a + i))
			goto undo;
	return 0;
undo:
	for (; i >= 0 ; i--)
		device_remove_file(dev, a + i);
	dev_err(dev, "%s: failed to create sysfs interface\n", __func__);
	return -ENODEV;
}

static void remove_sysfs_interfaces(struct device *dev,
	struct device_attribute *a, int size)
{
	int i;
	for (i = 0; i < size; i++)
		device_remove_file(dev, a + i);
}

static int taos_get_id(struct tsl2772_chip *chip, u8 *id, u8 *rev)
{
	int rc = taos_i2c_read(chip, TSL277X_REVID, rev);
	if (rc)
		return rc;
	return taos_i2c_read(chip, TSL277X_CHIPID, id);
}

static int power_on(struct tsl2772_chip *chip)
{
	int rc;
	rc = pltf_power_on(chip);
	if (rc)
		return rc;
	dev_dbg(&chip->client->dev, "%s: chip was off, restoring regs\n",
			__func__);
	return flush_regs(chip);
}

static int prox_idev_open(struct input_dev *idev)
{
	int rc = 0;

	dev_dbg(&idev->dev, "%s\n", __func__);
#if 0
	mutex_lock(&chip->lock);
	if (!chip->powered) {
		rc = power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	rc = taos_prox_enable(chip, 1);
	if (rc && !als)
		pltf_power_off(chip);
chip_on_err:
	mutex_unlock(&chip->lock);
#endif

	return rc;
}

static void prox_idev_close(struct input_dev *idev)
{
	dev_dbg(&idev->dev, "%s\n", __func__);
#if 0
	mutex_lock(&chip->lock);
	taos_prox_enable(chip, 0);
	if (!chip->a_idev || !chip->a_idev->users)
		pltf_power_off(chip);
	mutex_unlock(&chip->lock);
#endif
}

static int als_idev_open(struct input_dev *idev)
{
	int rc = 0;

	dev_dbg(&idev->dev, "%s\n", __func__);
#if 0
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);
	mutex_lock(&chip->lock);
	if (!chip->powered) {
		rc = power_on(chip);
		if (rc)
			goto chip_on_err;
	}
	rc = taos_als_enable(chip, 1);
	if (rc && !prox)
		pltf_power_off(chip);
chip_on_err:
	mutex_unlock(&chip->lock);
#endif
	return rc;
}

static void als_idev_close(struct input_dev *idev)
{
	dev_dbg(&idev->dev, "%s\n", __func__);
#if 0
	struct tsl2772_chip *chip = dev_get_drvdata(&idev->dev);
	mutex_lock(&chip->lock);
	taos_als_enable(chip, 0);
	if (!chip->p_idev || !chip->p_idev->users)
		pltf_power_off(chip);
	mutex_unlock(&chip->lock);
#endif
}

#if CONFIG_OF
int sensor_power_init(struct device *dev)
{
	static struct regulator *v_sensor;
	int ret = 0;

	if (!v_sensor) {
		v_sensor = regulator_get(dev, "vdd");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			return -EIO;
		}
	}

	regulator_set_voltage(v_sensor, 2800000, 2800000);
	ret = regulator_enable(v_sensor);
	if (ret < 0)
		return -1;

	msleep(20);
	return 0;
}

static int board_tsl2772_power(struct device *dev, enum tsl2772_pwr_state state)
{
	static struct regulator *v_sensor;
	static enum tsl2772_pwr_state flags = POWER_OFF;
	int ret = 0;

	if (!v_sensor) {
		v_sensor = regulator_get(dev, "vdd");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			return -EIO;
		}
		regulator_set_voltage(v_sensor, 2800000, 2800000);
	}

	if ((flags == POWER_OFF) && (state == POWER_ON)) {
		ret = regulator_enable(v_sensor);
		if (ret < 0)
			return -1;
		flags = POWER_ON;
	} else if ((flags == POWER_ON) && (state == POWER_OFF)) {
		regulator_disable(v_sensor);
		flags = POWER_OFF;
	} else {
		dev_dbg(dev, "%s: unexpected state %d\n", __func__, state);
	}

	msleep(20);

	/* modified to brute force capture of gpio
	 * - when stolen by other driver
	 */
	dev_err(dev, "%s: state %d\n", __func__, state);

	return 0;
}

void board_tsl2772_teardown(struct device *dev)
{
	static struct regulator *v_sensor;

	pr_info("board_tsl2772_teardow CALLED\n");
	if (!v_sensor) {
		v_sensor = regulator_get(dev, "vdd");
		if (IS_ERR(v_sensor)) {
			v_sensor = NULL;
			return;
		}
	}
	regulator_disable(v_sensor);
}
#endif

static int taos_probe(struct i2c_client *client,
	const struct i2c_device_id *idp)
{
	int i, ret;
	u8 id, rev;
	struct device *dev = &client->dev;
	static struct tsl2772_chip *chip;
	struct tsl2772_i2c_platform_data *pdata = dev->platform_data;
	bool powered = 0;

	dev_info(dev, "%s: client->irq = %d\n", __func__, client->irq);
	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c smbus byte data unsupported\n",
				__func__);
		ret = -EOPNOTSUPP;
		goto i2c_check_failed;
	}

	if (NULL == pdata) {
#ifdef CONFIG_OF
		dev_err(dev,
			"%s: platform data required, allocate in CONFIG_OF!\n",
			__func__);
		pdata = kzalloc(sizeof(struct tsl2772_i2c_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			ret = -ENOMEM;
			goto data_malloc_failed;
		}
		dev->platform_data = pdata;
#else
		dev_err(dev, "%s: platform data required\n", __func__);
		ret = -EINVAL;
		goto init_failed;
#endif
	}

	chip = kzalloc(sizeof(struct tsl2772_chip), GFP_KERNEL);
	if (!chip) {
		ret = -ENOMEM;
		goto malloc_failed;
	}

	chip->client = client;
	chip->pdata = pdata;
	i2c_set_clientdata(client, chip);
#ifdef CONFIG_OF
	tsl2772_parse_dt(chip);
#endif
	if (!(pdata->prox_name || pdata->als_name) || client->irq < 0) {
		dev_err(dev, "%s: no reason to run.\n", __func__);
		ret = -EINVAL;
		goto init_failed;
	}

#ifdef CONFIG_OF
	pdata->platform_init = NULL;/* sensor_power_init; */
	pdata->platform_power = board_tsl2772_power;
	pdata->platform_teardown = NULL; /* board_tsl2772_teardown; */
#endif

	if (pdata->platform_init) {
		ret = pdata->platform_init(dev);
		if (ret)
			goto init_failed;
	}
	if (pdata->platform_power) {
		ret = pdata->platform_power(dev, POWER_ON);
		if (ret) {
			dev_err(dev, "%s: pltf power on failed\n", __func__);
			goto pon_failed;
		}
		powered = true;
		chip->powered = true;
	usleep_range(10000, 10005);
	}

	chip->seg_num_max = chip->pdata->segment_num ?
			chip->pdata->segment_num : ARRAY_SIZE(segment_default);
	if (chip->pdata->segment)
		ret = set_segment_table(chip, chip->pdata->segment,
			chip->pdata->segment_num);
	else
		ret =  set_segment_table(chip, segment_default,
			ARRAY_SIZE(segment_default));
	if (ret)
		goto set_segment_failed;

	ret = taos_get_id(chip, &id, &rev);
	if (ret)
		goto id_failed;
	for (i = 0; i < ARRAY_SIZE(tsl277x_ids); i++) {
		if (id == tsl277x_ids[i])
			break;
	}
	if (i < ARRAY_SIZE(tsl277x_names)) {
		dev_info(dev, "%s: '%s rev. %d' detected\n", __func__,
			tsl277x_names[i], rev);
	} else {
		dev_err(dev, "%s: not supported chip id\n", __func__);
		ret = -EOPNOTSUPP;
		goto id_failed;
	}
	mutex_init(&chip->lock);
	set_pltf_settings(chip);

	ret = flush_regs(chip);
	if (ret)
		goto flush_regs_failed;
#if 0
	if (pdata->platform_power) {
		pdata->platform_power(dev, POWER_OFF);
		powered = false;
		chip->powered = false;
	}
#endif

	if (!pdata->prox_name)
		goto bypass_prox_idev;
	chip->p_idev = input_allocate_device();
	if (!chip->p_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, pdata->prox_name);
		ret = -ENODEV;
		goto input_p_alloc_failed;
	}
	chip->p_idev->name = pdata->prox_name;
	chip->p_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->p_idev->evbit);
	set_bit(ABS_DISTANCE, chip->p_idev->absbit);
	input_set_abs_params(chip->p_idev, ABS_DISTANCE, 0, 1, 0, 0);
	chip->p_idev->open = prox_idev_open;
	chip->p_idev->close = prox_idev_close;
	dev_set_drvdata(&chip->p_idev->dev, chip);
	ret = input_register_device(chip->p_idev);
	if (ret) {
		input_free_device(chip->p_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, pdata->prox_name);
		goto input_p_alloc_failed;
	}

	ret = add_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
	if (ret)
		goto input_p_sysfs_failed;
bypass_prox_idev:
	if (!pdata->als_name)
		goto bypass_als_idev;
	chip->a_idev = input_allocate_device();
	if (!chip->a_idev) {
		dev_err(dev, "%s: no memory for input_dev '%s'\n",
				__func__, pdata->als_name);
		ret = -ENODEV;
		goto input_a_alloc_failed;
	}
	chip->a_idev->name = pdata->als_name;
	chip->a_idev->id.bustype = BUS_I2C;
	set_bit(EV_ABS, chip->a_idev->evbit);
	set_bit(ABS_MISC, chip->a_idev->absbit);
	input_set_abs_params(chip->a_idev, ABS_MISC, 0, 65535, 0, 0);
	chip->a_idev->open = als_idev_open;
	chip->a_idev->close = als_idev_close;
	dev_set_drvdata(&chip->a_idev->dev, chip);
	ret = input_register_device(chip->a_idev);
	if (ret) {
		input_free_device(chip->a_idev);
		dev_err(dev, "%s: cant register input '%s'\n",
				__func__, pdata->prox_name);
		goto input_a_alloc_failed;
	}
	ret = add_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
	if (ret)
		goto input_a_sysfs_failed;

bypass_als_idev:
	INIT_WORK(&chip->irq_work, irq_work_func);
	ret = request_irq(client->irq, taos_irq, IRQF_TRIGGER_FALLING,
			dev_name(dev), chip);
	if (ret) {
		dev_info(dev, "Failed to request irq %d\n", client->irq);
		goto irq_register_fail;
	}

	dev_info(dev, "Probe ok.\n");
	return 0;

irq_register_fail:
	if (chip->a_idev) {
		remove_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
input_a_sysfs_failed:
		input_unregister_device(chip->a_idev);
	}
input_a_alloc_failed:
	if (chip->p_idev) {
		remove_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
input_p_sysfs_failed:
		input_unregister_device(chip->p_idev);
	}
input_p_alloc_failed:
flush_regs_failed:
id_failed:
	kfree(chip->segment);
set_segment_failed:
	if (powered && pdata->platform_power) {
		pdata->platform_power(dev, POWER_OFF);
		chip->powered = false;
	}
pon_failed:
	if (pdata->platform_teardown)
		pdata->platform_teardown(dev);
init_failed:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
#ifdef CONFIG_OF
malloc_failed:
	kfree(pdata);
data_malloc_failed:
#endif
i2c_check_failed:
	dev_err(dev, "Probe failed.\n");

	return ret;
}

static int taos_suspend(struct device *dev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	struct tsl2772_i2c_platform_data *pdata = chip->pdata;

	dev_dbg(dev, "%s\n", __func__);
	mutex_lock(&chip->lock);
	chip->in_suspend = 1;
	if (chip->prox_enabled) {
		if (pdata->proximity_can_wake) {
			dev_dbg(dev, "set wake on proximity\n");
			chip->wake_irq = 1;
		} else {
			dev_dbg(dev, "proximity off\n");
			taos_prox_enable(chip, 0);
		}
	}

	if (chip->als_enabled) {
		if (pdata->als_can_wake) {
			dev_dbg(dev, "set wake on als\n");
			chip->wake_irq = 1;
		} else {
			dev_dbg(dev, "als off\n");
			taos_als_enable(chip, 0);
		}
	}

	if (chip->wake_irq) {
		irq_set_irq_wake(chip->client->irq, 1);
	} else if (chip->powered) {
		dev_dbg(dev, "powering off\n");
		pltf_power_off(chip);
	}
	mutex_unlock(&chip->lock);

	return 0;
}

static int taos_resume(struct device *dev)
{
	struct tsl2772_chip *chip = dev_get_drvdata(dev);
	bool als_on, prx_on;
	int rc = 0;
	mutex_lock(&chip->lock);
	prx_on = chip->prox_enabled;
	als_on = chip->als_enabled;
	chip->in_suspend = 0;
	dev_dbg(dev,
		"%s: powerd %d, als_on %d enabled %d, prx_on %d  enabled %d\n",
		__func__, chip->powered, als_on, chip->als_status, prx_on,
		chip->prox_status);
	if (chip->wake_irq) {
		dev_info(dev, "disable irq wake\n");
		irq_set_irq_wake(chip->client->irq, 0);
		chip->wake_irq = 0;
	}
	if (!chip->powered) {
		dev_dbg(dev, "powering on\n");
		rc = power_on(chip);
		if (rc)
			goto err_power;

		set_pltf_settings(chip);
		flush_regs(chip);
	}
	if (prx_on && !chip->prox_status) {
		dev_dbg(dev, "prox enable\n");
		(void)taos_prox_enable(chip, 1);
	}
	if (als_on && !chip->als_status)
		(void)taos_als_enable(chip, 1);
	if (chip->irq_pending) {
		dev_dbg(dev, "%s: pending interrupt\n", __func__);
		chip->irq_pending = 0;
		(void)taos_check_and_report(chip);
		enable_irq(chip->client->irq);
	}
err_power:
	mutex_unlock(&chip->lock);

	return 0;
}

static int taos_remove(struct i2c_client *client)
{
	struct tsl2772_chip *chip = i2c_get_clientdata(client);
	free_irq(client->irq, chip);

	if (chip->a_idev) {
		remove_sysfs_interfaces(&chip->a_idev->dev,
			als_attrs, ARRAY_SIZE(als_attrs));
		input_unregister_device(chip->a_idev);
	}
	if (chip->p_idev) {
		remove_sysfs_interfaces(&chip->p_idev->dev,
			prox_attrs, ARRAY_SIZE(prox_attrs));
		input_unregister_device(chip->p_idev);
	}
	if (chip->pdata->platform_teardown)
		chip->pdata->platform_teardown(&client->dev);
	i2c_set_clientdata(client, NULL);
	kfree(chip->segment);
	kfree(chip);
	return 0;
}

static struct of_device_id taos_match_table[] = {
	{ .compatible = "taos:tsl2772",},
	{},
};

static struct i2c_device_id taos_idtable[] = {
	{ "tsl2772", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, taos_idtable);

static const struct dev_pm_ops taos_pm_ops = {
	.suspend = taos_suspend,
	.resume  = taos_resume,
};

static struct i2c_driver taos_driver = {
	.driver = {
		.name = "tsl2772",
		.owner	= THIS_MODULE,
		.of_match_table = taos_match_table,
		.pm = &taos_pm_ops,
	},
	.id_table = taos_idtable,
	.probe = taos_probe,
	.remove = taos_remove,
};

static int __init taos_init(void)
{
	return i2c_add_driver(&taos_driver);
}

static void __exit taos_exit(void)
{
	i2c_del_driver(&taos_driver);
}

module_init(taos_init);
module_exit(taos_exit);

MODULE_AUTHOR("Aleksej Makarov <aleksej.makarov@sonyericsson.com>");
MODULE_DESCRIPTION("TAOS tsl2772 ambient light and proximity sensor driver");
MODULE_LICENSE("GPL");
