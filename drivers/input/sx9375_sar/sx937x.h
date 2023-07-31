/*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*/
#ifndef __SX937X_H__
#define __SX937X_H__

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

#include <linux/sensors.h>

//Interrupt Control
#define SX937X_IRQ_SOURCE  0x4000
#define SX937X_IRQ_MASK_A  0x4004
#define SX937X_IRQ_MASK_B  0x800C
#define SX937X_IRQ_SETUP  0x4008

//Device State Configuration
#define SX937X_DEVICE_RESET  0x4240
#define SX937X_COMMAND  0x4280
#define SX937X_CMMAND_BUSY  0x4284

//Pin Customization
#define SX937X_PIN_SETUP_A  0x42C0
#define SX937X_PIN_SETUP_B  0x42C4

//Device Information
#define SX937X_DEVICE_INFO  0x42CC

//Status Registers
#define SX937X_DEVICE_STATUS_A  0x8000
#define SX937X_DEVICE_STATUS_B  0x8004
#define SX937X_DEVICE_STATUS_C  0x8008

//Status Output Setup
#define SX937X_STATUS_OUTPUT_0  0x8010
#define SX937X_STATUS_OUTPUT_1  0x8014
#define SX937X_STATUS_OUTPUT_2  0x8018
#define SX937X_STATUS_OUTPUT_3  0x801C

//Phase Enabling
#define SX937X_SCAN_PERIOD_SETUP  0x8020
#define SX937X_GENERAL_SETUP  0x8024

//AFE Parameter Setup
#define SX937X_AFE_PARAMETERS_PH0  0x8028
#define SX937X_AFE_PARAMETERS_PH1  0x8034
#define SX937X_AFE_PARAMETERS_PH2  0x8040
#define SX937X_AFE_PARAMETERS_PH3  0x804C
#define SX937X_AFE_PARAMETERS_PH4  0x8058
#define SX937X_AFE_PARAMETERS_PH5  0x8064
#define SX937X_AFE_PARAMETERS_PH6  0x8070
#define SX937X_AFE_PARAMETERS_PH7  0x807C

//AFE Connection Setup
#define SX937X_AFE_CS_USAGE_PH0  0x8030
#define SX937X_AFE_CS_USAGE_PH1  0x803C
#define SX937X_AFE_CS_USAGE_PH2  0x8048
#define SX937X_AFE_CS_USAGE_PH3  0x8054
#define SX937X_AFE_CS_USAGE_PH4  0x8060
#define SX937X_AFE_CS_USAGE_PH5  0x806C
#define SX937X_AFE_CS_USAGE_PH6  0x8078
#define SX937X_AFE_CS_USAGE_PH7  0x8084

//ADC, RAW Filter and Debounce Filter
#define SX937X_FILTER_SETUP_A_PH0  0x8088
#define SX937X_FILTER_SETUP_A_PH1  0x80A8
#define SX937X_FILTER_SETUP_A_PH2  0x80C8
#define SX937X_FILTER_SETUP_A_PH3  0x80E8
#define SX937X_FILTER_SETUP_A_PH4  0x8108
#define SX937X_FILTER_SETUP_A_PH5  0x8128
#define SX937X_FILTER_SETUP_A_PH6  0x8148
#define SX937X_FILTER_SETUP_A_PH7  0x8168

//Average and UseFilter Filter
#define SX937X_FILTER_SETUP_B_PH0  0x808C
#define SX937X_FILTER_SETUP_B_PH1  0x80AC
#define SX937X_FILTER_SETUP_B_PH2  0x80CC
#define SX937X_FILTER_SETUP_B_PH3  0x80EC
#define SX937X_FILTER_SETUP_B_PH4  0x810C
#define SX937X_FILTER_SETUP_B_PH5  0x812C
#define SX937X_FILTER_SETUP_B_PH6  0x814C
#define SX937X_FILTER_SETUP_B_PH7  0x816C

//Filter Setup C
#define SX937X_USE_FLT_SETUP_PH0  0x8090
#define SX937X_USE_FLT_SETUP_PH1  0x80B0
#define SX937X_USE_FLT_SETUP_PH2  0x80D0
#define SX937X_USE_FLT_SETUP_PH3  0x80F0
#define SX937X_USE_FLT_SETUP_PH4  0x8110
#define SX937X_USE_FLT_SETUP_PH5  0x8130
#define SX937X_USE_FLT_SETUP_PH6  0x8150
#define SX937X_USE_FLT_SETUP_PH7  0x8170

//Filter Setup D
#define SX937X_ADC_QUICK_FILTER_0  0x81E0
#define SX937X_ADC_QUICK_FILTER_1  0x81E4
#define SX937X_ADC_QUICK_FILTER_2  0x81E8
#define SX937X_ADC_QUICK_FILTER_3  0x81EC

