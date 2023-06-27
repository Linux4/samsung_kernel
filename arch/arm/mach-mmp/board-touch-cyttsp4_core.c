#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/gpio_event.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <asm/mach-types.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/regulator/machine.h>
#include <mach/devices.h>
#if defined(CONFIG_MACH_BAFFIN)
#include <mach/mfp-pxa988-baffin.h>
#endif
#if defined(CONFIG_MACH_WILCOX)
#include <mach/mfp-pxa1088-wilcox.h>
#elif defined(CONFIG_MACH_DEGAS)
#include <mach/mfp-pxa1088-degas.h>
#elif defined(CONFIG_MACH_CS05)
#include <mach/mfp-pxa1088-cs05.h>
#elif defined(CONFIG_MACH_BAFFINQ)
#include <mach/mfp-pxa1088-baffinq.h>
#endif
#include "common.h"

/* cyttsp */
#include <linux/cyttsp4_bus.h>
#include <linux/cyttsp4_core.h>
#include <linux/cyttsp4_btn.h>
#include <linux/cyttsp4_mt.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>


/*
  KEVI added + : 001-86666
  There are four buttons physically. One logical button consists of two physical button.
  One logical button has dummy button and real button. When dummy button was detected,
  this event will be ignored by driver.
  Enable BUTTON_PALM_REJECTION if this is TSG4.PANSAM.LT02 project. (cyttsp4_regs.h)
*/


#define CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_INCLUDE_FW

#ifdef CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP4_INCLUDE_FW
#if defined(CONFIG_MACH_BAFFIN)
#include "cyttsp4_img_baffinRD.h"
#elif defined(CONFIG_MACH_CS05)
#include "cyttsp4_img_cs05.h"
#else
#include "cyttsp4_img.h"
#endif
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = cyttsp4_img,
	.size = ARRAY_SIZE(cyttsp4_img),
	.ver = cyttsp4_ver,
	.vsize = ARRAY_SIZE(cyttsp4_ver),
};
#else
static struct cyttsp4_touch_firmware cyttsp4_firmware = {
	.img = NULL,
	.size = 0,
	.ver = NULL,
	.vsize = 0,
};
#endif

#define CYTTSP4_USE_I2C

#ifdef CYTTSP4_USE_I2C
#define CYTTSP4_I2C_NAME "cyttsp4_i2c_adapter"
#define CYTTSP4_I2C_TCH_ADR 0x24
#define CYTTSP4_LDR_TCH_ADR 0x24

//#define TMA400_GPIO_TSP_INT   2
//#define CYTTSP4_I2C_IRQ_GPIO 2 /* J6.9, C19, GPMC_AD14/GPIO_38 */
//#define CYTTSP4_I2C_RST_GPIO 14 /* J6.10, D18, GPMC_AD13/GPIO_37 */

#define TSP_SDA	mfp_to_gpio(GPIO088_GPIO_88)
#define TSP_SCL	mfp_to_gpio(GPIO087_GPIO_87)
#define TSP_INT	mfp_to_gpio(GPIO094_GPIO_94)
#define KEY_LED_GPIO mfp_to_gpio(GPIO096_GPIO_96)
#endif

#define CY_MAXX 540
#define CY_MAXY 960
#define CY_MINX 0
#define CY_MINY 0

#define CY_ABS_MIN_X CY_MINX
#define CY_ABS_MIN_Y CY_MINY
#define CY_ABS_MAX_X CY_MAXX
#define CY_ABS_MAX_Y CY_MAXY
#define CY_ABS_MIN_P 0
#define CY_ABS_MAX_P 255
#define CY_ABS_MIN_W 0
#define CY_ABS_MAX_W 255

#define CY_ABS_MIN_T 0

#define CY_ABS_MAX_T 15

#define CY_IGNORE_VALUE 0xFFFF

#define  NLSX4373_EN_GPIO    14

