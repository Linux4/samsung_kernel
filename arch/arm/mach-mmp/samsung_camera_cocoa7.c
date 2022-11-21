/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * Created for samsung by Vincent Wan <zswan@marvell.com>,2012/03/31
 * Created for samsung harrison project based on pxa986, by Vincent Wan, 2013/02/17
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/i2c/ft5306_touch.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/mfd/88pm80x.h>
#include <linux/regulator/machine.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdhci.h>
#include <linux/platform_data/pxa_sdhci.h>
#include <linux/sd8x_rfkill.h>
#include <linux/regmap.h>
#include <linux/mfd/88pm80x.h>
#include <linux/platform_data/mv_usb.h>
#include <linux/pm_qos.h>
#include <linux/clk.h>
#include <linux/lps331ap.h>
#include <linux/spi/pxa2xx_spi.h>
#include <linux/workqueue.h>
#include <linux/i2c-gpio.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#endif

#include <asm/hardware/gic.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/setup.h>

#include <mach/addr-map.h>
#include <mach/clock-pxa988.h>
#include <mach/mmp_device.h>
#include <mach/mfp-pxa986-cocoa7.h>
#include <mach/irqs.h>
#include <mach/isl29043.h>
#include <mach/pxa988.h>
#include <mach/soc_coda7542.h>
#include <mach/regs-rtc.h>
#include <mach/regs-ciu.h>
#include <plat/pxa27x_keypad.h>
#include <plat/pm.h>
#include <media/soc_camera.h>
#include <mach/isp_dev.h>
#include <mach/gpio-edge.h>
#include <mach/samsung_camera_cocoa7.h>
#include "common.h"

#define BUS_MIPI 0
#define BUS_PARALLEL 1

/* Camera sensor PowerDowN pins */
static int pwd_main_en, pwd_main_rst;
static int pwd_sub_en, pwd_sub_rst, SUB_EN_CAM_1dot8V, SUB_EN_VT_1dot8V, SUB_EN_VT_2dot8V;
static int switch_mclk_gpio;
static struct regulator *V_3M_CAM_CORE_1dot2V_BUCK5;
static struct regulator *V_CAM_IO_1dot8V_LDO11;
static struct regulator *V_3M_A2dot8V_LDO8;

int vcamio, vcamavdd, vcamcore;
/* ONLY for parallel camera, SSG never use it currently, but __MUST__ keep it here */

static void pxa988_SSG_cam_pin_init(struct device *dev, int on)
{

}

struct mmp_cam_pdata mv_cam_data_forssg = {
	.name = "EMEI",
	.clk_enabled = 0,
	.dma_burst = 64,
	.mipi_enabled = 0,
	.mclk_min = 24,
	.mclk_src = 3,
	.mclk_div = 13,
	.init_pin = pxa988_SSG_cam_pin_init,
	.bus_type = V4L2_MBUS_CSI2_LANES,//should be re-checked in sensor drivers by Vincent.
	.dphy = {0x0804, 0x33, 0x0900},
	.lane = 1,
	.dphy3_algo = 0,

};

/* ONLY for SSG S5K camera, __MUST__ keep it here */
struct s5k_cam_mclk s5k_cam_mclk_en ;
#define MCLK_PIN 0
#define GPIO_PIN 1
static void switch_mclk_gpio_mfp(int pinid) {

	unsigned int val;

	unsigned long mclk_mfps[] = {
		GPIO077_CAM_MCLK | MFP_LPM_DRIVE_LOW,
	};

	unsigned long gpio_mfps[] = {
		GPIO077_GPIO_77
	};

	switch_mclk_gpio = mfp_to_gpio(GPIO077_GPIO_77);

	if (pinid == MCLK_PIN) {
			mfp_config(ARRAY_AND_SIZE(mclk_mfps));
			val = mfp_read(switch_mclk_gpio);
			printk("--------MCLK pin Switchs TO mclk,  MFP Setting is :0x%x---------", val);
	} if (pinid == GPIO_PIN) {
			mfp_config(ARRAY_AND_SIZE(gpio_mfps));
			val = mfp_read(switch_mclk_gpio);
			printk("--------MCLK pin Switchs TO gpio,  MFP Setting is :0x%x---------", val);

			if (switch_mclk_gpio && gpio_request(switch_mclk_gpio, "switch_mclk_gpio77")) {
			printk(KERN_ERR "------switch_mclk_gpio_mfp-------Request GPIO failed,"
					"gpio: %d\n", switch_mclk_gpio);
			switch_mclk_gpio = 0;
			return -1;;
	}
		gpio_direction_output(switch_mclk_gpio, 0);

		gpio_free(switch_mclk_gpio);

	}

	return;

}

