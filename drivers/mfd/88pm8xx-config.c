#include "88pm8xx-config.h"

#define PM800_BASE_PAGE  0x0
#define PM800_POWER_PAGE 0x1
#define PM800_GPADC_PAGE 0x2
/*
 * board specfic configurations also parameters from customers,
 * this parameters are passed form board dts file
 */
static void pmic_board_config(struct pm80x_chip *chip, struct device_node *np)
{
	unsigned int page, reg, mask, data;
	const __be32 *values;
	int size, rows, index;

	values = of_get_property(np, "marvell,pmic-board-cfg", &size);
	if (!values) {
		dev_warn(chip->dev, "no valid property for %s\n", np->name);
		return;
	}

	/* number of elements in array */
	size /= sizeof(*values);
	rows = size / 4;
	dev_info(chip->dev, "Proceed PMIC board specific configuration (%d items)\n", rows);
	index = 0;

	while (rows--) {
		page = be32_to_cpup(values + index++);
		reg = be32_to_cpup(values + index++);
		mask = be32_to_cpup(values + index++);
		data = be32_to_cpup(values + index++);
		switch (page) {
		case PM800_BASE_PAGE:
			dev_info(chip->dev, "base [0x%02x] <- 0x%02x,  mask = [0x%2x]\n", reg, data, mask);
			regmap_update_bits(chip->regmap, reg, mask, data);
			break;
		case PM800_POWER_PAGE:
			dev_info(chip->dev, "power [0x%02x] <- 0x%02x,  mask = [0x%2x]\n", reg, data, mask);
			regmap_update_bits(chip->subchip->regmap_power, reg, mask, data);
			break;
		case PM800_GPADC_PAGE:
			dev_info(chip->dev, "gpadc [0x%02x] <- 0x%02x,  mask = [0x%2x]\n", reg, data, mask);
			regmap_update_bits(chip->subchip->regmap_gpadc, reg, mask, data);
			break;
		default:
			dev_warn(chip->dev, "Unsupported page for %d\n", page);
			break;
		}
	}

	return;
}

void parse_powerup_down_log(struct pm80x_chip *chip)
{
	int up_log, down1_log, down2_log, bit;
	static const char *powerup_name[7] = {
		"ONKEY_WAKEUP    ",
		"CHG_WAKEUP      ",
		"EXTON_WAKEUP    ",
		"RESERVED        ",
		"RTC_ALARM_WAKEUP",
		"FAULT_WAKEUP    ",
		"BAT_WAKEUP      "
	};

	static const char *powerd1_name[8] = {
		"OVER_TEMP ",
		"UV_VSYS1  ",
		"SW_PDOWN  ",
		"FL_ALARM  ",
		"WDT       ",
		"LONG_ONKEY",
		"OV_VSYS   ",
		"RTC_RESET "
	};

	static const char *powerd2_name[5] = {
		"HYB_DONE   ",
		"UV_VSYS2   ",
		"HW_RESET   ",
		"PGOOD_PDOWN",
		"LONKEY_RTC "
	};

	/*power up log*/
	regmap_read(chip->regmap, PM800_POWER_UP_LOG, &up_log);
	dev_info(chip->dev, "powerup log 0x%x: 0x%x\n", PM800_POWER_UP_LOG, up_log);
	dev_info(chip->dev, " -------------------------------\n");
	dev_info(chip->dev, "|     name(power up) |  status  |\n");
	dev_info(chip->dev, "|--------------------|----------|\n");
	for (bit = 0; bit < ARRAY_SIZE(powerup_name); bit++)
		dev_info(chip->dev, "|  %s  |    %x     |\n",
			powerup_name[bit], (up_log >> bit) & 1);
	dev_info(chip->dev, " -------------------------------\n");
	/*power down log1*/
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG1, &down1_log);
	dev_info(chip->dev, "powerdown log1 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG1, down1_log);
	dev_info(chip->dev, " -------------------------------\n");
	dev_info(chip->dev, "| name(power down1)  |  status  |\n");
	dev_info(chip->dev, "|--------------------|----------|\n");
	for (bit = 0; bit < ARRAY_SIZE(powerd1_name); bit++)
		dev_info(chip->dev, "|    %s      |    %x     |\n",
			powerd1_name[bit], (down1_log >> bit) & 1);
	dev_info(chip->dev, " -------------------------------\n");
	/*power down log2*/
	regmap_read(chip->regmap, PM800_POWER_DOWN_LOG2, &down2_log);
	dev_info(chip->dev, "powerdown log2 0x%x: 0x%x\n", PM800_POWER_DOWN_LOG2, down2_log);
	dev_info(chip->dev, " -------------------------------\n");
	dev_info(chip->dev, "|  name(power down2) |  status  |\n");
	dev_info(chip->dev, "|--------------------|----------|\n");
	for (bit = 0; bit < ARRAY_SIZE(powerd2_name); bit++)
		dev_info(chip->dev, "|    %s     |    %x     |\n",
			powerd2_name[bit], (down2_log>> bit) & 1);
	dev_info(chip->dev, " -------------------------------\n");

	/* write to clear */
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG1, 0xff);
	regmap_write(chip->regmap, PM800_POWER_DOWN_LOG2, 0xff);

	/* mask reserved bits and sleep indication */
	down2_log &= 0x1e;

	/* keep globals for external usage */
	chip->powerup = up_log;
	chip->powerdown1 = down1_log;
	chip->powerdown2 = down2_log;
}

