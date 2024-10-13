/**************************************************************
*                                                             *
* Driver for NanJingTianYiHeXin HX9031AS & HX9023S Cap Sensor *
*                                                             *
**************************************************************/

#define HX9031AS_DRIVER_VER "b4b7fc" //Change-Id

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>

#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include "hx9031as.h"
/*A06 code for SR-AL7160A-01-185 | SR-AL7160A-01-183 by huofudong at 20240409 start*/
#ifdef HX9031AS_FACTORY_NODE
#include "../sensor_fac_node.h"
#endif
#include "../sensor_user_node.h"
/*A06 code for SR-AL7160A-01-185 | SR-AL7160A-01-183 by  at huofudong 20240409 end*/
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
/*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
#include <linux/sensor/sensors_core.h>
#ifdef HX9031AS_USER_NODE
#define VENDOR_NAME      "TYHX"
#define MODEL_NAME       "HX9031"
#define SELF_CALI_BL_0x04 -14336
const int g_hx_irq_maxnum = 28;
/*A06 code for AL7160A-2616 by jiashixian at 20240604 start*/
const int g_hx_cali_maxnum = 2;
/*A06 code for AL7160A-2616 by jiashixian at 20240604 end*/
static bool g_anfr_flag = true;
static int g_cali_count = 0;
static int g_irq_count = 0;
static int gs_last_state = 0;
#endif
/*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 end*/
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 end*/
static struct i2c_client *hx9031as_i2c_client = NULL;
static struct hx9031as_platform_data hx9031as_pdata;
static uint8_t ch_enable_status = 0x00;
static uint8_t hx9031as_polling_enable = 0;
static int hx9031as_polling_period_ms = 0;
static volatile uint8_t hx9031as_irq_from_suspend_flag = 0;
static volatile uint8_t hx9031as_irq_en_flag = 1;

static int32_t data_raw[HX9031AS_CH_NUM] = {0};
static int32_t data_diff[HX9031AS_CH_NUM] = {0};
static int32_t data_lp[HX9031AS_CH_NUM] = {0};
static int32_t data_bl[HX9031AS_CH_NUM] = {0};
static uint16_t data_offset_dac[HX9031AS_CH_NUM] = {0};
static uint8_t hx9031as_data_accuracy = 16;
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
static struct hx9031as_platform_data* g_self;
uint8_t g_channel1 = 0;
const char g_ch1_mask1 = 0x01;
const char g_ch2_mask1 = 0x02;
const char g_ch3_mask1 = 0x04;
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
const char g_all_mask1 = 0x07;
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 end*/
static bool g_onoff_enable = false;
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
//hx9031as default threshold setting value
/*A06 code for AL7160A-70 jiashixian at 20240416 start*/
/*A06 code for AL7160A-417 | AL7160A-2783 jiashixian at 20240607 start*/
static struct hx9031as_near_far_threshold hx9031as_ch_thres[HX9031AS_CH_NUM] = {
    {.thr_near = 32000, .thr_far = 32000}, //ch0
    {.thr_near = 1120, .thr_far = 1088},
    {.thr_near = 32000, .thr_far = 32000},
    {.thr_near = 192, .thr_far = 160},
    {.thr_near = 352, .thr_far = 320},
};
/*A06 code for AL7160A-417 | AL7160A-2783 jiashixian at 20240607 end*/
/*A06 code for AL7160A-70 jiashixian at 20240416 end*/
static DEFINE_MUTEX(hx9031as_i2c_rw_mutex);
static DEFINE_MUTEX(hx9031as_ch_en_mutex);
static DEFINE_MUTEX(hx9031as_cali_mutex);

#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source *hx9031as_wake_lock = NULL;
#else
static struct wake_lock hx9031as_wake_lock;
#endif
//=============================================={ TYHX i2c wrap begin
//Starting from the specified start register, continuously write count values
static int linux_common_i2c_write(struct i2c_client *client, uint8_t *reg_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[HX9031AS_MAX_XFER_SIZE + 1] = {0};
    struct i2c_msg msg[1];

    if(count > HX9031AS_MAX_XFER_SIZE) {
        count = HX9031AS_MAX_XFER_SIZE;
        PRINT_ERR("block write over size!!!\n");
    }
    buf[0] = *reg_addr;
    memcpy(buf + 1, txbuf, count);

    msg[0].addr = client->addr;
    msg[0].flags = 0;//write
    msg[0].len = count + 1;
    msg[0].buf = buf;

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
    if (ARRAY_SIZE(msg) != ret) {
        PRINT_ERR("linux_common_i2c_write failed. ret=%d\n", ret);
        ret = -1;
        for(ii = 0; ii < msg[0].len; ii++) {
            PRINT_ERR("msg[0].addr=0x%04X, msg[0].flags=0x%04X, msg[0].len=0x%04X, msg[0].buf[%02d]=0x%02X\n",
                      msg[0].addr,
                      msg[0].flags,
                      msg[0].len,
                      ii,
                      msg[0].buf[ii]);
        }
    } else {
        ret = 0;
    }

    return ret;
}

//Starting from the specified start register, continuously read count values
static int linux_common_i2c_read(struct i2c_client *client, uint8_t *reg_addr, uint8_t *rxbuf, int count)
{
    int ret = -1;
    int ii = 0;
    struct i2c_msg msg[2];

    if(count > HX9031AS_MAX_XFER_SIZE) {
        count = HX9031AS_MAX_XFER_SIZE;
        PRINT_ERR("block read over size!!!\n");
    }

    msg[0].addr = client->addr;
    msg[0].flags = 0;//write
    msg[0].len = 1;
    msg[0].buf = reg_addr;

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;//read
    msg[1].len = count;
    msg[1].buf = rxbuf;

    ret = i2c_transfer(client->adapter, msg, ARRAY_SIZE(msg));
    if (ARRAY_SIZE(msg) != ret) {
        PRINT_ERR("linux_common_i2c_read failed. ret=%d\n", ret);
        ret = -1;
        PRINT_ERR("msg[0].addr=0x%04X, msg[0].flags=0x%04X, msg[0].len=0x%04X, msg[0].buf[0]=0x%02X\n",
                  msg[0].addr,
                  msg[0].flags,
                  msg[0].len,
                  msg[0].buf[0]);
        if(msg[1].len >= 1) {
            for(ii = 0; ii < msg[1].len; ii++) {
                PRINT_ERR("msg[1].addr=0x%04X, msg[1].flags=0x%04X, msg[1].len=0x%04X, msg[1].buf[%02d]=0x%02X\n",
                          msg[1].addr,
                          msg[1].flags,
                          msg[1].len,
                          ii,
                          msg[1].buf[ii]);
            }
        }
    } else {
        ret = 0;
    }

    return ret;
}

//return 0 for success, return -1 for errors.
static int hx9031as_read(uint8_t addr, uint8_t *rxbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9031as_i2c_rw_mutex);
    ret = linux_common_i2c_read(hx9031as_i2c_client, &addr, rxbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_read failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9031as_i2c_rw_mutex);
    return ret;
}

//return 0 for success, return -1 for errors.
static int hx9031as_write(uint8_t addr, uint8_t *txbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9031as_i2c_rw_mutex);
    ret = linux_common_i2c_write(hx9031as_i2c_client, &addr, txbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9031as_i2c_rw_mutex);
    return ret;
}
//==============================================} TYHX i2c wrap end

//=============================================={ TYHX common code begin
static void hx9031as_data_lock(uint8_t lock_flag)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};

    if(HX9031AS_DATA_LOCK == lock_flag) {
        ret = hx9031as_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }

        rx_buf[0] = rx_buf[0] | 0x10;
        ret = hx9031as_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    } else if(HX9031AS_DATA_UNLOCK == lock_flag) {
        ret = hx9031as_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }

        rx_buf[0] = rx_buf[0] & 0xE7;
        ret = hx9031as_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    } else {
        PRINT_ERR("ERROR!!! hx9031as_data_lock wrong para. now do data unlock!\n");
        ret = hx9031as_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }

        rx_buf[0] = rx_buf[0] & 0xE7;
        ret = hx9031as_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    }
}

static int hx9031as_id_check(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t rxbuf[1] = {0};

    for(ii = 0; ii < HX9031AS_ID_CHECK_COUNT; ii++) {
        ret = hx9031as_read(RO_60_DEVICE_ID, rxbuf, 1);
        if(ret < 0) {
            PRINT_ERR("hx9031as_read failed\n");
            continue;
        }
        hx9031as_pdata.device_id = rxbuf[0];
        if((HX9031AS_CHIP_ID == hx9031as_pdata.device_id))
            break;
        else
            continue;
    }

    if(HX9031AS_CHIP_ID == hx9031as_pdata.device_id) {
        ret = hx9031as_read(RO_5F_VERION_ID, rxbuf, 1);
        if(ret < 0) {
            PRINT_ERR("hx9031as_read failed\n");
        }
        hx9031as_pdata.version_id = rxbuf[0];
        PRINT_INF("success! hx9031as_pdata.device_id=0x%02X(HX9031AS) hx9031as_pdata.version_id=0x%02X\n",
                  hx9031as_pdata.device_id, hx9031as_pdata.version_id);
        return 0;
    } else {
        PRINT_ERR("failed! hx9031as_pdata.device_id=0x%02X(UNKNOW_CHIP_ID) hx9031as_pdata.version_id=0x%02X\n",
                  hx9031as_pdata.device_id, hx9031as_pdata.version_id);
        return -1;
    }
}

