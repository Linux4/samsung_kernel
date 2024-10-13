/*! \file sx93_3x7x_sar.c
 * \brief  Semtech CAP Sensor Driver for SAR
 *
 * Copyright (c) 2023 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>

#include "sx933x.h"
//#include "sx937x.h"

/* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 start */
#ifdef SX933X_FACTORY_NODE
#include "../sensor_fac_node.h"
#endif
/* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 end */
/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240423 start*/
#include <linux/sensor/sensors_core.h>
#include "../sensor_user_node.h"
static bool g_onoff_enable = false;
#ifdef SX933X_USER_NODE
#define VENDOR_NAME      "SEMTECH"
#define MODEL_NAME       "SX9325"
#endif
static bool g_Enabled = false;
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240423 end*/
/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#define DYNAMIC_AVG_FLT             1
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/

#define USE_THREAD_IRQ              1
#define ENABLE_STAY_AWAKE           0
#define ENABLE_ESD_RECOVERY         0
#define EXTRA_LOG_TIME              1

#define DISABLE_DURING_SUSPEND      0
#define PAUSE_DURING_SUSPEND        1
#define WAKE_BY_IRQ                 0

#if (WAKE_BY_IRQ + PAUSE_DURING_SUSPEND + DISABLE_DURING_SUSPEND) > 1
#error Only enable one or none of them
#endif

#if ENABLE_STAY_AWAKE
#ifdef CONFIG_PM
#include <linux/pm_wakeup.h>
#include <linux/pm_wakeirq.h>
#define WAKE_TMOUT 2000 //milliseconds
#else
#error CONFIG_PM is not enabled in the Kconfig
#endif
#endif //ENABLE_STAY_AWAKE

#if WAKE_BY_IRQ
#include <linux/pm_wakeirq.h>
#endif

#if ENABLE_ESD_RECOVERY
#define SUPPORT_POWER_ONOFF     0
#endif

#if EXTRA_LOG_TIME
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#define EXTRA_LOG_DATE_TIME 0
#endif //EXTRA_LOG_TIME

#define I2C_M_WR_SMTC           0 /* for i2c Write */
#define I2C_M_RD_SMTC           1 /* for i2c Read */
#define NUM_IRQ_BITS            8
#define NUM_RETRY_ON_I2C_ERR    3
#define SLEEP_BETWEEN_RETRY     50 //in ms
#define I2C_DELAY_AFTER_RESUME  50 //in ms

//Refer to register 0x8000
typedef enum {
    RELEASED = 0,
    PROX1 = 1,  //SX933X: PROXSTAT
    PROX2 = 2,  //SX933X: TABLESTAT
    PROX3 = 3,  //SX933X: BODYSTAT
    PROX4 = 4   //SX933X: Not used
}PROX_STATUS;

typedef enum {
    NOT_USED = 0,   //this phase is not used.
    MAIN = 1,       //its CS is connected to an antenna for detecting human proximity.
    REF = 2         //Reference phase. Used to correct the temperature drift of the correspoinding MAIN phase
}PHASE_USAGE;

//Refer to register 0x4280
typedef enum{
    CMD_ACTIVATE    = 0xF,
    CMD_COMPENSATE  = 0xE,
    CMD_PAUSE       = 0xD,
    CMD_RESUME      = 0xC,    
        
}COMMANDS;

typedef struct reg_addr_val_s
{
    //declare it as u32 is for parsing the dts, though regiter addr is 16 bits, 
    u32 addr;
    u32 val;
}reg_addr_val_t, *reg_addr_val_p;

typedef struct phase_s
{
    char *name; //Name to show in the input getevent
    struct input_dev *input;

    //Refer to register 0x8000, prox4_mask is not used by the sx933x
    u32 prox4_mask;
    u32 prox3_mask;
    u32 prox2_mask;
    u32 prox1_mask;
    /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 start */
    char phase_status;
    int keycode_far;
    int keycode_close;
    /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 end */
    bool enabled;
    PHASE_USAGE usage;
    PROX_STATUS state;
}phase_t, *phase_p;

/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
struct dumpinfo {
    int ph_state;
    u16 ref_cap;
    u16 total_cap;
    s32 raw_data;
    s32 baseline_data;
    s32 diff_data;
};
struct dumpinfo g_dumpinfo;
u8 g_channel = 0;
const int g_bitoffset = 4;
const int g_bitshift = 10;
enum channel_info {
    CH0,
    CH1,
    CH2,
    CH3,
    CH4,
};
/*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 start*/
const char g_ch1_mask = 0x01;
const char g_ch2_mask = 0x02;
const char g_ch3_mask = 0x04;
const char g_all_mask = 0x07;

#ifdef SX933X_USER_NODE
const int g_irq_maxnum = 28;
const int g_cali_maxnum = 2;
static bool g_anfr_flag = true;
static int g_cali_count = 0;
static int g_irq_count = 0;
static int gs_last_state = 0;
#endif
/*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 end*/
/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/

typedef struct variables_s
{
    u16 reading_reg;
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
    u8 avg_pos_flt[NUM_PHASES];
    u8 avg_neg_flt[NUM_PHASES];
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
    u32 irq_mask;
    u32 enabled_phases;
}variables_t, *variables_p;

typedef struct debug_flag_s
{
    //log raw data (useful, diff, etc.) when proximity status is changed
    char log_raw_data;
    char log_dbg_data;
    char log_hex_data;
}debug_flag_t;

typedef struct self_s
{
    struct i2c_client *client;
    int irq_id; /* irq number used */
    int gpio;
    int num_regs;   //Number of registers need to be configured. Parsed from dts
    reg_addr_val_t *regs_addr_val;

    phase_t *phases;

    /*Each bit represents whether its correspoding phase is used for(1) this runction or not(0)
    for example: main_phases=0x5=0b0101, means PHASE 2 and 0 are used as the main phase*/
    u32 main_phases;
    u32 ref_phases;
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
    u32 fw_num;
    bool irq_wake_status;
    char channel_status;
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/

    //phases are for temperature reference correction
    int ref_phase_a;
    int ref_phase_b;
    int ref_phase_c;

    variables_t variables;
    debug_flag_t dbg_flag;

    spinlock_t lock; /* Spin Lock used for nirq worker function */
#if !USE_THREAD_IRQ
    struct delayed_work worker; /* work struct for worker function */
#endif

#if ENABLE_STAY_AWAKE
    struct wakeup_source *wake_lock;
#endif

#if ENABLE_ESD_RECOVERY
    struct delayed_work esd_worker;
#endif

    struct mutex phen_lock;

    //Function that will be called back when corresponding IRQ is raised
    //Refer to register 0x4000
    void (*irq_handler[NUM_IRQ_BITS])(struct self_s* self);
    /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
    #ifdef SX933X_USER_NODE
    struct device *factory_device;
    struct device *factory_device_sub;
    struct device *factory_device_wifi;
    #endif
    /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
}self_t, *Self;

static void smtc_log_raw_data(Self self);

/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
static void smtc_enable_avg_flt(Self self, int ph, bool enable);
static void smtc_update_avg_flt(Self self, u16 reg_addr, u32 reg_val);
static void smtc_record_avg_flt(Self self);

#ifdef SMTC_SX933X
#define SMTC_GET_AVG_POS(reg_val) (reg_val>>8  & 0x7)
#define SMTC_GET_AVG_NEG(reg_val) (reg_val>>11 & 0x7)
static inline u32 smtc_combine_avg_flt(u32 reg_val, u8 pos, u8 neg){
    return (reg_val & ~0x3F00) | neg<<11 | pos<<8;
}

static inline u32 smtc_mask_avg_flt(u32 reg_val){
    return (reg_val & ~0x3F00);
}

#else
#define GET_AVG_POS(reg_val) (reg_val>>8  & 0xF)
#define GET_AVG_NEG(reg_val) (reg_val>>12 & 0x7)

static inline u32 smtc_combine_avg_flt(u32 reg_val, u8 pos, u8 neg){
    return (reg_val & ~0x7F00) | neg<<12 | pos<<8;
}

static inline u32 smtc_mask_avg_flt(u32 reg_val){
    return (reg_val & ~0x7F00);
}
#endif    //SMTC_SX933X
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
//#################################################################################################

//The value of all other variables not configured in the table are 0.
/* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 start */
/* a06 code for AL7160A-32 by huofudong at 20240401 start*/
static Self g_self;
#if defined(SMTC_SX933X)
static phase_t smtc_phase_table[] =
{
    {
        .name = "grip_sensor_sub",
        .keycode_close = KEY_SAR1_CLOSE,
        .keycode_far = KEY_SAR1_FAR,
        .phase_status = 0x01,
        .prox1_mask = 1 << 24,
        .prox2_mask = 1 << 16,
        .prox3_mask = 1 << 8
    },
    {
        .name = "grip_sensor",
        .keycode_close = KEY_SAR2_CLOSE,
        .keycode_far = KEY_SAR2_FAR,
        .phase_status = 0x02,
        .prox1_mask = 1 << 25,
        .prox2_mask = 1 << 17,
        .prox3_mask = 1 << 9
    },
    {
        .name = "grip_sensor_wifi",
        .keycode_close = KEY_SAR3_CLOSE,
        .keycode_far = KEY_SAR3_FAR,
        .phase_status = 0x04,
        .prox1_mask = 1 << 26,
        .prox2_mask = 1 << 18,
        .prox3_mask = 1 << 10
    },
    {
        .name = "SMTC SAR Sensor CH3",
        .prox1_mask = 1 << 27,
        .prox2_mask = 1 << 19,
        .prox3_mask = 1 << 11,
    },
    {
        .name = "SMTC SAR Sensor CH4",
        .prox1_mask = 1 << 28,
        .prox2_mask = 1 << 20,
        .prox3_mask = 1 << 12
    },
    {
        .name = "SMTC SAR Sensor CH5",
        .prox1_mask = 1 << 29,
        .prox2_mask = 1 << 21,
        .prox3_mask = 1 << 13
    }
};

#else
static phase_t smtc_phase_table[] =
{
    {
        .name = "SMTC SAR Sensor CH0",
        .prox1_mask = 1 << 24,
        .prox2_mask = 1 << 16,
        .prox3_mask = 1 << 8,
        .prox4_mask = 1 << 0
    },
    {
        .name = "SMTC SAR Sensor CH1",
        .prox1_mask = 1 << 25,
        .prox2_mask = 1 << 17,
        .prox3_mask = 1 << 9,
        .prox4_mask = 1 << 1
    },
    {
        .name = "SMTC SAR Sensor CH2",
        .prox1_mask = 1 << 26,
        .prox2_mask = 1 << 18,
        .prox3_mask = 1 << 10,
        .prox4_mask = 1 << 2
    },
    {
        .name = "SMTC SAR Sensor CH3",
        .prox1_mask = 1 << 27,
        .prox2_mask = 1 << 19,
        .prox3_mask = 1 << 11,
        .prox4_mask = 1 << 3
    },
    {
        .name = "SMTC SAR Sensor CH4",
        .prox1_mask = 1 << 28,
        .prox2_mask = 1 << 20,
        .prox3_mask = 1 << 12,
        .prox4_mask = 1 << 4
    },
    {
        .name = "SMTC SAR Sensor CH5",
        .prox1_mask = 1 << 29,
        .prox2_mask = 1 << 21,
        .prox3_mask = 1 << 13,
        .prox4_mask = 1 << 5
    },
    {
        .name = "SMTC SAR Sensor CH6",
        .prox1_mask = 1 << 30,
        .prox2_mask = 1 << 22,
        .prox3_mask = 1 << 14,
        .prox4_mask = 1 << 6
    },
    {
        .name = "SMTC SAR Sensor CH7",
        .prox1_mask = 1 << 31,
        .prox2_mask = 1 << 23,
        .prox3_mask = 1 << 15,
        .prox4_mask = 1 << 7
    }
};
#endif
/* a06 code for AL7160A-32 by huofudong at 20240401 end*/
/* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 end */

