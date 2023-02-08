#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c-gpio.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/sensor/sensors_axes_s16.h>
#include <linux/sensor/tmd27723.h>
#include <linux/sensor/cm36686.h>
#include <linux/sensor/adpd142.h>
#include <linux/mpu.h>
#include <linux/nfc/sec_nfc_com.h>
#include <linux/delay.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <plat/devs.h>
#include <plat/iic.h>
#include "board-universal222ap.h"
#include <linux/sensor/alps_magnetic.h>

#include <linux/clk.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <plat/s3c64xx-spi.h>
#include <mach/spi-clocks.h>

#if defined(CONFIG_SENSORS_VFS61XX)
#include <linux/vfs61xx_platform.h>
#endif
#define GPIO_PROXI_INT		EXYNOS4_GPX2(7)

#define GPIO_NFC_IRQ		EXYNOS4_GPX1(7)
#define GPIO_NFC_EN		EXYNOS4_GPF1(4)
#define GPIO_NFC_FIRMWARE	EXYNOS4_GPM1(5)
#ifdef CONFIG_SEC_NFC_CLK_REQ
#define GPIO_NFC_CLK_REQ	EXYNOS4_GPX1(0)
#define GPIO_NFC_CLK_EN		EXYNOS4_GPM1(0)
#endif
#define GPIO_HRM_INT	EXYNOS4_GPX3(4)

#define GPIO_PROX_HRM_SDA EXYNOS4_GPF2(1)
#define GPIO_PROX_HRM_SCL EXYNOS4_GPF2(2)

extern unsigned int system_rev;

