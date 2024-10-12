#ifndef JADARD_MODULE_H
#define JADARD_MODULE_H

#define JD_ONE_SIZE       1
#define JD_TWO_SIZE       2
#define JD_THREE_SIZE  3
#define JD_FOUR_SIZE   4
#define JD_FIVE_SIZE   5
#define JD_SIX_SIZE       6
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
    uint8_t (*fp_dd_register_read)(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
    uint8_t (*fp_dd_register_write)(uint8_t cmd, uint8_t *par, uint8_t par_len);

    uint8_t    (*fp_DbicReg_write)(uint8_t cmd, uint8_t *par, uint8_t par_len);
    uint8_t    (*fp_DbicReg_read)(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
    void (*fp_set_sleep_mode)(uint8_t *value, uint8_t value_len);

    int    (*fp_dd_reg_read)(uint8_t page, uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
    int    (*fp_dd_reg_write)(uint8_t page, uint8_t cmd, uint8_t *par, uint8_t par_len);
    int    (*fp_dd_reg_std_cmd_read)(uint8_t cmd, uint8_t *rpar, uint8_t rpar_len);
    int    (*fp_dd_reg_std_cmd_write)(uint8_t cmd, uint8_t *par, uint8_t par_len, uint8_t only_cmd);

    void (*fp_read_fw_ver)(void);
    void (*fp_mutual_data_set)(uint8_t data_type);
    bool (*fp_get_touch_data)(uint8_t *buf, uint8_t length);
    void (*fp_report_points)(struct jadard_ts_data *ts);
    int (*fp_parse_report_data)(struct jadard_ts_data *ts, int irq_event, int ts_status);
    int (*fp_distribute_touch_data)(struct jadard_ts_data *ts, uint8_t *buf, int irq_event, int ts_status);

    int (*fp_flash_read)(uint32_t start_addr, uint8_t *rdata, uint32_t rlen);
    int (*fp_flash_write)(uint32_t start_addr, uint8_t *wdata, uint32_t wlen);
    int (*fp_flash_erase)(void);

    int (*fp_get_mutual_data)(uint8_t data_type, uint8_t *rdata, uint16_t rlen);

    bool (*fp_chip_detect)(void);
    void (*fp_chip_init)(void);
    void (*fp_EnterBackDoor)(void);
    void (*fp_ExitBackDoor)(void);
    void (*fp_pin_reset)(void);
    void (*fp_soft_reset)(void);
    void (*fp_ic_reset)(bool reload_cfg, bool int_off_on);
    void (*fp_ic_soft_reset)(void);
    void (*fp_report_data_reinit)(void);

    void (*fp_touch_info_set)(void);
    void (*fp_usb_detect_set)(uint8_t *usb_status);
    uint8_t (*fp_get_freq_band)(void);
    void (*fp_log_touch_state)(void);
    void (*fp_resume_set_func)(bool suspended);

    void (*fp_set_high_sensitivity)(bool enable);
    void (*fp_set_rotate_border)(uint16_t rotate);
    void (*fp_set_earphone_enable)(bool enable);
    void (*fp_set_SMWP_enable)(bool SMWP_enable);
    int (*fp_sorting_test)(void);

#ifdef JD_AUTO_UPGRADE_FW
    int (*fp_read_fw_ver_bin)(void);
#endif

    int (*fp_ram_read)(uint32_t start_addr, uint8_t *rdata, uint32_t rlen);
#ifdef JD_ZERO_FLASH
    int (*fp_ram_write)(uint32_t start_addr, uint8_t *wdata, uint32_t wlen);
    int (*fp_0f_upgrade_fw)(char *file_name);
    void (*fp_0f_operation)(struct work_struct *work);
#endif
#ifdef CONFIG_TOUCHSCREEN_JADARD_SORTING
    void (*fp_APP_SetSortingMode)(uint8_t *value, uint8_t value_len);
    void (*fp_APP_ReadSortingMode)(uint8_t *pValue, uint8_t pValue_len);
    void (*fp_APP_GetLcdSleep)(uint8_t *pStatus);
    void (*fp_APP_SetSortingSkipFrame)(uint8_t value);
    void (*fp_APP_SetSortingKeepFrame)(uint8_t value);
    bool (*fp_APP_ReadSortingBusyStatus)(uint8_t mpap_handshake_finish);
    void (*fp_GetSortingDiffData)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_GetSortingDiffDataMax)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_GetSortingDiffDataMin)(uint8_t *pValue, uint16_t pValue_len);
    void (*fp_Fw_DBIC_Off)(void);
    void (*fp_Fw_DBIC_On)(void);
    void (*fp_StartMCU)(void);
#endif
};

void jadard_mcu_cmd_struct_init(void);

#endif
