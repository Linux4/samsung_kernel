#include "jadard_sorting.h"
extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;
extern struct jadard_common_variable g_common_variable;

void jadard_sorting_init(void);

static uint8_t LCD_Status = JD_LCD_STATUS_DISPLAY_ON;
static int SOC_Fail_flag;
static bool Fail_flag;
static int ModeEnter_Flag;
static bool Switch_Mode_Timeout_Flag;
static int PST_GetFrameOfRawData_Number;

/* {
 *    {Skip_OpenData_Frame_Number, Get_OpenData_Frame_Number},
 *    {Skip_ShortData_Frame_Number, Get_ShortData_Frame_Number},
 *    {Skip_NormalSBDev_Frame_Number, Get_NormalSBDev_Frame_Number},
 *    {Skip_NormalActiveNoise_Frame_Number, Get_NormalActiveNoise_Frame_Number},
 *    {Skip_NormalIdleData_Frame_Number, Get_NormalIdleData_Frame_Number},
 *    {Skip_NormalIdleNoise_Frame_Number, Get_NormalIdleNoise_Frame_Number},
 *    {Skip_LpwugSBDev_Frame_Number, Get_LpwugSBDev_Frame_Number},
 *    {Skip_LpwugActiveNoise_Frame_Number, Get_LpwugActiveNoise_Frame_Number},
 *    {Skip_LpwugIdleData_Frame_Number, Get_LpwugIdleData_Frame_Number},
 *    {Skip_LpwugIdleNoise_Frame_Number, Get_LpwugIdleNoise_Frame_Number}
 * }
 */
uint16_t jd_pst_global_variable[JD_SORTING_ACTIVE_ITEM][2] = {
    {3, 3},
    {3, 3},
    {3, 3},
    {3, 50},
    {3, 3},
    {3, 50},
    {3, 3},
    {3, 50},
    {3, 3},
    {3, 50}
};

static int jd_test_data_pop_out(char *rslt_buf, char *filepath)
{
    struct file *raw_file = NULL;
    struct filename *vts_name = NULL;
    mm_segment_t fs;
    loff_t pos = 0;
    int ret_val = JD_SORTING_OK;

    JD_D("%s: Entering!\n", __func__);
    JD_I("data size=0x%04X\n", (uint32_t)strlen(rslt_buf));
    JD_I("Ouput result to %s\n", filepath);
#if defined(__JADARD_KMODULE__)
    vts_name = jd_getname_kernel(filepath);
#else
    vts_name = getname_kernel(filepath);
#endif

    if (raw_file == NULL)
        raw_file = filp_open(filepath, O_TRUNC|O_CREAT|O_RDWR, 0660);

    raw_file = filp_open(filepath, O_TRUNC|O_CREAT|O_RDWR, 0660);
    if (IS_ERR(raw_file)) {
        JD_E("%s open file failed = %ld\n", __func__, PTR_ERR(raw_file));
        ret_val = -EIO;
        goto SAVE_DATA_ERR;
    }

    fs = get_fs();
    set_fs(get_ds());
    vfs_write(raw_file, rslt_buf, JD_OUTPUT_BUFFER_SIZE * JD_SORTING_ACTIVE_ITEM * sizeof(char), &pos);
    if (raw_file != NULL) {
        filp_close(raw_file, NULL);
    }
    set_fs(fs);

SAVE_DATA_ERR:
    JD_D("%s: End!\n", __func__);

    return ret_val;
}

static void jadard_sorting_data_deinit(void)
{
    int i = 0;

    if (jd_g_sorting_threshold) {
        for (i = 0; i < JD_READ_THRESHOLD_SIZE; i++) {
            if (jd_g_sorting_threshold[i]) {
                vfree(jd_g_sorting_threshold[i]);
                jd_g_sorting_threshold[i] = NULL;
            }
        }
        vfree(jd_g_sorting_threshold);
        jd_g_sorting_threshold = NULL;
        JD_I("Free the jd_g_sorting_threshold\n");
    }

    if (jd_g_file_path) {
        vfree(jd_g_file_path);
        jd_g_file_path = NULL;
    }

    if (jd_g_rslt_data) {
        vfree(jd_g_rslt_data);
        jd_g_rslt_data = NULL;
    }

    if (GET_RAWDATA) {
        vfree(GET_RAWDATA);
        GET_RAWDATA = NULL;
    }
}

static void jadard_pst_enter_normal_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_NORMAL_ACTIVE_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_NORMAL_ACTIVE_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

#if (JD_PRODUCT_TYPE == 0)
static void jadard_pst_enter_slpin_mode(void)
{
    int retry = 100;
    uint8_t par[JD_ONE_SIZE];

    JD_I("Start to Enter Sleep In\n");

    jadard_pst_enter_normal_mode();
    g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);

    if (LCD_Status != JD_LCD_STATUS_SLEEP_IN) {
        if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_0) {
        g_module_fp.fp_dd_register_write(0x28, NULL, 0);
        g_module_fp.fp_dd_register_write(0x10, NULL, 0);
        } else if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_1) {
            par[0] = 0x28;
            g_module_fp.fp_dd_reg_std_cmd_write(0x28, par, 1, 1);
            par[0] = 0x10;
            g_module_fp.fp_dd_reg_std_cmd_write(0x10, par, 1, 1);
        }
    }

    do {
        mdelay(10);
        g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);
    } while ((--retry > 0) && (LCD_Status == JD_LCD_STATUS_SLEEP_OUT));

    if (LCD_Status != JD_LCD_STATUS_SLEEP_IN) {
        JD_I("Enter Sleep In Fail\n");
    } else {
        JD_I("Enter Sleep In Finished\n");
    }
}
#else
static void jadard_pst_enter_slpin_mode_bymppw(void)
{
    int retry = 100;
    uint8_t mpap_sleep_password[JD_TWO_SIZE];

    JD_I("Start to Enter Sleep In\n");

    mpap_sleep_password[0] = JD_MPAP_PW_SLEEP_IN_hbyte;
    mpap_sleep_password[1] = JD_MPAP_PW_SLEEP_IN_lbyte;
    g_module_fp.fp_set_sleep_mode(mpap_sleep_password, sizeof(mpap_sleep_password));

    do {
        mdelay(10);
        g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);
    } while ((--retry > 0) && (LCD_Status == JD_LCD_STATUS_SLEEP_OUT));

    if (LCD_Status != JD_LCD_STATUS_SLEEP_IN) {
        JD_I("Enter Sleep In Fail\n");
    } else {
        JD_I("Enter Sleep In Finished\n");
    }

    jadard_pst_enter_normal_mode();
}
#endif

