#ifndef __ASM_ARCH_CAMERA_H__
#define __ASM_ARCH_CAMERA_H__

#include <media/mrvl-camera.h>
#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
#include <media/mc_debug.h>
#endif
#if defined(CONFIG_MACH_LT02)
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

#else

struct mmp_cam_pdata {
	/* CCIC_GATE, CCIC_RST, CCIC_DBG, CCIC_DPHY clocks */
	struct clk *clk[4];
	char *name;
	int dma_burst;
	int mclk_min;
	int mclk_src;
	int mclk_div;
	int (*init_pin)(struct device *dev, int on);
	int (*init_clk)(struct device *dev);
	/*
	 * @on use 2 bits descript:
	 * bit 0: 1 - enable;	0 - disable
	 * bit 1: 1 - mipi;	0 - parallel
	 */
	void (*enable_clk)(struct device *dev, int on);
};
#endif


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

//extern int camera_power_reset;
//extern int camera_power_standby;
extern int camera_flash_en;
extern int camera_flash_set;

/*++ Marvell_VIA Flash Setting(KTD2692) : dhee79.lee@samsung.com ++*/
#ifdef CONFIG_MACH_DELOS3GVIA
/* KTD2692 : command time delay(us) */
#define T_DS		12
#define T_EOD_H		350
#define T_EOD_L		4
#define T_H_LB		3
#define T_L_LB		2*T_H_LB
#define T_L_HB		3
#define T_H_HB		2*T_L_HB
#define T_RESET		700
/* KTD2692 : command address(A2:A0) */
#define LVP_SETTING		0x0 << 5
#define FLASH_TIMEOUT	0x1 << 5
#define MIN_CURRENT		0x2 << 5
#define MOVIE_CURRENT	0x3 << 5
#define FLASH_CURRENT	0x4 << 5
#define MODE_CONTROL	0x5 << 5

extern void KTD2692_ctrl_cmd(unsigned int ctl_cmd);
#endif
/*-- Marvell_VIA Flash Setting(KTD2692) : dhee79.lee@samsung.com --*/


int isppwr_power_control(int on);

#ifdef CONFIG_VIDEO_MRVL_CAM_DEBUG
struct ccic_mcd {
	struct mcd              mcd;
	struct mcd_entity       *pitem[MCD_ENTITY_END];
};
#endif /* CONFIG_VIDEO_MRVL_CAM_DEBUG */

#endif