static inline u32 smtc_get_chip_id(u32 chip_id){
    return (chip_id >> 8) & 0xFFFF;
}

//#define LOG_CHIP_NAME ""
#define LOG_CHIP_NAME SMTC_CHIP_NAME "."
#if EXTRA_LOG_TIME
static char* smtc_get_log_time(void)
{
    struct timex txc;
    struct rtc_time tm;
    int ms;
#if EXTRA_LOG_DATE_TIME
    static char log_time[] = "11-30 11:29:00.123";
#else
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
    static char log_time[] = "11:29:00.1234567890";
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/
#endif

    do_gettimeofday(&txc.time);
    rtc_time_to_tm(txc.time.tv_sec, &tm);

    ms = txc.time.tv_usec;
    while(ms > 999){
        ms = (int)(ms/1000);
    }

#if EXTRA_LOG_DATE_TIME
    sprintf(log_time, "%02d-%02d %02d:%02d:%02d.%03d",
        tm.tm_mon, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec, ms);
#else
    sprintf(log_time, "%02d:%02d:%02d.%03d",
        tm.tm_hour, tm.tm_min, tm.tm_sec, ms);
#endif

    return log_time;
}
#define SMTC_LOG_DBG(fmt, args...)   pr_info(" %s D "  LOG_CHIP_NAME "%s(%d):" fmt "\n", smtc_get_log_time(), __func__, __LINE__, ##args)
#define SMTC_LOG_INF(fmt, args...)   pr_info (" %s I "  LOG_CHIP_NAME "%s(%d):" fmt "\n", smtc_get_log_time(), __func__, __LINE__, ##args)
#define SMTC_LOG_WRN(fmt, args...)   pr_warn (" %s W "  LOG_CHIP_NAME "%s(%d):" fmt "\n", smtc_get_log_time(), __func__, __LINE__, ##args)
#define SMTC_LOG_ERR(fmt, args...)   pr_err  (" %s E "  LOG_CHIP_NAME "%s(%d):" fmt "\n", smtc_get_log_time(), __func__, __LINE__, ##args)

#else //EXTRA_LOG_TIME
#define SMTC_LOG_DBG(fmt, args...)   pr_debug(" D "  LOG_CHIP_NAME "%s(%d):" fmt "\n", __func__, __LINE__, ##args)
#define SMTC_LOG_INF(fmt, args...)   pr_info (" I "  LOG_CHIP_NAME "%s(%d):" fmt "\n", __func__, __LINE__, ##args)
#define SMTC_LOG_WRN(fmt, args...)   pr_warn (" W "  LOG_CHIP_NAME "%s(%d):" fmt "\n", __func__, __LINE__, ##args)
#define SMTC_LOG_ERR(fmt, args...)   pr_err  (" E "  LOG_CHIP_NAME "%s(%d):" fmt "\n", __func__, __LINE__, ##args)
#endif //EXTRA_LOG_TIME


//#################################################################################################
static int smtc_i2c_write(Self self, u16 reg_addr, u32 reg_val)
{
    int ret = -ENOMEM;
    int num_retried = 0;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg;
    unsigned char w_buf[6];

    i2c = self->client;
    w_buf[0] = (u8)(reg_addr>>8);
    w_buf[1] = (u8)(reg_addr);
    w_buf[2] = (u8)(reg_val>>24);
    w_buf[3] = (u8)(reg_val>>16);
    w_buf[4] = (u8)(reg_val>>8);
    w_buf[5] = (u8)(reg_val);

    msg.addr = i2c->addr;
    msg.flags = I2C_M_WR_SMTC;
    msg.len = 6; //2 bytes regaddr + 4 bytes data
    msg.buf = (u8 *)w_buf;

    while(true)
    {
        ret = i2c_transfer(i2c->adapter, &msg, 1);
        if (ret > 0)
            break;

        if (num_retried++ < NUM_RETRY_ON_I2C_ERR)
        {
            SMTC_LOG_ERR("i2c write reg 0x%x error %d. Goint to retry", reg_addr, ret);
            if(SLEEP_BETWEEN_RETRY != 0)
                msleep(SLEEP_BETWEEN_RETRY);
        }
        else{
            SMTC_LOG_ERR("i2c write reg 0x%x error %d after retried %d times", reg_addr, ret, NUM_RETRY_ON_I2C_ERR);
             break;
        }
    }
    
    return ret;
}

//=================================================================================================
//return number of bytes read from the i2c device or error codes
static int smtc_i2c_read(Self self, u16 reg_addr, u32 *reg_val)
{
    int ret = -ENOMEM;
    int num_retried = 0;
    struct i2c_client *i2c = 0;
    struct i2c_msg msg[2];
    u8 w_buf[2];
    u8 buf[4];
    

    if (self && self->client)
    {
        i2c = self->client;
        w_buf[0] = (u8)(reg_addr>>8);
        w_buf[1] = (u8)(reg_addr);

        msg[0].addr = i2c->addr;
        msg[0].flags = I2C_M_WR_SMTC;
        msg[0].len = 2;
        msg[0].buf = (u8 *)w_buf;

        msg[1].addr = i2c->addr;;
        msg[1].flags = I2C_M_RD_SMTC;
        msg[1].len = 4;
        msg[1].buf = (u8 *)buf;
            
        while(true)
        {
            ret = i2c_transfer(i2c->adapter, msg, 2);
            if (ret > 0){
                *reg_val = (u32)buf[0]<<24 | (u32)buf[1]<<16 | (u32)buf[2]<<8 | (u32)buf[3];
                break;
            }
                
            if (num_retried++ < NUM_RETRY_ON_I2C_ERR)
            {
                SMTC_LOG_ERR("i2c read reg 0x%x error %d. Goint to retry", reg_addr, ret);
                if(SLEEP_BETWEEN_RETRY != 0)
                    msleep(SLEEP_BETWEEN_RETRY);
            }
            else{
                SMTC_LOG_ERR("i2c read reg 0x%x error %d after retried %d times", 
                    reg_addr, ret, NUM_RETRY_ON_I2C_ERR);
                 break;
            }
       }

    }

    if (reg_addr == 0x4000){
        SMTC_LOG_DBG("reg_addr=0x%X reg_val=0x%X ret=%d", reg_addr, *reg_val, ret);
    }
    return ret;
}

//=================================================================================================
static int smtc_send_cmd(Self self, COMMANDS cmd)
{
    int retry_times=100, state, ret;
    while(1)
    {
        ret = smtc_i2c_read(self, REG_CMD_STATE, &state);
        if (ret < 0){
            SMTC_LOG_ERR("Failed to send command.");
            return -EIO;
        }
        if ((state & 1) == 0){
            SMTC_LOG_DBG("Chip is free and capable to process new command.");
            break;
        }
        
        if (--retry_times == 0){
            SMTC_LOG_WRN("Chip keeps busy after 10 times retry.");
            break;
        }
        msleep(10);
        SMTC_LOG_DBG("Chip is busy, go to retry.");
    }

    ret = smtc_i2c_write(self, REG_CMD, (u32)cmd);
    if (ret < 0){
        SMTC_LOG_ERR("Failed to send command.");
        return -EIO;
    }

    return 0;
}

//#################################################################################################
static int smtc_read_and_clear_irq(Self self, u32 *irq_src)
{
    int ret = 0;
    u32 reg_val;

    ret = smtc_i2c_read(self, REG_IRQ_SRC, &reg_val);
    if (ret > 0)
    {
        reg_val &= 0xFF;
        SMTC_LOG_DBG("irq_src= 0x%X", reg_val);

        if (irq_src){
            *irq_src = reg_val;
        }
        ret = 0;
    }
    else if (ret==0){
        ret = -EIO;
    }

    return ret;
}

//=================================================================================================
static int smtc_calibrate(Self self)
{
    int ret = 0;
    SMTC_LOG_INF("Enter");

    ret = smtc_send_cmd(self, CMD_COMPENSATE);
    if (ret){
        SMTC_LOG_ERR("Failed to calibrate. ret=%d", ret);
    }
    return ret;
}

//=================================================================================================
static int smtc_is_irq_low(Self self)
{
    return !gpio_get_value(self->gpio);
}


//#################################################################################################
static ssize_t smtc_calibration_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{    
    int i, count=0, ph, shift;
    u32 reg_val = 0;
    u16 offset;
    Self self = dev_get_drvdata(dev);

    for (i=0; i<60; i++)
    {
        smtc_i2c_read(self, 0x8004,  &reg_val);
        if (reg_val & COMPENSATING_MASK){
            msleep(50);
        }else{
            break;
        }
    }

    if (i==30){
        count = sprintf(buf, "%s", "chip is still compensating after waiting for 3 seconds.");
        SMTC_LOG_WRN("%s", buf);
        return count;
    }else if (i!=0){
        SMTC_LOG_INF("chip completed the compensation after waiting for %d ms", i*100);
    }

    for(ph =0; ph < NUM_PHASES; ph++)
    {
        shift = ph*4;
        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);

        count += sprintf(buf+count, "PH%d=%d ", ph, offset);
    }
    count += sprintf(buf+count, "\n");
    SMTC_LOG_INF("%s", buf);
    return count;
}

//-------------------------------------------------------------------------------------------------
static ssize_t smtc_calibration_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    Self self = dev_get_drvdata(dev);
    SMTC_LOG_INF("Enter");
    smtc_calibrate(self);
    return count;
}

//=================================================================================================
static ssize_t smtc_reg_write_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u32 reg_addr = 0, reg_val = 0;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%x,%x", &reg_addr, &reg_val) != 2)
    {
        SMTC_LOG_ERR("Invalid command format. Example: ehco '0x4280,0xE' > reg_write");
        return -EINVAL;
    }
    SMTC_LOG_INF("0x%X= 0x%X", reg_addr, reg_val);

    if (reg_addr == REG_PHEN)
    {
        mutex_lock(&self->phen_lock);
        self->variables.enabled_phases = reg_val;
        smtc_i2c_write(self, reg_addr, reg_val);
        mutex_unlock(&self->phen_lock);
    }
    else{
        smtc_i2c_write(self, reg_addr, reg_val);
    }
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
    smtc_update_avg_flt(self, reg_addr, reg_val);
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/

    return count;
}
//-------------------------------------------------------------------------------------------------
static ssize_t smtc_reg_write_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf,
        "\nUsage: echo reg_addr,reg_val > reg_write\n"
        "Example: echo 0x4004,0x74 > reg_write\n");
}

//=================================================================================================
static ssize_t smtc_reg_read_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    int ret;
    u32 reg_val=0;
    u16 reg_addr = 0;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%x", &reg_addr) != 1)
    {
        SMTC_LOG_ERR(
            "echo reg_addr > reg_read; cat reg_read\n"
            "Example: echo 0x4000 > reg_read; cat reg_read\n");
        return -EINVAL;
    }

    self->variables.reading_reg = reg_addr;
    ret = smtc_i2c_read(self, reg_addr, &reg_val);

    if (ret < 0){
        SMTC_LOG_ERR("Failed to read register 0x%X", reg_addr);
    }else{
        SMTC_LOG_INF("0x%X= 0x%X", reg_addr, reg_val);
    }

    return count;
}
//-------------------------------------------------------------------------------------------------        
static ssize_t smtc_reg_read_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{    
    int count=0, ret;
    u32 reg_val = 0, reading_reg;
    Self self = NULL;
    self = dev_get_drvdata(dev);
    reading_reg = self->variables.reading_reg;

    if (reading_reg == 0xFFFF){
        count = sprintf(buf, 
            "\nUsage: echo reg_addr > reg_read; cat reg_read\n"
            "Example: echo 0x4000 > reg_read; cat reg_read\n");
    }else{
        ret = smtc_i2c_read(self, reading_reg, &reg_val);

        if (ret < 0){
            count = sprintf(buf, "Failed to read reg=0x%X\n", reading_reg);
        }else{
            count = sprintf(buf, "0x%X= 0x%X\n", reading_reg, reg_val);
        }
    }

    return count;
}

