/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, Focaltech Ltd. All rights reserved.
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
#include <asm/uaccess.h>
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
#include <linux/gunyah/gh_irq_lend.h>
#include <linux/gunyah/gh_mem_notifier.h>
#include "focaltech_common.h"
#include "touch_feature.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_MAX_POINTS_SUPPORT              10 /* constant value, can't be changed */
#define FTS_MAX_KEYS                        4
#define FTS_KEY_DIM                         10
#define FTS_ONE_TCH_LEN_V2                  8
#define FTS_ONE_TCH_LEN                     6
#define FTS_TOUCH_DATA_LEN  (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN + 3)
/*M55 code for QN6887A-371 by yuli at 20231026 start*/
#define FTS_TOUCH_DATA_LEN_V2               (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN_V2 + 4 + 2)
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
#if FTS_SAMSUNG_FOD_EN
#define FTS_FOD_REPORT_DATA_LEN             9
#undef FTS_TOUCH_DATA_LEN_V2
#define FTS_TOUCH_DATA_LEN_V2               (FTS_MAX_POINTS_SUPPORT * FTS_ONE_TCH_LEN_V2 + 4 + 2 + FTS_FOD_REPORT_DATA_LEN)
#endif // FTS_SAMSUNG_FOD_EN
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
/*M55 code for QN6887A-371 by yuli at 20231026 end*/
#define FTS_GESTURE_POINTS_MAX              6
#define FTS_GESTURE_DATA_LEN               (FTS_GESTURE_POINTS_MAX * 4 + 4)

#define FTS_MAX_ID                          0x0A
#define FTS_TOUCH_X_H_POS                   3
#define FTS_TOUCH_X_L_POS                   4
#define FTS_TOUCH_Y_H_POS                   5
#define FTS_TOUCH_Y_L_POS                   6
#define FTS_TOUCH_PRE_POS                   7
#define FTS_TOUCH_AREA_POS                  8
#define FTS_TOUCH_OFF_MINOR                 9
/*M55 code for QN6887A-150 by yuli at 20231012 start*/
#define FTS_TOUCH_OFF_MISC                  10
/*M55 code for QN6887A-150 by yuli at 20231012 end*/

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


#define FTS_TOUCH_HIRES_X                   10

#define FTS_HI_RES_X_MAX                    16

/*****************************************************************************
*  Alternative mode (When something goes wrong, the modules may be able to solve the problem.)
*****************************************************************************/
/*
 * For commnication error in PM(deep sleep) state
 */
/*M55 code for QN6887A-576 by yuli at 20231102 start*/
#define FTS_PATCH_COMERR_PM                     1
/*M55 code for QN6887A-576 by yuli at 20231102 end*/
#define FTS_TIMEOUT_COMERR_PM                   700
/*M55 code for P240111-04942 by hehaoran at 20240118 start*/
#define FTS_TIMEOUT_COMERR_TRUSTED_TOUCH        20
/*M55 code for P240111-04942 by hehaoran at 20240118 end*/
#define EVB0                           "EVB0"
#define EVB1                           "EVB1"

/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 start*/
typedef struct {
    const char *panel_name;
    int fw_num;
} fts_upgrade_module;
/*M55 code for SR-QN6887A-01-391 by hehaoran at 20231008 end*/
enum FTS_TP_MODEL {
    MODEL_DEFAULT = 0,
    MODEL_ICNA3512_FT3683G_VS,
    MODEL_ICNA3512_FT3683G_TM,
};

