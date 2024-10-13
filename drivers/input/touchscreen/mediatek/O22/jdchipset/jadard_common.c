#include "jadard_platform.h"
#include "jadard_common.h"
#include "jadard_module.h"

#if defined(__JADARD_KMODULE__)
struct filename* (*jd_getname_kernel)(const char *filename);
struct file* (*jd_file_open_name)(struct filename *name, int flags, umode_t mode);
#endif

struct jadard_module_fp g_module_fp;
struct jadard_ts_data *pjadard_ts_data = NULL;
struct jadard_ic_data *pjadard_ic_data = NULL;
struct jadard_report_data *pjadard_report_data = NULL;
struct jadard_host_data *pjadard_host_data = NULL;
struct jadard_debug *pjadard_debug = NULL;
struct proc_dir_entry *pjadard_touch_proc_dir = NULL;

#if defined(JD_OPPO_FUNC)
#define JADARD_PROC_TOUCHPANLE_FOLDER       "touchpanel"
#define JADARD_PROC_BASELINE_TEST_FILE       "baseline_test"
#define JADARD_PROC_BLACK_SCREEN_TEST_FILE "black_screen_test"

struct proc_dir_entry *pjadard_touchpanel_proc_dir = NULL;
struct proc_dir_entry *pjadard_baseline_test_file = NULL;
struct proc_dir_entry *pjadard_blackscreen_baseline_file = NULL;
#endif

char *jd_panel_maker_list[JD_PANEL_MAKER_LIST_SIZE] = {
    "AUO",      /* 友達     */
    "BOE",      /* 京東方   */
    "CPT",      /* 華映     */
    "CSOT",     /* 華星     */
    "CTC",      /* 深超     */
    "CTO",      /* 華銳     */
    "HSD",      /* 瀚宇彩晶 */
    "INX",      /* 群創     */
    "IVO",      /* 龍騰     */
    "JDI",      /* 日本顯示 */
    "LGD",      /* 樂金顯示 */
    "MDT",      /* 華佳彩   */
    "PANDA",    /* 中電熊貓 */
    "SDC",      /* 三星顯示 */
    "SHARP",    /* 夏普     */
    "TM",       /* 天馬     */
    "TRULY",    /* 信利     */
    "HKC",      /* 惠科     */
    "N/A"
};

#if defined(JD_AUTO_UPGRADE_FW) || defined(JD_ZERO_FLASH)
char *jd_i_CTPM_firmware_name = "jd9522t_xx_boe_firmware.bin";
#if defined(JD_AUTO_UPGRADE_FW)
uint8_t *g_jadard_fw = NULL;
uint32_t g_jadard_fw_len;
uint32_t g_jadard_fw_ver = 0;
uint32_t g_jadard_fw_cid_ver = 0;
#endif
#endif

#ifdef JD_USB_DETECT_GLOBAL
/* Get usb status from system */
bool jd_usb_detect_flag;
#if defined(__JADARD_KMODULE__)
EXPORT_SYMBOL(jd_usb_detect_flag);
#endif
#endif

#ifdef JD_SMART_WAKEUP
/* {Double-Click, Up, Down, Left, Rright, C, Z, M,
 *    O, S, V, DV, LV, RV, W, e,
 *    AT, RF, LF, Two fingers Up, Two fingers Down, Two fingers Left, Two fingers Right}
 */
uint8_t jd_gest_event[JD_GEST_SUP_NUM] = {
    0x80, 0x01, 0x02, 0x03, 0x04, 0x20, 0x21, 0x22,
    0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A,
    0x2B, 0x2C, 0x2D, 0x51, 0x52, 0x53, 0x54
};
/*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 start*/
uint16_t jd_gest_key_def[JD_GEST_SUP_NUM] = {
    KEY_WAKEUP, 251, 252, 253, 254, 255, 256, 257,
    258, 259, 260, 261, 262, 263, 264, 265,
    266, 267, 268, 269, 270, 271, 272
};
/*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 end*/
bool proc_smwp_send_flag = false;
#define JADARD_PROC_SMWP_FILE "SMWP"
struct proc_dir_entry *jadard_proc_SMWP_file = NULL;
#define JADARD_PROC_GESTURE_FILE "GESTURE"
struct proc_dir_entry *jadard_proc_GESTURE_file = NULL;
#endif

#ifdef JD_HIGH_SENSITIVITY
bool proc_Hsens_send_flag = false;
#define JADARD_PROC_HIGH_SENSITIVITY_FILE "SENSITIVITY"
struct proc_dir_entry *jadard_proc_high_sensitivity_file = NULL;
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
#define JADARD_PROC_SORTING_TEST_FILE  "sorting_test"
struct proc_dir_entry *jadard_proc_sorting_test_file;
#endif

#ifdef JD_ROTATE_BORDER
bool proc_rotate_border_flag = false;
#define JADARD_PROC_ROTATE_BORDER_FILE "BORDER"
struct proc_dir_entry *jadard_proc_rotate_border_file = NULL;
#endif

#ifdef JD_EARPHONE_DETECT
bool proc_earphone_detect_flag = false;
#define JADARD_PROC_EARPHONE_FILE "EARPHONE"
struct proc_dir_entry *jadard_proc_earphone_file = NULL;
#endif

/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 start*/
#define HWINFO_NAME        "tp_wake_switch"
static struct platform_device hwinfo_device = {
    .name = HWINFO_NAME,
    .id = -1,
};
/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 end*/

static bool jadard_DbicWaitBusReady(uint8_t cmd)
{
    uint8_t status = 0;
    uint8_t int_clr_busy = JD_DBIC_INT_CLR_BUSY_MSK;
    uint32_t retry_time = 0x30;

    /* Get ready flag of bus status */
    g_module_fp.fp_register_read((uint32_t)JD_DBIC_STATUS, &status, 1);

    /* Wait bus ready & timeout */
    while ((status & (uint8_t)JD_DBIC_STATUS_RDY_BUSY_MSK) != (uint8_t)JD_DBIC_STATUS_RDY_MSK) {
        /* Add delay time to avoid bus busy error */
        if (cmd == 0x29) {
            msleep(10);
        }

        if ((retry_time--) <= 0) {
            /* Clear bus error flag */
            g_module_fp.fp_register_write((uint32_t)JD_DBIC_INT_CLR, &int_clr_busy, 1);
            return true;
        }

        /* Get ready flag of bus status */
        g_module_fp.fp_register_read((uint32_t)JD_DBIC_STATUS, &status, 1);
    }

    return false;
}

static bool jadard_DbicWaitBusTransferDone(bool int_clr_en)
{
    uint8_t status;
    uint8_t irq_valid_msk = JD_DBIC_IRQ_VALID_MSK;
    uint8_t int_clr_busy = JD_DBIC_INT_CLR_BUSY_MSK;
    uint32_t retry_time = 0x30;

    /* Get transfer status of bus */
    g_module_fp.fp_register_read((uint32_t)JD_DBIC_STATUS, &status, 1);

    while ((status & (uint8_t)JD_DBIC_IRQ_VALID_MSK) == 0x00) {
        if (((retry_time--) <= 0) || (status & (uint8_t)JD_DBIC_STATUS_BUSY_MSK)) {
            /* Clear bus error flag */
            g_module_fp.fp_register_write((uint32_t)JD_DBIC_INT_CLR, &int_clr_busy, 1);
            return true;
        }

        /* Get transfer status of bus */
        g_module_fp.fp_register_read((uint32_t)JD_DBIC_STATUS, &status, 1);
    }

    /* Clear transfer flag */
    if (int_clr_en) {
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_INT_CLR, &irq_valid_msk, 1);
    }

    /* Return error when spi_done_int = 1 & dbic_busy_err = 1 */
    if (status & (uint8_t)JD_DBIC_STATUS_BUSY_MSK) {
        /* Clear bus error flag */
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_INT_CLR, &int_clr_busy, 1);
        return true;
    }

    return false;
}

uint8_t jadard_DbicWriteDDReg(uint8_t cmd, uint8_t *par, uint8_t par_len)
{
    uint8_t i;
    bool dbi_c_status;
    uint8_t write_run = JD_DBIC_WRITE_RUN;

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_Fw_DBIC_Off();
    mdelay(100);
#endif

    /* Re-send command when dbic busy error */
    DBIC_CMD_RESEND:

    /* End condition of payload */
    /* 1st condition: cmd and len equal 0x00 */
    /* 2nd condition: timeout of dbi c bus     */
    if (jadard_DbicWaitBusReady(cmd)) {
        goto DBIC_CMD_RESEND;
    } else {
        if ((cmd == 0x10) || (cmd == 0x11) || (cmd == 0x28) ||
            (cmd == 0x29) || (cmd == 0x34) || (cmd == 0x35)) {
            par_len = 0;
        }

        /* Setup cmd & length */
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_CMD, &cmd, 1);
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_CMD_LEN, &par_len, 1);

        /* Enable transfer */
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_SPI_RUN, &write_run, 1);

        /* Polling transfer one byte done or task done */
        dbi_c_status = jadard_DbicWaitBusTransferDone(true);

        if (dbi_c_status) {
            goto DBIC_CMD_RESEND;
        }

        /* Write parameter */
        for (i = 0; i < par_len; i++) {
            if (jadard_DbicWaitBusReady(cmd)) {
                goto DBIC_CMD_RESEND;
            }

            /* Write */
            g_module_fp.fp_register_write((uint32_t)JD_DBIC_WRITE_DATA, par+i, 1);

            /* Polling transfer one byte done or task done */
            dbi_c_status = jadard_DbicWaitBusTransferDone(true);

            if (dbi_c_status) {
                goto DBIC_CMD_RESEND;
            }
        }
    }

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_Fw_DBIC_On();
#endif

    return JD_DBIC_READ_WRITE_SUCCESS;
}

uint8_t jadard_DbicReadDDReg(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len)
{
    uint8_t i;
    uint8_t read_data_cmd = JD_DBIC_READ_DATA_CMD;
    uint8_t read_run = JD_DBIC_READ_RUN;
    uint8_t irq_valid_msk = JD_DBIC_IRQ_VALID_MSK;

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_Fw_DBIC_Off();
    mdelay(100);
#endif

    /* Set read cmd */
    if (jadard_DbicWriteDDReg(JD_DBIC_READ_SET_CMD, &cmd, 1) == JD_DBIC_READ_WRITE_FAIL) {
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
        g_module_fp.fp_Fw_DBIC_On();
#endif
        return JD_DBIC_READ_WRITE_FAIL;
    } else {
        /* Setup cmd & length */
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_CMD, &read_data_cmd, 1);
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_CMD_LEN, &rpar_len, 1);

        /* Enable transfer */
        g_module_fp.fp_register_write((uint32_t)JD_DBIC_SPI_RUN, &read_run, 1);

        /* Read parameter */
        for (i = 0; i < rpar_len; i++) {
            /* Polling transfer one byte done or task done */
            if (jadard_DbicWaitBusTransferDone(false)) {
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
                g_module_fp.fp_Fw_DBIC_On();
#endif
                return JD_DBIC_READ_WRITE_FAIL;
            }

            g_module_fp.fp_register_read((uint32_t)JD_DBIC_READ_DATA, rpar+i, 1);
            g_module_fp.fp_register_write((uint32_t)JD_DBIC_INT_CLR, &irq_valid_msk, 1);
        }
    }
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_Fw_DBIC_On();
#endif

    return JD_DBIC_READ_WRITE_SUCCESS;
}