int pm800_init_config(struct pm80x_chip *chip, struct device_node *np)
{
	int data;
	if (!chip || !chip->regmap || !chip->subchip
	    || !chip->subchip->regmap_power) {
		pr_err("%s:chip is not availiable!\n", __func__);
		return -EINVAL;
	}

	/*base page:reg 0xd0.7 = 1 32kHZ generated from XO */
	regmap_read(chip->regmap, PM800_RTC_CONTROL, &data);
	data |= (1 << 7);
	regmap_write(chip->regmap, PM800_RTC_CONTROL, data);

	/* Set internal digital sleep voltage as 1.2V */
	regmap_read(chip->regmap, PM800_LOW_POWER1, &data);
	data &= ~(0xf << 4);
	regmap_write(chip->regmap, PM800_LOW_POWER1, data);
	/*
	 * enable 32Khz-out-3 low jitter XO_LJ = 1 in pm800
	 * enable 32Khz-out-2 low jitter XO_LJ = 1 in pm822
	 * they are the same bit
	 */
	regmap_read(chip->regmap, PM800_LOW_POWER2, &data);
	data |= (1 << 5);
	regmap_write(chip->regmap, PM800_LOW_POWER2, data);

	switch (chip->type) {
	case CHIP_PM800:
		/* enable 32Khz-out-from XO 1, 2, 3 all enabled */
		regmap_write(chip->regmap, PM800_RTC_MISC2, 0x2a);
		break;

	case CHIP_PM822:
		/* select 22pF internal capacitance on XTAL1 and XTAL2*/
		regmap_read(chip->regmap, PM800_RTC_MISC6, &data);
		data |= (0x7 << 4);
		regmap_write(chip->regmap, PM800_RTC_MISC6, data);

		/* Enable 32Khz-out-from XO 1, 2 all enabled */
		regmap_write(chip->regmap, PM800_RTC_MISC2, 0xa);

		/* gps use the LDO13 set the current 170mA  */
		regmap_read(chip->subchip->regmap_power,
					PM822_LDO13_CTRL, &data);
		data &= ~(0x3);
		data |= (0x2);
		regmap_write(chip->subchip->regmap_power,
					PM822_LDO13_CTRL, data);
		/* low power config
		 * 1. base_page 0x21, BK_CKSLP_DIS is gated 1ms after sleep mode entry.
		 * 2. base_page 0x23, REF_SLP_EN reference group enter low power mode.
		 *    REF_UVL_SEL set to be 5.6V
		 * 3. base_page 0x50, 0x55 OSC_CKLCK buck FLL is locked
		 * 4. gpadc_page 0x06, GP_SLEEP_MODE MEANS_OFF scale set to be 8
		 *    MEANS_EN_SLP set to 1, GPADC operates in sleep duty cycle mode.
		 */
		regmap_read(chip->regmap, PM800_LOW_POWER2, &data);
		data |= (1 << 4);
		regmap_write(chip->regmap, PM800_LOW_POWER2, data);

		regmap_read(chip->regmap, PM800_LOW_POWER_CONFIG4, &data);
		data |= (1 << 4);
		data &= ~(0x3 < 2);
		regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG4, data);

		regmap_read(chip->regmap, PM800_OSC_CNTRL1, &data);
		data |= (1 << 0);
		regmap_write(chip->regmap, PM800_OSC_CNTRL1, data);

		regmap_read(chip->regmap, PM800_OSC_CNTRL6, &data);
		data &= ~(1 << 0);
		regmap_write(chip->regmap, PM800_OSC_CNTRL6, data);

		regmap_read(chip->subchip->regmap_gpadc, PM800_GPADC_MISC_CONFIG2, &data);
		data |= (0x7 << 4);
		regmap_write(chip->subchip->regmap_gpadc, PM800_GPADC_MISC_CONFIG2, data);

		/*
		 * enable LDO sleep mode
		 * TODO: GPS and RF module need to test after enable
		 * ldo3 sleep mode may make emmc not work when resume, disable it
		 */
		regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP1, 0xba);
		regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP2, 0xaa);
		regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP3, 0xaa);
		regmap_write(chip->subchip->regmap_power, PM800_LDO_SLP4, 0x0a);

		break;

	case CHIP_PM86X:
		/* enable buck1 dual phase mode */
		regmap_read(chip->subchip->regmap_power, PM860_BUCK1_MISC,
				&data);
		data |= BUCK1_DUAL_PHASE_SEL;
		regmap_write(chip->subchip->regmap_power, PM860_BUCK1_MISC,
				data);

		/* xo_cap sel bit(4~6)= 100 12pf register:0xe8 */
		regmap_read(chip->regmap, PM860_MISC_RTC3, &data);
		data |= (0x4 << 4);
		regmap_write(chip->regmap, PM860_MISC_RTC3, data);

		switch (chip->chip_id) {
		case CHIP_PM86X_ID_A0:
			/* set gpio4 and gpio5 to be DVC mode */
			regmap_read(chip->regmap, PM860_GPIO_4_5_CNTRL, &data);
			data |= PM860_GPIO4_GPIO_MODE(7) | PM860_GPIO5_GPIO_MODE(7);
			regmap_write(chip->regmap, PM860_GPIO_4_5_CNTRL, data);

			/* enable SLP_CNT_HD for 88pm860 A0 chip */
			regmap_update_bits(chip->regmap, PM860_A0_SLP_CNT2,
					   PM860_A0_SLP_CNT_HD, PM860_A0_SLP_CNT_HD);
			break;
		case CHIP_PM86X_ID_Z3:
		default:
			/* set gpio3 and gpio4 to be DVC mode */
			regmap_read(chip->regmap, PM860_GPIO_2_3_CNTRL, &data);
			data |= PM860_GPIO3_GPIO_MODE(7);
			regmap_write(chip->regmap, PM860_GPIO_2_3_CNTRL, data);

			regmap_read(chip->regmap, PM860_GPIO_4_5_CNTRL, &data);
			data |= PM860_GPIO4_GPIO_MODE(7);
			regmap_write(chip->regmap, PM860_GPIO_4_5_CNTRL, data);
			break;
		}

		break;

	default:
		dev_err(chip->dev, "Unknown device type: %d\n", chip->type);
		break;
	}

	/*
	 * block wakeup attempts when VSYS rises above
	 * VSYS_UNDER_RISE_TH1, or power off may fail.
	 * it is set to prevent contimuous attempt to power up
	 * incase the VSYS is above the VSYS_LOW_TH threshold.
	 */
	regmap_read(chip->regmap, PM800_RTC_MISC5, &data);
	data |= 0x1;
	regmap_write(chip->regmap, PM800_RTC_MISC5, data);

	/* enabele LDO and BUCK clock gating in lpm */
	regmap_read(chip->regmap, PM800_LOW_POWER_CONFIG3, &data);
	data |= (1 << 7);
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG3, data);
	/*
	 * disable reference group sleep mode
	 * - to reduce power fluctuation in suspend
	 */
	regmap_read(chip->regmap, PM800_LOW_POWER_CONFIG4, &data);
	data &= ~(1 << 7);
	regmap_write(chip->regmap, PM800_LOW_POWER_CONFIG4, data);

	/* enable voltage change in pmic, POWER_HOLD = 1 */
	regmap_read(chip->regmap, PM800_WAKEUP1, &data);
	data |= (1 << 7);
	regmap_write(chip->regmap, PM800_WAKEUP1, data);

	/* enable buck sleep mode */
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP1, 0xaa);
	regmap_write(chip->subchip->regmap_power, PM800_BUCK_SLP2, 0x2);

	/*
	 * set buck2 and buck4 driver selection to be full.
	 * this bit is now reserved and default value is 0,
	 * for full drive, set to 1.
	 */
	regmap_read(chip->subchip->regmap_power, 0x7c, &data);
	data |= (1 << 2);
	regmap_write(chip->subchip->regmap_power, 0x7c, data);

	regmap_read(chip->subchip->regmap_power, 0x82, &data);
	data |= (1 << 2);
	regmap_write(chip->subchip->regmap_power, 0x82, data);

	/* need to config board specific parameters */
	if (np)
		pmic_board_config(chip, np);


	return 0;
}