#define TSP_DEBUG_OPTION 0
int touch_debug_option = TSP_DEBUG_OPTION;

#define	CAPRI_TSP_DEBUG(fmt, args...)	\
	if (touch_debug_option)	\
		printk("[TSP %s:%4d] " fmt, \
		__func__, __LINE__, ## args);

static struct regulator *tsp_regulator_3_3=NULL;
static struct regulator *tsp_regulator_1_8=NULL;

extern unsigned int system_rev;

unsigned long tsp_int_suspend_mfpr[] = {
	GPIO094_GPIO_94 | MFP_PULL_LOW
	};

unsigned long tsp_int_wakeup_mfpr[] = {
	GPIO094_GPIO_94 | MFP_PULL_HIGH
	};

// for TSP i2c operation (switch gpio or hw i2c func)
void i2c1_pin_changed(int gpio)
{
	unsigned long i2c1_g_mfps[] = {
		GPIO087_GPIO_87 | MFP_PULL_FLOAT,		/* SCL */
		GPIO088_GPIO_88 | MFP_PULL_FLOAT,		/* SDA */
	};

	unsigned long i2c1_hw_mfps[] = {
		GPIO087_CI2C_SCL_2,		/* SCL */
		GPIO088_CI2C_SDA_2,		/* SDA */
	};

        usleep_range(5000, 8000);

	if(gpio)
	{
		mfp_config(ARRAY_AND_SIZE(i2c1_g_mfps));
        	gpio_direction_input(MFP_PIN_GPIO87);
        	gpio_direction_input(MFP_PIN_GPIO88);
	}
	else
		mfp_config(ARRAY_AND_SIZE(i2c1_hw_mfps));

        usleep_range(5000, 8000);

	return;
}

int cyttsp4_hw_power(int on, int use_irq, int irq_gpio)
{
	int ret = 0;
	static u8 is_power_on = 0;

	CAPRI_TSP_DEBUG(" %s set %d \n", __func__, on);

	if (tsp_regulator_3_3 == NULL){
		tsp_regulator_3_3 = regulator_get(NULL, "v_tsp_3v3");
		if(IS_ERR(tsp_regulator_3_3)){
			tsp_regulator_3_3 = NULL;
			printk("get touch_regulator_3v3 regulator error\n");
			goto exit;
		}
	}
#if defined(CONFIG_MACH_BAFFIN)	
	if (tsp_regulator_1_8 == NULL){
		tsp_regulator_1_8 = regulator_get(NULL, "v_tsp_1v8");
		if(IS_ERR(tsp_regulator_1_8)){
			tsp_regulator_1_8 = NULL;
			printk("get touch_regulator_1v8 regulator error\n");
			goto exit;
		}
	}
#endif

	if (on == 1) {
		if (!is_power_on){
			is_power_on = 1;
			#if defined(CONFIG_CS05_BD_02)
			regulator_set_voltage(tsp_regulator_3_3, 2900000, 2900000);
			#else
			regulator_set_voltage(tsp_regulator_3_3, 2850000, 2850000);
			#endif

			ret = regulator_enable(tsp_regulator_3_3);
			if (ret) {
				is_power_on = 0;
				pr_err("can not enable TSP AVDD 3.3V, ret=%d\n", ret);
				goto exit;
			}
			#if defined(CONFIG_MACH_BAFFIN)
			regulator_set_voltage(tsp_regulator_1_8, 1800000, 1800000);

			ret = regulator_enable(tsp_regulator_1_8);
			if (ret) {
				is_power_on = 0;
				pr_err("can not enable TSP AVDD 1.8V, ret=%d\n", ret);
				goto exit;
			}
			#endif
                        mfp_config(ARRAY_AND_SIZE(tsp_int_wakeup_mfpr));

	                /* Delay for 10 msec */
                        msleep(10);
			i2c1_pin_changed(0);		
		}

		/* Enable the IRQ */
		/*	if (use_irq) {
			enable_irq(gpio_to_irq(irq_gpio));
			pr_debug("Enabled IRQ %d for TSP\n", gpio_to_irq(irq_gpio));
	        }*/
	} else if (on == 0) {
		/* Disable the IRQ */
		/*if (use_irq) {
			pr_debug("Disabling IRQ %d for TSP\n", gpio_to_irq(irq_gpio));
			disable_irq_nosync(gpio_to_irq(irq_gpio));
		}*/
		if (is_power_on)  {
			is_power_on = 0;
			i2c1_pin_changed(1);
			ret = regulator_disable(tsp_regulator_3_3);
			if (ret) {
				is_power_on = 1;
				pr_err("can not disable TSP AVDD 3.3V, ret=%d\n", ret);
				goto exit;
			}
			#if defined(CONFIG_MACH_BAFFIN)
			ret = regulator_disable(tsp_regulator_1_8);
			if (ret) {
				is_power_on = 1;
				pr_err("can not disable TSP AVDD 1.8V, ret=%d\n", ret);
				goto exit;
			}
			#endif
                        mfp_config(ARRAY_AND_SIZE(tsp_int_suspend_mfpr));

                        /* Delay for 100 msec */
                        msleep(100);
		}
	} else {  /* on == 2 */
		if (is_power_on)  {
		is_power_on = 0;
		//i2c1_pin_changed(1); because, CS05 model' lcd and tsp use same i2c line.
			ret = regulator_disable(tsp_regulator_3_3);
			if (ret) {
				is_power_on = 1;
				pr_err("can not disable TSP AVDD 3.3V, ret=%d\n", ret);
				goto exit;
			}
			#if defined(CONFIG_MACH_BAFFIN)
			ret = regulator_disable(tsp_regulator_1_8);
			if (ret) {
				is_power_on = 1;
				pr_err("can not disable TSP AVDD 1.8V, ret=%d\n", ret);
				goto exit;
			}
			#endif
                        mfp_config(ARRAY_AND_SIZE(tsp_int_suspend_mfpr));

                        /* Delay for 100 msec */
                        msleep(100);
		}
	}

exit:
	return ret;
}

static int cyttsp4_xres(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;

	cyttsp4_hw_power(0, true, irq_gpio);

	/* Delay for 10 msec */
	//msleep(1000);

	cyttsp4_hw_power(1, true, irq_gpio);

	return rc;
}

static int cyttsp4_init(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev)
{
	int irq_gpio = pdata->irq_gpio;
	int rc = 0;
#if 0
	if (on) {
		rc = gpio_request(irq_gpio, NULL);
		if (rc < 0) {
			gpio_free(irq_gpio);
			rc = gpio_request(irq_gpio, NULL);
		}
		if (rc < 0)
  			dev_err(dev, "%s: Fail request gpio=%d\n",
 					__func__, irq_gpio);
		else
			gpio_direction_input(irq_gpio);

		cyttsp4_hw_power(1, false, 0);
	} else {
		cyttsp4_hw_power(0, false, 0);
		gpio_free(irq_gpio);
	}

#else
	if (on)
		cyttsp4_hw_power(1, false, 0);
	else
		cyttsp4_hw_power(0, false, 0);
#endif
	dev_info(dev, "%s: INIT CYTTSP IRQ gpio=%d r=%d\n",
			__func__, irq_gpio, rc);

	return rc;
}

static int cyttsp4_wakeup(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	int irq_gpio = pdata->irq_gpio;

	return cyttsp4_hw_power(1, true, irq_gpio);
}

static int cyttsp4_sleep(struct cyttsp4_core_platform_data *pdata,
		struct device *dev, atomic_t *ignore_irq)
{
	int irq_gpio = pdata->irq_gpio;

#if defined(CONFIG_MACH_CS05)
	return cyttsp4_hw_power(2, true, irq_gpio);
#else
	return cyttsp4_hw_power(0, true, irq_gpio);
#endif
}

static int cyttsp4_power(struct cyttsp4_core_platform_data *pdata,
		int on, struct device *dev, atomic_t *ignore_irq)
{
	if (on)
		return cyttsp4_wakeup(pdata, dev, ignore_irq);

	return cyttsp4_sleep(pdata, dev, ignore_irq);
}

static int cyttsp4_irq_stat(struct cyttsp4_core_platform_data *pdata,
		struct device *dev)
{

	return gpio_get_value(pdata->irq_gpio);
}

static int cyttsp4_led_power_onoff (int on)
{
#if defined(CONFIG_MACH_BAFFIN)
	if (system_rev == 1)
		return 0;
#endif

	if(on == 1)
		gpio_direction_output(KEY_LED_GPIO, 1);
	else
		gpio_direction_output(KEY_LED_GPIO, 0);
	
	return 0;
}

#ifdef BUTTON_PALM_REJECTION
static u16 cyttsp4_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_MENU,		/* 139 */
	KEY_MENU,		/* 139 */
	KEY_BACK,		/* 158 */
	KEY_BACK,		/* 158 */
};
#else
/* Button to keycode conversion */
static u16 cyttsp4_btn_keys[] = {
	/* use this table to map buttons to keycodes (see input.h) */
	KEY_MENU,		/* 139 */
	KEY_BACK,		/* 158 */
};
#endif	// BUTTON_PALM_REJECTION