static void hx9031as_ch_cfg(void)
{
    int ret = -1;
    int ii = 0;
    uint16_t ch_cfg = 0;
    uint8_t cfg[HX9031AS_CH_NUM * 2] = {0};

    uint8_t CS0 = 0;
    uint8_t CS1 = 0;
    uint8_t CS2 = 0;
    uint8_t CS3 = 0;
    uint8_t CS4 = 0;
    uint8_t NA = 16;
    uint8_t CH0_POS = NA;
    uint8_t CH0_NEG = NA;
    uint8_t CH1_POS = NA;
    uint8_t CH1_NEG = NA;
    uint8_t CH2_POS = NA;
    uint8_t CH2_NEG = NA;
    uint8_t CH3_POS = NA;
    uint8_t CH3_NEG = NA;
    uint8_t CH4_POS = NA;
    uint8_t CH4_NEG = NA;

    ENTER;
    if(HX9023S_ON_BOARD == HX9031AS_CHIP_SELECT) {
        CS0 = 0; //Lshift0
        CS1 = 2; //Lshift2
        CS2 = 4; //Lshift4
        CS3 = 6; //Lshift6
        CS4 = 8; //Lshift8
        NA = 16; //Lshift16
        PRINT_INF("HX9023S_ON_BOARD\n");
    } else if(HX9031AS_ON_BOARD == HX9031AS_CHIP_SELECT) {
        CS0 = 4; //Lshift4
        CS1 = 2; //Lshift2
        CS2 = 6; //Lshift6
        CS3 = 0; //Lshift0
        CS4 = 8; //Lshift8
        NA = 16; //Lshift16
        PRINT_INF("HX9031AS_ON_BOARD\n");
    }

    //TODO Customized channel configuration begins====================
    //The positive and negative poles of each channel can be connected to any CS, and unused configurations are NA
    /*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 start*/
    /*A06 code for AL7160A-70 jiashixian at 20240416 start*/
    /*A06 code for AL7160A-107 jiashixian at 20240423 start*/
    /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
    CH0_POS = CS1;
    CH0_NEG = NA;
    CH1_POS = CS0;
    CH1_NEG = NA;
    CH2_POS = CS4;
    CH2_NEG = NA;
    CH3_POS = CS3;
    CH3_NEG = NA;
    CH4_POS = CS2;
    CH4_NEG = NA;
    /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
    /*A06 code for AL7160A-107 jiashixian at 20240423 end*/
    /*A06 code for AL7160A-70 jiashixian at 20240416 end*/
    /*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 end*/
    //end ======================================================

    ch_cfg = (uint16_t)((0x03 << CH0_POS) + (0x02 << CH0_NEG));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << CH1_POS) + (0x02 << CH1_NEG));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << CH2_POS) + (0x02 << CH2_NEG));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << CH3_POS) + (0x02 << CH3_NEG));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << CH4_POS) + (0x02 << CH4_NEG));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ret = hx9031as_write(RW_03_CH0_CFG_7_0, cfg, HX9031AS_CH_NUM * 2);
    if(0 != ret) {
        PRINT_ERR("hx9031as_write failed\n");
    }
}

static void hx9031as_reg_init(void)
{
    int ii = 0;
    int ret = -1;

    while(ii < (int)ARRAY_SIZE(hx9031as_reg_init_list)) {
        ret = hx9031as_write(hx9031as_reg_init_list[ii].addr, &hx9031as_reg_init_list[ii].val, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
        ii++;
    }
}

static void hx9031as_read_offset_dac(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9031AS_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    uint32_t data = 0;

    hx9031as_data_lock(HX9031AS_DATA_LOCK);
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9031AS_CH_NUM * bytes_per_channel;
    ret = hx9031as_read(RW_15_OFFSET_DAC0_7_0, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            data = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data = data & 0xFFF;//12bit
            data_offset_dac[ii] = data;
        }
    }
    hx9031as_data_lock(HX9031AS_DATA_UNLOCK);

    PRINT_DBG("OFFSET_DAC, %-8d, %-8d, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3], data_offset_dac[4]);
}

static void hx9031as_manual_offset_calibration(uint8_t ch_id)//Manually calibrate a specified channel once
{
    int ret = -1;
    uint8_t buf[2] = {0};

    mutex_lock(&hx9031as_cali_mutex);
    ret = hx9031as_read(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    ret = hx9031as_read(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }

    if (ch_id < 4) {
        buf[0] |= (1 << (ch_id + 4));
        ret = hx9031as_write(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    } else {
        buf[1] |= 0x10;
        ret = hx9031as_write(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    }

    PRINT_INF("ch_%d will calibrate in next convert cycle (ODR=%dms)\n", ch_id, HX9031AS_ODR_MS);//Can call this function twice in a single ord cycle
    TYHX_DELAY_MS(HX9031AS_ODR_MS);
    mutex_unlock(&hx9031as_cali_mutex);
}

static void hx9031as_manual_offset_calibration_all_chs(void)//Manually calibrate all channels
{
    int ret = -1;
    uint8_t buf[2] = {0};

    /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
    #ifdef HX9031AS_USER_NODE
    g_anfr_flag = false;
    #endif
    /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 end*/
    mutex_lock(&hx9031as_cali_mutex);
    ret = hx9031as_read(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    ret = hx9031as_read(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }

    buf[0] |= 0xF0;
    buf[1] |= 0x10;

    ret = hx9031as_write(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_write failed\n");
    }
    ret = hx9031as_write(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_write failed\n");
    }

    PRINT_INF("channels will calibrate in next convert cycle (ODR=%dms)\n", HX9031AS_ODR_MS);//
    TYHX_DELAY_MS(HX9031AS_ODR_MS);
    mutex_unlock(&hx9031as_cali_mutex);
}

static int32_t hx9031as_get_thres_near(uint8_t ch)
{
    int ret = -1;
    uint8_t buf[2] = {0};

    if(4 == ch) {
        ret = hx9031as_read(RW_9E_PROX_HIGH_DIFF_CFG_CH4_0, buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }
    } else {
        ret = hx9031as_read(RW_80_PROX_HIGH_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }
    }

    hx9031as_ch_thres[ch].thr_near = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;
    PRINT_INF("hx9031as_ch_thres[%d].thr_near=%d\n", ch, hx9031as_ch_thres[ch].thr_near);
    return hx9031as_ch_thres[ch].thr_near;
}

static int32_t hx9031as_get_thres_far(uint8_t ch)
{
    int ret = -1;
    uint8_t buf[2] = {0};

    if(4 == ch) {
        ret = hx9031as_read(RW_A2_PROX_LOW_DIFF_CFG_CH4_0, buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }
    } else {
        ret = hx9031as_read(RW_88_PROX_LOW_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }
    }

    hx9031as_ch_thres[ch].thr_far = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;
    PRINT_INF("hx9031as_ch_thres[%d].thr_far=%d\n", ch, hx9031as_ch_thres[ch].thr_far);
    return hx9031as_ch_thres[ch].thr_far;
}

static int32_t hx9031as_set_thres_near(uint8_t ch, int32_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9031as_ch_thres[ch].thr_near = (val & 0x03FF) * 32;

    if(4 == ch) {
        ret = hx9031as_write(RW_9E_PROX_HIGH_DIFF_CFG_CH4_0, buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    } else {
        ret = hx9031as_write(RW_80_PROX_HIGH_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    }

    PRINT_INF("hx9031as_ch_thres[%d].thr_near=%d\n", ch, hx9031as_ch_thres[ch].thr_near);
    return hx9031as_ch_thres[ch].thr_near;
}

static int32_t hx9031as_set_thres_far(uint8_t ch, int32_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9031as_ch_thres[ch].thr_far = (val & 0x03FF) * 32;

    if(4 == ch) {
        ret = hx9031as_write(RW_A2_PROX_LOW_DIFF_CFG_CH4_0, buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    } else {
        ret = hx9031as_write(RW_88_PROX_LOW_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
        }
    }

    PRINT_INF("hx9031as_ch_thres[%d].thr_far=%d\n", ch, hx9031as_ch_thres[ch].thr_far);
    return hx9031as_ch_thres[ch].thr_far;
}

static void hx9031as_get_prox_state(void)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    hx9031as_pdata.prox_state_reg = 0;
    ret = hx9031as_read(RO_6B_PROX_STATUS, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    hx9031as_pdata.prox_state_reg = buf[0];

    PRINT_INF("prox_state_reg=0x%02X\n", hx9031as_pdata.prox_state_reg);
}

#if 0
static int hx9031as_chs_en(uint8_t on_off)
{
    int ret = -1;
    uint8_t tx_buf[1] = {0};

    if((1 == on_off) && (0 == hx9031as_pdata.chs_en_flag)) {
        hx9031as_pdata.prox_state_reg = 0;
        tx_buf[0] = hx9031as_pdata.channel_used_flag;
        ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
            return -1;
        }
        hx9031as_pdata.chs_en_flag = 1;
        TYHX_DELAY_MS(10);
    } else if((0 == on_off) && (1 == hx9031as_pdata.chs_en_flag)) {
        tx_buf[0] = 0x00;
        ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
            return -1;
        }
        hx9031as_pdata.chs_en_flag = 0;
    }

    PRINT_INF("hx9031as_pdata.chs_en_flag=%d\n", hx9031as_pdata.chs_en_flag);
    return 0;
}
#endif

static void hx9031as_data_select(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[1] = {0};

    ret = hx9031as_read(RW_38_RAW_BL_RD_CFG, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }

    for(ii = 0; ii < 4; ii++) {//ch0~sh3
        hx9031as_pdata.sel_diff[ii] = buf[0] & (0x01 << ii);
        hx9031as_pdata.sel_lp[ii] = !hx9031as_pdata.sel_diff[ii];
        hx9031as_pdata.sel_bl[ii] = buf[0] & (0x10 << ii);
        hx9031as_pdata.sel_raw[ii] = !hx9031as_pdata.sel_bl[ii];
    }

    ret = hx9031as_read(RW_3A_INTERRUPT_CFG1, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }

    //ch4
    hx9031as_pdata.sel_diff[4] = buf[0] & (0x01 << 2);
    hx9031as_pdata.sel_lp[4] = !hx9031as_pdata.sel_diff[4];
    hx9031as_pdata.sel_bl[4] = buf[0] & (0x01 << 3);
    hx9031as_pdata.sel_raw[4] = !hx9031as_pdata.sel_bl[4];

//    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
//        PRINT_INF("diff[%d]=%d\n", ii, hx9031as_pdata.sel_diff[ii]);
//        PRINT_INF("  lp[%d]=%d\n", ii, hx9031as_pdata.sel_lp[ii]);
//        PRINT_INF("  bl[%d]=%d\n", ii, hx9031as_pdata.sel_bl[ii]);
//        PRINT_INF(" raw[%d]=%d\n", ii, hx9031as_pdata.sel_raw[ii]);
//    }
}

static void hx9031as_sample(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9031AS_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    int32_t data = 0;

    hx9031as_data_lock(HX9031AS_DATA_LOCK);
    hx9031as_data_select();
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9031AS_CH_NUM * bytes_per_channel;
    ret = hx9031as_read(RO_E8_RAW_BL_CH0_0, rx_buf, bytes_all_channels - bytes_per_channel); //data of ch0~ch3
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    ret = hx9031as_read(RO_B5_RAW_BL_CH4_0, rx_buf + (bytes_all_channels - bytes_per_channel), bytes_per_channel); //data of ch4
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if(16 == hx9031as_data_accuracy) {
            data = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));
            data = (data > 0x7FFF) ? (data - (0xFFFF + 1)) : data;
        } else {
            data = ((rx_buf[ii * bytes_per_channel + 2] << 16) | (rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data = (data > 0x7FFFFF) ? (data - (0xFFFFFF + 1)) : data;
        }
        data_raw[ii] = 0;
        data_bl[ii] = 0;
        if(true == hx9031as_pdata.sel_raw[ii]) {
            data_raw[ii] = data;
        }
        if(true == hx9031as_pdata.sel_bl[ii]) {
            data_bl[ii] = data;
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9031AS_CH_NUM * bytes_per_channel;
    ret = hx9031as_read(RO_F4_LP_DIFF_CH0_0, rx_buf, bytes_all_channels - bytes_per_channel);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    ret = hx9031as_read(RO_B8_LP_DIFF_CH4_0, rx_buf + (bytes_all_channels - bytes_per_channel), bytes_per_channel);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if(16 == hx9031as_data_accuracy) {
            data = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));
            data = (data > 0x7FFF) ? (data - (0xFFFF + 1)) : data;
        } else {
            data = ((rx_buf[ii * bytes_per_channel + 2] << 16) | (rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data = (data > 0x7FFFFF) ? (data - (0xFFFFFF + 1)) : data;
        }
        data_lp[ii] = 0;
        data_diff[ii] = 0;
        if(true == hx9031as_pdata.sel_lp[ii]) {
            data_lp[ii] = data;
        }
        if(true == hx9031as_pdata.sel_diff[ii]) {
            data_diff[ii] = data;
        }
    }
    //====================================================================================================
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if(true == hx9031as_pdata.sel_lp[ii] && true == hx9031as_pdata.sel_bl[ii]) {
            data_diff[ii] = data_lp[ii] - data_bl[ii];
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9031AS_CH_NUM * bytes_per_channel;
    ret = hx9031as_read(RW_15_OFFSET_DAC0_7_0, rx_buf, bytes_all_channels);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        data = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
        data = data & 0xFFF;//12bit
        data_offset_dac[ii] = data;
    }
    //====================================================================================================
    hx9031as_data_lock(HX9031AS_DATA_UNLOCK);

    PRINT_DBG("accuracy=%d\n", hx9031as_data_accuracy);
    PRINT_DBG("DIFF  , %-8d, %-8d, %-8d, %-8d, %-8d\n", data_diff[0], data_diff[1], data_diff[2], data_diff[3], data_diff[4]);
    PRINT_DBG("RAW   , %-8d, %-8d, %-8d, %-8d, %-8d\n", data_raw[0], data_raw[1], data_raw[2], data_raw[3], data_raw[4]);
    PRINT_DBG("OFFSET, %-8d, %-8d, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3], data_offset_dac[4]);
    PRINT_DBG("BL    , %-8d, %-8d, %-8d, %-8d, %-8d\n", data_bl[0], data_bl[1], data_bl[2], data_bl[3], data_bl[4]);
    PRINT_DBG("LP    , %-8d, %-8d, %-8d, %-8d, %-8d\n", data_lp[0], data_lp[1], data_lp[2], data_lp[3], data_lp[4]);
}
//==============================================} TYHX common code end

static void hx9031as_disable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(1 == hx9031as_irq_en_flag) {
        disable_irq_nosync(hx9031as_pdata.irq);
        hx9031as_irq_en_flag = 0;
        PRINT_DBG("irq_%d is disabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is disabled already!\n", irq);
    }
}

static void hx9031as_enable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(0 == hx9031as_irq_en_flag) {
        enable_irq(hx9031as_pdata.irq);
        hx9031as_irq_en_flag = 1;
        PRINT_DBG("irq_%d is enabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is enabled already!\n", irq);
    }
}
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
/*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
//=================================================================================================
//ANFR
#ifdef HX9031AS_USER_NODE
static void tyhx_anfr_irq()
{
    int ii = 0;
    uint8_t touch_state = 0;
    hx9031as_get_prox_state();
    touch_state = (hx9031as_pdata.prox_state_reg >> 1) & 0x1;
    PRINT_INF("touch_state= 0x%X", touch_state);
    if ((touch_state) != gs_last_state) {
        g_irq_count++;
        gs_last_state = touch_state;
        PRINT_INF("state update %d, g_irq_count= %d", gs_last_state, g_irq_count);
    } else {
        PRINT_INF("state not update %d", gs_last_state);
    }
    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if (data_bl[ii] < SELF_CALI_BL_0x04) {
            PRINT_DBG("BL = %d\n",data_bl[ii]);
            g_cali_count++;
            break;
        }
    }
}

static void tyhx_anfr_forcereport()
{
    int ii = 0;
    /*A06 code for AL7160A-1650 | AL7160A-2616 by jiashixian at 20240604 start*/
    uint8_t touch_state = 0;
    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
        if ((ii == 1) || (ii == 4)) {
            hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
            if (g_onoff_enable == false) {
                if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 1);
                    input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                }
            }
            PRINT_INF("%s ANFR reports touch", hx9031as_pdata.chs_info[ii].name);
        } else {
            hx9031as_get_prox_state();
            touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;
            if (PROXACTIVE == touch_state) {
                if (hx9031as_pdata.chs_info[ii].state == PROXACTIVE) {
                    PRINT_DBG("%s already PROXACTIVE, nothing report\n", hx9031as_pdata.chs_info[ii].name);
                } else {
                    if (g_onoff_enable == false) {
                        if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                            input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                            /*A06 code for AL7160A-3004 by jiashixian at 20240613 start*/
                            input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                            /*A06 code for AL7160A-3004 by jiashixian at 20240613 end*/
                            input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                        }
                    }
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9031as_pdata.chs_info[ii].name);
                }
            } else if (IDLE == touch_state) {
                    if (hx9031as_pdata.chs_info[ii].state == IDLE) {
                        PRINT_DBG("%s already released, nothing report\n", hx9031as_pdata.chs_info[ii].name);
                    } else {
                        if (g_onoff_enable == false) {
                            if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                                input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                                /*A06 code for AL7160A-3004 by jiashixian at 20240613 start*/
                                input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                                /*A06 code for AL7160A-3004 by jiashixian at 20240613 end*/
                                input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                            }
                        }
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
                    }
            }
        }
    }
    /*A06 code for AL7160A-1650 | AL7160A-2616 by jiashixian at 20240604 end*/
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

