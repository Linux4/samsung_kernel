#ifndef _MARVELL_CAMERA_H_
#define _MARVELL_CAMERA_H_

#include <linux/delay.h>
#include <media/soc_camera.h>

/* Sensor installation related */
enum {
	SENSOR_USED		= (1 << 31),
	SENSOR_UNUSED		= 0,
	SENSOR_POS_LEFT		= (1 << 2),
	SENSOR_POS_RIGHT	= 0,
	SENSOR_POS_FRONT	= (1 << 1),
	SENSOR_POS_BACK		= 0,
	SENSOR_RES_HIGH		= (1 << 0),
	SENSOR_RES_LOW		= 0,
};



/* MIPI related */
/* Sensor MIPI behavior descriptor, sensor driver should pass it to controller
 * driver, and let controller driver decide how to config its PHY registers */
struct csi_dphy_desc {
	u32 clk_mul;
	u32 clk_div;	/* clock_lane_freq = input_clock * clk_mul / clk_div */
	u32 clk_freq;	/* not suggest to use, maybe useful for legacy code */
	u32 cl_prepare;	/* cl_* describes clock lane timing in the unit of ns */
	u32 cl_zero;
	u32 hs_prepare;	/* hs_* describes data LP to HS transition timing */
	u32 hs_zero;	/* in the unit of clock lane period(DDR period) */
	u32 nr_lane;	/* When set to 0, S/W will try to figure out a value */
};

struct csi_dphy_calc {
	char name[16];
	int hs_termen_pos;
	int hs_settle_pos;	/* 0~100 */
};

/* Controller driver PHY register data, maybe need to add more item for other
 * controller type */
struct csi_dphy_reg {
	u16 cl_termen;
	u16 cl_settle;
	u16 cl_miss;
	u16 hs_termen;
	u16 hs_settle;
	u16 hs_rx_to;
	u16 lane;	/* When set to 0, S/W will try to figure out a value */
	u16 vc;		/* Virtual channel */
	u16 dt1;	/* Data type 1: For video or main data type */
	u16 dt2;	/* Data type 2: For thumbnail or auxiliry data type */
};

/*
 *Add macro definiton for sensor power.
 *Plese note the POWER_OFF and POWER_ON
 *value is fixed since in soc_camera.c
 *the value is directly used.
 */
#define POWER_OFF		0
#define POWER_ON		1
#define POWER_SAVING		2
#define POWER_RESTORE		3

/* V4L2 related */
#define V4L2_CID_PRIVATE_FIRMWARE_DOWNLOAD	(V4L2_CID_PRIVATE_BASE + 0)
#define V4L2_CID_PRIVATE_GET_MIPI_PHY		(V4L2_CID_PRIVATE_BASE + 1)

/* sleep function for sensor power sequence, only provide 1ms precision */
/* According to Documentation/timers/timers-howto.txt, we should choose *sleep
 * family function according to the time:
 * >= 20ms	msleep
 * >= 10us	usleep
 * for camera power usecase, we mostly need 1~2ms for power on/off sequence and
 * 100~500us for software reset, no precision control below 100us is used,
 * so sleep_range is good enough for us */
#define cam_msleep(x) \
do { \
	unsigned int ms = (x); \
	if (ms >= 20) \
		msleep(ms); \
	else \
		usleep_range(ms*1000, ms*1000+100); \
} while (0)

#endif