//=================================================================================================
static ssize_t smtc_raw_data_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int ph, bytes=0;
    u32 reg_val;
    u16 offset;
    s32 useful, average, diff;
    Self self = dev_get_drvdata(dev);

    for (ph=0; ph<NUM_PHASES; ph++)
    {
        int shift = ph*4;
        smtc_i2c_read(self, REG_PH0_USEFUL  + shift,    &reg_val);
        useful = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_AVERAGE + shift,    &reg_val);
        average = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_DIFF    + shift,    &reg_val);
        diff = (s32)reg_val>>10;

        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);

        bytes += sprintf(buf+bytes, "ph= %d useful= %d average= %d diff= %d offset= %d\n",
            ph, useful, average, diff, offset);

    }
    smtc_log_raw_data(self);

    return bytes;
}

//=================================================================================================
static void smtc_test_func(Self self)
{
    SMTC_LOG_INF("You can modify this function freely to test any of driver functionality.");
}

static ssize_t smtc_debug_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    int cmd, arg;
    Self self = dev_get_drvdata(dev);

    if (sscanf(buf, "%d,%d", &cmd, &arg) != 2)
    {
        SMTC_LOG_ERR("Invalid command format. Use 'cat debug' to show the usages.");
        return -EINVAL;
    }

    switch (cmd){
    case 0:
        smtc_test_func(self);
        break;
    case 1:
        self->dbg_flag.log_raw_data = arg;
        SMTC_LOG_INF("%s log raw data when proximity status is changed", arg ? "Enable" : "Disable");
        break;
    case 2:
        self->dbg_flag.log_dbg_data = arg;
        SMTC_LOG_INF("%s log debug data", arg ? "Enable" : "Disable");
        break;
    case 3:
        self->dbg_flag.log_hex_data = arg;
        SMTC_LOG_INF("%s log hex data", arg ? "Enable" : "Disable");
        break;

   default:
        SMTC_LOG_ERR("Invalid command=%d. Use 'cat debug' to show the usages.", cmd);
    }

    return count;
}

//-------------------------------------------------------------------------------------------------
static ssize_t smtc_debug_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s",
        "Usage: echo 'cmd,arg' > debug\n"
        "cmd:\n"
        "1: Turn on(arg=1) | off(arg=0), default=0\n"
        "   Log raw data when proximity status is changed"
        "2: Turn on(arg=1) | off(arg=0), default=1\n"
        "   Log debug data, such as PROX RAW, UseFilter DeltaVar, etc."
        "3: Turn on(arg=1) | off(arg=0), default=0\n"
        "   Log some of registers value(Useful, average, etc.) in hex format.\n"
    );
}

//=================================================================================================
//read and print the value of registers in the dts
static ssize_t smtc_dump_reg_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{  
    int i, bytes=0;
    u32 addr, val;
    Self self = dev_get_drvdata(dev);

    for (i=0; i<self->num_regs; i++)
    {
        addr = self->regs_addr_val[i].addr;
        
        if (smtc_i2c_read(self, addr, &val) > 0){
            bytes += sprintf(buf+bytes, "0x%X= 0x%08X\n", addr, val);
        }else{
            bytes += sprintf(buf+bytes, "0x%X= FAILED\n", addr);
        }
    }
    return bytes;
}

static ssize_t smtc_irq_gpio_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{  
    Self self = dev_get_drvdata(dev);
    int value = gpio_get_value(self->gpio);
    return sprintf(buf, "GPIO-%d=%d %s\n", self->gpio, value, value ? "hi" : "lo");
}

//=================================================================================================
static DEVICE_ATTR(cali,           0664, smtc_calibration_show,    smtc_calibration_store);
static DEVICE_ATTR(reg_write,      0664, smtc_reg_write_show,      smtc_reg_write_store);
static DEVICE_ATTR(reg_read,       0664, smtc_reg_read_show,       smtc_reg_read_store);
static DEVICE_ATTR(raw_data,       0444, smtc_raw_data_show,       NULL);
static DEVICE_ATTR(debug,          0664, smtc_debug_show,          smtc_debug_store);
static DEVICE_ATTR(dump_reg,       0444, smtc_dump_reg_show,       NULL);
static DEVICE_ATTR(irq_gpio,       0444, smtc_irq_gpio_show,       NULL);

static struct attribute *smtc_sysfs_nodes_attr[] =
{
    &dev_attr_cali.attr,
    &dev_attr_reg_write.attr,
    &dev_attr_reg_read.attr,
    &dev_attr_raw_data.attr,
    &dev_attr_debug.attr,
    &dev_attr_dump_reg.attr,
    &dev_attr_irq_gpio.attr,
    NULL,
};
static struct attribute_group smtc_sysfs_nodes =
{
    .attrs = smtc_sysfs_nodes_attr,
};

/*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 start*/
//=================================================================================================
//ANFR
#ifdef SX933X_USER_NODE
static void smtc_anfr_irq(Self self)
{
    u32 prox_state = 0;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);
    if (((prox_state >> 24) & 1) != gs_last_state) {
        g_irq_count++;
        gs_last_state = (prox_state >> 24) & 1;
        SMTC_LOG_INF("state update %d, g_irq_count= %d", gs_last_state, g_irq_count);
    } else {
        SMTC_LOG_INF("state not update %d", gs_last_state);
    }
}

static void smtc_anfr_forcereport(Self self)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;
    /*A06 code for AL7160A-1650 | AL7160A-2616 by jiashixian at 20240604 start*/
    u32 prox_state = 0;
    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN) {
            continue;
        }
        input = phase->input;
        if ((ph == 0) || (ph == 1)){
            phase->state = PROX1;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 1);
                    input_report_rel(input, REL_X, 1);
                    input_sync(input);
                }
            }
            SMTC_LOG_INF("%s ANFR reports touch", phase->name);
        } else {
                smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
                SMTC_LOG_INF("prox_state= 0x%X", prox_state);
                if (prox_state & phase->prox1_mask) {
                    if (phase->state == PROX1){
                        SMTC_LOG_DBG("%s is PROX1 already", phase->name);
                    } else {
                        SMTC_LOG_INF("%s ANFR reports PROX1 , onoff:%d", phase->name, g_onoff_enable);
                        phase->state = PROX1;
                        if (g_onoff_enable == false) {
                            if ((self->channel_status) & (phase->phase_status)) {
                                input_report_rel(input, REL_MISC, 1);
                                /*A06 code for AL7160A-3004 by jiashixian at 20240613 start*/
                                input_report_rel(input, REL_X, 2);
                                /*A06 code for AL7160A-3004 by jiashixian at 20240613 end*/
                                input_sync(input);
                            }
                        }
                    }
                } else {
                        if (phase->state == RELEASED) {
                            SMTC_LOG_DBG("%s is RELEASED already", phase->name);
                        } else {
                            SMTC_LOG_INF("%s ANFR reports release, onoff:%d", phase->name, g_onoff_enable);
                            phase->state = RELEASED;
                            if (g_onoff_enable == false) {
                                if ((self->channel_status) & (phase->phase_status)) {
                                    input_report_rel(input, REL_MISC, 2);
                                    /*A06 code for AL7160A-3004 by jiashixian at 20240613 start*/
                                    input_report_rel(input, REL_X, 2);
                                    /*A06 code for AL7160A-3004 by jiashixian at 20240613 end*/
                                    input_sync(input);
                                    }
                                }
                        }
                }
        }
    }
    /*A06 code for AL7160A-1650 | AL7160A-2616 by jiashixian at 20240604 by jiashixian at 20240528 end*/
}
#endif
/*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 end*/

/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
#ifdef SX933X_USER_NODE
static void sx_smtc_enable_report(Self self, struct input_dev *temp_input)
{
    int ph = 0;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        if (strcmp(phase->input->name, temp_input->name) != 0) {
            continue;
        }

        input = phase->input;
        if (prox_state & phase->prox1_mask) {
            SMTC_LOG_INF("%s reports PROX1 , onoff:%d", phase->name, g_onoff_enable);
            phase->state = PROX1;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 1);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                }
            }
        } else {
            SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
            phase->state = RELEASED;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 2);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                }
            }
        }
    }
}

static void sx_smtc_disable_report(Self self, struct input_dev *temp_input)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 0; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        if (strcmp(phase->input->name, temp_input->name) != 0) {
            continue;
        }

        input = phase->input;
        phase->state = RELEASED;
        if (g_onoff_enable == false) {
            if (((self->channel_status) & (phase->phase_status)) == 0) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
        SMTC_LOG_INF("%s reports release", phase->name);
    }
}

static ssize_t sx_smtc_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    Self self = dev_get_drvdata(dev);
    int status = -1;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    SMTC_LOG_INF("dev->name:%s", temp_input_dev->name);

    if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
        if(self->channel_status & g_ch1_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
        if(self->channel_status & g_ch2_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
        if(self->channel_status & g_ch3_mask) {
            status = 1;
        } else {
            status = 0;
        }
    }

    return snprintf(buf, 8, "%d\n", status);
}

static ssize_t sx_smtc_enable_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    Self self = dev_get_drvdata(dev);
    u8 enable = 0;
    int ret = 0;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        SMTC_LOG_ERR("Invalid Argument");
        return ret;
    }

    SMTC_LOG_INF("dev->name:%s new_value = %u", temp_input_dev->name, enable);

    if (enable == 1) {
        if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
            if (!(self->channel_status & g_ch1_mask)) {
                self->channel_status |= g_ch1_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
            if (!(self->channel_status & g_ch2_mask)) {
                self->channel_status |= g_ch2_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
            if (!(self->channel_status & g_ch3_mask)) {
                self->channel_status |= g_ch3_mask;
            }
        } else {
            SMTC_LOG_ERR("input_dev error");
        }
        SMTC_LOG_INF("self->channel_status = 0x%x", self->channel_status);

        if (self->channel_status & g_all_mask) {
            g_Enabled = true;
        }

        if (self->irq_wake_status != true) {
            enable_irq_wake(self->irq_id);
            self->irq_wake_status = true;
            SMTC_LOG_INF("enable irq wake success.");
        }
        /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 start*/
        #ifdef SX933X_USER_NODE
        if (g_anfr_flag) {
            smtc_anfr_forcereport(g_self);
        } else {
            sx_smtc_enable_report(self, temp_input_dev);
        }
        #else
        {
            sx_smtc_enable_report(self, temp_input_dev);
        }
        #endif
        /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 end*/
    } else if (enable == 0) {
        if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
            if (self->channel_status & g_ch1_mask) {
                self->channel_status &= ~g_ch1_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
            if (self->channel_status & g_ch2_mask) {
                self->channel_status &= ~g_ch2_mask;
            }
        } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
            if (self->channel_status & g_ch3_mask) {
                self->channel_status &= ~g_ch3_mask;
            }
        } else {
            SMTC_LOG_ERR("input_dev error");
        }

        if (!(self->channel_status & g_all_mask)) {
            g_Enabled = false;
        }

        if (self->irq_wake_status != false) {
            disable_irq_wake(self->irq_id);
            self->irq_wake_status = false;
            SMTC_LOG_INF("disable irq wake success.");
        }

        sx_smtc_disable_report(self, temp_input_dev);
    } else {
        SMTC_LOG_ERR("Invalid param. Usage: ehco 0/1 > enable");
    }

    return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        sx_smtc_enable_show, sx_smtc_enable_store);

static struct attribute *ss_smtc_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group ss_smtc_attributes_group = {
    .attrs = ss_smtc_attributes
};