/*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 end*/
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
static void hx_tyhx_enable_report()
{
    int ii = 0;
    uint8_t touch_state = 0;

    hx9031as_get_prox_state();
    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;

            if (PROXACTIVE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9031as_pdata.chs_info[ii].name);
            } else if (IDLE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
            } else {
                PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
            }
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

static void hx_tyhx_disable_report()
{
    int ii = 0;
    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            hx9031as_pdata.chs_info[ii].state = IDLE;
            if (g_onoff_enable == false) {
                if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))) {
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                    input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                }
            }
            PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
        }
	} else {
        PRINT_DBG("g_self if NULL!");
	}
}

static ssize_t hx_tyhx_enable_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    int status = -1;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    PRINT_INF("dev->name:%s", temp_input_dev->name);
    if (g_self != NULL) {
        if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
            if(g_self->channel_status & g_ch1_mask1) {
                status = 1;
            } else {
                status = 0;
            }
        }
        /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
        if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
            if(g_self->channel_status & g_ch2_mask1) {
                status = 1;
            } else {
                status = 0;
            }
        }

        if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
            if(g_self->channel_status & g_ch3_mask1) {
                status = 1;
            } else {
                status = 0;
            }
        }
        /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
    } else {
        PRINT_DBG("g_self if NULL!");
    }
    return snprintf(buf, 8, "%d\n", status);
}

static ssize_t hx_tyhx_enable_store(struct device *dev,
    struct device_attribute *attr, const char *buf, size_t count)
{
    u8 enable = 0;
    int ret = 0;
    struct input_dev* temp_input_dev = NULL;

    temp_input_dev = container_of(dev, struct input_dev, dev);

    ret = kstrtou8(buf, 2, &enable);
    if (ret) {
        PRINT_ERR("Invalid Argument");
        return ret;
    }
    if (g_self != NULL) {
        PRINT_INF("dev->name:%s new_value = %u", temp_input_dev->name, enable);

        if (enable == 1) {
            if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
                if (!(g_self->channel_status & g_ch1_mask1)) {
                    g_self->channel_status |= g_ch1_mask1;
                }
            /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
            } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
                if (!(g_self->channel_status & g_ch2_mask1)) {
                    g_self->channel_status |= g_ch2_mask1;
                }
            } else if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
                if (!(g_self->channel_status & g_ch3_mask1)) {
                    g_self->channel_status |= g_ch3_mask1;
                }
            } else {
                PRINT_ERR("input_dev error");
            }
            /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
            PRINT_INF("g_self->channel_status = 0x%x", g_self->channel_status);

            if (g_self->channel_status & g_all_mask1) {
                hx9031as_irq_from_suspend_flag = 1;
            }

            if (hx9031as_irq_en_flag != 1) {
                /*A06 code for AL7160A-3687 jiashixian at 20240628 start*/
                enable_irq_wake(g_self->irq);
                /*A06 code for AL7160A-3687 jiashixian at 20240628 end*/
                hx9031as_irq_en_flag = 1;
                PRINT_INF("enable irq wake success.");
            }
            /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
            if (g_anfr_flag) {
                tyhx_anfr_forcereport();
            } else {
                hx_tyhx_enable_report();
            }
            /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 end*/
        } else if (enable == 0) {
            if (!strncmp(temp_input_dev->name, "grip_sensor_sub", sizeof("grip_sensor_sub"))) {
                if (g_self->channel_status & g_ch1_mask1) {
                    g_self->channel_status &= ~g_ch1_mask1;
                }
            /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
            } else if (!strncmp(temp_input_dev->name, "grip_sensor_wifi", sizeof("grip_sensor_wifi"))) {
                if (g_self->channel_status & g_ch2_mask1) {
                    g_self->channel_status &= ~g_ch2_mask1;
                }
            } else if (!strncmp(temp_input_dev->name, "grip_sensor", sizeof("grip_sensor"))) {
                if (g_self->channel_status & g_ch3_mask1) {
                    g_self->channel_status &= ~g_ch3_mask1;
                }
            } else {
                PRINT_ERR("input_dev error");
            }
            /*A06 code for AL7160A-417 jiashixian at 20240506 end*/

            if (!(g_self->channel_status & g_all_mask1)) {
                hx9031as_irq_from_suspend_flag = 0;
            }
            /*A06 code for AL7160A-3687 jiashixian at 20240628 start*/
            if (hx9031as_irq_en_flag != 0) {
                disable_irq_wake(g_self->irq);
                hx9031as_irq_en_flag = 0;
                PRINT_INF("disable irq wake success.");
            }
            /*A06 code for AL7160A-3687 jiashixian at 20240628 end*/
            hx_tyhx_disable_report();
        } else {
            PRINT_ERR("Invalid param. Usage: ehco 0/1 > enable");
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }

    return count;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
        hx_tyhx_enable_show, hx_tyhx_enable_store);

static struct attribute *ty_tyhx_attributes[] = {
    &dev_attr_enable.attr,
    NULL,
};

static struct attribute_group ty_tyhx_attributes_group = {
    .attrs = ty_tyhx_attributes
};

