/*
 *
 * Zinitix bt541 touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */


#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if defined(CONFIG_PM_RUNTIME)
#include <linux/pm_runtime.h>
#endif
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/async.h>

#include "bt541_ts.h"
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>

/* sec config */
#define TSP_VERBOSE_DEBUG
#define CONFIG_SEC_FACTORY_TEST
#define SUPPORTED_TOUCH_KEY
#define BT541_USE_INPUT_OPEN_CLOSE	1

#ifdef CONFIG_SEC_FACTORY_TEST
#include "linux/input/sec_cmd.h"
#include <linux/sec_class.h>
#endif
/* MTK DMA I2C*/
#define TPD_SUPPORT_I2C_DMA  1

/* PAT MODE */
#define PAT_CONTROL

#if TPD_SUPPORT_I2C_DMA
#include <linux/dma-mapping.h>
#define _ERROR(e)      ((0x01 << e) | (0x01 << (sizeof(s32) * 8 - 1)))
#define eRROR	       _ERROR(1)
#define GTP_ADDR_LENGTH 		0
#define I2C_MASTER_CLOCK	      400
#define IIC_DMA_MAX_TRANSFER_SIZE     250
#define ERROR_IIC      _ERROR(2)
#endif

#define CHECK_HWID				0

/*zinitix support define */
#define ZINITIX_DEBUG				1
#define ZINITIX_I2C_CHECKSUM			1
#define NOT_SUPPORTED_TOUCH_DUMMY_KEY

#ifdef SUPPORTED_PALM_TOUCH
#define TOUCH_POINT_MODE			2
#else
#define TOUCH_POINT_MODE			0
#endif

#define MAX_SUPPORTED_FINGER_NUM		2 /* max 10 */

#define TC_SECTOR_SZ				8

/* Touch Key define*/
#ifdef SUPPORTED_TOUCH_KEY
#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
#define MAX_SUPPORTED_BUTTON_NUM		2 /* max 8 */
#define SUPPORTED_BUTTON_NUM			2
#else
#define MAX_SUPPORTED_BUTTON_NUM		2 /* max 8 */
#define SUPPORTED_BUTTON_NUM			2
#endif
#endif

/* Upgrade Method*/
#define TOUCH_ONESHOT_UPGRADE			1
/* if you use isp mode, you must add i2c device :
   name = "zinitix_isp" , addr 0x50*/

/* resolution offset */
#define ABS_PT_OFFSET				(0)

#define TOUCH_FORCE_UPGRADE			1
#define USE_CHECKSUM				1

#define CHIP_OFF_DELAY				50 /*ms*/
#define CHIP_ON_DELAY				20 /*ms*/
#define FIRMWARE_ON_DELAY			60 /*ms*/

#define DELAY_FOR_SIGNAL_DELAY			30 /*us*/
#define DELAY_FOR_TRANSCATION			50
#define DELAY_FOR_POST_TRANSCATION		10
#define IUM_SET_TIMEOUT				64 /*1.7s*/
#define BT541_HZ				1000
#define ESD_SEC_HZ				1000
/*PAT MODE*/

#ifdef PAT_CONTROL
/*------------------------------
	<<< apply to server >>>
	0x00 : no action
	0x01 : clear nv
	0x02 : pat magic
	0x03 : rfu

	<<< use for temp bin >>>
	0x05 : forced clear nv & f/w update before pat magic, eventhough same f/w
	0x06 : rfu
-------------------------------*/
#define PAT_CONTROL_NONE			0x00
#define PAT_CONTROL_CLEAR_NV			0x01
#define PAT_CONTROL_PAT_MAGIC			0x02
#define PAT_CONTROL_FORCE_UPDATE		0x05

#define PAT_MAX_LCIA				0x80
#define PAT_MAX_MAGIC				0xF5
#define PAT_MAGIC_NUMBER			0x83
/*addr*/
#define PAT_CAL_DATA				0x00
#define PAT_DUMMY_VERSION			0x02
#define PAT_FIX_VERSION 			0x04
#endif

enum power_control {
	POWER_OFF,
	POWER_ON,
	POWER_ON_SEQUENCE,
};

/* Key Enum */
enum key_event {
	ICON_BUTTON_UNCHANGE,
	ICON_BUTTON_DOWN,
	ICON_BUTTON_UP,
};

enum fw_sequence {
	fw_false = 0,
	fw_true,
	fw_force,
};


/* ESD Protection */
/*second : if 0, no use. if you have to use, 3 is recommended*/
/*if H/W TSP_ta use, delete ESD */
#define ESD_TIMER_INTERVAL			1
#define SCAN_RATE_HZ				100
#define CHECK_ESD_TIMER				3

/*Test Mode (Monitoring Raw Data) */
#define MAX_RAW_DATA_SZ				576 /* 32x18 */
#define MAX_TRAW_DATA_SZ	\
	(MAX_RAW_DATA_SZ + 4*MAX_SUPPORTED_FINGER_NUM + 2)
/* preriod raw data interval */

#define RAWDATA_DELAY_FOR_HOST			100

struct raw_ioctl {
	u32 sz;
	u32 buf;
};

struct reg_ioctl {
	u32 addr;
	u32 val;
};

#define TOUCH_SEC_MODE				48
#define TOUCH_REF_MODE				10
#define TOUCH_NORMAL_MODE			5
#define TOUCH_DELTA_MODE			3
#define TOUCH_SDND_MODE				6
#define TOUCH_CNDDATA_MODE			7 /* current raw data */
#define TOUCH_REFERENCE_MODE			8 /* nv raw data */
#define TOUCH_DND_MODE				11
#define TOUCH_H_GAP_JITTER_MODE		16
#define TOUCH_REF_ABNORMAL_TEST_MODE	33

/*  Other Things */
#define INIT_RETRY_CNT				1
#define I2C_SUCCESS				0
#define I2C_FAIL				1

/*---------------------------------------------------------------------*/

/* Register Map*/
#define BT541_SWRESET_CMD			0x0000
#define BT541_WAKEUP_CMD			0x0001

#define BT541_IDLE_CMD				0x0004
#define BT541_SLEEP_CMD				0x0005

#define BT541_CLEAR_INT_STATUS_CMD		0x0003
#define BT541_CALIBRATE_CMD			0x0006
#define BT541_SAVE_STATUS_CMD			0x0007
#define BT541_SAVE_CALIBRATION_CMD		0x0008
#define BT541_RECALL_FACTORY_CMD		0x000f

#define BT541_THRESHOLD				0x0020

#define BT541_DEBUG_REG				0x0115 /* 0~7 */

#define BT541_TOUCH_MODE			0x0010
#define BT541_CHIP_REVISION			0x0011
#define BT541_FIRMWARE_VERSION			0x0012

#define BT541_MINOR_FW_VERSION			0x0121

#define BT541_VENDOR_ID				0x001C
#define BT541_MODULE_ID				0x001E
#define BT541_HW_ID				0x0014

#define BT541_DATA_VERSION_REG			0x0013
#define BT541_SUPPORTED_FINGER_NUM		0x0015
#define BT541_EEPROM_INFO			0x0018
#define BT541_INITIAL_TOUCH_MODE		0x0019

#define BT541_TOTAL_NUMBER_OF_X			0x0060
#define BT541_TOTAL_NUMBER_OF_Y			0x0061

#define BT541_DELAY_RAW_FOR_HOST		0x007f

#define BT541_BUTTON_SUPPORTED_NUM		0x00B0
#define BT541_BUTTON_SENSITIVITY		0x00B2
#define BT541_DUMMY_BUTTON_SENSITIVITY		0X00C8
#define BT541_BTN_WIDTH				0x016d
#define BT541_REAL_WIDTH			0x01c0


#define BT541_X_RESOLUTION			0x00C0
#define BT541_Y_RESOLUTION			0x00C1

#define BT541_POINT_STATUS_REG			0x0080
#define BT541_ICON_STATUS_REG			0x00AA

#define BT541_AFE_FREQUENCY			0x0100
#define BT541_DND_N_COUNT			0x0122
#define BT541_DND_U_COUNT			0x0135
#define BT541_SHIFT_VALUE			0x012B
#define BT541_ISRC_CTRL				0x014F

#define BT541_RAWDATA_REG			0x0200

#define BT541_EEPROM_INFO_REG			0x0018

#define BT541_INT_ENABLE_FLAG			0x00f0
#define BT541_PERIODICAL_INTERRUPT_INTERVAL	0x00f1

#define BT541_CHECKSUM_RESULT			0x012c

#define BT541_INIT_FLASH			0x01d0
#define BT541_WRITE_FLASH			0x01d1
#define BT541_READ_FLASH			0x01d2

#define ZINITIX_INTERNAL_FLAG_02		0x011e

#define BT541_OPTIONAL_SETTING			0x0116

/* Interrupt & status register flag bit
   -------------------------------------------------
 */
#define BIT_PT_CNT_CHANGE			0
#define BIT_DOWN				1
#define BIT_MOVE				2
#define BIT_UP					3
#define BIT_PALM				4
#define BIT_PALM_REJECT				5
#define RESERVED_0				6
#define RESERVED_1				7
#define BIT_WEIGHT_CHANGE			8
#define BIT_PT_NO_CHANGE			9
#define BIT_REJECT				10
#define BIT_PT_EXIST				11
#define RESERVED_2				12
#define BIT_MUST_ZERO				13
#define BIT_DEBUG				14
#define BIT_ICON_EVENT				15

/* button */
#define BIT_O_ICON0_DOWN			0
#define BIT_O_ICON1_DOWN			1
#define BIT_O_ICON2_DOWN			2
#define BIT_O_ICON3_DOWN			3
#define BIT_O_ICON4_DOWN			4
#define BIT_O_ICON5_DOWN			5
#define BIT_O_ICON6_DOWN			6
#define BIT_O_ICON7_DOWN			7

#define BIT_O_ICON0_UP				8
#define BIT_O_ICON1_UP				9
#define BIT_O_ICON2_UP				10
#define BIT_O_ICON3_UP				11
#define BIT_O_ICON4_UP				12
#define BIT_O_ICON5_UP				13
#define BIT_O_ICON6_UP				14
#define BIT_O_ICON7_UP				15


#define SUB_BIT_EXIST				0
#define SUB_BIT_DOWN				1
#define SUB_BIT_MOVE				2
#define SUB_BIT_UP				3
#define SUB_BIT_UPDATE				4
#define SUB_BIT_WAIT				5


#define zinitix_bit_set(val, n)		((val) &= ~(1<<(n)), (val) |= (1<<(n)))
#define zinitix_bit_clr(val, n)		((val) &= ~(1<<(n)))
#define zinitix_bit_test(val, n)	((val) & (1<<(n)))
#define zinitix_swap_v(a, b, t)		((t) = (a), (a) = (b), (b) = (t))
#define zinitix_swap_16(s)		(((((s) & 0xff) << 8) | (((s) >> 8) & 0xff)))

/* end header file */

#ifdef CONFIG_SEC_FACTORY_TEST

#define MAX_FW_PATH 255
#define TSP_FW_FILENAME "zinitix_fw.bin"
#define TSP_CMD_X_NUM		19
#define TSP_CMD_Y_NUM		10
#define TSP_CMD_NODE_NUM	(TSP_CMD_X_NUM * TSP_CMD_Y_NUM)

struct tsp_raw_data {
	s16 reference_data_abnormal[TSP_CMD_NODE_NUM];
	s16 dnd_data[TSP_CMD_NODE_NUM];
	s16 hfdnd_data[TSP_CMD_NODE_NUM];
	s32 hfdnd_data_sum[TSP_CMD_NODE_NUM];
	s16 delta_data[TSP_CMD_NODE_NUM];
	s16 vgap_data[TSP_CMD_NODE_NUM];
	s16 hgap_data[TSP_CMD_NODE_NUM];
	s16 reference_data[TSP_CMD_NODE_NUM*2 + TSP_CMD_NODE_NUM];
	s16 gapjitter_data[TSP_CMD_NODE_NUM];
};

#if defined(CONFIG_SAMSUNG_LPM_MODE)
	extern unsigned int poweroff_charging;
#endif

static void fw_update(void *device_data);
static void get_fw_ver_bin(void *device_data);
static void get_fw_ver_ic(void *device_data);
static void module_off_master(void *device_data);
static void module_on_master(void *device_data);
static void module_off_slave(void *device_data);
static void module_on_slave(void *device_data);
static void get_chip_vendor(void *device_data);
static void get_chip_name(void *device_data);
static void get_threshold(void *device_data);
static void get_x_num(void *device_data);
static void get_y_num(void *device_data);
static void not_support_cmd(void *device_data);

/* Vendor dependant command */
static void get_reference(void *device_data);
static void get_dnd(void * device_data);
static void get_hfdnd(void * device_data);
static void get_dnd_v_gap(void * device_data);
static void get_dnd_h_gap(void * device_data);
static void get_hfdnd_v_gap(void * device_data);
static void get_hfdnd_h_gap(void * device_data);
static void get_delta(void *device_data);
static void run_reference_read (void *device_data);
static void run_dnd_read(void *device_data);
static void run_hfdnd_read(void *device_data);
static void run_dnd_v_gap_read(void *device_data);
static void run_dnd_h_gap_read(void * device_data);
static void run_hfdnd_v_gap_read(void *device_data);
static void run_hfdnd_h_gap_read(void * device_data);
static void run_delta_read(void *device_data);
static void hfdnd_spec_adjust(void *device_data);
static void clear_reference_data(void *device_data);
static void run_gapjitter_read(void *device_data);
static void get_gapjitter(void *device_data);
static void run_force_calibration(void *device_data);
#ifdef PAT_CONTROL
static void get_pat_information(void *device_data);
static void get_calibration_nv_data(void *device_data);
static void get_tune_fix_ver_data(void *device_data);
static void set_calibration_nv_data(void *device_data);
static void set_tune_fix_ver_data(void *device_data);
#endif

static void dead_zone_enable(void *device_data);
static void run_mis_cal_read(void *device_data);
static void get_mis_cal(void *device_data);


static struct sec_cmd bt541_commands[] = {
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("module_off_master", module_off_master),},
	{SEC_CMD("module_on_master", module_on_master),},
	{SEC_CMD("module_off_slave", module_off_slave),},
	{SEC_CMD("module_on_slave", module_on_slave),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},

	/* vendor dependant command */
	{SEC_CMD("run_reference_read", run_reference_read),},
	{SEC_CMD("get_dnd_all_data", get_reference),},
	{SEC_CMD("run_delta_read", run_delta_read),},
	{SEC_CMD("get_delta_all_data", get_delta),},
	{SEC_CMD("run_dnd_read", run_dnd_read),},
	{SEC_CMD("get_dnd", get_dnd),},
	{SEC_CMD("run_dnd_v_gap_read", run_dnd_v_gap_read),},
	{SEC_CMD("get_dnd_v_gap", get_dnd_v_gap),},
	{SEC_CMD("run_dnd_h_gap_read", run_dnd_h_gap_read),},
	{SEC_CMD("get_dnd_h_gap", get_dnd_h_gap),},
	{SEC_CMD("run_hfdnd_read", run_hfdnd_read),},
	{SEC_CMD("get_hfdnd", get_hfdnd),},
	{SEC_CMD("run_hfdnd_v_gap_read", run_hfdnd_v_gap_read),},
	{SEC_CMD("get_hfdnd_v_gap", get_hfdnd_v_gap),},
	{SEC_CMD("run_hfdnd_h_gap_read", run_hfdnd_h_gap_read),},
	{SEC_CMD("get_hfdnd_h_gap", get_hfdnd_h_gap),},
	{SEC_CMD("run_gapjitter_read", run_gapjitter_read),},
	{SEC_CMD("get_gapjitter", get_gapjitter),},
	{SEC_CMD("hfdnd_spec_adjust", hfdnd_spec_adjust),},
	{SEC_CMD("clear_reference_data", clear_reference_data),},
	{SEC_CMD("run_force_calibration", run_force_calibration),},
#ifdef PAT_CONTROL
	{SEC_CMD("get_pat_information", get_pat_information),},
	{SEC_CMD("get_calibration_nv_data", get_calibration_nv_data),},
	{SEC_CMD("get_tune_fix_ver_data", get_tune_fix_ver_data),},
	{SEC_CMD("set_calibration_nv_data", set_calibration_nv_data),},
	{SEC_CMD("set_tune_fix_ver_data", set_tune_fix_ver_data),},
#endif
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("run_mis_cal_read", run_mis_cal_read),},
	{SEC_CMD("get_mis_cal", get_mis_cal),},
};
#endif

#define TSP_NORMAL_EVENT_MSG			1
static int m_ts_debug_mode = ZINITIX_DEBUG;

#if ESD_TIMER_INTERVAL
static struct workqueue_struct *esd_tmr_workqueue;
#endif

struct coord {
	u16	x;
	u16	y;
	u8	width;
	u8	sub_status;
#if (TOUCH_POINT_MODE == 2)
	u8	minor_width;
	u8	angle;
#endif
};

struct point_info {
	u16	status;
#if (TOUCH_POINT_MODE == 1)
	u16	event_flag;
#else
	u8	finger_cnt;
	u8	time_stamp;
#endif
	struct coord coord[MAX_SUPPORTED_FINGER_NUM];
};
struct sec_point_info {
//	u8	finger_cnt;
	u8	finger_state;
	int	move_count;
};

#define TOUCH_V_FLIP				0x01
#define TOUCH_H_FLIP				0x02
#define TOUCH_XY_SWAP				0x04

/*Test Mode (Monitoring Raw Data) */
/*---------------------------------------------------------------------*/
/* GF1 */
/*---------------------------------------------------------------------*/
#define TSP_INIT_TEST_RATIO			100

#define	SEC_DND_N_COUNT				20
#define	SEC_DND_U_COUNT				10
#define	SEC_DND_FREQUENCY			239		/*100khz*/

#define	SEC_HFDND_N_COUNT			20
#define	SEC_HFDND_U_COUNT			10
#define	SEC_HFDND_FREQUENCY			47		/*500khz*/

#define SEC_ISRC_CTRL				0x0F00

/*---------------------------------------------------------------------*/
struct capa_info {
	u16	vendor_id;
	u16	ic_revision;
	u16	fw_version;
	u16	fw_minor_version;
	u16	reg_data_version;
	u16	threshold;
	u16	key_threshold;
	u16	dummy_threshold;
	u32	ic_fw_size;
	u32	MaxX;
	u32	MaxY;
	u32	MinX;
	u32	MinY;
	u8	gesture_support;
	u16	multi_fingers;
	u16	button_num;
	u16	ic_int_mask;
	u16	x_node_num;
	u16	y_node_num;
	u16	total_node_num;
	u16	hw_id;
	u16	module_id;
	u16	afe_frequency;
	u16	shift_value;
	u16	N_cnt;
	u16	U_cnt;
	u16 isrc_ctrl;
	u16	i2s_checksum;
};

enum work_state {
	NOTHING = 0,
	NORMAL,
	ESD_TIMER,
	EALRY_SUSPEND,
	SUSPEND,
	RESUME,
	LATE_RESUME,
	UPGRADE,
	REMOVE,
	SET_MODE,
	HW_CALIBRAION,
	RAW_DATA,
	PROBE,
};

enum {
	BUILT_IN = 0,
	UMS,
	REQ_FW,
};

struct bt541_ts_info;

struct bt541_ts_platform_data {
	u32			gpio_int;
	u32			gpio_ldo_en;
	u32			x_resolution;
	u32			y_resolution;
	u32			page_size;
	u32			orientation;
	int			bringup;
	int			vdd_en;
	int			vdd_en_flag;
	int			mis_cal_check;
	const char		*fw_name;
	struct regulator	*vreg_vio;
	bool			support_lpm;
#ifdef PAT_CONTROL
	int pat_function;
	int afe_base;
#endif

};

struct bt541_ts_info {
	struct i2c_client			*client;
	struct input_dev			*input_dev;
	struct bt541_ts_platform_data		*pdata;
	struct pinctrl				*pinctrl;
	char					phys[32];
	struct capa_info			cap_info;
	struct point_info			touch_info;
	struct point_info			reported_touch_info;
	struct sec_point_info			sec_point_info[MAX_SUPPORTED_FINGER_NUM];
	u16					icon_event_reg;
	u16					prev_icon_event;
	int					irq;
	u8					button[MAX_SUPPORTED_BUTTON_NUM];
	u8					work_state;
	struct semaphore			work_lock;
#if ESD_TIMER_INTERVAL
	struct work_struct			tmr_work;
	struct timer_list			esd_timeout_tmr;
	struct timer_list			*p_esd_timeout_tmr;
	spinlock_t				lock;
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend			early_suspend;
#endif
	struct semaphore			raw_data_lock;
	u16					touch_mode;
	s16					cur_data[MAX_TRAW_DATA_SZ];
	u8					update;
	int					octa_id;
	bool					enabled;
#ifdef CONFIG_SEC_FACTORY_TEST
	struct sec_cmd_data			sec;
	struct tsp_raw_data			*raw_data;
	int					touch_count;
	s16 Gap_max_x;
	s16 Gap_max_y;
	s16 Gap_max_val;
	s16 Gap_min_x;
	s16 Gap_min_y;
	s16 Gap_min_val;
	s16 Gap_Gap_val;
	s16 Gap_node_num;
	u32 version;
	bool latest_flag;
	bool pat_flag;
#endif
#ifdef PAT_CONTROL
	int cal_count;
	int tune_fix_ver;
#endif
	bool ium_lock_enable;
};
/* Dummy touchkey code */
#define KEY_DUMMY_HOME1				249
#define KEY_DUMMY_HOME2				250
#define KEY_DUMMY_MENU				251
#define KEY_DUMMY_HOME				252
#define KEY_DUMMY_BACK				253
/*<= you must set key button mapping*/
#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
	KEY_RECENT, KEY_BACK};