#if (JD_PRODUCT_TYPE == 0)
static void jadard_pst_enter_slpout_mode(void)
{
    int retry = 100;
    uint8_t par[JD_ONE_SIZE];

    JD_I("Start to Enter Sleep Out\n");

    jadard_pst_enter_normal_mode();
    g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);

    if (LCD_Status != JD_LCD_STATUS_SLEEP_OUT) {
        if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_0) {
            g_module_fp.fp_dd_register_write(0x11, NULL, 0);
            g_module_fp.fp_dd_register_write(0x29, NULL, 0);
        } else if (g_common_variable.dbi_dd_reg_mode == JD_DDREG_MODE_1) {
            par[0] = 0x11;
            g_module_fp.fp_dd_reg_std_cmd_write(0x11, par, 1, 1);
            par[0] = 0x29;
            g_module_fp.fp_dd_reg_std_cmd_write(0x29, par, 1, 1);
        }
    }

    do {
        mdelay(10);
        g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);
    } while ((--retry > 0) && (LCD_Status == JD_LCD_STATUS_SLEEP_IN));

    if (LCD_Status != JD_LCD_STATUS_SLEEP_OUT) {
        JD_I("Enter Sleep Out Fail\n");
    } else {
        JD_I("Enter Sleep Out Finished\n");
    }
}
#else
static void jadard_pst_enter_slpout_mode_bymppw(void)
{
    int retry = 100;
    uint8_t mpap_sleep_password[JD_TWO_SIZE];

    JD_I("Start to Enter Sleep Out\n");

    mpap_sleep_password[0] = JD_MPAP_PW_SLEEP_OUT_hbyte;
    mpap_sleep_password[1] = JD_MPAP_PW_SLEEP_OUT_lbyte;
    g_module_fp.fp_set_sleep_mode(mpap_sleep_password, sizeof(mpap_sleep_password));

    do {
        mdelay(10);
        g_module_fp.fp_APP_GetLcdSleep(&LCD_Status);
    } while ((--retry > 0) && (LCD_Status == JD_LCD_STATUS_SLEEP_IN));

    if (LCD_Status != JD_LCD_STATUS_SLEEP_OUT) {
        JD_I("Enter Sleep Out Fail\n");
    } else {
        JD_I("Enter Sleep Out Finished\n");
    }

    jadard_pst_enter_normal_mode();
}
#endif

static void jadard_PST_Enter_LpwugIdle_Diff_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_LPWUG_IDLE_DIFF_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_LPWUG_IDLE_DIFF_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_LpwugIdle_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_LPWUG_IDLE_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_LPWUG_IDLE_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_LpwugActive_Diff_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_LPWUG_ACTIVE_DIFF_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_LPWUG_ACTIVE_DIFF_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_LpwugActive_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_LPWUG_ACTIVE_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_LPWUG_ACTIVE_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_NormalIdle_Diff_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_NORMAL_IDLE_DIFF_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_NORMAL_IDLE_DIFF_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_NormalIdle_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_NORMAL_IDLE_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_NORMAL_IDLE_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_NormalActive_Diff_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_NORMAL_ACTIVE_DIFF_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_NORMAL_ACTIVE_DIFF_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_PST_Enter_NormalActive_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_NORMAL_ACTIVE_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_NORMAL_ACTIVE_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_pst_enter_short_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_SHORT_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_SHORT_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_pst_enter_open_mode(void)
{
    uint8_t os_test_mode_password[JD_TWO_SIZE];

    os_test_mode_password[0] = JD_MPAP_PW_MP_OPEN_START_hbyte;
    os_test_mode_password[1] = JD_MPAP_PW_MP_OPEN_START_lbyte;
    g_module_fp.fp_APP_SetSortingMode(os_test_mode_password, sizeof(os_test_mode_password));
}

static void jadard_ReadSortingMode(int *PST_MP_Mode_Index)
{
    uint8_t OS_TEST_MODE_PASSWORD[JD_TWO_SIZE];

    *PST_MP_Mode_Index = -1;
    OS_TEST_MODE_PASSWORD[0] = 0x00;
    OS_TEST_MODE_PASSWORD[1] = 0x00;

    g_module_fp.fp_APP_ReadSortingMode(OS_TEST_MODE_PASSWORD, sizeof(OS_TEST_MODE_PASSWORD));

    if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_SORTING_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_SORTING_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_Sorting;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_SHORT_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_SHORT_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_Short;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_OPEN_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_OPEN_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_Open;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_NORMAL_ACTIVE_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_NORMAL_ACTIVE_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_Normal_Active;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_NORMAL_ACTIVE_DIFF_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_NORMAL_ACTIVE_DIFF_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_Normal_Active_Diff;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_NORMAL_IDLE_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_NORMAL_IDLE_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_Normal_Idle;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_NORMAL_IDLE_DIFF_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_NORMAL_IDLE_DIFF_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_Normal_Idle_Diff;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_LPWUG_ACTIVE_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_LPWUG_ACTIVE_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_LPWUG_Active;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_LPWUG_ACTIVE_DIFF_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_LPWUG_ACTIVE_DIFF_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_LPWUG_Active_Diff;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_LPWUG_IDLE_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_LPWUG_IDLE_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_LPWUG_Idle;
    } else if ((OS_TEST_MODE_PASSWORD[0] == JD_MPAP_PW_MP_LPWUG_IDLE_DIFF_FINISH_hbyte) && (OS_TEST_MODE_PASSWORD[1] == JD_MPAP_PW_MP_LPWUG_IDLE_DIFF_FINISH_lbyte)) {
        *PST_MP_Mode_Index = JD_PST_MP_Mode_MP_LPWUG_Idle_Diff;
    }
}

static void jadard_PST_SwitchSortMode_Check(int *enter_flag, int PST_MP_Mode_Index, bool *switch_mode_timeout_flag)
{
    int counter = 0;
    *enter_flag = -1;
    *switch_mode_timeout_flag = false;

    while (*enter_flag != PST_MP_Mode_Index) {
        mdelay(50);
        jadard_ReadSortingMode(enter_flag);

        if (counter++ > 200) {
            *switch_mode_timeout_flag = true;
            *enter_flag = JD_PST_MP_Mode_Null;
            JD_E("Switch_SortMode_Fail\n");
            return;
        }
    }
}

static void jadard_PST_LpwugIdle_Diff_Mode(void)
{
    JD_I("Start to Enter LPWUG Idle Diff Mode\n");
    g_module_fp.fp_APP_SetSortingKeepFrame(0);
    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[9][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[9][1];

    jadard_PST_Enter_LpwugIdle_Diff_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_LPWUG_Idle_Diff, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter LPWUG Idle Diff Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter LPWUG Idle Diff Mode Finished\n");
    }
}

static void jadard_PST_LpwugIdle_Mode(void)
{
    JD_I("Start to Enter LPWUG Idle Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[8][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[8][1];

    jadard_PST_Enter_LpwugIdle_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_LPWUG_Idle, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter LPWUG Idle Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter LPWUG Idle Mode Finished\n");
    }
}

