#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/sii8246.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/iic.h>

#include <mach/hs-iic.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#ifdef CONFIG_MFD_MAX77804K
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77804k-private.h>
#else
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#include <linux/mfd/max77803-private.h>
#endif
#endif

#include <linux/power_supply.h>
#include <linux/notifier.h>
#include <linux/regulator/consumer.h>
#include "board-universal5260.h"
#include "linux/power_supply.h"

/*MHL control I2C*/
#define I2C_BUS_ID_MHL	11

#define USE_HW_I2C

/*DDC I2C */
#define DDC_I2C 6

/*MHL LDO*/
#define MHL_LDO1_8 "vcc_1.8v_mhl"
#define MHL_LDO3_3 "vcc_3.3v_mhl"
#define MHL_LDO1_2 "vsil_1.2a"

/*Event of receiving*/
#define PSY_BAT_NAME "battery"
/*Event of sending*/
#define PSY_CHG_NAME "max77693-charger"

#define MHL_DEFAULT_SWING 0x74

static void sii8246_cfg_gpio(void)
{
	printk(KERN_INFO "%s()\n", __func__);

	/* AP_MHL_SDA */
	s3c_gpio_cfgpin(GPIO_MHL_SDA_18V, S3C_GPIO_SFN(0x0));
	s3c_gpio_setpull(GPIO_MHL_SDA_18V, S3C_GPIO_PULL_NONE);

	/* AP_MHL_SCL */
	s3c_gpio_cfgpin(GPIO_MHL_SCL_18V, S3C_GPIO_SFN(0x0));
	s3c_gpio_setpull(GPIO_MHL_SCL_18V, S3C_GPIO_PULL_NONE);

	s5p_gpio_set_drvstr(GPIO_MHL_SCL_18V, S5P_GPIO_DRVSTR_LV4);
	s5p_gpio_set_drvstr(GPIO_MHL_SDA_18V, S5P_GPIO_DRVSTR_LV4);

	gpio_request(GPIO_MHL_INT, "MHL_INT");
	s5p_register_gpio_interrupt(GPIO_MHL_INT);
	s3c_gpio_setpull(GPIO_MHL_INT, S3C_GPIO_PULL_DOWN);
	irq_set_irq_type(gpio_to_irq(GPIO_MHL_INT), IRQ_TYPE_EDGE_RISING);
	s3c_gpio_cfgpin(GPIO_MHL_INT, S3C_GPIO_SFN(0xF));


	s3c_gpio_setpull(GPIO_MHL_RST, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(GPIO_HDMI_EN, S3C_GPIO_PULL_NONE);
	gpio_request_one(GPIO_MHL_RST, GPIOF_OUT_INIT_LOW, "MHL_RST");
	gpio_request_one(GPIO_HDMI_EN, GPIOF_OUT_INIT_LOW, "HDMI_EN");
	gpio_free(GPIO_MHL_RST);
	gpio_free(GPIO_HDMI_EN);
}

static void sii8246_power_onoff(bool on)
{
	pr_info("sii8246: %s() %s\n", __func__, on ? "ON":"OFF");
	
	gpio_request_one(GPIO_HDMI_EN, GPIOF_OUT_INIT_LOW, "HDMI_EN");
	usleep_range(5000, 6000);
	if (on) {
		gpio_set_value(GPIO_HDMI_EN, 1);
	} else {
		gpio_set_value(GPIO_HDMI_EN, 0);
		gpio_set_value(GPIO_MHL_RST, 0);
	}
	usleep_range(10000, 20000);
	gpio_free(GPIO_HDMI_EN);
}

static void sii8246_reset(void)
{
	pr_info("sii8246: %s()\n", __func__);

	gpio_request_one(GPIO_MHL_RST, GPIOF_OUT_INIT_LOW, "MHL_RST");
	usleep_range(10000, 20000);
	gpio_set_value(GPIO_MHL_RST, 1);
	usleep_range(10000, 20000);
	gpio_free(GPIO_MHL_RST);
}

static bool vbus_present(void)
{
	union power_supply_propval value;

	psy_do_property("sec-charger", get,
				POWER_SUPPLY_PROP_ONLINE, value);
	pr_info("sec-charger : %d\n", value.intval);
	if (value.intval == POWER_SUPPLY_TYPE_BATTERY ||
			value.intval == POWER_SUPPLY_TYPE_WPC)
		return false;
	else
		return true;
}

static int muic_mhl_cb(struct mhl_platform_data *pdata, bool otg_enable,
								int plim)
{
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	int current_cable_type = POWER_SUPPLY_TYPE_MISC;
	int sub_type = ONLINE_SUB_TYPE_MHL;
	int power_type = ONLINE_POWER_TYPE_UNKNOWN;
	int muic_cable_type = max77803_muic_get_charging_type();

	switch (muic_cable_type) {
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		return 0;
	default:
		break;
	}
	pr_info("muic_mhl_cb:otg_enable=%d, plim=%d\n", otg_enable, plim);

	if (plim == 0x00) {
		pr_info("TA charger 500mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_500;
	} else if (plim == 0x01) {
		pr_info("TA charger 900mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_900;
	} else if (plim == 0x02) {
		pr_info("TA charger 1500mA\n");
		power_type = ONLINE_POWER_TYPE_MHL_1500;
	} else if (plim == 0x03) {
		pr_info("USB charger\n");
		power_type = ONLINE_POWER_TYPE_USB;
	} else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

	if (otg_enable) {
		if (!vbus_present()) {
			if (!lpcharge) {
				otg_control(true);
				current_cable_type = POWER_SUPPLY_TYPE_BATTERY;
				power_type = ONLINE_POWER_TYPE_UNKNOWN;
			}
		}
	} else
		otg_control(false);

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");
		if (psy)
			break;
	}
	if (i == 10) {
		pr_err("%s: fail to get battery ps\n", __func__);
		return -1;
	}

	pdata->charging_type = current_cable_type;
	value.intval = current_cable_type<<ONLINE_TYPE_MAIN_SHIFT
		| sub_type<<ONLINE_TYPE_SUB_SHIFT
		| power_type<<ONLINE_TYPE_PWR_SHIFT;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE,
		&value);
	if (ret) {
		pr_err("%s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
	}
	return 0;
}

static BLOCKING_NOTIFIER_HEAD(acc_notifier);

int acc_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&acc_notifier, nb);
}

