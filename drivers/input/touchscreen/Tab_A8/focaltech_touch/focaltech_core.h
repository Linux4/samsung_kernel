/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
/*****************************************************************************
*
* File Name: focaltech_core.h

* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

#ifndef __LINUX_FOCALTECH_CORE_H__
#define __LINUX_FOCALTECH_CORE_H__
/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/uaccess.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/dma-mapping.h>

#include <linux/touchscreen_info.h>

#include "focaltech_common.h"
/* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 start */
#include <linux/input/sec_cmd.h>
/* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 start */

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT              10 /* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_DIM                         10
#define FTS_ONE_TCH_LEN                     6
#define FTS_TOUCH_DATA_LEN  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)

#define FTS_GESTURE_POINTS_MAX              6
#define FTS_GESTURE_DATA_LEN               (FTS_GESTURE_POINTS_MAX * 4 + 4)

#define FTS_MAX_ID                          0x0A
#define FTS_TOUCH_X_H_POS                   3
#define FTS_TOUCH_X_L_POS                   4
#define FTS_TOUCH_Y_H_POS                   5
#define FTS_TOUCH_Y_L_POS                   6
#define FTS_TOUCH_PRE_POS                   7
#define FTS_TOUCH_AREA_POS                  8
#define FTS_TOUCH_POINT_NUM                 2
#define FTS_TOUCH_EVENT_POS                 3
#define FTS_TOUCH_ID_POS                    5
#define FTS_COORDS_ARR_SIZE                 4
#define FTS_X_MIN_DISPLAY_DEFAULT           0
#define FTS_Y_MIN_DISPLAY_DEFAULT           0
#define FTS_X_MAX_DISPLAY_DEFAULT           720
#define FTS_Y_MAX_DISPLAY_DEFAULT           1280

#define FTS_TOUCH_DOWN                      0
#define FTS_TOUCH_UP                        1
#define FTS_TOUCH_CONTACT                   2
#define EVENT_DOWN(flag)                    ((FTS_TOUCH_DOWN == flag) || (FTS_TOUCH_CONTACT == flag))
#define EVENT_UP(flag)                      (FTS_TOUCH_UP == flag)
#define EVENT_NO_DOWN(data)                 (!data->point_num)

#define FTS_MAX_COMPATIBLE_TYPE             4
#define FTS_MAX_COMMMAND_LENGTH             16


/*****************************************************************************
*  Alternative mode (When something goes wrong, the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * For commnication error in PM(deep sleep) state
 */
/*Tab A8 code for AX6300DEV-3701 by yuli at 2021/12/10 start*/
#define FTS_PATCH_COMERR_PM                     1
/*Tab A8 code for AX6300DEV-3701 by yuli at 2021/12/10 end*/
#define FTS_TIMEOUT_COMERR_PM                   700

#define FTS_HIGH_REPORT                         0
#define FTS_SIZE_DEFAULT                        15

/*Tab A8 code for SR-AX6300-01-450 by yuli at 2021/11/20 start*/
#define FTS_TOUCH_EXT_PROC                       1
/*Tab A8 code for SR-AX6300-01-450 by yuli at 2021/11/20 end*/

/*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 start*/
#define HEADSET_PLUGOUT_STATE               0
#define HEADSET_PLUGIN_STATE                1

