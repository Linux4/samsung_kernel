#ifndef __MELFAS_TS_H__
#define __MELFAS_TS_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm_wakeup.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/kernel.h>
#include <linux/spu-verify.h>

#include "melfas_reg.h"

#include <linux/sec_class.h>
#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/sec_batt.h>
#endif

#ifdef CONFIG_VBUS_NOTIFIER
#include <linux/muic/common/muic.h>
#include <linux/muic/common/muic_notifier.h>
#include <linux/vbus_notifier.h>
#endif

#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
#include <linux/atomic.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <soc/qcom/scm.h>

#define SECURE_TOUCH_ENABLE	1
#define SECURE_TOUCH_DISABLE	0
#endif

#if IS_ENABLED(CONFIG_TOUCHSCREEN_DUMP_MODE)
#include "../sec_tsp_dumpkey.h"
extern struct tsp_dump_callbacks dump_callbacks;
#endif

#include "../sec_input.h"
#include "../sec_tsp_log.h"

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
#include <linux/input/stui_inf.h>
#endif


#ifndef I2C_M_DMA_SAFE
#define I2C_M_DMA_SAFE		0
#endif

#define MMS_FW_UPDATE_DEBUG	0

#define MELFAS_TS_DEVICE_NAME	"melfas_ts"
#define MELFAS_TS_I2C_NAME	"MELFAS_TS"
#define TOUCH_PRINT_INFO_DWORK_TIME	30000	/* 30s */
#define POWER_ON_DELAY        150 /* ms */
#define TOUCH_RESET_DWORK_TIME		10
#define TOUCH_POWER_ON_DWORK_TIME	100

#define MAX_FINGER_NUM			10
#define MELFAS_TS_EVENT_BUFF_SIZE		16

#define MELFAS_TS_ESD_COUNT_FOR_DISABLE		7
#define MELFAS_TS_FW_MAX_SECT_NUM		4

/* vector id */
#define VECTOR_ID_SCREEN_RX				1
#define VECTOR_ID_SCREEN_TX				2
#define VECTOR_ID_KEY_RX				3
#define VECTOR_ID_KEY_TX				4
#define VECTOR_ID_PRESSURE				5
#define VECTOR_ID_OPEN_RESULT				7
#define VECTOR_ID_OPEN_RX				8
#define VECTOR_ID_OPEN_TX				9
#define VECTOR_ID_SHORT_RESULT				10
#define VECTOR_ID_SHORT_RX				11
#define VECTOR_ID_SHORT_TX				12

#define OPEN_SHORT_TEST		1
#define CHECK_ONLY_OPEN_TEST	1
#define CHECK_ONLY_SHORT_TEST	2

#define MELFAS_TS_COORDINATE_ACTION_NONE		0
#define MELFAS_TS_COORDINATE_ACTION_PRESS		1
#define MELFAS_TS_COORDINATE_ACTION_MOVE		2
#define MELFAS_TS_COORDINATE_ACTION_RELEASE	3

#define DIFF_SCALER 1000

/* 16 byte */
struct melfas_ts_event_coordinate {
	u8 eid:2;
	u8 tid:4;
	u8 tchsta:2;
	u8 x_11_4;
	u8 y_11_4;
	u8 y_3_0:4;
	u8 x_3_0:4;
	u8 major;
	u8 minor;
	u8 z:6;
	u8 ttype_3_2:2;
	u8 left_event:5;
	u8 max_energy_flag:1;
	u8 ttype_1_0:2;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 noise_status:2;
	u8 reserved10:2;
	u8 virtual_key_info;
	u8 reserved12;
	u8 reserved13;
	u8 reserved14;
	u8 reserved15;

} __attribute__ ((packed));

/* 16 byte */
struct melfas_ts_gesture_status {
	u8 eid:2;
	u8 stype:4;
	u8 sf:2;
	u8 gesture_id;
	u8 gesture_data_1;
	u8 gesture_data_2;
	u8 gesture_data_3;
	u8 gesture_data_4;
	u8 reserved_1;
	u8 left_event_5_0:5;
	u8 reserved_2:3;
	u8 noise_level;
	u8 max_strength;
	u8 hover_id_num:4;
	u8 reserved10:4;
	u8 reserved11;
	u8 reserved12;
	u8 reserved13;
	u8 reserved14;
	u8 reserved15;
} __attribute__ ((packed));