static int pmic_rw_reg(u8 reg, u8 value)
{
	int ret;
	u8 data, buf[2];

	struct i2c_client *client = chip_g->client;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = 1,
			.buf = &data,
		},
	};

	/*
	 * I2C pins may be in non-AP pinstate, and __i2c_transfer
	 * won't change it back to AP pinstate like i2c_transfer,
	 * so change i2c pins to AP pinstate explicitly here.
	 */
	i2c_pxa_set_pinstate(client->adapter, "default");

	/*
	 * set i2c to pio mode
	 * for in power off sequence, irq has been disabled
	 */
	i2c_set_pio_mode(client->adapter, 1);

	buf[0] = reg;
	ret = __i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0) {
		pr_err("%s read register fails...\n", __func__);
		WARN_ON(1);
	}
	/* issue SW power down */
	msgs[0].addr = client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 2;
	msgs[0].buf[0] = reg;
	msgs[0].buf[1] = data | value;
	ret = __i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0) {
		pr_err("%s write data fails: ret = %d\n", __func__, ret);
		WARN_ON(1);
	}
	return ret;

}

void sw_poweroff(void)
{
	int ret;

	pr_info("turning off power....\n");
	ret = pmic_rw_reg(PM800_WAKEUP1, PM800_SW_PDOWN);
	if (ret < 0)
		pr_err("%s, turn off power fail", __func__);
}