static ssize_t smtc_vendor_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t smtc_name_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t smtc_grip_flush_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val = 0;
    int ret = 0;
    int ph = 0;
    phase_p phase = NULL;
    struct input_dev *input = NULL;

    ret = kstrtou8(buf, 10, &val);
    if (ret < 0) {
        SMTC_LOG_INF("Invalid value of input, input=%ld", ret);
        return -EINVAL;
    }

    for (ph = 0; ph < (sizeof(smtc_phase_table)/sizeof(smtc_phase_table[0])); ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN) {
            continue;
        }

        input = phase->input;

        if (!strcmp(dev->kobj.name, phase->name)) {

            if (g_onoff_enable == false) {
                input_report_rel(input, REL_MAX, val);
                input_sync(input);
            }

            SMTC_LOG_INF("ret=%u, name=%s", val, phase->name);
            return count;
        }
    }

    SMTC_LOG_INF("unknown type");

    return count;
}

static DEVICE_ATTR(name, S_IRUGO, smtc_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, smtc_vendor_show, NULL);
static DEVICE_ATTR(grip_flush, S_IWUSR | S_IWGRP, NULL, smtc_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_grip_flush,
    NULL,
};
#endif
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
//#################################################################################################
static void smtc_get_ph_prox_state(u32 prox_reg_val, PROX_STATUS ph_state[NUM_PHASES])
{
    int ph;
    phase_p phase;
    
    for (ph=0; ph<NUM_PHASES; ph++)
    {
        phase = &smtc_phase_table[ph];
        //The prox4_mask of sx933x is always 0
        if (prox_reg_val & phase->prox4_mask){
            ph_state[ph] = PROX4;
        }
        else if (prox_reg_val & phase->prox3_mask){
            ph_state[ph] = PROX3;
        }
        else if (prox_reg_val & phase->prox2_mask){
            ph_state[ph] = PROX2;
        }
        else if (prox_reg_val & phase->prox1_mask){
            ph_state[ph] = PROX1;
        }
        else{
            ph_state[ph] = RELEASED;
        }
    }
}

static void smtc_init_variables(Self self)
{
    /*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
    mutex_init(&self->phen_lock);
    /*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
    self->dbg_flag.log_raw_data = 0;
    self->dbg_flag.log_dbg_data = 1;
    self->dbg_flag.log_hex_data = 0;
    self->variables.reading_reg = REG_WHO_AMI;

    self->variables.irq_mask = 0x70;
    self->variables.enabled_phases = 0x0;
}

/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
static void smtc_enable_avg_flt(Self self, int ph, bool enable)
{
    u32 reg_val, new_reg_val;
    u16 reg_addr;
    u8 pos_flt, neg_flt;

    reg_addr = REG_AVG_FLT_PH0 + ph*AVG_FLT_PH_OFF;
    smtc_i2c_read(self, reg_addr, &reg_val);

    if (enable)
    {
        pos_flt = self->variables.avg_pos_flt[ph];
        neg_flt = self->variables.avg_neg_flt[ph];

        new_reg_val = smtc_combine_avg_flt(reg_val, pos_flt, neg_flt);
        SMTC_LOG_INF("Enable average filter pos=0x%X neg=0x%X ph=%d reg_addr=0x%X reg_val=0x%X==>0x%X",
            pos_flt, neg_flt, ph, reg_addr, reg_val, new_reg_val);
    }
    else
    {
        new_reg_val = smtc_mask_avg_flt(reg_val);
        SMTC_LOG_INF("Disable average filter ph=%d reg_addr=0x%X reg_val=0x%X==>0x%X",
            ph, reg_addr, reg_val, new_reg_val);
    }

    smtc_i2c_write(self, reg_addr, new_reg_val);
}

static void smtc_update_avg_flt(Self self, u16 reg_addr, u32 reg_val)
{
    int ph=0;
    variables_p var = &self->variables;

    for(ph=0; ph<NUM_PHASES; ph++)
    {
        if (reg_addr == REG_AVG_FLT_PH0 + ph*AVG_FLT_PH_OFF)
            break;
    }

    if (ph < NUM_PHASES)
    {
        var->avg_pos_flt[ph] = SMTC_GET_AVG_POS(reg_val);
        var->avg_neg_flt[ph] = SMTC_GET_AVG_NEG(reg_val);

        SMTC_LOG_INF("Update average filter ph= %d reg_addr= 0x%X reg_val= 0x%X pos= 0x%X neg= 0x%X",
            ph, reg_addr, reg_val, var->avg_pos_flt[ph], var->avg_neg_flt[ph]);
    }
}
static void smtc_record_avg_flt(Self self)
{
    int ph;
    u32 reg_val;
    variables_p var = &self->variables;

    for (ph=0; ph<NUM_PHASES; ph++)
    {
        smtc_i2c_read(self, REG_AVG_FLT_PH0+ph*AVG_FLT_PH_OFF, &reg_val);
        var->avg_pos_flt[ph] = SMTC_GET_AVG_POS(reg_val);
        var->avg_neg_flt[ph] = SMTC_GET_AVG_NEG(reg_val);
    }
}
#else
#define smtc_enable_avg_flt(...) ((void)0)
#define smtc_record_avg_flt(...) ((void)0)
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
//=================================================================================================

static int smtc_parse_dts(Self self)
{
    int ph;
    struct device_node *of_node = self->client->dev.of_node;
    enum of_gpio_flags flags;

    if (!of_node)
    {
        SMTC_LOG_ERR("of_node == NULL");
        return -EINVAL;
    }

    self->gpio = of_get_named_gpio_flags(of_node, "Semtech,nirq-gpio", 0, &flags);
    SMTC_LOG_DBG("irq_gpio= %d", self->gpio);

    if (self->gpio < 0)
    {
        SMTC_LOG_ERR("Failed to get irq_gpio.");
        return -EINVAL;
    }

#if WAKE_BY_IRQ
    if (!of_get_property(of_node, "wakeup-source", NULL)){
        SMTC_LOG_ERR("wakeup-source must be enabled in the dtsi.");
        return -EINVAL;
    }
#endif
    //.............................................................................................
    //load main and reference phase setup
    if (of_property_read_u32(of_node, "semtech,main-phases", &self->main_phases) )
    {
        SMTC_LOG_ERR("Failed to get main-phases");
        return -EINVAL;
    }
    SMTC_LOG_DBG("main-phases= 0x%X", self->main_phases);


    if (of_property_read_u32(of_node, "semtech,ref-phases", &self->ref_phases) )
    {
        SMTC_LOG_ERR("Failed to get ref-phases");
        return -EINVAL;
    }
    SMTC_LOG_DBG("ref-phases= 0x%X", self->ref_phases);
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
    if (of_property_read_u32(of_node, "semtech,fw-num", &self->fw_num)) {
        SMTC_LOG_ERR("Failed to get fw-num");
        return -EINVAL;
    }
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/


    self->ref_phase_a = -1;
    self->ref_phase_b = -1;
    self->ref_phase_c = -1;

    for (ph=0; ph<NUM_PHASES; ph++)
    {
        if (1<<ph & self->ref_phases){
            if (self->ref_phase_a==-1){
                self->ref_phase_a = ph;
            }
            else if (self->ref_phase_b==-1){
                self->ref_phase_b = ph;
            }
            else if (self->ref_phase_c==-1){
                self->ref_phase_c = ph;
            }
            else{
                SMTC_LOG_ERR("Max 3 reference phases are supported. ref_phases passed from dts= 0x%X",
                    self->ref_phases);
                return -EINVAL;
            }
        }
    }
    SMTC_LOG_INF("ref_phase_a= %d ref_phase_b= %d ref_phase_c= %d",
        self->ref_phase_a, self->ref_phase_b, self->ref_phase_c);

    //.............................................................................................
    //load register settings
    self->num_regs = of_property_count_u32_elems(of_node, "Semtech,reg-init");
    SMTC_LOG_DBG("number of registers= %d", self->num_regs);

    if (unlikely(self->num_regs <= 0 || self->num_regs %2 != 0))
    {
        SMTC_LOG_ERR("Invalid reg_num= %d, please check dtsi config", self->num_regs);
        return -EINVAL;
    }
    else
    {
        self->num_regs /= 2;
        self->regs_addr_val = devm_kzalloc(&self->client->dev, sizeof(reg_addr_val_t)*self->num_regs, GFP_KERNEL);
        if (unlikely(!self->regs_addr_val))
        {
            SMTC_LOG_ERR("Failed to alloc memory, num_reg= %d", self->num_regs);
            return -ENOMEM;
        }

        if (of_property_read_u32_array(of_node, "Semtech,reg-init",
                                (u32*)self->regs_addr_val,
                                sizeof(reg_addr_val_t) * self->num_regs/sizeof(u32)))
       {
            SMTC_LOG_ERR("Failed to load registers from the dts");
            return -EINVAL;
        }
    }

    return 0;
}

//=================================================================================================
static int smtc_check_hardware(Self self)
{
    int ret;
    u32 chip_id;

    ret = smtc_i2c_read(self, REG_WHO_AMI, &chip_id);
    if(ret < 0)
    {
        SMTC_LOG_ERR("Failed to read chip id. ret= %d.", ret);
        return ret;
    }

    chip_id = smtc_get_chip_id(chip_id);
    if (chip_id != SMTC_CHIP_ID){
        SMTC_LOG_ERR("read chip id 0x%X != 0x%X expected id.", chip_id, SMTC_CHIP_ID);
        return -ENODEV;
    }

    SMTC_LOG_INF("chip_id= 0x%X", chip_id);
    return 0;
}

//=================================================================================================
static int smtc_init_irq_gpio(Self self)
{
    int ret = 0;
    SMTC_LOG_DBG("");

    if (!gpio_is_valid(self->gpio))
    {
        SMTC_LOG_ERR("Invalid irq_gpio= %d. Please check DTS stup.", self->gpio);
        return -EINVAL;
    }
    
    ret = gpio_request(self->gpio, "smtc_nirq_gpio");
    if (ret < 0)
    {
        SMTC_LOG_ERR("Failed to request GPIO= %d", self->gpio);
        return ret;
    }

    ret = gpio_direction_input(self->gpio);
    if (ret < 0)
    {
        SMTC_LOG_ERR("Failed to set GPIO= %d as input", self->gpio);
        gpio_free(self->gpio);
        return ret;
    }

    self->irq_id = self->client->irq = gpio_to_irq(self->gpio);
    SMTC_LOG_DBG("Get irq= %d from gpio= %d", self->irq_id, self->gpio);

#if WAKE_BY_IRQ
    SMTC_LOG_INF("Enable wake up function on gpio=%d irq=%d", self->gpio, self->irq_id);
    device_init_wakeup(&self->client->dev, true);
    ret = dev_pm_set_wake_irq(&self->client->dev, self->irq_id);
    if (ret < 0){
        SMTC_LOG_INF("Failed to nable wake up function on gpio=%d irq=%d", self->gpio, self->irq_id);
    }
#endif

    return ret;
}

//=================================================================================================
static int smtc_init_registers(Self self)
{
    int i = 0, ret=0;
    u16 reg_addr;
    u32 reg_val;

    SMTC_LOG_INF("Going to init registers passed from the DTS");
    for (i=0; i < self->num_regs; i++)
    {
        reg_addr = self->regs_addr_val[i].addr;
        reg_val  = self->regs_addr_val[i].val;
        SMTC_LOG_DBG("0x%X= 0x%X", reg_addr, reg_val);

        if (reg_addr == 0x4004 && (reg_val & 1<<4)==0){
            SMTC_LOG_WRN("0x4004=0x%X, usually compensation IRQ should be enabled.");
        }

        if (likely(reg_addr != REG_PHEN)){
            ret = smtc_i2c_write(self, reg_addr, reg_val);
        }else{
            mutex_lock(&self->phen_lock);
            self->variables.enabled_phases = reg_val;
            ret = smtc_i2c_write(self, reg_addr, reg_val);
            mutex_unlock(&self->phen_lock);
        }

        if (ret <= 0){
            SMTC_LOG_ERR("Failed to write reg=0x%x value=0x%X", reg_addr, reg_val);
            return ret;
        }
    }

    /*Chip must be activated once after reset.
    You can disable all phases in the register settings if your product need to 
    put the SAR sensor into the sleep mode during startup
    */
    ret = smtc_send_cmd(self, CMD_ACTIVATE);
    if (ret){return ret;}

    return 0;
}

