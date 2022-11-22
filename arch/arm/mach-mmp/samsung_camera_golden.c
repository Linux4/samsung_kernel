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
#include <mach/mfp-pxa986-golden.h>
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
#include "common.h"


#ifdef CONFIG_FLED_RT5033 //RT5033 Flash LED : 20131031 dhee79.lee@samsung.com
#include <linux/leds/rtfled.h>
#elif CONFIG_LEDS_RT8547
#include <linux/leds-rt8547.h>
#endif

struct class *camera_class; 

int camera_flash_en, camera_flash_set;

/**++ For Flash Application : dhee79.lee@samsung.com 20130905 ++**/
#define FALSE 0
#define TRUE 1
int rear_camera = FALSE;
/**-- For Flash Application : dhee79.lee@samsung.com 20130905 --**/


/* Camera sensor GPIO pins */
static int Main_STBY, Main_RST, Sub_EN, Sub_RST;
/* Camera sensor Power GPIO pins */
static int CAM_AF, CAM_IO, CAM_AVDD;
#ifdef CONFIG_FLED_RT5033 
static int CAM_CORE;
#endif

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
			printk("--------MCLK pin Switchs TO mclk,  MFP Setting is :0x%x---------", val);
	} if (pinid == GPIO_PIN) {
			mfp_config(ARRAY_AND_SIZE(gpio_mfps));
			val = mfp_read(switch_mclk_gpio);
			printk("--------MCLK pin Switchs TO gpio,  MFP Setting is :0x%x---------", val);

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
			printk("--------Pins Switchs TO I2C,  MFP  Setting is GPIO079 :0x%x, GPIO080 :0x%x---------", val, val1);
	} if (pinid == GPIO_PIN) {
			mfp_config(ARRAY_AND_SIZE(gpio_mfps));
			val = mfp_read(switch_i2c_gpioa);
			val1 = mfp_read(switch_i2c_gpiob);
			printk("--------Pins Switchs TO GPIO,  MFP  Setting is GPIO079 :0x%x, GPIO080 :0x%x---------", val, val1);

			if (switch_i2c_gpioa && gpio_request(switch_i2c_gpioa, "switch_mclk_gpio79")) {
				printk(KERN_ERR "------switch_i2c_gpio_mfp-------Request GPIO failed,"
						"gpio: %d\n", switch_i2c_gpioa);
				switch_i2c_gpioa = 0;
				return -1;
			}

			if (switch_i2c_gpiob && gpio_request(switch_i2c_gpiob, "switch_mclk_gpio80")) {
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

static int sr030pc30_pin_init(void){

	Sub_EN = mfp_to_gpio(MFP_PIN_GPIO82);
	Sub_RST = mfp_to_gpio(MFP_PIN_GPIO81);
		
	if((!Sub_EN) || (!Sub_RST)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: Sub_EN :%d, Sub_RST:%d\n", Sub_EN, Sub_RST);	
		return -1;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (Sub_EN && gpio_request(Sub_EN, "pwd_sub_en_gpio82")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", Sub_EN);
		Sub_EN = 0;
		return -1;
	}

	if (Sub_RST && gpio_request(Sub_RST, "pwd_sub_rst_gpio81")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", Sub_RST);
		Sub_RST = 0;
		return -1;
	}

	return 0;
}

static int s5k43_pin_init(void){

	Main_STBY = mfp_to_gpio(MFP_PIN_GPIO14);
	Main_RST = mfp_to_gpio(MFP_PIN_GPIO15);

	CAM_AF = mfp_to_gpio(MFP_PIN_GPIO6);
	CAM_IO = mfp_to_gpio(MFP_PIN_GPIO79);
	CAM_AVDD = mfp_to_gpio(MFP_PIN_GPIO80);
#ifdef CONFIG_FLED_RT5033 	
	CAM_CORE = mfp_to_gpio(MFP_PIN_GPIO7);
#endif
	
		
	if( (!Main_STBY) || (! Main_RST) || (! CAM_AF) || (! CAM_IO) || (! CAM_AVDD)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: Main_STBY :%d, Main_RST:%d, CAM_AF :%d, CAM_IO:%d, CAM_AVDD :%d\n", Main_STBY, Main_RST, CAM_AF, CAM_IO, CAM_AVDD);	
		return -1;
	}

#ifdef CONFIG_FLED_RT5033 	
	if(! CAM_CORE){
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: CAM_CORE :%d\n", CAM_CORE);	
		return -1;
	}	
#endif
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

	
	if (CAM_AF && gpio_request(CAM_AF, "CAM_HI_SENSOR_PWD_6")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", CAM_AF);
		CAM_AF = 0;
		return -1;
	}
#ifdef CONFIG_FLED_RT5033 		
	if (CAM_CORE && gpio_request(CAM_CORE, "CAM_HI_SENSOR_PWD_7")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", CAM_CORE);
		CAM_CORE = 0;
		return -1;
	}
#endif
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

static int sr030pc30_power(struct device *dev, int flag)
{
#ifdef CONFIG_LEDS_RT8547
	static struct regulator *vcamera_vbuck5;   // 5M CORE : 1.2V

	if (!vcamera_vbuck5) {
		vcamera_vbuck5 = regulator_get(dev, "v_cam_c");
		if (IS_ERR(vcamera_vbuck5)) {
			vcamera_vbuck5 = NULL;
			pr_err(KERN_ERR "Enable vcamera_vbuck5 failed!\n");
			return -EIO;
		}
	}
#endif	
	if (flag) {
		switch_i2c_gpio_mfp(I2C_PIN);
		
		Cam_Printk("---sr030pc30_power power ON ----------\n");

		/* Sensor AVDD : 2.8V ON */
		gpio_direction_output(CAM_AVDD, 1);	

		udelay(50);

		/* Sensor IO : 1.8V ON */
		gpio_direction_output(CAM_IO, 1);	
		
		/* 5M Core : 1.2V ON */
#ifdef CONFIG_LEDS_RT8547
		regulator_set_voltage(vcamera_vbuck5, 1200000, 1200000);
		regulator_enable(vcamera_vbuck5);	
#elif CONFIG_FLED_RT5033		
		gpio_direction_output(CAM_CORE, 1);	
#endif
		
		/*  5M Core : 1.2V OFF  */
		msleep(5);
#ifdef CONFIG_LEDS_RT8547
		regulator_disable(vcamera_vbuck5);		
#elif CONFIG_FLED_RT5033
		gpio_direction_output(CAM_CORE, 0);	
#endif

		msleep(1);

		/* VT STBY Enable */
		gpio_direction_output(Sub_EN, 1);	

		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		pxa_ccic_enable_mclk(samsung_camera.pcdev, V4L2_MBUS_PARALLEL);

		msleep(1);		

		/* VT Rest Enable */
		gpio_direction_output(Sub_RST, 0);
		msleep(5);
		gpio_direction_output(Sub_RST, 1);	
		
		msleep(40);	
		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		samsung_camera.i2c_pxa_reset(samsung_camera.i2c);
		
	}else {
		Cam_Printk("---sr030pc30_power power OFF ----------\n");

		/* VT Rest Disable */
		gpio_direction_output(Sub_RST, 0);

		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		msleep(5);
		pxa_ccic_disable_mclk(samsung_camera.pcdev, V4L2_MBUS_PARALLEL);

		msleep(5);

		/* VT STBY Disable */
		gpio_direction_output(Sub_EN, 0);	
		
		/* Sensor IO : 1.8V OFF */
		gpio_direction_output(CAM_IO, 0);	

		/* Sensor AVDD : 2.8V OFF */
		gpio_direction_output(CAM_AVDD, 0);	


		switch_i2c_gpio_mfp(GPIO_PIN);
	}

	return 0;
}


void camera_flash_on_off(int value);
static int s5k43_power(struct device *dev, int flag)
{
	static int initialized = FALSE; // for checking is  probe state	
#ifdef CONFIG_LEDS_RT8547
	static struct regulator *vcamera_vbuck5;   // 5M CORE : 1.2V

	if (!vcamera_vbuck5) {
		vcamera_vbuck5 = regulator_get(dev, "v_cam_c");
		if (IS_ERR(vcamera_vbuck5)) {
			vcamera_vbuck5 = NULL;
			pr_err(KERN_ERR "Enable vcamera_vbuck5 failed!\n");
			return -EIO;
		}
	}
#endif

	if (flag) {
		switch_i2c_gpio_mfp(I2C_PIN);
		
		Cam_Printk("---camera power ON ----------\n");
		/* Sensor AVDD : 2.8V ON */
		gpio_direction_output(CAM_AVDD, 1);	
		
		msleep(1);

		/* Sensor IO : 1.8V ON */
		gpio_direction_output(CAM_IO, 1);	
		/* AF : 2.8V ON */
		gpio_direction_output(CAM_AF, 1);	

		msleep(1);

		/* VT STBY Enable */
		gpio_direction_output(Sub_EN, 1);	
		
		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		pxa_ccic_enable_mclk(samsung_camera.pcdev, V4L2_MBUS_CSI2);	

		/* VT Rest Enable */
		gpio_direction_output(Sub_RST, 0);
		msleep(5);
		gpio_direction_output(Sub_RST, 1);	
		
		msleep(2);

		/* VT STBY Disable */
		gpio_direction_output(Sub_EN, 0);	

		/* 5M Core : 1.2V ON */
#ifdef CONFIG_LEDS_RT8547
		regulator_set_voltage(vcamera_vbuck5, 1200000, 1200000);
		regulator_enable(vcamera_vbuck5);	
#else		
		gpio_direction_output(CAM_CORE, 1);	
#endif
		msleep(1);		

		/* 5M STBY Enable */
		gpio_direction_output(Main_STBY, 1);

		/* 5M Reset Enable*/		
		gpio_direction_output(Main_RST, 0);	
		msleep(2);
		gpio_direction_output(Main_RST, 1);	

		msleep(5);		
		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		samsung_camera.i2c_pxa_reset(samsung_camera.i2c);
		
	}else {
		Cam_Printk("---camera power OFF ----------\n");

		if((initialized==TRUE)&&(rear_camera==FALSE))
			camera_flash_on_off(POWER_OFF);  // Flash Off
		
		/* 5M Reset Disable*/		
		gpio_direction_output(Main_RST, 0);	
		msleep(1);
		
		/* Ccic Mclk enbale, enable/disable clk api is in mmp_camera.c */
		msleep(5);
		pxa_ccic_disable_mclk(samsung_camera.pcdev, V4L2_MBUS_CSI2);
		
		/* 5M STBY Disable */
		gpio_direction_output(Main_STBY, 0);	

		/* VT Rest Disable */
		gpio_direction_output(Sub_RST, 0);
		
		/*  5M Core : 1.2V OFF  */
#ifdef CONFIG_LEDS_RT8547
		regulator_disable(vcamera_vbuck5);		
#else
		gpio_direction_output(CAM_CORE, 0);	
#endif
		/* Sensor IO : 1.8V OFF */
		gpio_direction_output(CAM_IO, 0);	

		/* Sensor AVDD : 2.8V OFF */
		gpio_direction_output(CAM_AVDD, 0);	
		
		/* AF : 2.8V OFF */
		gpio_direction_output(CAM_AF, 0);	

		switch_i2c_gpio_mfp(GPIO_PIN);

		if(initialized==FALSE)
			initialized = TRUE;
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
	[ID_S5K4ECGX] = {
		.mount_pos	= SENSOR_USED | SENSOR_POS_BACK | SENSOR_RES_HIGH,
		.bus_type	= V4L2_MBUS_CSI2,
		.bus_flag	= V4L2_MBUS_CSI2_2_LANE, /* s5k used 2 lanes */
		.dphy		= {0x0a06, 0x33, 0x0900},
		.mipi_enabled	= 0,
		.dphy3_algo = 0,
		.plat		= &mv_cam_data_forssg,
	},
	[ID_SR030PC30] = {
		.mount_pos	= SENSOR_USED | SENSOR_POS_FRONT | SENSOR_RES_LOW,
		.bus_type	= V4L2_MBUS_PARALLEL,
		.bus_flag	= 0,	
		.plat		= &mv_cam_data_forssg,
	},
};

static struct i2c_board_info camera_i2c[] = {
	{
		I2C_BOARD_INFO("subsensor", (0xab >> 1)),
	},
	{
		I2C_BOARD_INFO("samsung_mainsensor", (0xac >> 1)),
	},
	{
		I2C_BOARD_INFO("sr030pc30", (0x60 >> 1)),
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
	[ID_S5K4ECGX] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[1],
		.i2c_adapter_id		= 0,
		.power = s5k43_power,
		.flags = V4L2_MBUS_CSI2_LANES,
		.module_name		= "samsung_mainsensor",
		.priv		= &sensor_data[ID_S5K4ECGX],
	},
	[ID_SR030PC30] = {
		.bus_id         = 0,            /* Must match with the camera ID */
		.power          = sr030pc30_power,
		.board_info     = &camera_i2c[2],
		.i2c_adapter_id = 0,
		.module_name    = "sr030pc30",
		.priv		= &sensor_data[ID_SR030PC30],
		.flags		= 0,	/* controller driver should copy priv->bus_flag
					 * here, so soc_camera_apply_board_flags can
					 * take effect */
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
	[ID_S5K4ECGX] = {
		.name	= "soc-camera-pdrv",
		.id	= 1,
		.dev	= {
			.platform_data = &iclink[ID_S5K4ECGX],
		},
	},	
	[ID_SR030PC30] = {
		.name	= "soc-camera-pdrv",
		.id	= 0,
		.dev	= {
			.platform_data = &iclink[ID_SR030PC30],
		},
	},	
};

static struct platform_device *camera_platform_devices[] = {
#if defined(CONFIG_SOC_CAMERA_SUBSENSOR)
	&camera[ID_SUBSENSOR],
#endif
#if defined(CONFIG_SOC_CAMERA_S5K4ECGX)
	&camera[ID_S5K4ECGX],
#endif
#if defined(CONFIG_VIDEO_SR030PC30)
	&camera[ID_SR030PC30],
#endif
};


/*++ AT_Command[Flash mode] : dhee79.lee@samsung.com_20121012 ++*/
void camera_flash_on_off(int value) // Flash
{
#ifdef CONFIG_FLED_RT5033
	rt_fled_info_t *rt5033_fled;
	rt5033_fled=rt_fled_get_info_by_name(NULL);
#endif

	if(value == POWER_ON)
	{
		Cam_Printk("Camera Flash ON..\n");
#ifdef CONFIG_FLED_RT5033		
		rt5033_fled->hal->fled_set_mode(rt5033_fled,FLASHLIGHT_MODE_TORCH);
#elif CONFIG_LEDS_RT8547
		rt8547_set_led_low();
#endif
		rear_camera = TRUE;
	}
	else
	{
		Cam_Printk("Camera Flash OFF..\n");
#ifdef CONFIG_FLED_RT5033		
		rt5033_fled->hal->fled_set_mode(rt5033_fled,FLASHLIGHT_MODE_OFF);
#elif CONFIG_LEDS_RT8547
		rt8547_set_led_off();
#endif		
		rear_camera = FALSE;
	}
}

static ssize_t store_flash(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)  
{ 
 	int value;
 	sscanf(buf, "%d", &value);
 
       camera_flash_on_off(value);
       
	return size;
}

 ssize_t front_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("front_camera_type: SOC\n");

	return sprintf(buf, "SOC");
}

 ssize_t front_camerafw_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("front_camerafw_type: SR030PC30\n");

	return sprintf(buf, "SR030PC30");
}

 ssize_t rear_camera_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camera_type: SOC\n");

	return sprintf(buf, "SOC");
}
 
  ssize_t rear_camerafw_type(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("rear_camerafw_type: S5K4ECGX\n");

	return sprintf(buf, "S5K4ECGX");
}

 ssize_t rear_camera_vendorid(struct device *dev, struct device_attribute *attr, char *buf)
{

    printk("rear_camera_vendorid\n");
    return sprintf(buf, "0x0903\n");
}

static DEVICE_ATTR(rear_flash, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, NULL, store_flash);
static DEVICE_ATTR(front_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  front_camera_type, NULL);
static DEVICE_ATTR(rear_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camera_type, NULL);
static DEVICE_ATTR(front_camfw, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  front_camerafw_type, NULL);
static DEVICE_ATTR(rear_camfw, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camerafw_type, NULL);
static DEVICE_ATTR(rear_vendorid, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,  rear_camera_vendorid, NULL);
/*-- AT_Command[Flash mode] --*/
void __init init_samsung_cam(void)
{
	int ret;
	struct device *dev_r;
	struct device *dev_f;

	Cam_Printk("init_samsung_cam..\n"); // Set LED gpio at the rt8547.c file

	ret= s5k43_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----s5k43_pin_init --failed!!!--------\n");
		return;
	}

	ret= sr030pc30_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----sr030pc30_pin_init --failed!!!--------\n");
		return;
	}

	ret= subsensor_pin_init();
	if (ret) {
		Cam_Printk("---error!!!!-----subsensor_pin_init --failed!!!--------\n");
		return;
	}

	platform_add_devices(ARRAY_AND_SIZE(camera_platform_devices));
	pxa988_add_cam(&mv_cam_data_forssg);
	
#ifdef CONFIG_LEDS_RT8547
	if (camera_flash_en && gpio_request(camera_flash_en, "CAM_HI_SENSOR_PWD_20")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_flash_en);
		camera_flash_en = 0;
		return;
	}
	if (camera_flash_set && gpio_request(camera_flash_set, "CAM_HI_SENSOR_PWD_97")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", camera_flash_set);
		camera_flash_set = 0;
		return;
	}
#endif

/*++ AT_Command[Flash mode] : dhee79.lee@samsung.com_20121012 ++*/
	camera_class = class_create(THIS_MODULE, "camera");
	
	if (IS_ERR(camera_class)) 
	{
	 printk("Failed to create camera_class!\n");
	 return PTR_ERR( camera_class );
        }
	
	dev_f= device_create(camera_class, NULL, 0, "%s", "front");	
	if (device_create_file(dev_f, &dev_attr_front_type) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_front_type.attr.name);
	if (device_create_file(dev_f, &dev_attr_front_camfw) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_front_camfw.attr.name);

	dev_r = device_create(camera_class, NULL, 0, "%s", "rear");		
	if (device_create_file(dev_r, &dev_attr_rear_type) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_type.attr.name);
	if (device_create_file(dev_r, &dev_attr_rear_camfw) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_camfw.attr.name);
	if (device_create_file(dev_r, &dev_attr_rear_flash) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_flash.attr.name);
	if (device_create_file(dev_r, &dev_attr_rear_vendorid) < 0)
	 printk("Failed to create device file(%s)!\n", dev_attr_rear_vendorid.attr.name);

/*-- AT_Command[Flash mode] --*/
}
