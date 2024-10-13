#ifndef _TOUCH_FEATURE_H_
#define _TOUCH_FEATURE_H_
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/power_supply.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/notifier.h>
#include <linux/version.h>
#include "sec_cmd.h"
/*A06 code for SR-AL7160A-01-102  by yuli at 20240409 start*/
#include "hal_kpd.h"
/*A06 code for SR-AL7160A-01-102 by yuli at 20240409 end*/

#ifndef CONFIG_ENABLE_REPORT_LOG
#define CONFIG_ENABLE_REPORT_LOG 0
#endif
#define IC_NAME                  "ODM_PROJECT"
#define HWINFO_NAME              "tp_wake_switch"
#define PROC_TPINFO_FILE         "tp_info"
#define TPINFO_STR_LEN           50
#define PANEL_NAME_LEN           50

#define TP_ERROR(fmt, arg...)          pr_err("<%s-ERR>[%s:%d] "fmt"\n", \
                                                   IC_NAME, __func__, __LINE__, ##arg)
#define TP_INFO(fmt, arg...)                \
    do {                                    \
        pr_info("%s-INFO>[%s:%d]"fmt"\n", IC_NAME,__func__, __LINE__, ##arg);\
    } while (0)
#define TP_REPORT(fmt, arg...)                \
    do {                                    \
        if (CONFIG_ENABLE_REPORT_LOG)                        \
            pr_err("<%s-REP>[%s:%d]"fmt"\n", __func__, __LINE__, ##arg);\
    } while (0)

struct lcm_info{
    char* module_name;
    u8 *fw_version;
    struct sec_cmd_data sec;

    void (*gesture_wakeup_enable)(void);
    void (*gesture_wakeup_disable)(void);
    int (*ito_test)(void);
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
int tp_detect_panel(const char *tp_ic);
const char* tp_choose_panel(void);
extern enum boot_mode_t tp_get_boot_mode(void);
#endif
