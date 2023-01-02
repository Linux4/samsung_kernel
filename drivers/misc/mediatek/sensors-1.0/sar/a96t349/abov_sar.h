/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 */
#ifndef ABOV_SAR_H
#define ABOV_SAR_H

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/interrupt.h>

/*
 *  I2C Registers
 */
#define ABOV_IRQSTAT_REG            0x00
#define ABOV_VERSION_REG            0x01
#define ABOV_MODELNO_REG            0x02
#define ABOV_VENDOR_ID_REG          0x03
#define ABOV_IRQSTAT_LEVEL_REG      0x04
#define ABOV_SOFTRESET_REG          0x06
#define ABOV_CTRL_MODE_REG          0x07
#define ABOV_CTRL_CHANNEL_REG       0x08
#define ABOV_CH0_DIFF_MSB_REG       0x1C
#define ABOV_CH0_DIFF_LSB_REG       0x1D
#define ABOV_CH1_DIFF_MSB_REG       0x1E
#define ABOV_CH1_DIFF_LSB_REG       0x1F
#define ABOV_RECALI_REG             0xFB


#define KEY_CAP_CH0    0x00
#define KEY_CAP_CH1    0x01


/* enable body stat */
#define ABOV_TCHCMPSTAT_TCHSTAT0_FLAG   0x03
#define ABOV_TCHCMPSTAT_TCHSTAT1_FLAG   0x0C


/**************************************
* define platform data
*
**************************************/
struct smtc_reg_data {
    unsigned char reg;
    unsigned char val;
};

typedef struct smtc_reg_data smtc_reg_data_t;
typedef struct smtc_reg_data *psmtc_reg_data_t;


struct _buttonInfo {
    /* The Key to send to the input */
    int keycode;
    /* Mask to look for on Touch Status */
    int mask;
    /* Current state of button. */
    int state;
};

struct _totalButtonInformation {
    struct _buttonInfo *buttons;
    int buttonSize;
    struct input_dev *input_bottom_sar;
    struct input_dev *input_top_sar;

};

typedef struct _totalButtonInformation buttonInformation_t;
typedef struct _totalButtonInformation *pbuttonInformation_t;

/* Define Registers that need to be initialized to values different than
 * default
 */
/*TabA7 Lite code for OT8-2216 by Hujincan at 20210128 start*/
static struct smtc_reg_data abov_i2c_reg_setup[] = {
    {
        .reg = ABOV_CTRL_MODE_REG,
        .val = 0x00,
    },
    {
        .reg = ABOV_RECALI_REG,
        .val = 0x01,
    },
};
/*TabA7 Lite code for OT8-2216 by Hujincan at 20210128 end*/



static struct _buttonInfo psmtcButtons[] = {
    {
        .keycode = KEY_CAP_CH0,
        .mask = ABOV_TCHCMPSTAT_TCHSTAT0_FLAG,
    },
    {
        .keycode = KEY_CAP_CH1,
        .mask = ABOV_TCHCMPSTAT_TCHSTAT1_FLAG,
    },
};

struct abov_platform_data {
    int i2c_reg_num;
    struct smtc_reg_data *pi2c_reg;
    struct regulator *vdd;
    /*TabA7 Lite code for OT8-221 by Hujincan at 20201218 start*/
    //struct regulator *vddio;
    /*TabA7 Lite code for OT8-221 by Hujincan at 20201218 start*/
    unsigned irq_gpio;
    /* used for custom setting for channel and scan period */
    int cap_channel_bottom;
    int cap_channel_top;
    pbuttonInformation_t pbuttonInformation;
    const char *fw_name;

    int (*get_is_nirq_low)(unsigned irq_gpio);
    int (*init_platform_hw)(void);
    void (*exit_platform_hw)(void);
#if defined(CONFIG_SENSORS)
    struct device *factory_device;
#ifdef CONFIG_HQ_PROJECT_HS03S
    struct device *factory_device_sub;
#else
    struct device *factory_device_wifi;
#endif  //hs03s
#endif
};
typedef struct abov_platform_data abov_platform_data_t;
typedef struct abov_platform_data *pabov_platform_data_t;

/***************************************
* define data struct/interrupt
* @pdev: pdev common device struction for linux
* @dworker: work struct for worker function
* @board: constant pointer to platform data
* @mutex: mutex for interrupt process
* @lock: Spin Lock used for nirq worker function
* @bus: either i2c_client or spi_client
* @pDevice: device specific struct pointer
*@read_flag : used for dump specified register
* @irq: irq number used
* @irqTimeout: msecs only set if useIrqTimer is true
* @irq_disabled: whether irq should be ignored
* @irq_gpio: irq gpio number
* @useIrqTimer: older models need irq timer for pen up cases
* @read_reg: record reg address which want to read
*@cust_prox_ctrl0 : used for custom setting for channel and scan period
* @init: (re)initialize device
* @refreshStatus: read register status
* @get_nirq_low: get whether nirq is low (platform data)
* @statusFunc: array of functions to call for corresponding status bit
***************************************/
#define USE_THREADED_IRQ

#define MAX_NUM_STATUS_BITS (2)

/*TabA7 Lite code for OT8-467 by Hujincan at 20210105 start*/
/*Tab A7 lite_T code for SR-AX3565A-01-166 by duxinqi at 2022/10/18 start*/
//#define SAR_USB_CALIBRATION
/*Tab A7 lite_T code for SR-AX3565A-01-166 by duxinqi at 2022/10/18 end*/
/*TabA7 Lite code for OT8-467 by Hujincan at 20210105 end*/

typedef struct abovXX abovXX_t, *pabovXX_t;
struct abovXX {
    struct device *pdev;
    struct delayed_work dworker;
    struct abov_platform_data *board;
#if defined(USE_THREADED_IRQ)
    struct mutex mutex;
#else
    spinlock_t    lock;
#endif
    void *bus;
    void *pDevice;
    int read_flag;
    int irq;
    int irqTimeout;
    char irq_disabled;
    /* whether irq should be ignored.. cases if enable/disable irq is not used
     * or does not work properly */
    u8 useIrqTimer;
    u8 read_reg;
    bool loading_fw;
    struct work_struct fw_update_work;

    #ifdef SAR_USB_CALIBRATION
    struct notifier_block notifier_charger;
    struct work_struct update_charger;
    struct workqueue_struct *charger_notify_wq;
    #endif

    /* Function Pointers */
    int (*init)(pabovXX_t this);
    /* since we are trying to avoid knowing registers, create a pointer to a
     * common read register which would be to read what the interrupt source
     * is from
     */
    int (*refreshStatus)(pabovXX_t this);
    int (*get_nirq_low)(unsigned irq_gpio);
    void (*statusFunc[MAX_NUM_STATUS_BITS])(pabovXX_t this);
#if defined(CONFIG_SENSORS)
    char channel_status;
    bool skip_data;
#endif
};

void abovXX_suspend(pabovXX_t this);
void abovXX_resume(pabovXX_t this);
int abovXX_sar_init(pabovXX_t this);
int abovXX_sar_remove(pabovXX_t this);

#ifdef SAR_USB_CALIBRATION
void sar_plat_charger_init(void);
#endif

#endif
