#ifndef JADARD_COMMON_H
#define JADARD_COMMON_H

#include <asm/segment.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/async.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/input/mt.h>
#include <linux/firmware.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/wakelock.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 start*/
#include <linux/rtc.h>
#include <linux/syscalls.h>
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 end*/
#include "jadard_platform.h"
#include <linux/touchscreen_info.h>
#include <linux/of_irq.h>
/*hs04 code for DEAL6398A-1067 by tangsumian at 20220820 start*/
#include <linux/init.h>
/*hs04 code for DEAL6398A-1067 by tangsumian at 20220820 end*/
/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 start*/
#include <linux/input/sec_cmd.h>
/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 end*/
#include "../tpd.h"

#if defined(JD_CONFIG_FB)
	#include <linux/notifier.h>
	#include <linux/fb.h>
#endif

#ifdef CONFIG_OF
	#include <linux/of_gpio.h>
#endif

#define JADARD_DRIVER_VER        "01.01" /* MAJOR_VER.MINOR_VER */
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 start*/
#define JADARD_DRIVER_CID_VER	 "00.01" /* MAJOR_CID_VER.MINOR_CID_VER */
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 end*/
#define JADARD_PROC_TOUCH_FOLDER "jadard_touch"
#define JD_FW_DUMP_FILE             "/sdcard/JD_FW_Dump.bin"
#define JD_TS_WAKE_LOCK_TIMEOUT     (5000)
#define JD_READ_LEN_OVERFLOW        (1)
#define JD_NO_ERR                   (0)
#define JD_WRITE_OVERFLOW           (-1)
#define JD_MEM_ALLOC_FAIL           (-2)
#define JD_UPGRADE_CONFLICT         (-3)
#define JD_TIME_OUT                 (-4)
#define JD_CHIP_ID_ERROR            (-5)
#define JD_APP_START_FAIL           (-6)
#define JD_CHECK_DATA_ERROR         (-7)
#define JD_FILE_OPEN_FAIL           (-8)
#define JD_INPUT_REG_FAIL           (-9)

#define JD_TOUCH_DATA_SIZE       (64)
#define JD_HID_TOUCH_DATA_SIZE   (JD_TOUCH_DATA_SIZE + 4)
#define JD_FINGER_DATA_SIZE      (5)
#define JD_TOUCH_STATE_INFO_SIZE (5)
#define JD_FINGER_NUM_ADDR       (0)
#define JD_TOUCH_COORD_INFO_ADDR (3)
#define JD_TOUCH_STATE_INFO_ADDR (53)
#define JD_TOUCH_EVENT_INFO_ADDR (2)
#define JD_GEST_SUP_NUM          (23)
/* HS04 code for SR-AL6398A-01-652 by hehaoran5 at 20220719 start*/
#define JD_UPGRADE_FW_RETRY_TIME (10)
/* HS04 code for SR-AL6398A-01-652 by hehaoran5 at 20220719 end*/
/*=========== Config select start ===========*/
/*
 *	Can't enable JD_AUTO_UPGRADE_FW and JD_ZERO_FLASH
 *	at the same time
 */
/* #define JD_AUTO_UPGRADE_FW */
#define JD_ZERO_FLASH

/*
 *	Can't enable JD_USB_DETECT_GLOBAL and JD_USB_DETECT_CALLBACK
 *	at the same time
 */
/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210610 start*/
#define JD_USB_DETECT_GLOBAL
/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210610 end*/
/* #define JD_USB_DETECT_CALLBACK */

/*
 *	JD_PROTOCOL_A : Report specific format of MTK platform
 *	JD_PROTOCOL_B(undef JD_PROTOCOL_A) : input_mt_init_slots(A, B) format of QCT platform
 *	JD_PROTOCOL_B_3PA : input_mt_init_slots(A, B, C) format of QCT platform
 */
/* #define JD_PROTOCOL_A */
#define JD_PROTOCOL_B_3PA

#define JD_RST_PIN_FUNC
#define JD_SMART_WAKEUP
#define JD_HIGH_SENSITIVITY
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 start*/
#define JD_EARPHONE_DETECT
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 end*/
/* #define CONFIG_CHIP_DTCFG */
/*=========== Config select end =============*/

enum JD_DATA_TYPE {
	JD_DATA_TYPE_RawData = 1,
	JD_DATA_TYPE_Baseline,
	JD_DATA_TYPE_Difference,
	/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 start*/
	JD_DATA_TYPE_LISTEN,
	JD_DATA_TYPE_LABEL,
	/*hs03s code for SR-AL5625-01-439 by yuanliding at 20210616 end*/
	JD_DATA_TYPE_REPORT_COORD
};