#else
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
	KEY_DUMMY_MENU, KEY_RECENT,// KEY_DUMMY_HOME1,
	/*KEY_DUMMY_HOME2,*/ KEY_BACK, KEY_DUMMY_BACK};
#endif

static int bt541_ts_open(struct input_dev *dev);
static void bt541_ts_close(struct input_dev *dev);
int bt541_pinctrl_configure(struct bt541_ts_info *info, int active);



#if TPD_SUPPORT_I2C_DMA
static s32 i2c_dma_read_mtk(struct bt541_ts_info *info,char *write_buf, unsigned int wlen, u8 *buffer, s32 len);
static s32 i2c_dma_write_mtk(struct bt541_ts_info *info, u8 *buffer, s32 len);
static u8 *gpDMABuf_va;
static dma_addr_t gpDMABuf_pa;
struct mutex dma_mutex;
DEFINE_MUTEX(dma_mutex);

static s32 i2c_dma_write_mtk(struct bt541_ts_info *info, u8 *buffer, s32 len)
{
	s32 ret = 0;
	s32 pos = 0;
	s32 transfer_length;
	u16 address = 0;
	int count = 0;
	struct i2c_msg msg = {
		.flags = !I2C_M_RD,
		.ext_flag = (info->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		.addr = (info->client->addr & I2C_MASK_FLAG),
		.timing = I2C_MASTER_CLOCK,
		.buf = (u8 *)(uintptr_t)gpDMABuf_pa,
	};

	if (buffer != NULL)
		address = (buffer[0]<<8) | buffer[1];

	mutex_lock(&dma_mutex);
	while (pos != len) {
		if (len - pos > (IIC_DMA_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH))
			transfer_length = IIC_DMA_MAX_TRANSFER_SIZE - GTP_ADDR_LENGTH;
		else
			transfer_length = len - pos;

		gpDMABuf_va[0] = (address >> 8) & 0xFF;
		gpDMABuf_va[1] = address & 0xFF;
		memcpy(&gpDMABuf_va[GTP_ADDR_LENGTH], &buffer[pos], transfer_length);

		msg.len = transfer_length + GTP_ADDR_LENGTH;
		//if (!info->is_lpm_suspend) {/*workround log too much*/
retry:
			ret = i2c_transfer(info->client->adapter, &msg, 1);
			if (ret < 0) {
			input_err(true, &info->client->dev, "%s I2c Transfer error! ", __func__);
				if (++count < 3)
					goto retry;
				ret = ERROR_IIC;
				break;
			}
		//} else {
		//	ret = ERROR_IIC;
		//	break;
		//}
		ret = 0;
		pos += transfer_length;
		address += transfer_length;
	}
	mutex_unlock(&dma_mutex);
	return ret;
}

static s32 i2c_dma_read_mtk(struct bt541_ts_info *info ,char *write_buf, unsigned int wlen, u8 *buffer, s32 len)
{
	s32 ret = eRROR;
	s32 pos = 0;
	s32 transfer_length;
	u16 address = 0;
	int count = 0;
	//u8 addr_buf[GTP_ADDR_LENGTH] = { 0 };

	struct i2c_msg msgs[2] = {
		{
		 .flags = 0,	/*!I2C_M_RD,*/
		 .addr = ( info->client->addr & I2C_MASK_FLAG),
		 .timing = I2C_MASTER_CLOCK,
		 .len = wlen,
		 .buf = write_buf,
		 },
		{
		 .flags = I2C_M_RD,
		 .ext_flag = (info->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
		 .addr = (info->client->addr & I2C_MASK_FLAG),
		 .timing = I2C_MASTER_CLOCK,
		 .buf = (u8 *)(uintptr_t)gpDMABuf_pa,
		},
	};

	if (write_buf != NULL)
		address = (write_buf[0]<<8) | write_buf[1];

	mutex_lock(&dma_mutex);
	while (pos != len) {
		if (len - pos > IIC_DMA_MAX_TRANSFER_SIZE)
			transfer_length = IIC_DMA_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;

		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;
retry:
		ret = i2c_transfer(info->client->adapter, &msgs[0], 1);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s I2C Transfer error!", __func__);
			if (++count < 3)
				goto retry;
			ret = ERROR_IIC;
			goto out;
		}

		usleep_range(200, 200);

		ret = i2c_transfer(info->client->adapter, &msgs[1], 1);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s I2C Transfer error!", __func__);
			ret = ERROR_IIC;
			break;
		}
		
		ret = 0;
		memcpy(&buffer[pos], gpDMABuf_va, transfer_length);
		pos += transfer_length;
		address += transfer_length;
	};
out:
	mutex_unlock(&dma_mutex);
	return ret;
}

#endif


static int cal_mode;
static int get_boot_mode(char *str)
{
	get_option(&str, &cal_mode);
	printk("%s get_boot_mode, uart_mode : %d\n", SECLOG, cal_mode);
	return 1;
}
__setup("calmode=", get_boot_mode);
/* define i2c sub functions*/
static inline s32 read_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;
	int count = 0;

#if TPD_SUPPORT_I2C_DMA
	ret = i2c_dma_read_mtk(info, (u8 *)&reg, 2, values, length);
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return ret;
#else	
retry:
	/* select register*/
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		usleep_range(BT541_HZ, BT541_HZ);
		if (++count < 8)
			goto retry;

		return ret;
	}
	/* for setup tx transaction. */
	usleep_range(DELAY_FOR_TRANSCATION, DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;
#endif
}

static inline s32 write_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;
	int count = 0;
	u8 pkt[10]; /* max packet */
	pkt[0] = (reg) & 0xff; /* reg addr */
	pkt[1] = (reg >> 8)&0xff;
	memcpy((u8 *)&pkt[2], values, length);

#if TPD_SUPPORT_I2C_DMA
	ret = i2c_dma_write_mtk(info, pkt, length + 2);
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return ret;

#else
retry:
	ret = i2c_master_send(client , pkt , length + 2);
	if (ret < 0) {
		usleep_range(BT541_HZ, BT541_HZ);

		if (++count < 8)
			goto retry;

		input_info(true, &info->client->dev, "%s, count = %d\n", __func__, count);

		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;	
#endif
}

static inline s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;

	return I2C_SUCCESS;
}

static inline s32 write_cmd(struct i2c_client *client, u16 reg)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;
	int count = 0;

#if TPD_SUPPORT_I2C_DMA
	ret = i2c_dma_write_mtk(info, (u8 *)&reg, 2);
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return ret;
	
#else
retry:
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		usleep_range(BT541_HZ, BT541_HZ);

		if (++count < 8)
			goto retry;

		return ret;
	}

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return I2C_SUCCESS;
#endif
}

static inline s32 read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;
	int count = 0;

#if TPD_SUPPORT_I2C_DMA
	ret = i2c_dma_read_mtk(info, (u8 *)&reg, 2, values, length);
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return ret;	
#else
retry:
	/* select register */
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		usleep_range(BT541_HZ, BT541_HZ);

		if (++count < 8)
			goto retry;

		return ret;
	}

	/* for setup tx transaction. */
	usleep_range(200, 200);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;

	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;
#endif
}

static inline s32 read_firmware_data(struct i2c_client *client,
		u16 addr, u8 *values, u16 length)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	int ret = 0;
	/* select register*/
#if TPD_SUPPORT_I2C_DMA
	ret = i2c_dma_read_mtk(info, (u8 *)&addr, 2, values, length);
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return ret;
#else
	ret = i2c_master_send(client , (u8 *)&addr , 2);
	if (ret < 0)
		return ret;

	/* for setup tx transaction. */
	usleep_range(BT541_HZ, BT541_HZ);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;
	usleep_range(DELAY_FOR_POST_TRANSCATION, DELAY_FOR_POST_TRANSCATION);
	return length;
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bt541_ts_early_suspend(struct early_suspend *h);
static void bt541_ts_late_resume(struct early_suspend *h);
#endif

static bool bt541_power_control(struct bt541_ts_info *info, u8 ctl);
static int bt541_power(struct bt541_ts_info *info, int enable);

static bool init_touch(struct bt541_ts_info *info, int fw_oneshot_upgrade);
static bool mini_init_touch(struct bt541_ts_info *info);
static void clear_report_data(struct bt541_ts_info *info);
#if ESD_TIMER_INTERVAL
static void esd_timer_start(u16 sec, struct bt541_ts_info *info);
static void esd_timer_stop(struct bt541_ts_info *info);
static void esd_timer_init(struct bt541_ts_info *info);
static void esd_timeout_handler(unsigned long data);
#endif

static long ts_misc_fops_ioctl(struct file *filp, unsigned int cmd,
		unsigned long arg);
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	.unlocked_ioctl = ts_misc_fops_ioctl,
	.compat_ioctl = ts_misc_fops_ioctl,
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE			0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION		_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE		_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA		_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION		_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG			_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG			_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)

struct bt541_ts_info *misc_info;

static u16 m_optional_mode = 0;
static u16 m_prev_optional_mode = 0;
static void bt541_set_optional_mode(struct bt541_ts_info *info, bool force)
{
	if (m_prev_optional_mode == m_optional_mode && !force)
		return;

	if (write_reg(info->client, BT541_OPTIONAL_SETTING, m_optional_mode) == I2C_SUCCESS) {
		m_prev_optional_mode = m_optional_mode;
		input_info(true, &misc_info->client->dev, "TA setting changed to %d\n",
				m_optional_mode&0x1);
	}
}

#define I2C_BUFFER_SIZE 64
static bool get_raw_data(struct bt541_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct bt541_ts_platform_data *pdata = info->pdata;
	u32 total_node = info->cap_info.total_node_num;
	s32 sz;
	int i;
	u32 temp_sz;
	int retry = 1000;

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "other process occupied.. (%d)\n", info->work_state);
		up(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int) && retry-- > 0)
			msleep(1);

		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
		msleep(1);
	}

	input_info(true, &info->client->dev, "read raw data\r\n");
	sz = total_node * 2;
	retry = 50;
	while (gpio_get_value(pdata->gpio_int) && retry-- > 0 )
		msleep(1);
	
	if (retry < 0) {
		input_info(true, &info->client->dev, "%s failed\n", __func__ );
		info->work_state = NOTHING;
		up(&info->work_lock);
		return false;
	}
	
	for (i = 0; sz > 0; i++) {
		temp_sz = I2C_BUFFER_SIZE;
		if (sz < I2C_BUFFER_SIZE)
			temp_sz = sz;
		if (read_raw_data(client, BT541_RAWDATA_REG + i, 
			(char *)(buff + (i * I2C_BUFFER_SIZE)), temp_sz) < 0) {
			input_err(true, &misc_info->client->dev, "error : read zinitix tc raw data\n");
			info->work_state = NOTHING;
			up(&info->work_lock);
			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}
	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	up(&info->work_lock);

	return true;
}

static bool ts_get_raw_data(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	s32 sz;
	u32 temp_sz;
	int i;

	if (down_trylock(&info->raw_data_lock)) {
		input_err(true, &client->dev, "Failed to occupy sema\n");
		info->touch_info.status = 0;
		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info);
	for (i = 0; sz > 0; i++) {
		temp_sz = I2C_BUFFER_SIZE;
		if (sz	< I2C_BUFFER_SIZE)
			temp_sz = sz;
		if (read_raw_data(info->client, BT541_RAWDATA_REG + i,
					(char *)((u8*)(info->cur_data) + (i * I2C_BUFFER_SIZE)), temp_sz) < 0) {
			input_err(true, &client->dev, "Failed to read raw data %d, %d \n", i, temp_sz);
			up(&info->raw_data_lock);
			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}

	info->update = 1;
	memcpy((u8 *)(&info->touch_info),
			(u8 *)&info->cur_data[total_node],
			sizeof(struct point_info));
	up(&info->raw_data_lock);

	return true;
}

#if ZINITIX_I2C_CHECKSUM
#define ZINITIX_I2C_CHECKSUM_WCNT		0x016a
#define ZINITIX_I2C_CHECKSUM_RESULT		0x016c
static bool i2c_checksum(struct bt541_ts_info *info, s16 *pChecksum, u16 wlength)
{
	s16 checksum_result;
	s16 checksum_cur;
	int i;

	checksum_cur = 0;
	for (i = 0; i < wlength; i++) {
		checksum_cur += (s16)pChecksum[i];
	}
	if (read_data(info->client,
				ZINITIX_I2C_CHECKSUM_RESULT,
				(u8 *)(&checksum_result), 2) < 0) {
		input_err(true, &info->client->dev, "error read i2c checksum rsult.-\n");
		return false;
	}
	if (checksum_cur != checksum_result) {
		input_err(true, &info->client->dev, "checksum error : %d, %d\n", checksum_cur, checksum_result);
		return false;
	}
	return true;
}

#endif

static bool ts_read_coord(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
#if (TOUCH_POINT_MODE == 1)
	int i;
#endif

	/* for	Debugging Tool */

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (info->update == 0) {
			if (ts_get_raw_data(info) == false)
				return false;
		} else {
			info->touch_info.status = 0;
		}

		input_err(true, &client->dev, "status = 0x%04X\n", info->touch_info.status);

		goto out;
	}

#if (TOUCH_POINT_MODE == 1)
	memset(&info->touch_info,
			0x0, sizeof(struct point_info));

#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum)
		if ((write_reg(info->client, ZINITIX_I2C_CHECKSUM_WCNT, 2)) != I2C_SUCCESS)
			return false;

#endif
	if (read_data(info->client, BT541_POINT_STATUS_REG, (u8 *)(&info->touch_info), 4) < 0) {
		input_err(true, &client->dev, "%s: Failed to read point info\n", __func__);

		return false;
	}

#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum)
		if (i2c_checksum(info, (s16 *)(&info->touch_info), 2) == false)
		return false;
#endif
	input_info(true, &client->dev, "status reg = 0x%x , event_flag = 0x%04x\n",
			info->touch_info.status, info->touch_info.event_flag);

	bt541_set_optional_mode(info, false);
	if (info->touch_info.event_flag == 0)
		goto out;

#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum)
		if ((write_reg(info->client, ZINITIX_I2C_CHECKSUM_WCNT, sizeof(struct point_info)/2)) != I2C_SUCCESS)
			return false;
#endif
	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (zinitix_bit_test(info->touch_info.event_flag, i)) {
			usleep_range(20, 20);

			if (read_data(info->client, BT541_POINT_STATUS_REG + 2 + ( i * 4),
						(u8 *)(&info->touch_info.coord[i]),
						sizeof(struct coord)) < 0) {
				input_err(true, &client->dev, "Failed to read point info\n");

				return false;
			}
#if ZINITIX_I2C_CHECKSUM
			if (info->cap_info.i2s_checksum)
				if (i2c_checksum(info, (s16 *)(&info->touch_info.coord[i]), sizeof(struct point_info)/2) == false)
					return false;
#endif
		}
	}

#else
#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum)
		if (write_reg(info->client,
					ZINITIX_I2C_CHECKSUM_WCNT,
					(sizeof(struct point_info)/2)) != I2C_SUCCESS) {
			input_err(true, &client->dev, "error write checksum wcnt.-\n");
			return false;
		}
#endif
	if (read_data(info->client, BT541_POINT_STATUS_REG,
				(u8 *)(&info->touch_info), sizeof(struct point_info)) < 0) {
		input_err(true, &client->dev, "Failed to read point info\n");

		return false;
	}
#if ZINITIX_I2C_CHECKSUM
	if (info->cap_info.i2s_checksum)
		if (i2c_checksum(info, (s16 *)(&info->touch_info), sizeof(struct point_info)/2) == false)
			return false;
#endif

	bt541_set_optional_mode(info, false);

#endif

out:
	/* error */
	if (zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO)) {
		input_err(true, &client->dev, "Invalid must zero bit(%04x)\n",
				info->touch_info.status);

		return false;
	}

	write_cmd(info->client, BT541_CLEAR_INT_STATUS_CMD);

	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(unsigned long data)
{
	struct bt541_ts_info *info = (struct bt541_ts_info *)data;

	info->p_esd_timeout_tmr = NULL;
	queue_work(esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct bt541_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock,flags);
	if (info->p_esd_timeout_tmr != NULL)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
	del_timer(info->p_esd_timeout_tmr);
#endif
	info->p_esd_timeout_tmr = NULL;
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->esd_timeout_tmr.expires = jiffies + msecs_to_jiffies(sec * ESD_SEC_HZ);
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	add_timer(&info->esd_timeout_tmr);
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_stop(struct bt541_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock,flags);
	if (info->p_esd_timeout_tmr)
#ifdef CONFIG_SMP
		del_singleshot_timer_sync(info->p_esd_timeout_tmr);
#else
	del_timer(info->p_esd_timeout_tmr);
#endif

	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void esd_timer_init(struct bt541_ts_info *info)
{
	unsigned long flags;
	spin_lock_irqsave(&info->lock,flags);
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->p_esd_timeout_tmr = NULL;
	spin_unlock_irqrestore(&info->lock, flags);
}

static void ts_tmr_work(struct work_struct *work)
{
	struct bt541_ts_info *info =
		container_of(work, struct bt541_ts_info, tmr_work);
	struct i2c_client *client = info->client;

#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "tmr queue work ++\n");
#endif

	if (down_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		esd_timer_start(CHECK_ESD_TIMER, info);

		return;
	}

	if (info->work_state != NOTHING) {
		input_info(true, &client->dev, "%s: Other process occupied (%d)\n",
				__func__, info->work_state);
		up(&info->work_lock);

		return;
	}

	if(info->ium_lock_enable == true)
		input_info(true, &client->dev, "ium_lock\n", __func__);

	info->work_state = ESD_TIMER;

	disable_irq(info->irq);
	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	clear_report_data(info);
	if (mini_init_touch(info) == false)
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "tmr queue work--\n");
#endif

	return;
fail_time_out_init:
	input_err(true, &client->dev, "%s: Failed to restart\n", __func__);
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return;
}
#endif

static bool bt541_power_sequence(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retry = 0;
	u16 chip_code;

retry_power_sequence:
	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(vendor cmd enable)\n");
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "Failed to read chip code\n");
		goto fail_power_sequence;
	}

	input_info(true, &client->dev, "%s: chip code = 0x%x\n", __func__, chip_code);
	usleep_range(10, 10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(intn clear)\n");
		goto fail_power_sequence;
	}
	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(nvm init)\n");
		goto fail_power_sequence;
	}
	usleep_range(2*BT541_HZ, 2*BT541_HZ);

	if (write_reg(client, 0xc001, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to send power sequence(program start)\n");
		goto fail_power_sequence;
	}
	usleep_range(FIRMWARE_ON_DELAY*BT541_HZ, FIRMWARE_ON_DELAY*BT541_HZ);	/* wait for checksum cal */

	if (write_reg(client, 0x002E, IUM_SET_TIMEOUT) != I2C_SUCCESS)
		input_err(true, &client->dev, "%s: failed to set ium timeout\n", __func__);

	return true;

fail_power_sequence:
	if (retry++ < 3) {
		usleep_range(CHIP_ON_DELAY*BT541_HZ, CHIP_ON_DELAY*BT541_HZ);
		input_info(true, &client->dev, "retry = %d\n", retry);
		goto retry_power_sequence;
	}

	input_err(true, &client->dev, "Failed to send power sequence\n");
	return false;
}

static int bt541_power(struct bt541_ts_info *info, int enable)
{
	struct i2c_client *client = info->client;
	int ret = 0;

	if (info->pdata->vdd_en_flag) {
		gpio_direction_output(info->pdata->gpio_ldo_en, enable);
		if(enable == 0)
			usleep_range(CHIP_OFF_DELAY*BT541_HZ, CHIP_OFF_DELAY*BT541_HZ);
		else
			usleep_range(CHIP_ON_DELAY*BT541_HZ, CHIP_ON_DELAY*BT541_HZ);
		input_info(true, &client->dev, "%s gpio_direction_ouput:%d\n", __func__, enable);
	}

	if (!IS_ERR_OR_NULL(info->pdata->vreg_vio)) {
		if (enable) {
			if (!regulator_is_enabled(info->pdata->vreg_vio)) {
				ret = regulator_enable(info->pdata->vreg_vio);
				if (ret) {
					input_err(true, &client->dev, "%s [ERROR] touch_regulator enable "
						"failed  (%d)\n", __func__, ret);
					return -EIO;
				}
				usleep_range(CHIP_ON_DELAY*BT541_HZ, CHIP_ON_DELAY*BT541_HZ);
				input_info(true, &client->dev, "%s power on\n", __func__);

			} else {
				input_info(true, &client->dev, "%s already power on\n", __func__);
			}
		} else {
			if (regulator_is_enabled(info->pdata->vreg_vio)) {
				ret = regulator_disable(info->pdata->vreg_vio);
				if(ret) {
					input_err(true, &client->dev, "%s [ERROR] touch_regulator disable "
						"failed (%d)\n", __func__, ret);
					return -EIO;
				}
				usleep_range(CHIP_ON_DELAY*BT541_HZ, CHIP_ON_DELAY*BT541_HZ);
				input_info(true, &client->dev, "%s power off\n", __func__);

			} else {
				input_info(true, &client->dev, "%s already power off\n", __func__);
			}
		}

	}

	return 0;
}