static void pxa_ccic_enable_mclk(struct mmp_camera_dev *pcdev, unsigned int bus_type)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	int ctrl1 = 0;

	switch_mclk_gpio_mfp(MCLK_PIN);

	if (bus_type == BUS_MIPI)
		pdata->bus_type == V4L2_MBUS_CSI2_LANES;
	else if  (bus_type == BUS_PARALLEL)
		pdata->bus_type == 0x0;

	pdata->enable_clk(dev, 1);
	ccic_reg_write(pcdev, REG_CLKCTRL,
			(pdata->mclk_src << 29) | pdata->mclk_div);
	dev_info(dev, "camera: set sensor mclk = %d MHz\n", pdata->mclk_min);

	switch (pdata->dma_burst) {
	case 128:
		ctrl1 = C1_DMAB128;
		break;
	case 256:
		ctrl1 = C1_DMAB256;
		break;
	}
	ccic_reg_write(pcdev, REG_CTRL1, ctrl1 | C1_RESERVED | C1_DMAPOSTED);
	if (pdata->bus_type != V4L2_MBUS_CSI2_LANES)
		ccic_reg_write(pcdev, REG_CTRL3, 0x4);
}

static void pxa_ccic_disable_mclk(struct mmp_camera_dev *pcdev, unsigned int bus_type)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;

	if (bus_type == BUS_MIPI)
		pdata->bus_type == V4L2_MBUS_CSI2_LANES;
	else if  (bus_type == BUS_PARALLEL)
		pdata->bus_type == 0x0;

	ccic_reg_write(pcdev, REG_CLKCTRL, 0x0);
	/*
	 * Bit[5:1] reserved and should not be changed
	 */
	ccic_reg_write(pcdev, REG_CTRL1, C1_RESERVED);
	pdata->enable_clk(&pcdev->pdev->dev, 0);

	switch_mclk_gpio_mfp(GPIO_PIN);
}

