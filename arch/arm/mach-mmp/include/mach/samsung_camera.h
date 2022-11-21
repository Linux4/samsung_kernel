/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * 
 * Created for samsung by Vincent Wan <zswan@marvell.com>,2012/03/31
 */
#include <linux/platform_device.h>
#include <mach/camera.h>
#ifdef CAM_DEBUG 
#define Cam_Printk(msg...) printk(msg)	
#else
#define Cam_Printk(msg...)
#endif

#ifdef CONFIG_SOC_CAMERA_SR030PC50
enum {
	ID_SR030PC50= 0,
};
#else
enum {
	ID_S5K4ECGX	= 0,
};
#endif

enum cam_power {
	CAM_POWER_OFF	= 0,
	CAM_POWER_ON	= 1
};

/* for s5k sensor power sequence workaround */
struct mmp_camera_dev;
struct pxa_i2c ;

struct s5k_cam_mclk {
	void (*disable_clk)(struct mmp_camera_dev *pdev);
	void (*enable_clk)(struct mmp_camera_dev *pdev);
	void (*i2c_pxa_reset)(struct pxa_i2c *i2c);
	struct pxa_i2c *i2c;
	struct mmp_camera_dev *pcdev;
};

extern struct s5k_cam_mclk s5k_cam_mclk_en;
extern void  init_samsung_cam(void);
extern struct mmp_cam_pdata mv_cam_data_forssg;