int acc_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&acc_notifier, nb);
}

void acc_notify(int event)
{
	blocking_notifier_call_chain(&acc_notifier, event, NULL);
}

static struct mhl_platform_data sii8246_pdata = {
	.gpio_cfg = sii8246_cfg_gpio,
	.power = sii8246_power_onoff,
	.hw_reset = sii8246_reset,
	.muic_mhl_cb = muic_mhl_cb,
	.vbus_present = vbus_present,
	.muic_otg_set = NULL,
	.swing_level = MHL_DEFAULT_SWING,
};

static struct i2c_board_info i2c_devs_sii8246[] __initdata = {
	{
		I2C_BOARD_INFO("sii8246_i2cc8", 0xC8>>1),
		.platform_data = &sii8246_pdata,
	},
	{
		I2C_BOARD_INFO("sii8246_i2c72", 0x72>>1),
		.platform_data = &sii8246_pdata,
	},
	{
		I2C_BOARD_INFO("sii8246_i2c7a", 0x7a>>1),
		.platform_data = &sii8246_pdata,
	},
	{
		I2C_BOARD_INFO("sii8246_i2c92", 0x92>>1),
		.platform_data = &sii8246_pdata,
	},
};

#ifdef USE_HW_I2C
static struct s3c2410_platform_i2c mhl_i2c7_data __initdata = {
	.flags		= 0,
	.frequency	= 100*1000,
	.sda_delay	= 100,
	.bus_num 	= I2C_BUS_ID_MHL,
};
#else
static struct i2c_gpio_platform_data mhl_gpio_i2c_data = {
	.sda_pin = GPIO_MHL_SDA_18V,
	.scl_pin = GPIO_MHL_SCL_18V,
	.udelay = 5,	/* (500KHz / 5) */
};

struct platform_device s3c_device_mhl_i2c = {
	.name = "i2c-gpio",
	.id = I2C_BUS_ID_MHL,
	.dev.platform_data = &mhl_gpio_i2c_data,
};
#endif /*USE_HW_I2C*/

static struct platform_device *universal5260_mhl_device[] __initdata = {
#ifdef USE_HW_I2C
	&s3c_device_i2c7,
#else
	&s3c_device_mhl_i2c,
#endif
};

void __init exynos5_universal5260_mhl_init(void)
{
	int ret;
#ifdef CONFIG_VIDEO_MHL_SII8246
	sii8246_cfg_gpio();
#ifdef USE_HW_I2C
	/*Register i2c specific device on i2c.num bus*/
	s3c_i2c7_set_platdata(&mhl_i2c7_data);
#endif
	i2c_devs_sii8246[0].irq = gpio_to_irq(GPIO_MHL_INT);
	ret = i2c_register_board_info(I2C_BUS_ID_MHL, i2c_devs_sii8246,
		ARRAY_SIZE(i2c_devs_sii8246));

	platform_add_devices(universal5260_mhl_device,
			ARRAY_SIZE(universal5260_mhl_device));

	pr_info("sii8246: %s is called ret=%d\n", __func__, ret);
#else
	pr_info("sii8246: %s is not executed\n", __func__);
#endif
}