static ssize_t tyhx_vendor_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t tyhx_name_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t tyhx_grip_flush_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val = 0;
    int ret = 0;
    int ii = 0;
    ret = kstrtou8(buf, 10, &val);
    if (ret < 0) {
        PRINT_INF("Invalid value of input, input=%ld", ret);
        return -EINVAL;
    }

    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
         if (false == hx9031as_pdata.chs_info[ii].used) {
             PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
             continue;
         }
        if (!strcmp(dev->kobj.name, hx9031as_pdata.chs_info[ii].input_dev_abs->name)) {

            if (g_onoff_enable == false) {
                input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MAX, val);
                input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
            }

            PRINT_INF("ret=%u, name=%s", val, hx9031as_pdata.chs_info[ii].input_dev_abs->name);
            return count;
        }
    }

    PRINT_INF("unknown type");

    return count;
}

static DEVICE_ATTR(name, S_IRUGO, tyhx_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, tyhx_vendor_show, NULL);
static DEVICE_ATTR(grip_flush, S_IWUSR | S_IWGRP, NULL, tyhx_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
    &dev_attr_name,
    &dev_attr_vendor,
    &dev_attr_grip_flush,
    NULL,
};
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 end*/

/*A06 code for SR-AL7160A-01-190 by jiashixian at 20240430 start*/
ssize_t tyhx_receiver_turnon(const char *buf, size_t count)
{
    int receiver_status = 0;
    if (sscanf(buf, "%x", &receiver_status) != 1) {
        PRINT_ERR("(receiver_report) The number of data are wrong\n");
        return -EINVAL;
    }
    g_anfr_flag = false;
    PRINT_INF("(receiver_report) anfr = %d\n", g_anfr_flag);

    return count;
}
#endif
/*A06 code for SR-AL7160A-01-190 by jiashixian at 20240430 end*/

/*A06 code for SR-AL7160A-01-185 |  SR-AL7160A-01-183 by huofudong at 20240409 start*/
#ifdef HX9031AS_FACTORY_NODE
static bool g_facEnable = false;
void hx_factory_cali(void)
{
    PRINT_INF("Enter");
    if (g_self != NULL) {
        hx9031as_manual_offset_calibration_all_chs();
    }
}

ssize_t hx_factory_set_enable(const char *buf, size_t count)
{
    u32 enable = 0;

    if (sscanf(buf, "%x", &enable) != 1) {
        PRINT_INF("The number of param are wrong\n");
        return -EINVAL;
    }

    if (enable == 1) {
        g_facEnable = true;
        PRINT_INF("enable success.");
    } else if (enable == 0) {
        g_facEnable = false;
        PRINT_INF("disable success.");
    } else {
        PRINT_INF("Invalid param. Usage: ehco 0/1 > enable");
    }

    return count;
}

ssize_t hx_factory_get_enable(char *buf)
{
    return sprintf(buf, "%d\n", g_facEnable);
}
#endif
/*user*/
static void hx9031_enable_report(char band_name[25])
{
    int ii = 0;
    uint8_t touch_state = 0;
    PRINT_DBG("band_name=%s",band_name);
    hx9031as_get_prox_state();
    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            if (strcmp(hx9031as_pdata.chs_info[ii].name, band_name) != 0) {
                continue;
            }

            touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;

            if (PROXACTIVE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9031as_pdata.chs_info[ii].name);
            } else if (IDLE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
            } else {
                PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
            }
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

static void hx9031_disable_report(char band_name[25])
{
    int ii = 0;

    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            if (strcmp(hx9031as_pdata.chs_info[ii].name, band_name) != 0) {
                continue;
            }

            hx9031as_pdata.chs_info[ii].state = IDLE;
            if (g_onoff_enable == false) {
                if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))== 0) {
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                    input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                }
            }
            PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