static void jadard_report_all_leave_event(struct jadard_ts_data *ts)
{
#ifndef JD_PROTOCOL_A
    int i;

    for (i = 0; i < pjadard_ic_data->JD_MAX_PT; i++) {
        input_mt_slot(ts->input_dev, i);
        input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
    }
#endif

    input_report_key(ts->input_dev, BTN_TOUCH, 0);
    input_sync(ts->input_dev);
}

int jadard_report_data_init(void)
{
    if (pjadard_report_data->touch_coord_info != NULL) {
        kfree(pjadard_report_data->touch_coord_info);
        pjadard_report_data->touch_coord_info = NULL;
    }

    pjadard_report_data->touch_coord_size = pjadard_ic_data->JD_MAX_PT * JD_FINGER_DATA_SIZE;
    pjadard_report_data->touch_coord_info =
        kzalloc(sizeof(uint8_t) * (pjadard_report_data->touch_coord_size), GFP_KERNEL);

    if (pjadard_report_data->touch_coord_info == NULL) {
        goto mem_alloc_fail;
    }

    if (pjadard_host_data == NULL) {
        pjadard_host_data = kzalloc(sizeof(struct jadard_host_data), GFP_KERNEL);
        if (pjadard_host_data == NULL)
            goto mem_alloc_fail;

        pjadard_host_data->id = kzalloc(sizeof(uint8_t)*(pjadard_ic_data->JD_MAX_PT), GFP_KERNEL);
        if (pjadard_host_data->id == NULL)
            goto mem_alloc_fail;

        pjadard_host_data->x = kzalloc(sizeof(int)*(pjadard_ic_data->JD_MAX_PT), GFP_KERNEL);
        if (pjadard_host_data->x == NULL) {
            goto mem_alloc_fail;
        } else {
            /* For setting touch event */
            memset(pjadard_host_data->x, 0xFF, sizeof(int)*(pjadard_ic_data->JD_MAX_PT));
        }

        pjadard_host_data->y = kzalloc(sizeof(int)*(pjadard_ic_data->JD_MAX_PT), GFP_KERNEL);
        if (pjadard_host_data->y == NULL)
            goto mem_alloc_fail;

        pjadard_host_data->w = kzalloc(sizeof(int)*(pjadard_ic_data->JD_MAX_PT), GFP_KERNEL);
        if (pjadard_host_data->w == NULL)
            goto mem_alloc_fail;

        pjadard_host_data->event = kzalloc(sizeof(int)*(pjadard_ic_data->JD_MAX_PT), GFP_KERNEL);
        if (pjadard_host_data->event == NULL)
            goto mem_alloc_fail;
    }

#ifdef JD_SMART_WAKEUP
    pjadard_host_data->SMWP_event_chk = 0;
#endif

    return JD_NO_ERR;

mem_alloc_fail:
    if (pjadard_host_data->id != NULL) {
        kfree(pjadard_host_data->id);
        pjadard_host_data->id = NULL;
    }

    if (pjadard_host_data->x != NULL) {
        kfree(pjadard_host_data->x);
        pjadard_host_data->x = NULL;
    }

    if (pjadard_host_data->y != NULL) {
        kfree(pjadard_host_data->y);
        pjadard_host_data->y = NULL;
    }

    if (pjadard_host_data->w != NULL) {
        kfree(pjadard_host_data->w);
        pjadard_host_data->w = NULL;
    }

    if (pjadard_host_data->event != NULL) {
        kfree(pjadard_host_data->event);
        pjadard_host_data->event = NULL;
    }

    if (pjadard_host_data != NULL) {
        kfree(pjadard_host_data);
        pjadard_host_data = NULL;
    }

    if (pjadard_report_data->touch_coord_info != NULL) {
        kfree(pjadard_report_data->touch_coord_info);
        pjadard_report_data->touch_coord_info = NULL;
    }
    JD_I("%s: Memory allocate fail!\n", __func__);

    return JD_MEM_ALLOC_FAIL;
}

#ifdef JD_SMART_WAKEUP
static void jadard_wake_event_report(void)
{
    int event_chk = pjadard_host_data->SMWP_event_chk;

    JD_I("%s: Entering\n", __func__);

    if (event_chk > 0) {
        JD_I("%s SMART WAKEUP KEY event %d press\n", __func__, event_chk);
        input_report_key(pjadard_ts_data->input_dev, event_chk, 1);
        input_sync(pjadard_ts_data->input_dev);

        JD_I("%s SMART WAKEUP KEY event %d release\n", __func__, event_chk);
        input_report_key(pjadard_ts_data->input_dev, event_chk, 0);
        input_sync(pjadard_ts_data->input_dev);
        pjadard_host_data->SMWP_event_chk = 0;
    }

    JD_I("%s: End\n", __func__);
}

static void jadard_wake_event_parse(struct jadard_ts_data *ts, int ts_status)
{
    uint8_t wake_event = pjadard_report_data->touch_event_info;
    int i;

    JD_I("%s: Entering, ts_status=%d, wake_event=0x%02x\n", __func__, ts_status, wake_event);

    for (i = 0; i < JD_GEST_SUP_NUM; i++) {
        if (wake_event == jd_gest_event[i]) {
            if (ts->gesture_cust_en[i]) {
                pjadard_host_data->SMWP_event_chk = jd_gest_key_def[i];
            } else {
                pjadard_host_data->SMWP_event_chk = 0;
            }
            break;
        }
    }

    JD_I("%s:end, SMWP_event_chk=%d\n", __func__, pjadard_host_data->SMWP_event_chk);
}
#endif

void jadard_report_points(struct jadard_ts_data *ts)
{
    int i;

    JD_D("%s: Entering\n", __func__);

    for (i = 0; i < pjadard_ic_data->JD_MAX_PT; i++) {
        if (pjadard_host_data->event[i] == JD_UP_EVENT) {
            JD_D("ID=%d, x=%d, y=%d\n", pjadard_host_data->id[i],
                pjadard_host_data->x[i], pjadard_host_data->y[i]);
            input_mt_slot(ts->input_dev, pjadard_host_data->id[i]);
            input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
        } else if ((pjadard_host_data->event[i] == JD_DOWN_EVENT) ||
                    (pjadard_host_data->event[i] == JD_CONTACT_EVENT)) {
            JD_D("ID=%d, x=%d, y=%d, w=%d\n", pjadard_host_data->id[i],
                pjadard_host_data->x[i], pjadard_host_data->y[i], pjadard_host_data->w[i]);

#ifndef JD_PROTOCOL_A
            input_mt_slot(ts->input_dev, pjadard_host_data->id[i]);
#endif
            input_report_key(ts->input_dev, BTN_TOUCH, 1);
            input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, pjadard_host_data->w[i]);
            input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, pjadard_host_data->id[i]);
#ifndef JD_PROTOCOL_A
            input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, pjadard_host_data->w[i]);
            input_report_abs(ts->input_dev, ABS_MT_PRESSURE, pjadard_host_data->w[i]);
#endif
        if (pjadard_ic_data->JD_X_RES <= pjadard_ic_data->JD_Y_RES) {
            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, pjadard_host_data->x[i]);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, pjadard_host_data->y[i]);
        } else if (pjadard_ic_data->JD_X_RES > pjadard_ic_data->JD_Y_RES) {
            input_report_abs(ts->input_dev, ABS_MT_POSITION_X, pjadard_host_data->y[i]);
            input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, pjadard_host_data->x[i]);
        }
#ifndef JD_PROTOCOL_A
            input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
#else
            input_mt_sync(ts->input_dev);
#endif
        }
    }

    input_sync(ts->input_dev);

    JD_D("%s:end\n", __func__);
}

static int jadard_report_data(struct jadard_ts_data *ts, int ts_path, int ts_status)
{
    JD_D("%s: Entering, ts_status=%d\n", __func__, ts_status);

    if (ts_path == JD_REPORT_COORD) {
        /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 start*/
        if (ts->tp_is_enabled != 0) {
            g_module_fp.fp_report_points(ts);
        }
        /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 end*/
#ifdef JD_SMART_WAKEUP
    } else if (ts_path == JD_REPORT_SMWP_EVENT) {
        jadard_wake_event_report();
#endif
    } else {
        JD_E("%s: Not support event\n", __func__);
        ts_status = JD_IRQ_EVENT_FAIL;
    }

    JD_D("%s: end, ts_status=%d\n", __func__, ts_status);

    return ts_status;
}

static int jadard_parse_report_points(struct jadard_ts_data *ts, int ts_status)
{
    uint8_t *pCoord_info = NULL;
    int x, y, w, i;

    JD_D("%s: Entering, ts_status=%d finger_num = %d\n", __func__,
        ts_status, pjadard_host_data->finger_num);

    if (pjadard_ic_data->JD_X_RES > pjadard_ic_data->JD_Y_RES) {
        ts->pdata->abs_x_max = pjadard_ic_data->JD_Y_RES;
        ts->pdata->abs_y_max = pjadard_ic_data->JD_X_RES;
    }

    for (i = 0; i < pjadard_ic_data->JD_MAX_PT; i++) {
        pCoord_info = pjadard_report_data->touch_coord_info + (JD_FINGER_DATA_SIZE * i);
        pjadard_host_data->id[i] = i;
        x  = (uint16_t)(pCoord_info[0] << 8 | pCoord_info[1]);
        y  = (uint16_t)(pCoord_info[2] << 8 | pCoord_info[3]);
        w  = pCoord_info[4]; /* Max:255 Min:0 */

        if ((x >= 0 && x <= ts->pdata->abs_x_max) &&
            (y >= 0 && y <= ts->pdata->abs_y_max)) {
            /* Set touch event */
            if (pjadard_host_data->x[i] == 0xFFFF) {
                pjadard_host_data->event[i] = JD_DOWN_EVENT;
            } else {
                pjadard_host_data->event[i] = JD_CONTACT_EVENT;
            }

            /* Set coordinate */
            pjadard_host_data->x[i] = x;
            pjadard_host_data->y[i] = y;
            pjadard_host_data->w[i] = w;
        } else {
            /* Set touch event */
            if (pjadard_host_data->x[i] == 0xFFFF) {
                pjadard_host_data->event[i] = JD_NO_EVENT;
            } else {
                pjadard_host_data->event[i] = JD_UP_EVENT;
            }

            /* Set coordinate to default */
            pjadard_host_data->x[i] = 0xFFFF;
            pjadard_host_data->y[i] = 0xFFFF;
            pjadard_host_data->w[i] = 0xFFFF;
        }

        JD_D("ID=%d, x=%d, y=%d, w=%d, event=%d\n", pjadard_host_data->id[i], pjadard_host_data->x[i],
            pjadard_host_data->y[i], pjadard_host_data->w[i], pjadard_host_data->event[i]);
    }

    JD_D("%s: end\n", __func__);

    return ts_status;
}