static void jadard_PST_LpwugActive_Diff_Mode(void)
{
    JD_I("Start to LPWUG Active Diff Mode\n");
    g_module_fp.fp_APP_SetSortingKeepFrame(0);
    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[7][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[7][1];

    jadard_PST_Enter_LpwugActive_Diff_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_LPWUG_Active_Diff, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter LPWUG Active Diff Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter LPWUG Active Diff Mode Finished\n");
    }
}

static void jadard_PST_LpwugActive_Mode(void)
{
    JD_I("Start to LPWUG Active Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[6][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[6][1];

    jadard_PST_Enter_LpwugActive_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_LPWUG_Active, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter LPWUG Active Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter LPWUG Active Mode Finished\n");
    }
}

static void jadard_PST_NormalIdle_Diff_Mode(void)
{
    JD_I("Start to Enter Normal Idle Diff Mode\n");
    g_module_fp.fp_APP_SetSortingKeepFrame(0);
    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[5][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[5][1];

    jadard_PST_Enter_NormalIdle_Diff_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_Normal_Idle_Diff, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Normal Idle Diff Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Normal Idle Diff Mode Finished\n");
    }
}

static void jadard_PST_NormalIdle_Mode(void)
{
    JD_I("Start to Enter Normal Idle Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[4][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[4][1];

    jadard_PST_Enter_NormalIdle_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_Normal_Idle, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Normal Idle Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Normal Idle Mode Finished\n");
    }
}

static void jadard_PST_NormalActive_Diff_Mode(void)
{
    JD_I("Start to Enter Normal Active Diff Mode\n");

    g_module_fp.fp_APP_SetSortingKeepFrame(0);
    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[3][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[3][1];

    jadard_PST_Enter_NormalActive_Diff_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_Normal_Active_Diff, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Normal Active Diff Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Normal Active Diff Mode Finished\n");
    }
}

static void jadard_PST_NormalActive_Mode(void)
{
    JD_I("Start to Enter Normal Active Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[2][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[2][1];

    jadard_PST_Enter_NormalActive_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_MP_Normal_Active, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Normal Active Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Normal Active Mode Finished\n");
    }
}

static void jadard_PST_Short_Mode(void)
{
    JD_I("Start to Enter Short Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[1][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[1][1];

    jadard_pst_enter_short_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_Short, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Short Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Short Mode Finished\n");
    }
}

static void jadard_PST_Open_Mode(void)
{
    JD_I("Start to Enter Open Mode\n");

    g_module_fp.fp_APP_SetSortingSkipFrame((uint8_t)jd_pst_global_variable[0][0]);
    PST_GetFrameOfRawData_Number = jd_pst_global_variable[0][1];

    jadard_pst_enter_open_mode();
    jadard_PST_SwitchSortMode_Check(&ModeEnter_Flag, JD_PST_MP_Mode_Open, &Switch_Mode_Timeout_Flag);

    if ((ModeEnter_Flag == JD_PST_MP_Mode_Null) && (Switch_Mode_Timeout_Flag == true)) {
        JD_E("Enter Open Mode Fail\n");
        Fail_flag = true;
    } else {
        JD_I("Enter Open Mode Finished\n");
    }
}

static void jadard_PST_Enter_Diff_Type_Mode(void)
{
    JD_I("Start to Enter Diff Type Mode\n");
    g_module_fp.fp_mutual_data_set(JD_DATA_TYPE_Difference);
    JD_I("Enter Diff Type Finished\n");
}

static void jadard_PST_Enter_RawData_Type_Mode(void)
{
    JD_I("Start to Enter RawData Type Mode\n");
    g_module_fp.fp_mutual_data_set(JD_DATA_TYPE_RawData);
    JD_I("Enter RawData Type Finished\n");
}

static void jadard_CPST_GetDataRun(void)
{
    uint16_t rdata_size = pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM * sizeof(uint16_t);
    int i = 0;

    do {
        if (g_module_fp.fp_get_mutual_data(JD_DATA_TYPE_RawData, GET_RAWDATA, rdata_size) < 0) {
            Fail_flag = true;
            break;
        }
    } while (i++ < PST_GetFrameOfRawData_Number);
}

static void jadard_Sorting_BusyStatus_Check(void)
{
    int counter = 0;

    JD_I("Start Sorting_BusyStatus_Check\n");

    while (g_module_fp.fp_APP_ReadSortingBusyStatus(JD_MPAP_HANDSHAKE_FINISH) == false) {
        mdelay(50);

        if (counter++ > 200) {
            Fail_flag = true;
            JD_E("MPAP_HandShake_Fail\n");
            return;
        } else {
            Fail_flag = false;
        }
    }

    JD_I("Sorting_BusyStatus_Check Pass\n");
}