ssize_t hx9031_set_enable(const char *buf, size_t count)
{
    u32 phase=0, enable=0;
    char band_name[25]="";

    if (g_self != NULL) {
        if (sscanf(buf, "%d,%d", &phase, &enable) == 2)
        {
            PRINT_DBG("%s phase=%d", enable ? "Enable" : "Disable", phase);

            if (enable == 1) {
                if (0 == phase) {
                    strcpy(band_name, "grip_sensor_sub");
                    if (!(g_self->channel_status & g_ch1_mask1)) {
                        g_self->channel_status |= g_ch1_mask1;
                    }
                } else if (1 == phase) {
                    /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
                    strcpy(band_name, "grip_sensor_wifi");
                    if (!(g_self->channel_status & g_ch2_mask1)) {
                        g_self->channel_status |= g_ch2_mask1;
                    }
                } else if (2 == phase) {
                    strcpy(band_name, "grip_sensor");
                    if (!(g_self->channel_status & g_ch3_mask1)) {
                        g_self->channel_status |= g_ch3_mask1;
                    }
                    /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
                } else {
                    PRINT_DBG("phase number error");
                }

                PRINT_DBG("self->channel_status = 0x%x", g_self->channel_status);

                hx9031_enable_report(band_name);

            } else if (enable == 0) {
                if (0 == phase) {
                    strcpy(band_name, "grip_sensor_sub");
                    if (g_self->channel_status & g_ch1_mask1) {
                        g_self->channel_status &= ~g_ch1_mask1;
                    }
                } else if (1 == phase) {
                    /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
                    strcpy(band_name, "grip_sensor_wifi");
                    if (g_self->channel_status & g_ch2_mask1) {
                        g_self->channel_status &= ~g_ch2_mask1;
                    }
                } else if (2 == phase) {
                    strcpy(band_name, "grip_sensor");
                    if (g_self->channel_status & g_ch3_mask1) {
                        g_self->channel_status &= ~g_ch3_mask1;
                    }
                    /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
                } else {
                    PRINT_DBG("phase  number error");
                }

                PRINT_DBG("self->channel_status = 0x%x", g_self->channel_status);

                hx9031_disable_report(band_name);
            } else {
                PRINT_DBG("echo 2,1 > enable #enable phase 2");
            }
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
    return count;
}

ssize_t hx9031_get_enable(char *buf)
{
    int bytes=0;
    bytes += sprintf(buf+bytes, "Current enabled phases: reg_val=0x%X",  g_self->channel_status);
    return bytes;
}

void hx9031_user_cali(void)
{
    PRINT_INF("hx9031_user_cali Enter");
    hx9031as_manual_offset_calibration_all_chs();
}


static void hx9031_onoff_enablereport()
{
    int ii = 0;
    uint8_t touch_state = 0;

    hx9031as_get_prox_state();
    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;

            if (PROXACTIVE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9031as_pdata.chs_info[ii].name);
            } else if (IDLE == touch_state) {
                if (g_onoff_enable == false) {
                    if ((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status)) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
            } else {
                PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
            }
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

static void hx9031_onoff_disablereport()
{
    int ii = 0;

    if (g_self != NULL) {
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
            /*A06 code for AL7160A-1265 by lichang at 20240520 start*/
            if (false == hx9031as_pdata.chs_info[ii].enabled) {
                PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }
            /*A06 code for AL7160A-1265 by lichang at 20240520 end*/
            if (false == hx9031as_pdata.chs_info[ii].used) {
                PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
                continue;
            }

            hx9031as_pdata.chs_info[ii].state = IDLE;
            if (g_onoff_enable == false) {
                if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))) {
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                    input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                    input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                }
            }
            PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
}

ssize_t hx9031_set_onoff(const char *buf, size_t count)
{
    if (g_self != NULL) {
        if (!strncmp(buf, "0", 1)) {
            PRINT_INF("self->channel_status = 0x%x", g_self->channel_status);
            hx9031_onoff_disablereport();
            g_onoff_enable = true;
            PRINT_INF("onoff Function set of off");
        } else if (!strncmp(buf, "1", 1)) {
            g_onoff_enable = false;
            PRINT_INF("self->channel_status = 0x%x", g_self->channel_status);
            hx9031_onoff_enablereport();
            PRINT_INF("onoff Function set of on");
        } else {
            PRINT_DBG("invalid param!");
        }
    } else {
        PRINT_DBG("g_self if NULL!");
    }
    return count;
}

ssize_t hx9031_get_onoff(char *buf)
{
    return snprintf(buf, 8, "%d\n", g_onoff_enable);
}

static ssize_t hx9031_set_dumpinfo(const char *buf, size_t count)
{
    int value = 0;

    if (sscanf(buf, "%d", &value) != 1) {
        PRINT_DBG("Invalid command format. Usage: ehco 1 > dumpinfo");
        return -EINVAL;
    }
/*A06 code for AL7160A-417 jiashixian at 20240506 start*/
    if (value == 0) {
        g_channel1 = 1;
    } else if (value == 1) {
        g_channel1 = 4;
    } else if (value == 2) {
        g_channel1 = 3;
    } else {
        g_channel1 = value;
    }
/*A06 code for AL7160A-417 jiashixian at 20240506 end*/
    return count;
}

static ssize_t hx9031_get_dumpinfo(char *buf)
{
    uint8_t state = 0;
    uint8_t a = 0;
    hx9031as_sample();
    hx9031as_get_prox_state();
    state = (hx9031as_pdata.prox_state_reg >> g_channel1) & 0x1;
    
    return snprintf(buf, PAGE_SIZE, "%ld,%d,%d,%d,%d,%d,%d\n",
            hx9031as_pdata.fw_num, state, a, a,
            data_lp[g_channel1], data_offset_dac[g_channel1],data_diff[g_channel1]);
}

#ifdef HX9031AS_FACTORY_NODE
static void hx9031as_input_report_key(void)
{
    int ii = 0;
    uint8_t touch_state = 0;

    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if (false == hx9031as_pdata.chs_info[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
            continue;
        }
        if (false == hx9031as_pdata.chs_info[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
            continue;
        }

        touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;

        if (BODYACTIVE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == BODYACTIVE)
                PRINT_DBG("%s already BODYACTIVE, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_close, 1);
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_close, 0);              
                hx9031as_pdata.chs_info[ii].state = BODYACTIVE;
                PRINT_DBG("%s report BODYACTIVE(5mm)\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else if (PROXACTIVE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == PROXACTIVE)
                PRINT_DBG("%s already PROXACTIVE, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_close, 1);
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_close, 0); 
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE(15mm)\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else if (IDLE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == IDLE)
                PRINT_DBG("%s already released, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_far, 1);
                input_report_key(hx9031as_pdata.chs_info[ii].input_key, hx9031as_pdata.chs_info[ii].keycode_far, 0); 
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else {
            PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
        }
        input_sync(hx9031as_pdata.chs_info[ii].input_key);
    }
}

#else

static void hx9031as_input_report_abs(void)
{
    int ii = 0;
    uint8_t touch_state = 0;

    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if (false == hx9031as_pdata.chs_info[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
            continue;
        }
        if (false == hx9031as_pdata.chs_info[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
            continue;
        }

        touch_state = (hx9031as_pdata.prox_state_reg >> ii) & 0x1;

        if (BODYACTIVE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == BODYACTIVE)
                PRINT_DBG("%s already BODYACTIVE, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                if (g_onoff_enable == false) {
                    if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = BODYACTIVE;
                PRINT_DBG("%s report BODYACTIVE(5mm)\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else if (PROXACTIVE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == PROXACTIVE)
                PRINT_DBG("%s already PROXACTIVE, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                if (g_onoff_enable == false) {
                    if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 1);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE(15mm)\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else if (IDLE == touch_state) {
            if (hx9031as_pdata.chs_info[ii].state == IDLE)
                PRINT_DBG("%s already released, nothing report\n", hx9031as_pdata.chs_info[ii].name);
            else {
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif
                if (g_onoff_enable == false) {
                    if (((g_self->channel_status) & (hx9031as_pdata.chs_info[ii].phase_status))) {
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_MISC, 2);
                        input_report_rel(hx9031as_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
                        input_sync(hx9031as_pdata.chs_info[ii].input_dev_abs);
                    }
                }
                hx9031as_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9031as_pdata.chs_info[ii].name);
            }
        } else {
            PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
        }
    }
}
#endif
/*A06 code for SR-AL7160A-01-185 |  SR-AL7160A-01-183 by huofudong at 20240409 end*/

//"tyhx,irq-gpio"Interrupt corresponding gpio number
//"tyhx,channel-flag"Each bit corresponds to a used channel, for example, 0xF represents 0, 1, 2, and 3 channels available
static int hx9031as_parse_dt(struct device *dev)
{
    int ret = -1;
    struct device_node *dt_node = dev->of_node;

    if (NULL == dt_node) {
        PRINT_ERR("No DTS node\n");
        return -ENODEV;
    }

#if HX9031AS_TEST_ON_MTK_DEMO_XY6761
    ret = of_property_read_u32(dt_node, "xy6761-fake-irq-gpio", &hx9031as_pdata.irq_gpio);
    if(ret < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#else
    hx9031as_pdata.irq_gpio = of_get_named_gpio_flags(dt_node, "tyhx,irq-gpio", 0, NULL);
    if(hx9031as_pdata.irq_gpio < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#endif

    PRINT_INF("hx9031as_pdata.irq_gpio=%d\n", hx9031as_pdata.irq_gpio);

    hx9031as_pdata.channel_used_flag = HX9031AS_CH_USED;
    ret = of_property_read_u32(dt_node, "tyhx,channel-flag", &hx9031as_pdata.channel_used_flag);
     PRINT_INF("false hx9031as_pdata.channel_used_flag=0x%X\n", hx9031as_pdata.channel_used_flag);//Effective channel flag bit: 9031AS maximum incoming 0x1F
    if(ret < 0) {
        PRINT_ERR("\"tyhx,channel-flag\" is not set in DT\n");
        return -1;
    }
    if(hx9031as_pdata.channel_used_flag > ((1 << HX9031AS_CH_NUM) - 1)) {
        PRINT_ERR("the max value of channel_used_flag is 0x%X\n", ((1 << HX9031AS_CH_NUM) - 1));
        return -1;
    }
    PRINT_INF("hx9031as_pdata.channel_used_flag=0x%X\n", hx9031as_pdata.channel_used_flag);
    /*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
    if (of_property_read_u32(dt_node, "tyhx,fw-num", &hx9031as_pdata.fw_num)) {
        PRINT_ERR("Failed to get fw-num");
        return -EINVAL;
    }
    PRINT_INF("hx9031as_pdata->fw_num=%d\n", hx9031as_pdata.fw_num);
    /*A06 code for  SR-AL7160A-01-183 by huofudong at 20240409 end*/
    return 0;
}

static int hx9031as_gpio_init(void)
{
    int ret = -1;
    if (gpio_is_valid(hx9031as_pdata.irq_gpio)) {
        ret = gpio_request(hx9031as_pdata.irq_gpio, "hx9031as_irq_gpio");
        if (ret < 0) {
            PRINT_ERR("gpio_request failed. ret=%d\n", ret);
            return ret;
        }
        ret = gpio_direction_input(hx9031as_pdata.irq_gpio);
        if (ret < 0) {
            PRINT_ERR("gpio_direction_input failed. ret=%d\n", ret);
            gpio_free(hx9031as_pdata.irq_gpio);
            return ret;
        }
    } else {
        PRINT_ERR("Invalid gpio num\n");
        return -1;
    }

    hx9031as_pdata.irq = gpio_to_irq(hx9031as_pdata.irq_gpio);
    PRINT_INF("hx9031as_pdata.irq_gpio=%d hx9031as_pdata.irq=%d\n", hx9031as_pdata.irq_gpio, hx9031as_pdata.irq);
    return 0;
}

static void hx9031as_gpio_deinit(void)
{
    ENTER;
    gpio_free(hx9031as_pdata.irq_gpio);
}

static void hx9031as_power_on(uint8_t on)
{
    if(on) {
        PRINT_INF("power on\n");
    } else {
        PRINT_INF("power off\n");
    }
}

#if HX9031AS_TEST_CHS_EN //Use a fully open and fully closed strategy during channel testing
static int hx9031as_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t tx_buf[1] = {0};

    en = !!en;
    if(ch_id >= HX9031AS_CH_NUM) {
        PRINT_ERR("channel index over range!!! ch_enable_status=0x%02X (ch_id=%d, en=%d)\n",
                  ch_enable_status, ch_id, en);
        return -1;
    }

    if(1 == en) {
        if(0 == ch_enable_status) {
            hx9031as_pdata.prox_state_reg = 0;
            tx_buf[0] = hx9031as_pdata.channel_used_flag;
            ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
            if(0 != ret) {
                PRINT_ERR("hx9031as_write failed\n");
                return -1;
            }
        }
        ch_enable_status |= (1 << ch_id);
    } else {
        ch_enable_status &= ~(1 << ch_id);
        if(0 == ch_enable_status) {
            tx_buf[0] = 0x00;
            ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
            if(0 != ret) {
                PRINT_ERR("hx9031as_write failed\n");
                return -1;
            }
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d disabled)\n", ch_enable_status, ch_id);
    }
    return 0;
}

#else

static int hx9031as_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};
    uint8_t tx_buf[1] = {0};

    en = !!en;
    if(ch_id >= HX9031AS_CH_NUM) {
        PRINT_ERR("channel index over range!!! ch_enable_status=0x%02X (ch_id=%d, en=%d)\n",
                  ch_enable_status, ch_id, en);
        return -1;
    }

    ret = hx9031as_read(RW_24_CH_NUM_CFG, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
        return -1;
    }
    ch_enable_status = rx_buf[0];

    if(1 == en) {
        if(0 == ch_enable_status) {//Open the first ch
            hx9031as_pdata.prox_state_reg = 0;
        }
        ch_enable_status |= (1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d enabled)\n", ch_enable_status, ch_id);
        TYHX_DELAY_MS(10);
    } else {
        ch_enable_status &= ~(1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9031as_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d disabled)\n", ch_enable_status, ch_id);
    }
    return 0;
}
#endif
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
static int hx9031as_ch_en_hal(uint8_t ch_id, uint8_t enable)//yasin: for upper layer
{
    int ret = -1;

    mutex_lock(&hx9031as_ch_en_mutex);
    if (1 == enable) {
        PRINT_INF("enable ch_%d(name:%s)\n", ch_id, hx9031as_pdata.chs_info[ch_id].name);
        ret = hx9031as_ch_en(ch_id, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_ch_en failed\n");
            mutex_unlock(&hx9031as_ch_en_mutex);
            return -1;
        }
        hx9031as_pdata.chs_info[ch_id].state = IDLE;
        hx9031as_pdata.chs_info[ch_id].enabled = true;

#ifdef CONFIG_PM_WAKELOCKS
        __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
        wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif

    } else if (0 == enable) {
        PRINT_INF("disable ch_%d(name:%s)\n", ch_id, hx9031as_pdata.chs_info[ch_id].name);
        ret = hx9031as_ch_en(ch_id, 0);
        if(0 != ret) {
            PRINT_ERR("hx9031as_ch_en failed\n");
            mutex_unlock(&hx9031as_ch_en_mutex);
            return -1;
        }
        hx9031as_pdata.chs_info[ch_id].state = IDLE;
        hx9031as_pdata.chs_info[ch_id].enabled = false;

#ifdef CONFIG_PM_WAKELOCKS
        __pm_wakeup_event(hx9031as_wake_lock, 1000);
#else
        wake_lock_timeout(&hx9031as_wake_lock, HZ * 1);
#endif

    } else {
        PRINT_ERR("unknown enable symbol\n");
    }
    mutex_unlock(&hx9031as_ch_en_mutex);

    return 0;
}

static void hx9031as_polling_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9031as_ch_en_mutex);
    hx9031as_sample();
    hx9031as_get_prox_state();

#ifdef HX9031AS_FACTORY_NODE
    hx9031as_input_report_key();
#else
    hx9031as_input_report_abs();
#endif

    if(1 == hx9031as_polling_enable)
        schedule_delayed_work(&hx9031as_pdata.polling_work, msecs_to_jiffies(hx9031as_polling_period_ms));
    mutex_unlock(&hx9031as_ch_en_mutex);
    return;
}

static irqreturn_t hx9031as_irq_handler(int irq, void *pvoid)
{
    ENTER;
    mutex_lock(&hx9031as_ch_en_mutex);
    if(1 == hx9031as_irq_from_suspend_flag) {
        hx9031as_irq_from_suspend_flag = 0;
        PRINT_INF("delay 50ms for waiting the i2c controller enter working mode\n");
        TYHX_DELAY_MS(50);//if awakened from suspend, this delay ensures that the i2c controller also wakes up from sleep and enters a working state
    }
    /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
   #ifdef HX9031AS_USER_NODE
    if ((g_irq_count >= g_hx_irq_maxnum) || (g_cali_count >= g_hx_cali_maxnum)) {
        g_anfr_flag = false;
    }
    PRINT_INF("cali_count:%d, anfr_flag:%d", g_cali_count, g_anfr_flag);
    #endif
    /*A06 code for SR-AL7160A-01-177 by jiashixian at 20240430 end*/
    hx9031as_sample();
    hx9031as_get_prox_state();

#ifdef HX9031AS_FACTORY_NODE
    hx9031as_input_report_key();
#else
    /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 start*/
    if (g_anfr_flag) {
        tyhx_anfr_irq();
        tyhx_anfr_forcereport();
    } else {
        hx9031as_input_report_abs();
    }
    /*A06 code for SR-AL7160A-01-189 by jiashixian at 20240430 end*/
#endif

    mutex_unlock(&hx9031as_ch_en_mutex);
    return IRQ_HANDLED;
}
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test begin
static ssize_t hx9031as_raw_data_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *p = buf;
    int ii = 0;

    ENTER;
    hx9031as_sample();
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        p += snprintf(p, PAGE_SIZE, "ch[%d]: DIFF=%-8d, RAW=%-8d, OFFSET=%-8d, BL=%-8d, LP=%-8d\n",
                      ii, data_diff[ii], data_raw[ii], data_offset_dac[ii], data_bl[ii], data_lp[ii]);
    }
    return (p - buf); //Returns the actual number of characters placed in buf
}

static ssize_t hx9031as_reg_write_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    unsigned int val = 0;
    uint8_t addr = 0;
    uint8_t tx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
        PRINT_ERR("please input two HEX numbers: aa,bb (aa: reg_address, bb: value_to_be_set)\n");
        return -EINVAL;
    }

    addr = (uint8_t)reg_address;
    tx_buf[0] = (uint8_t)val;

    ret = hx9031as_write(addr, tx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_write failed\n");
    }

    PRINT_INF("WRITE:Reg0x%02X=0x%02X\n", addr, tx_buf[0]);
    return count;
}