int jadard_parse_report_data(struct jadard_ts_data *ts, int irq_event, int ts_status)
{
    JD_D("%s: start now_status=%d\n", __func__, ts_status);

    switch (irq_event) {
    case JD_REPORT_COORD:
        ts_status = jadard_parse_report_points(ts, ts_status);
        break;
#if defined(JD_SMART_WAKEUP)
    case JD_REPORT_SMWP_EVENT:
        jadard_wake_event_parse(ts, ts_status);
        break;
#endif
    default:
        JD_E("%s: Not support event(%d)\n", __func__, irq_event);
        ts_status = JD_IRQ_EVENT_FAIL;
        break;
    }

    JD_D("%s: end now_status=%d\n", __func__, ts_status);

    return ts_status;
}

int jadard_distribute_touch_data(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status)
{
    struct jadard_report_data *pReport_data = pjadard_report_data;

    JD_D("%s: Entering, ts_status=%d\n", __func__, ts_status);

    switch (irq_event) {
    case JD_REPORT_COORD:
        /* Set finger number & Coordinate Info. */
        pjadard_host_data->finger_num = buf[JD_FINGER_NUM_ADDR];
        memcpy(pReport_data->touch_coord_info, buf + JD_TOUCH_COORD_INFO_ADDR,
                pReport_data->touch_coord_size);

        /* Copy touch state info. */
        memcpy(pReport_data->touch_state_info, buf + JD_TOUCH_STATE_INFO_ADDR,
                sizeof(pReport_data->touch_state_info));
        break;
#if defined(JD_SMART_WAKEUP)
    case JD_REPORT_SMWP_EVENT:
        pjadard_report_data->touch_event_info = buf[JD_TOUCH_EVENT_INFO_ADDR];
        break;
#endif
    default:
        JD_E("%s: Not support event(%d)\n", __func__, irq_event);
        ts_status = JD_IRQ_EVENT_FAIL;
        break;
    }

    JD_D("%s: End, ts_status=%d\n", __func__, ts_status);

    return ts_status;
}

void jadard_log_touch_state(void)
{
    /* Touch State 1 */
    JD_I("Water = %d, Glove = %d, Stylus = %d, Palm = %d\n",
        (pjadard_report_data->touch_state_info[0] >> 7) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 6) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 5) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 4) & 0x01);
    JD_I("Hover = %d, ESD = %d, Jitter = %d, Idle = %d\n",
        (pjadard_report_data->touch_state_info[0] >> 3) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 2) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 1) & 0x01,
        (pjadard_report_data->touch_state_info[0] >> 0) & 0x01);
    /* Touch State 2 */
    JD_I("LPWUP = %d, Re-CBT = %d, Edge-SPRS = %d, Finger-SP = %d, Zero Flash = %d, BSL = %d\n",
        (pjadard_report_data->touch_state_info[1] >> 7) & 0x01,
        (pjadard_report_data->touch_state_info[1] >> 6) & 0x01,
        (pjadard_report_data->touch_state_info[1] >> 5) & 0x01,
        (pjadard_report_data->touch_state_info[1] >> 4) & 0x01,
        (pjadard_report_data->touch_state_info[1] >> 3) & 0x01,
        (pjadard_report_data->touch_state_info[1] >> 2) & 0x01);
    /* Touch State 3 */
    JD_I("Touch Mode   = %d(0:Low Ground, 1:Normal, 2:AC mode)\n",
        (pjadard_report_data->touch_state_info[2] >> 6) & 0x03);
    JD_I("Trigger Mode = %d(0:Level trigger, 1:Edge trigger)\n",
        (pjadard_report_data->touch_state_info[2] >> 5) & 0x01);
    /* Touch State 4 */
    JD_I("Frequency state = 0x%04x\n",
        (pjadard_report_data->touch_state_info[3] << 8) | pjadard_report_data->touch_state_info[4]);
}

static int jadard_err_ctrl(uint8_t *buf, int ts_status)
{
    if (pjadard_ts_data->rst_active) {
        /* drop 1st interrupts after chip reset */
        pjadard_ts_data->rst_active = false;
        JD_I("[rst_active]:%s: Back from reset, ready to serve.\n", __func__);
        ts_status = JD_RST_OK;
    }
    JD_D("%s: end, ts_status=%d\n", __func__, ts_status);

    return ts_status;
}

static int jadard_touch_get(uint8_t *buf, int buf_len, int ts_status)
{
    JD_D("%s: Entering, ts_status=%d\n", __func__, ts_status);

    if (!g_module_fp.fp_get_touch_data(buf, buf_len)) {
        JD_E("%s: can't read data from chip!\n", __func__);
        ts_status = JD_TS_GET_DATA_FAIL;
    }

    return ts_status;
}

static int jadard_ts_operation(struct jadard_ts_data *ts, int irq_event, int ts_status)
{
    int touch_data_len = pjadard_report_data->touch_data_size;
    uint8_t touch_data[touch_data_len];

    JD_D("%s: Entering, ts_status=%d, irq_event=%d\n", __func__, ts_status, irq_event);
    memset(touch_data, 0x00, sizeof(touch_data));

    ts_status = jadard_touch_get(touch_data, touch_data_len, ts_status);
    if (ts_status == JD_TS_GET_DATA_FAIL)
        return ts_status;

    /* Copy FW package for APK */
#if defined(CONFIG_TOUCHSCREEN_JADARD_DEBUG)
    if (pjadard_ts_data->debug_fw_package_enable == true) {
        if (touch_data_len + 4 <= JD_HID_TOUCH_DATA_SIZE) {
            pjadard_report_data->report_rate_count &= 0xFFFF;
            pjadard_report_data->touch_all_info[1] = (uint8_t)(pjadard_report_data->report_rate_count >> 8);
            pjadard_report_data->touch_all_info[2] = (uint8_t)(pjadard_report_data->report_rate_count);
            memcpy((pjadard_report_data->touch_all_info) + 4, touch_data, touch_data_len);
            pjadard_report_data->report_rate_count++;
        } else {
            JD_E("%s: pjadard_report_data->touch_all_info was overflow\n", __func__);
        }
    }
#endif

    ts_status = jadard_err_ctrl(touch_data, ts_status);

    if ((ts_status == JD_REPORT_DATA) || (ts_status == JD_TS_NORMAL_START)) {
        ts_status = g_module_fp.fp_distribute_touch_data(ts, touch_data, irq_event, ts_status);
        ts_status = g_module_fp.fp_parse_report_data(ts, irq_event, ts_status);
    } else {
        return ts_status;
    }

    return jadard_report_data(ts, irq_event, ts_status);
}

#if defined(JD_USB_DETECT_GLOBAL)
void jadard_cable_detect(bool renew)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    uint32_t connect_status = jd_usb_detect_flag;

    if (ts->usb_status) {
        if (((!!connect_status) != ts->usb_connected) || renew) {
            if (!!connect_status) {
                ts->usb_status[1] = 0x01;
                ts->usb_connected = 0x01;
            } else {
                ts->usb_status[1] = 0x00;
                ts->usb_connected = 0x00;
            }

            g_module_fp.fp_usb_detect_set(ts->usb_status);
            JD_I("%s: Cable status change: 0x%2.2X\n", __func__, ts->usb_connected);
        }
    }
}
#endif

void jadard_ts_work(struct jadard_ts_data *ts)
{
    int ts_status = JD_TS_NORMAL_START;
    int irq_event = JD_REPORT_COORD;

    if (pjadard_debug != NULL)
        pjadard_debug->fp_touch_dbg_func(ts, JD_ATTN_IN);

#if defined(JD_USB_DETECT_GLOBAL)
    /* Update usb status when power on */
    if (ts->update_usb_status < 3) {
        jadard_cable_detect(true);
        ts->update_usb_status++;
    } else {
        jadard_cable_detect(false);
    }
#endif
#ifdef JD_SMART_WAKEUP
    if (atomic_read(&ts->suspend_mode) && (ts->SMWP_enable)) {
        irq_event = JD_REPORT_SMWP_EVENT;
        __pm_wakeup_event(ts->ts_SMWP_wake_lock, JD_TS_WAKE_LOCK_TIMEOUT);
    }
#endif

    switch (irq_event) {
    case JD_REPORT_COORD:
        ts_status = jadard_ts_operation(ts, irq_event, ts_status);
        break;
    case JD_REPORT_SMWP_EVENT:
        ts_status = jadard_ts_operation(ts, irq_event, ts_status);
        break;
    default:
        JD_E("%s: Not support event(%d)\n", __func__, irq_event);
        ts_status = JD_IRQ_EVENT_FAIL;
        break;
    }

#ifdef JD_RST_PIN_FUNC
    if (ts_status == JD_TS_GET_DATA_FAIL) {
        JD_I("%s: Now reset the Touch chip.\n", __func__);
        g_module_fp.fp_ic_reset(false, true);
    }
#endif

    if (pjadard_debug != NULL)
        pjadard_debug->fp_touch_dbg_func(ts, JD_ATTN_OUT);
}

#if defined(JD_USB_DETECT_CALLBACK)
/*hs14 code for AL6528A-425 by liudi at 20221025 start*/
void jadard_usb_status(int status, bool renew)
/*hs14 code for AL6528A-425 by liudi at 20221025 end*/
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    if (ts->usb_status) {
        if ((status != ts->usb_connected) || renew) {
            if (status) {
                ts->usb_status[1] = 0x01;
                ts->usb_connected = 0x01;
            } else {
                ts->usb_status[1] = 0x00;
                ts->usb_connected = 0x00;
            }

            g_module_fp.fp_usb_detect_set(ts->usb_status);
            JD_I("%s: Cable status update: 0x%2.2X\n", __func__, ts->usb_connected);
        }
    }
}

