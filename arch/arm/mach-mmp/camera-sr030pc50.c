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
#include <mach/camera-sr030pc50.h>

#if defined(CONFIG_MACH_ARUBA_TD)
#include <mach/mfp-pxa988-aruba.h>
#elif defined(CONFIG_MACH_HENDRIX)
#include <mach/mfp-pxa986-hendrix.h>
#endif

/* Camera sensor PowerDowN pins */
static int vt_cam_stby, vt_cam_reset;

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
	.bus_type = V4L2_MBUS_CSI2_LANES,
	.dphy = {0x1007, 0x33, 0x0900},/*MIPI clock lane frequency 192Mhz, to get the hs_settle 0x10, hs_termen 0x07.*/
	.lane = 1,/*lyh: sr030 with 1 data lane*/
	.dphy3_algo = 0,

};

/* ONLY for SSG S5K camera, __MUST__ keep it here */
struct sr030_cam_mclk sr030_cam_mclk_en ;


static int sr030_power(struct device *dev, int flag)
{
	static struct regulator *vcamera_io;
	static struct regulator *vcamera_analog;
	static struct regulator *vcamera_core;

	if (!vcamera_core) {
		vcamera_core = regulator_get(dev, "v_cam_c_1v8");
		if (IS_ERR(vcamera_core)) {
			vcamera_core = NULL;
			pr_err(KERN_ERR "Enable vcamera_core failed!\n");
			return -EIO;
		}
	}

	if (!vcamera_io) {
		vcamera_io = regulator_get(dev, "v_cam_io");
		if (IS_ERR(vcamera_io)) {
			vcamera_io = NULL;
			pr_err(KERN_ERR "Enable vcamera_io failed!\n");
			return -EIO;
		}
	}

	if (!vcamera_analog) {
		vcamera_analog = regulator_get(dev, "v_cam_avdd");
		if (IS_ERR(vcamera_analog)) {
			vcamera_analog = NULL;
			pr_err(KERN_ERR "Enable vcamera_analog failed!\n");
			return -EIO;
		}
	}


	if (flag) {
		Cam_Printk("---camera power ON -----the (1) step-----camera: io/ana/core ON-----------\n");
		/*lyh: need to pull down the gpio14/gpio15 in boot*/
		gpio_direction_output(vt_cam_reset, 0);	
		gpio_direction_output(vt_cam_stby, 0);	

		regulator_set_voltage(vcamera_io, 1800000, 1800000);
		regulator_enable(vcamera_io);

		regulator_set_voltage(vcamera_analog, 2800000, 2800000);
		regulator_enable(vcamera_analog);
		
		regulator_set_voltage(vcamera_core, 1800000, 1800000);
		regulator_enable(vcamera_core);


		msleep(4);		
		Cam_Printk("---camera power ON -----the (2) step-----camera:  delay 4ms,  set sensor mclk = 24 MHz-----------\n");
		sr030_cam_mclk_en.enable_clk(sr030_cam_mclk_en.pcdev);

		
		msleep(4);	
		Cam_Printk("---camera power ON -----the (3) step-----camera:  delay 4ms to stable PLL, standby ON-----------\n");
		gpio_direction_output(vt_cam_stby, 1);	/* enable */


		Cam_Printk(" ---camera power ON -----the (4) step-----camera: reset ON-----------\n");		
		msleep(30);
		gpio_direction_output(vt_cam_reset, 1);	/* enable */
		
		/*lyh: TODO, maybe don't need to reset i2c for sr030*/
		/*for s5k power off maybe pull down the i2c data pin, so we have to reset i2c controller */
		sr030_cam_mclk_en.i2c_pxa_reset(sr030_cam_mclk_en.i2c);
		
	}else {

		Cam_Printk("---camera power OFF -----the (1) step-----camera: reset OFF-----------\n");	
		gpio_direction_output(vt_cam_reset, 0);	/* disable */

		// mipi disable
		Cam_Printk("---camera power OFF -----the (2) step-----camera: disable sensor mclk-----------\n");	
		sr030_cam_mclk_en.disable_clk(sr030_cam_mclk_en.pcdev);
		
		Cam_Printk("---camera power OFF -----the (3) step-----camera: standby OFF-----------\n");	
		gpio_direction_output(vt_cam_stby, 0);	/* disable */

		Cam_Printk("---camera power OFF -----the (4) step-----camera: LDO OFF-----------\n");	
		regulator_disable(vcamera_core);
		regulator_disable(vcamera_analog);
		regulator_disable(vcamera_io);

	}

	return 0;
}

static struct i2c_board_info camera_i2c[] = {
	{
		I2C_BOARD_INFO("sr030pc50", (0x60 >> 1)),
	},
};

static struct soc_camera_link iclink[] = {
	[ID_SR030PC50] = {
		.bus_id			= 0, /* Must match with the camera ID */
		.board_info		= &camera_i2c[ID_SR030PC50],
		.i2c_adapter_id		= 0,
		.power = sr030_power,
		.flags = V4L2_MBUS_CSI2_LANES,
		.module_name		= "sr030pc50",
	},
};

static struct platform_device __attribute__((unused)) camera[] = {
	[ID_SR030PC50] = {
		.name	= "soc-camera-pdrv",
		.id	= 0,
		.dev	= {
			.platform_data = &iclink[ID_SR030PC50],
		},
	},
};

void __init register_camera_device(void)
{

	vt_cam_stby = mfp_to_gpio(MFP_PIN_GPIO71);
	vt_cam_reset = mfp_to_gpio(MFP_PIN_GPIO72);

	if( (!vt_cam_stby) || (! vt_cam_reset)) {
		printk(KERN_ERR "mfp_to_gpio  failed,"
				"gpio: vt_cam_stby :%d, vt_cam_reset:%d\n", vt_cam_stby, vt_cam_reset);
		return;
	}

	/* Camera hold these GPIO forever, should not be accquired by others */
	if (vt_cam_stby && gpio_request(vt_cam_stby, "CAM_HI_SENSOR_PWD_14")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", vt_cam_stby);
		vt_cam_stby = 0;
		return;
	}
	if (vt_cam_reset && gpio_request(vt_cam_reset, "CAM_HI_SENSOR_PWD_15")) {
		printk(KERN_ERR "Request GPIO failed,"
				"gpio: %d\n", vt_cam_reset);
		vt_cam_reset = 0;
		return;
	}
	
	platform_device_register(&camera[ID_SR030PC50]);

	pxa988_add_cam(&mv_cam_data_forssg);
}