static bool bt541_power_control(struct bt541_ts_info *info, u8 ctl)
{
	int ret = 0;

	input_info(true, &info->client->dev, "%s, %d\n", __func__, ctl);

	ret = bt541_power(info, ctl);
	if (ret)
		return false;

	if (ctl == POWER_ON_SEQUENCE) {
		return bt541_power_sequence(info);
	}
	info->ium_lock_enable = false;

	return true;
}

#define DEF_IUM_ADDR	64*2
#define DEF_IUM_ADDR_OFFSET	0xF0A0
#define DEF_IUM_LOCK	0xF0F6
#define DEF_IUM_UNLOCK	0xF0FA

bool tsp_nvm_ium_lock(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	if(info->ium_lock_enable == false)
	{
		if(write_cmd(client, DEF_IUM_LOCK))
		{
			input_err(true, &client->dev, "failed ium lock\n", __func__);
			return false;
		}
		info->ium_lock_enable = true;
		msleep(40);
	}

	return true;
}
bool tsp_nvm_ium_unlock(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;

	if(write_cmd(client, DEF_IUM_UNLOCK))
	{
		input_err(true, &client->dev, "failed ium unlock\n", __func__);
		return false;
	}

	info->ium_lock_enable = false;
	return true;
}

#ifdef PAT_CONTROL
/* Use TSP NV area
 * buff[0] : cal_count data or tune_fix_verison
 * addr : 0x00 cal_count 0x02 and 0x04 tune_fix_version and tune_dummy_fix_version
 */
void set_tsp_nvm_data(struct bt541_ts_info *info, u8 addr, u8 data)
{
	struct i2c_client *client = info->client;
	char buff[2] = { 0 };

	input_info(true, &info->client->dev, "%s\n", __func__);

	buff[0] = data;

	if (write_data(client, addr + DEF_IUM_ADDR_OFFSET,
			(u8 *)buff, 2) < 0) {
		input_err(true, &client->dev, "%s error : write zinitix \n", __func__);
		goto fail_ium_random_write;
	}

	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to write nvm wp disable\n", __func__);
		goto fail_ium_random_write;
	}
	
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	if (write_cmd(client, 0xF0F8) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed save ium\n", __func__);
		goto fail_ium_random_write;
	}
	usleep_range(30*BT541_HZ, 30*BT541_HZ);

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n", __func__);
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	return;

fail_ium_random_write:
	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	return;
	
}

int get_tsp_nvm_data(struct bt541_ts_info *info, u8 addr)
{
	struct i2c_client *client = info->client;
	char buff[2] = { 0 };

	input_info(true, &info->client->dev, "%s, addr:%u\n", __func__, addr);

	/* send NV data using command
	 * Use TSP NV area : in this model, use only one byte
	 * buff[0] : cal_count_data or tune_fix_data
	*/

	if (info->ium_lock_enable == false) {
		if (write_cmd(client, DEF_IUM_LOCK)) {
			input_err(true, &client->dev, "failed ium lock\n", __func__);
			goto fail_ium_random_read;
		}
		info->ium_lock_enable = true;
		msleep(40);
	}

	memset(&buff, 0x00, 2);
	if (read_raw_data(client, addr + DEF_IUM_ADDR_OFFSET, (u8 *)buff, 2) < 0) {
		input_err(true, &client->dev, "Failed to read raw data \n");
		goto fail_ium_random_read;
	}
	

	return buff[0];

fail_ium_random_read:

	info->ium_lock_enable = false;
	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	return -1;
	
}
/* zinitix test source */
#if 0
static void ium_random_write(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	int page_sz = info->pdata->page_size;
	u16 temp_sz, size;
	u16 chip_code;

	u8 buff[64]; // custom data buffer
	u16 length, buff_start;

	//Enable IRQ
	disable_irq(info->irq);

////////////// input custom data
	for( i=0 ; i<64 ; i++)
		buff[i] = i;
///////////// input custom data end
	
//////////data write start
	buff_start = 4;	//custom setting address(0~62)
	length = 8;		// custom odd number setting(max 64)
	if(length > TC_SECTOR_SZ)
		length = TC_SECTOR_SZ;
	if (write_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
			(u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "error : write zinitix tc firmare\n");
		goto fail_ium_random_write;
	}


	buff_start += TC_SECTOR_SZ;	//custom setting address(0~62)
	length = 6;					// custom setting(max 64)
	if(length > TC_SECTOR_SZ)
		length = TC_SECTOR_SZ;

	if (write_data(client, buff_start + DEF_IUM_ADDR_OFFSET,
			(u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "error : write zinitix tc firmare\n");
		goto fail_ium_random_write;
	}
//////////data write end

//////////for save rom start
	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to write nvm wp disable\n");
		goto fail_ium_random_write;
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	if (write_cmd(client, 0xF0F8) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed save ium\n");
		goto fail_ium_random_write;
	}
	usleep_range(30*BT541_HZ, 30*BT541_HZ);

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);
//////////for save rom end

	enable_irq(info->irq);
	return;

fail_ium_random_write:
	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->irq);
	return;
}
static void ium_random_read(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	int page_sz = info->pdata->page_size;
	u16 temp_sz, size;
	u16 chip_code;

	u8 buff[64]; // custom data buffer
	u16 length, buff_start;

	//Enable IRQ
	disable_irq(info->irq);

	for(i=0 ; i<64 ; i++)
		buff[i] = i;

	buff_start = 8;	//custom setting address(0~62)
	length = 6;		// custom setting(max 64)
	if(length > TC_SECTOR_SZ)
		length = TC_SECTOR_SZ;

	if (read_raw_data(client, buff_start + DEF_IUM_ADDR_OFFSET, (u8 *)&buff[buff_start], length) < 0) {
		input_err(true, &client->dev, "Failed to read raw data %d\n", length);
		goto fail_ium_random_read;
	}
	

	enable_irq(info->irq);
	return;

fail_ium_random_read:

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->irq);
	return;
}
static void ium_write(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int i;
	int page_sz = info->pdata->page_size;
	u32	size;
	u16 chip_code;

	u8 buff[64*3]; // custom data buffer(buff[128]~)

	////////////// input custom data
	///////////// input custom data end
	//Enable IRQ
	disable_irq(info->client->irq);
	clear_report_data(info);

	size = page_sz*3;		

	for( i=DEF_IUM_ADDR ; i<size ; i++)
			buff[i] = i;
	
	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON);
	musleep_range(10*BT541_HZ, 10*BT541_HZ);

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (vendor cmd enable)\n");
		goto fail_ium_write;
	}

	usleep_range(10, 10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (intn clear)\n");
		goto fail_ium_write;
	}

	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (nvm init)\n");
		goto fail_ium_write;
	}

	usleep_range(5*BT541_HZ, 5*BT541_HZ);

	input_info(true, &client->dev, "init flash\n");

	if (write_reg(client, 0xc003, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to write nvm vpp on\n");
		goto fail_ium_write;
	}

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
		goto fail_ium_write;
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);
	
	if (write_cmd(client, BT541_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to init flash\n");
		goto fail_ium_write;
	}

	input_info(true, &client->dev, "writing ium data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		if(flash_addr == DEF_IUM_ADDR)
		{
			if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
				input_err(true, &client->dev, "failed to write nvm wp disable\n");
				goto fail_ium_write;
			}
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
		}
		for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
			/*zinitix_debug_msg("write :addr=%04x, len=%d\n",	flash_addr, TC_SECTOR_SZ);*/
			/*zinitix_printk(KERN_INFO "writing :addr = %04x, len=%d \n", flash_addr, TC_SECTOR_SZ);*/
			if (write_data(client, BT541_WRITE_FLASH,
					(u8 *)&buff[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "error : write zinitix tc firmare\n");
				goto fail_ium_write;
			}
			flash_addr += TC_SECTOR_SZ;
			usleep_range(100, 100);

		}

		usleep_range(30*BT541_HZ, 30*BT541_HZ); /*for fuzing delay*/
	}

	if (write_reg(client, 0xc003, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm write vpp off\n");
		goto fail_ium_write;
	}

	input_info(true, &client->dev,"ium write done\n");

fail_ium_write:
	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
	}
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->client->irq);
	return;
}
static void ium_read(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	int i;
	int page_sz = info->pdata->page_size;
	u32 size;
	u16 chip_code;
	u8 buff[64*3]; // custom data buffer(buff[128]~)

	//Enable IRQ
	disable_irq(info->client->irq);
	clear_report_data(info);

	size = page_sz*3;		

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON);
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (vendor cmd enable)\n");
		goto fail_ium_read;
	}

	usleep_range(10, 10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (intn clear)\n");
		goto fail_ium_read;
	}

	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (nvm init)\n");
		goto fail_ium_read;
	}

	usleep_range(5*BT541_HZ, 5*BT541_HZ);

	input_info(true, &client->dev,"init flash\n");

	if (write_cmd(client, BT541_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to init flash\n");
		goto fail_ium_read;
	}

	input_info(true, &client->dev, "read ium data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
			/*zinitix_debug_msg("read :addr=%04x, len=%d\n",flash_addr, TC_SECTOR_SZ);*/
			if (read_firmware_data(client,
						BT541_READ_FLASH,
						(u8 *)&buff[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "Failed to read ium_data\n");

				goto fail_ium_read;
			}

			flash_addr += TC_SECTOR_SZ;
		}
	}
	for (i = 128; i < 191; i++) {
		input_info(true, &client->dev, "buf %d \n", buff[i]);
	}
	
	input_info(true, &client->dev,"ium read done\n");

fail_ium_read:

	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	enable_irq(info->client->irq);

	return;
}
#endif
#endif

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(struct bt541_ts_info *info, const u8 *firmware_data,
		u16 cur_version, u16 cur_minor_version, u16 cur_reg_version, u16 cur_hw_id)
{
	u16 new_version;
	u16 new_minor_version;
	u16 new_reg_version;
	/*u16 new_chip_code;*/
#if CHECK_HWID
	u16 new_hw_id;
#endif
	new_version = (u16) (firmware_data[0x34] | (firmware_data[0x35]<<8));
	new_minor_version = (u16) (firmware_data[0x38] | (firmware_data[0x39]<<8));
	new_reg_version = (u16) (firmware_data[0x3C] | (firmware_data[0x3D]<<8));
	/*new_chip_code = (u16) (m_firmware_data[64] | (m_firmware_data[65]<<8));*/

#if CHECK_HWID
	new_hw_id =  (u16)(firmware_data[0x7528] | (firmware_data[0x7529]<<8));
	input_info(true, &info->client->dev, "cur HW_ID = 0x%x, new HW_ID = 0x%x\n",
			cur_hw_id, new_hw_id);
#endif

	input_info(true, &info->client->dev, "cur version = 0x%x, new version = 0x%x\n",
			cur_version, new_version);

	input_info(true, &info->client->dev, "cur minor version = 0x%x, new minor version = 0x%x\n",
			cur_minor_version, new_minor_version);
	input_info(true, &info->client->dev, "cur reg data version = 0x%x, new reg data version = 0x%x\n",
			cur_reg_version, new_reg_version);

	if (cal_mode) {
		input_info(true, &info->client->dev, "didn't update TSP F/W!! in CAL MODE\n");
		return false;
	}

	if (cur_version > 0xFF)
		return true;
	if (cur_version < new_version)
		return true;
	else if (cur_version > new_version)
		return false;
#if CHECK_HWID
	if (cur_hw_id != new_hw_id)
		return true;
#endif
	if (cur_minor_version < new_minor_version)
		return true;
	else if (cur_minor_version > new_minor_version)
		return false;
	if (cur_reg_version < new_reg_version)
		return true;

	return false;
}
#endif

static u8 ts_upgrade_firmware(struct bt541_ts_info *info,
		const u8 *firmware_data, u32 size, int fw_state)
{
	struct i2c_client *client = info->client;
	u32 flash_addr;
	u16 reg_val;
	u8 *verify_data;
	int retry_cnt = 0;
	int i;
	int page_sz = info->pdata->page_size;
	u16 chip_code;

	info->latest_flag = false;
	verify_data = kzalloc(size, GFP_KERNEL);
	if (verify_data == NULL) {
		input_err(true, &client->dev, "cannot alloc verify buffer\n");
		return false;
	}

	if (fw_state == fw_false) {
		input_info(true, &client->dev, "%s don't fw update\n", __func__);
		kfree(verify_data);
		return true;
	} else if (fw_state == fw_force) {
		goto retry_upgrade;
	}
	
#ifdef PAT_CONTROL
/* PAT_CONTROL_FORCE_UPDATE : device restore */
	if (info->pdata->pat_function == PAT_CONTROL_FORCE_UPDATE) {
		input_info(true, &client->dev, "%s rune forced f/w update and excute autotune \n", __func__);
		goto retry_upgrade;
	}
#endif

	if (ts_check_need_upgrade(info, firmware_data, info->cap_info.fw_version,
		info->cap_info.fw_minor_version, info->cap_info.reg_data_version,
		info->cap_info.hw_id) == false) {
		info->latest_flag = true; // latest version flag
		input_info(true, &client->dev, "%s fw_version latest version\n", __func__);
		kfree(verify_data);
		return true;
	}


retry_upgrade:
	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON);
	usleep_range(10*BT541_HZ, 10*BT541_HZ);

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (vendor cmd enable)\n");
		goto fail_upgrade;
	}

	usleep_range(10, 10);

	if (read_data(client, 0xcc00, (u8 *)&chip_code, 2) < 0) {
		input_err(true, &client->dev, "failed to read chip code\n");
		goto fail_upgrade;
	}

	input_dbg(true, &client->dev, "chip code = 0x%x\n", chip_code);
	usleep_range(10, 10);

	flash_addr = (firmware_data[0x61]<<16) | (firmware_data[0x62]<<8) | firmware_data[0x63];
	flash_addr += ((firmware_data[0x65]<<16) | (firmware_data[0x66]<<8) | firmware_data[0x67]);
	
	if(flash_addr != 0 && flash_addr <= 0x10000)
		size = flash_addr;

	if (write_reg(client, 0xc201, 0x00be) != I2C_SUCCESS) {
		dev_err(&client->dev, "power sequence error (set clk speed)\n");
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "f/w size = 0x%x\n", size);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (intn clear)\n");
		goto fail_upgrade;
	}

	usleep_range(10, 10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "power sequence error (nvm init)\n");
		goto fail_upgrade;
	}

	usleep_range(5*BT541_HZ, 5*BT541_HZ);

	input_info(true, &client->dev, "init flash\n");

	if (write_reg(client, 0xc003, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to write nvm vpp on\n");
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to write nvm wp disable\n");
		goto fail_upgrade;
	}

	if (write_cmd(client, BT541_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to init flash\n");
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "writing firmware data\n");
	for (flash_addr = 0; flash_addr < size; ) {
#ifdef PAT_CONTROL
/* zinitix patch, devide firmware section and calibration section when fw update. */ 
		if(flash_addr == DEF_IUM_ADDR)
		{
			if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
				input_err(true, &client->dev, "nvm wp enable\n");
				goto fail_upgrade;
			}
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
			for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
				/*zinitix_debug_msg("write :addr=%04x, len=%d\n",	flash_addr, TC_SECTOR_SZ);*/
				/*zinitix_printk(KERN_INFO "writing :addr = %04x, len=%d \n", flash_addr, TC_SECTOR_SZ);*/
				if (write_data(client, BT541_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
					input_err(true, &client->dev, "error : write zinitix tc firmare\n");
					goto fail_upgrade;
				}
				flash_addr += TC_SECTOR_SZ;
				usleep_range(100, 100);
			
			}
			
			if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS) {
				input_err(true, &client->dev, "failed to write nvm wp disable\n");
				goto fail_upgrade;
			}
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
		}
		else
#endif
		{
			for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
				/*zinitix_debug_msg("write :addr=%04x, len=%d\n",	flash_addr, TC_SECTOR_SZ);*/
				/*zinitix_printk(KERN_INFO "writing :addr = %04x, len=%d \n", flash_addr, TC_SECTOR_SZ);*/
				if (write_data(client, BT541_WRITE_FLASH,
						(u8 *)&firmware_data[flash_addr], TC_SECTOR_SZ) < 0) {
					input_err(true, &client->dev, "error : write zinitix tc firmare\n");
					goto fail_upgrade;
				}
				flash_addr += TC_SECTOR_SZ;
				usleep_range(100, 100);

			}
		}
		msleep(30);

	}

	if (write_reg(client, 0xc003, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm write vpp off\n");
		goto fail_upgrade;
	}

	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS) {
		input_err(true, &client->dev, "nvm wp enable\n");
		goto fail_upgrade;
	}

	input_info(true, &client->dev,"init flash\n");

	if (write_cmd(client, BT541_INIT_FLASH) != I2C_SUCCESS) {
		input_err(true, &client->dev, "failed to init flash\n");
		goto fail_upgrade;
	}

	input_info(true, &client->dev, "read firmware data\n");

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < page_sz / TC_SECTOR_SZ; i++) {
			/*zinitix_debug_msg("read :addr=%04x, len=%d\n",flash_addr, TC_SECTOR_SZ);*/
			if (read_firmware_data(client,
						BT541_READ_FLASH,
						(u8 *)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				input_err(true, &client->dev, "Failed to read firmare\n");

				goto fail_upgrade;
			}

			flash_addr += TC_SECTOR_SZ;
		}
	}
	/* verify */
	input_info(true, &client->dev, "verify firmware data\n");
#ifdef PAT_CONTROL
	for (i = DEF_IUM_ADDR; i < DEF_IUM_ADDR + page_sz; i++)
		verify_data[i] = firmware_data[i];
#endif
	if (memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size) == 0) {
		input_info(true, &client->dev, "upgrade finished\n");
		if (verify_data) {
			kfree(verify_data);
			verify_data = NULL;
		}
		bt541_power_control(info, POWER_OFF);
		bt541_power_control(info, POWER_ON_SEQUENCE);

		for (i = 0; i < 5; i++) {
			if (read_data(client, BT541_CHECKSUM_RESULT,
					(u8 *)&reg_val, 2) < 0) {
				usleep_range(10*BT541_HZ, 10*BT541_HZ);
				continue;
			}
		}

		if (reg_val != 0x55aa) {
			input_err(true, &client->dev, "upgrade done, but firmware checksum fail. reg_val = %x\n", reg_val);
			goto fail_upgrade;
		}

		return true;
	}

fail_upgrade:
	bt541_power_control(info, POWER_OFF);

	if (retry_cnt++ < INIT_RETRY_CNT) {
		input_err(true, &client->dev, "upgrade failed : so retry... (%d)\n", retry_cnt);
		goto retry_upgrade;
	}

	if (verify_data)
		kfree(verify_data);

	input_info(true, &client->dev, "Failed to upgrade\n");

	return false;
}