static int jadard_charger_notifier_callback(struct notifier_block *nb,
                                unsigned long val, void *v)
{
    int ret = 0;
    struct power_supply *psy_usb = NULL;
    struct power_supply *psy_ac = NULL;
    union power_supply_propval prop_usb;
    union power_supply_propval prop_ac;

    psy_usb = power_supply_get_by_name("usb");
    if (!psy_usb) {
        JD_E("%s: Couldn't get usb psy\n", __func__);
        return -EINVAL;
    }

    psy_ac = power_supply_get_by_name("ac");
    if (!psy_ac) {
        JD_E("%s: Couldn't get ac psy\n", __func__);
        return -EINVAL;
    }

    if (!strcmp(psy_usb->desc->name, "usb")) {
        if (psy_usb && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_ONLINE, &prop_usb);
            if (ret < 0) {
                JD_E("%s: Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", __func__, ret);
                return ret;
            }
        }
    }

    if (!strcmp(psy_ac->desc->name, "ac")) {
        if (psy_ac && val == POWER_SUPPLY_PROP_STATUS) {
            ret = power_supply_get_property(psy_ac, POWER_SUPPLY_PROP_ONLINE, &prop_ac);
            if (ret < 0) {
                JD_E("%s: Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", __func__, ret);
                return ret;
            }
        }
    }

    if (prop_usb.intval || prop_ac.intval) {
        jadard_usb_status(1, false);
    } else {
        jadard_usb_status(0, false);
    }
    return 0;
}

static void jadard_charger_init(struct work_struct *work)
{
    int ret = 0;
    struct power_supply *psy_usb = NULL;
    struct power_supply *psy_ac = NULL;
    union power_supply_propval prop_usb;
    union power_supply_propval prop_ac;

    pjadard_ts_data->charger_notif.notifier_call = jadard_charger_notifier_callback;
    ret = power_supply_reg_notifier(&pjadard_ts_data->charger_notif);
    if (ret) {
        JD_E("Unable to register charger_notifier: %d\n", ret);
    }

    /* if power supply supplier registered brfore TP
    * ps_notify_callback will not receive PSY_EVENT_PROP_ADDED
    * event, and will cause miss to set TP into charger state.
    * So check PS state in probe.
    */
    psy_usb = power_supply_get_by_name("usb");
    if (!psy_usb) {
        JD_E("%s: Couldn't get usb psy\n", __func__);
        goto SKIP_PSY;
    }

    psy_ac = power_supply_get_by_name("ac");
    if (!psy_ac) {
        JD_E("%s: Couldn't get ac psy\n", __func__);
        goto SKIP_PSY;
    }

    if (!strcmp(psy_usb->desc->name, "usb")) {
        ret = power_supply_get_property(psy_usb, POWER_SUPPLY_PROP_ONLINE, &prop_usb);
        if (ret < 0) {
            JD_E("%s: Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", __func__, ret);
        }
    }

    if (!strcmp(psy_ac->desc->name, "ac")) {
        ret = power_supply_get_property(psy_ac, POWER_SUPPLY_PROP_ONLINE, &prop_ac);
        if (ret < 0) {
            JD_E("%s: Couldn't get POWER_SUPPLY_PROP_ONLINE rc=%d\n", __func__, ret);
        }
    }

    if (prop_usb.intval || prop_ac.intval) {
        jadard_usb_status(1, true);
    } else {
        jadard_usb_status(0, true);
    }
SKIP_PSY:
    JD_I("%s:() end\n", __func__);
}
#endif

#if defined(JD_AUTO_UPGRADE_FW)
static int jadard_upgrade_FW(void)
{
    bool result = false;

    jadard_int_enable(false);

    if (g_module_fp.fp_flash_write(0, g_jadard_fw, g_jadard_fw_len) < 0) {
        JD_E("%s: TP upgrade fail\n", __func__);
    } else {
        JD_I("%s: TP upgrade success\n", __func__);
        g_module_fp.fp_read_fw_ver();
        result = true;
    }

    kfree(g_jadard_fw);
    g_jadard_fw = NULL;
    g_jadard_fw_len = 0;

    jadard_int_enable(true);

    return result;
}

static bool jadard_auto_upgrade_check(void)
{
    bool upgrade = false;

    JD_D("%s:Entering\n", __func__);

    if (g_module_fp.fp_read_fw_ver_bin() == JD_NO_ERR) {
        JD_I("pjadard_ic_data->fw_ver=%08x, g_jadard_fw_ver=%08x\n",
            pjadard_ic_data->fw_ver, g_jadard_fw_ver);
        JD_I("pjadard_ic_data->fw_cid_ver=%08x, g_jadard_fw_cid_ver=%08x\n",
            pjadard_ic_data->fw_cid_ver, g_jadard_fw_cid_ver);

        if ((pjadard_ic_data->fw_ver < g_jadard_fw_ver) ||
            (pjadard_ic_data->fw_cid_ver < g_jadard_fw_cid_ver)) {
            JD_I("Need to upgrade FW\n");
            upgrade = true;
        } else {
            JD_I("No need to upgrade FW\n");
        }
    }

    return upgrade;
}

static int jadard_get_FW(void)
{
    int ret = JD_NO_ERR;
    int RetryCnt;
    const struct firmware *fw = NULL;

    JD_I("file name = %s\n", jd_i_CTPM_firmware_name);

    for (RetryCnt = 0; RetryCnt < JD_UPGRADE_FW_RETRY_TIME; RetryCnt++) {
        ret = request_firmware(&fw, jd_i_CTPM_firmware_name, pjadard_ts_data->dev);
        if (ret < 0) {
            JD_E("%s: Open file fail(ret:%d), RetryCnt = %d\n", __func__, ret, RetryCnt);
            mdelay(1000);
        } else {
            break;
        }
    }

    if (RetryCnt == JD_UPGRADE_FW_RETRY_TIME) {
        JD_E("%s: Open file fail retry over %d\n", __func__, JD_UPGRADE_FW_RETRY_TIME);
        return JD_FILE_OPEN_FAIL;
    }

    g_jadard_fw_len = fw->size;
    g_jadard_fw = kzalloc(sizeof(char) * g_jadard_fw_len, GFP_KERNEL);

    if (g_jadard_fw == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        ret = JD_MEM_ALLOC_FAIL;
    } else {
        memcpy(g_jadard_fw, fw->data, sizeof(char) * g_jadard_fw_len);
    }
    release_firmware(fw);

    return ret;
}

static void jadard_upgrade_process(struct work_struct *work)
{
    JD_D("%s, Entering\n", __func__);

    if (jadard_get_FW() >= 0) {
        if (jadard_auto_upgrade_check()) {
            if (jadard_upgrade_FW() <= 0) {
                JD_E("Auto upgrade fail\n");
            } else {
                JD_I("Auto upgrade finish\n");
            }
        }
    }
}
#endif

#if defined(JD_CONFIG_NODE)
static ssize_t ts_suspend_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    return snprintf(buf, 20, "ts->suspended=%d\n", ts->suspended);
}

static ssize_t ts_suspend_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    if (buf[0] == '1') {
        jadard_chip_common_suspend(ts);
    } else if (buf[0] == '0') {
        jadard_chip_common_resume(ts);
    }

    JD_I("%s: ts->suspended = %d.\n", __func__, ts->suspended);

    return count;
}

static DEVICE_ATTR_RW(ts_suspend);

static struct attribute *jadard_ts_debug_attrs[] = {
    &dev_attr_ts_suspend.attr,
    NULL,
};

static struct attribute_group jadard_ts_debug_attr_group = {
    .attrs = jadard_ts_debug_attrs,
};

static int jadard_ts_filesys_create(struct jadard_ts_data *ts)
{
    int retval;

    /* create sysfs debug files */
    retval = sysfs_create_group(&ts->dev->kobj,
            &jadard_ts_debug_attr_group);
    if (retval < 0) {
        JD_E("%s: Fail to create debug files!\n", __func__);
        return -ENOMEM;
    }

    /* convenient access to sysfs node */
    retval = sysfs_create_link(NULL, &ts->dev->kobj, "touchscreen");
    if (retval < 0) {
        JD_E("%s: Failed to create link!\n", __func__);
        return -ENOMEM;
    }

    return 0;
}

static void jadard_ts_filesys_remove(struct jadard_ts_data *ts)
{
    sysfs_remove_link(NULL, "touchscreen");
    sysfs_remove_group(&ts->dev->kobj, &jadard_ts_debug_attr_group);
}
#endif

#ifdef JD_SMART_WAKEUP
static ssize_t jadard_SMWP_read(struct file *file, char *buf,
                               size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    size_t ret = 0;
    char *buf_tmp = NULL;

    if (!proc_smwp_send_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);

        if (buf_tmp != NULL) {
            ret += snprintf(buf_tmp + ret, len - ret, "SMWP_enable=%d\n", ts->SMWP_enable);

            if (copy_to_user(buf, buf_tmp, len))
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);

            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }

        proc_smwp_send_flag = true;
    } else {
        proc_smwp_send_flag = false;
    }

    return ret;
}

static ssize_t jadard_SMWP_write(struct file *file, const char *buf,
                                size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    char buf_tmp[10] = {0};

    if (len >= sizeof(buf_tmp)) {
        JD_I("%s: no command exceeds %ld chars.\n", __func__, sizeof(buf_tmp));
        return -EFAULT;
    }

    if (copy_from_user(buf_tmp, buf, len)) {
        return -EFAULT;
    }

    if (buf_tmp[0] == '1')
        ts->SMWP_enable = true;
    else
        ts->SMWP_enable = false;

    g_module_fp.fp_set_SMWP_enable(ts->SMWP_enable);
    JD_I("%s: SMART_WAKEUP_enable = %d.\n", __func__, ts->SMWP_enable);

    return len;
}

static struct file_operations jadard_proc_SMWP_ops = {
    .owner = THIS_MODULE,
    .read = jadard_SMWP_read,
    .write = jadard_SMWP_write,
};

static ssize_t jadard_GESTURE_read(struct file *file, char *buf,
                                  size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    size_t ret = 0;
    char *buf_tmp = NULL;
    int i;

    if (!proc_smwp_send_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);

        if (buf_tmp != NULL) {
            for (i = 0; i < JD_GEST_SUP_NUM; i++)
                ret += snprintf(buf_tmp + ret, len - ret, "ges_en[%d]=%d \n",
                                i, ts->gesture_cust_en[i]);

            if (copy_to_user(buf, buf_tmp, len))
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);

            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }

        proc_smwp_send_flag = true;
    } else {
        proc_smwp_send_flag = false;
    }

    return ret;
}