void pmic_reset(void)
{
	int ret;

	/* pmic reset contain two parts: fault wake up and sw_powerdown */
	pr_info("pmic power down/up reset....\n");
	ret = pmic_rw_reg(PM800_RTC_MISC5, PM800_FAULT_WAKEUP_EN | PM800_FAULT_WAKEUP);
	if (ret < 0) {
		pr_err("%s, enbale pmic fault wakeup fail!", __func__);
		return;
	}
	ret = pmic_rw_reg(PM800_WAKEUP1, PM800_SW_PDOWN);
	if (ret < 0)
		pr_err("%s, turn off power fail", __func__);

}

void extern_set_buck1_slp_volt(int on)
{
	int data;
	static int data_old;
	static bool get_data_old;

	/*
	 * needs to set buck1 sleep voltage as 0.95v if gps is powered on,
	 * and set it back when gps is powered off;
	 * This function provide such an interface to satisfy it.
	 */
	if (!get_data_old) {
		regmap_read(chip_g->subchip->regmap_power, PM800_BUCK1_SLEEP, &data_old);
		get_data_old = true;
	}
	data = data_old;
	if (on) {
		/*
		 * this means gps power on
		 * buck1 sleep voltage is set to be 0.95v
		 */
		data &= ~PM800_BUCK1_SLP_MASK;
		data |= PM800_BUCK1_SLP_V095;
		regmap_write(chip_g->subchip->regmap_power, PM800_BUCK1_SLEEP, data);
	} else {
		regmap_write(chip_g->subchip->regmap_power, PM800_BUCK1_SLEEP, data);
	}
}
EXPORT_SYMBOL(extern_set_buck1_slp_volt);