extern int headset_notifier_register(struct notifier_block *nb);
extern int headset_notifier_unregister(struct notifier_block *nb);
/*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 end*/

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
struct ftxxxx_proc {
    struct proc_dir_entry *proc_entry;
    u8 opmode;
    u8 cmd_len;
    u8 cmd[FTS_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
    u32 irq_gpio;
    u32 irq_gpio_flags;
    u32 reset_gpio;
    u32 reset_gpio_flags;
    bool have_key;
    u32 key_number;
    u32 keys[FTS_MAX_KEYS];
    u32 key_y_coords[FTS_MAX_KEYS];
    u32 key_x_coords[FTS_MAX_KEYS];
    u32 x_max;
    u32 y_max;
    u32 x_min;
    u32 y_min;
    u32 max_touch_number;
};

struct ts_event {
    int x;      /*x coordinate */
    int y;      /*y coordinate */
    int p;      /* pressure */
    int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
    int id;     /*touch ID */
    int area;
};

struct pen_event {
    int inrange;
    int tip;
    int x;      /*x coordinate */
    int y;      /*y coordinate */
    int p;      /* pressure */
    int flag;   /* touch event flag: 0 -- down; 1-- up; 2 -- contact */
    int id;     /*touch ID */
    int tilt_x;
    int tilt_y;
    int tool_type;
};

/*Tab A8 code for AX6300DEV-3764|AX6300DEV-3763 by yuli at 2021/12/18 start*/
enum PALM_FLAG {
    PALM_TOUCH = 1,
    PALM_UNKNOWN = 2,
    PALM_HANDKNIFE = 3,
};
/*Tab A8 code for AX6300DEV-3764|AX6300DEV-3763 by yuli at 2021/12/18 end*/

struct fts_ts_data {
    struct i2c_client *client;
    struct spi_device *spi;
    struct device *dev;
    struct input_dev *input_dev;
    struct input_dev *pen_dev;
    struct fts_ts_platform_data *pdata;
    struct ts_ic_info ic_info;
    struct workqueue_struct *ts_workqueue;
    struct work_struct fwupg_work;
    struct delayed_work esdcheck_work;
    struct delayed_work prc_work;
    struct work_struct resume_work;
    struct ftxxxx_proc proc;
    spinlock_t irq_lock;
    struct mutex report_mutex;
    struct mutex bus_lock;
    unsigned long intr_jiffies;
    int irq;
    int log_level;
    int fw_is_running;      /* confirm fw is running when using spi:default 0 */
    int dummy_byte;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
    struct completion pm_completion;
    bool pm_suspend;
#endif
    bool suspended;
    bool fw_loading;
    bool irq_disabled;
    bool power_disabled;
    bool glove_mode;
    bool cover_mode;
    bool charger_mode;
    bool gesture_mode;      /* gesture enable or disable, default: disable */
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 start*/
    bool earphone_mode;
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 end*/
    bool prc_mode;
    struct pen_event pevent;
    /* multi-touch */
    struct ts_event *events;
    u8 *bus_tx_buf;
    u8 *bus_rx_buf;
    int bus_type;
    u8 *point_buf;
    int pnt_buf_size;
    int touchs;
    int key_state;
    int touch_point;
    int point_num;
    /*Tab A8 code for AX6300DEV-3608 by yuli at 2021/12/2 start*/
    enum PALM_FLAG palm_flag; /* Palm flag: 0 -- NA; 1 -- Palm Touch; 2 -- unknown; 3 -- Palm Screen Shot */
    /*Tab A8 code for AX6300DEV-3608 by yuli at 2021/12/2 end*/
    struct regulator *vdd;
    struct regulator *vcc_i2c;
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 start*/
    struct notifier_block earphone_notif;
    struct workqueue_struct *fts_earphone_notify_wq;
    struct work_struct earphone_notify_work;
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 end*/
    /* Tab A8 code for SR-AX6300-01-453 by fengzhigang at 2021/11/23 start */
    struct notifier_block charger_notif;    //S2,X660 Charge flag
    struct workqueue_struct *fts_charger_notify_wq;
    struct work_struct charger_notify_work;
    bool fw_loaded_ok;
    /* Tab A8 code for SR-AX6300-01-453 by fengzhigang at 2021/11/23 end */
#if FTS_PINCTRL_EN
    struct pinctrl *pinctrl;
    struct pinctrl_state *pins_active;
    struct pinctrl_state *pins_suspend;
    struct pinctrl_state *pins_release;
#endif
#if defined(CONFIG_FB) || defined(CONFIG_DRM)
    struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
    struct early_suspend early_suspend;
#endif