static ssize_t jadard_GESTURE_write(struct file *file, const char *buf,
                                   size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    char buf_tmp[JD_GEST_SUP_NUM + 1] = {0};
    int i;

    if (len > sizeof(buf_tmp)) {
        JD_I("%s: no command exceeds %ld chars.\n", __func__, sizeof(buf_tmp));
        return -EFAULT;
    }

    if (copy_from_user(buf_tmp, buf, len)) {
        return -EFAULT;
    }

    JD_I("jadard_GESTURE_CMD = %s\n", buf_tmp);

    for (i = 0; i < len - 1 && i < JD_GEST_SUP_NUM; i++) {
        if (buf_tmp[i] == '1') {
            ts->gesture_cust_en[i] = true;
        } else {
            ts->gesture_cust_en[i] = false;
        }
        JD_I("gesture en[%d]=%d \n", i, ts->gesture_cust_en[i]);
    }

    return len;
}

static struct file_operations jadard_proc_Gesture_ops = {
    .owner = THIS_MODULE,
    .read = jadard_GESTURE_read,
    .write = jadard_GESTURE_write,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
static ssize_t jadard_sorting_test_read(struct file *file, char *buf,
                                    size_t len, loff_t *pos)
{
    size_t ret = 0;
    char *buf_tmp = NULL;
    int val;

    if (!pjadard_debug->proc_send_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);

        if (buf_tmp != NULL) {
            val = g_module_fp.fp_sorting_test();

            if (val == 0) {
                ret += snprintf(buf_tmp + ret, len - ret, "Sorting_Test Pass(%d)\n", val);
            } else {
                ret += snprintf(buf_tmp + ret, len - ret, "Sorting_Test Fail(%d)\n", val);
            }

            if (copy_to_user(buf, buf_tmp, len)) {
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);
            }

            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }

        pjadard_debug->proc_send_flag = true;
    } else {
        pjadard_debug->proc_send_flag = false;
    }

    return ret;
}

static struct file_operations jadard_proc_sorting_test_ops = {
    .owner = THIS_MODULE,
    .read = jadard_sorting_test_read,
};

#if defined(JD_OPPO_FUNC)
static struct file_operations jadard_proc_baseline_test_ops = {
    .owner = THIS_MODULE,
    .read = jadard_sorting_test_read,
};


static struct file_operations jadard_proc_black_test_ops = {
    .owner = THIS_MODULE,
    .read = jadard_sorting_test_read,
};
#endif

extern void jadard_sorting_init(void);
#endif

#ifdef JD_HIGH_SENSITIVITY
static ssize_t jadard_high_sensitivity_read(struct file *file, char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    size_t ret = 0;
    char *buf_tmp = NULL;

    if (!proc_Hsens_send_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);
        if (buf_tmp != NULL) {
            ret += snprintf(buf_tmp + ret, len - ret, "High_sensitivity_enable=%d\n", ts->high_sensitivity_enable);
            if (copy_to_user(buf, buf_tmp, len))
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);
            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }
        proc_Hsens_send_flag = true;
    } else {
        proc_Hsens_send_flag = false;
    }

    return ret;
}

static ssize_t jadard_high_sensitivity_write(struct file *file, const char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    char buf_tmp[10] = {0};

    if (len >= sizeof(buf_tmp)) {
        JD_I("%s: no command exceeds %ld chars.\n", __func__, sizeof(buf_tmp));
        return -EFAULT;
    }

    if (copy_from_user(buf_tmp, buf, len)) {
        return -EFAULT;
    }

    if (buf_tmp[0] == '1') {
        ts->high_sensitivity_enable = true;
    } else {
        ts->high_sensitivity_enable = false;
    }

    g_module_fp.fp_set_high_sensitivity(ts->high_sensitivity_enable);
    JD_I("%s: High_sensitivity_enable = %d.\n", __func__, ts->high_sensitivity_enable);

    return len;
}

static struct file_operations jadard_proc_high_sensitivity_ops = {
    .owner = THIS_MODULE,
    .read = jadard_high_sensitivity_read,
    .write = jadard_high_sensitivity_write,
};
#endif

#ifdef JD_ROTATE_BORDER
static ssize_t jadard_rotate_border_read(struct file *file, char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    size_t ret = 0;
    char *buf_tmp = NULL;

    if (!proc_rotate_border_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);
        if (buf_tmp != NULL) {
            ret += snprintf(buf_tmp + ret, len - ret, "rotate_border = %d (0x%04x)\n", ts->rotate_switch_mode, ts->rotate_border);
            if (copy_to_user(buf, buf_tmp, len))
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);
            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }
        proc_rotate_border_flag = true;
    } else {
        proc_rotate_border_flag = false;
    }

    return ret;
}

static ssize_t jadard_rotate_border_write(struct file *file, const char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    char buf_tmp[10] = {0};
    int RotateType;

    if (len >= sizeof(buf_tmp)) {
        JD_I("%s: no command exceeds %ld chars.\n", __func__, sizeof(buf_tmp));
        return -EFAULT;
    }

    if (copy_from_user(buf_tmp, buf, len)) {
        return -EFAULT;
    }

    RotateType = buf_tmp[0] - '0';
    if ((RotateType < 1) || (RotateType > 3)) {
        JD_E("%s: RotateType must (1,2,3)\n", __func__);
        return -EINVAL;
    }

    ts->rotate_switch_mode = RotateType;
    switch (RotateType) {
    case 1: /* Vertical */
        ts->rotate_border = 0xA55A;
        break;
    case 2: /* Horizontal notch left side */
        ts->rotate_border = 0xA11A;
        break;
    case 3: /* horizontal notch right side */
        ts->rotate_border = 0xA33A;
        break;
    }

    g_module_fp.fp_set_rotate_border(ts->rotate_border);
    JD_I("%s: rotate_border = 0x%04x\n", __func__, ts->rotate_border);

    return len;
}

static struct file_operations jadard_proc_rotate_border_ops = {
    .owner = THIS_MODULE,
    .read = jadard_rotate_border_read,
    .write = jadard_rotate_border_write,
};
#endif

#ifdef JD_EARPHONE_DETECT
static ssize_t jadard_earphone_read(struct file *file, char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    size_t ret = 0;
    char *buf_tmp = NULL;

    if (!proc_earphone_detect_flag) {
        buf_tmp = kzalloc(len * sizeof(char), GFP_KERNEL);
        if (buf_tmp != NULL) {
            ret += snprintf(buf_tmp + ret, len - ret, "Earphone_enable = %d\n", ts->earphone_enable);
            if (copy_to_user(buf, buf_tmp, len))
                JD_E("%s, copy_to_user fail: %d\n", __func__, __LINE__);
            kfree(buf_tmp);
        } else {
            JD_E("%s, Memory allocate fail: %d\n", __func__, __LINE__);
        }
        proc_earphone_detect_flag = true;
    } else {
        proc_earphone_detect_flag = false;
    }

    return ret;
}

static ssize_t jadard_earphone_write(struct file *file, const char *buf,
                                            size_t len, loff_t *pos)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    char buf_tmp[10] = {0};

    if (len >= sizeof(buf_tmp)) {
        JD_I("%s: no command exceeds %ld chars.\n", __func__, sizeof(buf_tmp));
        return -EFAULT;
    }

    if (copy_from_user(buf_tmp, buf, len)) {
        return -EFAULT;
    }

    if (buf_tmp[0] == '1') {
        ts->earphone_enable = true;
    } else {
        ts->earphone_enable = false;
    }

    g_module_fp.fp_set_earphone_enable(ts->earphone_enable);
    JD_I("%s: Earphone_enable = %d.\n", __func__, ts->earphone_enable);

    return len;
}

static struct file_operations jadard_proc_earphone_ops = {
    .owner = THIS_MODULE,
    .read = jadard_earphone_read,
    .write = jadard_earphone_write,
};
#endif

int jadard_common_proc_init(void)
{
    pjadard_touch_proc_dir = proc_mkdir(JADARD_PROC_TOUCH_FOLDER, NULL);
#if defined(JD_OPPO_FUNC)
    pjadard_touchpanel_proc_dir = proc_mkdir(JADARD_PROC_TOUCHPANLE_FOLDER, NULL);
#endif

    if (pjadard_touch_proc_dir == NULL) {
        JD_E(" %s: pjadard_touch_proc_dir file create failed!\n", __func__);
        return -ENOMEM;
    }

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    jadard_sorting_init();

    jadard_proc_sorting_test_file = proc_create(JADARD_PROC_SORTING_TEST_FILE, (S_IRUGO),
                                        pjadard_touch_proc_dir, &jadard_proc_sorting_test_ops);
    if (jadard_proc_sorting_test_file == NULL) {
        JD_E(" %s: proc self_test file create failed!\n", __func__);
        goto fail_1;
    }
#if defined(JD_OPPO_FUNC)
    pjadard_baseline_test_file = proc_create(JADARD_PROC_BASELINE_TEST_FILE, (S_IRUGO),
                                        pjadard_touchpanel_proc_dir, &jadard_proc_baseline_test_ops);
    if (pjadard_baseline_test_file == NULL) {
        JD_E(" %s: proc self_test file create failed!\n", __func__);
        goto fail_1_1;
    }

    pjadard_blackscreen_baseline_file = proc_create(JADARD_PROC_BLACK_SCREEN_TEST_FILE, (S_IRUGO),
                                        pjadard_touchpanel_proc_dir, &jadard_proc_black_test_ops);
    if (pjadard_blackscreen_baseline_file == NULL) {
        JD_E(" %s: proc black_test file create failed!\n", __func__);
        goto fail_1_2;
    }
#endif
#endif

#ifdef JD_SMART_WAKEUP
    jadard_proc_SMWP_file = proc_create(JADARD_PROC_SMWP_FILE, (S_IWUSR | S_IRUGO | S_IWUGO),
                                       pjadard_touch_proc_dir, &jadard_proc_SMWP_ops);
    if (jadard_proc_SMWP_file == NULL) {
        JD_E(" %s: proc SMWP file create failed!\n", __func__);
        goto fail_2;
    }

    jadard_proc_GESTURE_file = proc_create(JADARD_PROC_GESTURE_FILE, (S_IWUSR | S_IRUGO | S_IWUGO),
                                          pjadard_touch_proc_dir, &jadard_proc_Gesture_ops);
    if (jadard_proc_GESTURE_file == NULL) {
        JD_E(" %s: proc GESTURE file create failed!\n", __func__);
        goto fail_3;
    }
#endif

#ifdef JD_HIGH_SENSITIVITY
    jadard_proc_high_sensitivity_file = proc_create(JADARD_PROC_HIGH_SENSITIVITY_FILE, (S_IWUSR | S_IRUGO | S_IWUGO),
                                       pjadard_touch_proc_dir, &jadard_proc_high_sensitivity_ops);
    if (jadard_proc_high_sensitivity_file == NULL) {
        JD_E(" %s: proc high sensitivity file create failed!\n", __func__);
        goto fail_4;
    }
#endif

#ifdef JD_ROTATE_BORDER
    jadard_proc_rotate_border_file = proc_create(JADARD_PROC_ROTATE_BORDER_FILE, (S_IWUSR | S_IRUGO | S_IWUGO),
                                       pjadard_touch_proc_dir, &jadard_proc_rotate_border_ops);
    if (jadard_proc_rotate_border_file == NULL) {
        JD_E(" %s: proc rotate border file create failed!\n", __func__);
        goto fail_5;
    }
#endif

#ifdef JD_EARPHONE_DETECT
    jadard_proc_earphone_file = proc_create(JADARD_PROC_EARPHONE_FILE, (S_IWUSR | S_IRUGO | S_IWUGO),
                                       pjadard_touch_proc_dir, &jadard_proc_earphone_ops);
    if (jadard_proc_earphone_file == NULL) {
        JD_E(" %s: proc earphone file create failed!\n", __func__);
        goto fail_6;
    }
#endif

    return 0;

#ifdef JD_EARPHONE_DETECT
fail_6:
    remove_proc_entry(JADARD_PROC_EARPHONE_FILE, pjadard_touch_proc_dir);
#endif
#ifdef JD_ROTATE_BORDER
fail_5:
    remove_proc_entry(JADARD_PROC_ROTATE_BORDER_FILE, pjadard_touch_proc_dir);
#endif
#ifdef JD_HIGH_SENSITIVITY
fail_4:
    #ifdef JD_SMART_WAKEUP
    remove_proc_entry(JADARD_PROC_GESTURE_FILE, pjadard_touch_proc_dir);
    #endif
#endif
#ifdef JD_SMART_WAKEUP
fail_3:
    remove_proc_entry(JADARD_PROC_SMWP_FILE, pjadard_touch_proc_dir);
fail_2:
#endif
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
#if defined(JD_OPPO_FUNC)
    remove_proc_entry(JADARD_PROC_BLACK_SCREEN_TEST_FILE, pjadard_touchpanel_proc_dir);
fail_1_2:
    remove_proc_entry(JADARD_PROC_BASELINE_TEST_FILE, pjadard_touchpanel_proc_dir);
fail_1_1:
#endif
    remove_proc_entry(JADARD_PROC_SORTING_TEST_FILE, pjadard_touch_proc_dir);

fail_1:
#endif

    return -ENOMEM;
}