int bt541_fw_update_from_kernel(struct bt541_ts_info *info, int fw_state)
{
	const struct firmware *fw;
	int retires = 3;
	int ret;
	char fw_path[64];

	input_info(true, &info->client->dev, "%s [START]\n", __func__);

	disable_irq(info->irq);
	clear_report_data(info);

	snprintf(fw_path, 64, "%s", info->pdata->fw_name);

	request_firmware(&fw, fw_path, &info->client->dev);

	if (!fw) {
		input_err(true, &info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		enable_irq(info->irq);
		goto ERROR;
	}

	//Update fw
	do {
		ret = ts_upgrade_firmware(info, fw->data, fw->size, fw_state);
		if (ret == true) {
			break;
		}
	} while (--retires);

	if (!retires) {
		input_err(true, &info->client->dev, "%s [ERROR] bt541_flash_fw failed\n", __func__);
		ret = -1;
	}

	release_firmware(fw);

	//Enable IRQ
	enable_irq(info->irq);

	if (ret < 0) {
		goto ERROR;
	}

	input_err(true, &info->client->dev, "%s [DONE]\n", __func__);
	return true;

ERROR:
	input_err(true, &info->client->dev, "%s [ERROR]\n", __func__);
	return false;
	
	
}


static bool ts_hw_calibration(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16	chip_eeprom_info;
	int time_out = 0;
	bool lock_flag = false;

	if (info->ium_lock_enable == true) { 
		if (tsp_nvm_ium_unlock(info) == false) {
			input_err(true, &client->dev, "failed ium unlock\n", __func__);
			return false;
		}
		lock_flag = true;
	}
	
	if (write_reg(client, BT541_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;
	usleep_range(10*BT541_HZ, 10*BT541_HZ);
	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
	usleep_range(10*BT541_HZ, 10*BT541_HZ);
	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
	msleep(50);
	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
	usleep_range(10*BT541_HZ, 10*BT541_HZ);;

	if (write_cmd(client, BT541_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, BT541_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	usleep_range(10*BT541_HZ, 10*BT541_HZ);
	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		msleep(500);
		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);

		if (read_data(client, BT541_EEPROM_INFO_REG,
				(u8 *)&chip_eeprom_info, 2) < 0)
			return false;

		input_dbg(true, &client->dev, "touch eeprom info = 0x%04X\r\n",
				chip_eeprom_info);
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if (time_out++ == 4) {
			write_cmd(client, BT541_CALIBRATE_CMD);
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
			write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
			input_err(true, &client->dev, "h/w calibration retry timeout.\n");
		}

		if (time_out++ > 10) {
			input_err(true, &client->dev, "h/w calibration timeout.\n");
			break;
		}

	} while (1);

	if (write_reg(client,
				BT541_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;

	if (info->cap_info.ic_int_mask != 0) {
		if (write_reg(client,
					BT541_INT_ENABLE_FLAG,
					info->cap_info.ic_int_mask)
				!= I2C_SUCCESS)
			return false;
	}

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, BT541_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	msleep(1000);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);

	if (lock_flag) {
		if (tsp_nvm_ium_lock(info) == false) {
			input_err(true, &client->dev, "failed ium lock\n", __func__);
		}
	}
	return true;
}

static bool init_touch(struct bt541_ts_info *info, int fw_oneshot_upgrade)
{
	struct bt541_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	u16 reg_val;
	int i;
	u16 chip_eeprom_info;
#if USE_CHECKSUM
	u16 chip_check_sum;
	u8 checksum_err;
#endif
	int retry_cnt = 0;
	bool magic_cal = false;

retry_init:
	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, BT541_EEPROM_INFO_REG,
					(u8 *)&chip_eeprom_info, 2) < 0) {
			input_err(true, &client->dev, "Failed to read eeprom info(%d)\n", i);
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
			continue;
		} else
			break;
	}

	if (i == INIT_RETRY_CNT)
		goto fail_init;

#if USE_CHECKSUM
	input_dbg(true, &client->dev, "%s: Check checksum\n", __func__);

	checksum_err = 0;

	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, BT541_CHECKSUM_RESULT,
					(u8 *)&chip_check_sum, 2) < 0) {
			usleep_range(10*BT541_HZ, 10*BT541_HZ);
			continue;
		}

#if defined(TSP_VERBOSE_DEBUG)
		input_info(true, &client->dev, "chip_check_sum 0x%04X\n", chip_check_sum);
#endif

		if (chip_check_sum == 0x55aa)
			break;
		else {
			checksum_err = 1;
			break;
		}
	}

	if (i == INIT_RETRY_CNT || checksum_err) {
		input_err(true, &client->dev, "Failed to check firmware data\n");
		if (checksum_err == 1 && retry_cnt < INIT_RETRY_CNT)
			retry_cnt = INIT_RETRY_CNT;

		goto fail_init;
	}
#endif

	if (write_cmd(client, BT541_SWRESET_CMD) != I2C_SUCCESS) {
		input_err(true, &client->dev, "Failed to write reset command\n");
		goto fail_init;
	}

	cap->button_num = SUPPORTED_BUTTON_NUM;

	reg_val = 0;
	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);
#if (TOUCH_POINT_MODE == 2)
	zinitix_bit_set(reg_val, BIT_PALM);
	zinitix_bit_set(reg_val, BIT_PALM_REJECT);
#endif

	if (cap->button_num > 0)
		zinitix_bit_set(reg_val, BIT_ICON_EVENT);

	cap->ic_int_mask = reg_val;

	if (write_reg(client, BT541_INT_ENABLE_FLAG, 0x0) != I2C_SUCCESS)
		goto fail_init;

	input_dbg(true, &client->dev, "%s: Send reset command\n", __func__);
	if (write_cmd(client, BT541_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

	/* get chip information */
	if (read_data(client, BT541_VENDOR_ID,
				(u8 *)&cap->vendor_id, 2) < 0) {
		input_err(true, &client->dev, "failed to read chip revision\n");
		goto fail_init;
	}


	if (read_data(client, BT541_CHIP_REVISION,
				(u8 *)&cap->ic_revision, 2) < 0) {
		input_err(true, &client->dev, "failed to read chip revision\n");
		goto fail_init;
	}

	cap->ic_fw_size = 32*1024;

	if (read_data(client, BT541_HW_ID, (u8 *)&cap->hw_id, 2) < 0) {
		input_err(true, &client->dev, "Failed to read hw id\n");
		goto fail_init;
	}

	if (read_data(client, BT541_THRESHOLD, (u8 *)&cap->threshold, 2) < 0)
		goto fail_init;

	if (read_data(client, BT541_BUTTON_SENSITIVITY,
				(u8 *)&cap->key_threshold, 2) < 0)
		goto fail_init;

	/*if (read_data(client, BT541_DUMMY_BUTTON_SENSITIVITY,
				(u8 *)&cap->dummy_threshold, 2) < 0)
			goto fail_init;*/

	if (read_data(client, BT541_TOTAL_NUMBER_OF_X,
				(u8 *)&cap->x_node_num, 2) < 0)
		goto fail_init;

	if (read_data(client, BT541_TOTAL_NUMBER_OF_Y,
				(u8 *)&cap->y_node_num, 2) < 0)
		goto fail_init;

	cap->total_node_num = cap->x_node_num * cap->y_node_num;


	if (read_data(client, BT541_SHIFT_VALUE,
				(u8 *)&cap->shift_value, 2) < 0)
		goto fail_init;
	input_dbg(true, &client->dev, "Shift value = %d\n", cap->shift_value);


	if (read_data(client, BT541_DND_N_COUNT,
				(u8 *)&cap->N_cnt, 2) < 0)
		goto fail_init;
	input_dbg(true, &client->dev, "N count = %d\n", cap->N_cnt);

	if (read_data(client, BT541_DND_U_COUNT,
				(u8 *)&cap->U_cnt, 2) < 0)
		goto fail_init;
	input_dbg(true, &client->dev, "u count = %d\n", cap->U_cnt);

	if (read_data(client, BT541_AFE_FREQUENCY,
				(u8 *)&cap->afe_frequency, 2) < 0)
		goto fail_init;
	input_dbg(true, &client->dev, "afe frequency = %d\n", cap->afe_frequency);

	
	if (read_data(client, BT541_ISRC_CTRL,
				(u8 *)&cap->isrc_ctrl, 2) < 0)
		goto fail_init;
	input_dbg(true, &client->dev, "isrc_ctrl = %d\n", cap->isrc_ctrl);

	/* get chip firmware version */
	if (read_data(client, BT541_FIRMWARE_VERSION,
				(u8 *)&cap->fw_version, 2) < 0)
		goto fail_init;

	if (read_data(client, BT541_MINOR_FW_VERSION,
				(u8 *)&cap->fw_minor_version, 2) < 0)
		goto fail_init;

	if (read_data(client, BT541_DATA_VERSION_REG,
				(u8 *)&cap->reg_data_version, 2) < 0)
		goto fail_init;

	if (!info->pdata->bringup) {
#if TOUCH_ONESHOT_UPGRADE
/* fw_true : nomal, fw_false : Don't fw update, fw_force : Do force fw update */
		if (!bt541_fw_update_from_kernel(info, fw_oneshot_upgrade))
			goto fail_init;  
#ifdef PAT_CONTROL
		if (write_reg(client, 0x002E, IUM_SET_TIMEOUT) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: failed to set ium timeout\n", __func__);
			//goto fail_init;
		}		
		if (tsp_nvm_ium_lock(info) == false) {
			input_err(true, &client->dev, "failed ium lock\n", __func__);
			goto fail_init;
		}
		if (info->pdata->pat_function == PAT_CONTROL_CLEAR_NV && info->latest_flag == false) { /* pat_function(1) */
			input_info(true, &client->dev, "%s ts_hw_calibration start \n", __func__);
			if (ts_hw_calibration(info) == false)
				goto fail_init;
			set_tsp_nvm_data(info, PAT_CAL_DATA, 0);
			if (read_data(client, BT541_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2) < 0)
				goto fail_init; /* get fix_tune_version */
			if (read_data(client, BT541_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2) < 0)
				goto fail_init;
			if (read_data(client, BT541_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2) < 0)
				goto fail_init;
			set_tsp_nvm_data(info, PAT_DUMMY_VERSION, (info->cap_info.fw_version << 4) | info->cap_info.fw_minor_version);
			set_tsp_nvm_data(info, PAT_FIX_VERSION, info->cap_info.reg_data_version);
		} else if (info->pdata->pat_function == PAT_CONTROL_PAT_MAGIC ||
				(info->pdata->pat_function == PAT_CONTROL_FORCE_UPDATE)) { /* pat_function(2)) */
			info->cal_count = get_tsp_nvm_data(info, PAT_CAL_DATA);
			info->tune_fix_ver = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) |
				get_tsp_nvm_data(info, PAT_FIX_VERSION);
			if(info->cal_count == -1 || info->tune_fix_ver == -1)
				goto fail_init;        
			if (info->pdata->afe_base > info->tune_fix_ver)
				magic_cal = true;
			if ((info->cal_count == 0) || (magic_cal == true) || info->pat_flag == true ||
				(info->pdata->pat_function == PAT_CONTROL_FORCE_UPDATE)) { /* || pat_function(5)*/
				input_info(true, &client->dev, "%s ts_hw_calibration start \n", __func__);
				if (ts_hw_calibration(info) == false)
					goto fail_init;
				if(info->pat_flag == true) {
					info->cal_count++;
					set_tsp_nvm_data(info, PAT_CAL_DATA, info->cal_count);
					info->pat_flag = false;
				} else {
					set_tsp_nvm_data(info, PAT_CAL_DATA, PAT_MAGIC_NUMBER);
				}
				
				if (read_data(client, BT541_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2) < 0)
					goto fail_init; /* get fix_tune_version */
				if (read_data(client, BT541_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2) < 0)
					goto fail_init;
				if (read_data(client, BT541_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2) < 0)
					goto fail_init;
				set_tsp_nvm_data(info, PAT_DUMMY_VERSION, (info->cap_info.fw_version << 4) | info->cap_info.fw_minor_version);
				set_tsp_nvm_data(info, PAT_FIX_VERSION, info->cap_info.reg_data_version);
			}
		} else if (info->pdata->pat_function == PAT_CONTROL_NONE) {
			info->cal_count = get_tsp_nvm_data(info, PAT_CAL_DATA);
			if (info->cal_count == 0) {
				input_info(true, &client->dev, "%s ts_hw_calibration start \n", __func__);
				if (ts_hw_calibration(info) == false)
					goto fail_init;
				if (read_data(client, BT541_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2) < 0)
					goto fail_init; /* get fix_tune_version */
				if (read_data(client, BT541_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2) < 0)
					goto fail_init;
				if (read_data(client, BT541_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2) < 0)
					goto fail_init;
				set_tsp_nvm_data(info, PAT_CAL_DATA, 0);
				set_tsp_nvm_data(info, PAT_DUMMY_VERSION, (info->cap_info.fw_version << 4) | info->cap_info.fw_minor_version);
				set_tsp_nvm_data(info, PAT_FIX_VERSION, info->cap_info.reg_data_version);
			}
		}

		info->cal_count = get_tsp_nvm_data(info, PAT_CAL_DATA);
		info->tune_fix_ver = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) |
			get_tsp_nvm_data(info, PAT_FIX_VERSION);
		
		if (tsp_nvm_ium_unlock(info) == false) {
			input_err(true, &client->dev, "failed ium unlock\n", __func__);
			goto fail_init;
		}
		input_info(true, &client->dev, "%s info->cal_count:0x%02x"
			" info->tune_fix_ver:0x%04x\n", __func__, info->cal_count, info->tune_fix_ver);
#else
		if (ts_hw_calibration(info) == false)
			goto fail_init;
#endif
			/* disable chip interrupt */
		if (write_reg(client, BT541_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;

			/* get chip firmware version */
		if (read_data(client, BT541_FIRMWARE_VERSION,
				(u8 *)&cap->fw_version, 2) < 0)
			goto fail_init;

		if (read_data(client, BT541_MINOR_FW_VERSION,
				(u8 *)&cap->fw_minor_version, 2) < 0)
			goto fail_init;

		if (read_data(client, BT541_DATA_VERSION_REG,
				(u8 *)&cap->reg_data_version, 2) < 0)
			goto fail_init;		

#endif
}

	if (read_data(client, BT541_EEPROM_INFO_REG,
				(u8 *)&chip_eeprom_info, 2) < 0)
		goto fail_init;

	if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit*/
		if (ts_hw_calibration(info) == false)
			goto fail_init;

		/* disable chip interrupt */
		if (write_reg(client, BT541_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;
	}

	/* initialize */
	if (write_reg(client, BT541_X_RESOLUTION,
				(u16)pdata->x_resolution) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT541_Y_RESOLUTION,
				(u16)pdata->y_resolution) != I2C_SUCCESS)
		goto fail_init;

	cap->MinX = (u32)0;
	cap->MinY = (u32)0;
	cap->MaxX = (u32)pdata->x_resolution;
	cap->MaxY = (u32)pdata->y_resolution;

	if (write_reg(client, BT541_BUTTON_SUPPORTED_NUM,
				(u16)cap->button_num) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT541_SUPPORTED_FINGER_NUM,
				(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_init;

	cap->multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	input_dbg(true, &client->dev, "max supported finger num = %d\n",
			cap->multi_fingers);

	cap->gesture_support = 0;
	input_dbg(true, &client->dev, "set other configuration\n");

	if (write_reg(client, BT541_INITIAL_TOUCH_MODE,
				TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT541_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_init;

#if ZINITIX_I2C_CHECKSUM
	if (read_data(client, ZINITIX_INTERNAL_FLAG_02,
				(u8 *)&reg_val, 2) < 0)
		goto fail_init;
	cap->i2s_checksum = !(!zinitix_bit_test(reg_val, 15));
	input_info(true, &client->dev, "use i2s checksum = %d\n",
			cap->i2s_checksum);
#endif

	bt541_set_optional_mode(info, true);
	/* soft calibration */
	/*	if (write_cmd(client, BT541_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_init;*/

	if (write_reg(client, 0x002E, IUM_SET_TIMEOUT) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to set ium timeout\n", __func__);
		//goto fail_init;
	}

	if (write_reg(client, BT541_INT_ENABLE_FLAG,
				cap->ic_int_mask) != I2C_SUCCESS)
		goto fail_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) { /* Test Mode */
		if (write_reg(client, BT541_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS) {
			input_err(true, &client->dev, "%s: Failed to set DELAY_RAW_FOR_HOST\n",
					__func__);

			goto fail_init;
		}
	}
#if ESD_TIMER_INTERVAL
	if (write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;

	read_data(client, BT541_PERIODICAL_INTERRUPT_INTERVAL, (u8 *)&reg_val, 2);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Esd timer register = %d\n", reg_val);
#endif
#endif
	info->version = (u32)((u32)(cap->hw_id & 0xff) << 16) | ((cap->fw_version & 0xf) << 12)
		| ((cap->fw_minor_version & 0xf) << 8) | (cap->reg_data_version & 0xff);
	input_info(true, &client->dev, "successfully initialized\n");
	return true;

fail_init:
	if (cal_mode) {
		input_err(true, &client->dev,"didn't update TSP F/W!! in CAL MODE\n");
		return false;
	}
	if (++retry_cnt <= INIT_RETRY_CNT) {
		bt541_power_control(info, POWER_OFF);
		bt541_power_control(info, POWER_ON_SEQUENCE);

		input_dbg(true, &client->dev, "retry to initiallize(retry cnt = %d)\n",
				retry_cnt);
		goto retry_init;

	} else if (retry_cnt == INIT_RETRY_CNT+1) {
		cap->ic_fw_size = 32*1024;

		input_dbg(true, &client->dev, "retry to initiallize(retry cnt = %d)\n", retry_cnt);

#if TOUCH_FORCE_UPGRADE

		if (!bt541_fw_update_from_kernel(info, true)) {
			input_info(true, &client->dev, "upgrade failed\n");
			return false;
		}

		msleep(100);

		/* hw calibration and make checksum */
		if (ts_hw_calibration(info) == false) {
			input_info(true, &client->dev, "failed to initiallize\n");
			return false;
		}
		goto retry_init;
#endif
	}

	input_err(true, &client->dev, "Failed to initiallize\n");

	return false;
}

static bool mini_init_touch(struct bt541_ts_info *info)
{
	struct bt541_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
#if USE_CHECKSUM
	u16 chip_check_sum;

	/*dev_info(&client->dev, "check checksum\n");*/

	if (read_data(client, BT541_CHECKSUM_RESULT,
				(u8 *)&chip_check_sum, 2) < 0)
		goto fail_mini_init;

	if (chip_check_sum != 0x55aa) {
		input_err(true, &client->dev, "Failed to check firmware"
				" checksum(0x%04x)\n", chip_check_sum);

		goto fail_mini_init;
	}
#endif

	if (write_cmd(client, BT541_SWRESET_CMD) != I2C_SUCCESS) {
		input_info(true, &client->dev, "Failed to write reset command\n");

		goto fail_mini_init;
	}

	/* initialize */
	if (write_reg(client, BT541_X_RESOLUTION,
				(u16)(pdata->x_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client,BT541_Y_RESOLUTION,
				(u16)(pdata->y_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	/*dev_info(&client->dev, "touch max x = %d\r\n", pdata->x_resolution);
	  dev_info(&client->dev, "touch max y = %d\r\n", pdata->y_resolution);*/

	if (write_reg(client, BT541_BUTTON_SUPPORTED_NUM,
				(u16)info->cap_info.button_num) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT541_SUPPORTED_FINGER_NUM,
				(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT541_INITIAL_TOUCH_MODE,
				TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT541_TOUCH_MODE,
				info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	bt541_set_optional_mode(info, true);

	if (write_reg(client, 0x002E, IUM_SET_TIMEOUT) != I2C_SUCCESS) {
		input_err(true, &client->dev, "%s: failed to set ium timeout\n", __func__);
		//goto fail_mini_init;
	}

	/* soft calibration */
	if (write_cmd(client, BT541_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT541_INT_ENABLE_FLAG,
				info->cap_info.ic_int_mask) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);;
	}

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(client, BT541_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS) {
			input_err(true, &client->dev, "Failed to set BT541_DELAY_RAW_FOR_HOST\n");

			goto fail_mini_init;
		}
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL,
				SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Started esd timer\n");
#endif
#endif

	input_info(true, &client->dev, "Successfully mini initialized\r\n");
	return true;

fail_mini_init:
	input_err(true, &client->dev, "Failed to initialize mini init\n");
#if 0 // if happen firmware crack, need re-define.
	bt541_power_control(info, POWER_OFF);
	bt541_power_control(info, POWER_ON_SEQUENCE);

	if (init_touch(info, fw_true) == false) {
		input_err(true, &client->dev, "Failed to initialize\n");

		return false;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Started esd timer\n");
#endif
#endif
	return true;
#else
	return false;
#endif
}

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
static char location_detect(struct bt541_ts_info *info, int coord, bool flag)
{
	/* flag ? coord = Y : coord = X */
	int x_devide = info->pdata->x_resolution / 3;
	int y_devide = info->pdata->y_resolution / 3;

	if (flag) {
		if (coord < y_devide)
			return 'H';
		else if (coord < y_devide * 2)
			return 'M';
		else
			return 'L';
	} else {
		if (coord < x_devide)
			return '0';
		else if (coord < x_devide * 2)
			return '1';
		else
			return '2';
	}

	return 'E';
}
#endif


static void clear_report_data(struct bt541_ts_info *info)
{
	int i;
	u8 reported = 0;
	u8 sub_status;

	input_dbg(true, &info->client->dev, "%s\n", __func__);
	for (i = 0; i < info->cap_info.button_num; i++) {
		if (info->button[i] == ICON_BUTTON_DOWN) {
			info->button[i] = ICON_BUTTON_UP;
			input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
			reported = true;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &info->client->dev, "key %s\n", i ? "Back R" :"Menu R"  );
#else
			input_info(true, &info->client->dev, "key R");
#endif
		}
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->reported_touch_info.coord[i].sub_status;
		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			reported = true;
			if (!m_ts_debug_mode && TSP_NORMAL_EVENT_MSG) {
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &info->client->dev,
					"R[%d] loc:%c%c V[%06x] tc:%d mc:%d\n",
					i, location_detect(info, info->reported_touch_info.coord[i].x, 1),
					location_detect(info, info->reported_touch_info.coord[i].y, 0),
					info->version, info->touch_count, info->sec_point_info[i].move_count);
#else
				input_info(true, &info->client->dev,
					"R[%d] (%d, %d) V[%06x] tc:%d mc:%d\n",
					i, info->reported_touch_info.coord[i].x, info->reported_touch_info.coord[i].y,
					info->version, info->touch_count, info->sec_point_info[i].move_count);
#endif
			info->sec_point_info[i].move_count = 0;

			}
		}
		info->reported_touch_info.coord[i].sub_status = 0;
		info->touch_count = 0;
	}

	if (reported)
		input_sync(info->input_dev);
}

#define	PALM_REPORT_WIDTH	200
#define	PALM_REJECT_WIDTH	255

static irqreturn_t bt541_touch_work(int irq, void *data)
{
	struct bt541_ts_info* info = (struct bt541_ts_info*)data;
	struct bt541_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u8 sub_status;
	u8 prev_sub_status;
	u32 x, y, maxX, maxY;
	u32 w;
	u32 tmp;
	u8 palm = 0;
	u16 val = 0;
#ifdef CONFIG_SEC_FACTORY
	int ret = 0;
#endif

	if (gpio_get_value(info->pdata->gpio_int)) {
		input_err(true, &client->dev, "Invalid interrupt\n");

		return IRQ_HANDLED;
	}

	if (down_trylock(&info->work_lock)) {
		input_err(true, &client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);

		return IRQ_HANDLED;
	}
#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif
	if (info->work_state != NOTHING) {
		input_err(true, &client->dev, "%s: Other process occupied\n", __func__);
		usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);

		if (!gpio_get_value(info->pdata->gpio_int)) {
			write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
			usleep_range(DELAY_FOR_SIGNAL_DELAY, DELAY_FOR_SIGNAL_DELAY);
		}

		goto out;
	}

	info->work_state = NORMAL;
#if ZINITIX_I2C_CHECKSUM
	i = 0;

	if (ts_read_coord(info) == false || info->touch_info.status == 0xffff
			|| info->touch_info.status == 0x1) {
		/* more retry*/
		for (i = 1; i < 50; i++) {	/* about 10ms*/
			if (!(ts_read_coord(info) == false || info->touch_info.status == 0xffff
						|| info->touch_info.status == 0x1))
				break;
		}

	}
	if (i == 50) {
		input_err(true, &client->dev, "Failed to read info coord\n");
		bt541_power_control(info, POWER_OFF);
		bt541_power_control(info, POWER_ON_SEQUENCE);

		clear_report_data(info);
		mini_init_touch(info);

		goto out;
	}
#else
	if (ts_read_coord(info) == false || info->touch_info.status == 0xffff
			|| info->touch_info.status == 0x1) { /* maybe desirable reset */
		input_err(true, &client->dev, "Failed to read info coord\n");
		bt541_power_control(info, POWER_OFF);
		bt541_power_control(info, POWER_ON_SEQUENCE);

		clear_report_data(info);
		mini_init_touch(info);

		goto out;
	}
#endif
	/* invalid : maybe periodical repeated int. */

	if (info->touch_info.status == 0x0)
		goto out;

	if (zinitix_bit_test(info->touch_info.status, BIT_ICON_EVENT)) {
		if (read_data(info->client, BT541_ICON_STATUS_REG,
					(u8 *)(&info->icon_event_reg), 2) < 0) {
			input_err(true, &client->dev, "Failed to read button info\n");
			write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);

			goto out;
		}

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg,
						(BIT_O_ICON0_DOWN + i))) {
				info->button[i] = ICON_BUTTON_DOWN;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 1);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &client->dev, "Key P\n");
#else
				input_info(true, &client->dev, "Key %s\n", i ? "back P" : "menu P");
#endif
			}
		}

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg,
						(BIT_O_ICON0_UP + i))) {
				info->button[i] = ICON_BUTTON_UP;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &client->dev, "Key R\n");
#else
				input_info(true, &client->dev, "Key %s\n", i ? "back R" : "menu R");
#endif

			}
		}
	}