/* return gpadc voltage */
int get_gpadc_volt(struct pm80x_chip *chip, int gpadc_id)
{
	int ret, volt;
	unsigned char buf[2];
	int gp_meas;

	switch (gpadc_id) {
	case PM800_GPADC0:
		gp_meas = PM800_GPADC0_MEAS1;
		break;
	case PM800_GPADC1:
		gp_meas = PM800_GPADC1_MEAS1;
		break;
	case PM800_GPADC2:
		gp_meas = PM800_GPADC2_MEAS1;
		break;
	case PM800_GPADC3:
		gp_meas = PM800_GPADC3_MEAS1;
		break;
	default:
		dev_err(chip->dev, "get GPADC failed!\n");
		return -EINVAL;

	}
	ret = regmap_bulk_read(chip->subchip->regmap_gpadc,
			       gp_meas, buf, 2);
	if (ret < 0) {
		dev_err(chip->dev, "Attention: failed to get volt!\n");
		return -EINVAL;
	}

	volt = ((buf[0] & 0xff) << 4) | (buf[1] & 0x0f);
	dev_dbg(chip->dev, "%s: volt value = 0x%x\n", __func__, volt);
	/* volt = value * 1.4 * 1000 / (2^12) */
	volt = ((volt & 0xfff) * 7 * 100) >> 11;
	dev_dbg(chip->dev, "%s: voltage = %dmV\n", __func__, volt);

	return volt;
}

/* return voltage via bias current from GPADC */
int get_gpadc_bias_volt(struct pm80x_chip *chip, int gpadc_id, int bias)
{
	int volt, data, gp_bias;

	switch (gpadc_id) {
	case PM800_GPADC0:
		gp_bias = PM800_GPADC_BIAS1;
		break;
	case PM800_GPADC1:
		gp_bias = PM800_GPADC_BIAS2;
		break;
	case PM800_GPADC2:
		gp_bias = PM800_GPADC_BIAS3;
		break;
	case PM800_GPADC3:
		gp_bias = PM800_GPADC_BIAS4;
		break;
	default:
		dev_err(chip->dev, "get GPADC failed!\n");
		return -EINVAL;

	}

	/* get the register value */
	if (bias > 76)
		bias = 76;
	if (bias < 1)
		bias = 1;
	bias = (bias - 1) / 5;

	regmap_read(chip->subchip->regmap_gpadc, gp_bias, &data);
	data &= 0xf0;
	data |= bias;
	regmap_write(chip->subchip->regmap_gpadc, gp_bias, data);

	volt = get_gpadc_volt(chip, gpadc_id);
	if ((volt < 0) || (volt > 1400)) {
		dev_err(chip->dev, "%s return %dmV\n", __func__, volt);
		return -EINVAL;
	}

	return volt;
}

/*
 * used by non-pmic driver
 * TODO: remove later
 */
int extern_get_gpadc_bias_volt(int gpadc_id, int bias)
{
	return get_gpadc_bias_volt(chip_g, gpadc_id, bias);
}
EXPORT_SYMBOL(extern_get_gpadc_bias_volt);