enum FTS_CONST_TYPE {
    ITO_PASS = 0,
    ITO_FAIL = 1,
    TIME_COEFFICIENT = 60,
    SYSTEM_TIME = 1900,
    TIME_BUFF_LEN = 64,
};
/*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/
struct ftxxxx_proc {
    struct proc_dir_entry *proc_entry;
    u8 opmode;
    u8 cmd_len;
    u8 cmd[FTS_MAX_COMMMAND_LENGTH];
};

struct fts_ts_platform_data {
    u32 type;
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
    int minor;
};


enum trusted_touch_mode_config {
    TRUSTED_TOUCH_VM_MODE,
    TRUSTED_TOUCH_MODE_NONE
};

enum trusted_touch_pvm_states {
    TRUSTED_TOUCH_PVM_INIT,
    PVM_I2C_RESOURCE_ACQUIRED,
    PVM_INTERRUPT_DISABLED,
    PVM_IOMEM_LENT,
    PVM_IOMEM_LENT_NOTIFIED,
    PVM_IRQ_LENT,
    PVM_IRQ_LENT_NOTIFIED,
    PVM_IOMEM_RELEASE_NOTIFIED,
    PVM_IRQ_RELEASE_NOTIFIED,
    PVM_ALL_RESOURCES_RELEASE_NOTIFIED,
    PVM_IRQ_RECLAIMED,
    PVM_IOMEM_RECLAIMED,
    PVM_INTERRUPT_ENABLED,
    PVM_I2C_RESOURCE_RELEASED,
    TRUSTED_TOUCH_PVM_STATE_MAX
};

enum trusted_touch_tvm_states {
    TRUSTED_TOUCH_TVM_INIT,
    TVM_IOMEM_LENT_NOTIFIED,
    TVM_IRQ_LENT_NOTIFIED,
    TVM_ALL_RESOURCES_LENT_NOTIFIED,
    TVM_IOMEM_ACCEPTED,
    TVM_I2C_SESSION_ACQUIRED,
    TVM_IRQ_ACCEPTED,
    TVM_INTERRUPT_ENABLED,
    TVM_INTERRUPT_DISABLED,
    TVM_IRQ_RELEASED,
    TVM_I2C_SESSION_RELEASED,
    TVM_IOMEM_RELEASED,
    TRUSTED_TOUCH_TVM_STATE_MAX
};

#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
#define TRUSTED_TOUCH_MEM_LABEL 0x7

#define TOUCH_RESET_GPIO_SIZE 0x1000
#define TOUCH_RESET_GPIO_OFFSET 0x4
#define TOUCH_INTR_GPIO_SIZE 0x1000
#define TOUCH_INTR_GPIO_OFFSET 0x8

#define TRUSTED_TOUCH_EVENT_LEND_FAILURE -1
#define TRUSTED_TOUCH_EVENT_LEND_NOTIFICATION_FAILURE -2
#define TRUSTED_TOUCH_EVENT_ACCEPT_FAILURE -3
#define    TRUSTED_TOUCH_EVENT_FUNCTIONAL_FAILURE -4
#define    TRUSTED_TOUCH_EVENT_RELEASE_FAILURE -5
#define    TRUSTED_TOUCH_EVENT_RECLAIM_FAILURE -6
#define    TRUSTED_TOUCH_EVENT_I2C_FAILURE -7
#define    TRUSTED_TOUCH_EVENT_NOTIFICATIONS_PENDING 5

struct trusted_touch_vm_info {
    enum gh_irq_label irq_label;
    enum gh_mem_notifier_tag mem_tag;
    enum gh_vm_names vm_name;
    const char *trusted_touch_type;
    u32 hw_irq;
    gh_memparcel_handle_t vm_mem_handle;
    u32 *iomem_bases;
    u32 *iomem_sizes;
    u32 iomem_list_size;
    void *mem_cookie;
    atomic_t vm_state;
#ifdef CONFIG_ARCH_QTI_VM
    u32 reset_gpio_base;
    u32 intr_gpio_base;
#endif
};
#endif

struct fts_ts_data {
    struct i2c_client *client;
    struct spi_device *spi;
    struct device *dev;
    struct input_dev *input_dev;
    struct fts_ts_platform_data *pdata;
    struct ts_ic_info ic_info;
    struct workqueue_struct *ts_workqueue;
    struct work_struct fwupg_work;
    struct delayed_work esdcheck_work;
    struct delayed_work prc_work;
    struct work_struct resume_work;
    struct work_struct suspend_work;
    struct ftxxxx_proc proc;
    spinlock_t irq_lock;
    struct mutex report_mutex;
    struct mutex bus_lock;
    struct mutex transition_lock;
    int irq;
    int log_level;
    int fw_is_running;      /* confirm fw is running when using spi:default 0 */
    int dummy_byte;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
    struct completion pm_completion;
    bool pm_suspend;
#endif
    /*M55 code for QN6887A-186 by hehaoran at 20231015 start*/
    atomic_t ito_testing;
    /*M55 code for QN6887A-186 by hehaoran at 20231015 end*/
    bool suspended;
    /*M55 code for QN6887A-4750 by yuli at 20240112 start*/
    bool gesture_suspend;
    /*M55 code for QN6887A-4750 by yuli at 20240112 end*/
    bool fw_loading;
    bool irq_disabled;
    bool power_disabled;
    bool glove_mode;
    bool cover_mode;
    bool charger_mode;
    bool gesture_mode;      /* gesture enable or disable, default: disable */
    int report_rate;
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    bool double_tap;
    bool single_tap;
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    bool spay_en;
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    #if FTS_SAMSUNG_FOD_EN
    /*M55 code for QN6887A-282 by yuli at 20231021 start*/
    u8 fod_enable;
    /*M55 code for QN6887A-282 by yuli at 20231021 end*/
    int fod_report_buf_size;
    u8 *fod_report_buf;
    struct ts_data_info ts_data_info;
    #endif // FTS_SAMSUNG_FOD_EN
    #if FTS_SAMSUNG_SCREEN_SHOT_EN
    int palm_flag; /* Palm flag: 0 -- NA; 1 -- Palm Touch or Palm Screen Shot */
    #endif // FTS_SAMSUNG_SCREEN_SHOT_EN
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
    /* multi-touch */
    struct ts_event *events;
    u8 *bus_tx_buf;
    u8 *bus_rx_buf;
    int bus_type;
    int bus_ver;
    u8 *point_buf;
    void *notifier_cookie;
    int pnt_buf_size;
    int touchs;
    int key_state;
    int touch_point;
    int point_num;
    struct regulator *vdd;
    struct regulator *vcc_i2c;
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

    const char *touch_environment;
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
    struct trusted_touch_vm_info *vm_info;
    struct mutex fts_clk_io_ctrl_mutex;
    struct completion trusted_touch_powerdown;
    struct clk *core_clk;
    struct clk *iface_clk;
    atomic_t trusted_touch_initialized;
    atomic_t trusted_touch_enabled;
    atomic_t trusted_touch_transition;
    atomic_t trusted_touch_event;
    atomic_t trusted_touch_abort_status;
    atomic_t trusted_touch_mode;