//=================================================================================================
static int smtc_wait_reset_done(Self self)
{
    int ret = 0, i;
    u32 irq_src=0;

    for (i=0; i<5; i++)
    {
        msleep(10);
        ret = smtc_i2c_read(self, REG_IRQ_SRC, &irq_src);
        if (ret > 0 && irq_src != 0){
            SMTC_LOG_INF("irq_src=0x%X", irq_src);
            return 0;
        }
    }

    SMTC_LOG_ERR("Failed to read reset IRQ");
    return ret;
}

static int smtc_reset_and_init_chip(Self self, bool from_probe)
{
    int ret = 0;
    SMTC_LOG_INF("Enter");

    if (!from_probe){
        disable_irq(self->irq_id);
    }
    ret = smtc_i2c_write(self, REG_RESET, 0xDE);
    if(ret<0){goto SUB_OUT;}

    ret = smtc_wait_reset_done(self);
    if(ret){goto SUB_OUT;}

    ret = smtc_init_registers(self);
    if(ret){goto SUB_OUT;}
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
    smtc_record_avg_flt(self);
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
SUB_OUT:
    if (from_probe){
        //don't raise an IRQ during probe, system may not be ready yet.
        if (!ret){
            ret = smtc_read_and_clear_irq(self, NULL);
        }
    }else{
        enable_irq(self->irq_id);
    }

    return ret;
}

//=================================================================================================
#if ENABLE_ESD_RECOVERY
#define _ESD_CHECK_INTERVAL     10*1000 //msec

#define _ESD_FAIL_CHECK_TIMES 3
#if _ESD_FAIL_CHECK_TIMES < 3
#error At least check 3 times
#endif

#if SUPPORT_POWER_ONOFF
static void ldo_power_off()
{
    SMTC_LOG_INF("Add vdd power off function of your system here");
}
static void ldo_power_on()
{
    SMTC_LOG_INF("Add vdd power on function of your system here");
}
#endif

static void smtc_esd_recover(Self self)
{
    int ret = 0, ph=0, idx=0, num_same_val;
    u32 reg_val, phen;
    static int useful_update_idx = 0;
#if SUPPORT_POWER_ONOFF    
    static int i2c_error_times=0;
#endif    
    static u32 ph_useful[NUM_PHASES][_ESD_FAIL_CHECK_TIMES];
    SMTC_LOG_DBG("Checking ESD failure");

    ret = smtc_i2c_read(self, REG_IRQ_MASK, &reg_val);
#if SUPPORT_POWER_ONOFF
    if (ret < 0)
    {
        if(++i2c_error_times > _ESD_FAIL_CHECK_TIMES)
        {
            //this error can only be recovered by power off/on
            SMTC_LOG_WRN("I2C error, power off/on SAR sensor");
            ldo_power_off();
            msleep(10);
            ldo_power_on();
            msleep(10);
            smtc_reset_and_init_chip(self, false);
            i2c_error_times = 0;
            return;
        }
    }
    else{
        i2c_error_times = 0;
    }
#else
    if (ret < 0){
        //i2c error may only be recovered by power off and on the chip
        SMTC_LOG_ERR("I2C error, it is not recoverable");
        return;
    }
#endif    

    //default value of 0x4004 is 0x60 and usually will be set to 0x70 in the dts
    if (reg_val == 0x60)
    {
        SMTC_LOG_WRN("Register 0x%X has been reset to default value=0x%X.", REG_IRQ_MASK, reg_val);
        smtc_reset_and_init_chip(self, false);
        return;
    }  

    phen = self->variables.enabled_phases;
    //update useful of each enabled phase
    for(ph=0; ph<NUM_PHASES; ph++)
    {
        if(phen & 1<<ph){
            smtc_i2c_read(self, REG_PH0_USEFUL + ph*4, &reg_val);
            ph_useful[ph][useful_update_idx] = reg_val;
        }
    }
    useful_update_idx = (useful_update_idx + 1) % _ESD_FAIL_CHECK_TIMES;

    //reset if any phase read the same value by CHECK_TIMES
    for(ph=0; ph<NUM_PHASES; ph++)
    {
        num_same_val = 0;
        
        if(phen & 1<<ph)
        {
            if (_ESD_FAIL_CHECK_TIMES == 3){
                SMTC_LOG_DBG("ph%d useful=[0x%X, 0x%X, 0x%X]", 
                    ph, ph_useful[ph][0], ph_useful[ph][1], ph_useful[ph][2]);
            }else{
                SMTC_LOG_DBG("ph%d useful=[0x%X, 0x%X, 0x%X, ...]", 
                    ph, ph_useful[ph][0], ph_useful[ph][1], ph_useful[ph][2]);
             }

            for(idx=1; idx<_ESD_FAIL_CHECK_TIMES; idx++)
            {
                if(ph_useful[ph][idx-1] == ph_useful[ph][idx])
                {
                    if (_ESD_FAIL_CHECK_TIMES == 3){
                        SMTC_LOG_INF("ph%d useful=[0x%X, 0x%X, 0x%X]", 
                            ph, ph_useful[ph][0], ph_useful[ph][1], ph_useful[ph][2]);
                    }else{
                        SMTC_LOG_INF("ph%d useful=[0x%X, 0x%X, 0x%X, ...]", 
                            ph, ph_useful[ph][0], ph_useful[ph][1], ph_useful[ph][2]);
                    }

                    if(++num_same_val >= _ESD_FAIL_CHECK_TIMES-1)
                    {
                        SMTC_LOG_WRN("Detected number of %d same useful values", _ESD_FAIL_CHECK_TIMES);
                        smtc_reset_and_init_chip(self, false);
                        return;
                    }
                }
            }
        }
    }
    
    SMTC_LOG_DBG("ESD check PASSED.");
}

static void smtc_esd_worker_func(struct work_struct *work)
{
    Self self = container_of(work, self_t, esd_worker.work);
    
    SMTC_LOG_DBG("Enter");
    smtc_esd_recover(self);
    schedule_delayed_work(&self->esd_worker, msecs_to_jiffies(_ESD_CHECK_INTERVAL));
    SMTC_LOG_DBG("Exit");
}
#endif //ENABLE_ESD_RECOVERY


//#################################################################################################
static void smtc_log_dbg_data(Self self)
{
    u16 off;
    int ph, state;
    u32 reg_val;
    s32 avg, diff, main_use, main_raw, dlt_var;
    PROX_STATUS ph_prox_state[NUM_PHASES];

    int ref_ph_a, ref_ph_b, ref_ph_c;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    char ref_a_name[] = "na";
    char ref_b_name[] = "na";
    char ref_c_name[] = "na";

    ref_ph_a = self->ref_phase_a;
    ref_ph_b = self->ref_phase_b;
    ref_ph_c = self->ref_phase_c;

    smtc_i2c_read(self, REG_DBG_SEL, &reg_val);
    ph = reg_val>>3 & 0x7;

    smtc_i2c_read(self, REG_PROX_STATUS, &reg_val);
    SMTC_LOG_DBG("REG_PROX_STATUS=0x%X", reg_val);
    smtc_get_ph_prox_state(reg_val, ph_prox_state);
    state = ph_prox_state[ph];

    if(ref_ph_a != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_a*4, &reg_val);
        ref_a_use = (s32)reg_val >> 10;
        sprintf(ref_a_name, "%d", ref_ph_a);
    }
    if(ref_ph_b != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_b*4, &reg_val);
        ref_b_use = (s32)reg_val >> 10;
        sprintf(ref_b_name, "%d", ref_ph_b);
    }
    if(ref_ph_c != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_c*4, &reg_val);
        ref_c_use = (s32)reg_val >> 10;
        sprintf(ref_c_name, "%d", ref_ph_c);
    }

    smtc_i2c_read(self, REG_RAW_DATA, &reg_val);
    main_raw = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_DLT_VAR, &reg_val);
    dlt_var = (s32)reg_val>>4;

    smtc_i2c_read(self, REG_PH0_USEFUL + ph*4, &reg_val);
    main_use = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_AVERAGE + ph*4, &reg_val);
    avg = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_DIFF + ph*4, &reg_val);
    diff = (s32)reg_val>>10;

    smtc_i2c_read(self, REG_PH0_OFFSET + ph*4*OFFSET_PH_REG_SHIFT, &reg_val);
    off = (u16)(reg_val & OFFSET_VAL_MASK);

    pr_info(
    "%s SMTC_DBG PH=%d USE=%d RAW=%d PH%s_USE=%d PH%s_USE=%d PH%s_USE=%d STATE=%d AVG=%d DIFF=%d OFF=%d DLT=%d SMTC_END\n",
    smtc_get_log_time(),
    ph, main_use, main_raw, ref_a_name, ref_a_use, ref_b_name, ref_b_use, ref_c_name, ref_c_use, 
    state, avg, diff, off, dlt_var);
}

//=================================================================================================
static void smtc_log_raw_data(Self self)
{
    int ph, state;
    u16 offset;
    s32 useful, average, diff;
    u32 reg_val, dbg_ph;
    u32 use_hex, avg_hex, dif_hex, dlt_hex;
    PROX_STATUS ph_prox_state[NUM_PHASES];

    int ref_ph_a, ref_ph_b, ref_ph_c;
    s32 ref_a_use=0, ref_b_use=0, ref_c_use=0;
    char ref_a_name[] = "na";
    char ref_b_name[] = "na";
    char ref_c_name[] = "na";

    ref_ph_a = self->ref_phase_a;
    ref_ph_b = self->ref_phase_b;
    ref_ph_c = self->ref_phase_c;

    smtc_i2c_read(self, REG_PROX_STATUS, &reg_val);
    SMTC_LOG_DBG("prox_state= 0x%X", reg_val);
    smtc_get_ph_prox_state(reg_val, ph_prox_state);

    smtc_i2c_read(self, REG_DBG_SEL, &dbg_ph);
    dbg_ph = (dbg_ph >> 3) & 0x7;
    smtc_i2c_read(self, REG_DLT_VAR, &dlt_hex);

    if(ref_ph_a != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_a*4, &reg_val);
        ref_a_use = (s32)reg_val >> 10;
        sprintf(ref_a_name, "%d", ref_ph_a);
    }
    if(ref_ph_b != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_b*4, &reg_val);
        ref_b_use = (s32)reg_val >> 10;
        sprintf(ref_b_name, "%d", ref_ph_b);
    }
    if(ref_ph_c != -1)
    {
        smtc_i2c_read(self, REG_PH0_USEFUL + ref_ph_c*4, &reg_val);
        ref_c_use = (s32)reg_val >> 10;
        sprintf(ref_c_name, "%d", ref_ph_c);
    }

    for(ph =0; ph<NUM_PHASES; ph++)
    {
        int shift = ph*4;
        smtc_i2c_read(self, REG_PH0_USEFUL  + shift,    &use_hex);
        useful = (s32)use_hex>>10;
        smtc_i2c_read(self, REG_PH0_AVERAGE + shift,    &avg_hex);
        average = (s32)avg_hex>>10;
        smtc_i2c_read(self, REG_PH0_DIFF    + shift,    &dif_hex);
        diff = (s32)dif_hex>>10;
        smtc_i2c_read(self, REG_PH0_OFFSET + shift*OFFSET_PH_REG_SHIFT,  &reg_val);
        offset = (u16)(reg_val & OFFSET_VAL_MASK);
        state = ph_prox_state[ph];

        pr_info(
        "%s SMTC_DAT PH= %d DIFF= %d USE= %d PH%s_USE= %d PH%s_USE= %d PH%s_USE= %d STATE= %d OFF= %d AVG= %d SMTC_END\n",
        smtc_get_log_time(),
        ph, diff, useful, ref_a_name, ref_a_use, ref_b_name, ref_b_use, ref_c_name, ref_c_use, state, offset, average);
        
        if (self->dbg_flag.log_hex_data){
            pr_info(
            "%s SMTC_HEX PH= %d USE= 0x%X AVG= 0x%X DIF= 0x%X PH%d_DLT= 0x%X SMTC_END\n",
            smtc_get_log_time(),
            ph, use_hex, avg_hex, dif_hex, dbg_ph, dlt_hex);
        }
    }
    
    if (self->dbg_flag.log_dbg_data){
        smtc_log_dbg_data(self);
    }
}
/*A06 code for SR-AL7160A-01-178 by huofudong at 20240417 start*/
#ifdef SX933X_USER_NODE
ssize_t smtc_receiver_turnon(const char *buf, size_t count)
{
    int receiver_status = 0;

    if (sscanf(buf, "%x", &receiver_status) != 1) {
        SMTC_LOG_ERR("(receiver_report) The number of data are wrong\n");
        return -EINVAL;
    }
    g_anfr_flag = false;
    SMTC_LOG_INF("(receiver_report) anfr = %d\n", g_anfr_flag);

    return count;
}
#endif
/*A06 code for SR-AL7160A-01-178 by huofudong at 20240417 end*/
//factory
#ifdef SX933X_FACTORY_NODE
static bool g_facEnable = false;
void factory_cali(void)
{
    SMTC_LOG_INF("Enter");
    if (g_self != NULL) {
        smtc_calibrate(g_self);
    }
}

