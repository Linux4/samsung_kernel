/*
 * GalaxyCore touchscreen driver
 *
 * Copyright (C) 2021 GalaxyCore Incorporated
 *
 * Copyright (C) 2021 Neo Chen <neo_chen@gcoreinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef GCORE_TPD_COMMON_H_
#define GCORE_TPD_COMMON_H_

/*----------------------------------------------------------*/
/* INCLUDE FILE                                             */
/*----------------------------------------------------------*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include "gcore_drv_config.h"

#include <linux/cdev.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>


#include <uapi/linux/sched/types.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)) \
	|| (LINUX_VERSION_CODE > KERNEL_VERSION(4, 10, 0))
#include <uapi/linux/sched/types.h>
#endif
#include <linux/notifier.h>
#if defined(CONFIG_DRV_SAMSUNG)
#include <linux/input/sec_cmd.h>
#endif

#if defined(CONFIG_DRV_SAMSUNG)
#define SEC_TSP_FACTORY_TEST
#endif
/*
 * Note.
 * Misc Debug Option
 */
/* #define CONFIG_DEBUG_SPI_SPEED */

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM
#define TP_RESUME_BY_FB_NOTIFIER
#endif

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#include "../tpd.h"
#ifdef CONFIG_MTK_LEGACY_PLATFORM
#include "mtk_spi.h"
#endif
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
#define TP_PS_DEVICE 		"gcore_ps"
#define TP_PS_INPUT_DEV		"alps_pxy"
#endif
//+S96818AA1-1936,daijun1.wt,add,2023/09/15,n28-tp add firmware upgrade path switching function
extern u8 gcore_hostdownload_cfg;
//-S96818AA1-1936,daijun1.wt,add,2023/09/15,n28-tp add firmware upgrade path switching function
/*
 * Log define
 */
#define GTP_ERROR(fmt, arg...)          pr_err("<GTP-ERR>[%s:%d] "fmt"\n", \
													__func__, __LINE__, ##arg)
#define GTP_DEBUG(fmt, arg...)				\
	do {									\
		if (1)						\
			pr_err("<GTP-DBG>[%s:%d]"fmt"\n", __func__, __LINE__, ##arg);\
	} while (0)
#define GTP_REPORT(fmt, arg...)				\
	do {									\
		if (CONFIG_ENABLE_REPORT_LOG)						\
			pr_debug("<GTP-REP>[%s:%d]"fmt"\n", __func__, __LINE__, ##arg);\
	} while (0)

#define GTP_DRIVER_NAME               "gcore"
#define GTP_MAX_TOUCH                 10
#define DEMO_DATA_SIZE                (6 * GTP_MAX_TOUCH + 1 + 2 + 2)

static const struct of_device_id tpd_of_match[] = {
	{.compatible = "gcore,touchscreen"},
	{},
};


/*
 * Raw Data
 */

#if defined(CONFIG_ENABLE_CHIP_TYPE_GC7371)
#define RAW_DATA_SIZE               (1296)
#define RAWDATA_ROW					(36)
#define RAWDATA_COLUMN				(18)
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7271)
#define RAW_DATA_SIZE               (1080)
#define RAWDATA_ROW					(36)
#define RAWDATA_COLUMN				(15)
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202)
#define RAW_DATA_SIZE               (1152)
#define RAWDATA_ROW					(32)
#define RAWDATA_COLUMN				(18)
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7372)
#define RAW_DATA_SIZE               (1296)
#define RAWDATA_ROW					(36)
#define RAWDATA_COLUMN				(18)
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7302)
#define RAW_DATA_SIZE               (1296)
#define RAWDATA_ROW					(36)
#define RAWDATA_COLUMN				(18)
#elif defined(CONFIG_ENABLE_CHIP_TYPE_GC7202H_GC7272)
#define RAW_DATA_SIZE               (1152)
#define RAWDATA_ROW					(32)
#define RAWDATA_COLUMN				(18)
#endif

#define DEMO_RAWDATA_SIZE           (DEMO_DATA_SIZE + RAW_DATA_SIZE)
#define FW_SIZE                     (64 * 1024)