//Steady and Saturation Setup
#define SX937X_STEADY_AND_SATURATION_PH0  0x809C
#define SX937X_STEADY_AND_SATURATION_PH1  0x80BC
#define SX937X_STEADY_AND_SATURATION_PH2  0x80DC
#define SX937X_STEADY_AND_SATURATION_PH3  0x80FC
#define SX937X_STEADY_AND_SATURATION_PH4  0x811C
#define SX937X_STEADY_AND_SATURATION_PH5  0x813C
#define SX937X_STEADY_AND_SATURATION_PH6  0x815C
#define SX937X_STEADY_AND_SATURATION_PH7  0x817C

//Failure Threshold Setup
#define SX937X_FAILURE_THRESHOLD_PH0  0x80A4
#define SX937X_FAILURE_THRESHOLD_PH1  0x80C4
#define SX937X_FAILURE_THRESHOLD_PH2  0x80E4
#define SX937X_FAILURE_THRESHOLD_PH3  0x8104
#define SX937X_FAILURE_THRESHOLD_PH4  0x8124
#define SX937X_FAILURE_THRESHOLD_PH5  0x8144
#define SX937X_FAILURE_THRESHOLD_PH6  0x8164
#define SX937X_FAILURE_THRESHOLD_PH7  0x8184

//Proximity Threshold
#define SX937X_PROX_THRESH_PH0  0x8098
#define SX937X_PROX_THRESH_PH1  0x80B8
#define SX937X_PROX_THRESH_PH2  0x80D8
#define SX937X_PROX_THRESH_PH3  0x80F8
#define SX937X_PROX_THRESH_PH4  0x8118
#define SX937X_PROX_THRESH_PH5  0x8138
#define SX937X_PROX_THRESH_PH6  0x8158
#define SX937X_PROX_THRESH_PH7  0x8178

//Startup Setup
#define SX937X_STARTUP_PH0  0x8094
#define SX937X_STARTUP_PH1  0x80B4
#define SX937X_STARTUP_PH2  0x80D4
#define SX937X_STARTUP_PH3  0x80F4
#define SX937X_STARTUP_PH4  0x8114
#define SX937X_STARTUP_PH5  0x8134
#define SX937X_STARTUP_PH6  0x8154
#define SX937X_STARTUP_PH7  0x8174

//Reference Correction Setup A
#define SX937X_REFERENCE_CORRECTION_PH0  0x80A0
#define SX937X_REFERENCE_CORRECTION_PH1  0x80C0
#define SX937X_REFERENCE_CORRECTION_PH2  0x80E0
#define SX937X_REFERENCE_CORRECTION_PH3  0x8100
#define SX937X_REFERENCE_CORRECTION_PH4  0x8120
#define SX937X_REFERENCE_CORRECTION_PH5  0x8140
#define SX937X_REFERENCE_CORRECTION_PH6  0x8160
#define SX937X_REFERENCE_CORRECTION_PH7  0x8180

//Reference Correction Setup B
#define SX937X_REF_ENGINE_1_CONFIG  0x8188
#define SX937X_REF_ENGINE_2_CONFIG  0x818C
#define SX937X_REF_ENGINE_3_CONFIG  0x8190
#define SX937X_REF_ENGINE_4_CONFIG  0x8194

//Smart Human Sensing Setup A
#define SX937X_ENGINE_1_CONFIG  0x8198
#define SX937X_ENGINE_1_X0  0x819C
#define SX937X_ENGINE_1_X1  0x81A0
#define SX937X_ENGINE_1_X2  0x81A4
#define SX937X_ENGINE_1_X3  0x81A8
#define SX937X_ENGINE_1_Y0  0x81AC
#define SX937X_ENGINE_1_Y1  0x81B0
#define SX937X_ENGINE_1_Y2  0x81B4
#define SX937X_ENGINE_1_Y3  0x81B8

//Smart Human Sensing Setup B
#define SX937X_ENGINE_2_CONFIG  0x81BC
#define SX937X_ENGINE_2_X0  0x81C0
#define SX937X_ENGINE_2_X1  0x81C4
#define SX937X_ENGINE_2_X2  0x81C8
#define SX937X_ENGINE_2_X3  0x81CC
#define SX937X_ENGINE_2_Y0  0x81D0
#define SX937X_ENGINE_2_Y1  0x81D4
#define SX937X_ENGINE_2_Y2  0x81D8
#define SX937X_ENGINE_2_Y3  0x81DC