struct melfas_tsp_snr_result_of_point {
	s16	max;
	s16	min;
	s16	average;
	s16	nontouch_peak_noise;
	s16	touch_peak_noise;
	s16	snr1;
	s16	snr2;
} __packed;

/**
* Device info structure
*/
struct melfas_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct input_dev *input_dev_proximity;
	char phys[32];
	struct pinctrl *pinctrl;
	struct sec_ts_plat_data *plat_data;
	struct sec_tclm_data *tdata;

	dev_t melfas_dev;
	struct class *class;

	struct mutex device_mutex;
	struct mutex i2c_mutex;
	struct mutex eventlock;
	struct mutex modechange;
	struct mutex sponge_mutex;
	struct mutex proc_mutex;

	struct sec_cmd_data sec;

	struct melfas_ts_event_coordinate coord[MAX_FINGER_NUM];
	volatile bool reset_is_on_going;
	struct delayed_work work_read_info;
	struct delayed_work work_print_info;
	struct delayed_work reset_work;
	struct delayed_work check_rawdata;

	int test_min;
	int test_max;
	int test_diff_max;

	u8 product_name[16];
	u8 event_id;

	int fod_lp_mode;

	u8 esd_cnt;
	bool disable_esd;

	unsigned int sram_addr_num;
	u32 sram_addr[8];

	u8 *print_buf;
	int *image_buf;

	bool test_busy;

#ifndef CONFIG_SAMSUNG_PRODUCT_SHIP
	struct cdev cdev;
	u8 *dev_fs_buf;
#endif
#ifdef CONFIG_VBUS_NOTIFIER
	struct notifier_block vbus_nb;
	bool ta_stsatus;
#endif
#ifdef USE_TSP_TA_CALLBACKS
	void (*register_cb)(struct tsp_callbacks *);
	struct tsp_callbacks callbacks;
#endif
	int debug_flag;

	u8 tsp_dump_lock;
	bool probe_done;
	u8 glove_enable;

	int open_short_type;
	int open_short_result;

	u32 defect_probability;
	u8 item_cmdata;
	u8 hover_event; /* keystring for protos */
#ifdef CONFIG_INPUT_SEC_SECURE_TOUCH
	atomic_t secure_enabled;
	atomic_t secure_pending_irqs;
	struct completion secure_powerdown;
	struct completion secure_interrupt;
#endif
	bool sponge_inf_dump;
	u8 sponge_dump_format;
	u8 sponge_dump_event;
	u8 sponge_dump_border_msb;
	u8 sponge_dump_border_lsb;
	bool sponge_dump_delayed_flag;
	u8 sponge_dump_delayed_area;
	u16 sponge_dump_border;

	int (*melfas_ts_i2c_write)(struct melfas_ts_data *ts, u8 *reg, int cnum, u8 *data, int len);
	int (*melfas_ts_i2c_read)(struct melfas_ts_data *ts, u8 *reg, int cnum, u8 *data, int len);
	int (*melfas_ts_read_sponge)(struct melfas_ts_data *ts, u8 *data, int len);
	int (*melfas_ts_write_sponge)(struct melfas_ts_data *ts, u8 *data, int len);
};

/**
* Firmware binary header info
*/
struct melfas_bin_hdr {
	char	 tag[8];
	u16	 core_version;
	u16	 section_num;
	u16	 contains_full_binary;
	u16	 reserved0;

	u32	 binary_offset;
	u32	 binary_length;

	u32	 extention_offset;
	u32	 reserved1;
} __attribute__ ((packed));

/**
* Firmware image info
*/
struct melfas_fw_img {
	u16	 type;
	u16	 version;

	u16	 start_page;
	u16	 end_page;