#define FW_VERSION_ADDR               0xFFF4
//+S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 add CB size
#define CB_SIZE                        (2048)
//-S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 add CB size

#define DEMO_RAWDATA_MAX_SIZE       (2048)
extern int g_rawdata_row;
extern int g_rawdata_col;
extern int notify_contin;

#define BIT0   (1<<0)		/* 0x01 */
#define BIT1   (1<<1)		/* 0x02 */
#define BIT2   (1<<2)		/* 0x04 */
#define BIT3   (1<<3)		/* 0x08 */
#define BIT4   (1<<4)		/* 0x10 */
#define BIT5   (1<<5)		/* 0x20 */
#define BIT6   (1<<6)		/* 0x40 */
#define BIT7   (1<<7)		/* 0x80 */

/*
 * GESTURE WAKEUP
 */
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#define GESTURE_NULL               0
#define GESTURE_DOUBLE_CLICK       1
#define GESTURE_UP                 2
#define GESTURE_DOWN               3
#define GESTURE_LEFT               4
#define GESTURE_RIGHT              5
#define GESTURE_C                  6
#define GESTURE_E                  7
#define GESTURE_M                  8
#define GESTURE_N                  9
#define GESTURE_O                  10
#define GESTURE_S                  11
#define GESTURE_V                  12
#define GESTURE_W                  13
#define GESTURE_Z                  14
#define GESTURE_PALM               15

#define GESTURE_MAX                16

#define GESTURE_KEY   KEY_WAKEUP
#endif

#ifdef CONFIG_ENABLE_FW_RAWDATA
enum FW_MODE {
		DEMO,
		RAWDATA,
		DEMO_RAWDATA,
		DEMO_RAWDATA_DEBUG,
		FW_DEBUG
};
#endif

enum fw_event_type {
	FW_UPDATE = 0,
	FW_READ_REG,
	FW_WRITE_REG,
	FW_READ_OPEN,
	FW_READ_SHORT,
	FW_EDGE_0,
	FW_EDGE_90,
	FW_CHARGER_PLUG,
	FW_CHARGER_UNPLUG,
	FW_HEADSET_PLUG,
	FW_HEADSET_UNPLUG,
	FW_READ_RAWDATA,
	FW_READ_DIFFDATA,
	FW_READ_NOISE,
	FW_GESTURE_ENABLE,
	FW_GESTURE_DISABLE,
	FW_GLOVE_ENABLE,
	FW_GLOVE_DISABLE,
	FW_REPORT_RATE_120,
	FW_REPORT_RATE_180,
	FW_REPORT_HIGH_SENSITIVITY,
	FW_REPORT_NORMAL_SENSITIVITY,
	FW_TEL_CALL,
	FW_TEL_HANDUP,
	FW_HANDSFREE_ON,
	FW_HANDSFREE_OFF,
	FW_RESUME,
};

enum fw_edge_event_type {
	fw_dead_zone_event = 0,
	fw_edge_event,
	fw_corner_event,
};

enum GCORE_TS_STAT {
	TS_NORMAL = 0,
	TS_SUSPEND,
	TS_UPDATE,
	TS_MPTEST,
};
extern int  gc_factory_test(void);
struct gcore_dev {
	struct input_dev *input_device;
	struct task_struct *thread;
	u8 *touch_data;
#ifdef SEC_TSP_FACTORY_TEST
    struct sec_cmd_data sec;
#endif
#if defined(CONFIG_TOUCH_DRIVER_INTERFACE_I2C)
	struct i2c_client *bus_device;
#elif defined(CONFIG_TOUCH_DRIVER_INTERFACE_SPI)
	struct spi_device *bus_device;
#endif
	struct proc_dir_entry *tpd_proc_dir;
	unsigned int touch_irq;
	spinlock_t irq_flag_lock;
	int irq_flag;
	int tpd_flag;
	bool fw_update_state;
	int fw_packet_len;

	struct mutex transfer_lock;