ssize_t factory_set_enable(const char *buf, size_t count)
{
    u32 enable = 0;

    if (sscanf(buf, "%x", &enable) != 1) {
        SMTC_LOG_INF("The number of param are wrong\n");
        return -EINVAL;
    }

    if (enable == 1) {
        g_facEnable = true;
        SMTC_LOG_INF("enable success.");
    } else if (enable == 0) {
        g_facEnable = false;
        SMTC_LOG_INF("disable success.");
    } else {
        SMTC_LOG_ERR("Invalid param. Usage: ehco 0/1 > enable");
    }

    return count;
}

ssize_t factory_get_enable(char *buf)
{
    return sprintf(buf, "%d\n", g_facEnable);
}
#endif

/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
void smtc_user_cali(void)
{
    SMTC_LOG_INF("smtc_user_cali Enter");
    if (g_self != NULL) {
        smtc_calibrate(g_self);
    }
}

static void smtc_enable_report(Self self,char band_name[25])
{
    int ph = 0;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);
    SMTC_LOG_INF("band_name=%s",band_name);
    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

         if (strcmp(phase->name, band_name) != 0) {
            continue;
        }
        input = phase->input;

        if (prox_state & phase->prox1_mask) {
            SMTC_LOG_INF("%s reports PROX1 , onoff:%d", phase->name, g_onoff_enable);
            phase->state = PROX1;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_key(input, phase->keycode_close, 1);
                    input_report_key(input, phase->keycode_close, 0);
                    input_sync(input);
                }
            }
        } else {
            SMTC_LOG_INF("%s reports release, onoff:%d", phase->name, g_onoff_enable);
            phase->state = RELEASED;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_key(input, phase->keycode_far, 1);
                    input_report_key(input, phase->keycode_far, 0);
                    input_sync(input);
                }
            }
        }
    }
}

static void smtc_disable_report(Self self,char band_name[25])
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        if (strcmp(phase->name, band_name) != 0) {
            continue;
        }
        input = phase->input;

        phase->state = RELEASED;
        if (g_onoff_enable == false) {
            if (((self->channel_status) & (phase->phase_status)) == 0) {
                input_report_key(input, phase->keycode_far, 1);
                input_report_key(input, phase->keycode_far, 0);
                input_sync(input);
            }
        }
        SMTC_LOG_INF("%s reports release", phase->name);
    }
}
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
ssize_t smtc_set_enable(const char *buf, size_t count)
{
    u32 phase=0, enable=0;
    char band_name[25]="";

    if (g_self != NULL) {
        if (sscanf(buf, "%d,%d", &phase, &enable) == 2)
        {
            SMTC_LOG_INF("%s phase=%d", enable ? "Enable" : "Disable", phase);
            if (phase >= NUM_PHASES){
                SMTC_LOG_ERR("phase=%d is not in the range [0, %d]", phase, NUM_PHASES-1);
                return -EINVAL;
            }

            if (enable == 1) {
                if (0 == phase) {
                    strcpy(band_name, "grip_sensor_sub");
                    if (!(g_self->channel_status & g_ch1_mask)) {
                        g_self->channel_status |= g_ch1_mask;
                    }
                } else if (1 == phase) {
                    strcpy(band_name, "grip_sensor");
                    if (!(g_self->channel_status & g_ch2_mask)) {
                        g_self->channel_status |= g_ch2_mask;
                    }
                } else if (2 == phase) {
                    strcpy(band_name, "grip_sensor_wifi");
                    if (!(g_self->channel_status & g_ch3_mask)) {
                        g_self->channel_status |= g_ch3_mask;
                    }
                } else {
                    SMTC_LOG_ERR("phase number error");
                }

                SMTC_LOG_INF("self->channel_status = 0x%x", g_self->channel_status);

                if (g_self->irq_wake_status != true) {
                    enable_irq_wake(g_self->irq_id);
                    g_self->irq_wake_status = true;
                    SMTC_LOG_INF("enable irq wake success.");
                }
                smtc_enable_report(g_self,band_name);

            } else if (enable == 0) {
                if (0 == phase) {
                    strcpy(band_name, "grip_sensor_sub");
                    if (g_self->channel_status & g_ch1_mask) {
                        g_self->channel_status &= ~g_ch1_mask;
                    }
                } else if (1 == phase) {
                    strcpy(band_name, "grip_sensor");
                    if (g_self->channel_status & g_ch2_mask) {
                        g_self->channel_status &= ~g_ch2_mask;
                    }
                } else if (2 == phase) {
                    strcpy(band_name, "grip_sensor_wifi");
                    if (g_self->channel_status & g_ch3_mask) {
                        g_self->channel_status &= ~g_ch3_mask;
                    }
                } else {
                    SMTC_LOG_ERR("phase  number error");
                }

                SMTC_LOG_INF("self->channel_status = 0x%x", g_self->channel_status);

                if ((g_self->irq_wake_status != false) && 0 == g_self->channel_status) {
                    disable_irq_wake(g_self->irq_id);
                    g_self->irq_wake_status = false;
                    SMTC_LOG_INF("disable irq wake success.");
                }
                smtc_disable_report(g_self,band_name);

            } else {
                SMTC_LOG_ERR("echo 2,1 > enable #enable phase 2");
            }
        }
    } else {
        SMTC_LOG_ERR("g_self if NULL!");
    }
    return count;
}
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
ssize_t smtc_get_enable(char *buf)
{
    int bytes=0;
    bytes += sprintf(buf+bytes, "Current enabled phases: reg_val=0x%X",  g_self->channel_status);
    return bytes;
}
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
static void smtc_onoff_enablereport(Self self)
{
    int ph = 0;
    u32 prox_state = 0;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_INF("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        if (prox_state & phase->prox1_mask) {
            SMTC_LOG_INF("%s reports PROX1", phase->name);
            phase->state = PROX1;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                    input_report_rel(input, REL_MISC, 1);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
                }
            }
        } else {
            SMTC_LOG_INF("%s reports release", phase->name);
            phase->state = RELEASED;
            if (g_onoff_enable == false) {
                if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
                }
            }
        }
    }
}

static void smtc_onoff_disablereport(Self self)
{
    int ph = 0;
    phase_p phase;
    struct input_dev *input;

    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        phase->state = RELEASED;
        if (g_onoff_enable == false) {
            if ((self->channel_status) & (phase->phase_status)) {
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
            }
        }
    SMTC_LOG_INF("%s reports release", phase->name);
    }
}
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
ssize_t smtc_set_onoff(const char *buf, size_t count)
{
    if (g_self != NULL) {
        if (!strncmp(buf, "0", 1)) {
            SMTC_LOG_INF("self->channel_status = 0x%x", g_self->channel_status);
            smtc_onoff_disablereport(g_self);
            g_onoff_enable = true;
            SMTC_LOG_INF("onoff Function set of off");
        } else if (!strncmp(buf, "1", 1)) {
            g_onoff_enable = false;
            SMTC_LOG_INF("self->channel_status = 0x%x", g_self->channel_status);
            smtc_onoff_enablereport(g_self);
            SMTC_LOG_INF("onoff Function set of on");
        } else {
            SMTC_LOG_ERR("invalid param!");
        }
    } else {
        SMTC_LOG_ERR("g_self if NULL!");
    }
    return count;
}

ssize_t smtc_get_onoff(char *buf)
{
    return snprintf(buf, 8, "%d\n", g_onoff_enable);
}

static ssize_t smtc_set_dumpinfo(const char *buf, size_t count)
{
    u8 value = 0;

    if (sscanf(buf, "%d", &value) != 1) {
        SMTC_LOG_ERR("Invalid command format. Usage: ehco 1 > dumpinfo");
        return -EINVAL;
    }

    g_channel = value;

    return count;
}

