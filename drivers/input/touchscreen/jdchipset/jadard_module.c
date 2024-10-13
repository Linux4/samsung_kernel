#include "jadard_common.h"
#include "jadard_platform.h"
#include "jadard_module.h"

extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;
struct jadard_common_variable g_common_variable;

#if defined(JD_AUTO_UPGRADE_FW) || defined(JD_ZERO_FLASH)
#ifdef JD_UPGRADE_FW_ARRAY
extern uint8_t jd_i_firmware[];
extern uint32_t jd_fw_size;
#endif
extern char *jd_i_CTPM_firmware_name;
#if defined(JD_ZERO_FLASH)
bool jd_g_f_0f_update = false;
#endif
#endif

const char *jadard_bit_map[16] = {
    [ 0] = "0000", [ 1] = "0001", [ 2] = "0010", [ 3] = "0011",
    [ 4] = "0100", [ 5] = "0101", [ 6] = "0110", [ 7] = "0111",
    [ 8] = "1000", [ 9] = "1001", [10] = "1010", [11] = "1011",
    [12] = "1100", [13] = "1101", [14] = "1110", [15] = "1111",
};

static int jadard_mcu_register_read(uint32_t ReadAddr, uint8_t *ReadData, uint32_t ReadLen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static int jadard_mcu_register_write(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static uint8_t jadard_mcu_dd_register_write(uint8_t page, uint8_t cmd, uint8_t *par, uint8_t par_len)
{
    uint8_t page_en;

    if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_1) {
        return jadard_DbiWriteDDReg(page, cmd, par, par_len);
    } else {
        page_en = g_module_fp.fp_ReadDbicPageEn();
        if (page_en != 0xFF) {
            if ((cmd == 0xDE) && (page_en > 0)) {
                g_module_fp.fp_SetDbicPage(par[0]);
                return JD_DBIC_READ_WRITE_SUCCESS;
            } else {
                return jadard_DbicWriteDDReg(cmd, par, par_len);
            }
        } else {
            return jadard_DbicWriteDDReg(cmd, par, par_len);
        }
    }
}

static uint8_t jadard_mcu_dd_register_read(uint8_t page, uint8_t cmd, uint8_t *par, uint8_t par_len, uint32_t offset)
{
    if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_1) {
        return jadard_DbiReadDDReg(page, cmd, par, par_len, offset);
    } else {
        return jadard_DbicReadDDReg(cmd, par, par_len);
    }
}

