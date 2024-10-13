#ifndef _SEC_CMD_H_
#define _SEC_CMD_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/stat.h>
#include <linux/err.h>
#include <linux/input.h>
#include <linux/sched/clock.h>
#include <linux/uaccess.h>
#include <linux/kfifo.h>
/*M55 code for P230918-03913 by suyurui at 20230923 start*/
#include <linux/sec_class.h>
/*M55 code for P230918-03913 by suyurui at 20230923 end*/
#define SECLOG  "[sec_input]"
#define SEC_CLASS_DEVT_TSP        10
#define SEC_CLASS_DEV_NAME_TSP        "tsp"

#define BTN_PALM                  0x118

#define INPUT_FEATURE_ENABLE_SETTINGS_AOT    (1 << 0) /* Double tap wakeup settings */
#define SEC_CMD(name, func)        .cmd_name = name, .cmd_func = func
#define SEC_CMD_H(name, func)        .cmd_name = name, .cmd_func = func, .cmd_log = 1
#define SEC_CMD_BUF_SIZE        (4096 - 1)
#define SEC_CMD_STR_LEN            256
#define SEC_CMD_RESULT_STR_LEN        (4096 - 1)
#define SEC_CMD_RESULT_STR_LEN_EXPAND    (SEC_CMD_RESULT_STR_LEN * 6)
#define SEC_CMD_PARAM_NUM        8

/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 start*/
#define KEY_BLACK_UI_GESTURE    0x1c7
typedef enum {
    /*M55 code for P240111-04531 by yuli at 20240116 start*/
    SPONGE_EVENT_TYPE_SPAY = 0x04,
    /*M55 code for P240111-04531 by yuli at 20240116 end*/
    SPONGE_EVENT_TYPE_SINGLE_TAP        = 0x08,
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    SPONGE_EVENT_TYPE_AOD_DOUBLETAB = 0x0B,
    /*M55 code for P231221-04897 by yuli at 20240106 end*/
    SPONGE_EVENT_TYPE_FOD_PRESS        = 0x0F,
    SPONGE_EVENT_TYPE_FOD_RELEASE        = 0x10,
    SPONGE_EVENT_TYPE_FOD_OUT        = 0x11,
    SPONGE_EVENT_TYPE_LONG_PRESS        = 0x12,
} SPONGE_EVENT_TYPE;

struct ts_data_info {
    unsigned int scrub_id;
    unsigned int scrub_x;
    unsigned int scrub_y;
    u8 fod_pos[SEC_CMD_STR_LEN];
    u8 fod_info_tx;
    u8 fod_info_rx;
    u8 fod_info_bytes;
    u16 fod_rect_data[4]; // 0: left, 1: top, 2: right, 3: bottom
    /*M55 code for P231221-04897 by yuli at 20240106 start*/
    u16 aod_rect_data[4]; // 0: width, 1: height, 2: x, 3: y
    /*M55 code for P231221-04897 by yuli at 20240106 end*/
};
/*M55 code for SR-QN6887A-01-370 by yuli at 20231009 end*/
struct sec_cmd {
    struct list_head    list;
    const char        *cmd_name;
    void            (*cmd_func)(void *device_data);
    int            cmd_log;
};

enum SEC_CMD_STATUS {
    SEC_CMD_STATUS_WAITING = 0,
    SEC_CMD_STATUS_RUNNING,
    SEC_CMD_STATUS_OK,
    SEC_CMD_STATUS_FAIL,
    SEC_CMD_STATUS_EXPAND,
    SEC_CMD_STATUS_NOT_APPLICABLE,
};

#define SEC_CMD_MAX_QUEUE    10

struct command {
    char    cmd[SEC_CMD_STR_LEN];
};

struct sec_cmd_data {
    struct device        *fac_dev;
    struct list_head    cmd_list_head;
    u8            cmd_state;
    char            cmd[SEC_CMD_STR_LEN];
    int            cmd_param[SEC_CMD_PARAM_NUM];
    char            *cmd_result;
    int            cmd_result_expand;
    int            cmd_result_expand_count;
    int            cmd_buffer_size;
    volatile bool        cmd_is_running;
    struct mutex        cmd_lock;
    struct mutex        fs_lock;
    struct kfifo        cmd_queue;
    struct mutex        fifo_lock;
    struct delayed_work    cmd_work;
    struct mutex        wait_lock;
    struct completion    cmd_result_done;
    bool            wait_cmd_result_done;
    int item_count;
    char cmd_result_all[SEC_CMD_RESULT_STR_LEN];
    u8 cmd_all_factory_state;
};

extern void sec_cmd_set_cmd_exit(struct sec_cmd_data *data);
extern void sec_cmd_set_default_result(struct sec_cmd_data *data);
extern void sec_cmd_set_cmd_result(struct sec_cmd_data *data, char *buff, int len);
extern void sec_cmd_set_cmd_result_all(struct sec_cmd_data *data, char *buff, int len, char *item);
extern int sec_cmd_init(struct sec_cmd_data *data, struct sec_cmd *cmds, int len, int devt);
extern void sec_cmd_exit(struct sec_cmd_data *data, int devt);
#endif /* _SEC_CMD_H_ */