#ifdef SUPPORTED_PALM_TOUCH
	if (zinitix_bit_test(info->touch_info.status, BIT_PALM)) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &client->dev, "Palm report\n");
#endif
		palm = 1;
	}

	if (zinitix_bit_test(info->touch_info.status, BIT_PALM_REJECT)) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		input_info(true, &client->dev, "Palm reject\n");
#endif
		palm = 2;
	}
#endif

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->touch_info.coord[i].sub_status;
		prev_sub_status = info->reported_touch_info.coord[i].sub_status;

		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			x = info->touch_info.coord[i].x;
			y = info->touch_info.coord[i].y;
			w = info->touch_info.coord[i].width;

			/* transformation from touch to screen orientation */
			if (pdata->orientation & TOUCH_V_FLIP)
				y = info->cap_info.MaxY
					+ info->cap_info.MinY - y;

			if (pdata->orientation & TOUCH_H_FLIP)
				x = info->cap_info.MaxX
					+ info->cap_info.MinX - x;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (pdata->orientation & TOUCH_XY_SWAP) {
				zinitix_swap_v(x, y, tmp);
				zinitix_swap_v(maxX, maxY, tmp);
			}

			if (x > maxX || y > maxY) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				input_err(true, &client->dev,
						"Invalid coord %d : x=%d, y=%d\n", i, x, y);
#endif
				continue;
			}

			info->touch_info.coord[i].x = x;
			info->touch_info.coord[i].y = y;

			if (w == 0)
				w = 1;

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

#if (TOUCH_POINT_MODE == 2)
			if (palm == 0) {
				if (w >= PALM_REPORT_WIDTH)
					w = PALM_REPORT_WIDTH - 10;
			} else if (palm == 1) {	/*palm report*/
				w = PALM_REPORT_WIDTH;
				/*				info->touch_info.coord[i].minor_width
				= PALM_REPORT_WIDTH;*/
			} else if (palm == 2) {	/* palm reject*/
				/*				x = y = 0;*/
				w = PALM_REJECT_WIDTH;
				/*				info->touch_info.coord[i].minor_width = PALM_REJECT_WIDTH;*/
			}
#endif
#ifdef CONFIG_SEC_FACTORY
			ret = read_data(client, BT541_REAL_WIDTH + i, (u8*)&val, 2);
			if (ret < 0)
					input_info(true, &client->dev, ": Failed to read %d's Real width %s\n", i, __func__);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)val);
#else
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)w);
#endif
			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)w);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR,
					(u32)((palm == 1) ? w-40 : w));
#if (TOUCH_POINT_MODE == 2)
			input_report_abs(info->input_dev,
					ABS_MT_TOUCH_MINOR, (u32)info->touch_info.coord[i].minor_width);
			/*			input_report_abs(info->input_dev,
							ABS_MT_WIDTH_MINOR, (u32)info->touch_info.coord[i].minor_width);*/
#ifdef SUPPORTED_PALM_TOUCH
			/*input_report_abs(info->input_dev, ABS_MT_ANGLE,
			  (palm > 1)?70:info->touch_info.coord[i].angle - 90);*/
			/*dev_info(&client->dev, "finger [%02d] angle = %03d\n", i,
			  info->touch_info.coord[i].angle);*/
			input_report_abs(info->input_dev, ABS_MT_PALM, (palm > 0)?1:0);
#endif
			/*			input_report_abs(info->input_dev, ABS_MT_PALM, 1);*/
#endif

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			if (info->sec_point_info[i].finger_state > 0)
				info->sec_point_info[i].move_count++;

			if (info->sec_point_info[i].finger_state == 0) {
				info->sec_point_info[i].finger_state = 1;
				info->sec_point_info[i].move_count = 0;
				info->touch_count++;
			}

			if (zinitix_bit_test(sub_status, SUB_BIT_DOWN))
			{

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
				input_info(true, &client->dev, "P[%d] loc(%c, %c) z:%d factoryZ:%d p:%d m:%d, %d tc:%d\n",
						i, location_detect(info, info->reported_touch_info.coord[i].x, 1),
						location_detect(info, info->reported_touch_info.coord[i].y, 0) ,
						w, val, (palm > 0)?1:0, w, ((palm == 1) ? w-40 : w), info->touch_count);
#else
				input_info(true, &client->dev, "P[%d] (%d, %d) z:%d factoryZ:%d p:%d m:%d, %d tc:%d\n",
						i, x, y, w, val, (palm > 0)?1:0, w, ((palm == 1) ? w-40 : w), info->touch_count);
#endif
			}
		} else if (zinitix_bit_test(sub_status, SUB_BIT_UP)||
				zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
			input_info(true, &client->dev,
				"R[%d] loc:%c%c V[%06x] tc:%d mc:%d\n",
				i, location_detect(info, info->reported_touch_info.coord[i].x, 1),
				location_detect(info, info->reported_touch_info.coord[i].y, 0),
				info->version, info->touch_count, info->sec_point_info[i].move_count);
#else

			input_info(true, &client->dev,
				"R[%d] (%d, %d) V[%06x] tc:%d mc:%d\n",
				i, info->reported_touch_info.coord[i].x, info->reported_touch_info.coord[i].y,
				info->version, info->touch_count, info->sec_point_info[i].move_count);
#endif

			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			info->sec_point_info[i].move_count = 0;
			info->sec_point_info[i].finger_state = 0;
			info->touch_count--;
		} else {
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
		}
	}
	memcpy((char *)&info->reported_touch_info, (char *)&info->touch_info,
			sizeof(struct point_info));

	input_sync(info->input_dev);

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	up(&info->work_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bt541_ts_late_resume(struct early_suspend *h)
{
	struct bt541_ts_info *info = misc_info;
	//info = container_of(h, struct bt541_ts_info, early_suspend);
	struct capa_info *cap = &(info->cap_info);

	if (info == NULL)
		return;
	input_info(true, &info->client->dev, "late resume++\r\n");

	down(&info->work_lock);
	if (info->work_state != RESUME
			&& info->work_state != EALRY_SUSPEND) {
		input_err(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
				info->work_state);
		up(&info->work_lock);
		return;
	}
#if 0
	write_cmd(info->client, BT541_WAKEUP_CMD);
	usleep_range(BT541_HZ, BT541_HZ);
#else
	bt541_power_control(info, POWER_ON_SEQUENCE);
#endif
	info->work_state = RESUME;
	if (mini_init_touch(info) == false)
		goto fail_late_resume;

	if (read_data(info->client, BT541_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2))
		;
	if (read_data(info->client, BT541_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2))
		;
	if (read_data(info->client, BT541_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2))
		;
	if (read_data(info->client, BT541_HW_ID, (u8 *)&cap->hw_id, 2))
		;
	if (read_data(info->client, BT541_VENDOR_ID, (u8 *)&cap->vendor_id, 2))
		;
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	input_err(true, &info->client->dev, "late resume--\n");
	return;
fail_late_resume:
	input_err(true, &info->client->dev, "failed to late resume\n");
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	return;
}

static void bt541_ts_early_suspend(struct early_suspend *h)
{
	struct bt541_ts_info *info = misc_info;
	/*info = container_of(h, struct bt541_ts_info, early_suspend);*/

	if (info == NULL)
		return;

	input_info(true, &info->client->dev, "early suspend++\n");

	disable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "invalid work proceedure (%d)\r\n",
				info->work_state);
		up(&info->work_lock);
		enable_irq(info->irq);
		return;
	}
	info->work_state = EALRY_SUSPEND;

	input_err(true, &info->client->dev, "clear all reported points\r\n");
	clear_report_data(info);

#if ESD_TIMER_INTERVAL
	/*write_reg(info->client, BT541_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &info->client->dev, "Stopped esd timer\n");
#endif
#endif

#if 0
	write_reg(info->client, BT541_INT_ENABLE_FLAG, 0x0);

	usleep_range(100, 100);
	if (write_cmd(info->client, BT541_SLEEP_CMD) != I2C_SUCCESS) {
		input_err(true, &misc_info->client->dev, "failed to enter into sleep mode\n");
		up(&info->work_lock);
		return;
	}
#else
	bt541_power_control(info, POWER_OFF);
#endif
	input_info(true, &info->client->dev, "early suspend--\n");
	up(&info->work_lock);
	return;
}
#endif	/* CONFIG_HAS_EARLYSUSPEND */

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int bt541_ts_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bt541_ts_info *info = i2c_get_clientdata(client);

	bt541_ts_open(info->input_dev);

	return 0;

}

static int bt541_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bt541_ts_info *info = i2c_get_clientdata(client);

	bt541_ts_close(info->input_dev);

	return 0;
}
#endif

static int bt541_ts_open(struct input_dev *dev)
{
	struct bt541_ts_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);

	if(info->enabled == true) {
		input_err(true, &client->dev, "%s already open\n", __func__);
		return 0;
	}

#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "resume++\n");
#endif
	down(&info->work_lock);
	if (info->work_state != SUSPEND) {
		input_err(true, &client->dev, "%s: Invalid work proceedure (%d)\n",
				__func__, info->work_state);
		up(&info->work_lock);

		return 0;
	}
	bt541_power_control(info, POWER_ON_SEQUENCE);
	bt541_pinctrl_configure(info, 1);
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->work_state = RESUME;
#else
	info->work_state = NOTHING;
	if (mini_init_touch(info) == false)
		input_err(true, &client->dev, "Failed to resume\n");

	if (read_data(client, BT541_FIRMWARE_VERSION, (u8 *)&cap->fw_version, 2))
		;
	if (read_data(client, BT541_MINOR_FW_VERSION, (u8 *)&cap->fw_minor_version, 2))
		;
	if (read_data(client, BT541_DATA_VERSION_REG, (u8 *)&cap->reg_data_version, 2))
		;
	if (read_data(client, BT541_HW_ID, (u8 *)&cap->hw_id, 2))
		;
	if (read_data(client, BT541_VENDOR_ID, (u8 *)&cap->vendor_id, 2))
		;

	if (!gpio_get_value(info->pdata->gpio_int))
	{
		write_cmd(info->client, BT541_CLEAR_INT_STATUS_CMD);
		usleep_range(50, 50);
		write_cmd(info->client, BT541_CLEAR_INT_STATUS_CMD);
		usleep_range(50, 50);
		write_cmd(info->client, BT541_CLEAR_INT_STATUS_CMD);
	}
	info->work_state = NOTHING;
#endif

#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "resume--\n");
#endif
	info->enabled = true;
	up(&info->work_lock);
	enable_irq(info->irq);
	return 0;
}


static void bt541_ts_close(struct input_dev *dev)
{
	struct bt541_ts_info *info = input_get_drvdata(dev);
	struct i2c_client *client = info->client;

	if(info->enabled == false) {
		input_err(true, &client->dev, "%s already suspended\n", __func__);
		return;
	}
	 
	input_info(true, &client->dev, "%s\n", __func__);
       

#ifndef CONFIG_HAS_EARLYSUSPEND
	disable_irq(info->irq);
#endif
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING
			&& info->work_state != SUSPEND) {
		input_err(true, &client->dev,"%s: Invalid work proceedure (%d)\n",
				__func__, info->work_state);
		up(&info->work_lock);
#ifndef CONFIG_HAS_EARLYSUSPEND
		enable_irq(info->irq);
#endif
		return;
	}

#ifndef CONFIG_HAS_EARLYSUSPEND
	clear_report_data(info);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Stopped esd timer\n");
#endif
#endif
#endif
	write_cmd(info->client, BT541_SLEEP_CMD);
	bt541_power_control(info, POWER_OFF);
	bt541_pinctrl_configure(info, 0);
	info->work_state = SUSPEND;

#if defined(TSP_VERBOSE_DEBUG)
	input_err(true, &info->client->dev, "suspend--\n");
#endif
	info->enabled = false;
	up(&info->work_lock);

	return;
}



static bool ts_set_touchmode(u16 value)
{
	int i;

	down(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_info(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);		
		up(&misc_info->work_lock);
		return -1;
	}

	misc_info->work_state = SET_MODE;

	if (value == TOUCH_DND_MODE) {
		if (write_reg(misc_info->client, BT541_DND_N_COUNT,
					SEC_DND_N_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set DND_N_COUNT\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					SEC_DND_U_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set DND_U_COUNT\n");

		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					SEC_DND_FREQUENCY) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					SEC_ISRC_CTRL) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set ISRC_CTRL\n");

	} else if (misc_info->touch_mode == TOUCH_DND_MODE) {
		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					misc_info->cap_info.U_cnt) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set U_COUNT\n");

		if (write_reg(misc_info->client, BT541_SHIFT_VALUE,
					misc_info->cap_info.shift_value) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set Shift\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					misc_info->cap_info.isrc_ctrl) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set ISRC_CTRL\n");		
	}

	if (value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

	input_info(true, &misc_info->client->dev, "[zinitix_touch] tsp_set_testmode, "
			"touchkey_testmode = %d\r\n", misc_info->touch_mode);

	if (misc_info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(misc_info->client, BT541_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Fail,to set BT541_DELAY_RAW_FOR_HOST.\r\n");
	}

	if (write_reg(misc_info->client, BT541_TOUCH_MODE,
				misc_info->touch_mode) != I2C_SUCCESS)
		input_err(true, &misc_info->client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", misc_info->touch_mode);

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20*BT541_HZ, 20*BT541_HZ);
		write_cmd(misc_info->client, BT541_CLEAR_INT_STATUS_CMD);
	}

	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);
	return 1;
}

static bool ts_set_touchmode2(u16 value)
{
	int i;

	down(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_err(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		up(&misc_info->work_lock);
		return -1;
	}

	misc_info->work_state = SET_MODE;

	if (value == TOUCH_DND_MODE) {
		if (write_reg(misc_info->client, BT541_DND_N_COUNT,
					SEC_HFDND_N_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set DND_N_COUNT\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					SEC_HFDND_U_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set DND_U_COUNT\n");

		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					SEC_HFDND_FREQUENCY) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					SEC_ISRC_CTRL) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set ISRC_CTRL\n");

	} else {
		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					misc_info->cap_info.U_cnt) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set U_COUNT\n");

		if (write_reg(misc_info->client, BT541_SHIFT_VALUE,
					misc_info->cap_info.shift_value) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set Shift\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					misc_info->cap_info.isrc_ctrl) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set ISRC_CTRL\n");		
	}

	if (value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

	input_err(true, &misc_info->client->dev, "[zinitix_touch] tsp_set_testmode, "
			"touchkey_testmode = %d\r\n", misc_info->touch_mode);

	if (misc_info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(misc_info->client, BT541_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Fail to set BT541_DELAY_RAW_FOR_HOST.\r\n");
	}

	if (write_reg(misc_info->client, BT541_TOUCH_MODE,
				misc_info->touch_mode) != I2C_SUCCESS)
		input_err(true, &misc_info->client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", misc_info->touch_mode);

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20*BT541_HZ, 20*BT541_HZ);
		write_cmd(misc_info->client, BT541_CLEAR_INT_STATUS_CMD);
	}

	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);
	return 1;
}