	struct mutex ITOTEST_lock;
	wait_queue_head_t wait;

	int irq_gpio;
	int rst_gpio;
	void (*rst_output) (int rst, int level);
	void (*irq_enable) (struct gcore_dev *gdev);
	void (*irq_disable) (struct gcore_dev *gdev);

#ifdef CONFIG_ENABLE_FW_RAWDATA
	enum FW_MODE fw_mode;
	wait_queue_head_t usr_wait;
	bool usr_read;
	bool data_ready;
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
	bool TEL_State;
	struct input_dev *input_ps_device;
	bool PS_Enale;
#ifdef CONFIG_PM_WAKELOCKS
	struct wakeup_source *prx_wake_lock_ps;
#else
	struct wake_lock prx_wake_lock_ps;
#endif
#endif

	/* for driver request event and fw reply with interrupt */
	enum fw_event_type fw_event;
	u8 *firmware;
	int fw_xfer;

	struct workqueue_struct *fwu_workqueue;
	struct workqueue_struct *gtp_workqueue;
	struct delayed_work fwu_work;
#if GCORE_WDT_RECOVERY_ENABLE
	struct delayed_work wdt_work;
#endif
//+S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 save CB size
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
	struct delayed_work cb_work;
	int cb_count;
	bool CB_ckstat;
	u8 *CB_value;
#endif
//-S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 save CB size

//+S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 get charge status
#ifdef CONFIG_GCORE_PSY_NOTIFY
	struct notifier_block charger_notifier;
	struct delayed_work charger_work;
	bool charger_mode;
#endif
//+S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 get charge status

//tian add notify event 
	struct delayed_work event_work;
	bool tel_screen_off;
	struct delayed_work resume_notify_work;
	int ts_stat;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	bool gesture_wakeup_en;
#endif

#ifdef CONFIG_DRM
	struct notifier_block drm_notifier;
#elif defined(CONFIG_FB)
	struct notifier_block fb_notifier;
#endif

	u8 fw_ver_in_bin[4];
	u8 fw_ver_in_reg[4];

	s16 *diffdata;

	u8 *fw_mem;

	bool notify_state;
};
enum exp_fn {
	GCORE_FW_UPDATE = 0,
	GCORE_FS_INTERFACE,
	GCORE_MP_TEST,
};

struct gcore_exp_fn {
	enum exp_fn fn_type;
	bool wait_int;
	bool event_flag;
	int (*init) (struct gcore_dev *);
	void (*remove) (struct gcore_dev *);
	struct list_head link;
};

struct gcore_exp_fn_data {
	bool initialized;
	bool fn_inited;
	struct list_head list;
	struct gcore_dev *gdev;
};
/* +S96818AA1-1936,daijun1.wt,modify,2023/08/22,code optimization, adding TP registration conditions */
 struct tag_bootmode {
	u32 size;
	u32 tag;
	u32 bootmode;
	u32 boottype;
};
/* -S96818AA1-1936,daijun1.wt,modify,2023/08/22,code optimization, adding TP registration conditions */
/*
 * Declaration
 */

extern s32 gcore_bus_read(u8 *buffer, s32 len);
extern s32 gcore_bus_write(const u8 *buffer, s32 len);
extern int gcore_touch_bus_init(void);
extern void gcore_touch_bus_exit(void);
extern void gcore_new_function_register(struct gcore_exp_fn *exp_fn);
extern void gcore_new_function_unregister(struct gcore_exp_fn *exp_fn);
extern s32 gcore_spi_write_then_read(u8 *txbuf, s32 n_tx, u8 *rxbuf, s32 n_rx);

extern int gcore_read_fw_version(u8 *version, int length);
extern int gcore_read_fw_version_idm(u8 *version, int length);

