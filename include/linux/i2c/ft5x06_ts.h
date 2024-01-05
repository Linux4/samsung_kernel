#ifndef __LINUX_FT5X0X_TS_H__
#define __LINUX_FT5X0X_TS_H__


/* -- dirver configure -- */
#define CFG_SUPPORT_AUTO_UPG 0
#define CFG_SUPPORT_UPDATE_PROJECT_SETTING  0
#define CFG_SUPPORT_TOUCH_KEY  0    //touch key, HOME, SEARCH, RETURN etc

#define CFG_MAX_TOUCH_POINTS  5
#define CFG_NUMOFKEYS 4
#define CFG_FTS_CTP_DRIVER_VERSION "3.0"

#define SCREEN_MAX_X    2560
#define SCREEN_MAX_Y    1600
#define TSP_MAX_X    	1024
#define TSP_MAX_Y    	768
#define PRESS_MAX       255

#define CFG_POINT_READ_BUF  (3 + 6 * (CFG_MAX_TOUCH_POINTS))

#define FT5X0X_NAME	"ft5x06_ts"//"synaptics_i2c_rmi"//"synaptics-rmi-ts"//

#define KEY_PRESS       1
#define KEY_RELEASE     0

#define POLLING_OR_INTERRUPT 0	//0--Interrupt		1--Polling
#define USE_EVENT_POINT		1	// 1--Point		0--event

//#define DEBUG

//#ifdef DEBUG
#define DbgPrintk	printk

//register address
#define FT5x0x_REG_FW_VER		0xA6
#define FT5x0x_REG_POINT_RATE	0x88
#define FT5X0X_REG_THGROUP	0x80

#define FT5x0x_EXTEND_FUN	0

struct ts_event {
    u16 au16_x[CFG_MAX_TOUCH_POINTS];              //x coordinate
    u16 au16_y[CFG_MAX_TOUCH_POINTS];              //y coordinate
    u8  au8_touch_event[CFG_MAX_TOUCH_POINTS];     //touch event:  0 -- down; 1-- contact; 2 -- contact
    u8  au8_finger_id[CFG_MAX_TOUCH_POINTS];       //touch ID
    u16 pressure;
    u8  touch_point;
};

struct ft5x0x_ts_data {
	struct input_dev	*input_dev;
	struct ts_event		event;
	struct work_struct	pen_event_work;
	struct workqueue_struct *ts_workqueue;
};


#ifndef ABS_MT_TOUCH_MAJOR
#define ABS_MT_TOUCH_MAJOR	0x30	/* touching ellipse */
#define ABS_MT_TOUCH_MINOR	0x31	/* (omit if circular) */
#define ABS_MT_WIDTH_MAJOR	0x32	/* approaching ellipse */
#define ABS_MT_WIDTH_MINOR	0x33	/* (omit if circular) */
#define ABS_MT_ORIENTATION	0x34	/* Ellipse orientation */
#define ABS_MT_POSITION_X	0x35	/* Center X ellipse position */
#define ABS_MT_POSITION_Y	0x36	/* Center Y ellipse position */
#define ABS_MT_TOOL_TYPE	0x37	/* Type of touching device */
#define ABS_MT_BLOB_ID		0x38	/* Group set of pkts as blob */
#endif /* ABS_MT_TOUCH_MAJOR */

#ifndef ABS_MT_TRACKING_ID
#define ABS_MT_TRACKING_ID 0x39 /* Unique ID of initiated contact */
#endif

#endif
