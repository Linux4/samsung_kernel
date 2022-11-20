/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * Created for samsung by Vincent Wan <zswan@marvell.com>,2012/03/31
 * Add dual camera architecture, add subsensor demo, sr030pc30 support, by Vincent Wan, 2013/2/16.
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
#include <mach/mfp-pxa986-goya.h>
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
#include <mach/samsung_camera_goya.h>
#include "common.h"


struct class *camera_class; 

#define FALSE 0
#define TRUE 1


/* Camera sensor GPIO pins */
static int Main_STBY, Main_RST;
/* Camera sensor Power GPIO pins */
static int CAM_IO, CAM_AVDD;

/* ONLY for parallel camera, SSG never use it currently, but __MUST__ keep it here */

static int pxa988_parallel_cam_pin_init(struct device *dev, int on)
{
	return 0;
}

struct mmp_cam_pdata mv_cam_data_forssg = {
	.name = "samsung_camera",
	.dma_burst = 64,
	.mclk_min = 24,
	.mclk_src = 3,
	.mclk_div = 13,
	.init_pin = pxa988_parallel_cam_pin_init,
};

/* ONLY for SSG S5K camera, __MUST__ keep it here */
struct samsung_camera_data samsung_camera ;

#define MCLK_PIN 0
#define GPIO_PIN 1
#define I2C_PIN 2

static int switch_mclk_gpio, switch_i2c_gpioa, switch_i2c_gpiob;
static int  switch_mclk_gpio_mfp(int pinid) {

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
			printk("--------MCLK pin Switchs TO mclk,  MFP Setting is :0x%x---------\n",val);
	} if (pinid == GPIO_PIN) {
			mfp_config(ARRAY_AND_SIZE(gpio_mfps));
			val = mfp_read(switch_mclk_gpio);
			printk("--------MCLK pin Switchs TO gpio,  MFP Setting is :0x%x---------\n", val);

			if (switch_mclk_gpio && gpio_request(switch_mclk_gpio, "switch_mclk_gpio77")) {
			printk(KERN_ERR "------switch_mclk_gpio_mfp-------Request GPIO failed,"
					"gpio: %d\n", switch_mclk_gpio);
			switch_mclk_gpio = 0;
			return -1;
	}
		gpio_direction_output(switch_mclk_gpio, 0);

		gpio_free(switch_mclk_gpio);

	}

	return 0;

}

static int  switch_i2c_gpio_mfp(int pinid) {

	unsigned int val,val1;

	unsigned long i2c_mfps[] = {
		GPIO053_CAM_SCL | MFP_LPM_FLOAT,
		GPIO054_CAM_SDA | MFP_LPM_FLOAT,
	};

	unsigned long gpio_mfps[] = {
		GPIO053_GPIO_53,
		GPIO054_GPIO_54,
	};

	switch_i2c_gpioa = mfp_to_gpio(GPIO053_GPIO_53);
	switch_i2c_gpiob = mfp_to_gpio(GPIO054_GPIO_54);

	if (pinid == I2C_PIN) {
			mfp_config(ARRAY_AND_SIZE(i2c_mfps));
			val = mfp_read(switch_i2c_gpioa);
			val1 = mfp_read(switch_i2c_gpiob);
			printk("--------Pins Switchs TO I2C, 2M MFP  Setting is GPIO053 :0x%x, GPIO054 :0x%x---------\n", val, val1);
	} if (pinid == GPIO_PIN) {
			mfp_config(ARRAY_AND_SIZE(gpio_mfps));
			val = mfp_read(switch_i2c_gpioa);
			val1 = mfp_read(switch_i2c_gpiob);
			printk("--------Pins Switchs TO GPIO,  2M MFP  Setting is GPIO053 :0x%x, GPIO054 :0x%x---------\n", val, val1);

			if (switch_i2c_gpioa && gpio_request(switch_i2c_gpioa, "switch_mclk_gpio53")) {
				printk(KERN_ERR "------switch_i2c_gpio_mfp-------Request GPIO failed,"
						"gpio: %d\n", switch_i2c_gpioa);
				switch_i2c_gpioa = 0;
				return -1;
			}

			if (switch_i2c_gpiob && gpio_request(switch_i2c_gpiob, "switch_mclk_gpio54")) {
				printk(KERN_ERR "------switch_i2c_gpio_mfp-------Request GPIO failed,"
						"gpio: %d\n", switch_i2c_gpiob);
				switch_i2c_gpiob = 0;
				return -1;
			}
			
		gpio_direction_output(switch_i2c_gpioa, 0);
		gpio_direction_output(switch_i2c_gpiob, 0);

		gpio_free(switch_i2c_gpioa);
		gpio_free(switch_i2c_gpiob);

	}

	return 0;

}