static struct touch_settings cyttsp4_sett_btn_keys = {
	.data = (uint8_t *)&cyttsp4_btn_keys[0],
	.size = ARRAY_SIZE(cyttsp4_btn_keys),
	.tag = 0,
};

static struct cyttsp4_core_platform_data _cyttsp4_core_platform_data = {
	.irq_gpio = TSP_INT,
//	.rst_gpio = CYTTSP4_I2C_RST_GPIO,
	.xres = cyttsp4_xres,
	.init = cyttsp4_init,
	.power = cyttsp4_power,
	.irq_stat = cyttsp4_irq_stat,
	.led_power = cyttsp4_led_power_onoff,
	.sett = {
		NULL,	/* Reserved */
		NULL,	/* Command Registers */
		NULL,	/* Touch Report */
		NULL,	/* Cypress Data Record */
		NULL,	/* Test Record */
		NULL,	/* Panel Configuration Record */
		NULL, /* &cyttsp4_sett_param_regs, */
		NULL, /* &cyttsp4_sett_param_size, */
		NULL,	/* Reserved */
		NULL,	/* Reserved */
		NULL,	/* Operational Configuration Record */
		NULL, /* &cyttsp4_sett_ddata, *//* Design Data Record */
		NULL, /* &cyttsp4_sett_mdata, *//* Manufacturing Data Record */
		NULL,	/* Config and Test Registers */
		&cyttsp4_sett_btn_keys,	/* button-to-keycode table */
	},
	.fw = &cyttsp4_firmware,
};