static ssize_t smtc_get_dumpinfo(char *buf)
{
    int shift = 0;
    u32 reg_val = 0;
    phase_p phase = NULL;

    if (g_self != NULL) {
        phase = &smtc_phase_table[g_channel];
        smtc_i2c_read(g_self, REG_PROX_STATUS, &reg_val);
        if (reg_val & phase->prox1_mask) {
            g_dumpinfo.ph_state = PROX1;
        } else {
            g_dumpinfo.ph_state = RELEASED;
        }

        shift = g_channel * g_bitoffset;
        smtc_i2c_read(g_self, REG_PH0_USEFUL + shift, &reg_val);
        g_dumpinfo.raw_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_AVERAGE + shift, &reg_val);
        g_dumpinfo.baseline_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_DIFF + shift, &reg_val);
        g_dumpinfo.diff_data = (s32)reg_val >> g_bitshift;
        smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
        g_dumpinfo.total_cap = (u16)(reg_val & OFFSET_VAL_MASK);

        g_dumpinfo.total_cap = ((((g_dumpinfo.total_cap & 0x00004000) >> 14) * 10608) +
            (((g_dumpinfo.total_cap & 0x00003F80) >> 7) * 221) +
            ((g_dumpinfo.total_cap & 0x0000007F) * 5)) / 100;

        if (g_channel == CH0 || g_channel == CH1) {
            shift = CH3 * g_bitoffset;
            smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
            g_dumpinfo.ref_cap = (u16)(reg_val & OFFSET_VAL_MASK);
        } else if(g_channel == CH2) {
            shift = CH4 * g_bitoffset;
            smtc_i2c_read(g_self, REG_PH0_OFFSET + shift * OFFSET_PH_REG_SHIFT, &reg_val);
            g_dumpinfo.ref_cap = (u16)(reg_val & OFFSET_VAL_MASK);
        } else {
            g_dumpinfo.ref_cap = 0;
            SMTC_LOG_INF("g_channel=%d", g_channel);
        }

        if (g_channel == CH0 || g_channel == CH1 || g_channel == CH2) {
            g_dumpinfo.ref_cap = ((((g_dumpinfo.ref_cap & 0x00004000) >> 14) * 10608) +
                (((g_dumpinfo.ref_cap & 0x00003F80) >> 7) * 221) +
                ((g_dumpinfo.ref_cap & 0x0000007F) * 5)) / 100;
        }

        return snprintf(buf, PAGE_SIZE, "%ld,%d,%d,%d,%d,%d,%d\n",
            g_self->fw_num, g_dumpinfo.ph_state, g_dumpinfo.total_cap, g_dumpinfo.ref_cap,
            g_dumpinfo.raw_data, g_dumpinfo.baseline_data, g_dumpinfo.diff_data);

        SMTC_LOG_INF("ph_state=%d", g_dumpinfo.ph_state);
    } else {
        SMTC_LOG_ERR("g_self is NULL!");
    }

    return 0;
}
/*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/
/*a06 code for SR-AL7160A-01-175 | SR-AL7160A-01-177 by huofudong at 20240417 start*/
#ifdef SX933X_FACTORY_NODE
static void smtc_process_touch_status_factory(Self self)
{
    int ph;
    u32 prox_state = 0;
    bool need_sync = false;
    phase_p phase;
    struct input_dev *input;

    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_DBG("prox_state= 0x%X", prox_state);

    for (ph = 0; ph < NUM_PHASES; ph++) {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        need_sync = false;
        if (prox_state & phase->prox1_mask) {
            if (phase->state == PROX1) {
                SMTC_LOG_DBG("%s is PROX1 already", phase->name);
            } else {
                SMTC_LOG_DBG("%s reports PROX1", phase->name);
                phase->state = PROX1;
                if (g_onoff_enable == false) {
                    input_report_key(input, phase->keycode_close, 1);
                    input_report_key(input, phase->keycode_close, 0);
                    need_sync = true;
                }
            }
        } else {
            if (phase->state == RELEASED) {
                SMTC_LOG_DBG("%s is RELEASED already", phase->name);
            } else {
                SMTC_LOG_DBG("%s reports RELEASED", phase->name);
                phase->state = RELEASED;
                if (g_onoff_enable == false) {
                    input_report_key(input, phase->keycode_far, 1);
                    input_report_key(input, phase->keycode_far, 0);
                    need_sync = true;
                }
            }
        }

        if (need_sync) {
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
            smtc_enable_avg_flt(self, ph, phase->state == RELEASED);
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
            input_sync(input);
        }
    }
}
#else
static void smtc_process_touch_status_normal(Self self)
{
    int ph;
    u32 prox_state = 0;
    bool need_sync = false, status_changed=false;
    phase_p phase;
    struct input_dev *input;
    char msg_updated_status[128];
    char msg_prox_status[128];
    int msg_len = 0;
    smtc_i2c_read(self, REG_PROX_STATUS, &prox_state);
    SMTC_LOG_DBG("prox_state= 0x%X", prox_state);
    if (self->dbg_flag.log_raw_data){
        smtc_log_raw_data(self);
    }

    for (ph = 0; ph < NUM_PHASES; ph++)
    {
        phase = &smtc_phase_table[ph];
        if (phase->usage != MAIN)
            continue;

        input = phase->input;
        need_sync = false;

        if (prox_state & phase->prox1_mask)
        {
            if (phase->state == PROX1){
                SMTC_LOG_DBG("%s is PROX1 already", phase->name);
            }
            else{
                SMTC_LOG_DBG("%s reports PROX1", phase->name);
                msg_len += sprintf(msg_updated_status+msg_len, " PH%d=1", ph);
                phase->state = PROX1;
                /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
                if (g_onoff_enable == false) {
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 1);
                        input_report_rel(input, REL_X, 2);
                    }
                }
                /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/
                need_sync = true;
            }
        }
        else
        {
            if (phase->state == RELEASED){
                SMTC_LOG_DBG("%s is RELEASED already", phase->name);
            }
            else
            {
                SMTC_LOG_DBG("%s reports RELEASED", phase->name);
                msg_len += sprintf(msg_updated_status+msg_len, " PH%d=0", ph);
                phase->state = RELEASED;
                /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
                if (g_onoff_enable == false) {
                    if ((self->channel_status) & (phase->phase_status)) {
                        input_report_rel(input, REL_MISC, 2);
                        input_report_rel(input, REL_X, 2);
                    }
                }
                /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/
                need_sync = true;
            }
        }

        if (need_sync){
/*a06 code for AL7160A-1118 by zhawei at 20240514 start*/
#if DYNAMIC_AVG_FLT
            smtc_enable_avg_flt(self, ph, phase->state == RELEASED);
#endif    //DYNAMIC_AVG_FLT
/*a06 code for AL7160A-1118 by zhawei at 20240514 end*/
            input_sync(input);
            status_changed = true;
        }
    }

    if (status_changed)
    {
        msg_len = 0;
        for (ph = 0; ph < NUM_PHASES; ph++)
        {
            phase = &smtc_phase_table[ph];
            if (phase->usage == MAIN){                    
                msg_len += sprintf(msg_prox_status+msg_len, " PH%d=%d", ph, phase->state);
            }
        }

        SMTC_LOG_INF("Prox status= 0x%08X %s Updated:%s", prox_state, msg_prox_status, msg_updated_status);
        
    }else{
        SMTC_LOG_INF("No proximity state is updated");
    }
}
#endif
static void smtc_process_touch_status(Self self)
{
    #ifdef SX933X_USER_NODE
    if (g_anfr_flag) {
        smtc_anfr_irq(self);
        smtc_anfr_forcereport(self);
    } else {
        smtc_process_touch_status_normal(self);
    }
    #else
        smtc_process_touch_status_factory(self);
    #endif
}
/*a06 code for SR-AL7160A-01-175 | SR-AL7160A-01-177 by huofudong at 20240417 end*/
//#################################################################################################
static void smtc_update_status(Self self)
{
    int ret=0, irq_bit=0, irq_src=0;

    ret = smtc_read_and_clear_irq(self, &irq_src);
    if (ret){
        SMTC_LOG_ERR("Failed to read irq source. ret=%d.", ret);
        goto SUB_OUT;
    }
    SMTC_LOG_INF("irq_src= 0x%X", irq_src);
    /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 start*/
    #ifdef SX933X_USER_NODE
    if (g_cali_count < g_cali_maxnum && ((irq_src >> 4) & 0x01)) {
        g_cali_count++;
        SMTC_LOG_INF("g_cali_count:%d", g_cali_count);
    }
    #endif 
    /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 end*/
    for (irq_bit=0; irq_bit<NUM_IRQ_BITS; irq_bit++)
    {
        if (irq_src >> irq_bit & 0x1)
        {
            if(self->irq_handler[irq_bit])
            {
                //call to smtc_process_touch_status() or smtc_log_raw_data()
                self->irq_handler[irq_bit](self);
            }else{
                SMTC_LOG_ERR("No handler to IRQ bit= %d", irq_bit);
            }
        }
    }

    if (irq_src == 0)
    {
        SMTC_LOG_INF("Force to update touch status");
        smtc_process_touch_status(self);
    }

SUB_OUT:
#if ENABLE_STAY_AWAKE && !WAKE_TMOUT
    SMTC_LOG_DBG("Release wake lock");
    __pm_relax(self->wake_lock);
#endif
    SMTC_LOG_DBG("Exit");
    return;
}

#if !USE_THREAD_IRQ
static void smtc_worker_func(struct work_struct *work)
{
    Self self = container_of(work, self_t, worker.work);
    SMTC_LOG_DBG("Enter");
    smtc_update_status(self);
    SMTC_LOG_DBG("Exit");
}
#endif

//=================================================================================================
//call flow: smtc_irq_handler--> smtc_worker_func--> smtc_process_touch_status
static irqreturn_t smtc_irq_isr(int irq, void *pvoid)
{
    Self self = (Self)pvoid;

#if USE_THREAD_IRQ
    SMTC_LOG_DBG("IRQ= %d is received", irq);
#else
    SMTC_LOG_INF("IRQ= %d is received", irq);
#endif
    /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 start*/
   #ifdef SX933X_USER_NODE
    if ((g_irq_count >= g_irq_maxnum) || (g_cali_count >= g_cali_maxnum)) {
        g_anfr_flag = false;
    }
    SMTC_LOG_INF("cali_count:%d, anfr_flag:%d", g_cali_count, g_anfr_flag);
    #endif
    /*a06 code for SR-AL7160A-01-177 by huofudong at 20240417 end*/
    if (!smtc_is_irq_low(self))
    {
        SMTC_LOG_ERR("GPIO=%d must be low when an IRQ is received.", self->gpio);
        smtc_read_and_clear_irq(self, NULL);
        return IRQ_HANDLED;
    }

#if ENABLE_STAY_AWAKE
#if WAKE_TMOUT
    SMTC_LOG_DBG("Stay awake for %d seconds", WAKE_TMOUT);
    __pm_wakeup_event(self->wake_lock, WAKE_TMOUT);
#else
    SMTC_LOG_DBG("Stay awake");
    __pm_stay_awake(self->wake_lock);
#endif    
#endif

#if USE_THREAD_IRQ
    SMTC_LOG_DBG("Update status with thread IRQ");
    smtc_update_status(self);
#else
    SMTC_LOG_DBG("Update status with worker");
    cancel_delayed_work(&self->worker);
    schedule_delayed_work(&self->worker, 0);
#endif

    return IRQ_HANDLED;
}

//#################################################################################################
static int smtc_probe(struct i2c_client *client, const struct i2c_device_id *id)
{    
    int ret = 0, i;
    Self self = NULL;
    struct input_dev *input = NULL;
    struct i2c_adapter *adapter = NULL;
    SMTC_LOG_INF("Start to initialize Semtech SAR sensor %s driver.", SMTC_CHIP_NAME);

    adapter = to_i2c_adapter(client->dev.parent);
    ret = i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA);
    if (!ret)
    {
        SMTC_LOG_ERR("Failed to check i2c functionality.");
        return ret;
    }

    //kernel will automatically free the memory allocated by the devm_kzalloc 
    //when probe is failed and driver is removed. 
    self = devm_kzalloc(&client->dev, sizeof(self_t), GFP_KERNEL);
    if (!self)
    {
        SMTC_LOG_ERR("Failed to create self. memory size=%d bytes.", sizeof(self_t));
        return -ENOMEM;
    }

    smtc_init_variables(self);
    client->dev.platform_data = self;
    i2c_set_clientdata(client, self);
    self->client = client;

    ret = smtc_parse_dts(self);
    if (ret) {return ret;}

    ret = smtc_check_hardware(self);
    if (ret) {return ret;}

    ret = smtc_init_irq_gpio(self);
    if (ret) {return ret;}

    ret = smtc_reset_and_init_chip(self, true);
    if (ret) {goto FREE_GPIO;}

    //refer to register 0x4000
    self->irq_handler[0] = 0; /* UNUSED */
    self->irq_handler[1] = 0; /* UNUSED */
    self->irq_handler[2] = 0; /* UNUSED */
    self->irq_handler[3] = smtc_log_raw_data;   //CONVEDONEIRQ
    self->irq_handler[4] = smtc_process_touch_status; //COMPDONEIRQ
    self->irq_handler[5] = smtc_process_touch_status; //FARANYIRQ
    self->irq_handler[6] = smtc_process_touch_status; //CLOSEANYIRQ
    self->irq_handler[7] = 0; /* RESET_STAT */
#ifdef SMTC_SX933X
    self->irq_handler[2] = smtc_process_touch_status; //PROX2(table) and PROX3(body) irq