static void pxa_ccic_enable_mclk(struct mmp_camera_dev *pcdev, 
									enum v4l2_mbus_type bus_type)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	int ctrl1 = 0;
	int mipi;

	switch_mclk_gpio_mfp(MCLK_PIN);

	if (bus_type == V4L2_MBUS_CSI2)
		mipi = MIPI_ENABLE;
	else
		mipi = MIPI_DISABLE;

	pdata->enable_clk(dev, mipi | POWER_ON);
	ccic_reg_write(pcdev, REG_CLKCTRL,
			(pdata->mclk_src << 29) | pdata->mclk_div);
	dev_dbg(dev, "camera: set sensor mclk = %d MHz\n", pdata->mclk_min);

	switch (pdata->dma_burst) {
	case 128:
		ctrl1 = C1_DMAB128;
		break;
	case 256:
		ctrl1 = C1_DMAB256;
		break;
	}
	ccic_reg_write(pcdev, REG_CTRL1, ctrl1 | C1_RESERVED | C1_DMAPOSTED);
	if (bus_type != V4L2_MBUS_CSI2)
		ccic_reg_write(pcdev, REG_CTRL3, 0x4);
}

static void pxa_ccic_disable_mclk(struct mmp_camera_dev *pcdev,
										enum v4l2_mbus_type bus_type)
{
	struct mmp_cam_pdata *pdata = pcdev->pdev->dev.platform_data;
	struct device *dev = &pcdev->pdev->dev;
	int mipi;

	switch_mclk_gpio_mfp(GPIO_PIN);
	
	ccic_reg_write(pcdev, REG_CLKCTRL, 0x0);
	/*
	 * Bit[5:1] reserved and should not be changed
	 */
	ccic_reg_write(pcdev, REG_CTRL1, C1_RESERVED);

	if (bus_type == V4L2_MBUS_CSI2)
		mipi = MIPI_ENABLE;
	else
		mipi = MIPI_DISABLE;

	pdata->enable_clk(dev, mipi | POWER_OFF);

}

static int subsensor_pin_init(void){

	return 0;
}