static void sec_nfc_gpio_init(void)
{
	int ret;
	unsigned int gpio;

	pr_info("%s\n", __func__);

	gpio = GPIO_NFC_IRQ;
	ret = gpio_request(gpio, "nfc_int");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_int.(%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(gpio, 0xf);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	gpio_free(gpio);

	gpio = GPIO_NFC_EN;
	ret = gpio_request(gpio, "nfc_ven");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_ven.(%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_free(gpio);

	gpio = GPIO_NFC_FIRMWARE;
	ret = gpio_request(gpio, "nfc_firm");
	if (ret)
		pr_err("%s, Failed to request gpio nfc_firm.(%d)\n",
			__func__, ret);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_free(gpio);

#ifdef CONFIG_SEC_NFC_CLK_REQ
	gpio = GPIO_NFC_CLK_REQ;
	ret = gpio_request(gpio, "nfc_clk_req");
	if (ret)
	pr_err("%s, Failed to request gpio clk_req.(%d)\n",
		__func__, ret);

	s3c_gpio_cfgpin(gpio, 0xf);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_free(gpio);

	gpio = GPIO_NFC_CLK_EN;
	ret = gpio_request(gpio, "nfc_clk_en");
	if (ret)
	pr_err("%s, Failed to request gpio clk_en.(%d)\n",
		__func__, ret);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_set_value(gpio, 1);
	gpio_free(gpio);
#endif

	gpio = GPIO_HRM_INT;
	ret = gpio_request(gpio, "hrm_int");
	if (ret)
	pr_err("%s, Failed to request gpio clk_en.(%d)\n",
		__func__, ret);

	s3c_gpio_cfgpin(gpio, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_pd_cfg(gpio, S5P_GPIO_PD_PREV_STATE);
	gpio_free(gpio);

	pr_info("%s, NFC init Finished!!!\n", __func__);
}


static struct sec_nfc_platform_data s3fnrn5_pdata = {
	.irq = GPIO_NFC_IRQ,
	.ven = GPIO_NFC_EN,
	.firm = GPIO_NFC_FIRMWARE,
	.wake = GPIO_NFC_FIRMWARE,
	/* .tvdd = GPIO_NFC_TVDD_EN, */
	/* .avdd = GPIO_NFC_AVDD_EN, */
#ifdef CONFIG_SEC_NFC_CLK_REQ
	.clk_req = GPIO_NFC_CLK_REQ,
	.clk_enable = GPIO_NFC_CLK_EN,
#endif
	.cfg_gpio = sec_nfc_gpio_init,
	/* .power_onoff = nfc_power, */
};

static struct i2c_board_info i2c_devs5[] __initdata = {
	{
		I2C_BOARD_INFO("sec-nfc", 0x27),
		.irq = IRQ_EINT(15),
		.platform_data = &s3fnrn5_pdata,
	},
};

static void light_sensor_power(bool on)
{
	static struct regulator *light_2v8 = NULL;
	static bool init_state = 1;
	static bool light_status = 0;

	pr_info("cm36686_%s : %s\n", __func__, (on)?"on":"off");

	if (on == light_status)
		return;

	if (init_state)
	{
		if (!light_2v8) {
			light_2v8 = regulator_get(NULL, "light_sensor_2v8");
			if (IS_ERR(light_2v8)) {
				pr_err("cm36686_%s : light_2v8 regulator not get!\n", __func__);
				light_2v8 = NULL;
				return;
			}
		}
		init_state = 0;
	}

	if (on) {
		regulator_enable(light_2v8);
		pr_info("cm36686_%s : light_2v8_enable\n", __func__);
		msleep(2);
		light_status = 1;
	} else {
		regulator_disable(light_2v8);
		pr_info("cm36686_%s : light_2v8_disable\n", __func__);
		light_status = 0;
	}
}

static void proxi_sensor_power(bool on)
{
	static struct regulator *proxy_2v8 = NULL;
	static bool init_state = 1;
	static bool light_status = 0;

	pr_info("cm36686_%s : %s\n", __func__, (on)?"on":"off");

	if (on == light_status)
		return;

	if (init_state)
	{
		if (!proxy_2v8) {
			proxy_2v8 = regulator_get(NULL, "proxi_sensor_2v8");
			if (IS_ERR(proxy_2v8)) {
				pr_err("cm36686_%s : proxy_2v8 regulator not get!\n", __func__);
				proxy_2v8 = NULL;
				return;
			}
		}
		init_state = 0;
	}

	if (on) {
		regulator_enable(proxy_2v8);
		pr_info("cm36686_%s : proxy_2v8_enable\n", __func__);
		msleep(2);
		light_status = 1;
	} else {
		regulator_disable(proxy_2v8);
		pr_info("cm36686_%s : proxy_2v8_disable\n", __func__);
		light_status = 0;
	}
}

static void proxi_led_power(bool on)
{
	static struct regulator *proxy_reg;
	static bool init_state = 1;
	static bool proxi_status = 0;

	pr_info("cm36686_%s : %s\n", __func__, (on)?"on":"off");

	if (on == proxi_status)
		return;

	if (init_state) {
		if (!proxy_reg) {
			proxy_reg = regulator_get(NULL, "led_a_3v0");
			if (IS_ERR(proxy_reg)) {
				pr_err("cm36686_%s : proxy_reg regulator not get!\n", __func__);
				proxy_reg = NULL;
				return;
			}
		}
		regulator_set_voltage(proxy_reg, 3000000, 3000000);
		init_state = 0;
	}

	if (on) {
		regulator_enable(proxy_reg);
		pr_info("cm36686_%s : proxy_reg_enable\n", __func__);
		msleep(2);
		proxi_status = 1;
	} else {
		regulator_disable(proxy_reg);
		pr_info("cm36686_%s : proxy_reg_disable\n", __func__);
		proxi_status = 0;
	}
}

static void alps_sensor_power(bool on)
{
	static struct regulator *alps_2v8 = NULL;
	static bool init_state = 1;
	static bool alps_status = 0;

	pr_info("%s : %s\n", __func__, (on)?"on":"off");

	if (on == alps_status)
		return;

	if (init_state)
	{
		if (!alps_2v8) {
			alps_2v8 = regulator_get(NULL, "alps_sensor_2v8");
			if (IS_ERR(alps_2v8)) {
				pr_err("%s : alps_2v8 regulator not get!\n", __func__);
				alps_2v8 = NULL;
				return;
			}
		}
		init_state = 0;
	}

	if (on) {
		regulator_enable(alps_2v8);
		pr_info("%s : alps_2v8_enable\n", __func__);
		msleep(3);
		alps_status = 1;
	} else {
		regulator_disable(alps_2v8);
		pr_info("%s : alps_2v8_disable\n", __func__);
		alps_status = 0;
	}
}

static void adpd142_sensor_power(bool onoff)
{
	struct regulator *vled_ic;
	int ret;

	pr_info("%s \n", __func__);
	if(system_rev > 1) {
		pr_info("%s, adpd142_vled_3v3_power control !!\n", __func__);
		vled_ic = regulator_get(NULL, "vled_3v3");
	} else {
		pr_info("%s, adpd142_led_a_3v0_power control !!\n", __func__);
		vled_ic = regulator_get(NULL, "led_a_3v0");
	}
	if (IS_ERR(vled_ic)) {
		pr_err("%s : could not get regulator vled_3v3\n", __func__);
		return;
	}

	if (onoff) {
		pr_info("%s, vled_on !!\n", __func__);
		ret = regulator_enable(vled_ic);
	} else {
		pr_info("%s, vled_off !!\n", __func__);
		ret = regulator_disable(vled_ic);
	}

	if (ret < 0)
		pr_warn("%s: regulator %sable is failed: %d\n", __func__,
				onoff ? "En" : "Dis", ret);

	regulator_put(vled_ic);
}

static void taos_gpio_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_PROXI_INT, "gpio_proximity_out");
	if (ret)
		pr_err("%s, Failed to request gpio gpio_proximity_out. (%d)\n",
			__func__, ret);

	s3c_gpio_setpull(GPIO_PROXI_INT, S3C_GPIO_PULL_NONE);
	gpio_free(GPIO_PROXI_INT);
}

static struct taos_platform_data taos_pdata = {
	.power	= light_sensor_power,
	.led_on	= proxi_led_power,
	.als_int = GPIO_PROXI_INT,
	.prox_thresh_hi = 700,
	.prox_thresh_low = 550,
	.prox_th_hi_cal = 470,
	.prox_th_low_cal = 380,
	.als_time = 0xED,
	.intr_filter = 0x33,
	.prox_pulsecnt = 0x08,
	.prox_gain = 0x28,
	.coef_atime = 50,
	.ga = 89,
	.coef_a = 1000,
	.coef_b = 1630,
	.coef_c = 1115,
	.coef_d = 1935,
	.min_max = MIN,
};

static void cm36686_gpio_init(void)
{
	int ret;

	pr_info("%s\n", __func__);

	ret = gpio_request(GPIO_PROXI_INT, "gpio_proximity_out");
	if (ret)
		pr_err("%s, Failed to request gpio gpio_proximity_out. (%d)\n",
			__func__, ret);

	s3c_gpio_setpull(GPIO_PROXI_INT, S3C_GPIO_PULL_UP);
	gpio_free(GPIO_PROXI_INT);
}

static struct cm36686_platform_data cm36686_pdata = {
	.cm36686_led_on = proxi_led_power,
	.irq = GPIO_PROXI_INT,
	.default_hi_thd = 14,
	.default_low_thd = 9,
	.cancel_hi_thd = 10,
	.cancel_low_thd = 5,
};

static struct mpu_platform_data mpu_iio_rev00 = {
	.int_config  = 0x00,
	.level_shifter = 0,
	.orientation = { 1,  0,  0,
			0, 1, 0,
			0,  0,  1 },
};

static struct mpu_platform_data mpu_iio_rev01 = {
	.int_config  = 0x00,
	.level_shifter = 0,
	.orientation = { -1,  0,  0,
			0, 1, 0,
			0,  0,  -1 },
};

static struct alps_platform_data alps_mag_pdata = {
	.orient = { 0,  -1,  0,
		1, 0, 0,
		 0,  0,  1 },
};


/**
structure hold adpd142 configuration data to be written during probe.
*/
static struct adpd_platform_data adpd142_config_data = {
	.adpd142_power = adpd142_sensor_power,
	.config_size = 53,
	.config_data = {
		0x00100001,
		0x000100ff,
		0x00020004,
		0x00060000,
		0x00111011,
		0x0012000A,
		0x00130320,
		0x00140449,
		0x00150333,
		0x001C4040,
		0x001D4040,
		0x00181400,
		0x00191400,
		0x001a1650,
		0x001b1000,
		0x001e1ff0,
		0x001f1Fc0,
		0x00201ad0,
		0x00211470,
		0x00231032,
		0x00241039,
		0x002502CC,
		0x00340000,
		0x00260000,
		0x00270800,
		0x00280000,
		0x00294003,
		0x00300330,
		0x00310213,
		0x00421d37,
		0x0043ada5,
		0x003924D2,
		0x00350330,
		0x00360813,
		0x00441d37,
		0x0045ada5,
		0x003b24D2,
		0x00590808,
		0x00320320,
		0x00330113,
		0x003a22d4,
		0x003c0006,
		0x00401010,
		0x0041004c,
		0x004c0004,
		0x004f2090,
		0x00500000,
		0x00523000,
		0x0053e400,
		0x005b1831,
		0x005d1b31,
		0x005e2808,
		0x004e0040,
	},
};

static struct i2c_board_info i2c_devs7_rev00[] __initdata = {
	{
		I2C_BOARD_INFO("taos", 0x39),
		.platform_data = &taos_pdata,
		.irq = IRQ_EINT(23),
	}, {
		I2C_BOARD_INFO("mpu6515", 0x68),
		.platform_data = &mpu_iio_rev00,
		.irq = IRQ_EINT(24),
	}, {
		I2C_BOARD_INFO("hscd_i2c", 0x0c),
		.platform_data = &alps_mag_pdata,
	}, {
		I2C_BOARD_INFO("adpd142", 0x64),
		.platform_data = &adpd142_config_data,
		.irq = GPIO_HRM_INT,
	},
};

static struct i2c_board_info i2c_devs7_rev01[] __initdata = {
	{
		I2C_BOARD_INFO("mpu6515", 0x68),
		.platform_data = &mpu_iio_rev01,
		.irq = IRQ_EINT(24),
	}, {
		I2C_BOARD_INFO("hscd_i2c", 0x0c),
		.platform_data = &alps_mag_pdata,
	},
};

/* I2C29 */
static struct i2c_gpio_platform_data gpio_i2c_data29 = {
	.scl_pin = GPIO_PROX_HRM_SCL,
	.sda_pin = GPIO_PROX_HRM_SDA,
	.udelay = 2,
};

static struct platform_device s3c_device_i2c29_emul = {
	.name = "i2c-gpio",
	.id = 29,
	.dev.platform_data = &gpio_i2c_data29,
};

static struct i2c_board_info i2c_device29_emul[] __initdata = {
	{
		I2C_BOARD_INFO("adpd142", 0x64),
		.platform_data = &adpd142_config_data,
		.irq = GPIO_HRM_INT,
	},
	{
		I2C_BOARD_INFO("cm36686", 0x60),
		.platform_data = &cm36686_pdata,
	},
};

static struct platform_device alps_pdata = {
	.name = "alps-input",
	.id = -1,
};

static struct platform_device *sensor_i2c_devices[] __initdata = {
	&s3c_device_i2c5,
	&s3c_device_i2c7,
	&s3c_device_i2c29_emul,
	&alps_pdata,
};

static void sensor_i2c_device29_setup_gpio(void)
{
	s3c_gpio_cfgpin(gpio_i2c_data29.scl_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data29.scl_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data29.scl_pin, S5P_GPIO_DRVSTR_LV1);

	s3c_gpio_cfgpin(gpio_i2c_data29.sda_pin, S3C_GPIO_INPUT);
	s3c_gpio_setpull(gpio_i2c_data29.sda_pin, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_drvstr(gpio_i2c_data29.sda_pin, S5P_GPIO_DRVSTR_LV1);
}

#if defined(CONFIG_SENSORS_VFS61XX)
#define GPIO_BTP_LDO_EN_18V		EXYNOS4_GPC0(1)
#define GPIO_BTP_LDO_EN_33V		EXYNOS4_GPC0(4)
#define GPIO_BTP_RST_N			EXYNOS4_GPC0(2)
#define GPIO_BTP_OCP_EN			EXYNOS4_GPC0(3)
#define GPIO_BTP_IRQ			EXYNOS4_GPX2(1)

#define GPIO_BTP_SPI_CLK		EXYNOS4_GPB(4)
#define GPIO_BTP_SPI_CS_N		EXYNOS4_GPB(5)
#define GPIO_BTP_SPI_MOSI		EXYNOS4_GPB(7)
#define GPIO_BTP_SPI_MISO		EXYNOS4_GPB(6)

static void vfs61xx_setup_gpio(void)
{
	s3c_gpio_cfgpin(GPIO_BTP_LDO_EN_18V, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	s3c_gpio_setpull(GPIO_BTP_LDO_EN_18V, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_BTP_LDO_EN_18V, 0);

	s3c_gpio_cfgpin(GPIO_BTP_LDO_EN_33V, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	s3c_gpio_setpull(GPIO_BTP_LDO_EN_33V, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_BTP_LDO_EN_33V, 0);

	s3c_gpio_cfgpin(GPIO_BTP_OCP_EN, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	s3c_gpio_setpull(GPIO_BTP_OCP_EN, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_BTP_OCP_EN, 0);

	s3c_gpio_cfgpin(GPIO_BTP_IRQ, S3C_GPIO_SFN(S3C_GPIO_INPUT));
	s3c_gpio_setpull(GPIO_BTP_IRQ, S3C_GPIO_PULL_UP);

	s3c_gpio_cfgpin(GPIO_BTP_RST_N, S3C_GPIO_SFN(S3C_GPIO_OUTPUT));
	s3c_gpio_setpull(GPIO_BTP_RST_N, S3C_GPIO_PULL_DOWN);
	gpio_set_value(GPIO_BTP_RST_N, 0);
}

static struct vfs61xx_platform_data vfs61xx_pdata = {
	.drdy = GPIO_BTP_IRQ,
	.sleep = GPIO_BTP_RST_N,
	.ldo_pin_33v = GPIO_BTP_LDO_EN_33V,
	.ldo_pin_18v = GPIO_BTP_LDO_EN_18V,
	.ocp_en = GPIO_BTP_OCP_EN,
	.orient = 1,
};

static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line		= GPIO_BTP_SPI_CS_N,
		.set_level	= gpio_set_value,
		.fb_delay	= 0x0,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias		= "validity_fingerprint",
		.max_speed_hz		= 15 * MHZ,
		.bus_num		= 1,
		.chip_select		= 0,
		.mode			= SPI_MODE_0,
		.irq			= IRQ_EINT(17),
		.controller_data	= &spi1_csi[0],
		.platform_data		= &vfs61xx_pdata,
	}
};
#endif

void __init board_universal_ss222ap_init_sensor(void)
{
	int ret;

	sec_nfc_gpio_init();

	if(system_rev == 0){
		taos_gpio_init();
	}else {
		cm36686_gpio_init();
	}

	s3c_i2c5_set_platdata(NULL);
	s3c_i2c7_set_platdata(NULL);
	ret = i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));
	if (ret < 0)
		pr_err("%s, i2c5 adding i2c fail(err=%d)\n",
					__func__, ret);

    if(system_rev == 0) {
        ret = i2c_register_board_info(7, i2c_devs7_rev00, ARRAY_SIZE(i2c_devs7_rev00));
        pr_info("%s, system_rev = %d\n",
                            __func__, system_rev);
        if (ret < 0) {
            pr_err("%s, i2c7_rev00 adding i2c fail(err=%d)\n",
                            __func__, ret);
        }
    } else {
	/* ALPS Power Control */
	if(system_rev == 5) {
		alps_mag_pdata.alps_magnetic_power = alps_sensor_power;
	}

	ret = i2c_register_board_info(7, i2c_devs7_rev01, ARRAY_SIZE(i2c_devs7_rev01));
        pr_info("%s, system_rev = %d\n",
                            __func__, system_rev);

	if (ret < 0) {
            pr_err("%s, i2c7_rev01 adding i2c fail(err=%d)\n",
				__func__, ret);
        }

	sensor_i2c_device29_setup_gpio();

	/* CM36686 Power Control */
	if(system_rev == 5) {
		cm36686_pdata.cm36686_light_power = light_sensor_power;
		cm36686_pdata.cm36686_proxi_power = proxi_sensor_power;
	}

	ret = i2c_register_board_info(29, i2c_device29_emul,
            ARRAY_SIZE(i2c_device29_emul));
	if (ret < 0) {
            pr_err("%s, i2c29 adding i2c fail(err=%d)\n",
                            __func__, ret);
        }
    }
	platform_add_devices(sensor_i2c_devices, ARRAY_SIZE(sensor_i2c_devices));

#if defined(CONFIG_SENSORS_VFS61XX)
	pr_info("%s: SENSORS_VFS61XX init\n", __func__);
	vfs61xx_setup_gpio();

	s3c64xx_spi1_pdata.dma_mode = PIO_MODE;
	vfs61xx_pdata.ldocontrol = 1;

	/* orient 1, except the hw rev 0.3(orient 0) */
	if(system_rev == 4)
		vfs61xx_pdata.orient = 0;
	pr_info("%s: vfs61xx system_rev: %d, orient : %d\n"
		, __func__, system_rev, vfs61xx_pdata.orient);

	if (!exynos_spi_cfg_cs(spi1_csi[0].line, 1)) {
		pr_info("%s: spi1_set_platdata ...\n", __func__);
		s3c64xx_spi1_set_platdata(&s3c64xx_spi1_pdata,
			EXYNOS_SPI_SRCCLK_SCLK, ARRAY_SIZE(spi1_csi));

		spi_register_board_info(spi1_board_info,
			ARRAY_SIZE(spi1_board_info));
	} else {
		pr_err("%s : Error requesting gpio for SPI-CH%d CS",
			__func__, spi1_board_info->bus_num);
	}
	platform_device_register(&s3c64xx_device_spi1);
#endif
}