void jadard_common_proc_deinit(void)
{
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    remove_proc_entry(JADARD_PROC_SORTING_TEST_FILE, pjadard_touch_proc_dir);
#if defined(JD_OPPO_FUNC)
    remove_proc_entry(JADARD_PROC_BASELINE_TEST_FILE, pjadard_touchpanel_proc_dir);
    remove_proc_entry(JADARD_PROC_BLACK_SCREEN_TEST_FILE, pjadard_touchpanel_proc_dir);
#endif
#endif
#ifdef JD_SMART_WAKEUP
    remove_proc_entry(JADARD_PROC_GESTURE_FILE, pjadard_touch_proc_dir);
    remove_proc_entry(JADARD_PROC_SMWP_FILE, pjadard_touch_proc_dir);
#endif
#ifdef JD_HIGH_SENSITIVITY
    remove_proc_entry(JADARD_PROC_HIGH_SENSITIVITY_FILE, pjadard_touch_proc_dir);
#endif
#ifdef JD_ROTATE_BORDER
    remove_proc_entry(JADARD_PROC_ROTATE_BORDER_FILE, pjadard_touch_proc_dir);
#endif
#ifdef JD_EARPHONE_DETECT
    remove_proc_entry(JADARD_PROC_EARPHONE_FILE, pjadard_touch_proc_dir);
#endif
}

/*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 start*/
static int jadard_input_open(struct input_dev *dev)
{
    pjadard_ts_data->tp_is_enabled = 1;
    return 0;
}

static void jadard_input_close(struct input_dev *dev)
{
    pjadard_ts_data->tp_is_enabled = 0;
}
/*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 end*/

int jadard_input_register(struct jadard_ts_data *ts)
{
    int ret = 0;
#if defined(JD_SMART_WAKEUP)
    int i;
#endif

    ret = jadard_dev_set(ts);
    if (ret < 0) {
        goto input_device_fail;
    }

    set_bit(EV_SYN, ts->input_dev->evbit);
    set_bit(EV_ABS, ts->input_dev->evbit);
    set_bit(EV_KEY, ts->input_dev->evbit);

#if defined(JD_SMART_WAKEUP)
    /*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 start*/
    set_bit(KEY_WAKEUP, ts->input_dev->keybit);
    /*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 end*/

    for (i = 1; i < JD_GEST_SUP_NUM; i++) {
        set_bit(jd_gest_key_def[i], ts->input_dev->keybit);
    }
#endif
    set_bit(BTN_TOUCH, ts->input_dev->keybit);
    set_bit(KEY_APPSELECT, ts->input_dev->keybit);
    set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#ifdef    JD_PROTOCOL_A
    input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#else
    set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
#if defined(JD_PROTOCOL_B_3PA)
    input_mt_init_slots(ts->input_dev, pjadard_ic_data->JD_MAX_PT, INPUT_MT_DIRECT);
#else
    input_mt_init_slots(ts->input_dev, pjadard_ic_data->JD_MAX_PT);
#endif
#endif
    JD_I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
      ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min, ts->pdata->abs_y_max);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, ts->pdata->abs_x_min, ts->pdata->abs_x_max,
                            ts->pdata->abs_x_fuzz, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, ts->pdata->abs_y_min, ts->pdata->abs_y_max,
                            ts->pdata->abs_y_fuzz, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min,
                            ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
#ifndef JD_PROTOCOL_A
    input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min,
                            ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
    input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, ts->pdata->abs_width_min,
                            ts->pdata->abs_width_max, ts->pdata->abs_pressure_fuzz, 0);
#endif
    /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 start*/
    ts->input_dev->open = jadard_input_open;
    ts->input_dev->close = jadard_input_close;
    /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 end*/
    if (jadard_input_register_device(ts->input_dev) == 0) {
        return JD_NO_ERR;
    } else {
        ret = JD_INPUT_REG_FAIL;
    }

input_device_fail:
    JD_I("%s, input device register fail!\n", __func__);
    return ret;
}

#ifdef JD_CONFIG_FB
static void jadard_fb_register(struct work_struct *work)
{
    int ret = 0;
    struct jadard_ts_data *ts = container_of(work, struct jadard_ts_data, work_fb.work);

    JD_I("%s: enter\n", __func__);
    ts->fb_notif.notifier_call = jadard_fb_notifier_callback;
    ret = fb_register_client(&ts->fb_notif);

    if (ret) {
        JD_E(" Unable to register fb_notifier: %d\n", ret);
    }
    /*hs14 code for AL6528A-213 by hehaoran5 at 20221002 start*/
    ts->esd_recovery_notifier.notifier_call = jadard_esd_recovery_notifier_callback;
    ret = register_esd_tp_recovery_notifier(&ts->esd_recovery_notifier);
    if (ret) {
        JD_E(" Unable to register esd recovery notifier: %d\n", ret);
    }
    /*hs14 code for AL6528A-213 by hehaoran5 at 20221002 end*/
}
#endif

#ifdef JD_CONFIG_SPRD_DRM
/*
*jadard_resume_work_function
*Param：struct work_struct *work
*Return：void
*Purpose：queue work for Reduce screen time
*/
static void jadard_resume_work_function(struct work_struct *work)
{
    jadard_chip_common_resume(pjadard_ts_data);
}

static int jadard_drm_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
    JD_I("event = %d, pjadard_ts_data->suspended = %d", (int)event, pjadard_ts_data->suspended);

    if ((event == DISPC_POWER_OFF) && !pjadard_ts_data->suspended) {
        jadard_chip_common_suspend(pjadard_ts_data);
    } else if ((event == DISPC_POWER_ON) && pjadard_ts_data->suspended) {
        queue_work(pjadard_ts_data->jadard_resume_wq, &pjadard_ts_data->jadard_resume_work);
    }

    return 0;
}
#endif

/*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 start*/
int jadard_enable_gesture_wakeup(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    if (ts == NULL) {
        return -EFAULT;
    }
    ts->gesture_cust_en[0] = true; // 0 is double click gesture
    ts->SMWP_enable = true;
    smart_wakeup_open_flag = true;
    g_module_fp.fp_set_SMWP_enable(ts->SMWP_enable);
    return 0;
}

int jadard_disable_gesture_wakeup(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    if (ts == NULL) {
        return -EFAULT;
    }
    ts->gesture_cust_en[0] = false; // 0 is double click gesture
    ts->SMWP_enable = false;
    smart_wakeup_open_flag = false;
    g_module_fp.fp_set_SMWP_enable(ts->SMWP_enable);
    return 0;
}
/*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 end*/

/*hs14 code for SR-AL6528A-01-525 by liudi at 20220908 start*/
static struct proc_dir_entry *jd_info_proc_entry;

static ssize_t jd_proc_getinfo_read(struct file *flip,char __user *buff,size_t size,loff_t *pPos)
{
    char buf[50] = {0};
    int rc = 0;
    if (pjadard_ic_data == NULL) {
        return -EFAULT;
    }
    /*hs14 code for AL6528A-46 by liudi at 20220915 start*/
    snprintf(buf, sizeof(buf), "IC_NAME:JD9522T_XX_BOE TOUCH_VER:%02x\n", pjadard_ic_data->fw_cid_ver & 0xFF);
    /*hs14 code for AL6528A-46 by liudi at 20220915 end*/
    rc = simple_read_from_buffer(buff,size,pPos,buf,strlen(buf));
    return rc;
}

static const struct file_operations jd_info_proc_fops = {
    .owner = THIS_MODULE,
    .read = jd_proc_getinfo_read,
};

static int32_t jd_extra_proc_init(void)
{
    jd_info_proc_entry = proc_create(JD_INFO_PROC_FILE,0666,NULL,&jd_info_proc_fops);
    if (NULL == jd_info_proc_entry) {
        printk("%s():Couldn't create proc entry!\n",__func__);
        return -EFAULT;
    } else {
        printk("%s():Create proc entry success\n",__func__);
    }
    return 0;
}
/*hs14 code for SR-AL6528A-01-525 by liudi at 20220908 end*/

/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 start*/
static ssize_t jd_ito_test_show(struct device *dev,struct device_attribute *attr, char *buf)
{
    size_t ret = 0;
    int val;

    JD_I("%s: enter\n", __func__);
    jadard_int_enable(false);

    val = g_module_fp.fp_sorting_test();

    jadard_int_enable(true);

    if (val == 0) {
        val = 1;
    } else {
        val = 0;
    }
    ret = sprintf(buf, "%d\n", val);
    return ret;
}