    /* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 start */
    struct sec_cmd_data sec;
    int fw_in_binary;
    int fw_in_ic;
    /* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 end */

    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 start */
    bool tp_is_enabled;
    /* Tab A8 code for AX6300DEV-3297 by yuli at 2021/12/6 end */
};

enum _FTS_BUS_TYPE {
    BUS_TYPE_NONE,
    BUS_TYPE_I2C,
    BUS_TYPE_SPI,
    BUS_TYPE_SPI_V2,
};

/* Tab A8 code for SR-AX6300-01-453 by fengzhigang at 2021/11/23 start */
enum _ex_mode {
    MODE_GLOVE = 0,
    MODE_COVER,
    MODE_CHARGER,
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 start*/
    MODE_EARPHONE,
    /*Tab A8 code for AX6300DEV-3887 by yuli at 2021/12/28 end*/
};

extern int fts_ex_mode_switch(enum _ex_mode mode, u8 value);
/* Tab A8 code for SR-AX6300-01-453 by fengzhigang at 2021/11/23 end */

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct fts_ts_data *fts_data;
/*Tab A8 code for SR-AX6300-01-450 by yuli at 2021/11/20 start*/
extern struct fts_upgrade *fwupgrade;
#include "focaltech_flash.h"
/*Tab A8 code for SR-AX6300-01-450 by yuli at 2021/11/20 end*/

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);
int fts_spi_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen);

/* Gesture functions */
int fts_gesture_init(struct fts_ts_data *ts_data);
int fts_gesture_exit(struct fts_ts_data *ts_data);
void fts_gesture_recovery(struct fts_ts_data *ts_data);
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data);
int fts_gesture_suspend(struct fts_ts_data *ts_data);
int fts_gesture_resume(struct fts_ts_data *ts_data);

/* Apk and functions */
int fts_create_apk_debug_channel(struct fts_ts_data *);
void fts_release_apk_debug_channel(struct fts_ts_data *);

/* ADB functions */
int fts_create_sysfs(struct fts_ts_data *ts_data);
int fts_remove_sysfs(struct fts_ts_data *ts_data);

/* ESD */
#if FTS_ESDCHECK_EN
int fts_esdcheck_init(struct fts_ts_data *ts_data);
int fts_esdcheck_exit(struct fts_ts_data *ts_data);
int fts_esdcheck_switch(bool enable);
int fts_esdcheck_proc_busy(bool proc_debug);
int fts_esdcheck_set_intr(bool intr);
int fts_esdcheck_suspend(void);
int fts_esdcheck_resume(void);
#endif

/* Production test */
#if FTS_TEST_EN
int fts_test_init(struct fts_ts_data *ts_data);
int fts_test_exit(struct fts_ts_data *ts_data);
#endif

/* Point Report Check*/
#if FTS_POINT_REPORT_CHECK_EN
int fts_point_report_check_init(struct fts_ts_data *ts_data);
int fts_point_report_check_exit(struct fts_ts_data *ts_data);
void fts_prc_queue_work(struct fts_ts_data *ts_data);
#endif

/* FW upgrade */
int fts_fwupg_init(struct fts_ts_data *ts_data);
int fts_fwupg_exit(struct fts_ts_data *ts_data);
int fts_fw_resume(bool need_reset);
int fts_fw_recovery(void);
int fts_upgrade_bin(char *fw_name, bool force);
int fts_enter_test_environment(bool test_state);

/* Other */
int fts_reset_proc(int hdelayms);
int fts_check_cid(struct fts_ts_data *ts_data, u8 id_h);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);
int fts_ex_mode_recovery(struct fts_ts_data *ts_data);

void fts_irq_disable(void);
void fts_irq_enable(void);

/* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 start */
extern struct sec_cmd fts_commands[];
extern int fts_get_array_size(void);
int fts_ts_sec_fn_init(struct fts_ts_data *ts);
void fts_ts_sec_fn_remove(struct fts_ts_data *ts);
extern int tp_gesture;
/* Tab A8 code for SR-AX6300-01-451 by gaozhengwei at 2021/11/22 end */

#endif /* __LINUX_FOCALTECH_CORE_H__ */

extern enum tp_module_used tp_is_used;