static int sr200pc20m_power(struct device *dev, int flag)
{
	if (!V_3M_CAM_CORE_1dot2V_BUCK5) {
		V_3M_CAM_CORE_1dot2V_BUCK5 = regulator_get(dev, "v_cam_c");
		if (IS_ERR(V_3M_CAM_CORE_1dot2V_BUCK5)) {
			V_3M_CAM_CORE_1dot2V_BUCK5 = NULL;
			pr_err(KERN_ERR "Enable V_3M_CAM_CORE_1dot2V_BUCK5 failed!\n");
			return -EIO;
		}
	}

	vcamcore = regulator_get_voltage(V_3M_CAM_CORE_1dot2V_BUCK5);
	pr_err(KERN_ERR "*************************************** jcjung vcamcore = %d **************************************\n", vcamcore);

	if (!V_CAM_IO_1dot8V_LDO11) {
		V_CAM_IO_1dot8V_LDO11 = regulator_get(dev, "v_cam_io");
		if (IS_ERR(V_CAM_IO_1dot8V_LDO11)) {
			V_CAM_IO_1dot8V_LDO11 = NULL;
			pr_err(KERN_ERR "Enable V_CAM_IO_1dot8V_LDO11 failed!\n");
			return -EIO;
		}
	}
	
	vcamio = regulator_get_voltage(V_CAM_IO_1dot8V_LDO11);
	pr_err(KERN_ERR "*************************************** jcjung vcamio = %d **************************************\n", vcamio);
	
	if (!V_3M_A2dot8V_LDO8) {
		V_3M_A2dot8V_LDO8 = regulator_get(dev, "v_ldo8");
		if (IS_ERR(V_3M_A2dot8V_LDO8)) {
			V_3M_A2dot8V_LDO8 = NULL;
			pr_err(KERN_ERR "Enable V_3M_A2dot8V_LDO8 failed!\n");
			return -EIO;
		}
	vcamavdd = regulator_get_voltage(V_3M_A2dot8V_LDO8);
	pr_err(KERN_ERR "*************************************** jcjung vcamavdd = %d **************************************\n", vcamavdd);
	}

	if (flag) {

		Cam_Printk("---sr200pc20m power ON -----the (1) step-----camera: VT Enable OFF-----------\n");
		gpio_direction_output(pwd_sub_en, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr200pc20m power ON -----the (2) step-----camera: VT Reset OFF-----------\n");
		gpio_direction_output(pwd_sub_rst, 0);	/* disable */
		msleep(10);

		Cam_Printk("---sr200pc20m power ON -----the (3) step-----camera: LDO ON-----------\n");
		Cam_Printk("---sr200pc20m power ON -----the (4) step-----camera: Sensor I/O (1.85V->1.80V) ON-----------\n");
		regulator_set_voltage(V_CAM_IO_1dot8V_LDO11, 1800000, 1800000);
		regulator_enable(V_CAM_IO_1dot8V_LDO11);
		msleep(2);

		Cam_Printk("---sr200pc20m power ON -----the (5) step-----camera: Sensor AVDD ON-----------\n");
		regulator_set_voltage(V_3M_A2dot8V_LDO8, 2800000, 2800000);
		regulator_enable(V_3M_A2dot8V_LDO8);
		msleep(2);

		Cam_Printk("---sr200pc20m power ON -----the (6) step-----camera: VT Sensor Core ON-----------\n");
		gpio_direction_output(SUB_EN_VT_1dot8V, 1);	/* enable */
		msleep(2);

		Cam_Printk("---sr200pc20m power ON -----the (7) step-----camera: Main Sensor Core ON 1.80V-----------\n");	
		regulator_set_voltage(V_3M_CAM_CORE_1dot2V_BUCK5, 1800000, 1800000);
		vcamcore = regulator_get_voltage(V_3M_CAM_CORE_1dot2V_BUCK5);
		pr_err(KERN_ERR "*************************************** jcjung vcamcore = %d **************************************\n", vcamcore);
		regulator_enable(V_3M_CAM_CORE_1dot2V_BUCK5);
		msleep(2);
		
		Cam_Printk("---sr200pc20m power ON -----the (8) step-----camera: set sensor mclk = 24 MHz-----------\n");
		pxa_ccic_enable_mclk(s5k_cam_mclk_en.pcdev, BUS_MIPI);
		msleep(2);
		
		Cam_Printk("---sr200pc20m power ON -----the (9) step-----camera: standby ON-----------\n");
		gpio_direction_output(pwd_main_en, 1);	/* enable */
		msleep(10);
		
		Cam_Printk(" ---sr200pc20m power ON -----the (10) step-----camera: reset ON-----------\n");		
		gpio_direction_output(pwd_main_rst, 0);	/* disable */
		msleep(20);
		gpio_direction_output(pwd_main_rst, 1);	/* enable */

		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		s5k_cam_mclk_en.i2c_pxa_reset(s5k_cam_mclk_en.i2c);

		vcamio = regulator_get_voltage(V_CAM_IO_1dot8V_LDO11);
		pr_err(KERN_ERR "*************************************** jcjung vcamio = %d **************************************\n", vcamio);
		vcamcore = regulator_get_voltage(V_3M_CAM_CORE_1dot2V_BUCK5);
		pr_err(KERN_ERR "*************************************** jcjung vcamcore = %d **************************************\n", vcamcore);
		vcamavdd = regulator_get_voltage(V_3M_A2dot8V_LDO8);
		pr_err(KERN_ERR "*************************************** jcjung vcamavdd = %d **************************************\n", vcamavdd);
		
	}else {

		Cam_Printk("---sr200pc20m power OFF -----the (1) step-----camera: VT Enable OFF-----------\n");
		gpio_direction_output(pwd_sub_en, 0);	/* disable */
		msleep(2);
		
		Cam_Printk("---sr200pc20m power OFF -----the (2) step-----camera: VT Reset OFF-----------\n");
		gpio_direction_output(pwd_sub_rst, 0);	/* disable */
		msleep(10);

		Cam_Printk("---sr200pc20m power OFF -----the (3) step-----camera: reset OFF-----------\n");	
		gpio_direction_output(pwd_main_rst, 0);	/* disable */
		msleep(10);

		Cam_Printk("---sr200pc20m power OFF -----the (4) step-----camera: disable sensor mclk-----------\n");	
		pxa_ccic_disable_mclk(s5k_cam_mclk_en.pcdev, BUS_MIPI); // mipi disable
		msleep(2);

		Cam_Printk("---sr200pc20m power OFF -----the (5) step-----camera: standby OFF-----------\n");	
		gpio_direction_output(pwd_main_en, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr200pc20m power OFF -----the (6) step-----camera: LDO OFF-----------\n");			
		Cam_Printk("---sr200pc20m power OFF -----the (7) step-----camera: Main Sensor Core OFF-----------\n");	
		regulator_disable(V_3M_CAM_CORE_1dot2V_BUCK5);
		msleep(2);

		Cam_Printk("---sr200pc20m power OFF -----the (8) step-----camera: VT Sensor Core OFF-----------\n");	
		gpio_direction_output(SUB_EN_VT_1dot8V, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr200pc20m power OFF -----the (9) step-----camera:  Sensor AVDD OFF-----------\n");	
		regulator_disable(V_3M_A2dot8V_LDO8);
		msleep(2);

		Cam_Printk("---sr200pc20m power OFF -----the (10) step-----camera:  Sensor I/O OFF-----------\n");	
		regulator_disable(V_CAM_IO_1dot8V_LDO11);

	}

	return 0;
}

static int sr130pc10_power(struct device *dev, int flag)
{
	if (!V_CAM_IO_1dot8V_LDO11) {
		V_CAM_IO_1dot8V_LDO11 = regulator_get(dev, "v_cam_io");
		if (IS_ERR(V_CAM_IO_1dot8V_LDO11)) {
			V_CAM_IO_1dot8V_LDO11 = NULL;
			pr_err(KERN_ERR "Enable V_CAM_IO_1dot8V_LDO11 failed!\n");
			return -EIO;
		}
	}

	if (!V_3M_CAM_CORE_1dot2V_BUCK5) {
		V_3M_CAM_CORE_1dot2V_BUCK5 = regulator_get(dev, "v_cam_c");
		if (IS_ERR(V_3M_CAM_CORE_1dot2V_BUCK5)) {
			V_3M_CAM_CORE_1dot2V_BUCK5 = NULL;
			pr_err(KERN_ERR "Enable V_3M_CAM_CORE_1dot2V_BUCK5 failed!\n");
			return -EIO;
		}
	}

	if (flag) {
		Cam_Printk("---sr130pc10_power  power ON -----the (1) step-----camera: Main Enable OFF-----------\n");
		gpio_direction_output(pwd_main_en, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr130pc10_power  power ON -----the (2) step-----camera: Main Reset OFF-----------\n");	
		gpio_direction_output(pwd_main_rst, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr130pc10_power  power ON -----the (3) step-----camera: LDO ON-----------\n");
		Cam_Printk("---sr130pc10_power  power ON -----the (4) step-----camera: Sensor I/O (1.85V->1.80V) ON-----------\n");
		regulator_set_voltage(V_CAM_IO_1dot8V_LDO11, 1850000, 1850000);
		regulator_set_voltage(V_CAM_IO_1dot8V_LDO11, 1800000, 1800000);
		regulator_enable(V_CAM_IO_1dot8V_LDO11);
		msleep(2);

		Cam_Printk("---sr130pc10_power  power ON -----the (5) step-----camera: Sensor AVDD ON-----------\n");
		gpio_direction_output(SUB_EN_VT_2dot8V, 1);	/* enable */
		msleep(2);

		Cam_Printk("---sr130pc10_power  power ON -----the (6) step-----camera: VT Sensor Core ON-----------\n");
		gpio_direction_output(SUB_EN_VT_1dot8V, 1);	/* enable */
		msleep(2);

		Cam_Printk("---sr130pc10_power  power ON -----the (7) step-----camera: Main Sensor Core ON-----------\n");
		regulator_set_voltage(V_3M_CAM_CORE_1dot2V_BUCK5, 1800000, 1800000);
		regulator_enable(V_3M_CAM_CORE_1dot2V_BUCK5);
		msleep(2);

		Cam_Printk("---sr130pc10_power power ON -----the (8) step-----camera: set sensor mclk = 24 MHz-----------\n");
		pxa_ccic_enable_mclk(s5k_cam_mclk_en.pcdev, BUS_PARALLEL);
		msleep(2);

		Cam_Printk("---sr130pc10_power power ON -----the (9) step-----camera: standby ON-----------\n");
		gpio_direction_output(pwd_sub_en, 1);	/* enable */
		msleep(10);
		
		Cam_Printk(" ---sr130pc10_power power ON -----the (10) step-----camera: reset ON-----------\n");		
		gpio_direction_output(pwd_sub_rst, 0);	/* disable */
		msleep(20);
		gpio_direction_output(pwd_sub_rst, 1);	/* enable */

		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		s5k_cam_mclk_en.i2c_pxa_reset(s5k_cam_mclk_en.i2c);
		
	}else {
		Cam_Printk("---sr130pc10_power  power OFF -----the (1) step-----camera: Main Enable OFF-----------\n");
		gpio_direction_output(pwd_main_en, 0);	/* disable */
		msleep(2);
		
		Cam_Printk("---sr130pc10_power  power OFF -----the (2) step-----camera: Main Reset OFF-----------\n");	
		gpio_direction_output(pwd_main_rst, 0);	/* disable */
		msleep(2);
	
		Cam_Printk("---sr130pc10_power power OFF -----the (3) step-----camera: reset OFF-----------\n");	
		gpio_direction_output(pwd_sub_rst, 0);	/* disable */
		msleep(2);

		// mipi disable
		Cam_Printk("---sr130pc10_power power OFF -----the (4) step-----camera: disable sensor mclk-----------\n");	
		pxa_ccic_disable_mclk(s5k_cam_mclk_en.pcdev, BUS_PARALLEL);
		msleep(2);
		
		Cam_Printk("---sr130pc10_power power OFF -----the (5) step-----camera: standby OFF-----------\n");	
		gpio_direction_output(pwd_sub_en, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr130pc10_power power OFF -----the (6) step-----camera: LDO OFF-----------\n");
		Cam_Printk("---sr130pc10_power power OFF -----the (7) step-----camera: Main Sensor Core OFF-----------\n");
		regulator_disable(V_3M_CAM_CORE_1dot2V_BUCK5);
		msleep(2);

		Cam_Printk("---sr130pc10_power power OFF -----the (8) step-----camera: VT Sensor Core OFF-----------\n");
		gpio_direction_output(SUB_EN_VT_1dot8V, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr130pc10_power power OFF -----the (9) step-----camera: Sensor AVDD OFF-----------\n");
		gpio_direction_output(SUB_EN_VT_2dot8V, 0);	/* disable */
		msleep(2);

		Cam_Printk("---sr130pc10_power power OFF -----the (10) step-----camera: Sensor I/O OFF-----------\n");
		regulator_disable(V_CAM_IO_1dot8V_LDO11);	

	}

	return 0;
}

//#if defined(CONFIG_SOC_CAMERA_SR130PC10)
#if 0
#define SENSOR_SDA	mfp_to_gpio(GPIO080_GPIO_80)
#define SENSOR_SCL	mfp_to_gpio(GPIO079_GPIO_79)

static struct i2c_gpio_platform_data i2c_subsensor_bus_data = {
	.sda_pin = SENSOR_SDA,
	.scl_pin = SENSOR_SCL,
	.udelay  = 3,
	.timeout = 100,
};

static struct platform_device i2c_subsensor_bus_device = {
	.name		= "i2c-gpio",
	.id		= 8,
	.dev = {
		.platform_data = &i2c_subsensor_bus_data,
	}
};
#endif

static struct i2c_board_info camera_i2c[] = {
	{
		I2C_BOARD_INFO("sr130pc10", (0x20 >> 1)),
	},
	{
		I2C_BOARD_INFO("samsung_mainsensor", (0x40 >> 1)),
	},
};

static struct soc_camera_link iclink[] = {
	[ID_SR130PC10] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[ID_SR130PC10],
		.i2c_adapter_id		= 0,// i2c gpio bus 8, by vincen wan.
		.power = sr130pc10_power,
		.flags = 0,
		.module_name		= "sr130pc10",
	},
	[ID_SR200PC20M] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[ID_SR200PC20M],
		.i2c_adapter_id		= 0,//i2c bus 0
		.power = sr200pc20m_power,
		.flags = V4L2_MBUS_CSI2_LANES,
		.module_name		= "samsung_mainsensor",
	},
};

static struct platform_device __attribute__((unused)) camera[] = {
	[ID_SR130PC10] = {
		.name	= "soc-camera-pdrv",
		.id	= 0,
		.dev	= {
			.platform_data = &iclink[ID_SR130PC10],
		},
	},
	[ID_SR200PC20M] = {
		.name	= "soc-camera-pdrv",
		.id	= 1,
		.dev	= {
			.platform_data = &iclink[ID_SR200PC20M],
		},
	},
};

static struct platform_device *camera_platform_devices[] = {
#if defined(CONFIG_SOC_CAMERA_SR130PC10)
	&camera[ID_SR130PC10],
#endif
#if defined(CONFIG_SOC_CAMERA_SR200PC20M)
	&camera[ID_SR200PC20M],
#endif
};

static int sr200pc20m_pin_init(void){

	//return 0;//for rev0.0 board, no need tune main sensor.by Vincent Wan.

	pwd_main_en = mfp_to_gpio(MFP_PIN_GPIO14);
	pwd_main_rst = mfp_to_gpio(MFP_PIN_GPIO15);
		
	if( (!pwd_main_en) || (! pwd_main_rst)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: pwd_main_en :%d, pwd_main_rst:%d\n", 
						pwd_main_en, pwd_main_rst);	
		return -1;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (pwd_main_en && gpio_request(pwd_main_en, "CAM_HI_SENSOR_PWD_14")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_main_en);
		pwd_main_en = 0;
		return -1;;
	}
	if (pwd_main_rst && gpio_request(pwd_main_rst, "CAM_HI_SENSOR_PWD_15")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_main_rst);
		pwd_main_rst = 0;
		return -1;;
	}

	return 0;
}

static int sr130pc10_pin_init(void){

	pwd_sub_en = mfp_to_gpio(MFP_PIN_GPIO81);
	pwd_sub_rst = mfp_to_gpio(MFP_PIN_GPIO82);
	SUB_EN_VT_1dot8V = mfp_to_gpio(MFP_PIN_GPIO104);
	SUB_EN_VT_2dot8V = mfp_to_gpio(MFP_PIN_GPIO103);
		
	if( (!pwd_sub_en) || (! pwd_sub_rst) 
						|| (!SUB_EN_VT_1dot8V) || (!SUB_EN_VT_2dot8V)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: pwd_sub_en :%d, pwd_sub_rst:%d, SUB_EN_VT_1dot8V:%d,  SUB_EN_VT_2dot8V:%d\n", 
					pwd_sub_en, pwd_sub_rst, SUB_EN_VT_1dot8V, SUB_EN_VT_2dot8V);	
		return -1;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (pwd_sub_en && gpio_request(pwd_sub_en, "pwd_sub_en_gpio81")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_sub_en);
		pwd_sub_en = 0;
		return -1;;
	}
	if (pwd_sub_rst && gpio_request(pwd_sub_rst, "pwd_sub_rst_gpio82")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_sub_rst);
		pwd_sub_rst = 0;
		return -1;;
	}
	if (SUB_EN_VT_1dot8V && gpio_request(SUB_EN_VT_1dot8V, "pwd_sub_rst_gpio104")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", SUB_EN_VT_1dot8V);
		pwd_sub_rst = 0;
		return -1;;
	}

	if (SUB_EN_VT_2dot8V && gpio_request(SUB_EN_VT_2dot8V, "pwd_sub_rst_gpio103")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", SUB_EN_VT_2dot8V);
		pwd_sub_rst = 0;
		return -1;;
	}
	return 0;
}

void __init init_samsung_cam(void)
{

	int ret;
	Cam_Printk("---camera---call--init_samsung_cam --for init!!!--------\n");
	ret= sr200pc20m_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----sr200pc20m_pin_init --failed!!!--------\n");
		return;
	}
	
	ret= sr130pc10_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----sr130pc10_pin_init --failed!!!--------\n");
		return;
	}

#if defined (CONFIG_SOC_CAMERA_SR130PC10)
	//pr_info("add i2c device: CONFIG_SOC_CAMERA_SR130PC10!!\n");
	//platform_device_register(&i2c_subsensor_bus_device);
	//i2c_register_board_info(4, ARRAY_AND_SIZE(mms136_i2c_devices));
#endif
	platform_add_devices(ARRAY_AND_SIZE(camera_platform_devices));
	pxa988_add_cam(&mv_cam_data_forssg);
}
