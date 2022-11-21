/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * Created for samsung by Vincent Wan <zswan@marvell.com>,2012/03/31
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
#ifdef CONFIG_SOC_CAMERA_SR030PC50
#include <mach/mfp-pxa986-hendrix.h>
#else
#include <mach/mfp-pxa988-aruba.h>
#endif
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
#include <mach/samsung_camera.h>

#ifdef CONFIG_SOC_CAMERA_SR030PC50
/* Camera sensor PowerDowN pins */
static int pwd_main1, pwd_main2;
#else
struct class *camera_class; // AT_Command : Flash Mode

/* Camera sensor PowerDowN pins */
int camera_power_standby, camera_power_reset, pwd_sub2,pwd_sub1;
int camera_flash_en, camera_flash_set;
#endif

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
	.bus_type = V4L2_MBUS_CSI2_LANES,
	.dphy = {0x0a06, 0x33, 0x0900},
	.lane = 2,
	.dphy3_algo = 0,

};
	
/* ONLY for SSG S5K camera, __MUST__ keep it here */
struct s5k_cam_mclk s5k_cam_mclk_en ;

static int s5k43_power(struct device *dev, int flag)
{
#ifndef CONFIG_SOC_CAMERA_SR030PC50
        static struct regulator *vcamera;
#endif
	static struct regulator *vcamera_ana;
	static struct regulator *vcamera_ana_1;
	static struct regulator *vcamera_vbuck5;

	if (!vcamera_vbuck5) {
		vcamera_vbuck5 = regulator_get(dev, "v_cam_c");
		if (IS_ERR(vcamera_vbuck5)) {
			vcamera_vbuck5 = NULL;
			pr_err(KERN_ERR "Enable vcamera_vbuck5 failed!\n");
			return -EIO;
		}
	}

	if (!vcamera_ana) {
		vcamera_ana = regulator_get(dev, "v_cam_io");
		if (IS_ERR(vcamera_ana)) {
			vcamera_ana = NULL;
			pr_err(KERN_ERR "Enable vcamera_ana failed!\n");
			return -EIO;
		}
	}

	if (!vcamera_ana_1) {
		vcamera_ana_1 = regulator_get(dev, "v_cam_avdd");
		if (IS_ERR(vcamera_ana_1)) {
			vcamera_ana_1 = NULL;
			pr_err(KERN_ERR "Enable vcamera_ana_1 failed!\n");
			return -EIO;
		}
	}

#ifndef CONFIG_SOC_CAMERA_SR030PC50
	if (!vcamera) {
		vcamera = regulator_get(dev, "v_cam_af");
		if (IS_ERR(vcamera)) {
			vcamera = NULL;
			pr_err(KERN_ERR "Enable vcamera failed!\n");
			return -EIO;
		}
	}
#endif
	if (flag) {
		Cam_Printk("---camera power ON -----the (1) step-----camera: LDO ON-----------\n");

		regulator_set_voltage(vcamera_vbuck5, 1200000, 1200000);
		regulator_enable(vcamera_vbuck5);

		regulator_set_voltage(vcamera_ana, 1800000, 1800000);
		regulator_enable(vcamera_ana);

		regulator_set_voltage(vcamera_ana_1, 2800000, 2800000);
		regulator_enable(vcamera_ana_1);
#ifndef CONFIG_SOC_CAMERA_SR030PC50
		regulator_set_voltage(vcamera, 2800000, 2800000);
		regulator_enable(vcamera);
#endif
		Cam_Printk("---camera power ON -----the (2) step-----camera: set sensor mclk = 24 MHz-----------\n");
		s5k_cam_mclk_en.enable_clk(s5k_cam_mclk_en.pcdev);

		Cam_Printk("---camera power ON -----the (3) step-----camera: standby ON-----------\n");
#ifdef CONFIG_SOC_CAMERA_SR030PC50
		gpio_direction_output(pwd_main1, 1);	/* enable */

		Cam_Printk(" ---camera power ON -----the (4) step-----camera: reset ON-----------\n");		
		gpio_direction_output(pwd_main2, 0);	/* enable */
		msleep(2);
		gpio_direction_output(pwd_main2, 1);	/* enable */
#else
		gpio_direction_output(camera_power_standby, 1);	/* enable */

		Cam_Printk(" ---camera power ON -----the (4) step-----camera: reset ON-----------\n");		
		gpio_direction_output(camera_power_reset, 0);	/* enable */
		msleep(2);
		gpio_direction_output(camera_power_reset, 1);	/* enable */
#endif
		/*for s5k power off maybe pull down the data pin, so we have to reset i2c controller */
		s5k_cam_mclk_en.i2c_pxa_reset(s5k_cam_mclk_en.i2c);
	}
	else 
	{
#ifdef CONFIG_SOC_CAMERA_SR030PC50
		Cam_Printk("---camera power OFF -----the (1) step-----camera: reset OFF-----------\n");	
		gpio_direction_output(pwd_main2, 0);	/* disable */

		// mipi disable
		Cam_Printk("---camera power OFF -----the (2) step-----camera: disable sensor mclk-----------\n");	
		s5k_cam_mclk_en.disable_clk(s5k_cam_mclk_en.pcdev);
		
		Cam_Printk("---camera power OFF -----the (3) step-----camera: standby OFF-----------\n");	
		gpio_direction_output(pwd_main1, 0);	/* disable */
#else
		Cam_Printk("---camera power OFF -----the (1) step-----camera: reset OFF-----------\n");	
		gpio_direction_output(camera_power_reset, 0);	/* disable */

		// mipi disable
		Cam_Printk("---camera power OFF -----the (2) step-----camera: disable sensor mclk-----------\n");	
		s5k_cam_mclk_en.disable_clk(s5k_cam_mclk_en.pcdev);

		Cam_Printk("---camera power OFF -----the (3) step-----camera: standby OFF-----------\n");	
		gpio_direction_output(camera_power_standby, 0);	/* disable */
#endif
		Cam_Printk("---camera power OFF -----the (4) step-----camera: LDO OFF-----------\n");	
		regulator_disable(vcamera_ana_1);
		regulator_disable(vcamera_ana);
		regulator_disable(vcamera_vbuck5);
#ifndef CONFIG_SOC_CAMERA_SR030PC50
		regulator_disable(vcamera);
#endif
	}

	return 0;
}