#endif
    atomic_t delayed_vm_probe_pending;
    struct sec_cmd_data sec;
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 start*/
    const char *fw_name;
    const char *ito_file;
    char *module_name;
    u8 firmware_ver;
    /*M55 code for SR-QN6887A-01-352 by yuli at 20230923 end*/
    /*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
    struct notifier_block tp_usb_notify;
    struct work_struct tp_usb_work_queue;
    /*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/
};

enum _FTS_BUS_TYPE {
    BUS_TYPE_NONE,
    BUS_TYPE_I2C,
    BUS_TYPE_SPI,
    BUS_TYPE_SPI_V2,
};

enum _FTS_BUS_VER {
    BUS_VER_DEFAULT = 1,
    BUS_VER_V2,
};

enum _FTS_TOUCH_ETYPE {
    TOUCH_DEFAULT = 0x00,
    TOUCH_PROTOCOL_v2 = 0x02,
    TOUCH_EXTRA_MSG = 0x08,
    TOUCH_PEN = 0x0B,
    TOUCH_GESTURE = 0x80,
    TOUCH_FW_INIT = 0x81,
    TOUCH_DEFAULT_HI_RES = 0x82,
    TOUCH_IGNORE = 0xFE,
    TOUCH_ERROR = 0xFF,
};

/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 start*/
enum _ex_mode {
    MODE_GLOVE = 0,
    MODE_COVER,
    MODE_CHARGER,
    REPORT_RATE,
};

extern int fts_ex_mode_switch(enum _ex_mode mode, u8 value);
/*M55 code for SR-QN6887A-01-356 by tangzhen at 20231007 end*/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
extern struct fts_ts_data *fts_data;

/* communication interface */
int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen);
int fts_read_reg(u8 addr, u8 *value);
int fts_write(u8 *writebuf, u32 writelen);
int fts_write_reg(u8 addr, u8 value);
void fts_hid2std(void);
int fts_bus_init(struct fts_ts_data *ts_data);
int fts_bus_exit(struct fts_ts_data *ts_data);

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
int fts_enter_test_environment(bool test_state);

/* Other */
int fts_reset_proc(int hdelayms);
int fts_wait_tp_to_valid(void);
void fts_release_all_finger(void);
void fts_tp_state_recovery(struct fts_ts_data *ts_data);
int fts_ex_mode_init(struct fts_ts_data *ts_data);
int fts_ex_mode_exit(struct fts_ts_data *ts_data);
int fts_ex_mode_recovery(struct fts_ts_data *ts_data);

void fts_irq_disable(void);
void fts_irq_enable(void);
int fts_ts_handle_trusted_touch_pvm(struct fts_ts_data *ts_data, int value);
int fts_ts_handle_trusted_touch_tvm(struct fts_ts_data *ts_data, int value);
bool tp_get_cmdline(const char *name);
int fts_ito_test(void);
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
#if FTS_SAMSUNG_FOD_EN
int fts_fod_enable(u8 value);
int fts_read_fod_info(u8 *data);
int fts_read_fod_pos(u8 *data);
int fts_set_fod_rect(u8 *data);
#endif // FTS_SAMSUNG_FOD_EN
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
/*M55 code for P231221-04897 by yuli at 20240106 start*/
int fts_set_aod_rect(u8 *data);
int tp_set_aod_rect(u16 aod_rect_data[4]);
/*M55 code for P231221-04897 by yuli at 20240106 end*/
#ifdef CONFIG_FTS_I2C_TRUSTED_TOUCH
#ifdef CONFIG_ARCH_QTI_VM
void fts_ts_trusted_touch_tvm_i2c_failure_report(struct fts_ts_data *fts_data);
#endif
#endif
#endif /* __LINUX_FOCALTECH_CORE_H__ */