//Offset Readback
#define SX937X_OFFSET_PH0  0x802C
#define SX937X_OFFSET_PH1  0x8038
#define SX937X_OFFSET_PH2  0x8044
#define SX937X_OFFSET_PH3  0x8050
#define SX937X_OFFSET_PH4  0x805C
#define SX937X_OFFSET_PH5  0x8068
#define SX937X_OFFSET_PH6  0x8074
#define SX937X_OFFSET_PH7  0x8080

//Useful Readback
#define SX937X_USEFUL_PH0  0x81F0
#define SX937X_USEFUL_PH1  0x81F4
#define SX937X_USEFUL_PH2  0x81F8
#define SX937X_USEFUL_PH3  0x81FC
#define SX937X_USEFUL_PH4  0x8200
#define SX937X_USEFUL_PH5  0x8204
#define SX937X_USEFUL_PH6  0x8208
#define SX937X_USEFUL_PH7  0x820C

//UseFilter Readback
#define SX937X_USEFILTER_PH0  0x8250
#define SX937X_USEFILTER_PH1  0x8254
#define SX937X_USEFILTER_PH2  0x8258
#define SX937X_USEFILTER_PH3  0x825C
#define SX937X_USEFILTER_PH4  0x8260
#define SX937X_USEFILTER_PH5  0x8264
#define SX937X_USEFILTER_PH6  0x8268
#define SX937X_USEFILTER_PH7  0x826C

//Average Readback
#define SX937X_AVERAGE_PH0  0x8210
#define SX937X_AVERAGE_PH1  0x8214
#define SX937X_AVERAGE_PH2  0x8218
#define SX937X_AVERAGE_PH3  0x821C
#define SX937X_AVERAGE_PH4  0x8220
#define SX937X_AVERAGE_PH5  0x8224
#define SX937X_AVERAGE_PH6  0x8228
#define SX937X_AVERAGE_PH7  0x822C

//Diff Readback
#define SX937X_DIFF_PH0  0x8230
#define SX937X_DIFF_PH1  0x8234
#define SX937X_DIFF_PH2  0x8238
#define SX937X_DIFF_PH3  0x823C
#define SX937X_DIFF_PH4  0x8240
#define SX937X_DIFF_PH5  0x8244
#define SX937X_DIFF_PH6  0x8248
#define SX937X_DIFF_PH7  0x824C

//Specialized Readback
#define SX937X_DEBUG_SETUP  0x8274
#define SX937X_DEBUG_READBACK_0  0x8278
#define SX937X_DEBUG_READBACK_1  0x827C
#define SX937X_DEBUG_READBACK_2  0x8280
#define SX937X_DEBUG_READBACK_3  0x8284
#define SX937X_DEBUG_READBACK_4  0x8288
#define SX937X_DEBUG_READBACK_5  0x828C
#define SX937X_DEBUG_READBACK_6  0x8290
#define SX937X_DEBUG_READBACK_7  0x8294
#define SX937X_DEBUG_READBACK_8  0x8298

/*      Chip ID   */
#define SX937X_WHOAMI_VALUE                   0x9370
/*command*/
#define SX937X_PHASE_CONTROL                  0x0000000F
#define SX937X_COMPENSATION_CONTROL           0x0000000E
#define SX937X_ENTER_CONTROL                  0x0000000D
#define SX937X_EXIT_CONTROL                   0x0000000C

//user define
#define USEFUL_TAB                      0x04
#define OFFSET_TAB                      0x0c
#define SAR_STATE_NEAR                  0
#define SAR_STATE_FAR                   5
#define SAR_STATE_NONE                  -1

/**************************************
*   define platform data
*
**************************************/
struct smtc_reg_data
{
    unsigned int reg;
    unsigned int val;
};

typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;


struct _buttonInfo
{
    /*! The Key to send to the input */
    int keycode;
    /*! Mask to look for on Touch Status */
    int ProxMask;
    /*! Mask to look for on Table Touch Status */
    int BodyMask;
    /*! Current state of button. */
    int state;
    bool enabled;
    char *name;
    //may different project use different buttons
    bool used;
//20221221 wnn add channel input and sensor class start
    int type;
    struct sensors_classdev sx9375_sensors_class_cdev;
    struct input_dev *sx9375_input_dev;
//20221221 wnn add channel input and sensor class end
};

struct totalButtonInformation
{
    struct _buttonInfo *buttons;
    int buttonSize;
    //struct input_dev *input;
};

typedef struct totalButtonInformation buttonInformation_t;
typedef struct totalButtonInformation *pbuttonInformation_t;


/* Define Registers that need to be initialized to values different than
* default
*/
/*define the value without Phase enable settings for easy changes in driver*/
static const struct smtc_reg_data sx937x_i2c_reg_setup[] =
{
};