/*
static struct cyttsp4_core cyttsp4_core_device = {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
	.dev = {
		.platform_data = &_cyttsp4_core_platform_data,
	},
};
*/

static struct cyttsp4_core_info cyttsp4_core_info __initdata= {
	.name = CYTTSP4_CORE_NAME,
	.id = "main_ttsp_core",
	.adap_id = CYTTSP4_I2C_NAME,
       .platform_data = &_cyttsp4_core_platform_data,
};




static const uint16_t cyttsp4_abs[] = {
	ABS_MT_POSITION_X, CY_ABS_MIN_X, CY_ABS_MAX_X, 0, 0,
	ABS_MT_POSITION_Y, CY_ABS_MIN_Y, CY_ABS_MAX_Y, 0, 0,
	ABS_MT_PRESSURE, CY_ABS_MIN_P, CY_ABS_MAX_P, 0, 0,
	CY_IGNORE_VALUE, CY_ABS_MIN_W, CY_ABS_MAX_W, 0, 0,
	ABS_MT_TRACKING_ID, CY_ABS_MIN_T, CY_ABS_MAX_T, 0, 0,
	ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0,
	ABS_MT_TOUCH_MINOR, 0, 255, 0, 0,
	ABS_MT_ORIENTATION, -128, 127, 0, 0,
};