static void jadard_Start_to_Get_DiffData_Function(void)
{
    int half_rawdata_len = pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM * sizeof(uint8_t);

    JD_I("Start to Get Diff Data\n");
    memset(GET_RAWDATA, 0x00, half_rawdata_len * 2);

    if (PST_GetFrameOfRawData_Number < 3)
        PST_GetFrameOfRawData_Number = 3;

    g_module_fp.fp_APP_SetSortingKeepFrame(PST_GetFrameOfRawData_Number);

    jadard_Sorting_BusyStatus_Check();
    if (Fail_flag == true) {
        JD_E("Get Diff Data Fail\n");
    } else {
        if (g_module_fp.fp_GetSortingDiffData) {
            g_module_fp.fp_GetSortingDiffData(GET_RAWDATA, half_rawdata_len * 2);
        } else {
#if JD_SORTING_NOISE_DIFF_MIN
            if (g_module_fp.fp_GetSortingDiffDataMax && g_module_fp.fp_GetSortingDiffDataMin) {
                g_module_fp.fp_GetSortingDiffDataMax(GET_RAWDATA, half_rawdata_len);
                g_module_fp.fp_GetSortingDiffDataMin(GET_RAWDATA + half_rawdata_len, half_rawdata_len);
#else
            if (g_module_fp.fp_GetSortingDiffDataMax) {
                g_module_fp.fp_GetSortingDiffDataMax(GET_RAWDATA, half_rawdata_len * 2);
#endif
            } else {
                JD_E("Get Diff Data function does not exist\n");
            }
        }
        JD_I("Get Diff Data Finished\n");
    }
}

static void jadard_Start_to_Get_RawData_Function(void)
{
    JD_I("Start to Get Raw Data\n");
    memset(GET_RAWDATA, 0x00, pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM * sizeof(uint16_t));

    if (PST_GetFrameOfRawData_Number < 3) {
        PST_GetFrameOfRawData_Number = 3;
    }

    jadard_CPST_GetDataRun();

    if (Fail_flag == true) {
        JD_E("Get Raw Data Fail\n");
    } else {
        JD_I("Get Raw Data Finished\n");
    }
}

static void jd_test_data_get(uint8_t *RAW, char *start_log, char *result, int now_item)
{
    uint32_t i;
    int raw_data;
    uint32_t index = 0;
    ssize_t len = 0;
    char *testdata = NULL;
    uint32_t SZ_SIZE = JD_OUTPUT_BUFFER_SIZE;

    JD_D("%s: Entering, Now type=%s!\n", __func__, jd_action_item_name[now_item]);

    testdata = vmalloc(sizeof(char) * SZ_SIZE);
    if (testdata == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return;
    } else {
        memset(testdata, 0x00, sizeof(char) * SZ_SIZE);
    }

    len += snprintf((testdata + len), SZ_SIZE - len, "%s", start_log);
    for (i = 0; i < pjadard_ic_data->JD_Y_NUM*pjadard_ic_data->JD_X_NUM; i++) {
        if (pjadard_ts_data->rawdata_little_endian) {
            raw_data = (((int8_t)RAW[index + 1] << 8) | RAW[index]);
        } else {
            raw_data = (((int8_t)RAW[index] << 8) | RAW[index + 1]);
        }

        if (i > 1 && ((i + 1) % pjadard_ic_data->JD_Y_NUM) == 0) {
            len += snprintf((testdata + len), SZ_SIZE - len, "%5d,\n", raw_data);
        }    else {
            len += snprintf((testdata + len), SZ_SIZE - len, "%5d,", raw_data);
        }

        index += 2;
    }
    len += snprintf((testdata + len), SZ_SIZE - len, "\n%s", result);

    memcpy(&jd_g_rslt_data[now_item * SZ_SIZE], testdata, SZ_SIZE);

    /* dbg */
    /*for(i = 0; i < SZ_SIZE; i++)
    {
        I("0x%04X, ", jd_g_rslt_data[i + (now_item * SZ_SIZE)]);
        if(i > 0 && (i % 16 == 15))
            printk("\n");
    }*/

    vfree(testdata);
    JD_D("%s: End!\n", __func__);
}

static uint32_t jadard_Item_Test_Function(uint8_t checktype)
{
    int i;
    int raw_data;
    uint32_t index = 0;
    char *rslt_log = NULL;
    char *start_log = NULL;
    int ret_val = JD_SORTING_OK;

    rslt_log = kzalloc(256 * sizeof(char), GFP_KERNEL);
    if (rslt_log == NULL) {
        JD_E("%s: rslt_log memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    start_log = kzalloc(256 * sizeof(char), GFP_KERNEL);
    if (start_log == NULL) {
        JD_E("%s: start_log memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    if (checktype >= JD_SORTING_OPEN_CHECK) {
        snprintf(start_log, 256, "\n%s%s\n", jd_action_item_name[checktype - JD_SORTING_OPEN_CHECK], ": data as follow!\n");
    }

    switch (checktype) {
    case JD_SORTING_OPEN_CHECK:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[0][i]) || (raw_data > jd_g_sorting_threshold[1][i])) {
                JD_E("%s: open check FAIL\n", __func__);
                ret_val = JD_SORTING_OPEN_CHECK_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: open check PASS\n", __func__);
        break;

    case JD_SORTING_SHORT_CHECK:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[2][i]) || (raw_data > jd_g_sorting_threshold[3][i])) {
                JD_E("%s: short check FAIL\n", __func__);
                ret_val = JD_SORTING_SHORT_CHECK_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: short check PASS\n", __func__);
        break;

    case JD_SORTING_NORMALACTIVE_SB_DEV:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[4][i]) || (raw_data > jd_g_sorting_threshold[5][i])) {
                JD_E("%s: Normal Active SB_DEV check FAIL\n", __func__);
                ret_val = JD_SORTING_NORMALACTIVE_SB_DEV_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: Normal Active SB_DEV check PASS\n", __func__);
        break;

    case JD_SORTING_NORMALACTIVE_NOISE:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[6][i]) || (raw_data > jd_g_sorting_threshold[7][i])) {
                JD_E("%s: Normal Active noise check FAIL\n", __func__);
                ret_val = JD_SORTING_NORMALACTIVE_NOISE_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: Normal Active noise check PASS\n", __func__);
        break;

    case JD_SORTING_NORMALIDLE_RAWDATA_CHECK:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[8][i]) || (raw_data > jd_g_sorting_threshold[9][i])) {
                JD_E("%s: Normal Idle Rawdata check FAIL\n", __func__);
                ret_val = JD_SORTING_NORMALIDLE_RAWDATA_CHECK_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: Normal Idle Rawdata check PASS\n", __func__);
        break;

    case JD_SORTING_NORMALIDLE_NOISE:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[10][i]) || (raw_data > jd_g_sorting_threshold[11][i])) {
                JD_E("%s: Normal Idle noise check FAIL\n", __func__);
                ret_val = JD_SORTING_NORMALIDLE_NOISE_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: Normal Idle noise check PASS\n", __func__);
        break;

    case JD_SORTING_LPWUGACTIVE_SB_DEV:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[12][i]) || (raw_data > jd_g_sorting_threshold[13][i])) {
                JD_E("%s: LPWUG Active SB_DEV check FAIL\n", __func__);
                ret_val = JD_SORTING_LPWUGACTIVE_SB_DEV_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: LPWUG Active SB_DEV check PASS\n", __func__);
        break;

    case JD_SORTING_LPWUGACTIVE_NOISE:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[14][i]) || (raw_data > jd_g_sorting_threshold[15][i])) {
                JD_E("%s: LPWUG Active noise check FAIL\n", __func__);
                ret_val = JD_SORTING_LPWUGACTIVE_NOISE_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: LPWUG Active noise check PASS\n", __func__);
        break;

    case JD_SORTING_LPWUGIDLE_RAWDATA_CHECK:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[16][i]) || (raw_data > jd_g_sorting_threshold[17][i])) {
                JD_E("%s: LPWUG Idle Rawdata check FAIL\n", __func__);
                ret_val = JD_SORTING_LPWUGIDLE_RAWDATA_CHECK_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: LPWUG Idle Rawdata check PASS\n", __func__);
        break;

    case JD_SORTING_LPWUGIDLE_NOISE:
        for (i = 0; i < (pjadard_ic_data->JD_Y_NUM * pjadard_ic_data->JD_X_NUM); i++) {
            if (pjadard_ts_data->rawdata_little_endian) {
                raw_data = (((int8_t)GET_RAWDATA[index + 1] << 8) | GET_RAWDATA[index]);
            } else {
                raw_data = (((int8_t)GET_RAWDATA[index] << 8) | GET_RAWDATA[index + 1]);
            }

            if ((raw_data < jd_g_sorting_threshold[18][i]) || (raw_data > jd_g_sorting_threshold[19][i])) {
                JD_E("%s: LPWUG Idle noise check FAIL\n", __func__);
                ret_val = JD_SORTING_LPWUGIDLE_NOISE_ERR;
                goto FAIL_END;
            }

            index += 2;
        }
        JD_I("%s: LPWUG Idle noise check PASS\n", __func__);
        break;

    default:
        JD_E("Wrong type=%d\n", checktype);
        break;
    }

    snprintf(rslt_log, 256, "\n%s%s\n", jd_action_item_name[checktype - JD_SORTING_OPEN_CHECK], " Test Pass!\n");
    JD_I("%s Test Pass!\n", jd_action_item_name[checktype - JD_SORTING_OPEN_CHECK]);
    goto END_FUNC;