static bool ts_set_touchmode16(u16 value)
{
	int i;

	down(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		input_err(true, &misc_info->client->dev, "other process occupied.. (%d)\n",
				misc_info->work_state);
		up(&misc_info->work_lock);
		return -1;
	}

	misc_info->work_state = SET_MODE;

	if (value == TOUCH_H_GAP_JITTER_MODE) {
		if (write_reg(misc_info->client, BT541_DND_N_COUNT,
					SEC_HFDND_N_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set DND_N_COUNT\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					SEC_HFDND_U_COUNT) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set DND_U_COUNT\n");

		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					SEC_HFDND_FREQUENCY) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					SEC_ISRC_CTRL) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev,  "Failed to set ISRC_CTRL\n");

	} else {
		if (write_reg(misc_info->client, BT541_AFE_FREQUENCY,
					misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set AFE_FREQUENCY\n");

		if (write_reg(misc_info->client, BT541_DND_U_COUNT,
					misc_info->cap_info.U_cnt) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set U_COUNT\n");

		if (write_reg(misc_info->client, BT541_ISRC_CTRL,
					misc_info->cap_info.isrc_ctrl) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Failed to set ISRC_CTRL\n");
	}

	if (value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

	input_err(true, &misc_info->client->dev, "[zinitix_touch] tsp_set_testmode, "
			"touchkey_testmode = %d\r\n", misc_info->touch_mode);

	if (misc_info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(misc_info->client, BT541_DELAY_RAW_FOR_HOST,
					RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			input_err(true, &misc_info->client->dev, "Fail to set BT541_DELAY_RAW_FOR_HOST.\r\n");
	}

	if (write_reg(misc_info->client, BT541_TOUCH_MODE,
				misc_info->touch_mode) != I2C_SUCCESS)
		input_err(true, &misc_info->client->dev, "[zinitix_touch] TEST Mode : "
				"Fail to set ZINITX_TOUCH_MODE %d.\r\n", misc_info->touch_mode);

	/* clear garbage data */
	for (i = 0; i < 10; i++) {
		usleep_range(20*BT541_HZ, 20*BT541_HZ);
		write_cmd(misc_info->client, BT541_CLEAR_INT_STATUS_CMD);
	}

	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);
	return 1;
}


static int ts_upgrade_sequence(const u8 *firmware_data, u32 firmware_size)
{
	disable_irq(misc_info->irq);
	down(&misc_info->work_lock);
	misc_info->work_state = UPGRADE;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	input_info(true, &misc_info->client->dev, "clear all reported points\r\n");
	clear_report_data(misc_info);

	input_info(true, &misc_info->client->dev, "start upgrade firmware\n");
	if (ts_upgrade_firmware(misc_info,
				firmware_data, firmware_size,
				fw_force) == false) {
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;
	}

	if (init_touch(misc_info, fw_false) == false) {
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &misc_info->client->dev, "Started esd timer\n");
#endif
#endif

	enable_irq(misc_info->irq);
	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);
	return 0;
}

#ifdef CONFIG_SEC_FACTORY_TEST

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	int ret = 0;
	const u8 *buff = 0;
	mm_segment_t old_fs = {0};
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	char fw_path[MAX_FW_PATH+1];
	char result[16] = {0};
	const struct firmware *fw;

	sec_cmd_set_default_result(sec);

	request_firmware(&fw, info->pdata->fw_name, &info->client->dev);
	if (!fw && (sec->cmd_param[0] == BUILT_IN)) {
		input_err(true, &info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		goto update_fail;
	}

	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
		goto update_fail;
	}
	info->cal_count = get_tsp_nvm_data(info, PAT_CAL_DATA);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
		goto update_fail;
	}
	input_info(true, &info->client->dev, "%s pat_flag cal_count:0x%02x\n", __func__, info->cal_count);
	if (info->cal_count >= PAT_MAGIC_NUMBER)
		info->pat_flag = true;

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		ret = ts_upgrade_sequence((u8 *)fw->data, fw->size);
		if (ret < 0) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			release_firmware(fw);
			return;
		}
		break;

	case UMS:
		old_fs = get_fs();
		set_fs(get_ds());

		snprintf(fw_path, MAX_FW_PATH, "/sdcard/%s", TSP_FW_FILENAME);
		fp = filp_open(fw_path, O_RDONLY, 0);
		if (IS_ERR(fp)) {
			input_err(true, &info->client->dev,
					"file %s open error:%lu\n", fw_path, (unsigned long)fp);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;

		if (fsize != info->cap_info.ic_fw_size) {
			input_err(true, &info->client->dev, "invalid fw size!!\n");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_open;
		}

		buff = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!buff) {
			input_err(true, &info->client->dev, "failed to alloc buffer for fw\n");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_alloc;
		}

		nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
		if (nread != fsize) {
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto err_fw_size;
		}

		filp_close(fp, current->files);
		set_fs(old_fs);
		input_info(true, &info->client->dev, "ums fw is loaded!!\n");

		ret = ts_upgrade_sequence((u8 *)buff, fsize);
		if (ret < 0) {
			kfree(buff);
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto update_fail;
		}
		break;

	default:
		input_err(true, &info->client->dev, "invalid fw file type!!\n");
		goto update_fail;
	}

	sec->cmd_state = 2;
	snprintf(result, sizeof(result) , "%s", "OK");
	sec_cmd_set_cmd_result(sec, result,
			strnlen(result, sizeof(result)));
	release_firmware(fw);
	kfree(buff);

	return;


	if (fp != NULL) {
err_fw_size:
		kfree(buff);
err_alloc:
		filp_close(fp, NULL);
err_open:
		set_fs(old_fs);
	}
update_fail:
	snprintf(result, sizeof(result) , "%s", "NG");
	sec_cmd_set_cmd_result(sec, result, strnlen(result, sizeof(result)));
	release_firmware(fw);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	const struct firmware *fw;
	char fw_path[64];
	u16 fw_version, fw_minor_version, reg_version, hw_id;
	u32 version;

	sec_cmd_set_default_result(sec);
	
	snprintf(fw_path, 64, "%s", info->pdata->fw_name);
	request_firmware(&fw, fw_path, &info->client->dev);

	if (!fw) {
		input_err(true, &info->client->dev, "%s [ERROR] request_firmware\n", __func__);
		snprintf(buf, sizeof(buf), "%s", "NG");
		goto EXIT;
	} 
	/* modify m_firmware_data */
	fw_version = (u16)(fw->data[0x34] | (fw->data[0x35] << 8));
	fw_minor_version = (u16)(fw->data[0x38] | (fw->data[0x39] << 8));
	reg_version = (u16)(fw->data[0x3C] | (fw->data[0x3D] << 8));
	hw_id =  (u16)(fw->data[0x30] | (fw->data[0x31]<<8));
	/*vendor_id = ntohs(*(u16 *)&m_firmware_data[0x57e2]);*/
	version = (u32)((u32)(hw_id & 0xff) << 16) | ((fw_version & 0xf ) << 12)
		| ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff);

	/*length = sizeof(vendor_id);
	snprintf(finfo->cmd_buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(finfo->cmd_buff + length, sizeof(finfo->cmd_buff) - length,
				"%06X", version);*/
	release_firmware(fw);

	snprintf(buf, sizeof(buf), "ZI%06X", version);

	sec->cmd_state = SEC_CMD_STATUS_OK;
EXIT:
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	u16 fw_version, fw_minor_version, reg_version, hw_id;
	u32 version;

	sec_cmd_set_default_result(sec);

	fw_version = info->cap_info.fw_version;
	fw_minor_version = info->cap_info.fw_minor_version;
	reg_version = info->cap_info.reg_data_version;
	hw_id = info->cap_info.hw_id;
	/*vendor_id = ntohs(info->cap_info.vendor_id);*/
	version = (u32)((u32)(hw_id & 0xff) << 16) | ((fw_version & 0xf) << 12)
		| ((fw_minor_version & 0xf) << 8) | (reg_version & 0xff);

	/*length = sizeof(vendor_id);
	snprintf(finfo->cmd_buff, length + 1, "%s", (u8 *)&vendor_id);
	snprintf(finfo->cmd_buff + length, sizeof(finfo->cmd_buff) - length,
				"%06X", version);*/

	snprintf(buf, sizeof(buf), "ZI%06X", version);

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf),
			"%d", info->cap_info.threshold);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

static void module_off_master(void *device_data)
{
	return;
}

static void module_on_master(void *device_data)
{
	return;
}

static void module_off_slave(void *device_data)
{
	return;
}

static void module_on_slave(void *device_data)
{
	return;
}

#define BT541_VENDOR_NAME "ZINITIX"

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s", BT541_VENDOR_NAME);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

#define BT541_CHIP_NAME "BT541C"

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%s", BT541_CHIP_NAME);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%u", info->cap_info.x_node_num);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buf, sizeof(buf), "%u", info->cap_info.y_node_num);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

static void not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	sprintf(buf, "%s", "NA");
	sec_cmd_set_cmd_result(sec, buf,
			strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;

	input_info(true, &info->client->dev, "%s: \"%s(%d)\"\n", __func__, buf,
			(int)strnlen(buf, sizeof(buf)));

	return;
}

/*
## Mis Cal result ##
FD : spec out
F3,F4 : i2c failed
F2 : power off state
F1 : not support mis cal concept
F0 : initial value in function
00 : pass
*/

static void run_tsp_rawdata_read(void *device_data, u16 rawdata_mode, s16* buff)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif

	ts_set_touchmode(rawdata_mode);
	get_raw_data(info, (u8 *)buff, 2);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "touch rawdata %d start\n", rawdata_mode);

	for (i = 0; i < x_num; i++) {
		printk("[%02d] :", i);
		for (j = 0; j < y_num; j++) {
			printk("%d\t", buff[(i * y_num) + j]);
		}
		printk("\n");
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static void run_mis_cal_read(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, x, y, node_num;
	char mis_cal_data = 0xF0;
	int ret = 0;
	s16 raw_data_buff[TSP_CMD_NODE_NUM];
	
#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	if (info->pdata->mis_cal_check == 0) {
		input_info(true, &info->client->dev, "%s: [ERROR] not support, %d\n", __func__);
		mis_cal_data = 0xF1;
		goto NG;
	}

	if (info->work_state == SUSPEND) {
		input_info(true, &info->client->dev, "%s: [ERROR] Touch is stopped\n",__func__);
		mis_cal_data = 0xF2;
		goto NG;
	}

	ts_set_touchmode(TOUCH_REF_ABNORMAL_TEST_MODE);
	ret = get_raw_data(info, (u8 *)raw_data->reference_data_abnormal, 2);
	if (!ret) {
		input_info(true, &info->client->dev, "%s:[ERROR] i2c fail!\n", __func__);
		mis_cal_data = 0xF3;
		goto NG;
	}
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "%s start\n", __func__);

	ret = 1;
	for (i = 0; i < x_num; i++) {
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;
			printk("%d ", raw_data->reference_data_abnormal[offset]);

			if (ret && raw_data->reference_data_abnormal[offset] > reference_data_abnormal_max[i][j]) {
				val =  raw_data->reference_data_abnormal[offset];
				x = i;
				y = j;
				node_num = offset;
				mis_cal_data = 0xFD;
				ret = 0;
			}
		}
		printk("\n");
	}
	
	if(!ret)
		goto NG;

	mis_cal_data = 0x00;
	snprintf(buf, sizeof(buf), "%d,%d,%d,%d", mis_cal_data, raw_data->reference_data_abnormal[0],
			raw_data->reference_data_abnormal[1], raw_data->reference_data_abnormal[2]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
NG:
	snprintf(buf, sizeof(buf), "%d,%d,%d,%d", mis_cal_data, 0, 0, 0);
	if (mis_cal_data == 0xFD) {
		run_tsp_rawdata_read(device_data, TOUCH_CNDDATA_MODE, raw_data_buff);
		run_tsp_rawdata_read(device_data, TOUCH_REFERENCE_MODE, raw_data_buff);
	}
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
 	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static void get_mis_cal(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int offset, x_node, y_node;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);
	
	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buf, sizeof(buf), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		enable_irq(info->irq);
		return;
	}
	
	offset = (x_node * y_num) + y_node;
	
	snprintf(buf, sizeof(buf), "%d", raw_data->reference_data_abnormal[offset]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);
	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static void run_dnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, x, y, node_num;
	bool result = true;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_DND_MODE);
	get_raw_data(info, (u8 *)raw_data->dnd_data, 2);
	ts_set_touchmode(TOUCH_POINT_MODE);

	input_info(true, &info->client->dev, "DND start\n");

	for (i = 0; i < x_num; i++) {
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;
			printk("%d ", raw_data->dnd_data[offset]);

			if ((raw_data->dnd_data[offset] > dnd_max[i][j]) ||
			  (raw_data->dnd_data[offset] &&
			  (raw_data->dnd_data[offset] < dnd_min[i][j]))) {
				val =  raw_data->dnd_data[offset];
				x = i;
				y = j;
				node_num = offset;
				result = false;
			}
		}
		printk("\n");
	}

	if (result) {
		input_info(true, &info->client->dev, "DND Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "DND Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, dnd_min[x][y], dnd_max[x][y], val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	enable_irq(info->irq);

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static void run_dnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val, x, y, node_num, fail_val;
	bool result = true;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "DND V Gap start\n");

	for (i = 0; i < x_num - 1; i++) {
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->dnd_data[offset];
			next_val = raw_data->dnd_data[offset + y_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
	}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
	else
				val = 100 - ((next_val * 100) / cur_val);

			printk("%d ", val);
			cur_val = (s16)(dnd_v_gap[i][j]);

			if (val > cur_val) {
				fail_val = val;
				x = i;
				y = j;
				node_num = offset;
				result = false;
				}
			raw_data->vgap_data[offset] = val;
			}
		printk("\n");
			}

	if (result) {
		input_info(true, &info->client->dev, "DND V Gap Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "DND V Gap Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, 0, dnd_v_gap[x][y], fail_val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void run_dnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val, x, y, node_num, fail_val;
	bool result = true;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "DND H Gap start\n");

	for (i = 0; i < x_num ; i++) {
		for (j = 0; j < y_num-1; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->dnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->dnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < y_num - 1; j++) {
					offset = (i * y_num) + j;

					next_val = raw_data->dnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset]
							= next_val;
						continue;
			}

					break;
		}
	}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
	else
				val = 100 - ((next_val * 100) / cur_val);

			printk("%d ", val);
			cur_val = (s16)(dnd_h_gap[i][j]);

			if (val > cur_val) {
				fail_val = val;
				x = i;
				y = j;
				node_num = offset;
			result = false;
			}
			raw_data->hgap_data[offset] = val;
		}
		printk("\n");
		}
	
	if (result) {
		input_info(true, &info->client->dev, "DND H Gap Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "DND H Gap Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, 0, dnd_h_gap[x][y], fail_val);
	}

	sec_cmd_set_cmd_result(sec, buf,strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void get_dnd_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num || y_node < 0 || y_node >= y_num - 1) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	sprintf(buf, "%d", raw_data->hgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_dnd_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num - 1 || y_node < 0 || y_node >= y_num) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	sprintf(buf, "%d", raw_data->vgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
}

static void run_hfdnd_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset;
	bool result = true;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	for (i = 0; i < TSP_CMD_NODE_NUM; i++)
		raw_data->hfdnd_data_sum[i] = 0;

	ts_set_touchmode2(TOUCH_DND_MODE);
	
	get_raw_data(info, (u8 *)raw_data->hfdnd_data, 2);
	write_cmd(misc_info->client, BT541_CLEAR_INT_STATUS_CMD);
	ts_set_touchmode2(TOUCH_POINT_MODE);
	
	input_info(true, &info->client->dev, "HF DND start\n");

	for (i = 0; i < x_num; i++) {
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;
			printk("%d ", raw_data->hfdnd_data[offset]);
		}
		printk("\n");
	}

	if (result) {
		input_info(true, &info->client->dev, "HF DND Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	enable_irq(info->irq);

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}


static void run_hfdnd_v_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val, x, y, node_num, fail_val;
	bool result = true;

	sec_cmd_set_default_result(sec);

	memset(raw_data->vgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "HF DND V Gap start\n");

	for (i = 0; i < x_num - 1; i++) {
		for (j = 0; j < y_num; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->hfdnd_data[offset];
			next_val = raw_data->hfdnd_data[offset + y_num];
			if (!next_val) {
				raw_data->vgap_data[offset] = next_val;
				continue;
			}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
	else
				val = 100 - ((next_val * 100) / cur_val);

			printk("%d ", val);
			cur_val = (s16)(hfdnd_v_gap[i][j]);

			if (val > cur_val) {
				fail_val = val;
				x = i;
				y = j;
				node_num = offset;
				result = false;
				}
			raw_data->vgap_data[offset] = val;
			}
		printk("\n");
			}

	if (result) {
		input_info(true, &info->client->dev, "HF DND V Gap Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "HF DND V Gap Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, 0, hfdnd_v_gap[x][y], fail_val);
			}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void run_hfdnd_h_gap_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, next_val, x, y, node_num, fail_val;
	bool result = true;

	sec_cmd_set_default_result(sec);

	memset(raw_data->hgap_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "HF DND H Gap start\n");

	for (i = 0; i < x_num ; i++) {
		for (j = 0; j < y_num-1; j++) {
			offset = (i * y_num) + j;

			cur_val = raw_data->hfdnd_data[offset];
			if (!cur_val) {
				raw_data->hgap_data[offset] = cur_val;
				continue;
			}

			next_val = raw_data->hfdnd_data[offset + 1];
			if (!next_val) {
				raw_data->hgap_data[offset] = next_val;
				for (++j; j < y_num - 1; j++) {
					offset = (i * y_num) + j;

					next_val = raw_data->hfdnd_data[offset];
					if (!next_val) {
						raw_data->hgap_data[offset]
							= next_val;
						continue;
			}

					break;
		}
	}

			if (next_val > cur_val)
				val = 100 - ((cur_val * 100) / next_val);
	   else
				val = 100 - ((next_val * 100) / cur_val);

			printk("%d ", val);
			cur_val = (s16)(hfdnd_h_gap[i][j]);

			if (val > cur_val) {
				fail_val = val;
				x = i;
				y = j;
				node_num = offset;
			result = false;
			}
			raw_data->hgap_data[offset] = val;
		}
		printk("\n");
		}
	
	if (result) {
		input_info(true, &info->client->dev, "HF DND H Gap Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "HF DND H Gap Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, 0, hfdnd_h_gap[x][y], fail_val);
	}

	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
}

static void get_hfdnd_h_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num || y_node < 0 || y_node >= y_num - 1) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	sprintf(buf, "%d", raw_data->hgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
}

static void get_hfdnd_v_gap(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_node, y_node;
	int node_num;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= x_num - 1 || y_node < 0 || y_node >= y_num) {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = (x_node * y_num) + y_node;

	sprintf(buf, "%d", raw_data->vgap_data[node_num]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
}

static void run_gapjitter_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int i, j, offset, val, cur_val, x, y, node_num, fail_val;
	bool result = true;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	memset(raw_data->gapjitter_data, 0x00, TSP_CMD_NODE_NUM);

	input_info(true, &info->client->dev, "Gap Jitter start\n");

	ts_set_touchmode16(TOUCH_H_GAP_JITTER_MODE);
	get_raw_data(info, (u8 *)raw_data->gapjitter_data, 2);
	write_cmd(misc_info->client, BT541_CLEAR_INT_STATUS_CMD);
	ts_set_touchmode16(TOUCH_POINT_MODE);

	for (i = 0; i < x_num ; i++) {
		for (j = 0; j < y_num-1; j++) {
			offset = (i * y_num) + j;

			val = raw_data->gapjitter_data[offset];

			printk("%d ", val);
			cur_val = (s16)(hfdnd_h_jitter_gap[i][j]);

			if (val > cur_val) {
				fail_val = val;
				x = i;
				y = j;
				node_num = offset;
				result = false;
			}
		}
		printk("\n");
		}

	if (result) {
		input_info(true, &info->client->dev, "Gap Jitter Pass\n");
		snprintf(buf, sizeof(buf), "OK\n");
	} else {
		input_err(true, &info->client->dev, "Gap Jitter Fail\n");
		snprintf(buf, sizeof(buf), "Fail,%d,%d,%d,%d\n",
				node_num, 0, hfdnd_h_jitter_gap[x][y], fail_val);
	}

	sec_cmd_set_cmd_result(sec, buf,strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static void get_gapjitter(void * device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;
	int offset, x_node, y_node;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	sec_cmd_set_default_result(sec);
	
	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buf, sizeof(buf), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}
	
	offset = (x_node * y_num) + y_node;
	snprintf(buf, sizeof(buf), "%d", raw_data->gapjitter_data[offset]);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buf);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}

static bool get_raw_data_size(struct bt541_ts_info *info, u8 *buff, int skip_cnt, int sz)
{
	struct i2c_client *client = info->client;
	struct bt541_ts_platform_data *pdata = info->pdata;
	int i;
	u32 temp_sz;
	int retry = 50;

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		input_err(true, &info->client->dev, "other process occupied.. (%d)\n", info->work_state);
		up(&info->work_lock);
		return false;
	}

	info->work_state = RAW_DATA;

	for (i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int))
			msleep(1);

		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
		msleep(1);
	}

	while (gpio_get_value(pdata->gpio_int) && retry-- > 0 )
		msleep(1);
	
	if (retry < 0) {
		input_info(true, &info->client->dev, "%s failed\n", __func__ );
		info->work_state = NOTHING;
		up(&info->work_lock);
		return false;
	}
	

	for (i = 0; sz > 0; i++) {
		temp_sz = I2C_BUFFER_SIZE;

		if (sz < I2C_BUFFER_SIZE)
			temp_sz = sz;
		if (read_raw_data(client, BT541_RAWDATA_REG + i,
			(char *)(buff + (i * I2C_BUFFER_SIZE)), temp_sz) < 0) {

			input_err(true, &misc_info->client->dev, "error : read zinitix tc raw data\n");
			info->work_state = NOTHING;
			up(&info->work_lock);
			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}

	write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	up(&info->work_lock);

	return true;
}

static void run_reference_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	struct tsp_raw_data *raw_data = info->raw_data;
	int min = 0xFFFF, max = 0x0000;
	s32 i, j, touchkey_node = 2;
	int buffer_offset;
	char buf[SEC_CMD_STR_LEN] = { 0 };

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_REFERENCE_MODE);
	get_raw_data_size(info, (u8 *)raw_data->reference_data, 2,
		2 * (info->cap_info.total_node_num * 2 + info->cap_info.y_node_num + info->cap_info.x_node_num));
	ts_set_touchmode(TOUCH_POINT_MODE);

	for (i = 0; i < info->cap_info.x_node_num; i++) {
		printk("%s: ref_data[%2d] : ", __func__, i);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			printk(" %5d", raw_data->reference_data[i * info->cap_info.y_node_num + j]);

			if (i == (info->cap_info.x_node_num - 1)) {
				if ((j == touchkey_node)||(j == (info->cap_info.y_node_num - 1) - touchkey_node)) {
					if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] < min &&
						raw_data->reference_data[(i * info->cap_info.y_node_num) + j] >= 0)
						min = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];

					if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] > max)
						max = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];
				}
			} else {
				if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] < min &&
					raw_data->reference_data[(i * info->cap_info.y_node_num) + j] >= 0)
					min = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];

				if (raw_data->reference_data[(i * info->cap_info.y_node_num) + j] > max)
					max = raw_data->reference_data[(i * info->cap_info.y_node_num) + j];
			}
		}
		printk("\n");
	}

	snprintf(buf, sizeof(buf), "%d,%d", min, max);
	sec_cmd_set_cmd_result(sec, buf,strnlen(buf, sizeof(buf)));

	buffer_offset = info->cap_info.total_node_num;
	for (i = 0; i < info->cap_info.x_node_num; i++) {
		printk("%s: ref_data(mode1)[%2d] : ", __func__, i);
		for (j = 0; j < info->cap_info.y_node_num; j++) {
			printk(" %5d", raw_data->reference_data[buffer_offset + i * info->cap_info.y_node_num + j]);
		}
		printk("\n");
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
	enable_irq(info->irq);

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif
	return;
}


