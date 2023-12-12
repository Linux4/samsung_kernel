#ifndef _TOUCH_FEATURE_H_
#define _TOUCH_FEATURE_H_
#include <linux/input.h>
#include <linux/proc_fs.h>
#include <linux/printk.h>
#include <linux/power_supply.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
/*Tab A9 code for SR-AX6739A-01-611 by zhawei at 20230516 start*/
#include <linux/notifier.h>
/*Tab A9 code for SR-AX6739A-01-611 by zhawei at 20230516 end*/
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
#include <linux/of.h>
#include "ccci_modem.h"
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/

#include "sec_cmd.h"

#ifndef CONFIG_ENABLE_REPORT_LOG
#define CONFIG_ENABLE_REPORT_LOG 0
#endif
#define IC_NAME                  "ODM_PROJECT"
#define HWINFO_NAME              "tp_wake_switch"
#define PROC_TPINFO_FILE         "tp_info"
#define TPINFO_STR_LEN           50

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
    /*Tab A9 code for AX6739A-241 by suyurui at 20230530 start*/
    u_int32_t *fw_version;
    /*Tab A9 code for AX6739A-241 by suyurui at 20230530 end*/
    /*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 start*/
    struct sec_cmd_data sec;
    /*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 end*/

    void (*gesture_wakeup_enable)(void);
    void (*gesture_wakeup_disable)(void);
    int (*ito_test)(void);
};
/*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 start*/
extern bool g_tp_is_enabled;
bool get_tp_enable(void);
void tp_common_init(struct lcm_info *g_lcm_info);
int sec_fn_init(struct sec_cmd_data *sec);
void disable_gesture_wakeup(void);
void enable_gesture_wakeup(void);
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 start*/
void disable_high_sensitivity(void);
void enable_high_sensitivity(void);
/*Tab A9 code for SR-AX6739A-01-343 by wenghailong at 20230606 end*/
/*Tab A9 code for SR-AX6739A-01-260 by wenghailong at 20230513 end*/
extern struct common_function g_common_function;
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 start*/
int tp_detect_panel(const char *tp_ic);
const char* tp_choose_panel(void);
extern enum boot_mode_t tp_get_boot_mode(void);
/*Tab A9 code for AX6739A-241 by suyurui at 20230526 end*/
#endif