static struct _buttonInfo psmtcButtons[] =
{
    {
        .keycode = KEY_0,
        .ProxMask = 1 << 24,
        .BodyMask = 1 << 16,
        .name = "grip_sensor",
        .enabled = false,
        .used = false,
        .type = 65560,
    },
    {
        .keycode = KEY_1,
        .ProxMask = 1 << 25,
        .BodyMask = 1 << 17,
        .name = "grip_sensor_sub",
        .enabled = false,
        .used = false,
        .type = 65636
    },
    {
        .keycode = KEY_2,
        .ProxMask = 1 << 26,
        .BodyMask = 1 << 18,
        .name = "grip_sensor_wifi",
        .enabled = false,
        .used = false,
        .type = 65575,
    },
    {
        .keycode = KEY_3,
        .ProxMask = 1 << 27,
        .BodyMask = 1 << 19,
        .name = "CapSense_Ch3",
        .enabled = false,
        .used = false,
    },
    {
        .keycode = KEY_4,
        .ProxMask = 1 << 28,
        .BodyMask = 1 << 20,
        .name = "CapSense_Ch4",
        .enabled = false,
        .used = false,
    },
    {
        .keycode = KEY_5,
        .ProxMask = 1 << 29,
        .BodyMask = 1 << 21,
        .name = "CapSense_Ch5",
        .enabled = false,
        .used = false,
    },
    {
        .keycode = KEY_6,
        .ProxMask = 1 << 30,
        .BodyMask = 1 << 22,
        .name = "CapSense_Ch6",
        .enabled = false,
        .used = false,
    },
    {
        .keycode = KEY_7,
        .ProxMask = 1 << 31,
        .BodyMask = 1 << 23,
        .name = "CapSense_Ch7",
        .enabled = false,
        .used = false,
    },
};

struct sx937x_platform_data
{
    int i2c_reg_num;
    struct smtc_reg_data *pi2c_reg;
    u32 button_used_flag;
    int irq_gpio;
    int ref_phase_a;
    int ref_phase_b;
    int ref_phase_c;
    pbuttonInformation_t pbuttonInformation;

    int     (*get_is_nirq_low)(void);
    int     (*init_platform_hw)(struct i2c_client *client);
    void    (*exit_platform_hw)(struct i2c_client *client);
};
typedef struct sx937x_platform_data sx937x_platform_data_t;
typedef struct sx937x_platform_data *psx937x_platform_data_t;

/***************************************
*  define data struct/interrupt
*
***************************************/

#define MAX_NUM_STATUS_BITS (8)

typedef struct sx93XX sx93XX_t, *psx93XX_t;
struct sx93XX
{
    void * bus; /* either i2c_client or spi_client */

    struct device *pdev; /* common device struction for linux */

    void *pDevice; /* device specific struct pointer */

    /* Function Pointers */
    int (*init)(psx93XX_t this); /* (re)initialize device */

    /* since we are trying to avoid knowing registers, create a pointer to a
    * common read register which would be to read what the interrupt source
    * is from
    */
    int (*refreshStatus)(psx93XX_t this); /* read register status */

    int (*get_nirq_low)(void); /* get whether nirq is low (platform data) */

    /* array of functions to call for corresponding status bit */
    void (*statusFunc[MAX_NUM_STATUS_BITS])(psx93XX_t this);

    /* Global variable */
    u8     failStatusCode;  /*Fail status code*/
    bool  reg_in_dts;

    spinlock_t       lock; /* Spin Lock used for nirq worker function */
    int irq; /* irq number used */

    /* whether irq should be ignored.. cases if enable/disable irq is not used
    * or does not work properly */
    char irq_disabled;

    u8 useIrqTimer; /* older models need irq timer for pen up cases */

    int irqTimeout; /* msecs only set if useIrqTimer is true */

    /* struct workqueue_struct *ts_workq;  */  /* if want to use non default */
    struct delayed_work dworker; /* work struct for worker function */

    struct delayed_work delay_get_offset;

    u8 phaseselect;

#if POWER_ENABLE
    int power_state;
#endif
#if SAR_IN_FRANCE
    bool sar_first_boot;
    int interrupt_count;
    u16 ch0_backgrand_cap;
    u16 ch1_backgrand_cap;
    u16 ch2_backgrand_cap;
    int user_test;

    bool anfr_export_exit;
#endif
};

void sx93XX_suspend(psx93XX_t this);
void sx93XX_resume(psx93XX_t this);
int sx93XX_IRQ_init(psx93XX_t this);
//int sx93XX_remove(psx93XX_t this);
static int sx937x_remove(struct i2c_client *client);

#endif