enum JD_KEEP_TYPE {
	JD_KEEP_TYPE_NormalValue = 1,
	JD_KEEP_TYPE_MaxValue,
	JD_KEEP_TYPE_MinValue,
	JD_KEEP_TYPE_PeakValue
};

enum JD_IRQ_EVENT {
	JD_REPORT_COORD = 1,
	JD_REPORT_SMWP_EVENT,
};

enum JD_TS_STATUS {
	JD_TS_GET_DATA_FAIL = -2,
	JD_IRQ_EVENT_FAIL = -1,
	JD_TS_NORMAL_START = 0,
	JD_REPORT_DATA,
	JD_RST_OK,
};

enum JD_DBIC_REG_ADDR {
	JD_DBIC_BASE_ADDR  = 0x400001,
	JD_DBIC_CMD        = (JD_DBIC_BASE_ADDR << 8) + 0x40,
	JD_DBIC_WRITE_DATA = (JD_DBIC_BASE_ADDR << 8) + 0x41,
	JD_DBIC_READ_DATA  = (JD_DBIC_BASE_ADDR << 8) + 0x42,
	JD_DBIC_DUMMY1     = (JD_DBIC_BASE_ADDR << 8) + 0x43,
	JD_DBIC_INT_EN     = (JD_DBIC_BASE_ADDR << 8) + 0x44,
	JD_DBIC_INT_CLR    = (JD_DBIC_BASE_ADDR << 8) + 0x45,
	JD_DBIC_SPI_RUN    = (JD_DBIC_BASE_ADDR << 8) + 0x46,
	JD_DBIC_CMD_LEN    = (JD_DBIC_BASE_ADDR << 8) + 0x47,
	JD_DBIC_SPI_FREQ   = (JD_DBIC_BASE_ADDR << 8) + 0x48,
	JD_DBIC_STATUS     = (JD_DBIC_BASE_ADDR << 8) + 0x49,
};

enum JD_DBIC_RELATED_SETTING {
	JD_DBIC_READ_WRITE_SUCCESS = 0x00,
	JD_DBIC_READ_WRITE_FAIL    = 0x01,
	JD_DBIC_IRQ_VALID_MSK      = 0x03,
	JD_DBIC_WRITE_RUN          = 0x01,
	JD_DBIC_READ_RUN           = 0x03,
	JD_DBIC_INT_CLR_BUSY       = 0x40,
	JD_DBIC_STATUS_RDY_MSK     = 0x80,
	JD_DBIC_READ_SET_CMD       = 0xF1,
	JD_DBIC_READ_DATA_CMD      = 0xF2,
};

struct jadard_ts_data {
	bool suspended;
	bool rawdata_little_endian;
	atomic_t suspend_mode;
	uint8_t protocol_type;
	uint8_t irq_enabled;
	uint8_t debug_log_touch_level;
	bool debug_fw_package_enable;
	int rst_gpio;
	bool rst_active;
	bool sorting_active;
	
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 start*/
	char *firmware_name;
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 end*/
	/*hs03s code for SR-AL5625-01-441 by yuanliding at 20210621 start*/
	char *ito_file_name;
	/*hs03s code for SR-AL5625-01-441 by yuanliding at 20210621 end*/

	struct device *dev;
	struct input_dev *input_dev;
	struct i2c_client *client;
	struct jadard_i2c_platform_data *pdata;
	struct mutex rw_lock;

/******* SPI-start *******/
	struct mutex spi_lock;
	struct spi_device *spi;
	int jd_irq;
/******* SPI-end *******/

#if defined(JD_CONFIG_FB)
	struct notifier_block fb_notif;
	struct workqueue_struct *jadard_fb_wq;
	struct delayed_work work_fb;
#endif

	struct workqueue_struct *fw_dump_wq;
	struct work_struct fw_dump_work;

#ifdef JD_AUTO_UPGRADE_FW
	struct workqueue_struct *jadard_upgrade_wq;
	struct delayed_work work_upgrade;
#endif

#ifdef JD_ZERO_FLASH
	struct workqueue_struct *jadard_0f_upgrade_wq;
	struct delayed_work work_0f_upgrade;
#endif