static int sr200pc20m_pin_init(void){

	Main_STBY = mfp_to_gpio(MFP_PIN_GPIO14);
	Main_RST = mfp_to_gpio(MFP_PIN_GPIO15);

	CAM_IO = mfp_to_gpio(MFP_PIN_GPIO79);
	CAM_AVDD = mfp_to_gpio(MFP_PIN_GPIO80);
		
	if( (!Main_STBY) || (! Main_RST) ||(! CAM_IO) || (! CAM_AVDD)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: Main_STBY :%d, Main_RST:%d\n", Main_STBY, Main_RST);	
		return -1;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (Main_STBY && gpio_request(Main_STBY, "CAM_HI_SENSOR_PWD_14")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", Main_STBY);
		Main_STBY = 0;
		return -1;
	}
	if (Main_RST && gpio_request(Main_RST, "CAM_HI_SENSOR_PWD_15")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", Main_RST);
		Main_RST = 0;
		return -1;
	}

	if (CAM_IO && gpio_request(CAM_IO, "CAM_HI_SENSOR_PWD_79")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", CAM_IO);
		CAM_IO = 0;
		return -1;
	}
	if (CAM_AVDD && gpio_request(CAM_AVDD, "CAM_HI_SENSOR_PWD_80")){
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", CAM_AVDD);
		CAM_AVDD = 0;
		return -1;
	}

	return 0;
}

static int subsensor_power(struct device *dev, int flag){
	Cam_Printk("---sub camera power-----flag: %d-----\n", flag);
	/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
	if (flag) {
		pxa_ccic_enable_mclk(samsung_camera.pcdev, V4L2_MBUS_PARALLEL);
	}else {
		pxa_ccic_disable_mclk(samsung_camera.pcdev, V4L2_MBUS_PARALLEL);
	}
	return 0;
}


static int sr200pc20m_power(struct device *dev, int flag)
{

	if (flag) {
		switch_i2c_gpio_mfp(I2C_PIN);
		
		Cam_Printk("---sr200pc20m camera power ON ----------\n");
		
		/* Sensor IO & Main Core & VT Core : 1.8V ON */
		gpio_direction_output(CAM_IO, 1);	
		/* Sensor AVDD : 2.8V ON */
		gpio_direction_output(CAM_AVDD, 1);	

		msleep(5);
		
		/* 2M STBY Enable */
		gpio_direction_output(Main_STBY, 1);
		
		msleep(1);
		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		pxa_ccic_enable_mclk(samsung_camera.pcdev, V4L2_MBUS_CSI2);	

		msleep(40);

		/* 2M Reset Enable*/		
		gpio_direction_output(Main_RST, 0);	
		msleep(2);
		gpio_direction_output(Main_RST, 1);	

		msleep(5);		
		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		samsung_camera.i2c_pxa_reset(samsung_camera.i2c);
		
	}else {
		Cam_Printk("---sr200pc20m camera power OFF ----------\n");

		/* 2M Reset Disable*/		
		msleep(1);		
		gpio_direction_output(Main_RST, 0);	
		
		msleep(1);
		
		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		pxa_ccic_disable_mclk(samsung_camera.pcdev, V4L2_MBUS_CSI2);

		msleep(5);
		
		/* 2M STBY Disable */
		gpio_direction_output(Main_STBY, 0);	

		/* Sensor AVDD : 2.8V OFF */
		gpio_direction_output(CAM_AVDD, 0);	

		/* Sensor IO & Main Core & VT Core : 1.8V OFF */
		gpio_direction_output(CAM_IO, 0);	
	

		switch_i2c_gpio_mfp(GPIO_PIN);

	}

	return 0;
}

static struct sensor_board_data sensor_data[] = {
	[ID_SUBSENSOR] = {
		.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
		.bus_type	= V4L2_MBUS_PARALLEL,
		.bus_flag	= 0,	
		.plat		= &mv_cam_data_forssg,
	},
	[ID_SR200PC20M] = {
		.mount_pos	= SENSOR_USED | SENSOR_POS_BACK | SENSOR_RES_HIGH,
		.bus_type	= V4L2_MBUS_CSI2,
		.bus_flag	= V4L2_MBUS_CSI2_1_LANE, /* s5k used 2 lanes */
		.dphy		= {0x0b05, 0x11, 0x0900},
		.mipi_enabled	= 0,
		.dphy3_algo = 0,
		.plat		= &mv_cam_data_forssg,
	},
};

static struct i2c_board_info camera_i2c[] = {
	{
		I2C_BOARD_INFO("subsensor", (0xab >> 1)),
	},
	{
		I2C_BOARD_INFO("samsung_mainsensor", (0x40 >> 1)),
	},
};

static struct soc_camera_link iclink[] = {
	[ID_SUBSENSOR] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.power          = subsensor_power,
		.board_info		= &camera_i2c[0],
		.i2c_adapter_id		= 0,
		.module_name    = "subsensor",
		.priv		= &sensor_data[ID_SUBSENSOR],
		.flags		= 0,	/* controller driver should copy priv->bus_flag
					 * here, so soc_camera_apply_board_flags can
					 * take effect */
	},
	[ID_SR200PC20M] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[1],
		.i2c_adapter_id		= 0,
		.power = sr200pc20m_power,
		.flags = V4L2_MBUS_CSI2_LANES,
		.module_name		= "samsung_mainsensor",
		.priv		= &sensor_data[ID_SR200PC20M],
	},
};