#endif

    ret = sysfs_create_group(&client->dev.kobj, &smtc_sysfs_nodes);
    if (ret)
    {
        SMTC_LOG_ERR("Failed to create sysfs node.");
        goto FREE_GPIO;
    }

    //.............................................................................................
    //Create input nodes for each main phase
    self->phases = smtc_phase_table;
    for (i=0; i<NUM_PHASES; i++)
    {
        phase_p phase = &smtc_phase_table[i];
        SMTC_LOG_DBG("name=%s input=0x%p enabled=%d usage=%d state=%d",
            phase->name, phase->input, phase->enabled, phase->usage, phase->usage);

        //main phases were specified in the dts
        if ((self->main_phases & 1<<i) == 0)
        {
            if (self->ref_phases & 1<<i){
                phase->usage = REF;
            }
            continue;
        }

        input = input_allocate_device();
        if (!input)
        {
            SMTC_LOG_ERR("Failed to create input device %s.", phase->name);
            ret = -ENOMEM;
            goto FREE_INPUTS;
        }
        /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
        #ifdef SX933X_FACTORY_NODE
        /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 start */
        input_set_capability(input, EV_KEY, phase->keycode_far);
        input_set_capability(input, EV_KEY, phase->keycode_close);
        /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 end */
        #else
        input_set_capability(input, EV_REL, REL_MISC);
        input_set_capability(input, EV_REL, REL_X);
        input_set_capability(input, EV_REL, REL_MAX);
        #endif
        /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
        input->name = phase->name;
        input->id.bustype = BUS_I2C;
        ret = input_register_device(input);
        if (ret)
        {
            SMTC_LOG_ERR("Failed to register input device %s. ret= %d", phase->name, ret);
            goto FREE_INPUTS;
        }

        phase->input = input;
        phase->enabled = false;
        phase->usage = MAIN;
        phase->state = RELEASED;

        /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
        #ifdef SX933X_USER_NODE
        input_set_drvdata(phase->input, self);
        self->channel_status = 0;

        if (!strcmp(phase->name, "grip_sensor")) {
            ret = sensors_register(&self->factory_device, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register main sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input main group");
                return -ENOMEM;
            }
        }

        if (!strcmp(phase->name, "grip_sensor_sub")) {
            ret = sensors_register(&self->factory_device_sub, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register sub sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input sub group");
                return -ENOMEM;
            }
        }

        if (!strcmp(phase->name, "grip_sensor_wifi")) {
            ret = sensors_register(&self->factory_device_wifi, self, sensor_attrs, phase->name);
            if (ret) {
                SMTC_LOG_ERR("cound not register wifi sensor(%d).", ret);
                return -ENOMEM;
            }

            ret = sysfs_create_group(&phase->input->dev.kobj, &ss_smtc_attributes_group);
            if (ret) {
                SMTC_LOG_ERR("cound not create input wifi group");
                return -ENOMEM;
            }
        }
        #endif
        /*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
    }

    //.............................................................................................
 #if USE_THREAD_IRQ
    SMTC_LOG_INF("Use thread IRQ");
    ret = request_threaded_irq(self->irq_id, NULL, smtc_irq_isr,
            IRQF_TRIGGER_LOW | IRQF_ONESHOT,
            client->dev.driver->name, self);

    if (ret){
        SMTC_LOG_ERR("Failed to request irq= %d.", self->irq_id);
        goto FREE_INPUTS;
    }
 #else
    SMTC_LOG_INF("Use hard IRQ");

    spin_lock_init(&self->lock);
    mutex_init(&self->reg_wr_lock);
    INIT_DELAYED_WORK(&self->worker, smtc_worker_func);

    ret = request_irq(self->irq_id, smtc_irq_isr, IRQF_TRIGGER_FALLING,
                      client->dev.driver->name, self);
    if (ret)
    {
        SMTC_LOG_ERR("Failed to request irq= %d.", self->irq_id);
        goto FREE_INPUTS;
    }
 #endif   
    /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 start */
    g_self = self;
    #ifdef SX933X_FACTORY_NODE
    sar_drv.sar_name_drv = "sx933x";
    sar_drv.get_cali = factory_cali;
    sar_drv.set_enable = factory_set_enable;
    sar_drv.get_enable = factory_get_enable;
    SMTC_LOG_INF("factory node create done.");
    #endif
    /* a06 code for SR-AL7160A-01-172  by suyurui at 2024/03/24 end */
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 start*/
    /*A06 code for SR-AL7160A-01-183 | SR-AL7160A-01-178 by huofudong at 20240417 start*/
    self->channel_status = 0;
    self->irq_wake_status = false;
    g_usernode.sar_name_drv = "sx933x";
    g_usernode.set_onoff = smtc_set_onoff;
    g_usernode.get_onoff = smtc_get_onoff;
    g_usernode.set_dumpinfo = smtc_set_dumpinfo;
    g_usernode.get_dumpinfo = smtc_get_dumpinfo;
    g_usernode.set_enable = smtc_set_enable;
    g_usernode.get_enable = smtc_get_enable;
    g_usernode.get_cali = smtc_user_cali;
    #ifdef SX933X_USER_NODE
    g_usernode.set_receiver_turnon = smtc_receiver_turnon;
    #endif
    SMTC_LOG_INF("user node create done.");
    /*A06 code for SR-AL7160A-01-183 | SR-AL7160A-01-178 by huofudong at 20240417 end*/
    /*a06 code for SR-AL7160A-01-174 by suyurui at 2024/04/02 end*/

    SMTC_LOG_INF("registered irq= %d.", self->irq_id);

#if ENABLE_STAY_AWAKE
    SMTC_LOG_INF("Enable wake lock");
    self->wake_lock = wakeup_source_register(SMTC_DRIVER_NAME);
    if (!self->wake_lock){
        SMTC_LOG_ERR("Failed to create wake lock.");
        ret = -ENOMEM;
        goto FREE_IRQ;
    }
#endif

#if ENABLE_ESD_RECOVERY
    INIT_DELAYED_WORK(&self->esd_worker, smtc_esd_worker_func);
    //60 seconds, make sure system is fully ready.
    schedule_delayed_work(&self->esd_worker, msecs_to_jiffies(60*1000));
#endif

    SMTC_LOG_INF("Done.");
    return 0;

    //.............................................................................................
#if ENABLE_STAY_AWAKE
FREE_IRQ:
    free_irq(self->irq_id, self);
#endif

FREE_INPUTS:
    for (i=0; i<NUM_PHASES; i++)
    {
        if (self->phases[i].input){
            input_unregister_device(self->phases[i].input);
        }
    }

    sysfs_remove_group(&client->dev.kobj, &smtc_sysfs_nodes);

FREE_GPIO:
    gpio_free(self->gpio);
    SMTC_LOG_INF("Failed to probe ret=%d.", ret);
    return ret;
}

//#################################################################################################
static int smtc_remove(struct i2c_client *client)
{
    int ret, i;
    Self self = i2c_get_clientdata(client);
    SMTC_LOG_INF("Remove driver.");

    //enter pause mode to save power
    ret = smtc_send_cmd(self, CMD_PAUSE);
    if (ret){
        SMTC_LOG_ERR("Failed to enter pause mode. ret= %d", ret);
    }
    smtc_read_and_clear_irq(self, NULL);
    free_irq(self->irq_id, self);

#if ENABLE_ESD_RECOVERY
    cancel_delayed_work_sync(&self->esd_worker);
#endif //ENABLE_ESD_RECOVERY

#if !USE_THREAD_IRQ    
    cancel_delayed_work_sync(&self->worker);
#endif //USE_THREAD_IRQ

    for (i=0; i<NUM_PHASES; i++)
    {
        if (self->phases[i].input){
            input_unregister_device(self->phases[i].input);
        }
    }
    sysfs_remove_group(&client->dev.kobj, &smtc_sysfs_nodes);
    gpio_free(self->gpio);

#if ENABLE_STAY_AWAKE    
    wakeup_source_unregister(self->wake_lock);
#endif //ENABLE_STAY_AWAKE

    return 0;
}

//===================================================`==============================================
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 start*/
static int smtc_suspend(struct device *dev)
{    
//.................................................................................................
#if DISABLE_DURING_SUSPEND
    Self self = dev_get_drvdata(dev);
    SMTC_LOG_INF("Disable SAR sensor");

    //turn off all phases
    SMTC_LOG_DBG("enabled_phases=0x%X", self->variables.enabled_phases);
    smtc_i2c_write(self, REG_PHEN, self->variables.enabled_phases & ~0xFF);
    disable_irq(self->irq_id);
    //release IRQ pin to avoid leakage of current
    smtc_read_and_clear_irq(self, NULL);

//.................................................................................................
#elif PAUSE_DURING_SUSPEND
    Self self = dev_get_drvdata(dev);
    if (g_Enabled == false) {
        SMTC_LOG_INF("Suspend SAR sensor");
        smtc_send_cmd(self, 0xD);
        //release IRQ pin to avoid leakage of currents
        smtc_read_and_clear_irq(self, NULL);
    }else {
        SMTC_LOG_INF("NO Suspend SAR sensor.");
    }

//.................................................................................................
#elif WAKE_BY_IRQ
    //wake up function is enabled in the probe
    SMTC_LOG_INF("wakeup by IRQ is enabled");

//.................................................................................................
#else
    Self self = dev_get_drvdata(dev);
    SMTC_LOG_INF("SAR sensor will be still running during suspend but with wakup disabled");
    disable_irq(self->irq_id);
    //don't raise IRQ during suspend
    smtc_i2c_write(self, REG_IRQ_MASK, 0x0);
    //release IRQ pin to avoid leakage of current
    smtc_read_and_clear_irq(self, NULL);
#endif

    return 0;
}

static int smtc_resume(struct device *dev)
{
//.................................................................................................
#if DISABLE_DURING_SUSPEND
    self_t *self = dev_get_drvdata(dev);
    SMTC_LOG_INF("Enable SAR sensor");
    enable_irq(self->irq_id);
    
    if (mutex_trylock(&self->phen_lock))
    {
        SMTC_LOG_DBG("enabled_phases=0x%X", self->variables.enabled_phases);
        smtc_i2c_write(self, REG_PHEN, self->variables.enabled_phases);
        mutex_unlock(&self->phen_lock);
        smtc_send_cmd(self, CMD_COMPENSATE);
    }else{
        SMTC_LOG_INF("Up layer is enabling phases");
    }

//.................................................................................................
#elif PAUSE_DURING_SUSPEND
    Self self = dev_get_drvdata(dev);
    if (g_Enabled == false) {
        SMTC_LOG_INF("Resume SAR sensor");
        smtc_send_cmd(self, CMD_RESUME);
    } else {
        SMTC_LOG_INF("NO Resume SAR sensor");
    }

//.................................................................................................
#elif WAKE_BY_IRQ
    SMTC_LOG_INF("Enter");

//.................................................................................................
#else //chip is enabled but with IRQ is disabled
    Self self = dev_get_drvdata(dev);
    SMTC_LOG_INF("Enable IRQ");
    enable_irq(self->irq_id);
    smtc_i2c_write(self, REG_IRQ_MASK, self->variables.irq_mask);

#if ENABLE_STAY_AWAKE
    //__pm_wakeup_event(self->wake_lock, WAKE_TMOUT);
    __pm_stay_awake(self->wake_lock);
#endif

//in case touch status updated during system suspend
#if USE_THREAD_IRQ    
    SMTC_LOG_DBG("Force to update status");
    smtc_update_status(self);
#else
    SMTC_LOG_DBG("Update status with worker");
    schedule_delayed_work(&self->worker, 0);
#endif //USE_THREAD_IRQ

#if ENABLE_STAY_AWAKE
#if WAKE_TMOUT
    SMTC_LOG_INF("Relax stay awake");
    __pm_relax(self->wake_lock);
#else
    SMTC_LOG_INF("smtc_update_status calls __pm_relax when there is no WAKE_TMOUT");
#endif
#endif //ENABLE_STAY_AWAKE

#endif

    return 0;
}
/*a06 code for SR-AL7160A-01-175  by huofudong at 20240416 end*/
//#################################################################################################
static struct i2c_device_id smtc_idtable[] =
{
    {SMTC_DRIVER_NAME, 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, smtc_idtable);

static struct of_device_id smtc_match_table[] =
{
    { .compatible = SMTC_COMPATIBLE_NAME,},
    { },
};

static const struct dev_pm_ops smtc_pm_ops =
{
    .suspend = smtc_suspend,
    .resume = smtc_resume,
};
static struct i2c_driver smtc_driver =
{
    .driver = {
        .owner = THIS_MODULE,
        .name = SMTC_DRIVER_NAME,
        .of_match_table = smtc_match_table,
        .pm = &smtc_pm_ops,
    },

    .id_table   = smtc_idtable,
    .probe      = smtc_probe,
    .remove     = smtc_remove,
};

static int __init smtc_i2c_init(void)
{
    SMTC_LOG_INF("Enter");
    return i2c_add_driver(&smtc_driver);
}
static void __exit smtc_i2c_exit(void)
{
    i2c_del_driver(&smtc_driver);
}

module_init(smtc_i2c_init);
module_exit(smtc_i2c_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX93_3x7x Capacitive SAR and Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("3.5");