static ssize_t hx9031as_reg_read_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    uint8_t addr = 0;
    uint8_t rx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x", &reg_address) != 1) {
        PRINT_ERR("please input a HEX number\n");
        return -EINVAL;
    }
    addr = (uint8_t)reg_address;

    ret = hx9031as_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031as_read failed\n");
    }

    PRINT_INF("READ:Reg0x%02X=0x%02X\n", addr, rx_buf[0]);
    return count;
}

static ssize_t hx9031as_channel_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ch_id = 0;
    int en = 0;

    ENTER;
    if (sscanf(buf, "%d,%d", &ch_id, &en) != 2) {
        PRINT_ERR("please input two DEC numbers: ch_id,en (ch_id: channel number, en: 1=enable, 0=disable)\n");
        return -EINVAL;
    }

    if((ch_id >= HX9031AS_CH_NUM) || (ch_id < 0)) {
        PRINT_ERR("channel number out of range, the effective number is 0~%d\n", HX9031AS_CH_NUM - 1);
        return -EINVAL;
    }

    if ((hx9031as_pdata.channel_used_flag >> ch_id) & 0x01) {
        hx9031as_ch_en_hal(ch_id, (en > 0) ? 1 : 0);
    } else {
        PRINT_ERR("ch_%d is unused, you can not enable or disable an unused channel\n", ch_id);
    }

    return count;
}

static ssize_t hx9031as_channel_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    ENTER;
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if ((hx9031as_pdata.channel_used_flag >> ii) & 0x1) {
            PRINT_INF("hx9031as_pdata.chs_info[%d].enabled=%d\n", ii, hx9031as_pdata.chs_info[ii].enabled);
            p += snprintf(p, PAGE_SIZE, "hx9031as_pdata.chs_info[%d].enabled=%d\n", ii, hx9031as_pdata.chs_info[ii].enabled);
        }
    }

    return (p - buf);
}

static ssize_t hx9031as_manual_offset_calibration_show(struct class *class, struct class_attribute *attr, char *buf)
{
    hx9031as_read_offset_dac();
    return sprintf(buf, "OFFSET_DAC, %-8d, %-8d, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3], data_offset_dac[4]);
}

static ssize_t hx9031as_manual_offset_calibration_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned long val;
    uint8_t ch_id = 0;

    ENTER;
    if (kstrtoul(buf, 10, &val)) {
        PRINT_ERR("Invalid Argument\n");
        return -EINVAL;
    }
    ch_id = (uint8_t)val;

    if (99 == ch_id) {
        PRINT_INF("you are enter the calibration test mode, all channels will be calibrated\n");
        hx9031as_manual_offset_calibration_all_chs();
        return count;
    }

    if (ch_id < HX9031AS_CH_NUM)
        hx9031as_manual_offset_calibration(ch_id);
    else
        PRINT_ERR(" \"echo ch_id > calibrate\" to do a manual calibrate(ch_id is a channel num (0~%d)\n", HX9031AS_CH_NUM);
    return count;
}

static ssize_t hx9031as_prox_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
    hx9031as_get_prox_state();
    return sprintf(buf, "prox_state_reg=0x%02X\n", hx9031as_pdata.prox_state_reg);
}

static ssize_t hx9031as_polling_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int value = 0;
    int ret = -1;

    ENTER;
    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        goto exit;
    }

    if (value >= 10) {
        hx9031as_polling_period_ms = value;
        if(1 == hx9031as_polling_enable) {
            PRINT_INF("polling is already enabled!, no need to do enable again!, just update the polling period\n");
            goto exit;
        }

        hx9031as_polling_enable = 1;
        hx9031as_disable_irq(hx9031as_pdata.irq); //Close the interrupt and stop the work queue corresponding to the bottom half of the interrupt

        PRINT_INF("polling started! period=%dms\n", hx9031as_polling_period_ms);
        schedule_delayed_work(&hx9031as_pdata.polling_work, msecs_to_jiffies(hx9031as_polling_period_ms));
    } else {
        if(0 == hx9031as_polling_enable) {
            PRINT_INF("polling is already disabled!, no need to do again!\n");
            goto exit;
        }
        hx9031as_polling_period_ms = 0;
        hx9031as_polling_enable = 0;
        PRINT_INF("polling stoped!\n");

        cancel_delayed_work(&hx9031as_pdata.polling_work);//Stop the corresponding work queue for polling and restart interrupt mode
        hx9031as_enable_irq(hx9031as_pdata.irq);
    }

exit:
    return count;
}

static ssize_t hx9031as_polling_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031as_polling_enable=%d hx9031as_polling_period_ms=%d\n", hx9031as_polling_enable, hx9031as_polling_period_ms);
    return sprintf(buf, "hx9031as_polling_enable=%d hx9031as_polling_period_ms=%d\n", hx9031as_polling_enable, hx9031as_polling_period_ms);
}

static ssize_t hx9031as_loglevel_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("tyhx_log_level=%d\n", tyhx_log_level);
    return sprintf(buf, "tyhx_log_level=%d\n", tyhx_log_level);
}

static ssize_t hx9031as_loglevel_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    tyhx_log_level = value;
    PRINT_INF("set tyhx_log_level=%d\n", tyhx_log_level);
    return count;
}

static ssize_t hx9031as_accuracy_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031as_data_accuracy=%d\n", hx9031as_data_accuracy);
    return sprintf(buf, "hx9031as_data_accuracy=%d\n", hx9031as_data_accuracy);
}

static ssize_t hx9031as_accuracy_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9031as_data_accuracy = (24 == value) ? 24 : 16;
    PRINT_INF("set hx9031as_data_accuracy=%d\n", hx9031as_data_accuracy);
    return count;
}

static ssize_t hx9031as_threshold_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int ch = 0;
    unsigned int thr_near = 0;
    unsigned int thr_far = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d", &ch, &thr_near, &thr_far) != 3) {
        PRINT_ERR("please input 3 numbers in DEC: ch,thr_near,thr_far (eg: 0,500,300)\n");
        return -EINVAL;
    }

    if(ch >= HX9031AS_CH_NUM || thr_near > (0x03FF * 32) || thr_far > thr_near) {
        PRINT_ERR("input value over range! (valid value: ch=%d, thr_near=%d, thr_far=%d)\n", ch, thr_near, thr_far);
        return -EINVAL;
    }

    thr_near = (thr_near / 32) * 32;
    thr_far = (thr_far / 32) * 32;

    PRINT_INF("set threshold: ch=%d, thr_near=%d, thr_far=%d\n", ch, thr_near, thr_far);
    hx9031as_set_thres_near(ch, thr_near);
    hx9031as_set_thres_far(ch, thr_far);

    return count;
}

static ssize_t hx9031as_threshold_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        hx9031as_get_thres_near(ii);
        hx9031as_get_thres_far(ii);
        PRINT_INF("ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9031as_ch_thres[ii].thr_near, hx9031as_ch_thres[ii].thr_far);
        p += snprintf(p, PAGE_SIZE, "ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9031as_ch_thres[ii].thr_near, hx9031as_ch_thres[ii].thr_far);
    }

    return (p - buf);
}

static ssize_t hx9031as_dump_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ret = -1;
    int ii = 0;
    char *p = buf;
    uint8_t rx_buf[1] = {0};

    for(ii = 0; ii < ARRAY_SIZE(hx9031as_reg_init_list); ii++) {
        ret = hx9031as_read(hx9031as_reg_init_list[ii].addr, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031as_read failed\n");
        }
        PRINT_INF("0x%02X=0x%02X\n", hx9031as_reg_init_list[ii].addr, rx_buf[0]);
        p += snprintf(p, PAGE_SIZE, "0x%02X=0x%02X\n", hx9031as_reg_init_list[ii].addr, rx_buf[0]);
    }

    p += snprintf(p, PAGE_SIZE, "driver version:%s\n", HX9031AS_DRIVER_VER);
    return (p - buf);
}

static ssize_t hx9031as_offset_dac_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    hx9031as_read_offset_dac();

    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        PRINT_INF("data_offset_dac[%d]=%dpF\n", ii, data_offset_dac[ii] * 58 / 1000);
        p += snprintf(p, PAGE_SIZE, "ch[%d]=%dpF ", ii, data_offset_dac[ii] * 58 / 1000);
    }
    p += snprintf(p, PAGE_SIZE, "\n");

    return (p - buf);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_raw_data = __ATTR(raw_data, 0664, hx9031as_raw_data_show, NULL);
static struct class_attribute class_attr_reg_write = __ATTR(reg_write,  0664, NULL, hx9031as_reg_write_store);
static struct class_attribute class_attr_reg_read = __ATTR(reg_read, 0664, NULL, hx9031as_reg_read_store);
static struct class_attribute class_attr_channel_en = __ATTR(channel_en, 0664, hx9031as_channel_en_show, hx9031as_channel_en_store);
static struct class_attribute class_attr_calibrate = __ATTR(calibrate, 0664, hx9031as_manual_offset_calibration_show, hx9031as_manual_offset_calibration_store);
static struct class_attribute class_attr_prox_state = __ATTR(prox_state, 0664, hx9031as_prox_state_show, NULL);
static struct class_attribute class_attr_polling_period = __ATTR(polling_period, 0664, hx9031as_polling_show, hx9031as_polling_store);
static struct class_attribute class_attr_threshold = __ATTR(threshold, 0664, hx9031as_threshold_show, hx9031as_threshold_store);
static struct class_attribute class_attr_loglevel = __ATTR(loglevel, 0664, hx9031as_loglevel_show, hx9031as_loglevel_store);
static struct class_attribute class_attr_accuracy = __ATTR(accuracy, 0664, hx9031as_accuracy_show, hx9031as_accuracy_store);
static struct class_attribute class_attr_dump = __ATTR(dump, 0664, hx9031as_dump_show, NULL);
static struct class_attribute class_attr_offset_dac = __ATTR(offset_dac, 0664, hx9031as_offset_dac_show, NULL);