	struct workqueue_struct *jadard_diag_wq;
	struct delayed_work jadard_diag_delay_wrok;

#ifdef JD_SMART_WAKEUP
	bool SMWP_enable;
	bool gesture_cust_en[JD_GEST_SUP_NUM];
	struct wake_lock ts_SMWP_wake_lock;
#endif

#if defined(JD_USB_DETECT_CALLBACK) || defined(JD_USB_DETECT_GLOBAL)
	uint8_t usb_connected;
	uint8_t *usb_status;
#endif

#ifdef JD_HIGH_SENSITIVITY
	bool high_sensitivity_enable;
#endif
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 start*/
#ifdef JD_EARPHONE_DETECT
	bool earphone_enable;
#endif
/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 end*/
	/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 start*/
	struct sec_cmd_data sec;
	/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 end*/

	/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 start*/
	struct notifier_block earphone_notif;
	/*hs04 code for DEAL6398A-522 by huangzhongjie at 20220808 end*/

	/*hs03s  code for DEVAL5626-589 by huangzhongjie at 20210902 start*/
	bool tp_is_enabled;
	/*hs03s  code for DEVAL5626-589 by huangzhongjie at 20210902 end*/
};

struct jadard_ic_data {
	char     chip_id[15];
	uint32_t fw_ver;
	uint32_t fw_cid_ver;
	int	     JD_X_NUM;
	int	     JD_Y_NUM;
	int	     JD_X_RES;
	int	     JD_Y_RES;
	int	     JD_MAX_PT;
	bool     JD_INT_EDGE;
};

enum jadard_input_protocol_type {
	PROTOCOL_TYPE_A	= 0,
	PROTOCOL_TYPE_B,
};

struct jadard_report_data {
	int touch_data_size;
	int touch_coord_size;
	uint8_t *touch_coord_info;
	uint8_t touch_state_info[JD_TOUCH_STATE_INFO_SIZE];
#if defined(JD_SMART_WAKEUP)
	uint8_t touch_event_info;
#endif
#if defined(CONFIG_TOUCHSCREEN_JADARD_DEBUG)
	uint32_t report_rate_count;
	uint8_t touch_all_info[JD_HID_TOUCH_DATA_SIZE];
#endif
};

struct jadard_host_data {
	uint8_t finger_num;
	uint8_t *id;
	int *x;
	int *y;
	int *w;
	int *event;
#ifdef JD_SMART_WAKEUP
	int SMWP_event_chk;
#endif
};

enum jadard_host_data_event{
    JD_DOWN_EVENT = 0,
    JD_UP_EVENT,
    JD_CONTACT_EVENT,
    JD_NO_EVENT
};

enum jadard_finger_event{
    JD_ATTN_IN = 1,
    JD_ATTN_OUT,
};

struct jadard_debug {
	bool proc_send_flag;
	bool *fw_dump_going;
	void (*fp_touch_dbg_func)(struct jadard_ts_data *ts, uint8_t start);
};


int jadard_chip_common_init(void);
void jadard_chip_common_deinit(void);
void jadard_ts_work(struct jadard_ts_data *ts);
uint8_t jadard_DbicWriteDDReg(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
uint8_t jadard_DbicReadDDReg(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
int jadard_report_data_init(void);
void jadard_report_points(struct jadard_ts_data *ts);
int jadard_distribute_touch_data(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status);
int jadard_parse_report_data(struct jadard_ts_data *ts, int irq_event, int ts_status);
void jadard_log_touch_state(void);
int jadard_input_register(struct jadard_ts_data *ts);

#if defined(JD_USB_DETECT_GLOBAL)
void jadard_cable_detect(bool force_renew);
#endif

extern enum tp_module_used tp_is_used;
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 start*/
extern char *mtp_chip_name;
extern int mtp_fw_ver;
/*hs03s  code for SR-AL5625-01-96 by wangdeyan at 20210519 end*/
extern int jadard_chip_common_suspend(struct jadard_ts_data *ts);
extern int jadard_chip_common_resume(struct jadard_ts_data *ts);
extern int jadard_parse_dt(struct jadard_ts_data *ts,
						struct jadard_i2c_platform_data *pdata);
extern int jadard_dev_set(struct jadard_ts_data *ts);
extern int jadard_input_register_device(struct input_dev *input_dev);

#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
extern int jadard_debug_init(void);
extern int jadard_debug_remove(void);
#endif

/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 start*/
extern int jadard_get_array_size(void);
extern struct sec_cmd jadard_commands[];
/*hs03s code for SR-AL5625-01-305 by yuanliding at 20210521 end*/

#endif
