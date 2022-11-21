#ifndef __ASM_ARCH_CAMERA_H__
#define __ASM_ARCH_CAMERA_H__

#include <media/mrvl-camera.h>
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <media/mc_debug.h>
#endif

struct mmp_cam_pdata {
	/* CCIC_GATE, CCIC_RST, CCIC_DBG, CCIC_DPHY clocks */
	struct clk *clk[4];
	char *name;
	int clk_enabled;
	int dphy[3];		/* DPHY: CSI2_DPHY3, CSI2_DPHY5, CSI2_DPHY6 */
	int dphy3_algo;		/* Exist 2 algos for calculate CSI2_DPHY3 */
	int bus_type;
	int mipi_enabled;	/* MIPI enabled flag */
	int lane;		/* ccic used lane number; 0 means DVP mode */
	int dma_burst;
	int mclk_min;
	int mclk_src;
	int mclk_div;
	void (*init_pin)(struct device *dev, int on);
	int (*init_clk)(struct device *dev, int init);
	void (*enable_clk)(struct device *dev, int on);
};

struct sensor_power_data {
	unsigned char *sensor_name; /*row sensor name  */
	int rst_gpio; /* sensor reset GPIO */
	int pwdn_gpio;  /* sensor power enable GPIO*/
	int rst_en; /* sensor reset value: 0 or 1 */
	int pwdn_en; /* sensor power value: 0 or 1*/
	const char *afvcc;
	const char *avdd;
	int afvcc_uV;
	int avdd_uV;
};

struct sensor_type {
	unsigned char chip_id;
	unsigned char *sensor_name;
	unsigned int reg_num;
	long reg_pid[3];/* REG_PIDH REG_PIDM REG_PIDL */
	/* REG_PIDH_VALUE REG_PIDM_VALUE REG_PIDL_VALUE */
	unsigned char reg_pid_val[3];
};

struct sensor_platform_data {
	int *chip_ident_id;
	struct sensor_power_data *sensor_pwd;
	struct sensor_type *sensor_cid;
	int sensor_num;
	int flash_enable;	/* Flash enable data; -1 means invalid */
	int (*power_on)(int);
};

extern int camera_power_reset;
extern int camera_power_standby;
extern int camera_flash_en;
extern int camera_flash_set;

int isppwr_power_control(int on);

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
struct ccic_mcd {
	struct mcd              mcd;
	struct mcd_entity       *pitem[MCD_ENTITY_END];
};
#endif /* CONFIG_VIDEO_MRVL_CAM_DEBUG */

#endif