	u32	 offset;
	u32	 length;
} __attribute__ ((packed));

/*
* Firmware update error code
*/
enum fw_update_errno {
	FW_ERR_FILE_READ = -4,
	FW_ERR_FILE_OPEN = -3,
	FW_ERR_FILE_TYPE = -2,
	FW_ERR_DOWNLOAD = -1,
	FW_ERR_NONE = 0,
	FW_ERR_UPTODATE = 1,
};

extern struct device *ptsp;

//fn
void melfas_ts_unlocked_release_all_finger(struct melfas_ts_data *ts);
void melfas_ts_locked_release_all_finger(struct melfas_ts_data *ts);
void melfas_set_grip_data_to_ic(struct device *dev, u8 flag);
void melfas_ts_run_rawdata(struct melfas_ts_data *ts, bool on_probe);
void melfas_ts_reset_work(struct work_struct *work);
void melfas_ts_read_info_work(struct work_struct *work);
void melfas_ts_print_info_work(struct work_struct *work);
void melfas_ts_get_custom_library(struct melfas_ts_data *ts);
void melfas_ts_reset(struct melfas_ts_data *ts, unsigned int ms);
int melfas_ts_set_cover_type(struct melfas_ts_data *ts, bool enable);
int melfas_ts_set_custom_library(struct melfas_ts_data *ts);
int melfas_ts_set_press_property(struct melfas_ts_data *ts);
int melfas_ts_set_fod_rect(struct melfas_ts_data *ts);
int melfas_ts_set_aod_rect(struct melfas_ts_data *ts);
int melfas_ts_ear_detect_enable(struct melfas_ts_data *ts, u8 enable);
int melfas_ts_pocket_mode_enable(struct melfas_ts_data *ts, u8 enable);
int melfas_ts_set_glove_mode(struct melfas_ts_data *ts, bool enable);
int melfas_ts_get_fw_version(struct melfas_ts_data *ts, u8 *ver_buf);
int melfas_ts_init_config(struct melfas_ts_data *ts);
int melfas_ts_read_from_sponge(struct melfas_ts_data *ts, u8 *data, int len);
int melfas_ts_write_to_sponge(struct melfas_ts_data *ts, u8 *data, int len);
int melfas_ts_set_lowpowermode(void *data, u8 mode);
int melfas_ts_get_image(struct melfas_ts_data *ts, u8 image_type);
int melfas_ts_run_test(struct melfas_ts_data *ts, u8 test_type);

#ifdef CONFIG_VBUS_NOTIFIER
int melfas_ts_charger_attached(struct melfas_ts_data *ts, bool status);
int melfas_ts_vbus_notification(struct notifier_block *nb, unsigned long cmd, void *data);
#endif
#ifdef USE_TSP_TA_CALLBACKS
void melfas_ts_charger_status_cb(struct tsp_callbacks *cb, int status);
void melfas_ts_register_callback(struct tsp_callbacks *cb);
#endif

//dump
void melfas_ts_init_proc(struct melfas_ts_data *ts);
void melfas_ts_sponge_dump_flush(struct melfas_ts_data *ts, int dump_area);
void melfas_ts_dump_tsp_log(struct device *dev);
void melfas_ts_check_rawdata(struct work_struct *work);

//fw
int melfas_ts_firmware_update_on_probe(struct melfas_ts_data *ts, bool force_update, bool on_probe);
int melfas_ts_firmware_update_on_hidden_menu(struct melfas_ts_data *ts, int update_type);

//cmd
int melfas_ts_fn_init(struct melfas_ts_data *ts);
void melfas_ts_sysfs_remove(struct melfas_ts_data *ts);
void melfas_ts_fn_remove(struct melfas_ts_data *ts);

//CORE
int melfas_ts_disable_esd_alert(struct melfas_ts_data *ts);

//vendor
int melfas_ts_dev_create(struct melfas_ts_data *ts);
int melfas_ts_sysfs_create(struct melfas_ts_data *ts);

//interrupt
irqreturn_t melfas_ts_irq_thread(int irq, void *ptr);

#endif