static ssize_t jd_ito_test_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t count)
{
    return count;
}

static DEVICE_ATTR(factory_check, 0644, jd_ito_test_show, jd_ito_test_store);
static struct attribute *jd_ito_test_attributes[] = {
    &dev_attr_factory_check.attr,
    NULL
};
static struct attribute_group jd_ito_test_attribute_group = {
    .attrs = jd_ito_test_attributes
};

static int jd_test_node_init(struct platform_device *tpinfo_device)
{
    int ret = 0;
    ret = sysfs_create_group(&tpinfo_device->dev.kobj, &jd_ito_test_attribute_group);
    if (0 != ret) {
        JD_E( "ERROR: ITO node create failed.");
        sysfs_remove_group(&tpinfo_device->dev.kobj, &jd_ito_test_attribute_group);
        return -EIO;
    } else {
        JD_I("ITO node create_succeeded.");
    }
    return ret;
}
/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 end*/

/*hs14 code for AL6528A-338 by liudi at 20221018 start*/
static int jadard_earphone_notifier_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
    static earphone_status = 0;
    if (event == HEADSET_PLUGIN_STATE) {
        pjadard_ts_data->earphone_enable = 1;
        JD_I("%s():earphone in\n",__func__);
    } else if (event == HEADSET_PLUGOUT_STATE) {
        pjadard_ts_data->earphone_enable = 0;
        JD_I("%s():earphone out\n",__func__);
    } else {
        JD_I("%s():earphone err\n",__func__);
        return -EINVAL;
    }
    g_module_fp.fp_set_earphone_enable(pjadard_ts_data->earphone_enable);

    if (earphone_status != pjadard_ts_data->earphone_enable) {
        JD_I("earphone status:%d\n",pjadard_ts_data->earphone_enable);
        earphone_status = pjadard_ts_data->earphone_enable;
    }

    return 0;
}
/*hs14 code for AL6528A-338 by liudi at 20221018 end*/