struct touch_framework cyttsp4_framework = {
	.abs = (uint16_t *)&cyttsp4_abs[0],
	.size = ARRAY_SIZE(cyttsp4_abs),
	.enable_vkeys = 0,
};

static struct cyttsp4_mt_platform_data _cyttsp4_mt_platform_data = {
	.frmwrk = &cyttsp4_framework,
	.flags = 0x00,
	.inp_dev_name = CYTTSP4_MT_NAME,
};

/*
struct cyttsp4_device cyttsp4_mt_device = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
	.dev = {
		.platform_data = &_cyttsp4_mt_platform_data,
	}
};
*/

struct cyttsp4_device_info cyttsp4_mt_info __initdata = {
	.name = CYTTSP4_MT_NAME,
	.core_id = "main_ttsp_core",
      .platform_data = &_cyttsp4_mt_platform_data,
};

static struct cyttsp4_btn_platform_data _cyttsp4_btn_platform_data = {
	.inp_dev_name = CYTTSP4_BTN_NAME,
};

/*
struct cyttsp4_device cyttsp4_btn_device = {
	.name = CYTTSP4_BTN_NAME,
	.core_id = "main_ttsp_core",
	.dev = {
		.platform_data = &_cyttsp4_btn_platform_data,
	}
};
*/

struct cyttsp4_device_info cyttsp4_btn_info __initdata = {
	.name = CYTTSP4_BTN_NAME,
	.core_id = "main_ttsp_core",
      .platform_data = &_cyttsp4_btn_platform_data,
};

#ifdef CYTTSP4_VIRTUAL_KEYS
static ssize_t cyttps4_virtualkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
		__stringify(EV_KEY) ":"
		__stringify(KEY_BACK) ":1360:90:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_MENU) ":1360:270:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_HOME) ":1360:450:160:180"
		":" __stringify(EV_KEY) ":"
		__stringify(KEY_SEARCH) ":1360:630:160:180"
		"\n");
}

static struct kobj_attribute cyttsp4_virtualkeys_attr = {
	.attr = {
		.name = "virtualkeys.cyttsp4_mt",
		.mode = S_IRUGO,
	},
	.show = &cyttps4_virtualkeys_show,
};

static struct attribute *cyttsp4_properties_attrs[] = {
	&cyttsp4_virtualkeys_attr.attr,
	NULL
};

static struct attribute_group cyttsp4_properties_attr_group = {
	.attrs = cyttsp4_properties_attrs,
};
#endif
void __init board_tsp_init(void)
{
	int ret = 0;

	printk("[TSP] board_tsp_init\n");

	ret = gpio_request(TSP_INT, "cyttsp4_tsp_int");
	if(!ret)
		gpio_direction_input(TSP_INT);
	else
		printk("gpio request fail!\n");
	
	if(gpio_request(MFP_PIN_GPIO87, "cyttsp4_tsp_scl")){
		printk(KERN_ERR "Request GPIO failed," "gpio: %d \n", TSP_SCL);
	}
	if(gpio_request(MFP_PIN_GPIO88, "cyttsp4_tsp_sda")){
		printk(KERN_ERR "Request GPIO failed," "gpio: %d \n", TSP_SDA);
	}
	if (gpio_request(KEY_LED_GPIO, "key_led_gpio")) {
		printk(KERN_ERR "Request GPIO failed," "gpio: %d \n", KEY_LED_GPIO);
	}
	
	cyttsp4_register_core_device(&cyttsp4_core_info);
	cyttsp4_register_device(&cyttsp4_mt_info);
	cyttsp4_register_device(&cyttsp4_btn_info);
}