extern int gcore_auto_update_hostdownload_proc(void *fw_buf);
extern int gcore_auto_update_flashdownload_proc(void *fw_buf);
extern int gcore_flashdownload_fspi_proc(void *fw_buf);
extern int gcore_flashdownload_proc(void *fw_buf);
extern int gcore_fw_mode_set_proc2(u8 mode);
extern s32 gcore_fw_read_reg(u32 addr, u8 *buffer, s32 len);
extern s32 gcore_fw_write_reg(u32 addr, u8 *buffer, s32 len);
extern int gcore_create_attribute(struct device *dev);
extern int gcore_idm_read_id(u8 *id, int length);
extern int gcore_update_hostdownload_idm2(u8 *fw_buf);
extern int gcore_upgrade_soft_reset(void);
extern int gcore_idm_tddi_reset(void);
extern int gcore_enter_idm_mode(void);
extern int gcore_exit_idm_mode(void);
extern s32 gcore_idm_read_reg(u32 addr, u8 *buffer, s32 len);
extern s32 gcore_idm_write_reg(u32 addr, u8 *buffer, s32 len);
extern s32 gcore_idm_read_page_reg(u8 page, u32 addr, u8 *buffer, s32 len);
extern s32 gcore_idm_write_page_reg(u8 page, u32 addr, u8 *buffer, s32 len);
extern s32 gcore_fw_read_rawdata(u8 *buffer, s32 len);
extern s32 gcore_fw_read_diffdata(u8 *buffer, s32 len);
extern void gcore_trigger_esd_recovery(void);
extern void add_CRC (u8 *fw_buf);
extern int gcore_fw_event_resume(void);
extern void gcore_resume_event_notify_work(struct work_struct *work);
#ifdef CONFIG_ENABLE_PROXIMITY_TP_SCREEN_OFF
extern int tpd_enable_ps(int enable);
#endif

//+S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 extern function
#ifdef CONFIG_SAVE_CB_CHECK_VALUE
extern void gcore_CB_value_download_work(struct work_struct *work);
#endif
//-S96818AA1-1936,wangtao14.wt,modify,2023/07/06,gc7272 extern function

extern struct gcore_exp_fn fw_update_fn;
extern struct gcore_exp_fn fs_interf_fn;
#if GCORE_MP_TEST_ON
extern struct gcore_exp_fn mp_test_fn;
#endif
extern struct gcore_exp_fn_data fn_data;

extern unsigned char gcore_mp_FW[];
extern bool demolog_enable;
extern int dump_demo_data_to_csv_file(const u8 *data, int rows, int cols);
extern int save_demo_data_to_file(void);

extern void gcore_request_firmware_update_work(struct work_struct *work);
extern void gcore_touch_release_all_point(struct input_dev *dev);
extern void gcore_touch_release_all_point255(struct input_dev *dev);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM
extern int gcore_sysfs_add_device(struct device *dev);
#endif

#ifdef CONFIG_DRM
extern int gcore_ts_drm_notifier_callback(struct notifier_block *self, \
											unsigned long event, void *data);
#endif
#ifdef TP_RESUME_BY_FB_NOTIFIER
extern int gcore_ts_fb_notifier_callback(struct notifier_block *self, \
											unsigned long event, void *data);
#endif

#ifdef CONFIG_GCORE_AUTO_UPDATE_FW_FLASHDOWNLOAD
extern int gcore_force_request_fireware_updata(char *fw_name,int fwname_len);
extern int force_erase_fash(void);
#endif

extern int gcore_start_mp_test(void);

#if GCORE_MP_TEST_ON
extern void mp_wait_int_set_fail(void);
#endif

extern void gcore_fw_event_notify_work(struct work_struct *work);

extern int gcore_fw_event_notify(enum fw_event_type event);

extern int check_notify_event(u8 rev_data,enum fw_event_type check_event);
//+S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 modify event cmd
extern void gcore_modify_fw_event_cmd(enum fw_event_type event);
extern void gcore_extern_notify_event(int event);
#if GCORE_WDT_RECOVERY_ENABLE
extern int gcore_esd_is_fail(void);
extern gcore_tp_esd_fail;
#endif
//-S96818AA1-1936,wangtao14.wt,add,2023/07/10,gc7272 modify event cmd
#endif /* GCORE_TPD_COMMON_H_  */