static void jadard_mcu_set_sleep_mode(uint8_t *value, uint8_t value_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_read_fw_ver(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_mutual_data_set(uint8_t data_type)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static int jadard_mcu_get_mutual_data(uint8_t data_type, uint8_t *rdata, uint16_t rlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static bool jadard_mcu_get_touch_data(uint8_t *buf, uint8_t length)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return false;
}

static int jadard_mcu_read_mutual_data(uint8_t *rdata, uint16_t rlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return false;
}

static void jadard_mcu_report_points(struct jadard_ts_data *ts)
{
    jadard_report_points(ts);
}

static int jadard_mcu_parse_report_data(struct jadard_ts_data *ts, int irq_event, int ts_status)
{
    return jadard_parse_report_data(ts, irq_event, ts_status);
}

static int jadard_mcu_distribute_touch_data(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status)
{
    return jadard_distribute_touch_data(ts, buf, irq_event, ts_status);
}

static int jadard_mcu_flash_read(uint32_t start_addr, uint8_t *rdata, uint32_t rlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static int jadard_mcu_flash_write(uint32_t start_addr, uint8_t *wdata, uint32_t wlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static int jadard_mcu_flash_erase(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static void jadard_mcu_EnterBackDoor(void)
{
    JD_I("%s: not support enter backdoor cmd\n", __func__);
}

static void jadard_mcu_ExitBackDoor(void)
{
    JD_I("%s: not support exit backdoor cmd\n", __func__);
}

static void jadard_mcu_PinReset(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_SoftReset(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_ResetMCU(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_PorInit(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

#ifdef JD_RST_PIN_FUNC
/*
 * Parameter signification:
 * reloadcfg int_off_on
 *   true      true : Reload config & HW reset & INT off->on
 *   true      false: Reload config & HW reset
 *   false     true : HW reset & INT off->on
 *   false     false: HW reset
*/
static void jadard_mcu_ic_reset(bool reload_cfg, bool int_off_on)
{
    JD_I("%s: reload_cfg = %d, int_off_on = %d\n", __func__, reload_cfg, int_off_on);
    pjadard_ts_data->rst_active = true;

    if (pjadard_ts_data->rst_gpio >= 0) {
        if (int_off_on) {
            jadard_int_enable(false);
        }

        g_module_fp.fp_pin_reset();

        if (reload_cfg) {
            g_module_fp.fp_report_data_reinit();
        }

        if (int_off_on) {
            jadard_int_enable(true);
        }
    }
}
#endif

static void jadard_mcu_ic_soft_reset(void)
{
    pjadard_ts_data->rst_active = true;
    jadard_int_enable(false);
    g_module_fp.fp_soft_reset();
    jadard_int_enable(true);
}

static void jadard_mcu_touch_info_set(void)
{
    pjadard_ic_data->JD_STYLUS_ID_EN = 0;
    pjadard_ic_data->JD_STYLUS_RATIO = 1;

    /* Data set by jadard_parse_dt() */
    JD_I("%s: JD_X_NUM = %d, JD_Y_NUM = %d, JD_MAX_PT = %d\n", __func__,
        pjadard_ic_data->JD_X_NUM, pjadard_ic_data->JD_Y_NUM, pjadard_ic_data->JD_MAX_PT);
    JD_I("%s: JD_X_RES = %d, JD_Y_RES = %d\n", __func__, pjadard_ic_data->JD_X_RES, pjadard_ic_data->JD_Y_RES);
    JD_I("%s: JD_INT_EDGE = %d\n", __func__, pjadard_ic_data->JD_INT_EDGE);
    JD_I("%s: JD_STYLUS_EN = %d, JD_STYLUS_ID_EN = %d, JD_STYLUS_RATIO = %d\n", __func__,
        pjadard_ic_data->JD_STYLUS_EN, pjadard_ic_data->JD_STYLUS_ID_EN, pjadard_ic_data->JD_STYLUS_RATIO);
}

static void jadard_mcu_report_data_reinit(void)
{
    if (jadard_report_data_init()) {
        JD_E("%s: allocate data fail\n", __func__);
    }
}

static void jadard_mcu_usb_detect_set(uint8_t *usb_status)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static uint8_t jadard_mcu_get_freq_band(void)
{
    JD_I("%s: not support get freq band cmd\n", __func__);
    return 0xFF;
}

static uint8_t jadard_mcu_ReadDbicPageEn(void)
{
    JD_I("%s: not support read dbic page\n", __func__);
    return 0xFF;
}

static int jadard_mcu_SetDbicPage(uint8_t page)
{
    JD_I("%s: not support set dbic page\n", __func__);
    return JD_NO_ERR;
}

static void jadard_mcu_log_touch_state(void)
{
    jadard_log_touch_state(jadard_bit_map);
}

#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL) || defined(JD_USB_DETECT_CALLBACK) ||\
    defined(JD_HIGH_SENSITIVITY) || defined(JD_ROTATE_BORDER) || defined(JD_EARPHONE_DETECT)
static void jadard_mcu_resume_set_func(bool suspended)
{
#ifdef JD_SMART_WAKEUP
    g_module_fp.fp_set_SMWP_enable(pjadard_ts_data->SMWP_enable);
#endif
#ifdef JD_USB_DETECT_GLOBAL
    jadard_cable_detect(true);
#endif
#ifdef JD_USB_DETECT_CALLBACK
    jadard_usb_status(pjadard_ts_data->usb_connected, true);
#endif
#ifdef JD_HIGH_SENSITIVITY
    g_module_fp.fp_set_high_sensitivity(pjadard_ts_data->high_sensitivity_enable);
#endif
#ifdef JD_ROTATE_BORDER
    g_module_fp.fp_set_rotate_border(pjadard_ts_data->rotate_border);
#endif
#ifdef JD_EARPHONE_DETECT
    g_module_fp.fp_set_earphone_enable(pjadard_ts_data->earphone_enable);
#endif
}
#endif

static void jadard_mcu_set_high_sensitivity(bool enable)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_set_rotate_border(uint16_t rotate)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

#ifdef JD_EARPHONE_DETECT
static void jadard_mcu_set_earphone_enable(uint8_t status)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}
#endif

static void jadard_mcu_set_SMWP_enable(bool enable)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_set_virtual_proximity(bool enable)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

#if defined(JD_AUTO_UPGRADE_FW)
static int jadard_mcu_read_fw_ver_bin(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}
#endif

static int jadard_mcu_ram_read(uint32_t start_addr, uint8_t *rdata, uint32_t rlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

#ifdef JD_ZERO_FLASH
static int jadard_mcu_ram_write(uint32_t start_addr, uint8_t *wdata, uint32_t wlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}

static int jadard_mcu_0f_upgrade_fw(char *file_name)
{
    int err = JD_NO_ERR;
#ifdef JD_UPGRADE_FW_ARRAY
    const uint8_t *fw_data = jd_i_firmware;
    JD_I("file name = %s\n", jd_i_CTPM_firmware_name);
#else
    int RetryCnt;
    const struct firmware *fw = NULL;

    JD_I("file name = %s\n", file_name);

    for (RetryCnt = 0; RetryCnt < JD_UPGRADE_FW_RETRY_TIME; RetryCnt++) {
        err = request_firmware(&fw, file_name, pjadard_ts_data->dev);
        if (err < 0) {
            JD_E("%s: Open file fail(ret:%d), RetryCnt = %d\n", __func__, err, RetryCnt);
            mdelay(1000);
        } else {
            break;
        }
    }

    if (RetryCnt == JD_UPGRADE_FW_RETRY_TIME) {
        JD_E("%s: Open file fail retry over %d\n", __func__, JD_UPGRADE_FW_RETRY_TIME);
        return JD_FILE_OPEN_FAIL;
    }
#endif
    if (jd_g_f_0f_update) {
        JD_W("%s: Other thread is upgrade now\n", __func__);
        err = JD_UPGRADE_CONFLICT;
    } else {
        JD_I("%s: Entering upgrade Flow\n", __func__);
        jadard_int_enable(false);
        jd_g_f_0f_update = true;

#ifdef JD_UPGRADE_FW_ARRAY
        JD_I("FW size = %d\n", (int)jd_fw_size);
        err = g_module_fp.fp_ram_write(0, (uint8_t *)fw_data, jd_fw_size);
#else
        JD_I("FW size = %d\n", (int)fw->size);
        err = g_module_fp.fp_ram_write(0, (uint8_t *)fw->data, fw->size);
        release_firmware(fw);
#endif
        jd_g_f_0f_update = false;

        if (err >= 0) {
            pjadard_ts_data->fw_ready = true;
        } else {
            pjadard_ts_data->fw_ready = false;
        }
    }

    return err;
}

#ifdef JD_ESD_CHECK
static int jadard_mcu_0f_esd_upgrade_fw(char *file_name)
{
    int err = JD_NO_ERR;
#ifdef JD_UPGRADE_FW_ARRAY
    const uint8_t *fw_data = jd_i_firmware;
    JD_I("file name = %s\n", jd_i_CTPM_firmware_name);
#else
    int RetryCnt;
    const struct firmware *fw = NULL;

    JD_I("file name = %s\n", file_name);

    for (RetryCnt = 0; RetryCnt < JD_UPGRADE_FW_RETRY_TIME; RetryCnt++) {
        err = request_firmware(&fw, file_name, pjadard_ts_data->dev);
        if (err < 0) {
            JD_E("%s: Open file fail(ret:%d), RetryCnt = %d\n", __func__, err, RetryCnt);
            mdelay(1000);
        } else {
            break;
        }
    }

    if (RetryCnt == JD_UPGRADE_FW_RETRY_TIME) {
        JD_E("%s: Open file fail retry over %d\n", __func__, JD_UPGRADE_FW_RETRY_TIME);
        return JD_FILE_OPEN_FAIL;
    }
#endif
    if (jd_g_f_0f_update) {
        JD_W("%s: Other thread is upgrade now\n", __func__);
        err = JD_UPGRADE_CONFLICT;
    } else {
        JD_I("%s: Entering upgrade Flow\n", __func__);
        jd_g_f_0f_update = true;

#ifdef JD_UPGRADE_FW_ARRAY
        JD_I("FW size = %d\n", (int)jd_fw_size);
        err = g_module_fp.fp_esd_ram_write(0, (uint8_t *)fw_data, jd_fw_size);
#else
        JD_I("FW size = %d\n", (int)fw->size);
        err = g_module_fp.fp_esd_ram_write(0, (uint8_t *)fw->data, fw->size);
        release_firmware(fw);
#endif
        jd_g_f_0f_update = false;

        if ((err >= 0) || (err == JD_PRAM_CRC_PASS)) {
            pjadard_ts_data->fw_ready = true;
        }
    }

    return err;
}

static int jadard_mcu_esd_ram_write(uint32_t start_addr, uint8_t *wdata, uint32_t wlen)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return JD_NO_ERR;
}
#endif

static void jadard_mcu_0f_operation(struct work_struct *work)
{
    int err = g_module_fp.fp_0f_upgrade_fw(jd_i_CTPM_firmware_name);

    if (err >= 0) {
        g_module_fp.fp_read_fw_ver();
        jadard_int_enable(true);
    }
}
#endif

static int jadard_mcu_sorting_test(void)
{
    JD_I("%s: not support ITO test\n", __func__);
    return JD_NO_ERR;
}

#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
static void jadard_mcu_APP_SetSortingMode(uint8_t *value, uint8_t value_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_APP_ReadSortingMode(uint8_t *pValue, uint8_t pValue_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_APP_GetLcdSleep(uint8_t *pStatus)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_APP_SetSortingSkipFrame(uint8_t value)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_APP_SetSortingKeepFrame(uint8_t value)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static bool jadard_mcu_APP_ReadSortingBusyStatus(uint8_t mpap_handshake_finish, uint8_t *pStatus)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
    return false;
}

static void jadard_mcu_GetSortingDiffData(uint8_t *pValue, uint16_t pValue_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_GetSortingDiffDataMax(uint8_t *pValue, uint16_t pValue_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_GetSortingDiffDataMin(uint8_t *pValue, uint16_t pValue_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_Fw_DBIC_Off(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_Fw_DBIC_On(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_StartMCU(void)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_SetMpBypassMain(void)
{
    JD_D("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_ClearMpBypassMain(void)
{
    JD_D("%s: nothing to do, only for function pointer initial\n", __func__);
}

static void jadard_mcu_ReadMpapErrorMsg(uint8_t *pValue, uint8_t pValue_len)
{
    JD_I("%s: nothing to do, only for function pointer initial\n", __func__);
}
#endif

static void jadard_mcu_fp_init(void)
{
    g_module_fp.fp_register_read               = jadard_mcu_register_read;
    g_module_fp.fp_register_write              = jadard_mcu_register_write;
    g_module_fp.fp_dd_register_read            = jadard_mcu_dd_register_read;
    g_module_fp.fp_dd_register_write           = jadard_mcu_dd_register_write;
    g_module_fp.fp_set_sleep_mode              = jadard_mcu_set_sleep_mode;
    g_module_fp.fp_read_fw_ver                 = jadard_mcu_read_fw_ver;
    g_module_fp.fp_mutual_data_set             = jadard_mcu_mutual_data_set;
    g_module_fp.fp_get_mutual_data             = jadard_mcu_get_mutual_data;
    g_module_fp.fp_get_touch_data              = jadard_mcu_get_touch_data;
    g_module_fp.fp_read_mutual_data            = jadard_mcu_read_mutual_data;
    g_module_fp.fp_report_points               = jadard_mcu_report_points;
    g_module_fp.fp_parse_report_data           = jadard_mcu_parse_report_data;
    g_module_fp.fp_distribute_touch_data       = jadard_mcu_distribute_touch_data;
    g_module_fp.fp_flash_read                  = jadard_mcu_flash_read;
    g_module_fp.fp_flash_write                 = jadard_mcu_flash_write;
    g_module_fp.fp_flash_erase                 = jadard_mcu_flash_erase;
    g_module_fp.fp_EnterBackDoor               = jadard_mcu_EnterBackDoor;
    g_module_fp.fp_ExitBackDoor                = jadard_mcu_ExitBackDoor;
    g_module_fp.fp_pin_reset                   = jadard_mcu_PinReset;
    g_module_fp.fp_soft_reset                  = jadard_mcu_SoftReset;
    g_module_fp.fp_ResetMCU                    = jadard_mcu_ResetMCU;
    g_module_fp.fp_PorInit                     = jadard_mcu_PorInit;
#ifdef JD_RST_PIN_FUNC
    g_module_fp.fp_ic_reset                    = jadard_mcu_ic_reset;
#endif

    g_module_fp.fp_ic_soft_reset               = jadard_mcu_ic_soft_reset;
    g_module_fp.fp_touch_info_set              = jadard_mcu_touch_info_set;
    g_module_fp.fp_report_data_reinit          = jadard_mcu_report_data_reinit;
    g_module_fp.fp_usb_detect_set              = jadard_mcu_usb_detect_set;
    g_module_fp.fp_get_freq_band               = jadard_mcu_get_freq_band;
    g_module_fp.fp_ReadDbicPageEn              = jadard_mcu_ReadDbicPageEn;
    g_module_fp.fp_SetDbicPage                 = jadard_mcu_SetDbicPage;
    g_module_fp.fp_log_touch_state             = jadard_mcu_log_touch_state;
#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL) || defined(JD_USB_DETECT_CALLBACK) ||\
    defined(JD_HIGH_SENSITIVITY) || defined(JD_ROTATE_BORDER) || defined(JD_EARPHONE_DETECT)
    g_module_fp.fp_resume_set_func             = jadard_mcu_resume_set_func;
#endif

    g_module_fp.fp_set_high_sensitivity        = jadard_mcu_set_high_sensitivity;
    g_module_fp.fp_set_rotate_border           = jadard_mcu_set_rotate_border;
#ifdef JD_EARPHONE_DETECT
    g_module_fp.fp_set_earphone_enable         = jadard_mcu_set_earphone_enable;
#endif
    g_module_fp.fp_set_SMWP_enable             = jadard_mcu_set_SMWP_enable;
    g_module_fp.fp_set_virtual_proximity       = jadard_mcu_set_virtual_proximity;
#ifdef JD_AUTO_UPGRADE_FW
    g_module_fp.fp_read_fw_ver_bin             = jadard_mcu_read_fw_ver_bin;
#endif

    g_module_fp.fp_ram_read                    = jadard_mcu_ram_read;
#ifdef JD_ZERO_FLASH
    g_module_fp.fp_ram_write                   = jadard_mcu_ram_write;
    g_module_fp.fp_0f_upgrade_fw               = jadard_mcu_0f_upgrade_fw;
#ifdef JD_ESD_CHECK
    g_module_fp.fp_0f_esd_upgrade_fw           = jadard_mcu_0f_esd_upgrade_fw;
    g_module_fp.fp_esd_ram_write               = jadard_mcu_esd_ram_write;
#endif
    g_module_fp.fp_0f_operation                = jadard_mcu_0f_operation;
#endif

    g_module_fp.fp_sorting_test                = jadard_mcu_sorting_test;
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    g_module_fp.fp_APP_SetSortingMode          = jadard_mcu_APP_SetSortingMode;
    g_module_fp.fp_APP_ReadSortingMode         = jadard_mcu_APP_ReadSortingMode;
    g_module_fp.fp_APP_GetLcdSleep             = jadard_mcu_APP_GetLcdSleep;
    g_module_fp.fp_APP_SetSortingSkipFrame     = jadard_mcu_APP_SetSortingSkipFrame;
    g_module_fp.fp_APP_SetSortingKeepFrame     = jadard_mcu_APP_SetSortingKeepFrame;
    g_module_fp.fp_APP_ReadSortingBusyStatus   = jadard_mcu_APP_ReadSortingBusyStatus;
    g_module_fp.fp_GetSortingDiffData          = jadard_mcu_GetSortingDiffData;
    g_module_fp.fp_GetSortingDiffDataMax       = jadard_mcu_GetSortingDiffDataMax;
    g_module_fp.fp_GetSortingDiffDataMin       = jadard_mcu_GetSortingDiffDataMin;
    g_module_fp.fp_Fw_DBIC_Off                 = jadard_mcu_Fw_DBIC_Off;
    g_module_fp.fp_Fw_DBIC_On                  = jadard_mcu_Fw_DBIC_On;
    g_module_fp.fp_StartMCU                    = jadard_mcu_StartMCU;
    g_module_fp.fp_SetMpBypassMain             = jadard_mcu_SetMpBypassMain;
    g_module_fp.fp_ClearMpBypassMain           = jadard_mcu_ClearMpBypassMain;
    g_module_fp.fp_ReadMpapErrorMsg            = jadard_mcu_ReadMpapErrorMsg;
#endif
}

void jadard_mcu_cmd_struct_init(void)
{
    JD_D("%s: Entering!\n", __func__);

    jadard_mcu_fp_init();
}