int jadard_chip_common_init(void)
{
    struct jadard_support_chip *ptr = g_module_fp.head_support_chip;
    struct jadard_support_chip *del_ptr = NULL;
    struct jadard_ts_data *ts = pjadard_ts_data;
    struct jadard_i2c_platform_data *pdata = NULL;
    int ret, err;
/*hs14 code for AL6528A-169 by liudi at 20220928 start*/
#ifdef JD_ZERO_FLASH
    uint32_t addr = JADARD_TVDD_ADDR;
    uint8_t value = JADARD_TVDD_CLOSE;
#endif
/*hs14 code for AL6528A-169 by liudi at 20220928 end*/

    JD_I("%s: enter\n", __func__);
#ifdef JD_ZERO_FLASH
    ts->fw_ready = false;
#endif

#if defined(__JADARD_KMODULE__)
    JD_I("Prepare kernel fp\n");
    jd_getname_kernel = (void *)kallsyms_lookup_name("getname_kernel");
    if (!jd_getname_kernel) {
        JD_E("prepare jd_getname_kernel failed!\n");
    }

    jd_file_open_name = (void *)kallsyms_lookup_name("file_open_name");
    if (!jd_file_open_name) {
        JD_E("prepare jd_file_open_name failed!\n");
        return JD_CHECK_DATA_ERROR;
    }
#endif

    pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
    if (pdata == NULL) {
        err = -ENOMEM;
        goto err_platform_data_fail;
    }

    pjadard_ic_data = kzalloc(sizeof(*pjadard_ic_data), GFP_KERNEL);
    if (pjadard_ic_data == NULL) {
        err = -ENOMEM;
        goto err_ic_data_fail;
    }

    pjadard_report_data = kzalloc(sizeof(struct jadard_report_data), GFP_KERNEL);
    if (pjadard_report_data == NULL) {
        err = -ENOMEM;
        goto err_alloc_touch_data_fail;
    }

    if (jadard_parse_dt(ts, pdata) < 0) {
        JD_I(" pdata is NULL\n");
        goto err_alloc_pdata_fail;
    }

#ifdef JD_RST_PIN_FUNC
    ts->rst_gpio = pdata->gpio_reset;
#endif
    jadard_gpio_power_config(pdata);
    ts->pdata = pdata;

    while (ptr != NULL) {
        g_module_fp.fp_chip_init = ptr->chip_init;
        g_module_fp.fp_chip_detect = ptr->chip_detect;

        if (g_module_fp.fp_chip_detect != NULL) {
            if (g_module_fp.fp_chip_detect() != false) {
                if (g_module_fp.fp_chip_init != NULL) {
                    g_module_fp.fp_chip_init();

                    /* Free jadard_support_chip memory */
                    del_ptr = g_module_fp.head_support_chip;
                    while (del_ptr != NULL) {
                        g_module_fp.head_support_chip = del_ptr;
                        del_ptr = del_ptr->next;
                        kfree(g_module_fp.head_support_chip);
                    }
                    g_module_fp.head_support_chip = NULL;
                    break;
                }
            }
        }

        ptr = ptr->next;
    }

    if (ptr == NULL) {
        JD_E("%s: Could not find Jadard Chipset\n", __func__);
        goto error_ic_detect_failed;
    }

#ifdef JD_AUTO_UPGRADE_FW
    ts->jadard_upgrade_wq = create_singlethread_workqueue("JD_upgrade_reuqest");
    if (!ts->jadard_upgrade_wq) {
        JD_E(" allocate syn_upgrade_wq failed\n");
        err = -ENOMEM;
        goto err_upgrade_wq_failed;
    }

    INIT_DELAYED_WORK(&ts->work_upgrade, jadard_upgrade_process);
    queue_delayed_work(ts->jadard_upgrade_wq, &ts->work_upgrade, msecs_to_jiffies(JD_UPGRADE_DELAY_TIME));
#endif

#ifdef JD_ZERO_FLASH
    /*hs14 code for AL6528A-169 by liudi at 20220928 start*/
    g_module_fp.fp_register_write(addr, &value, 1);
    /*hs14 code for AL6528A-169 by liudi at 20220928 end*/
    ts->jadard_0f_upgrade_wq = create_singlethread_workqueue("JD_0f_update_reuqest");
    INIT_DELAYED_WORK(&ts->work_0f_upgrade, g_module_fp.fp_0f_operation);
    queue_delayed_work(ts->jadard_0f_upgrade_wq, &ts->work_0f_upgrade, msecs_to_jiffies(JD_UPGRADE_DELAY_TIME));
#endif

    g_module_fp.fp_touch_info_set();

#ifdef CONFIG_OF
    ts->pdata->abs_pressure_min = 0;
    ts->pdata->abs_pressure_max = 200;
    ts->pdata->abs_width_min    = 0;
    ts->pdata->abs_width_max    = 200;
    pdata->usb_status[0]        = 0xF0;
    pdata->usb_status[1]        = 0x00;
#endif
    ts->suspended                = false;
    ts->rawdata_little_endian    = true;
#if defined(JD_USB_DETECT_CALLBACK) || defined(JD_USB_DETECT_GLOBAL)
    ts->usb_connected = 0;
    ts->usb_status = pdata->usb_status;
#endif

#ifdef JD_PROTOCOL_A
    ts->protocol_type = PROTOCOL_TYPE_A;
#else
    ts->protocol_type = PROTOCOL_TYPE_B;
#endif
    JD_I("%s: Use Protocol Type %c\n", __func__,
        ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');
    ret = jadard_input_register(ts);
    if (ret) {
        JD_E("%s: Unable to register %s input device\n",
            __func__, ts->input_dev->name);
        goto err_input_register_device_failed;
    }

    /*hs14 code for AL6528A-338 by liudi at 20221018 start*/
    pjadard_ts_data->earphone_notify.notifier_call = jadard_earphone_notifier_callback;
    ret = headset_notifier_register(&pjadard_ts_data->earphone_notify);
    if (ret) {
        JD_E(" Unable to register register notifier: %d\n", ret);
    }
    /*hs14 code for AL6528A-338 by liudi at 20221018 end*/
#ifdef JD_CONFIG_FB
    ts->jadard_fb_wq = create_singlethread_workqueue("JD_FB_reuqest");
    if (!ts->jadard_fb_wq) {
        JD_E(" allocate syn_fb_wq failed\n");
        err = -ENOMEM;
        goto err_get_intr_bit_failed;
    }

    INIT_DELAYED_WORK(&ts->work_fb, jadard_fb_register);
    queue_delayed_work(ts->jadard_fb_wq, &ts->work_fb, msecs_to_jiffies(JD_FB_DELAY_TIME));
#endif

#ifdef JD_SMART_WAKEUP
    ts->gesture_cust_en[0] = true;
    ts->SMWP_enable = false;
    ts->ts_SMWP_wake_lock = wakeup_source_register(ts->dev, JADARD_common_NAME);
#endif

    err = jadard_report_data_init();
    if (err) {
        goto err_report_data_init_failed;
    }

    if (jadard_common_proc_init()) {
        JD_E(" %s: jadard_common proc_init failed!\n", __func__);
        goto err_creat_proc_file_failed;
    }

#if defined(JD_USB_DETECT_CALLBACK)
    if (ts->usb_status) {
        ts->jadard_usb_detect_wq = create_singlethread_workqueue("JD_usb_detect_reuqest");
        INIT_DELAYED_WORK(&ts->work_usb_detect, jadard_charger_init);
        queue_delayed_work(ts->jadard_usb_detect_wq, &ts->work_usb_detect, msecs_to_jiffies(JD_USB_DETECT_DELAY_TIME));
    }
#endif

    err = jadard_ts_register_interrupt();
    if (err) {
        goto err_register_interrupt_failed;
    }

#if defined(JD_CONFIG_NODE)
    err = jadard_ts_filesys_create(ts);
    if (err) {
        goto err_sysfs_init_failed;
    }
#endif

#if defined(JD_CONFIG_SPRD_DRM)
    ts->jadard_resume_wq = create_singlethread_workqueue("jadard_resume_wq");
    INIT_WORK(&ts->jadard_resume_work, jadard_resume_work_function);

    ts->drm_notify.notifier_call = jadard_drm_notifier_callback;
    ret = disp_notifier_register(&ts->drm_notify);
    if (ret < 0) {
        JD_E("Failed to register");
        goto err_register_drm_notif_failed;
    }
#endif

#ifdef JD_ZERO_FLASH
    /* Close ATTN before fw upgrade ready */
    jadard_int_enable(false);
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
    if (jadard_debug_init())
        JD_E(" %s: debug initial failed!\n", __func__);
#endif

    /*hs14 code for SR-AL6528A-01-525 by liudi at 20220908 start*/
    jd_extra_proc_init();
    /*hs14 code for SR-AL6528A-01-525 by liudi at 20220908 end*/

    /*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 start*/
    platform_device_register(&hwinfo_device);
    jd_test_node_init(&hwinfo_device);
    /*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 end*/

    /*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 start*/
    err = sec_cmd_init(&ts->sec, jadard_commands, jadard_get_array_size(), SEC_CLASS_DEVT_TSP);
    if (err < 0) {
        JD_E("%s: Failed to sec_cmd_init\n", __func__);
    } else {
        JD_I("%s: sec_cmd_init success!\n", __func__);
    }
    /*hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 end*/

/*hs14 code for SR-AL6528A-01-483 by hehaoran5 at 20220913 start*/
#ifdef HQ_D85_BUILD
    jadard_enable_gesture_wakeup();
#endif
/*hs14 code for SR-AL6528A-01-483 by hehaoran5 at 20220913 end*/

    /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 start*/
    ret = sysfs_create_link(&ts->sec.fac_dev->kobj,&ts->input_dev->dev.kobj, "input");
    if (ret < 0) {
        JD_E("create enable node fail\n");
    }
    /*hs14 code for SR-AL6528A-01-488 by liudi at 20220914 end*/
    return 0;

#if defined(JD_CONFIG_SPRD_DRM)
err_register_drm_notif_failed:
#endif
#if defined(JD_CONFIG_NODE)
#if defined(JD_CONFIG_SPRD_DRM)
    jadard_ts_filesys_remove(ts);
#endif
err_sysfs_init_failed:
#endif
err_register_interrupt_failed:
    remove_proc_entry(JADARD_PROC_TOUCH_FOLDER, NULL);
#if defined(JD_OPPO_FUNC)
    remove_proc_entry(JADARD_PROC_TOUCHPANLE_FOLDER, NULL);
#endif
err_creat_proc_file_failed:
err_report_data_init_failed:
#ifdef JD_SMART_WAKEUP
    wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#endif
#ifdef JD_CONFIG_FB
    cancel_delayed_work_sync(&ts->work_fb);
    destroy_workqueue(ts->jadard_fb_wq);
err_get_intr_bit_failed:
#endif
err_input_register_device_failed:
    input_free_device(ts->input_dev);

#ifdef JD_ZERO_FLASH
    cancel_delayed_work_sync(&ts->work_0f_upgrade);
    destroy_workqueue(ts->jadard_0f_upgrade_wq);
#endif

#ifdef JD_AUTO_UPGRADE_FW
    cancel_delayed_work_sync(&ts->work_upgrade);
    destroy_workqueue(ts->jadard_upgrade_wq);
err_upgrade_wq_failed:
#endif
error_ic_detect_failed:
    if (gpio_is_valid(pdata->gpio_irq))
        gpio_free(pdata->gpio_irq);
#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset))
        gpio_free(pdata->gpio_reset);
#endif
#ifndef CONFIG_OF
err_power_failed:
#endif
    jadard_gpio_power_deconfig(pdata);
err_alloc_pdata_fail:
    kfree(pjadard_report_data);
err_alloc_touch_data_fail:
    kfree(pjadard_ic_data);
err_ic_data_fail:
    kfree(pdata);
err_platform_data_fail:
    kfree(ts);

    return err;
}

void jadard_chip_common_deinit(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

#if defined(JD_CONFIG_SPRD_DRM)
    disp_notifier_unregister(&ts->drm_notify);
#endif

#if defined(JD_CONFIG_NODE)
    jadard_ts_filesys_remove(ts);
#endif

#ifdef CONFIG_TOUCHSCREEN_JADARD_DEBUG
    jadard_debug_remove();
#endif

    remove_proc_entry(JADARD_PROC_TOUCH_FOLDER, NULL);
#if defined(JD_OPPO_FUNC)
    remove_proc_entry(JADARD_PROC_TOUCHPANLE_FOLDER, NULL);
#endif

    jadard_common_proc_deinit();

#ifdef JD_SMART_WAKEUP
    wakeup_source_unregister(ts->ts_SMWP_wake_lock);
#endif

#ifdef JD_CONFIG_FB
    /*hs14 code for AL6528A-213 by hehaoran5 at 20221002 start*/
    unregister_esd_tp_recovery_notifier(&ts->esd_recovery_notifier);
    /*hs14 code for AL6528A-213 by hehaoran5 at 20221002 end*/
    if (fb_unregister_client(&ts->fb_notif))
        JD_E("Error occurred while unregistering fb_notifier.\n");
    cancel_delayed_work_sync(&ts->work_fb);
    destroy_workqueue(ts->jadard_fb_wq);
#endif
    /*hs14 code for AL6528A-338 by liudi at 20221018 start*/
    headset_notifier_unregister(&pjadard_ts_data->earphone_notify);
    /*hs14 code for AL6528A-338 by liudi at 20221018 end*/

    input_free_device(ts->input_dev);

#ifdef JD_ZERO_FLASH
    cancel_delayed_work_sync(&ts->work_0f_upgrade);
    destroy_workqueue(ts->jadard_0f_upgrade_wq);
#endif

#ifdef JD_AUTO_UPGRADE_FW
    cancel_delayed_work_sync(&ts->work_upgrade);
    destroy_workqueue(ts->jadard_upgrade_wq);
#endif

#ifdef JD_USB_DETECT_CALLBACK
    cancel_delayed_work_sync(&ts->work_usb_detect);
    destroy_workqueue(ts->jadard_usb_detect_wq);
#endif

    jadard_gpio_power_deconfig(ts->pdata);

    if (gpio_is_valid(ts->pdata->gpio_irq))
        gpio_free(ts->pdata->gpio_irq);

#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(ts->pdata->gpio_reset))
        gpio_free(ts->pdata->gpio_reset);
#endif

    if (pjadard_host_data->id != NULL) {
        kfree(pjadard_host_data->id);
        pjadard_host_data->id = NULL;
    }

    if (pjadard_host_data->x != NULL) {
        kfree(pjadard_host_data->x);
        pjadard_host_data->x = NULL;
    }

    if (pjadard_host_data->y != NULL) {
        kfree(pjadard_host_data->y);
        pjadard_host_data->y = NULL;
    }

    if (pjadard_host_data->w != NULL) {
        kfree(pjadard_host_data->w);
        pjadard_host_data->w = NULL;
    }

    if (pjadard_host_data->event != NULL) {
        kfree(pjadard_host_data->event);
        pjadard_host_data->event = NULL;
    }

    if (pjadard_host_data != NULL) {
        kfree(pjadard_host_data);
        pjadard_host_data = NULL;
    }

    if (pjadard_report_data->touch_coord_info != NULL) {
        kfree(pjadard_report_data->touch_coord_info);
        pjadard_report_data->touch_coord_info = NULL;
    }

    kfree(pjadard_report_data);
    kfree(pjadard_ic_data);
    kfree(ts->pdata);
    kfree(ts);
}

int jadard_chip_common_suspend(struct jadard_ts_data *ts)
{
    bool cancel_diag_thread = true;

    if (ts->suspended) {
        JD_I("%s: Already suspended, Skipped\n", __func__);
        return 0;
    } else {
        JD_I("%s: enter\n", __func__);
        ts->suspended = true;
    }

/*hs14 code for AL6528A-425 by liudi at 20221025 start*/
#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL) || defined(JD_USB_DETECT_CALLBACK) ||\
    defined(JD_HIGH_SENSITIVITY) || defined(JD_ROTATE_BORDER) || defined(JD_EARPHONE_DETECT)
/*hs14 code for AL6528A-425 by liudi at 20221025 end*/
    g_module_fp.fp_resume_set_func(ts->suspended);
#endif

#if defined(JD_SMART_WAKEUP)
    cancel_diag_thread = !ts->SMWP_enable;
#endif
    /* Cancel mutual data thread, if exist */
    if (ts->diag_thread_active && cancel_diag_thread) {
        ts->diag_thread_active = false;
        cancel_delayed_work_sync(&ts->jadard_diag_delay_wrok);
    }

    if ((pjadard_debug != NULL) && (*pjadard_debug->fw_dump_going)) {
        JD_I("%s: Flash dump is going, reject suspend\n", __func__);
        return 0;
    }

#ifdef JD_SMART_WAKEUP
    if (ts->SMWP_enable) {
        atomic_set(&ts->suspend_mode, 1);
        irq_set_irq_wake(ts->jd_irq , 1);
        JD_I("[jadard] %s: SMART_WAKEUP enable\n", __func__);
        return 0;
    }
#endif

    jadard_int_enable(false);
    atomic_set(&ts->suspend_mode, 1);

    JD_I("%s: end\n", __func__);
    return 0;
}

int jadard_chip_common_resume(struct jadard_ts_data *ts)
{
#ifdef JD_ZERO_FLASH
    bool skip_fw_upgrade = false;
#endif

    if (!ts->suspended) {
        JD_I("%s: Already resumed, Skipped\n", __func__);
        return 0;
    } else {
        JD_I("%s: enter\n", __func__);
        ts->suspended = false;
    }

    atomic_set(&ts->suspend_mode, 0);

#if defined(JD_SMART_WAKEUP)
    if (ts->SMWP_enable) {
        irq_set_irq_wake(ts->jd_irq , 0);
    }
#endif

#ifdef JD_ZERO_FLASH
    JD_I("Upgrade fw in zero flash mode\n");
#if defined(JD_SMART_WAKEUP)
    skip_fw_upgrade = ts->SMWP_enable;
#endif
    if (ts->diag_thread_active && skip_fw_upgrade) {
        JD_I("Diag thread is active! Skip Update with zero flash\n");
    } else if (g_module_fp.fp_0f_upgrade_fw(jd_i_CTPM_firmware_name) < 0) {
        JD_E("Something is wrong! Skip Update with zero flash\n");
        return 0;
    }
#endif

    jadard_report_all_leave_event(ts);

/*hs14 code for AL6528A-425 by liudi at 20221025 start*/
#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL) || defined(JD_USB_DETECT_CALLBACK) ||\
    defined(JD_HIGH_SENSITIVITY) || defined(JD_ROTATE_BORDER) || defined(JD_EARPHONE_DETECT)
/*hs14 code for AL6528A-425 by liudi at 20221025 end*/
    g_module_fp.fp_resume_set_func(ts->suspended);
#endif

    jadard_int_enable(true);
    /*hs14 code for SR-AL6528A-01-464 by hehaoran5 at 202201008 start*/
    if (pjadard_ic_data != NULL) {
        JD_I("IC_NAME:JD9522T_XX_BOE TOUCH_VER:%02x\n", pjadard_ic_data->fw_cid_ver & 0xFF);
    }
    /*hs14 code for SR-AL6528A-01-464 by hehaoran5 at 202201008 end*/
    JD_I("%s: end \n", __func__);
    return 0;
}