FAIL_END:
    snprintf(rslt_log, 256, "\n%s%s\n", jd_action_item_name[checktype - JD_SORTING_OPEN_CHECK], " Test Fail!\n");
    JD_I("%s Test Fail!\n", jd_action_item_name[checktype - JD_SORTING_OPEN_CHECK]);

END_FUNC:
    jd_test_data_get(GET_RAWDATA, start_log, rslt_log, checktype - JD_SORTING_OPEN_CHECK);

    if (start_log) {
        kfree(start_log);
        start_log = NULL;
    }

    if (rslt_log) {
        kfree(rslt_log);
        rslt_log = NULL;
    }

    return ret_val;
}

static uint32_t jadard_TestItem_Select(uint8_t sorting_item, bool retry_flag)
{
    Fail_flag = false;

    switch (sorting_item) {
    case JD_SORTING_SLEEP_IN:
    #if (JD_PRODUCT_TYPE == 0)
        jadard_pst_enter_slpin_mode();
        mdelay(2000);
    #else
        jadard_pst_enter_slpin_mode_bymppw();
        mdelay(1000);
        g_module_fp.fp_soft_reset();
    #endif
        break;

    case JD_SORTING_SLEEP_OUT:
    #if (JD_PRODUCT_TYPE == 0)
        jadard_pst_enter_slpout_mode();
    #else
        jadard_pst_enter_slpout_mode_bymppw();
    #endif
        mdelay(500);
        break;

    case JD_SORTING_OPEN_CHECK:
        jadard_PST_Open_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_OPEN_CHECK) != JD_SORTING_OK)
            return JD_SORTING_OPEN_CHECK_ERR;
        break;

    case JD_SORTING_SHORT_CHECK:
        jadard_PST_Short_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_SHORT_CHECK) != JD_SORTING_OK)
            return JD_SORTING_SHORT_CHECK_ERR;
        break;

    case JD_SORTING_NORMALACTIVE_SB_DEV:
        jadard_PST_NormalActive_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_NORMALACTIVE_SB_DEV) != JD_SORTING_OK)
            return JD_SORTING_NORMALACTIVE_SB_DEV_ERR;
        break;

    case JD_SORTING_NORMALACTIVE_NOISE:
        jadard_PST_NormalActive_Diff_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_Diff_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_DiffData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_NORMALACTIVE_NOISE) != JD_SORTING_OK)
            return JD_SORTING_NORMALACTIVE_NOISE_ERR;
        break;

    case JD_SORTING_NORMALIDLE_RAWDATA_CHECK:
        jadard_PST_NormalIdle_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_NORMALIDLE_RAWDATA_CHECK) != JD_SORTING_OK)
            return JD_SORTING_NORMALIDLE_RAWDATA_CHECK_ERR;
        break;

    case JD_SORTING_NORMALIDLE_NOISE:
        jadard_PST_NormalIdle_Diff_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_Diff_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_DiffData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_NORMALIDLE_NOISE) != JD_SORTING_OK)
            return JD_SORTING_NORMALIDLE_NOISE_ERR;
        break;

    case JD_SORTING_LPWUGACTIVE_SB_DEV:
        jadard_PST_LpwugActive_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_LPWUGACTIVE_SB_DEV) != JD_SORTING_OK)
            return JD_SORTING_LPWUGACTIVE_SB_DEV_ERR;
        break;

    case JD_SORTING_LPWUGACTIVE_NOISE:
        jadard_PST_LpwugActive_Diff_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_Diff_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_DiffData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_LPWUGACTIVE_NOISE) != JD_SORTING_OK)
            return JD_SORTING_LPWUGACTIVE_NOISE_ERR;
        break;

    case JD_SORTING_LPWUGIDLE_RAWDATA_CHECK:
        jadard_PST_LpwugIdle_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_RawData_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_RawData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_LPWUGIDLE_RAWDATA_CHECK) != JD_SORTING_OK)
            return JD_SORTING_LPWUGIDLE_RAWDATA_CHECK_ERR;
        break;

    case JD_SORTING_LPWUGIDLE_NOISE:
        jadard_PST_LpwugIdle_Diff_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_PST_Enter_Diff_Type_Mode();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        jadard_Start_to_Get_DiffData_Function();
        if (Fail_flag == true) {
            if (retry_flag == false) {
                return jadard_TestItem_Select(sorting_item, true);
            } else {
                SOC_Fail_flag++;
                return JD_SORTING_SOC_ERR;
            }
        }
        if (jadard_Item_Test_Function(JD_SORTING_LPWUGIDLE_NOISE) != JD_SORTING_OK)
            return JD_SORTING_LPWUGIDLE_NOISE_ERR;
        break;

    default:
        break;
    }

    return JD_SORTING_OK;
}

static int jadard_power_cal(int pow, int number)
{
    int i = 0;
    int result = 1;

    for (i = 0; i < pow; i++)
        result *= 10;
    result = result * number;

    return result;
}

static int jadard_str2int(char *str)
{
    int i = 0;
    int temp_cal = 0;
    int result = 0;
    int str_len = strlen(str);
    int negtive_flag = 0;
    for (i = 0; i < strlen(str); i++) {
        if (str[i] == '-') {
            negtive_flag = 1;
            continue;
        }
        temp_cal = str[i] - '0';
        result += jadard_power_cal(str_len - i - 1, temp_cal);
    }

    if (negtive_flag == 1) {
        result = 0 - result;
    }

    return result;
}

static int jadard_string_search(char *str1, char *str2)
{
    const char *cur;
    const char *last;
    int str1_len = strlen(str1);
    int str2_len = strlen(str2) - 1;
    int result = -1;

    last =    str2 + str2_len - str1_len;

    for (cur = str2; cur <= last; ++cur) {
        if (cur[0] == str1[0] && memcmp(cur, str1, str1_len) == 0) {
            JD_D("%s: match item %s\n", __func__, str1);
            return 0;
        }
    }

    return result;
}