static struct attribute *hx9031as_class_attrs[] = {
    &class_attr_raw_data.attr,
    &class_attr_reg_write.attr,
    &class_attr_reg_read.attr,
    &class_attr_channel_en.attr,
    &class_attr_calibrate.attr,
    &class_attr_prox_state.attr,
    &class_attr_polling_period.attr,
    &class_attr_threshold.attr,
    &class_attr_loglevel.attr,
    &class_attr_accuracy.attr,
    &class_attr_dump.attr,
    &class_attr_offset_dac.attr,
    NULL,
};
ATTRIBUTE_GROUPS(hx9031as_class);
#else
static struct class_attribute hx9031as_class_attributes[] = {
    __ATTR(raw_data, 0664, hx9031as_raw_data_show, NULL),
    __ATTR(reg_write,  0664, NULL, hx9031as_reg_write_store),
    __ATTR(reg_read, 0664, NULL, hx9031as_reg_read_store),
    __ATTR(channel_en, 0664, hx9031as_channel_en_show, hx9031as_channel_en_store),
    __ATTR(calibrate, 0664, hx9031as_manual_offset_calibration_show, hx9031as_manual_offset_calibration_store),
    __ATTR(prox_state, 0664, hx9031as_prox_state_show, NULL),
    __ATTR(polling_period, 0664, hx9031as_polling_show, hx9031as_polling_store),
    __ATTR(threshold, 0664, hx9031as_threshold_show, hx9031as_threshold_store),
    __ATTR(loglevel, 0664, hx9031as_loglevel_show, hx9031as_loglevel_store),
    __ATTR(accuracy, 0664, hx9031as_accuracy_show, hx9031as_accuracy_store),
    __ATTR(dump, 0664, hx9031as_dump_show, NULL),
    __ATTR(offset_dac, 0664, hx9031as_offset_dac_show, NULL),
    __ATTR_NULL,
};
#endif

struct class hx9031as_class = {
        .name = "hx9031as",
        .owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups = hx9031as_class_groups,
#else
        .class_attrs = hx9031as_class_attributes,
#endif
    };
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test end
/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 start*/
#ifdef HX9031AS_FACTORY_NODE
static int hx9031as_input_init_key(struct i2c_client *client)
{
    int ii = 0;
    int iii = 0;
    int jj = 0;
    int ret = -1;

    hx9031as_pdata.chs_info = devm_kzalloc(&client->dev,
                                           sizeof(struct hx9031as_channel_info) * HX9031AS_CH_NUM,
                                           GFP_KERNEL);
    if (NULL == hx9031as_pdata.chs_info) {
        PRINT_ERR("devm_kzalloc failed\n");
        ret = -ENOMEM;
        goto failed_devm_kzalloc;
    }

    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        hx9031as_pdata.chs_info[ii].used = false;
        hx9031as_pdata.chs_info[ii].enabled = false;
        if ((hx9031as_pdata.channel_used_flag >> ii) & 0x1) {
            hx9031as_pdata.chs_info[ii].used = true;
            hx9031as_pdata.chs_info[ii].state = IDLE;
        }
    }
    /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
    hx9031as_pdata.chs_info[1].keycode_close = KEY_SAR1_CLOSE;
    hx9031as_pdata.chs_info[1].keycode_far = KEY_SAR1_FAR;
    hx9031as_pdata.chs_info[3].keycode_close = KEY_SAR2_CLOSE;
    hx9031as_pdata.chs_info[3].keycode_far = KEY_SAR2_FAR;
    hx9031as_pdata.chs_info[4].keycode_close = KEY_SAR3_CLOSE;
    hx9031as_pdata.chs_info[4].keycode_far = KEY_SAR3_FAR;
    /*A06 code for AL7160A-32 by huofudong at 20240401 start*/
    snprintf(hx9031as_pdata.chs_info[1].name,sizeof(hx9031as_pdata.chs_info[1].name),"grip_sensor_sub");
    snprintf(hx9031as_pdata.chs_info[3].name,sizeof(hx9031as_pdata.chs_info[3].name),"grip_sensor_wifi");
    snprintf(hx9031as_pdata.chs_info[4].name,sizeof(hx9031as_pdata.chs_info[4].name),"grip_sensor");
    /*A06 code for AL7160A-32 by huofudong at 20240401 end*/
    hx9031as_pdata.chs_info[0].used = false;
    hx9031as_pdata.chs_info[2].used = false;
    /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
    for (iii = 0; iii < HX9031AS_CH_NUM; iii++) {
        if (hx9031as_pdata.chs_info[iii].used != true) {
            continue;
        }

        hx9031as_pdata.chs_info[iii].input_key = input_allocate_device();
        if (NULL == hx9031as_pdata.chs_info[iii].input_key) {
        PRINT_ERR("input_allocate_device failed\n");
        ret = -ENOMEM;
        goto failed_input_allocate_device;
        }
        hx9031as_pdata.chs_info[iii].input_key->name = hx9031as_pdata.chs_info[iii].name;
        __set_bit(EV_KEY, hx9031as_pdata.chs_info[iii].input_key->evbit);
        __set_bit(hx9031as_pdata.chs_info[iii].keycode_close, hx9031as_pdata.chs_info[iii].input_key->keybit);
        __set_bit(hx9031as_pdata.chs_info[iii].keycode_far, hx9031as_pdata.chs_info[iii].input_key->keybit);
        ret = input_register_device(hx9031as_pdata.chs_info[iii].input_key);
        if(ret) {
            PRINT_ERR("input_register_device failed\n");
            goto failed_input_register_device;
        }
    }

    PRINT_INF("input init success\n");
    return ret;

failed_input_register_device:
    for(jj = iii - 1; jj >= 0; jj--) {
        input_free_device(hx9031as_pdata.chs_info[jj].input_key);
    }
    PRINT_ERR("failed_input_register_device\n");
    return ret;
failed_input_allocate_device:
    devm_kfree(&client->dev, hx9031as_pdata.chs_info);
failed_devm_kzalloc:
    PRINT_ERR("hx9031as_input_init_key failed\n");
    return ret;
}

static void hx9031as_input_deinit_key(struct i2c_client *client)
{
    int ii = 0;
    ENTER;
    for (ii = 0; ii < 3; ii++) {
    input_unregister_device(hx9031as_pdata.chs_info[ii].input_key);
    input_free_device(hx9031as_pdata.chs_info[ii].input_key);
    }
    devm_kfree(&client->dev, hx9031as_pdata.chs_info);
}
/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 end*/
#else
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
static int hx9031as_input_init_abs(struct i2c_client *client)
{
    int ii = 0;
    int jj = 0;
    int iii = 0;
    int ret = -1;

    hx9031as_pdata.chs_info = devm_kzalloc(&client->dev,
                                           sizeof(struct hx9031as_channel_info) * HX9031AS_CH_NUM,
                                           GFP_KERNEL);
    if (NULL == hx9031as_pdata.chs_info) {
        PRINT_ERR("devm_kzalloc failed\n");
        ret = -ENOMEM;
        goto failed_devm_kzalloc;
    }

    for (iii = 0; iii < HX9031AS_CH_NUM; iii++) {
        hx9031as_pdata.chs_info[iii].used = false;
        hx9031as_pdata.chs_info[iii].enabled = false;
        if ((hx9031as_pdata.channel_used_flag >> iii) & 0x1) {
            hx9031as_pdata.chs_info[iii].used = true;
            hx9031as_pdata.chs_info[iii].state = IDLE;
        }
    }
    /*A06 code for AL7160A-417 jiashixian at 20240506 start*/
    snprintf(hx9031as_pdata.chs_info[1].name,sizeof(hx9031as_pdata.chs_info[1].name),"grip_sensor_sub");
    snprintf(hx9031as_pdata.chs_info[3].name,sizeof(hx9031as_pdata.chs_info[3].name),"grip_sensor_wifi");
    snprintf(hx9031as_pdata.chs_info[4].name,sizeof(hx9031as_pdata.chs_info[4].name),"grip_sensor");
    hx9031as_pdata.chs_info[1].phase_status = 0x01;
    hx9031as_pdata.chs_info[3].phase_status = 0x02;
    hx9031as_pdata.chs_info[4].phase_status = 0x04;
    hx9031as_pdata.chs_info[0].used = false;
    hx9031as_pdata.chs_info[2].used = false;
    /*A06 code for AL7160A-417 jiashixian at 20240506 end*/
    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if (hx9031as_pdata.chs_info[ii].used != true) {
            continue;
        }

        hx9031as_pdata.chs_info[ii].input_dev_abs = input_allocate_device();
        if (NULL == hx9031as_pdata.chs_info[ii].input_dev_abs) {
            PRINT_ERR("input_allocate_device failed, ii=%d\n", ii);
            ret = -ENOMEM;
            goto failed_input_allocate_device;
        }

        hx9031as_pdata.chs_info[ii].input_dev_abs->name = hx9031as_pdata.chs_info[ii].name;
        input_set_capability(hx9031as_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_X);
        input_set_capability(hx9031as_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_MISC);
        /*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
        input_set_capability(hx9031as_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_MAX);
        /*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 end*/
        ret = input_register_device(hx9031as_pdata.chs_info[ii].input_dev_abs);
        if (ret) {
            PRINT_ERR("input_register_device failed, ii=%d\n", ii);
            goto failed_input_register_device;
        }
    }

    PRINT_INF("input init success\n");
    return ret;

failed_input_register_device:
    for(jj = ii - 1; jj >= 0; jj--) {
        input_unregister_device(hx9031as_pdata.chs_info[jj].input_dev_abs);
    }
    ii++;
failed_input_allocate_device:
    for(jj = ii - 1; jj >= 0; jj--) {
        input_free_device(hx9031as_pdata.chs_info[jj].input_dev_abs);
    }
    devm_kfree(&client->dev, hx9031as_pdata.chs_info);
failed_devm_kzalloc:
    PRINT_ERR("hx9031as_input_init_abs failed\n");
    return ret;
}
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
static void hx9031as_input_deinit_abs(struct i2c_client *client)
{
    int ii = 0;

    ENTER;
    for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        input_unregister_device(hx9031as_pdata.chs_info[ii].input_dev_abs);
        input_free_device(hx9031as_pdata.chs_info[ii].input_dev_abs);
    }
    devm_kfree(&client->dev, hx9031as_pdata.chs_info);
}
#endif

