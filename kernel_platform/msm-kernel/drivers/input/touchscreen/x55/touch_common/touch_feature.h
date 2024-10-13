#ifndef _TOUCH_FEATURE_H_
#define _TOUCH_FEATURE_H_
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/power_supply.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>

#include <linux/of.h>

#include "sec_cmd.h"

#ifndef CONFIG_ENABLE_REPORT_LOG
#define CONFIG_ENABLE_REPORT_LOG 0
#endif
#define IC_NAME                  "ODM_PROJECT"
#define HWINFO_NAME              "tp_wake_switch"
#define PROC_TPINFO_FILE         "tp_info"
#define TPINFO_STR_LEN           64

#define TP_ERROR(fmt, arg...)          printk("<%s-ERR>[%s:%d] "fmt"\n", \
                                                   IC_NAME, __func__, __LINE__, ##arg)
#define TP_INFO(fmt, arg...)                \
    do {                                    \
        printk("%s-INFO>[%s:%d]"fmt"\n", IC_NAME,__func__, __LINE__, ##arg);\
    } while (0)
#define TP_REPORT(fmt, arg...)                \
    do {                                    \
        if (CONFIG_ENABLE_REPORT_LOG)                        \
            printk("<%s-REP>[%s:%d]"fmt"\n", __func__, __LINE__, ##arg);\
    } while (0)

struct lcm_info{
    char* module_name;
    u8 *fw_version;
    struct sec_cmd_data sec;
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
    struct ts_data_info *ts_data_info;
    void (*fod_enable)(u8 value);
    void (*get_fod_info)(void);
    void (*get_fod_pos)(void);
    int (*set_fod_rect)(u16 fod_rect_data[4]);
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    int (*set_aod_rect)(u16 aod_rect_data[4]);
    /*M55 code for P231221-04897 by yuli at 20240106 end*/
    void (*single_tap_enable)(void);
    void (*single_tap_disable)(void);
    /*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
    void (*gesture_wakeup_enable)(void);
    void (*gesture_wakeup_disable)(void);
    int (*ito_test)(void);
    /*M55 code for QN6887A-3421 by yuli at 20231220 start*/
    void (*tp_resume_work)(void);
    /*M55 code for QN6887A-3421 by yuli at 20231220 end*/
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    void (*spay_enable)(void);
    void (*spay_disable)(void);
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
};

extern bool g_tp_is_enabled;
bool get_tp_enable(void);
void tp_common_init(struct lcm_info *g_lcm_info);
int sec_fn_init(struct sec_cmd_data *sec);
void disable_gesture_wakeup(void);
void enable_gesture_wakeup(void);
void disable_high_sensitivity(void);
void enable_high_sensitivity(void);
extern struct common_function g_common_function;
/*M55 code for SR-QN6887A-01-365 by wenghailong at 202300821 start*/
bool tp_get_boot_mode(void);
/*M55 code for SR-QN6887A-01-365 by wenghailong at 202300821 end*/
/*M55 code for QN6887A-45 by yubo at 20230911 start*/
bool tp_get_pcbainfo(const char *pcbainfo);
/*M55 code for QN6887A-45 by yubo at 20230911 end*/
#endif