static int jadard_set_global_variable(char *str, int *val_parsed)
{
    int count = 0;
    int comma_count = 0;
    int i, item_num, var_val, var_len, *comma_position = NULL;
    char value[5];

    for (item_num = 0; item_num < JD_SORTING_ACTIVE_ITEM; item_num++) {
        if (val_parsed[item_num]) {
            continue;
        }

        if (jadard_string_search(jd_action_item_name[item_num], str) == 0) {
            comma_position = kzalloc(sizeof(int) * JD_COMMA_NUMBER, GFP_KERNEL);
            if (comma_position == NULL) {
                JD_E("%s: Memory alloc fail\n", __func__);
                return JD_MEM_ALLOC_FAIL;
            }

            /* Find all comma's position */
            while (str[count] != ASCII_LF) {
                if (str[count] == ASCII_COMMA) {
                    comma_position[comma_count++] = count;
                }
                count++;
            }

            if (comma_count == JD_COMMA_NUMBER) {
                var_len = comma_position[5] - comma_position[4] - 1; /* skip frame number char length */
                memset(value, 0x00, sizeof(value));
                for (i = 0; i < var_len; i++)
                    value[i] = str[comma_position[4] + 1 + i];
                var_val = jadard_str2int(value);
                jd_pst_global_variable[item_num][0] = var_val; /* set skip frame number */
                JD_D("%s: Set skip frame number for item %s: %d\n", __func__, jd_action_item_name[item_num], var_val);

                var_len = count - comma_position[5] - 1; /* get frame number char length */
                memset(value, 0x00, sizeof(value));
                for (i = 0; i < var_len; i++)
                    value[i] = str[comma_position[5] + 1 + i];
                var_val = jadard_str2int(value);
                jd_pst_global_variable[item_num][1] = var_val; /* set get frame number */
                JD_D("%s: Set get frame number for item %s: %d\n", __func__, jd_action_item_name[item_num], var_val);

                val_parsed[item_num] = 1;
            }

            kfree(comma_position);
            break;
        }
    }

    return 0;
}