static struct platform_device __attribute__((unused)) camera[] = {
	[ID_SUBSENSOR] = {
		.name	= "soc-camera-pdrv",
		.id	= 0,
		.dev	= {
			.platform_data = &iclink[ID_SUBSENSOR],
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
#if defined(CONFIG_SOC_CAMERA_SUBSENSOR)
	&camera[ID_SUBSENSOR],
#endif
#if defined(CONFIG_SOC_CAMERA_SR200PC20M)
	&camera[ID_SR200PC20M],
#endif
};

 ssize_t rear_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: SOC\n");

	return sprintf(buf, "SOC");
}
 
  ssize_t rear_camerafw_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camerafw_type: SR200PC20M\n");

	return sprintf(buf, "SR200PC20M");
}

 ssize_t rear_camera_vendorid(struct device *dev, struct device_attribute *attr, char *buf)
{

    printk("rear_camera_vendorid\n");
    return sprintf(buf, "0x0903\n");
}

int camera_antibanding_val = 2; /*default : CAM_BANDFILTER_50HZ = 2*/

ssize_t camera_antibanding_show (struct device *dev, struct device_attribute *attr, char *buf) {
	int count;

	count = sprintf(buf, "%d", camera_antibanding_val);
	printk("%s : antibanding is %d \n", __func__, camera_antibanding_val);

	return count;
}

ssize_t camera_antibanding_store (struct device *dev, struct device_attribute *attr, const char *buf, size_t size) {
	int tmp = 0;

	sscanf(buf, "%d", &tmp);
	if ((2 == tmp) || (3 == tmp)) { /* CAM_BANDFILTER_50HZ = 2, CAM_BANDFILTER_60HZ = 3*/
		camera_antibanding_val = tmp;
		printk("%s : antibanding is %d\n",__func__, camera_antibanding_val);
	}

	return size;
}

static struct device_attribute camera_antibanding_attr = {
	.attr = {
		.name = "Cam_antibanding",
		.mode = (S_IRUSR|S_IRGRP | S_IWUSR|S_IWGRP)},
	.show = camera_antibanding_show,
	.store = camera_antibanding_store
};


static DEVICE_ATTR(rear_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camera_type, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camerafw_type, NULL);
static DEVICE_ATTR(rear_vendorid, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camera_vendorid, NULL);
/*-- AT_Command[Flash mode] --*/
void __init init_samsung_cam(void)
{
	int ret;
	struct device *dev_r;

	Cam_Printk("init_samsung_cam..\n"); // Set LED gpio at the rt8547.c file

	ret= sr200pc20m_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----s5k43_pin_init --failed!!!--------\n");
		return;
	}

	ret= subsensor_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----subsensor_pin_init --failed!!!--------\n");
		return;
	}

	platform_add_devices(ARRAY_AND_SIZE(camera_platform_devices));
	pxa988_add_cam(&mv_cam_data_forssg);


/*++ AT_Command[Flash mode] : dhee79.lee@samsung.com_20121012 ++*/
	camera_class = class_create(THIS_MODULE, "camera");
	
	if (IS_ERR(camera_class)) 
	{
	 printk("Failed to create camera_class!\n");
	 return PTR_ERR( camera_class );
        }

	dev_r = device_create(camera_class, NULL, 0, "%s", "rear");		
	if (device_create_file(dev_r, &dev_attr_rear_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_rear_type.attr.name);
	if (device_create_file(dev_r, &dev_attr_rear_camfw) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
	if (device_create_file(dev_r, &dev_attr_rear_vendorid) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_rear_vendorid.attr.name);
	if (device_create_file(dev_r, &camera_antibanding_attr) < 0) {
		printk("Failed to create device file: %s\n", camera_antibanding_attr.attr.name);
	}
/*-- AT_Command[Flash mode] --*/
}