static int hx9031as_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ii = 0;
    int ret = 0;

    PRINT_INF("i2c address:0x%02X\n", client->addr);
    if (!i2c_check_functionality(to_i2c_adapter(client->dev.parent), I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        PRINT_ERR("i2c_check_functionality failed\n");
        ret = -EIO;
        goto failed_i2c_check_functionality;
    }
    i2c_set_clientdata(client, &hx9031as_pdata);
    hx9031as_i2c_client = client;
    hx9031as_pdata.pdev = &client->dev;
    client->dev.platform_data = &hx9031as_pdata;

//{begin ============================Configure DTS properties and implement power on related content
    ret = hx9031as_parse_dt(&client->dev);//yasin: power, irq, regs
    if (ret) {
        PRINT_ERR("hx9031as_parse_dt failed\n");
        ret = -ENODEV;
        goto failed_parse_dt;
    }

    ret = hx9031as_gpio_init();
    if (ret) {
        PRINT_ERR("hx9031as_gpio_init failed\n");
        ret = -1;
        goto failed_gpio_init;
    }

    client->irq = hx9031as_pdata.irq;
    hx9031as_power_on(1);
//}end =============================================================

    ret = hx9031as_id_check();
    if(0 != ret) {
        PRINT_INF("hx9031as_id_check failed, retry\n");
        if(0x28 == client->addr)
            client->addr = 0x2A;
        else
            client->addr = 0x28;
        PRINT_INF("i2c address:0x%02X\n", client->addr);
        ret = hx9031as_id_check();
        if(0 != ret) {
            PRINT_ERR("hx9031as_id_check failed\n");
            goto failed_id_check;
        }
    }

    hx9031as_reg_init();//Register initialization, chip_select can be used for chip differentiation
    hx9031as_ch_cfg();//Channel configuration
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        hx9031as_set_thres_near(ii, hx9031as_ch_thres[ii].thr_near);
        hx9031as_set_thres_far(ii, hx9031as_ch_thres[ii].thr_far);
    }

    INIT_DELAYED_WORK(&hx9031as_pdata.polling_work, hx9031as_polling_work_func);
#ifdef CONFIG_PM_WAKELOCKS
    hx9031as_wake_lock = wakeup_source_register(&client->dev, "hx9031as_wakelock");
#else
    wake_lock_init(&hx9031as_wake_lock, WAKE_LOCK_SUSPEND, "hx9031as_wakelock");
#endif
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
#ifdef HX9031AS_FACTORY_NODE
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
    ret = hx9031as_input_init_key(client);
    if(0 != ret) {
        PRINT_ERR("hx9031as_input_init_key failed\n");
        goto failed_input_init;
    }
#else
    ret = hx9031as_input_init_abs(client);
    if(0 != ret) {
        PRINT_ERR("hx9031as_input_init_abs failed\n");
        goto failed_input_init;
    }
#endif

    ret = class_register(&hx9031as_class);//debug fs path:/sys/class/hx9031as/*
    if (ret < 0) {
        PRINT_ERR("class_register failed\n");
        goto failed_class_register;
    }

    ret = request_threaded_irq(hx9031as_pdata.irq, NULL, hx9031as_irq_handler,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                               hx9031as_pdata.pdev->driver->name, (&hx9031as_pdata));
    if(ret < 0) {
        PRINT_ERR("request_irq failed irq=%d ret=%d\n", hx9031as_pdata.irq, ret);
        goto failed_request_irq;
    }
    enable_irq_wake(hx9031as_pdata.irq);//enable irq wakeup PM
    hx9031as_irq_en_flag = 1;//irq is enabled after request by default

#if HX9031AS_TEST_CHS_EN //enable channels for test
    PRINT_INF("enable all chs for test\n");
    for(ii = 0; ii < HX9031AS_CH_NUM; ii++) {
        if ((hx9031as_pdata.channel_used_flag >> ii) & 0x1) {
            hx9031as_ch_en_hal(ii, 1);
        }
    }
#endif
    /*A06 code for SR-AL7160A-01-185 | SR-AL7160A-01-183 by huofudong at 20240409 start*/
    /*A06 code for SR-AL7160A-01-190 by jiashixian at 20240430 start*/
    g_self = &hx9031as_pdata;
    #ifdef HX9031AS_FACTORY_NODE
    sar_drv.sar_name_drv = "hx9031as";
    sar_drv.get_cali = hx_factory_cali;
    sar_drv.set_enable = hx_factory_set_enable;
    sar_drv.get_enable = hx_factory_get_enable;
    PRINT_INF("factory node create done.");
    #endif
    g_self->channel_status = 0;
    g_usernode.sar_name_drv = "hx9031as";
    g_usernode.set_enable = hx9031_set_enable;
    g_usernode.get_enable = hx9031_get_enable;
    g_usernode.set_onoff = hx9031_set_onoff;
    g_usernode.get_onoff = hx9031_get_onoff;
    g_usernode.set_dumpinfo = hx9031_set_dumpinfo;
    g_usernode.get_dumpinfo = hx9031_get_dumpinfo;
    g_usernode.get_cali = hx9031_user_cali;
    #ifdef HX9031AS_USER_NODE
    g_usernode.set_receiver_turnon = tyhx_receiver_turnon;
    #endif
    PRINT_INF("user node create done.");
    /*A06 code for SR-AL7160A-01-190 by jiashixian at 20240430 end*/
    /*A06 code for SR-AL7160A-01-185 | SR-AL7160A-01-183 by huofudong at 20240409 end*/

    /*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
    #ifdef HX9031AS_USER_NODE
        for (ii = 0; ii < HX9031AS_CH_NUM; ii++) {
         if (false == hx9031as_pdata.chs_info[ii].used) {
             PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031as_pdata.chs_info[ii].name);
             continue;
         }
    input_set_drvdata(hx9031as_pdata.chs_info[ii].input_dev_abs, g_self);
    g_self->channel_status = 0;

    if (!strcmp(hx9031as_pdata.chs_info[ii].input_dev_abs->name, "grip_sensor")) {
        ret = sensors_register(&g_self->factory_device, g_self, sensor_attrs,hx9031as_pdata.chs_info[ii].name);
        if (ret) {
            PRINT_ERR("cound not register main sensor(%d).", ret);
            return -ENOMEM;
        }

        ret = sysfs_create_group(&hx9031as_pdata.chs_info[ii].input_dev_abs->dev.kobj, &ty_tyhx_attributes_group);
        if (ret) {
            PRINT_ERR("cound not create input main group");
            return -ENOMEM;
        }
    }

    if (!strcmp(hx9031as_pdata.chs_info[ii].input_dev_abs->name, "grip_sensor_sub")) {
        ret = sensors_register(&g_self->factory_device_sub, g_self, sensor_attrs,hx9031as_pdata.chs_info[ii].name);
        if (ret) {
            PRINT_ERR("cound not register sub sensor(%d).", ret);
            return -ENOMEM;
         }

          ret = sysfs_create_group(&hx9031as_pdata.chs_info[ii].input_dev_abs->dev.kobj, &ty_tyhx_attributes_group);
         if (ret) {
             PRINT_ERR("cound not create input sub group");
             return -ENOMEM;
         }
    }

    if (!strcmp(hx9031as_pdata.chs_info[ii].input_dev_abs->name, "grip_sensor_wifi")) {
        ret = sensors_register(&g_self->factory_device_wifi, g_self, sensor_attrs,hx9031as_pdata.chs_info[ii].name);
        if (ret) {
            PRINT_ERR("cound not register wifi sensor(%d).", ret);
            return -ENOMEM;
        }

        ret = sysfs_create_group(&hx9031as_pdata.chs_info[ii].input_dev_abs->dev.kobj, &ty_tyhx_attributes_group);
        if (ret) {
            PRINT_ERR("cound not create input wifi group");
            return -ENOMEM;
        }
    }
    }
    #endif
    /*A06 code for SR-AL7160A-01-188  by jiashixian at 20240419 end*/

    PRINT_INF("probe success\n");
    return 0;

failed_request_irq:
    class_unregister(&hx9031as_class);
failed_class_register:
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
#ifdef HX9031AS_FACTORY_NODE
    hx9031as_input_deinit_key(client);
#else
    hx9031as_input_deinit_abs(client);
#endif
failed_input_init:
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_unregister(hx9031as_wake_lock);
#else
    wake_lock_destroy(&hx9031as_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9031as_pdata.polling_work));
failed_id_check:
    hx9031as_power_on(0);
    hx9031as_gpio_deinit();
failed_gpio_init:
failed_parse_dt:
failed_i2c_check_functionality:
    PRINT_ERR("probe failed\n");
    return ret;
}

static int hx9031as_remove(struct i2c_client *client)
{
    ENTER;
    free_irq(hx9031as_pdata.irq, &hx9031as_pdata);
    class_unregister(&hx9031as_class);
#ifdef HX9031AS_FACTORY_NODE
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
    hx9031as_input_deinit_key(client);
#else
    hx9031as_input_deinit_abs(client);
#endif
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_unregister(hx9031as_wake_lock);
#else
    wake_lock_destroy(&hx9031as_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9031as_pdata.polling_work));
    hx9031as_power_on(0);
    hx9031as_gpio_deinit();
    return 0;
}

static int hx9031as_suspend(struct device *dev)
{
    ENTER;
    hx9031as_irq_from_suspend_flag = 1;
    return 0;
}

static int hx9031as_resume(struct device *dev)
{
    ENTER;
    hx9031as_irq_from_suspend_flag = 0;
    return 0;
}

static struct i2c_device_id hx9031as_i2c_id_table[] = {
    { HX9031AS_DRIVER_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, hx9031as_i2c_id_table);
#ifdef CONFIG_OF
static struct of_device_id hx9031as_of_match_table[] = {
#if HX9031AS_TEST_ON_MTK_DEMO_XY6761
    {.compatible = "mediatek,sar_hx9031as"},
#else
    {.compatible = "tyhx,hx9031as"},
#endif
    { },
};
#else
#define hx9031as_of_match_table NULL
#endif

static const struct dev_pm_ops hx9031as_pm_ops = {
    .suspend = hx9031as_suspend,
    .resume = hx9031as_resume,
};

static struct i2c_driver hx9031as_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = HX9031AS_DRIVER_NAME,
        .of_match_table = hx9031as_of_match_table,
        .pm = &hx9031as_pm_ops,
    },
    .id_table = hx9031as_i2c_id_table,
    .probe = hx9031as_probe,
    .remove = hx9031as_remove,
};

static int __init hx9031as_module_init(void)
{
    ENTER;
    PRINT_INF("driver version:%s\n", HX9031AS_DRIVER_VER);
    return i2c_add_driver(&hx9031as_i2c_driver);
}

static void __exit hx9031as_module_exit(void)
{
    ENTER;
    i2c_del_driver(&hx9031as_i2c_driver);
}

module_init(hx9031as_module_init);
module_exit(hx9031as_module_exit);

MODULE_AUTHOR("Yasin Lee <yasin.lee.x@gmail.com><yasin.lee@tianyihexin.com>");
MODULE_DESCRIPTION("Driver for NanJingTianYiHeXin HX9031AS & HX9023S Cap Sensor");
MODULE_ALIAS("sar driver");
MODULE_LICENSE("GPL");