static int jadard_set_sorting_threshold(char **result, int data_lines)
{
    int i, j, k, item_num, *th_parsed = NULL;
    int count_type = -1;
    int count_data = 0;
    char data[5];
    bool item_found = false;
    int count_size = pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM;

    th_parsed = kzalloc(sizeof(int) * JD_SORTING_ACTIVE_ITEM, GFP_KERNEL);
    if (th_parsed == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    for (i = 0; i < data_lines; i++) {
        if (!item_found) {
            /* Sorting action name */
            for (item_num = 0; item_num < JD_SORTING_ACTIVE_ITEM; item_num++) {
                if (th_parsed[item_num])
                    continue;

                if (jadard_string_search(jd_action_item_name[item_num], result[i]) == 0) {
                    count_type = item_num;
                    count_data = 0;
                    item_found = true;
                    th_parsed[item_num] = 1;
                    break;
                }
            }
        } else {
            /* Sorting threshold */
            j = 0;
            k = 0;
            memset(data, 0x00, sizeof(data));

            while (result[i][j] != ASCII_LF) {
                if (result[i][j] == ACSII_COLON) {
                    /* Sorting min threshold */
                    jd_g_sorting_threshold[count_type * 2][count_data] = jadard_str2int(data);
                    k = 0;
                    memset(data, 0x00, sizeof(data));
                } else if (result[i][j] == ASCII_COMMA) {
                    /* Sorting max threshold */
                    jd_g_sorting_threshold[count_type * 2 + 1][count_data] = jadard_str2int(data);
                    k = 0;
                    count_data++;
                    memset(data, 0x00, sizeof(data));
                } else {
                    if (k < sizeof(data) - 1) {
                        /* Get threshold data */
                        data[k++] = result[i][j];
                    } else {
                        JD_E("data buffer was overflow!\n");
                        break;
                    }
                }
                j++;
            }

            if (count_data == count_size)
                item_found = false;
        }
    }

    for (item_num = 0; item_num < JD_SORTING_ACTIVE_ITEM; item_num++) {
        if (!th_parsed[item_num])
            JD_I("Sorting threshold not found for item %s\n", jd_action_item_name[item_num]);
    }
    kfree(th_parsed);

    return JD_SORTING_OK;
}

static int jadard_remove_space_cr(const struct firmware *file_entry, char **result, int data_lines, int line_max_len)
{
    int count = 0;
    int str_count = 0;
    int char_count = 0;
    int item_num, *val_parsed = NULL;
    char *temp_str = NULL;
    bool keep_searching = true;
    /* Search Keyword Setting */
    bool keyword_found = false;
    char *start_keyword = "TestItem";
    char *end_keyword = "Threshold";

    /* Search Global Variables: start */
    val_parsed = kzalloc(sizeof(int) * JD_SORTING_ACTIVE_ITEM, GFP_KERNEL);
    if (val_parsed == NULL) {
        JD_E("%s: val_parsed memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    }

    do {
        if (!temp_str) {
            temp_str = kzalloc(line_max_len * sizeof(char), GFP_KERNEL);
            if (temp_str == NULL) {
                JD_E("%s: temp_str memory alloc fail\n", __func__);
                return JD_MEM_ALLOC_FAIL;
            }
        }

        switch (file_entry->data[count]) {
        case ASCII_CR:
        case ACSII_SPACE:
            count++;
            break;
        case ASCII_LF:
            if (char_count != 0) {
                temp_str[char_count] = ASCII_LF;
                /*
                 * 1. If find end keyword, stop search global variables by break do-while loop.
                 * 2. If start keyword has been found in pervious line, keep to search global variables.
                 * 3. Try to find start keyword. If found it, start search global variables in next line.
                 */
                if (jadard_string_search(end_keyword, temp_str) == 0)
                    keep_searching = false;
                else if (keyword_found)
                    jadard_set_global_variable(temp_str, val_parsed);
                else if (jadard_string_search(start_keyword, temp_str) == 0)
                    keyword_found = true;

                kfree(temp_str);
                temp_str = NULL;
                char_count = 0;
            }
            count++;
            break;
        default:
            temp_str[char_count++] = file_entry->data[count];
            count++;
            break;
        }
    } while ((count < file_entry->size) && keep_searching);

    for (item_num = 0; item_num < JD_SORTING_ACTIVE_ITEM; item_num++) {
        if (!val_parsed[item_num]) {
            JD_D("Gloable variable not found for item %s\n", jd_action_item_name[item_num]);
        }
    }
    kfree(val_parsed);
    val_parsed = NULL;
    /* Search Global Variables: end */

    /* (Continue to parse file) */

    /* Search Threshold: start */
    while ((count < file_entry->size) && (str_count < data_lines)) {
        switch (file_entry->data[count]) {
        case ASCII_CR:
        case ACSII_SPACE:
            count++;
            break;
        case ASCII_LF:
            if (char_count != 0) {
                result[str_count][char_count] = ASCII_LF;
                char_count = 0;
                str_count++;
            }
            count++;
            break;
        default:
            result[str_count][char_count++] = file_entry->data[count];
            count++;

            /* Handle no newline characters */
            if (count == file_entry->size) {
                if (char_count != 0) {
                    result[str_count][char_count] = ASCII_LF;
                }
            }
            break;
        }
    }
    /* Search Threshold: end */

    return 0;
}

static int jadard_parse_sorting_threshold_file(void)
{
    int err = JD_SORTING_OK, i;
    const struct firmware *file_entry = NULL;
    char *file_name = "jd9522t_xx_boe_mp_test.txt";
    char **result = NULL;
    int data_lines = (pjadard_ic_data->JD_X_NUM + 1) * JD_SORTING_ACTIVE_ITEM;
    int line_max_len = pjadard_ic_data->JD_Y_NUM * 12 + 4;

    JD_D("%s,Entering \n", __func__);
    JD_I("sorting threshold file name = %s\n", file_name);

    err = request_firmware(&file_entry, file_name, pjadard_ts_data->dev);
    if (err < 0) {
        JD_E("%s: Open file fail(err:%d)\n", __func__, err);
        err = JD_FILE_OPEN_FAIL;
        goto END_FILE_OPEN_FAIL;
    }

    result = vmalloc(data_lines * sizeof(char *));
    if (result == NULL) {
        JD_E("%s: result memory alloc fail\n", __func__);
        return JD_MEM_ALLOC_FAIL;
    } else {
        memset(result, 0x00, data_lines * sizeof(char *));
    }

    for (i = 0 ; i < data_lines; i++) {
        result[i] = vmalloc(line_max_len * sizeof(char));
        if (result[i] == NULL) {
            JD_E("%s: result[%d] memory alloc fail\n", __func__, i);
            goto END_FUNC;
        } else {
            /* Set end character for strlen */
            memset(result[i], 0x00, line_max_len * sizeof(char));
        }
    }

    JD_I("data_lines=%d ,line_max_len=%d\n", data_lines, line_max_len);
    JD_I("file_size=%d\n", (int)file_entry->size);

    err = jadard_remove_space_cr(file_entry, result, data_lines, line_max_len);
    if (err != JD_SORTING_OK) {
        JD_E("%s: Find threshold section/global variables from file fail, go end!\n", __func__);
        goto END_FUNC;
    }

    err = jadard_set_sorting_threshold(result, data_lines);
    if (err != JD_SORTING_OK) {
        JD_E("%s: Load criteria from file fail, go end!\n", __func__);
        goto END_FUNC;
    }

END_FUNC:
    for (i = 0 ; i < data_lines; i++) {
        if (result[i]) {
            vfree(result[i]);
        }
    }
    vfree(result);
    release_firmware(file_entry);

END_FILE_OPEN_FAIL:
    JD_I("%s,end \n", __func__);

    return err;
}

static char *get_date_time_str(void)
{
    struct timespec now_time;
    struct rtc_time rtc_now_time;
    static char time_data_buf[128] = { 0 };

    getnstimeofday(&now_time);
    rtc_time_to_tm(now_time.tv_sec, &rtc_now_time);
    snprintf(time_data_buf, sizeof(time_data_buf), "%04d%02d%02d-%02d%02d%02d",
        (rtc_now_time.tm_year + 1900), rtc_now_time.tm_mon + 1,
        rtc_now_time.tm_mday, rtc_now_time.tm_hour, rtc_now_time.tm_min,
        rtc_now_time.tm_sec);

    return time_data_buf;
}
static int jadard_read_sorting_threshold(void)
{
    int ret = JD_SORTING_OK;
    int i;
#if JD_SORTING_THRESHOLD_DBG
    int j;
#endif

    JD_READ_THRESHOLD_SIZE = (JD_SORTING_ACTIVE_ITEM)*2;

    JD_I("JD_SORTING_ACTIVE_ITEM(%d) JD_READ_THRESHOLD_SIZE(%d)\n", JD_SORTING_ACTIVE_ITEM, JD_READ_THRESHOLD_SIZE);

    jd_g_sorting_threshold = vmalloc(sizeof(int *) * JD_READ_THRESHOLD_SIZE);
    if (jd_g_sorting_threshold == NULL) {
        JD_E("jd_g_sorting_threshold memory alloc fail\n");
        return JD_MEM_ALLOC_FAIL;
    } else {
        memset(jd_g_sorting_threshold, 0x00, sizeof(int *) * JD_READ_THRESHOLD_SIZE);
    }

    for (i = 0; i < JD_READ_THRESHOLD_SIZE; i++) {
        jd_g_sorting_threshold[i] = vmalloc(sizeof(int) * (pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM));
        if (jd_g_sorting_threshold[i] == NULL) {
            JD_E("jd_g_sorting_threshold[%d] memory alloc fail\n", i);
            return JD_MEM_ALLOC_FAIL;
        } else {
            memset(jd_g_sorting_threshold[i], 0x00, sizeof(int) * (pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM));
        }
    }
    ret = jadard_parse_sorting_threshold_file();

    jd_g_file_path = vmalloc(64 * sizeof(char));
    if (jd_g_file_path == NULL) {
        JD_E("jd_g_file_path memory alloc fail\n");
        return JD_MEM_ALLOC_FAIL;
    } else {
        memset(jd_g_file_path, 0x00, 64 * sizeof(char));
    }

    jd_g_rslt_data = vmalloc(JD_OUTPUT_BUFFER_SIZE * JD_SORTING_ACTIVE_ITEM * sizeof(char));
    if (jd_g_rslt_data == NULL) {
        JD_E("jd_g_rslt_data memory alloc fail\n");
        return JD_MEM_ALLOC_FAIL;
    } else {
        memset(jd_g_rslt_data, 0x00, JD_OUTPUT_BUFFER_SIZE * JD_SORTING_ACTIVE_ITEM * sizeof(char));
    }

    GET_RAWDATA = vmalloc(pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM * sizeof(uint16_t));
    if (GET_RAWDATA == NULL) {
        JD_E("GET_RAWDATA memory alloc fail\n");
        return JD_MEM_ALLOC_FAIL;
    } else {
        memset(GET_RAWDATA, 0x00, pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM * sizeof(uint16_t));
    }

#if JD_SORTING_THRESHOLD_DBG
    for (i = 0; i < JD_READ_THRESHOLD_SIZE; i = i + 2) {
        JD_I("[%d]", i);
        for (j = 0; j < pjadard_ic_data->JD_X_NUM * pjadard_ic_data->JD_Y_NUM; j++) {
            if (j % pjadard_ic_data->JD_Y_NUM == 0) {
                JD_I("\n");
            } else {
                JD_I("%d:%d ", jd_g_sorting_threshold[i][j], jd_g_sorting_threshold[i + 1][j]);
            }
        }
    }
#endif

    return ret;
}

/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 start*/
static int dev_mkdir(char *name, umode_t mode)
{
    int ret;
    mm_segment_t fs;

    JD_I("mkdir: %s\n", name);
    fs = get_fs();
    set_fs(KERNEL_DS);
    ret = ksys_mkdir(name, mode);
    set_fs(fs);

    return ret;
}
/*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 end*/

static int jadard_sorting_test(void)
{
    uint32_t ret = JD_SORTING_OK;

    JD_I("%s: enter \n", __func__);
    /*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 start*/
    if ((dev_mkdir(JD_RSLT_OUT_PATH, 0777)) != 0) {
        JD_E("Failed to create directory for mp_test\n");
    }
    /*hs14 code for SR-AL6528A-01-524 by liudi at 20220909 end*/

    /*hs14 code for AL6528A-90 by hehaoran5 at 20220923 start*/
    if (pjadard_ts_data->suspended == true) {
        JD_E("%s: please do self test in normal active mode\n", __func__);
        return JD_TIME_OUT;
    }
    /*hs14 code for AL6528A-90 by hehaoran5 at 20220923 end*/
#ifdef JD_ZERO_FLASH
    if (pjadard_ts_data->fw_ready == false) {
        JD_E("%s: FW was not ready, can`t running ITO test\n", __func__);
        return JD_SORTING_UPGRADE_FW_ERR;
    }
#endif

    mutex_lock(&(pjadard_ts_data->sorting_active));
    jadard_int_enable(false);
    g_module_fp.fp_Fw_DBIC_Off();
    SOC_Fail_flag = 0;

    ret = jadard_read_sorting_threshold();
    if (ret != JD_SORTING_OK) {
        JD_E("jadard_read_sorting_threshold fail!\n");
        goto END_SORTING_TEST;
    }

#if JD_SORTING_LPWUG_CHECK
    /* 0.Sleep in/out */
    JD_I("[JD_SORTING_SLEEP_IN]\n");
    ret += jadard_TestItem_Select(JD_SORTING_SLEEP_IN, false);
    JD_I("JD_SORTING_SLEEP_IN: End %d\n\n", ret);

    JD_I("[JD_SORTING_SLEEP_OUT]\n");
    ret += jadard_TestItem_Select(JD_SORTING_SLEEP_OUT, false);
    JD_I("JD_SORTING_SLEEP_OUT: End %d\n\n", ret);
#endif

    /* 1.Open Test */
    JD_I("[JD_SORTING_OPEN_CHECK]\n");
    ret += jadard_TestItem_Select(JD_SORTING_OPEN_CHECK, false);
    JD_I("JD_SORTING_OPEN_CHECK: End %d\n\n", ret);

    /* 2. Short Test */
    JD_I("[JD_SORTING_SHORT_CHECK]\n");
    ret += jadard_TestItem_Select(JD_SORTING_SHORT_CHECK, false);
    JD_I("JD_SORTING_SHORT_CHECK: End %d\n\n", ret);

    /* 3. NORMAL ACTIVE SB_DEV Test */
    JD_I("[JD_SORTING_NORMALACTIVE_SB_DEV]\n");
    ret += jadard_TestItem_Select(JD_SORTING_NORMALACTIVE_SB_DEV, false);
    JD_I("JD_SORTING_NORMALACTIVE_SB_DEV: End %d\n\n", ret);

    /* 4. NORMAL ACTIVE NOISE Test */
    JD_I("[JD_SORTING_NORMALACTIVE_NOISE]\n");
    ret += jadard_TestItem_Select(JD_SORTING_NORMALACTIVE_NOISE, false);
    JD_I("JD_SORTING_NORMALACTIVE_NOISE: End %d\n\n", ret);

    /* 5. NORMAL IDLE RAWDATA CHECK Test */
    JD_I("[JD_SORTING_NORMALIDLE_RAWDATA_CHECK]\n");
    ret += jadard_TestItem_Select(JD_SORTING_NORMALIDLE_RAWDATA_CHECK, false);
    JD_I("JD_SORTING_NORMALIDLE_RAWDATA_CHECK: End %d\n\n", ret);

    /* 6. NORMAL IDLE NOISE Test */
    JD_I("[JD_SORTING_NORMALIDLE_NOISE]\n");
    ret += jadard_TestItem_Select(JD_SORTING_NORMALIDLE_NOISE, false);
    JD_I("JD_SORTING_NORMALIDLE_NOISE: End %d\n\n", ret);

#if JD_SORTING_LPWUG_CHECK
    /* 7. LPWUG ACTIVE SB_DEV Test */
    JD_I("[JD_SORTING_LPWUGACTIVE_SB_DEV]\n");
    ret += jadard_TestItem_Select(JD_SORTING_LPWUGACTIVE_SB_DEV, false);
    JD_I("JD_SORTING_LPWUGACTIVE_SB_DEV: End %d\n\n", ret);

    /* 8. LPWUG ACTIVE NOISE Test */
    JD_I("[JD_SORTING_LPWUGACTIVE_NOISE]\n");
    ret += jadard_TestItem_Select(JD_SORTING_LPWUGACTIVE_NOISE, false);
    JD_I("JD_SORTING_LPWUGACTIVE_NOISE: End %d\n\n", ret);

    /* 9. LPWUG IDLE RAWDATA CHECK Test */
    JD_I("[JD_SORTING_LPWUGIDLE_RAWDATA_CHECK]\n");
    ret += jadard_TestItem_Select(JD_SORTING_LPWUGIDLE_RAWDATA_CHECK, false);
    JD_I("JD_SORTING_LPWUGIDLE_RAWDATA_CHECK: End %d\n\n", ret);

    /* 10. LPWUG IDLE NOISE Test */
    JD_I("[JD_SORTING_LPWUGIDLE_NOISE]\n");
    ret += jadard_TestItem_Select(JD_SORTING_LPWUGIDLE_NOISE, false);
    JD_I("JD_SORTING_LPWUGIDLE_NOISE: End %d\n\n", ret);
#endif

    /*hs14 code for AL6528A-172 by liudi at 20221008 start*/
    if ((ret != 0) || (SOC_Fail_flag >= 6)) {
        ret = 1;
    }
    snprintf(jd_g_file_path, JD_RSLT_OUT_FILE_SIZE-1, "%s%s_%s_%s.txt", JD_RSLT_OUT_PATH, JD_RSLT_OUT_FILE, get_date_time_str(), ret == 0 ? "PASS" : "FAIL");
    /*hs14 code for AL6528A-172 by liudi at 20221008 end*/

    jd_test_data_pop_out(jd_g_rslt_data, jd_g_file_path);
    /* Enter normal mode */
    jadard_pst_enter_normal_mode();
    g_module_fp.fp_soft_reset();
    g_module_fp.fp_StartMCU();

END_SORTING_TEST:
    jadard_sorting_data_deinit();

    JD_I("Sorting test result = %d \n", ret);

    if ((ret != 0) || (SOC_Fail_flag >= 6)) {
        ret = 1;
    }

    JD_I("%s:OUT\n", __func__);
    g_module_fp.fp_Fw_DBIC_On();
    jadard_int_enable(true);
    mutex_unlock(&(pjadard_ts_data->sorting_active));

    return ret;
}

void jadard_sorting_init(void)
{
    JD_I("%s: enter \n", __func__);
    mutex_init(&(pjadard_ts_data->sorting_active));
    g_module_fp.fp_sorting_test = jadard_sorting_test;
}