static void get_reference(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	//struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
/*	struct i2c_client *client = info->client;
	struct tsp_factory_info *finfo = info->factory_info;
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;*/

	sec_cmd_set_default_result(sec);
/*
	x_node = finfo->cmd_param[0];
	y_node = finfo->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%s", "abnormal");
		set_cmd_result(info, finfo->cmd_buff,
		strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
		finfo->cmd_state = FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->ref_data[node_num];
	snprintf(finfo->cmd_buff, sizeof(finfo->cmd_buff), "%u", val);
	set_cmd_result(info, finfo->cmd_buff,
	strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
	finfo->cmd_state = OK;

	dev_info(&client->dev, "%s: %s(%d)\n", __func__, finfo->cmd_buff,
			(int)strnlen(finfo->cmd_buff, sizeof(finfo->cmd_buff)));
*/
	return;
}

static void get_dnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buf, sizeof(buf), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->dnd_data[node_num];
	snprintf(buf, sizeof(buf), "%u", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
		(int)strnlen(buf, sizeof(buf)));

	return;
}

static void get_hfdnd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buf, sizeof(buf), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->hfdnd_data[node_num];
	snprintf(buf, sizeof(buf), "%u", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
		(int)strnlen(buf, sizeof(buf)));

	return;
}

static void run_delta_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	s16 min, max;
	s32 i,j, offset;
	int x_num = info->cap_info.x_node_num, y_num = info->cap_info.y_node_num;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	disable_irq(info->irq);
	sec_cmd_set_default_result(sec);

	ts_set_touchmode(TOUCH_DELTA_MODE);
	get_raw_data(info, (u8 *)raw_data->delta_data, 10);
	ts_set_touchmode(TOUCH_POINT_MODE);

	min = (s16)0x7FFF;
	max = (s16)0x8000;

	for (i = 0; i < x_num; i++) {
		for (j = 0; j < y_num; j++) {
//			printk("delta_data : %d\n", raw_data->delta_data[j+i]);
			offset = (i * y_num) + j;
			printk("%d ", raw_data->delta_data[offset]);

			if (raw_data->delta_data[offset] < min &&
					raw_data->delta_data[offset] != 0)
				min = raw_data->delta_data[offset];

			if (raw_data->delta_data[offset] > max)
				max = raw_data->delta_data[offset];

		}
		printk("\n");
	}

	snprintf(buf, sizeof(buf), "%d,%d\n", min, max);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: \"%s\"(%d)\n", __func__, buf,
			(int)strlen(buf));

	enable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif

	return;
}

static void get_delta(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	struct tsp_raw_data *raw_data = info->raw_data;
	unsigned int val;
	int x_node, y_node;
	int node_num;

	sec_cmd_set_default_result(sec);

	x_node = sec->cmd_param[0];
	y_node = sec->cmd_param[1];

	if (x_node < 0 || x_node >= info->cap_info.x_node_num ||
		y_node < 0 || y_node >= info->cap_info.y_node_num) {
		snprintf(buf, sizeof(buf), "%s", "abnormal");
		sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;

		return;
	}

	node_num = x_node * info->cap_info.y_node_num + y_node;

	val = raw_data->delta_data[node_num];
	snprintf(buf, sizeof(buf), "%u", val);
	sec_cmd_set_cmd_result(sec, buf, strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__, buf,
		(int)strnlen(buf, sizeof(buf)));

	return;
}

static void hfdnd_spec_adjust(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	int test;

	sec_cmd_set_default_result(sec);

	test = sec->cmd_param[0];

	if (test) {
		dnd_h_gap = assy_dnd_h_gap;
		dnd_v_gap = assy_dnd_v_gap;
		hfdnd_h_gap = assy_hfdnd_h_gap;
		hfdnd_v_gap = assy_hfdnd_v_gap;
		dnd_max = assy_dnd_max;
		dnd_min = assy_dnd_min;
		hfdnd_max = assy_hfdnd_max;
		hfdnd_min = assy_hfdnd_min;		
		hfdnd_h_jitter_gap = assy_hfdnd_h_jitter_gap;
	} else {
		dnd_h_gap = tsp_dnd_h_gap;
		dnd_v_gap = tsp_dnd_v_gap;
		hfdnd_h_gap = tsp_hfdnd_h_gap;
		hfdnd_v_gap = tsp_hfdnd_v_gap;
		dnd_max = tsp_dnd_max;
		dnd_min = tsp_dnd_min;
		hfdnd_max = tsp_hfdnd_max;
		hfdnd_min = tsp_hfdnd_min;		
		hfdnd_h_jitter_gap = tsp_hfdnd_h_jitter_gap;
	}

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
	return;
}

static void clear_reference_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	struct i2c_client *client = info->client;
	char buf[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL, 0);
#endif

	write_reg(client, BT541_EEPROM_INFO_REG, 0xffff);

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	usleep_range(100, 100);
	if (write_cmd(client, BT541_SAVE_STATUS_CMD) != I2C_SUCCESS)
		return;

	msleep(500);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);
	usleep_range(100, 100);

#if ESD_TIMER_INTERVAL
	write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	input_info(true, &info->client->dev, "%s: TSP clear calibration bit\n", __func__);

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
			buf, (int)strnlen(buf, sizeof(buf)));
	return;
}

static void run_force_calibration(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	char buff = 0;
	struct i2c_client *client = info->client;
	int i;
	bool ret;

	disable_irq(info->irq);
	
	sec_cmd_set_default_result(sec);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
	write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL, 0);
#endif

	write_reg(client, 0x01CA, 1300);
	ret = ts_hw_calibration(info);
	if (ret == true) {
		input_info(true, &client->dev, "%s: TSP calibration Pass\n", __func__);
	} else {
		input_info(true, &client->dev, "%s: TSP calibration Fail\n", __func__);
		goto out;
	}

#ifdef PAT_CONTROL
/* adb and factory lcia */
	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &client->dev, "failed ium lock\n", __func__);
		goto out;
	}
	buff = get_tsp_nvm_data(info, PAT_CAL_DATA);
	if (buff >= PAT_MAGIC_NUMBER)
		buff = 1;
	else if (buff >= PAT_MAX_LCIA)
		buff = PAT_MAX_LCIA;
	else
		buff = buff + 1;
	
	info->cal_count = buff;

	if (read_data(client, BT541_FIRMWARE_VERSION, (u8 *)&info->cap_info.fw_version, 2) < 0)
		goto out;
	if (read_data(client, BT541_DATA_VERSION_REG, (u8 *)&info->cap_info.reg_data_version, 2) < 0)
		goto out;
	if (read_data(client, BT541_MINOR_FW_VERSION, (u8 *)&info->cap_info.fw_minor_version, 2) < 0)
		goto out;
	set_tsp_nvm_data(info, PAT_CAL_DATA, info->cal_count);
	set_tsp_nvm_data(info, PAT_DUMMY_VERSION, (info->cap_info.fw_version << 4) | info->cap_info.fw_minor_version);
	set_tsp_nvm_data(info, PAT_FIX_VERSION, info->cap_info.reg_data_version);
	info->tune_fix_ver = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) |
				get_tsp_nvm_data(info, PAT_FIX_VERSION);
	input_info(true, &client->dev, " %s info->cal_count:0x%02x info->tune_fix_ver: 0x%04x\n", __func__,
		info->cal_count, info->tune_fix_ver);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &client->dev, "failed ium unlock\n", __func__);
		goto out;
	}
#endif

out:
	write_reg(client, 0x01CA, 0);
	write_reg(client, 0x0149, 0);

	for (i = 0; i < 5; i++) {
		write_cmd(client, BT541_CLEAR_INT_STATUS_CMD);
		usleep_range(10, 10);
	}
#if ESD_TIMER_INTERVAL
	write_reg(client, BT541_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	if (ret) {
		snprintf(buf, sizeof(buf), "%s", "OK");
		sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_OK;
	} else {
		snprintf(buf, sizeof(buf), "%s", "NG");
		sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	input_info(true, &info->client->dev, "%s: %s(%d)\n", __func__,
				buf, (int)strnlen(buf, sizeof(buf)));

	enable_irq(info->irq);
	
	return;
}

#ifdef PAT_CONTROL
static void get_pat_information(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

    /* fixed tune version will be saved at excute autotune */
	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
		goto FAIL;
	}
	info->cal_count = get_tsp_nvm_data(info, PAT_CAL_DATA);
	info->tune_fix_ver = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) |
		get_tsp_nvm_data(info, PAT_FIX_VERSION);
	if (tsp_nvm_ium_unlock(info) == false){
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
	}
	input_info(true, &info->client->dev, "%s info->cal_count: 0x%02x"
		" info->tune_fix_ver:0x%04x\n", __func__, info->cal_count, info->tune_fix_ver);
	snprintf(buff, sizeof(buff), "P%02XT%04X",info->cal_count, info->tune_fix_ver);

	sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
FAIL:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, (int)strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	input_info(true, &info->client->dev, "%s: %s\n", __func__, buff);
	return;
}

static void get_calibration_nv_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	char buff = 0;

	sec_cmd_set_default_result(sec);
	
	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
	}
	buff = get_tsp_nvm_data(info, PAT_CAL_DATA);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
	}

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: fix_ver_data 0x%02x  %s(%d)\n", __func__,
			buff, buf, (int)strnlen(buf, sizeof(buf)));
	return;        
}

static void get_tune_fix_ver_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	char buff = 0;

	sec_cmd_set_default_result(sec);
	
	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
	}
	buff = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) + get_tsp_nvm_data(info, PAT_FIX_VERSION);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
	}

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: fix_ver_data 0x%04x  %s(%d)\n", __func__,
			buff, buf, (int)strnlen(buf, sizeof(buf)));
	return;        
}

static void set_calibration_nv_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];
	char buff = 0;

	sec_cmd_set_default_result(sec);

	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
	}
	set_tsp_nvm_data(info, PAT_CAL_DATA, val);
	buff = get_tsp_nvm_data(info, PAT_CAL_DATA);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
	}

	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: nv_data 0x%02x %s(%d) \n", __func__,
			buff, buf, (int)strnlen(buf, sizeof(buf)));
	return;        
}

static void set_tune_fix_ver_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];
	char buff = 0;

	sec_cmd_set_default_result(sec);

	if (tsp_nvm_ium_lock(info) == false) {
		input_err(true, &info->client->dev, "failed ium lock\n", __func__);
	}
	//set_tsp_nvm_data(info, PAT_DUMMY_VERSION, 0x00);
	set_tsp_nvm_data(info, PAT_FIX_VERSION, val);
	buff = (get_tsp_nvm_data(info, PAT_DUMMY_VERSION) << 8) + get_tsp_nvm_data(info, PAT_FIX_VERSION);
	if (tsp_nvm_ium_unlock(info) == false) {
		input_err(true, &info->client->dev, "failed ium unlock\n", __func__);
	}
 
	snprintf(buf, sizeof(buf), "%s", "OK");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s: fix_ver_data 0x%04x %s(%d)\n", __func__,
			buff, buf, (int)strnlen(buf, sizeof(buf)));
	return;        
}
#endif

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct bt541_ts_info *info = container_of(sec, struct bt541_ts_info, sec);
	char buf[SEC_CMD_STR_LEN] = { 0 };
	int val = sec->cmd_param[0];

	sec_cmd_set_default_result(sec);

	if(val) //disable
		zinitix_bit_clr(m_optional_mode, 3);
	else //enable
		zinitix_bit_set(m_optional_mode, 3);

	snprintf(buf, sizeof(buf), "dead_zone %s", val ? "disable" : "enable");
	sec_cmd_set_cmd_result(sec, buf, (int)strnlen(buf, sizeof(buf)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, &info->client->dev, "%s(), %s\n", __func__, buf);

	return;
}

static ssize_t show_enabled(struct device *dev, struct device_attribute
			*devattr, char *buf)
{
	struct bt541_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	int val = 0;

	if (info->work_state == EALRY_SUSPEND || info->work_state == SUSPEND)
		val = 0;
	else
		val = 1;
	input_info(true, &client->dev, " enabled is %d", info->work_state);

	return snprintf(buf, sizeof(val), "%d\n", val);
}
static DEVICE_ATTR(enabled, S_IRUGO, show_enabled, NULL);

static struct attribute *sec_touch_pretest_attributes[] = {
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group sec_touch_pretest_attr_group = {
	.attrs	= sec_touch_pretest_attributes,
};


#ifdef SUPPORTED_TOUCH_KEY
static ssize_t show_touchkey_threshold(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bt541_ts_info *info = dev_get_drvdata(dev);
	struct capa_info *cap = &(info->cap_info);

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	input_info(true, &info->client->dev, "%s: key threshold = %d\n", __func__,
			cap->key_threshold);

	return snprintf(buf, 41, "%d", cap->key_threshold);
#else
	input_info(true, &info->client->dev, "%s: key threshold = %d %d %d %d\n", __func__,
			cap->dummy_threshold, cap->key_threshold, cap->key_threshold, cap->dummy_threshold);

	return snprintf(buf, 41, "%d %d %d %d", cap->dummy_threshold,
			cap->key_threshold,  cap->key_threshold,
			cap->dummy_threshold);
#endif
}

static ssize_t show_touchkey_sensitivity(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct bt541_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u16 val = 0;
	int ret = 0;
	int i;

#ifdef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	if (!strcmp(attr->attr.name, "touchkey_recent"))
		i = 0;
	else if (!strcmp(attr->attr.name, "touchkey_back"))
		i = 1;
	else {
		input_err(true, &info->client->dev, "%s: Invalid attribute\n", __func__);

		goto err_out;
	}

#else
	if (!strcmp(attr->attr.name, "touchkey_dummy_btn1"))
		i = 0;
	else if (!strcmp(attr->attr.name, "touchkey_recent"))
		i = 1;
	else if (!strcmp(attr->attr.name, "touchkey_back"))
		i = 2;
	else if (!strcmp(attr->attr.name, "touchkey_dummy_btn4"))
		i = 3;
	else if (!strcmp(attr->attr.name, "touchkey_dummy_btn5"))
		i = 4;
	else if (!strcmp(attr->attr.name, "touchkey_dummy_btn6"))
		i = 5;
	else {
		input_err(true, &info->client->dev, "%s: Invalid attribute\n", __func__);

		goto err_out;
	}
#endif
	down(&info->work_lock);
	ret = read_data(client, BT541_BTN_WIDTH + i, (u8 *)&val, 2);
	up(&info->work_lock);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s: Failed to read %d's key sensitivity\n",
				__func__, i);

		goto err_out;
	}

	input_info(true, &info->client->dev, "%s: %d's key sensitivity = %d\n",
			__func__, i, val);

	return snprintf(buf, 6, "%d", val);

err_out:
	return sprintf(buf, "NG");
}

static ssize_t show_back_key_raw_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t show_menu_key_raw_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, show_touchkey_threshold, NULL);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, show_touchkey_sensitivity, NULL);
#ifndef NOT_SUPPORTED_TOUCH_DUMMY_KEY
static DEVICE_ATTR(touchkey_dummy_btn1, S_IRUGO,
		show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn3, S_IRUGO,
		show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn4, S_IRUGO,
		show_touchkey_sensitivity, NULL);
static DEVICE_ATTR(touchkey_dummy_btn6, S_IRUGO,
		show_touchkey_sensitivity, NULL);
#endif
static DEVICE_ATTR(touchkey_raw_back, S_IRUGO, show_back_key_raw_data, NULL);
static DEVICE_ATTR(touchkey_raw_menu, S_IRUGO, show_menu_key_raw_data, NULL);

static struct attribute *touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_raw_menu.attr,
	&dev_attr_touchkey_raw_back.attr,
#ifndef NOT_SUPPORTED_TOUCH_DUMMY_KEY
	&dev_attr_touchkey_dummy_btn1.attr,
	&dev_attr_touchkey_dummy_btn3.attr,
	&dev_attr_touchkey_dummy_btn4.attr,
	&dev_attr_touchkey_dummy_btn6.attr,
#endif
	NULL,
};
static struct attribute_group touchkey_attr_group = {
	.attrs = touchkey_attributes,
};
#endif

static int init_sec_factory(struct bt541_ts_info *info)
{
	struct i2c_client *client = info->client;
#ifdef SUPPORTED_TOUCH_KEY
	struct device *factory_tk_dev;
#endif
	struct device *sec_pretest_dev;
	struct tsp_raw_data *raw_data;
	int retval;

	raw_data = kzalloc(sizeof(struct tsp_raw_data), GFP_KERNEL);
	if (unlikely(!raw_data)) {
		input_err(true, &info->client->dev, "%s: Failed to allocate memory\n",
			__func__);
		retval = -ENOMEM;
		return retval;
	}

	retval = sec_cmd_init(&info->sec, bt541_commands,
			ARRAY_SIZE(bt541_commands), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &info->client->dev,
			"%s: Failed to sec_cmd_init\n", __func__);
		kfree(raw_data);
		return retval;
	}

	retval = sysfs_create_link(&info->sec.fac_dev->kobj,
		&info->input_dev->dev.kobj, "input");
	if (retval < 0) {
		input_err(true, &client->dev, "%s [ERROR] sysfs_create_link\n", __func__);
		sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
		kfree(raw_data);
		return retval;
	}

#ifdef SUPPORTED_TOUCH_KEY
	factory_tk_dev = sec_device_create(info, "sec_touchkey");
	if (IS_ERR(factory_tk_dev)) {
		input_err(true, &info->client->dev, "Failed to create factory dev\n");
		retval = -ENODEV;
		goto err_create_device;
	}
#endif

	/* /sys/class/sec/tsp/input/ */
	sec_pretest_dev = sec_device_create(info, "input");
	if (IS_ERR(sec_pretest_dev)) {
		input_err(true, &info->client->dev, "Failed to create device (%s)!\n", "tsp");
		goto err_create_device;
	}

#ifdef SUPPORTED_TOUCH_KEY
	retval = sysfs_create_group(&factory_tk_dev->kobj, &touchkey_attr_group);
	if (unlikely(retval)) {
		input_err(true, &info->client->dev, "Failed to create touchkey sysfs group\n");
		goto err_create_sysfs;
	}
#endif

	info->raw_data = raw_data;

	dnd_h_gap = assy_dnd_h_gap;
	dnd_v_gap = assy_dnd_v_gap;
	hfdnd_h_gap = assy_hfdnd_h_gap;
	hfdnd_v_gap = assy_hfdnd_v_gap;
	hfdnd_h_jitter_gap = assy_hfdnd_h_jitter_gap;
	dnd_max = assy_dnd_max;
	dnd_min = assy_dnd_min;
	hfdnd_max = assy_hfdnd_max;
	hfdnd_min = assy_hfdnd_min;	
	reference_data_abnormal_max = assy_reference_data_abnormal_max;
	return retval;

err_create_sysfs:
err_create_device:
	kfree(raw_data);

	return retval;
}
#endif

