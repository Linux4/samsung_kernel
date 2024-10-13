#ifndef JADARD_MODULE_H
#define JADARD_MODULE_H

#define JD_ONE_SIZE    1
#define JD_TWO_SIZE    2
#define JD_THREE_SIZE  3
#define JD_FOUR_SIZE   4
#define JD_FIVE_SIZE   5
#define JD_SIX_SIZE    6
#define JD_SEVEN_SIZE  7
#define JD_EIGHT_SIZE  8

struct jadard_common_variable {
    uint32_t FW_SIZE;
    uint32_t RAM_LEN;
    uint8_t dbi_dd_reg_mode;
};

struct jadard_support_chip {
    void (*chip_init)(void);
    bool (*chip_detect)(void);
    struct jadard_support_chip *next;
};

struct jadard_module_fp {
    /* Support multiple chip */
    struct jadard_support_chip *head_support_chip;

    int (*fp_register_read)(uint32_t ReadAddr, uint8_t *ReadData, uint32_t ReadLen);
    int (*fp_register_write)(uint32_t WriteAddr, uint8_t *WriteData, uint32_t WriteLen);
    uint8_t (*fp_dd_register_read)(uint8_t page, uint8_t cmd, uint8_t *rpar, uint8_t rpar_len, uint32_t offset);
    uint8_t (*fp_dd_register_write)(uint8_t page, uint8_t cmd, uint8_t *par, uint8_t par_len);
    void (*fp_set_sleep_mode)(uint8_t *value, uint8_t value_len);
    void (*fp_read_fw_ver)(void);
    void (*fp_mutual_data_set)(uint8_t data_type);
    int (*fp_get_mutual_data)(uint8_t data_type, uint8_t *rdata, uint16_t rlen);
    bool (*fp_get_touch_data)(uint8_t *buf, uint8_t length);
    int (*fp_read_mutual_data)(uint8_t *rdata, uint16_t rlen);
    void (*fp_report_points)(struct jadard_ts_data *ts);
    int (*fp_parse_report_data)(struct jadard_ts_data *ts, int irq_event, int ts_status);
    int (*fp_distribute_touch_data)(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status);
    int (*fp_flash_read)(uint32_t start_addr, uint8_t *rdata, uint32_t rlen);
    int (*fp_flash_write)(uint32_t start_addr, uint8_t *wdata, uint32_t wlen);
    int (*fp_flash_erase)(void);
    void (*fp_EnterBackDoor)(void);
    void (*fp_ExitBackDoor)(void);
    void (*fp_pin_reset)(void);
    void (*fp_soft_reset)(void);
    void (*fp_ResetMCU)(void);
    void (*fp_PorInit)(void);
#ifdef JD_RST_PIN_FUNC
    void (*fp_ic_reset)(bool reload_cfg, bool int_off_on);
#endif

    void (*fp_ic_soft_reset)(void);
    void (*fp_touch_info_set)(void);
    void (*fp_report_data_reinit)(void);
    void (*fp_usb_detect_set)(uint8_t *usb_status);
    uint8_t (*fp_get_freq_band)(void);
    uint8_t (*fp_ReadDbicPageEn)(void);
    int (*fp_SetDbicPage)(uint8_t page);
    void (*fp_log_touch_state)(void);
#if defined(JD_SMART_WAKEUP) || defined(JD_USB_DETECT_GLOBAL) || defined(JD_USB_DETECT_CALLBACK) ||\
    defined(JD_HIGH_SENSITIVITY) || defined(JD_ROTATE_BORDER) || defined(JD_EARPHONE_DETECT)
    void (*fp_resume_set_func)(bool suspended);
#endif

    void (*fp_set_high_sensitivity)(bool enable);
    void (*fp_set_rotate_border)(uint16_t rotate);
#ifdef JD_EARPHONE_DETECT
    void (*fp_set_earphone_enable)(uint8_t status);
#endif
    void (*fp_set_SMWP_enable)(bool enable);
    void (*fp_set_virtual_proximity)(bool enable);
#ifdef JD_AUTO_UPGRADE_FW
    int (*fp_read_fw_ver_bin)(void);
#endif

    int (*fp_ram_read)(uint32_t start_addr, uint8_t *rdata, uint32_t rlen);
#ifdef JD_ZERO_FLASH
    int (*fp_ram_write)(uint32_t start_addr, uint8_t *wdata, uint32_t wlen);
    int (*fp_0f_upgrade_fw)(char *file_name);
#ifdef JD_ESD_CHECK
    int (*fp_0f_esd_upgrade_fw)(char *file_name);
    int (*fp_esd_ram_write)(uint32_t start_addr, uint8_t *wdata, uint32_t wlen);
#endif
    void (*fp_0f_operation)(struct work_struct *work);
#endif

    int (*fp_sorting_test)(void);
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    void (*fp_APP_SetSortingMode)(uint8_t *value, uint8_t value_len);
    void (*fp_APP_ReadSortingMode)(uint8_t *pValue, uint8_t pValue_len);
    void (*fp_APP_GetLcdSleep)(uint8_t *pStatus);
    void (*fp_APP_SetSortingSkipFrame)(uint8_t value);
    void (*fp_APP_SetSortingKeepFrame)(uint8_t value);
    bool (*fp_APP_ReadSortingBusyStatus)(uint8_t mpap_handshake_finish, uint8_t *pStatus);
    void (*fp_GetSortingDiffData)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_GetSortingDiffDataMax)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_GetSortingDiffDataMin)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_Fw_DBIC_Off)(void);
    void (*fp_Fw_DBIC_On)(void);
    void (*fp_StartMCU)(void);
    void (*fp_SetMpBypassMain)(void);
    void (*fp_ClearMpBypassMain)(void);
    void (*fp_ReadMpapErrorMsg)(uint8_t *pValue, uint8_t pValue_len);
#endif
};

void jadard_mcu_cmd_struct_init(void);

#endif