static struct i2c_board_info camera_i2c[] = {
	{
		I2C_BOARD_INFO("s5k4ecgx", (0xac >> 1)),
	},
};

static struct soc_camera_link iclink[] = {
	[ID_S5K4ECGX] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[ID_S5K4ECGX],
		.i2c_adapter_id		= 0,
		.power = s5k43_power,
		.flags = V4L2_MBUS_CSI2_LANES,
		.module_name		= "s5k4ecgx",
	},
};

static struct platform_device __attribute__((unused)) camera[] = {
	[ID_S5K4ECGX] = {
		.name	= "soc-camera-pdrv",
		.id	= 0,
		.dev	= {
			.platform_data = &iclink[ID_S5K4ECGX],
		},
	},
};

#ifndef CONFIG_SOC_CAMERA_SR030PC50
/*++ AT_Command[Flash mode] : dhee79.lee@samsung.com_20121012 ++*/
void camera_flash_on_off(int value) // Flash
{
	if(value == POWER_ON)
	{
		Cam_Printk("Camera Flash ON..\n");
		gpio_direction_output(camera_flash_en, 1);	/* enable */
		gpio_direction_output(camera_flash_set, 1);
	}
	else
	{
		Cam_Printk("Camera Flash OFF..\n");
		gpio_direction_output(camera_flash_en, 0);	/*Disable */
		gpio_direction_output(camera_flash_set, 0);
	}
}

static ssize_t store_flash(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)  
{ 
 int value;
 sscanf(buf, "%d", &value);
 
       camera_flash_on_off(value);
       
 return size;
}

 ssize_t rear_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	//printk("rear_camera_type: SOC\n");

	return sprintf(buf, "SOC");
}
 
static DEVICE_ATTR(rear_flash, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, NULL, store_flash);
static DEVICE_ATTR(rear_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camera_type, NULL);
/*-- AT_Command[Flash mode] --*/
#endif
#ifdef CONFIG_SOC_CAMERA_SR030PC50
void __init init_samsung_cam(void)
{

	pwd_main1 = mfp_to_gpio(MFP_PIN_GPIO14);
	pwd_main2 = mfp_to_gpio(MFP_PIN_GPIO15);
		
	if( (!pwd_main1) || (! pwd_main2)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: pwd_main1 :%d, pwd_main2:%d\n", pwd_main1, pwd_main2);	
		return;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (pwd_main1 && gpio_request(pwd_main1, "CAM_HI_SENSOR_PWD_14")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_main1);
		pwd_main1 = 0;
		return;
	}
	if (pwd_main2 && gpio_request(pwd_main2, "CAM_HI_SENSOR_PWD_15")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", pwd_main2);
		pwd_main2 = 0;
		return;
	}
	
	platform_device_register(&camera[ID_S5K4ECGX]);

	pxa988_add_cam(&mv_cam_data_forssg);
}
#else
void __init init_samsung_cam(void)
{
	struct device *dev_t;

	camera_power_standby = mfp_to_gpio(MFP_PIN_GPIO14);
	camera_power_reset = mfp_to_gpio(MFP_PIN_GPIO15);
	camera_flash_en = mfp_to_gpio(MFP_PIN_GPIO20);
	camera_flash_set = mfp_to_gpio(MFP_PIN_GPIO8)||mfp_to_gpio(MFP_PIN_GPIO97);
		
	if( (!camera_power_standby) || (! camera_power_reset)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: pwd_main1 :%d, pwd_main2:%d\n", camera_power_standby, camera_power_reset);	
		return;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (camera_power_standby && gpio_request(camera_power_standby, "CAM_HI_SENSOR_PWD_14")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_power_standby);
		camera_power_standby = 0;
		return;
	}
	if (camera_power_reset && gpio_request(camera_power_reset, "CAM_HI_SENSOR_PWD_15")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_power_reset);
		camera_power_reset = 0;
		return;
	}
	if (camera_flash_en && gpio_request(camera_flash_en, "CAM_HI_SENSOR_PWD_20")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_flash_en);
		camera_flash_en = 0;
		return;
	}
	if (camera_flash_set && gpio_request(camera_flash_set, "CAM_HI_SENSOR_PWD_8")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_flash_set);
		camera_flash_set = 0;
		return;
	}
	
	platform_device_register(&camera[ID_S5K4ECGX]);

	pxa988_add_cam(&mv_cam_data_forssg);

/*++ AT_Command[Flash mode] : dhee79.lee@samsung.com_20121012 ++*/
	camera_class = class_create(THIS_MODULE, "camera");
	
	if (IS_ERR(camera_class)) 
	{
	 printk("Failed to create camera_class!\n");
	 return PTR_ERR( camera_class );
}
	
	
	dev_t = device_create(camera_class, NULL, 0, "%s", "rear");
	
	if (device_create_file(dev_t, &dev_attr_rear_flash) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_flash.attr.name);
	if (device_create_file(dev_t, &dev_attr_rear_type) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_type.attr.name);

/*-- AT_Command[Flash mode] --*/
}
#endif