static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 version;
	u16 mode;
	const struct firmware *fw;
	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;

	if (misc_info == NULL) {
		pr_err("%s misc device NULL?\n", SECLOG);
		return -1;
	}

	switch (cmd) {

	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, 4)) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}
		if (nval)
			input_err(true, &misc_info->client->dev, "[zinitix_touch] on debug mode (%d)\n",
					nval);
		else
			input_err(true, &misc_info->client->dev, "[zinitix_touch] off debug mode (%d)\n",
					nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;

		input_info(true, &misc_info->client->dev, "[zinitix_touch]: firmware size = %d, %d\r\n",
			(int)sz, misc_info->cap_info.ic_fw_size);
		if (misc_info->cap_info.ic_fw_size != sz) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: firmware size error\r\n");
			return -1;
		}
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_DATA:
		request_firmware(&fw, misc_info->pdata->fw_name, &misc_info->client->dev);

		if (!fw) {
			input_err(true, &misc_info->client->dev, "%s [ERROR] request_firmware\n", __func__);
			return -1;
		}
		
		if (copy_from_user(fw->data,
					argp, misc_info->cap_info.ic_fw_size)) {
			release_firmware(fw);
			return -1;
		}

		version = (u16) (fw->data[52] | (fw->data[53]<<8));

		input_err(true, &misc_info->client->dev, "[zinitix_touch]: firmware version = %x\r\n", version);

		if (copy_to_user(argp, &version, sizeof(version))) {
			release_firmware(fw);
			return -1;
		}
		
		release_firmware(fw);

		break;

	case TOUCH_IOCTL_START_UPGRADE:
		request_firmware(&fw, misc_info->pdata->fw_name, &misc_info->client->dev);
		if (!fw) {
			input_err(true, &misc_info->client->dev, "%s [ERROR] request_firmware\n", __func__);
			return -1;
		}
		ret = ts_upgrade_sequence((u8 *)fw->data, fw->size);

		release_firmware(fw);

		return ret;
		
	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		msleep(100);

		/* h/w calibration */
		if (ts_hw_calibration(misc_info) == true)
			ret = 0;

		mode = misc_info->touch_mode;
		if (write_reg(misc_info->client,
					BT541_TOUCH_MODE, mode) != I2C_SUCCESS) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: failed to set touch mode %d.\n",
					mode);
			goto fail_hw_cal;
		}

		if (write_cmd(misc_info->client,
					BT541_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;
fail_hw_cal:
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (copy_from_user(&nval, argp, 4)) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user\r\n");
			misc_info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode((u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]:other process occupied.. (%d)\n",
					misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl,
					argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (read_data(misc_info->client,
					(u16)reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

		if (copy_to_user((void *)(unsigned long)reg_ioctl.val, (u8 *)&nval, 4)) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_to_user\n");
			return -1;
		}

		input_info(true, &misc_info->client->dev, "read : reg addr = 0x%x, val = 0x%x\n",
				reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_SET_REG:

		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\n",
					misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
					argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user(1)\n");
			return -1;
		}

		if (copy_from_user(&val, (void *)(unsigned long)reg_ioctl.val, 4)) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user(2)\n");
			return -1;
		}

		if (write_reg(misc_info->client,
					(u16)reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		input_info(true, &misc_info->client->dev, "write : reg addr = 0x%x, val = 0x%x\r\n",
				reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_DONOT_TOUCH_EVENT:
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.. (%d)\r\n",
					misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (write_reg(misc_info->client,
					BT541_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		input_info(true, &misc_info->client->dev, "write : reg addr = 0x%x, val = 0x0\r\n",
				BT541_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_SEND_SAVE_STATUS:
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			input_err(true, &misc_info->client->dev, "[zinitix_touch]: other process occupied.." \
					"(%d)\r\n", misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;
		write_reg(misc_info->client, 0xc003, 0x0001);
		write_reg(misc_info->client, 0xc104, 0x0001);
		if (write_cmd(misc_info->client,
					BT541_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		msleep(1000);	/* for fusing eeprom */
		write_reg(misc_info->client, 0xc003, 0x0000);
		write_reg(misc_info->client, 0xc104, 0x0000);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		down(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			up(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
					argp, sizeof(struct raw_ioctl))) {
			up(&misc_info->raw_data_lock);
			input_err(true, &misc_info->client->dev, "[zinitix_touch] error : copy_from_user\r\n");
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];
		if (raw_ioctl.sz > MAX_TRAW_DATA_SZ*2)
			raw_ioctl.sz = MAX_TRAW_DATA_SZ*2;
		if (copy_to_user((void *)(unsigned long)raw_ioctl.buf, (u8 *)u8Data,
					raw_ioctl.sz)) {
			up(&misc_info->raw_data_lock);
			return -1;
		}

		up(&misc_info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tsp_dt_ids[] = {
	{ .compatible = "Zinitix,bt541_ts", },
	{},
};
MODULE_DEVICE_TABLE(of, tsp_dt_ids);

static int bt541_ts_probe_dt(struct device_node *np,
		struct device *dev,
		struct bt541_ts_platform_data *pdata)
{
	//struct bt541_ts_info *info = dev_get_drvdata(dev);
	int ret;

	input_dbg(true, dev, "%s [START]\n", __func__);

	if (!np)
		return -EINVAL;

	pdata->gpio_int = of_get_named_gpio(np,"bt541,irq-gpio", 0);
	if (pdata->gpio_int < 0) {
		input_err(true, dev, "%s: of_get_named_gpio failed: %d\n", __func__,
				pdata->gpio_int);
		pdata->gpio_int = 188;
	}

	pdata->vdd_en = of_get_named_gpio(np,"bt541,vdd_en", 0);
	if (pdata->vdd_en < 0) {
		pdata->vdd_en_flag = 0;
		input_err(true, dev, "%s: vdd_en_flag %d\n", __func__, pdata->vdd_en_flag);
	} else {
		pdata->vdd_en_flag = 1;
		input_err(true, dev, "%s: vdd_en_flag %d\n", __func__, pdata->vdd_en_flag);
	}

	ret = of_property_read_u32(np, "bt541,x_resolution", &pdata->x_resolution);
	if (ret) {
		pdata->x_resolution = 539;
		input_err(true, dev, "%s [ERROR] max_x and hard data insert %d\n", __func__, pdata->x_resolution);
	}

	ret = of_property_read_u32(np, "bt541,y_resolution", &pdata->y_resolution);
	if (ret) {
		pdata->y_resolution = 959;
		input_err(true, dev, "%s [ERROR] max_y and hard data insert %d\n", __func__, pdata->y_resolution);
	}

	ret = of_property_read_u32(np, "bt541,orientation", &pdata->orientation);
	if (ret) {
		pdata->orientation = 0;
		input_err(true, dev, "%s [ERROR] orientation and hard data insert %d\n", __func__, pdata->orientation);
	}

	ret = of_property_read_u32(np, "bt541,page_size", &pdata->page_size);
	if (ret) {
		pdata->page_size = 128;
		input_err(true, dev, "%s [ERROR] orientation and hard data insert %d\n", __func__, pdata->page_size);
	}
	ret = of_property_read_string(np, "bt541,fw_name", &pdata->fw_name);
	if (ret < 0)
		pdata->fw_name = "tsp_zinitix/bt541_GP.fw";

	ret = of_property_read_u32(np, "bt541,bringup", &pdata->bringup);
	if (ret < 0)
		pdata->bringup = 0;

	ret = of_property_read_u32(np, "bt541,mis_cal_check", &pdata->mis_cal_check);
	if (ret < 0)
		pdata->mis_cal_check = 0;

#ifdef	PAT_CONTROL
	ret = of_property_read_u32(np, "bt541,pat_function", &pdata->pat_function);
	if (ret < 0) {
		pdata->pat_function = 0;
		input_err(true, dev, "Failed to get pat_function property\n");
	}

	ret = of_property_read_u32(np, "bt541,afe_base", &pdata->afe_base);
	if (ret < 0) {
		pdata->afe_base = 0;
		input_err(true, dev, "Failed to get afe_Base property\n");		  
	}
#endif
	input_info(true, dev, "%s: get touch_regulator start\n");
	pdata->vreg_vio = regulator_get(dev, "vgp1");
	if (IS_ERR(pdata->vreg_vio)) {
		pdata->vreg_vio = NULL;
		input_info(true, dev, "%s: get touch_regulator error\n",
				__func__);
		if (pdata->vdd_en_flag) {
			input_info(true, dev, "%s: touch vdd control use gpio_en\n", __func__);
		} else {
			input_info(true, dev, "%s: touch vdd control do not use gpio_en"
				"and don't get regulator error\n", __func__);
			return -EIO;
		}

	}
	if (pdata->vreg_vio) {
		ret = regulator_set_voltage(pdata->vreg_vio, 3000000, 3000000);
		if (ret < 0)
			input_info(true, dev, "%s: set voltage error(%d)\n", __func__, ret);
	}
	input_info(true, dev, "%s max_x:%d, max_y:%d, irq:%d, orientation:%d, page_size:%d,"
		"bringup:%d vdd_en_flag:%d pat_function:%d afe_base:%d mis_cal_check:%d\n", __func__, pdata->x_resolution,
		pdata->y_resolution, pdata->gpio_int, pdata->orientation,
		pdata->page_size, pdata->bringup, pdata->vdd_en_flag, pdata->pat_function, pdata->afe_base, pdata->mis_cal_check);

	return 0;
}
#endif

int bt541_pinctrl_configure(struct bt541_ts_info *info, int active)
{
	struct pinctrl_state *set_state_i2c;
	int retval;

	input_dbg(true, &info->client->dev, "%s: %d\n", __func__, active);

	if (active) {
		set_state_i2c =	pinctrl_lookup_state(info->pinctrl, "tsp_int_active");
		if (IS_ERR(set_state_i2c)) {
			input_err(true, &info->client->dev,
				"%s: cannot get pinctrl(i2c) active state\n", __func__);
			return PTR_ERR(set_state_i2c);
		}
	} else {
		set_state_i2c =	pinctrl_lookup_state(info->pinctrl, "tsp_int_suspend");
		if (IS_ERR(set_state_i2c)) {
			input_err(true, &info->client->dev,
				"%s: cannot get pinctrl(i2c) suspend state\n", __func__);
			return PTR_ERR(set_state_i2c);
		}
	}
	retval = pinctrl_select_state(info->pinctrl, set_state_i2c);
	if (retval) {
		input_err(true, &info->client->dev,
			"%s: cannot set pinctrl(i2c) %d state\n", __func__, active);
		return retval;
	}

	return 0;
}


static int bt541_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bt541_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;

	struct device_node *np = client->dev.of_node;

	/*info->octa_id = get_lcd_attached("GET");
	if (!octa_id) {
		input_err(true, &client->dev, "%s: LCD is not attached\n", __func__);
		ret = -EIO;
		goto err_octa_id;
	}*/

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "Not compatible i2c function\n");
		ret = -EIO;
		goto err_i2c_check;
	}
	
	info = kzalloc(sizeof(struct bt541_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		info->pdata = devm_kzalloc(&client->dev,
				sizeof(struct bt541_ts_platform_data), GFP_KERNEL);
		if (!info->pdata) {
			input_err(true, &client->dev,
					"%s [ERROR] pdata devm_kzalloc\n", __func__);
			goto err_no_platform_data;
		}
		ret = bt541_ts_probe_dt(np, &client->dev, info->pdata);
		if (ret) {
			input_err(true, &client->dev, "%s [ERROR] tsp is not"
					" vdd_en or vdd_regulator\n", __func__);
			goto err_no_platform_data;
		}
	} else
#endif
	{
		info->pdata = client->dev.platform_data;
		if (info->pdata == NULL) {
			input_err(true, &client->dev, "%s [ERROR] pdata is null\n", __func__);
			ret = -EINVAL;
			goto err_no_platform_data;
		}
	}

	ret = gpio_request(info->pdata->gpio_int, "bt541_int");
	if (ret < 0) {
		input_err(true, &client->dev, "%s: Request GPIO failed, gpio %d (%d)\n", BT541_TS_DEVICE,
				info->pdata->gpio_int, ret);
		goto err_gpio_alloc;
	}
	/* if pinctrl set up at dts file, gpio_direction_input don't use. */ 
	/*gpio_direction_input(info->pdata->gpio_int);*/
	i2c_set_clientdata(client, info);
	info->client = client;
	input_dev = input_allocate_device();
	if (!input_dev) {
		input_err(true, &client->dev, "Failed to allocate input device\n");
		ret = -EPERM;
		goto err_alloc;
	}

	info->input_dev = input_dev;
	info->work_state = PROBE;
	info->enabled = true;

	/* because buffer size, MTK use TPD_SUPPORT_I2C_DMA */ 
#if TPD_SUPPORT_I2C_DMA
	client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	gpDMABuf_va = (u8 *) dma_alloc_coherent(&client->dev, IIC_DMA_MAX_TRANSFER_SIZE,
		&gpDMABuf_pa, GFP_KERNEL);
	if (!gpDMABuf_va) {
		input_err(true, &client->dev, "%s Allocate DMA I2C Buffer failed!\n", __func__);
		ret = -EPERM;
		goto err_dma;
	}
	memset(gpDMABuf_va, 0, IIC_DMA_MAX_TRANSFER_SIZE);
#endif

	/* MTK use DTS file pinctrl, If it don't use, pinctrl is default. */
	info->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(info->pinctrl)) {
		if (PTR_ERR(info->pinctrl) == -EPROBE_DEFER)
			goto err_pinctrl;

		input_err(true, &info->client->dev, "%s: Target does not use pinctrl\n", __func__);
		info->pinctrl = NULL;
	}

	if (info->pinctrl) {
		ret = bt541_pinctrl_configure(info, 1);
		if (ret)
			input_err(true, &info->client->dev, "%s: cannot set pinctrl state\n", __func__);
	}

	if (bt541_power_control(info, POWER_ON_SEQUENCE) == false) {
		ret = -EPERM;
		goto err_power_sequence;
	}
	
	/* FW version read from tsp */

	memset(&info->reported_touch_info,
			0x0, sizeof(struct point_info));

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;
	info->pat_flag = false;
	if (init_touch(info, fw_true) == false) {
		ret = -EPERM;
		goto err_init_touch;
	}

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		info->button[i] = ICON_BUTTON_UNCHANGE;

	snprintf(info->phys, sizeof(info->phys),
			"%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen";
	input_dev->id.bustype = BUS_I2C;
	/*	input_dev->id.vendor = 0x0001; */
	input_dev->phys = info->phys;
	/*	input_dev->id.product = 0x0002; */
	/*	input_dev->id.version = 0x0100; */
	input_dev->dev.parent = &client->dev;
#if BT541_USE_INPUT_OPEN_CLOSE
	input_dev->open = bt541_ts_open;
	input_dev->close = bt541_ts_close;
#endif
	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		set_bit(BUTTON_MAPPING_KEY[i], info->input_dev->keybit);

	if (info->pdata->orientation & TOUCH_XY_SWAP) {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
				info->cap_info.MinX,
				info->cap_info.MaxX + ABS_PT_OFFSET,
				0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
				info->cap_info.MinY,
				info->cap_info.MaxY + ABS_PT_OFFSET,
				0, 0);
	} else {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
				info->cap_info.MinX,
				info->cap_info.MaxX + ABS_PT_OFFSET,
				0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
				info->cap_info.MinY,
				info->cap_info.MaxY + ABS_PT_OFFSET,
				0, 0);
	}

	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
			0, 255, 0, 0);
#ifdef CONFIG_SEC_FACTORY
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
			0, 3000, 0, 0);
#endif
	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MAJOR,
			0, 255, 0, 0);

#if (TOUCH_POINT_MODE == 2)
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
			0, 255, 0, 0);
	/*	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MINOR,
		0, 255, 0, 0); */
	/*	input_set_abs_params(info->input_dev, ABS_MT_ORIENTATION,
		-128, 127, 0, 0);  */
	/*	input_set_abs_params(info->input_dev, ABS_MT_ANGLE,
		-90, 90, 0, 0);*/
	input_set_abs_params(info->input_dev, ABS_MT_PALM,
			0, 1, 0, 0);
#endif

	set_bit(MT_TOOL_FINGER, info->input_dev->keybit);
	input_mt_init_slots(info->input_dev, info->cap_info.multi_fingers,0);

	input_info(true, &client->dev, "register %s input device\n",
			info->input_dev->name);
	input_set_drvdata(info->input_dev, info);
	ret = input_register_device(info->input_dev);
	if (ret) {
		input_err(true, &info->client->dev, "unable to register %s input device\r\n",
				info->input_dev->name);
		goto err_input_register_device;
	}

	/* configure irq */
	info->irq = gpio_to_irq(info->pdata->gpio_int);
	if (info->irq < 0)
		input_info(true, &client->dev, "error. gpio_to_irq(..) function is not \
				supported? you should define GPIO_TOUCH_IRQ.\n");

	input_info(true, &client->dev, "request irq (irq = %d, pin = %d) \r\n",
			info->irq, info->pdata->gpio_int);

	info->work_state = NOTHING;

	sema_init(&info->work_lock, 1);

#if ESD_TIMER_INTERVAL
	spin_lock_init(&info->lock);
	INIT_WORK(&info->tmr_work, ts_tmr_work);
	esd_tmr_workqueue =
		create_singlethread_workqueue("esd_tmr_workqueue");

	if (!esd_tmr_workqueue) {
		input_err(true, &client->dev, "Failed to create esd tmr work queue\n");
		ret = -EPERM;

		goto err_esd_input_unregister_device;
	}

	esd_timer_init(info);
	esd_timer_start(CHECK_ESD_TIMER, info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Started esd timer\n");
#endif
#endif
	ret = request_threaded_irq(info->irq, NULL, bt541_touch_work,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT , BT541_TS_DEVICE, info);

	if (ret) {
		input_err(true, &client->dev, "unable to register irq.(%s)\n",
				info->input_dev->name);
		goto err_request_irq;
	}
	input_info(true, &client->dev, "zinitix touch probe.\r\n");
#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = bt541_ts_early_suspend;
	info->early_suspend.resume = bt541_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#if defined(CONFIG_PM_RUNTIME) && !defined(CONFIG_HAS_EARLYSUSPEND)
	pm_runtime_enable(&client->dev);
#endif

	sema_init(&info->raw_data_lock, 1);

	ret = misc_register(&touch_misc_device);
	if (ret) {
		input_err(true, &client->dev, "Failed to register touch misc device\n");
		goto err_misc_register;
	}

#ifdef CONFIG_SEC_FACTORY_TEST
	ret = init_sec_factory(info);
	if (ret) {
		input_err(true, &client->dev, "Failed to init sec factory device\n");

		goto err_kthread_create_failed;
	}
#endif
	return 0;

#ifdef CONFIG_SEC_FACTORY_TEST
err_kthread_create_failed:
	kfree(info->raw_data);
#endif
err_misc_register:
	free_irq(info->irq, info);
err_request_irq:
#if ESD_TIMER_INTERVAL
err_esd_input_unregister_device:
#endif
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;
err_input_register_device:
err_init_touch:
err_power_sequence:
err_pinctrl:
#if TPD_SUPPORT_I2C_DMA
err_dma:
#endif
	if (info->input_dev) {
		input_free_device(info->input_dev);
	}
	gpio_free(info->pdata->gpio_int);
err_alloc:
err_gpio_alloc:
err_no_platform_data:
	input_info(true, &client->dev, "Failed to probe\n");
	kfree(info);
err_mem_alloc:
err_i2c_check:
//err_octa_id:
	return ret;
}

static int bt541_ts_remove(struct i2c_client *client)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);
	struct bt541_ts_platform_data *pdata = info->pdata;

	disable_irq(info->irq);
	down(&info->work_lock);

	info->work_state = REMOVE;

#ifdef CONFIG_SEC_FACTORY_TEST
	kfree(info->raw_data);
#endif
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	write_reg(info->client, BT541_PERIODICAL_INTERRUPT_INTERVAL, 0);
	esd_timer_stop(info);
#if defined(TSP_VERBOSE_DEBUG)
	input_info(true, &client->dev, "Stopped esd timer\n");
#endif
	destroy_workqueue(esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);

	misc_deregister(&touch_misc_device);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	up(&info->work_lock);
	kfree(info);

	return 0;
}

void bt541_ts_shutdown(struct i2c_client *client)
{
	struct bt541_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &client->dev, "%s++\n",__func__);
	disable_irq(info->irq);
	down(&info->work_lock);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	esd_timer_stop(info);
#endif
	up(&info->work_lock);
	bt541_power_control(info, POWER_OFF);
	input_info(true, &client->dev, "%s--\n",__func__);
}


static struct i2c_device_id bt541_idtable[] = {
	{BT541_TS_DEVICE, 0},
	{ }
};

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops bt541_ts_pm_ops ={
#if defined(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(bt541_ts_suspend, bt541_ts_resume, NULL)
#else
	SET_SYSTEM_SLEEP_PM_OPS(bt541_ts_suspend, bt541_ts_resume)
#endif
};
#endif

static struct i2c_driver bt541_ts_driver = {
	.probe		= bt541_ts_probe,
	.remove		= bt541_ts_remove,
	.shutdown	= bt541_ts_shutdown,
	.id_table	= bt541_idtable,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= BT541_TS_DEVICE,
		.of_match_table	= tsp_dt_ids,
#if 0 /* do not pm */
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm		= &bt541_ts_pm_ops,
#endif
#endif
	},
};

#ifdef CONFIG_BATTERY_SAMSUNG
extern unsigned int lpcharge;
#endif

static void __init bt541_ts_init_async(void *dummy, async_cookie_t cookie)
{
	i2c_add_driver(&bt541_ts_driver);
}

static int __init bt541_ts_init(void)
{
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge) {
		pr_err("%s %s : LPM Charging mode!!\n", SECLOG, __func__);
		return 0;
	}
#endif
	pr_err("%s %s \n", SECLOG, __func__);

	return async_schedule(bt541_ts_init_async, NULL); 
}

static void __exit bt541_ts_exit(void)
{
	i2c_del_driver(&bt541_ts_driver);
}

module_init(bt541_ts_init);
module_exit(bt541_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_LICENSE("GPL");

