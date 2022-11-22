/* drivers/input/touchscreen/sec_ts.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SEC_TS_H__
#define __SEC_TS_H__

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_INPUT_BOOSTER
#include <linux/input/input_booster.h>
#endif

//#define SEC_TS_I2C_NAME "sec_ts"
#define SEC_TS_I2C_NAME "sec_ts"
#define SEC_TS_DEVICE_NAME "SEC_TS"

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define tsp_debug_dbg(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_dbg(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_info(mode, dev, fmt, ...)	\
({								\
	if (mode) {							\
		dev_info(dev, fmt, ## __VA_ARGS__);		\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);		\
	}				\
	else					\
		dev_info(dev, fmt, ## __VA_ARGS__);	\
})

#define tsp_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {					\
		dev_err(dev, fmt, ## __VA_ARGS__);	\
		sec_debug_tsp_log(fmt, ## __VA_ARGS__);	\
	}				\
	else					\
		dev_err(dev, fmt, ## __VA_ARGS__); \
})
#else
#define tsp_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tsp_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_DWORK
#define TOUCH_OPEN_DWORK_TIME 10
#endif

#define MAX_SUPPORT_TOUCH_COUNT 10
#define MAX_SUPPORT_HOVER_COUNT 1

#define SEC_TS_EVENTID_HOVER 10

#define SEC_TS_DEFAULT_FW_NAME "tsp_sec/s6smc43_s6_img_REL.bin"
#define SEC_TS_DEFAULT_PARA_NAME "tsp_sec/s6smc41_para_REL_DGA0_V0106_150114_193317.bin"
#define SEC_TS_MAX_FW_PATH 64

#define SEC_TS_STATE_POWER_ON  1
#define SEC_TS_STATE_POWER_OFF 0

#define I2C_WRITE_BUFFER_SIZE 7

#define SEC_TS_DRV_VERSION "g_zerof.v02"

#define SEC_TS_FW_HEADER_SIGN 0x53494654
#define SEC_TS_FW_CHUNK_SIGN  0x53434654

#define CONFIG_FW_UPDATE_ON_PROBE

#define SEC_TS_CMD_SW_RESET 0x42
#define SEC_TS_CMD_AFE_CALIBRATION 0x43
#define SEC_TS_READ_TOUCH_RAWDATA 0x76
#define SEC_TS_READ_TOUCH_SELF_RAWDATA 0x77
#define SEC_TS_READ_RAW_CHANNEL 0x58
#define SEC_TS_READ_DEVICE_ID 0x52

#define SEC_TS_CMD_SENSE_ON 0x40
#define SEC_TS_CMD_SENSE_OFF 0x41
#define SEC_TS_CMD_NOISE_MODE 0x77
#define SEC_TS_CMD_STATEMANAGE_ON 0x48
#define SEC_TS_CMD_CHG_SYSMODE 0xD7

#define SEC_TS_CMD_CLEAR_EVENT_STACK 0x60
#define SEC_TS_READ_ONE_EVENT 0x71

#define SEC_TS_CMD_SET_TOUCHFUNCTION 0x63
#define SEC_TS_READ_SET_TOUCHFUNCTION 0x64
#define SEC_TS_READ_FW_STATUS 0x51
#define SEC_TS_READ_BOOT_STATUS 0x55
#define SEC_TS_READ_TS_STATUS 0x70
#define SEC_TS_READ_LV3 0xD2
#define SEC_TS_READ_FW_VERSION 0xA3
#define SEC_TS_READ_PARA_VERSION 0xA4

#define SEC_TS_Status_Event 0
#define SEC_TS_Coordinate_Event 1
#define SEC_TS_Gesture_Event 2
#define SEC_TS_Event_Buff_Size 6

#define SEC_TS_Coordinate_Action_None 0
#define SEC_TS_Coordinate_Action_Press 1
#define SEC_TS_Coordinate_Action_Release 3
#define SEC_TS_Coordinate_Action_Move 2

#define SEC_TS_TOUCHTYPE_NORMAL	0
#define SEC_TS_TOUCHTYPE_PROXIMITY 1
#define SEC_TS_TOUCHTYPE_FLIPCOVER 2
#define SEC_TS_TOUCHTYPE_GLOVE 3
#define SEC_TS_TOUCHTYPE_STYLUS 4
#define SEC_TS_TOUCHTYPE_HOVER 5
#define SEC_TS_TOUCHTYPE_PALM 6

#define SEC_TS_BIT_SETFUNC_TOUCH (0x1<<0)
#define SEC_TS_BIT_SETFUNC_MUTUAL (0x1<<0)
#define SEC_TS_BIT_SETFUNC_HOVER (0x1<<1)
#define SEC_TS_BIT_SETFUNC_CLEARCOVER (0x1<<2)
#define SEC_TS_BIT_SETFUNC_GLOVE (0x1<<3)
#define SEC_TS_BIT_SETFUNC_CHARGER (0x1<<4)
#define SEC_TS_BIT_SETFUNC_STYLUS (0x1<<5)

#define STATE_MANAGE_ON		1
#define STATE_MANAGE_OFF	0

#define TOUCH_MODE_TOUCH	2

#define TOUCH_STATE_TOUCH	2 

#define CMD_STR_LEN 32
#define CMD_PARAM_NUM 8
#define CMD_RESULT_STR_LEN 512

//#define SMARTCOVER_COVER        // for  Various Cover
#undef SMARTCOVER_COVER

#ifdef SMARTCOVER_COVER
#define MAX_W 16                // zero is 16 x 28
#define MAX_H 32                // byte size to IC
#define MAX_TX MAX_W
#define MAX_BYTE MAX_H
#endif


enum sec_ts_cover_id {
        SEC_TS_FLIP_WALLET = 0,
        SEC_TS_VIEW_COVER,
        SEC_TS_COVER_NOTHING1,
        SEC_TS_VIEW_WIRELESS,
        SEC_TS_COVER_NOTHING2,
        SEC_TS_CHARGER_COVER,
        SEC_TS_VIEW_WALLET,
        SEC_TS_LED_COVER,
        SEC_TS_CLEAR_FLIP_COVER,
        SEC_TS_MONTBLANC_COVER = 100,
};


extern struct sec_ts_callbacks *charger_callbacks;
struct sec_ts_callbacks {
	void (*inform_charger)(struct sec_ts_callbacks *, int type);
};

struct sec_ts_coordinate{
        u8 id;
        u8 ttype;
        u8 action;
        u16 x;
        u16 y;
        u8 touch_width;
        u8 hover_flag;
        u8 glove_flag;
        u8 touch_height;
	u16 mcount;
};

struct sec_ts_event_coordinate{
	u8 tchsta:3;
	u8 ttype:3;
	u8 eid:2;

	u8 tid:4;
	u8 nt:4;

	u8 x_11_4;

	u8 y_11_4;

	u8 y_3_0:4;
	u8 x_3_0:4;

	u8 z;
//	u8 h;
//	u8 pf:1;
//	u8 reserved_7:7;
} __attribute__ ((packed));

struct sec_ts_data {
	u32 isr_pin;

	u32 crc_addr;
	u32 fw_addr;
	u32 para_addr;

	struct device *dev;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct sec_ts_plat_data *plat_data;
	struct factory_data *f_data;
	struct sec_ts_coordinate coord[MAX_SUPPORT_TOUCH_COUNT];

	uint32_t flags;

	int touch_count;
	int tx_count;
	int rx_count;
	int ta_status;
	int power_status;
	int raw_status;
	int touchkey_glove_mode_status;
	u8 glove_enables;
	u8 hover_enables;
	struct sec_ts_event_coordinate touchtype;
	bool touched[11];
	u8 gesture_status[6];

	struct sec_ts_callbacks callbacks;

	struct mutex lock;
	struct mutex device_mutex;
	struct mutex i2c_mutex;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend *early_suspend;
#endif

#ifdef USE_OPEN_DWORK
	struct delayed_work open_work;
#endif

	int irq;
	bool enabled;

	//factory_data
        struct device *fac_dev_ts;
        bool cmd_is_running;
        unsigned char cmd_state;
        char cmd[CMD_STR_LEN];
        int cmd_param[CMD_PARAM_NUM];
        char cmd_buff[CMD_RESULT_STR_LEN];
        char cmd_result[CMD_RESULT_STR_LEN];
        struct mutex cmd_lock;
        struct list_head cmd_list_head;
        void (*sec_ts_fn_init)(void *device_data);
        int SenseChannelLength;
        int ForceChannelLength;
        short *pFrame;
        unsigned char *cx_data;
        struct delayed_work cover_cmd_work;
        int delayed_cmd_param[2];

        bool touch_stopped;
        bool reinit_done;
        bool flip_enable;
        unsigned int cover_type;

#ifdef FTS_SUPPORT_2NDSCREEN
	u8 SIDE_Flag;
	u8 previous_SIDE_value;
#endif

#ifdef SMARTCOVER_COVER
        bool smart_cover[MAX_BYTE][MAX_TX];
        bool changed_table[MAX_TX][MAX_BYTE];
        u8 send_table[MAX_TX][4];
#endif

	int tspid_val;
	int tspid2_val;

	int (*sec_ts_i2c_write)(struct sec_ts_data * ts, u8 reg, u8 * data, int len);
	int (*sec_ts_i2c_read)(struct sec_ts_data * ts, u8 reg, u8 * data, int len);
	int (*sec_ts_i2c_write_burst)(struct sec_ts_data *ts, u8 *data, int len);
#ifdef SEC_TS_SUPPORT_STRINGLIB
	int (*sec_ts_read_from_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
	int (*sec_ts_write_to_string)(struct fts_ts_info *info, unsigned short *reg, unsigned char *data, int length);
#endif

};

struct sec_ts_plat_data {
	int max_x;
	int max_y;
	//int intx_pin;
	int num_tx;
	int num_rx;
	unsigned gpio;
	int irq_type;

	const char *firmware_name;
	const char *parameter_name;
	const char *model_name;
	const char *project_name;
	const char *regulator_dvdd;
	const char *regulator_avdd;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_default;
	struct pinctrl_state *pins_sleep;

	int (*power)(void *data, bool on);
	void (*recovery_mode)(bool on);
	void (*enable_sync)(bool on);
	void (*register_cb)(struct sec_ts_callbacks *);

	unsigned tspid;
	unsigned tspid2;
};

int sec_ts_firmware_update_on_probe(struct sec_ts_data *ts);
int sec_ts_firmware_update_on_hidden_menu(struct sec_ts_data *ts,int update_type);
int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode);
int sec_ts_hover_enables(struct sec_ts_data *ts, int enables);
int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable);

int sec_ts_function( int (*func_init)(void *device_data),
                void (*func_remove)(void));

//static void clear_cover_cmd_work(struct work_struct *work);

void sec_ts_delay(unsigned int ms);
int sec_ts_fn_init(struct sec_ts_data *ts);

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

#endif
