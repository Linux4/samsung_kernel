/*************************************************************
*                                                            *
*  Driver for NanJingTianYiHeXin HX9031A & HX9023E Cap Sense *
*                                                            *
*************************************************************/

#define HX9031A_TEST_ON_MTK_DEMO 0
#define HX9031A_HW_MONITOR_EN 0
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#define HX9031A_ALG_COMPILE_EN 1
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
#define HX9031A_DRIVER_VER "Change-Id d67aaf-Temp20221016"
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
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
//#include <linux/wakelock.h>
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
#include <linux/sensors.h>
#ifdef CONFIG_PM_WAKELOCKS
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h>
#endif
#include "hx9031a.h"
#include <linux/fs.h>
#include <linux/notifier.h>
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
#include "../sensor_user_node.h"
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/
/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 start*/
#define SDCARD_INSERT_EVENT 0x1U
RAW_NOTIFIER_HEAD(sdcard_hx9031_notify);
/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 end*/
/*Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/6 start*/
extern char *sar_name;
/*Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/6 end*/
static struct i2c_client *hx9031a_i2c_client = NULL;
static struct hx9031a_platform_data hx9031a_pdata;
/*Tab A8 code for SR-AX6300-01-82 by mayuhang at 2021/9/15 start*/
static uint8_t hx9031a_monitor_addr = RA_PRF_CFG_0x02;
static uint8_t hx9031a_monitor_val = 0;
static uint8_t ch_enable_status = 0x00;
static uint8_t hx9031a_polling_enable = 0;
static int hx9031a_polling_period_ms = 0;

static struct workqueue_struct* sar_work;
static struct work_struct sar_enable_work;

static int16_t data_raw[HX9031A_CH_NUM] = {0};
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
static int32_t data_diff[HX9031A_CH_NUM] = {0};
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
static int16_t data_lp[HX9031A_CH_NUM] = {0};
static int16_t data_bl[HX9031A_CH_NUM] = {0};
static uint16_t data_offset_dac[HX9031A_CH_NUM] = {0};
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
static struct hx9031a_near_far_threshold hx9031a_ch_thres[HX9031A_CH_NUM] = {
    {.thr_near = 288, .thr_far = 224}, //ch0
    {.thr_near = 160, .thr_far = 128},
    {.thr_near = 128, .thr_far = 128},
    {.thr_near = 32767, .thr_far = 32767},
};
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
static uint8_t hx9031a_ch_near_state[HX9031A_CH_NUM] = {0};
static volatile uint8_t hx9031a_irq_from_suspend_flag = 0;
static volatile uint8_t hx9031a_irq_en_flag = 1;
static volatile uint8_t hx9031a_hw_monitor_en_flag = HX9031A_HW_MONITOR_EN;
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
static volatile uint8_t hx9031a_output_switch = HX9031A_OUTPUT_LP_BL;
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
static DEFINE_MUTEX(hx9031a_i2c_rw_mutex);
static DEFINE_MUTEX(hx9031a_ch_en_mutex);
#ifdef CONFIG_PM_WAKELOCKS
static struct wakeup_source hx9031a_wake_lock;
#else
static struct wake_lock hx9031a_wake_lock;
#endif
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
static int anfr_status = 1;
static int irq_count = 0;
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
//true = on; false = off;
static bool g_onoff_flag = true;
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/

/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
static volatile uint8_t hx9031a_alg_ref_ch_drdy_int_en_flag = 0;
static volatile uint8_t hx9031a_alg_dynamic_threshold_en = 0;
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
static int16_t hx9031a_alg_ch_thres_ini[HX9031A_CH_NUM] = {
    0, 0, 0, 0
};
static int16_t hx9031a_alg_ch_thres_drift[HX9031A_CH_NUM] = {
    0, 0, 0, 0
};
static struct hx9031a_near_far_threshold hx9031a_alg_ch_thres_backup[HX9031A_CH_NUM] = {
    {.thr_near = 0, .thr_far = 0}, //ch0
    {.thr_near = 0, .thr_far = 0},
    {.thr_near = 0, .thr_far = 0},
    {.thr_near = 0, .thr_far = 0},
};

//#define INT16_MAX = 32767           //#define INT16_MIN = -32768
static int16_t hx9031a_alg_ch_thres_drift_min[4] = {2000, -8000, -32768, 2000};
static int16_t hx9031a_alg_ch_thres_drift_max[4] = {32767, -2000, -8000, 32767};
static int16_t hx9031a_alg_ch_thres_drift_offset[4] = {700, 600, 4000, 500};

static int hx9031a_alg_ref_ch_drdy_int_en(uint8_t ch_id, uint8_t en);
static int hx9031a_alg_dynamic_threshold_init(uint8_t ref_ch_id);
static int hx9031a_alg_dynamic_threshold_adjust(uint8_t ref_ch_id);
#endif
static int linux_common_i2c_write(struct i2c_client *client, uint8_t *reg_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[HX9031A_MAX_XFER_SIZE + 1] = {0};
    struct i2c_msg msg[1];

    if(count > HX9031A_MAX_XFER_SIZE) {
        count = HX9031A_MAX_XFER_SIZE;
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

static int linux_common_i2c_read(struct i2c_client *client, uint8_t *reg_addr, uint8_t *rxbuf, int count)
{
    int ret = -1;
    int ii = 0;
    struct i2c_msg msg[2];

    if(count > HX9031A_MAX_XFER_SIZE) {
        count = HX9031A_MAX_XFER_SIZE;
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
static int hx9031a_read(uint8_t addr, uint8_t *rxbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9031a_i2c_rw_mutex);
    ret = linux_common_i2c_read(hx9031a_i2c_client, &addr, rxbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_read failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9031a_i2c_rw_mutex);
    return ret;
}

//return 0 for success, return -1 for errors.
static int hx9031a_write(uint8_t addr, uint8_t *txbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9031a_i2c_rw_mutex);
    ret = linux_common_i2c_write(hx9031a_i2c_client, &addr, txbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9031a_i2c_rw_mutex);
    return ret;
}


static void hx9031a_disable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(1 == hx9031a_irq_en_flag) {
        disable_irq_nosync(hx9031a_pdata.irq);
        hx9031a_irq_en_flag = 0;
        PRINT_DBG("irq_%d is disabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is disabled already!\n", irq);
    }
}

static void hx9031a_enable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(0 == hx9031a_irq_en_flag) {
        enable_irq(hx9031a_pdata.irq);
        hx9031a_irq_en_flag = 1;
        PRINT_DBG("irq_%d is enabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is enabled already!\n", irq);
    }
}

static void hx9031a_data_lock(uint8_t lock_flag)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};

    if(HX9031A_DATA_LOCK == lock_flag) {
        ret = hx9031a_read(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_read failed\n");
        }

        //rx_buf[0] = rx_buf[0] | 0x10;
        rx_buf[0] = 0x10;

        ret = hx9031a_write(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
        }
    } else if(HX9031A_DATA_UNLOCK == lock_flag) {
        ret = hx9031a_read(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_read failed\n");
        }

        //rx_buf[0] = rx_buf[0] & 0xEF;
        rx_buf[0] = 0x00;

        ret = hx9031a_write(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
        }
    } else {
        PRINT_ERR("ERROR!!! hx9031a_data_lock wrong para. now do data unlock!\n");
        ret = hx9031a_read(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_read failed\n");
        }

        //rx_buf[0] = rx_buf[0] & 0xEF;
        rx_buf[0] = 0x00;

        ret = hx9031a_write(RA_DSP_CONFIG_CTRL1_0xc8, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
        }
    }
}

static int8_t hx9031a_id_check(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t device_id = 0;
    uint8_t rxbuf[1] = {0};
    for(ii = 0; ii < HX9031A_ID_CHECK_COUNT; ii++) {
        ret = hx9031a_read(RA_ID_REG_0x60, rxbuf, 1);
        if(ret < 0) {
            PRINT_ERR("i2c read error\n");
            continue;
        }
        device_id = rxbuf[0];
        if((HX90XX_CHIP_ID_9031A == device_id))
            break;
        else
            continue;
    }

    if(HX90XX_CHIP_ID_9031A == device_id) {
        PRINT_ERR("success! device_id=0x%02X(HX9023E or HX9031A)\n", device_id);
        return device_id;
    } else {
        PRINT_ERR("failed! device_id=0x%02X(UNKNOW_CHIP_ID)\n", device_id);
        return -1;
    }
}

#define BOARD_INFO "androidboot.pcbainfo="
#define BOARD_WIFI "WIOL_6301AA"
bool Is_Wifi_Version()
{
    char* str = NULL;
    char board_string[12] = {0};
    int ret = 0;
    printk(KERN_ERR "%s\n",saved_command_line);
    str = strstr(saved_command_line,BOARD_INFO);
    if(str == NULL)
    {
        printk(KERN_ERR "strstr func error\n");
        return false;
    }
    ret = sscanf(str,BOARD_INFO"%s",board_string);
    if(ret == -1)
    {
        printk(KERN_ERR "sscanf func error\n");
        return false;
    }
    printk(KERN_ERR "Board_info:%s\n",board_string);
    if(!strncmp(board_string,BOARD_WIFI,11))
        return true;
    return false;
}


uint8_t hx9031a_get_board_info(void)
{
    if(Is_Wifi_Version())
    {
        printk(KERN_ERR "Board_Info Is Wifi Version\n");
        printk(KERN_ERR "Return HX9023E_ON_BOARD\n");
        return HX9023E_ON_BOARD;
    }
    printk(KERN_ERR "Board_Info Is LTE Version\n");
    printk(KERN_ERR "Return HX9031A_ON_BOARD\n");

    return HX9031A_ON_BOARD;
}

void hx9031a_ch_cfg(uint8_t chip_select)
{
    int ret = -1;
    int ii = 0;
    uint16_t ch_cfg = 0;
    uint8_t cfg[HX9031A_CH_NUM * 2] = {0};

    uint8_t CS0 = 0;
    uint8_t CS1 = 0;
    uint8_t CS2 = 0;
    uint8_t CS3 = 0;
    uint8_t CS4 = 0;
    uint8_t NA = 0;
    uint8_t CH0_POS = 0; //ANT1
    uint8_t CH0_NEG = 0;
    uint8_t CH1_POS = 0; //cs3 : cs1's ref
    uint8_t CH1_NEG = 0;
    uint8_t CH2_POS = 0; //cs1 : signal line
    uint8_t CH2_NEG = 0;
    uint8_t CH3_POS = 0;
    uint8_t CH3_NEG = 0;

    ENTER;


    if(HX9023E_ON_BOARD == chip_select) {
        CS0 = 0;
        CS1 = 1;
        CS2 = 2;
        CS3 = 3;
        CS4 = 4;
        NA = 10;
        PRINT_INF("HX9023E_ON_BOARD\n");
    } else if(HX9031A_ON_BOARD == chip_select) {
        CS0 = 2;
        CS1 = 1;
        CS2 = 3;
        CS3 = 0;
        CS4 = 4;
        NA = 10;
        PRINT_INF("HX9031A_ON_BOARD\n");
    } else {
        PRINT_ERR("unknow chip on board\n");
    }


    if(HX9023E_ON_BOARD == chip_select) {
        CH0_POS = NA;//CS1 MLB_ANT
        CH0_NEG = NA;//CS0 MLB_REF
        CH1_POS = NA;//CS2 HB_ANT
        CH1_NEG = NA;
        CH2_POS = CS1;
        CH2_NEG = CS0;
        CH3_POS = CS0;
        CH3_NEG = NA;
    } else if(HX9031A_ON_BOARD == chip_select) {
        CH0_POS = CS1;//CS1 MLB_ANT
        CH0_NEG = CS0;//CS0 MLB_REF
        CH1_POS = CS2;//CS2 HB_ANT
        CH1_NEG = CS4;
        CH2_POS = CS3;//CS3 WIFI_ANT
        CH2_NEG = CS4;//CS4 WIFI_REF
        CH3_POS = CS4;
        CH3_NEG = NA;
#if HX9031A_TEST_ON_MTK_DEMO
        CH0_POS = CS0;
        CH0_NEG = NA;
        CH1_POS = CS1;
        CH1_NEG = NA;
        CH2_POS = CS2;
        CH2_NEG = NA;
        CH3_POS = CS4;
        CH3_NEG = NA;
#endif
    } else {
        PRINT_ERR("unknow chip on board\n");
    }

    ch_cfg = (uint16_t)((0x03 << (CH0_POS * 2)) + (0x02 << (CH0_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH1_POS * 2)) + (0x02 << (CH1_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH2_POS * 2)) + (0x02 << (CH2_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH3_POS * 2)) + (0x02 << (CH3_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ret = hx9031a_write(RA_CH0_CFG_7_0_0x03, cfg, HX9031A_CH_NUM * 2);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }
}

static void hx9031a_register_init(void)
{
    int ii = 0;
    int ret = -1;

    while(ii < ARRAY_SIZE(hx9031a_reg_init_list)) {
        //PRINT_DBG("WRITE:Reg0x%02X=0x%02X\n", hx9031a_reg_init_list[ii].addr, hx9031a_reg_init_list[ii].val);
        ret = hx9031a_write(hx9031a_reg_init_list[ii].addr, &hx9031a_reg_init_list[ii].val, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
        }

        if(hx9031a_monitor_addr == hx9031a_reg_init_list[ii].addr)
            hx9031a_monitor_val = hx9031a_reg_init_list[ii].val;

        ii++;
    }
}

static void hx9031a_read_offset_dac(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9031A_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    uint32_t data[HX9031A_CH_NUM] = {0};

    hx9031a_data_lock(HX9031A_DATA_LOCK);
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9031A_CH_NUM * bytes_per_channel;
    ret = hx9031a_read(RA_OFFSET_DAC0_7_0_0x15, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0xFFF;//12位
            data_offset_dac[ii] = data[ii];
        }
    }
    hx9031a_data_lock(HX9031A_DATA_UNLOCK);

    PRINT_DBG("OFFSET_DAC: %8d, %8d, %8d, %8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3]);
}

static void hx9031a_manual_offset_calibration(uint8_t ch_id)
{
    int ret = 0;
    uint8_t buf[1] = {0};
    PRINT_ERR("hx9031a_calibration in\n");
    ENTER;
    ret = hx9031a_read(RA_OFFSET_CALI_CTRL_0xc2, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    buf[0] |= (1 << (ch_id + 4));
    ret = hx9031a_write(RA_OFFSET_CALI_CTRL_0xc2, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }
}

#if 0
static void hx9031a_int_clear(void)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    ENTER;
    ret = hx9031a_read(RA_GLOBAL_CTRL0_0x00, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    buf[0] |= 0x10;
    ret = hx9031a_write(RA_GLOBAL_CTRL0_0x00, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }
}
#endif

static uint16_t hx9031a_get_thres_near(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[2] = {0};

    ret = hx9031a_read(RA_PROX_HIGH_DIFF_CFG_CH0_0_0x80 + (ch * CH_DATA_2BYTES), buf, 2);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    hx9031a_ch_thres[ch].thr_near = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;
    return hx9031a_ch_thres[ch].thr_near;
}
static uint16_t hx9031a_get_thres_far(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[2] = {0};

    ret = hx9031a_read(RA_PROX_LOW_DIFF_CFG_CH0_0_0x88 + (ch * CH_DATA_2BYTES), buf, 2);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    hx9031a_ch_thres[ch].thr_far = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;
    return hx9031a_ch_thres[ch].thr_far;
}

static uint16_t hx9031a_set_thres_near(uint8_t ch, uint16_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9031a_ch_thres[ch].thr_near = (val & 0x03FF) * 32;
    ret = hx9031a_write(RA_PROX_HIGH_DIFF_CFG_CH0_0_0x80 + (ch * CH_DATA_2BYTES), buf, 2);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }

    PRINT_INF("hx9031a_ch_thres[%d].thr_near=%d\n", ch, hx9031a_ch_thres[ch].thr_near);
    return hx9031a_ch_thres[ch].thr_near;
}

static uint16_t hx9031a_set_thres_far(uint8_t ch, uint16_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9031a_ch_thres[ch].thr_far = (val & 0x03FF) * 32;
    ret = hx9031a_write(RA_PROX_LOW_DIFF_CFG_CH0_0_0x88 + (ch * CH_DATA_2BYTES), buf, 2);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }

    PRINT_INF("hx9031a_ch_thres[%d].thr_far=%d\n", ch, hx9031a_ch_thres[ch].thr_far);
    return hx9031a_ch_thres[ch].thr_far;
}

/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
static void hx9031a_get_prox_state(int32_t *data_compare, uint8_t ch_num)
{
    int ret = -1;
    uint8_t ii = 0;
    int16_t near = 0;
    int16_t far = 0;
    uint8_t buf[1] = {0};

    hx9031a_pdata.prox_state_reg = 0;
    hx9031a_pdata.prox_state_cmp = 0;
    for(ii = 0; ii < ch_num; ii++) {
        hx9031a_ch_near_state[ii] = 0;
    }

    ret = hx9031a_read(RA_PROX_STATUS_0x6b, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }
    hx9031a_pdata.prox_state_reg = buf[0];

    for(ii = 0; ii < ch_num; ii++) {
        near = hx9031a_get_thres_near(ii);
        far = hx9031a_get_thres_far(ii);
        if(data_compare[ii] >= near) {
            hx9031a_ch_near_state[ii] = 1;//near
            hx9031a_pdata.prox_state_cmp |= (1 << ii);
        } else if(data_compare[ii] <= far) {
            hx9031a_ch_near_state[ii] = 0;//far
        }
    }

    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        if((0 == (hx9031a_pdata.prox_state_reg_pre >> ii & 0x1)) && (1 == (hx9031a_pdata.prox_state_reg >> ii & 0x1))) {
            hx9031a_pdata.prox_state_far_to_near_flag |= (1 << ii);
        } else {
            hx9031a_pdata.prox_state_far_to_near_flag &= ~(1 << ii);
        }
    }
    hx9031a_pdata.prox_state_reg_pre = hx9031a_pdata.prox_state_reg;
    hx9031a_pdata.prox_state_cmp_pre = hx9031a_pdata.prox_state_cmp;

    PRINT_INF("prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9031a_ch_near_state:%d-%d-%d-%d, far_to_near_flag=0x%02X\n",
              hx9031a_pdata.prox_state_reg,
              hx9031a_pdata.prox_state_cmp,
              hx9031a_ch_near_state[3],
              hx9031a_ch_near_state[2],
              hx9031a_ch_near_state[1],
              hx9031a_ch_near_state[0],
              hx9031a_pdata.prox_state_far_to_near_flag);
}
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/


static void hx9031a_data_switch(uint8_t output_switch)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    //7:4 raw(1:bl 0:raw) 3:0 diff(1:diff 0:lp)
    if(HX9031A_OUTPUT_RAW_DIFF == output_switch) { //raw & diff
        hx9031a_output_switch = HX9031A_OUTPUT_RAW_DIFF;
        buf[0] = 0x0F;
        PRINT_INF("output data: raw & diff\n");
    } else { //lp & bl
        hx9031a_output_switch = HX9031A_OUTPUT_LP_BL;
        buf[0] = 0xF0;
        PRINT_INF("output data: lp & bl\n");
    }

    ret = hx9031a_write(RA_RAW_BL_RD_CFG_0x38, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }
}

static void hx9031a_sample(void)
{
    int ret = -1;
    uint8_t ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9031A_CH_NUM * CH_DATA_BYTES_MAX] = {0};

    uint16_t data[HX9031A_CH_NUM] = {0};

    hx9031a_data_lock(HX9031A_DATA_LOCK);
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9031A_CH_NUM * bytes_per_channel;
    ret = hx9031a_read(RA_RAW_BL_CH0_0_0xe8, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));
            if(HX9031A_OUTPUT_RAW_DIFF == hx9031a_output_switch) {
                data_raw[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii];
                data_bl[ii] = 0;
            } else {
                data_raw[ii] = 0;
                data_bl[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii];
            }
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9031A_CH_NUM * bytes_per_channel;
    ret = hx9031a_read(RA_LP_DIFF_CH0_0_0xf4, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));
            if(HX9031A_OUTPUT_RAW_DIFF == hx9031a_output_switch) {
                data_diff[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii];
                data_lp[ii] = 0;
            } else {
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
                data_lp[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii];
                data_diff[ii] = data_lp[ii] - data_bl[ii];
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
            }

        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9031A_CH_NUM * bytes_per_channel;
    ret = hx9031a_read(RA_OFFSET_DAC0_7_0_0x15, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0xFFF;//12位
            data_offset_dac[ii] = data[ii];
        }
    }
    //====================================================================================================
    hx9031a_data_lock(HX9031A_DATA_UNLOCK);

    PRINT_DBG("DIFF  : %-8d, %-8d, %-8d, %-8d\n", data_diff[0], data_diff[1], data_diff[2], data_diff[3]);
    PRINT_DBG("RAW   : %-8d, %-8d, %-8d, %-8d\n", data_raw[0], data_raw[1], data_raw[2], data_raw[3]);
    PRINT_DBG("OFFSET: %-8d, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3]);
    PRINT_DBG("BL    : %-8d, %-8d, %-8d, %-8d\n", data_bl[0], data_bl[1], data_bl[2], data_bl[3]);
    PRINT_DBG("LP    : %-8d, %-8d, %-8d, %-8d\n", data_lp[0], data_lp[1], data_lp[2], data_lp[3]);
}

/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
void self_cali(void)
{
    int i = 0;
    if (anfr_status == SELF_CALI_NUM) {
        for (i = 0; i < HX9031A_CH_NUM; i++) {
            if (data_bl[i] < SELF_CALI_BL_0x04) {
                anfr_status = 0;
                break;
            }
        }
    }
    PRINT_ERR("lc_anfr_status = %d,irq_count = %d,data_bl[%d] = %d\n", anfr_status, irq_count, i, data_bl[i]);
    if (irq_count >= IRQ_EXIT_NUM) {
        anfr_status = 0;
    }
}
/*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 start*/
const int g_anfr_event = 1;
const int g_noanfr_event = 2;

/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
static void hx9031a_onoff_enablereport(void)
{
    int ii = 0;
    uint8_t touch_state = 0;
    int num_channels = 0;
    struct input_dev *input = NULL;
    struct hx9031a_channel_info *channel_p  = NULL;

    num_channels = ARRAY_SIZE(hx9031a_channels);

    if (unlikely(NULL == hx9031a_channels)) {
        PRINT_ERR("hx9031a_channels==NULL!!!\n");
        return;
    }
    if (!g_onoff_flag) {
        PRINT_ERR("%s:hx9031 onoff sar off\n", __func__);
        return ;
    }
    if (irq_count <= IRQ_EXIT_NUM && anfr_status == 1) {
        PRINT_ERR("lc_self_cali  irq_count = %d\n",irq_count);
        irq_count++;
        self_cali();
    }
    PRINT_ERR("lc_anfr_status = %d\n",anfr_status);

    for (ii = 0; ii < num_channels; ii++) {
        if (false == hx9031a_channels[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031a_channels[ii].name);
            continue;
        }
        if (false == hx9031a_channels[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031a_channels[ii].name);
            continue;
        }
        channel_p = &hx9031a_channels[ii];
        if (NULL == channel_p) {
            PRINT_ERR("ch_%d is NULL!!!\n", ii);
            return;
        }
        input = channel_p->hx9031a_input_dev;
        if (NULL == input) {
            PRINT_ERR("ch_%d input device is NULL!!!\n", ii);
            return;
        }
        if (anfr_status == SELF_CALI_NUM) {
            PRINT_ERR("LC_SELF\n");
            input_report_abs(input, ABS_DISTANCE, 0);
            input_report_rel(input, REL_X, g_anfr_event);
            input_sync(input);
        } else {
            PRINT_ERR("LC_recover\n");
            touch_state = hx9031a_pdata.prox_state_reg >> ii & 0x1;
            if (BODYACTIVE == touch_state) {
                if (channel_p->state == BODYACTIVE)
                    PRINT_DBG("%s already BODYACTIVE, nothing report\n", channel_p->name);
                else {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(1 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(2 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }
                    channel_p->state = BODYACTIVE;
                    PRINT_DBG("%s report BODYACTIVE(5mm)\n", channel_p->name);
                }
            } else if (PROXACTIVE == touch_state) {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(1 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(2 == ii){
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }
                    channel_p->state = PROXACTIVE;
                    PRINT_DBG("%s report PROXACTIVE(15mm)\n", channel_p->name);
            } else if (IDLE == touch_state) {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(1 == ii){
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }else if(2 == ii){
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            input_sync(input);
                    }
                    channel_p->state = IDLE;
                    PRINT_DBG("%s report released\n", channel_p->name);
            } else {
                PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
            }
        }
    }
}
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/

static void hx9031a_input_report(void)
{
    int ii = 0;
    uint8_t touch_state = 0;
    int num_channels = 0;
    struct input_dev *input = NULL;
    struct hx9031a_channel_info *channel_p  = NULL;

    num_channels = ARRAY_SIZE(hx9031a_channels);

    if (unlikely(NULL == hx9031a_channels)) {
        PRINT_ERR("hx9031a_channels==NULL!!!\n");
        return;
    }
    /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
    if (!g_onoff_flag) {
        PRINT_ERR("%s:hx9031 onoff sar off\n", __func__);
        return ;
    }
    /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
    if (irq_count <= IRQ_EXIT_NUM && anfr_status == 1) {
        PRINT_ERR("lc_self_cali  irq_count = %d\n",irq_count);
        irq_count++;
        self_cali();
    }
    PRINT_ERR("lc_anfr_status = %d\n",anfr_status);
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/

    for (ii = 0; ii < num_channels; ii++) {
        if (false == hx9031a_channels[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9031a_channels[ii].name);
            continue;
        }
        if (false == hx9031a_channels[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9031a_channels[ii].name);
            continue;
        }

        channel_p = &hx9031a_channels[ii];
        if (NULL == channel_p) {
            PRINT_ERR("ch_%d is NULL!!!\n", ii);
            return;
        }

        input = channel_p->hx9031a_input_dev;

        if (NULL == input) {
            PRINT_ERR("ch_%d input device is NULL!!!\n", ii);
            return;
        }

        /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
        if (anfr_status == SELF_CALI_NUM) {
            PRINT_ERR("LC_SELF\n");
            input_report_abs(input, ABS_DISTANCE, 0);
            input_report_rel(input, REL_X, g_anfr_event);
            input_sync(input);
        } else {
            PRINT_ERR("LC_recover\n");
            //touch_state = hx9031a_ch_near_state[ii];
            touch_state = hx9031a_pdata.prox_state_reg >> ii & 0x1;
            //touch_state = hx9031a_pdata.prox_state_cmp & (1 << ii);
            /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/9/6 start*/
            if (BODYACTIVE == touch_state) {
                if (channel_p->state == BODYACTIVE)
                    PRINT_DBG("%s already BODYACTIVE, nothing report\n", channel_p->name);
                else {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR1_FAR, 1);
                            input_report_key(input, KEY_SAR1_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }else if(1 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR2_FAR, 1);
                            input_report_key(input, KEY_SAR2_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }else if(2 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR3_FAR, 1);
                            input_report_key(input, KEY_SAR3_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }
                    channel_p->state = BODYACTIVE;
                    PRINT_DBG("%s report BODYACTIVE(5mm)\n", channel_p->name);
                }
            } else if (PROXACTIVE == touch_state) {
                if (channel_p->state == PROXACTIVE)
                    PRINT_DBG("%s already PROXACTIVE, nothing report\n", channel_p->name);
                else {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR1_FAR, 1);
                            input_report_key(input, KEY_SAR1_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);

                    }else if(1 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR2_FAR, 1);
                            input_report_key(input, KEY_SAR2_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }else if(2 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR3_FAR, 1);
                            input_report_key(input, KEY_SAR3_FAR, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 0);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }
                    channel_p->state = PROXACTIVE;
                    PRINT_DBG("%s report PROXACTIVE(15mm)\n", channel_p->name);
                }
            } else if (IDLE == touch_state) {
                if (channel_p->state == IDLE)
                    PRINT_DBG("%s already released, nothing report\n", channel_p->name);
                else {
    #ifdef CONFIG_PM_WAKELOCKS
                    __pm_wakeup_event(&hx9031a_wake_lock, 1000);
    #else
                    wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
    #endif
                    if(0 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR1_CLOSE, 1);
                            input_report_key(input, KEY_SAR1_CLOSE, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }else if(1 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR2_CLOSE, 1);
                            input_report_key(input, KEY_SAR2_CLOSE, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }else if(2 == ii){
                            #ifdef HQ_FACTORY_BUILD
                            input_report_key(input, KEY_SAR3_CLOSE, 1);
                            input_report_key(input, KEY_SAR3_CLOSE, 0);
                            #else
                            input_report_abs(input, ABS_DISTANCE, 5);
                            input_report_rel(input, REL_X, g_noanfr_event);
                            #endif
                            input_sync(input);
                    }
            /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/9/6 end*/
                    channel_p->state = IDLE;
                    PRINT_DBG("%s report released\n", channel_p->name);
                }
            } else {
                PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
            }
        }
        /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
    }
}
/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 end*/

static int hx9031a_parse_dt(struct device *dev)
{
    int ret = -1;
    struct device_node *dt_node = dev->of_node;

    if (NULL == dt_node) {
        PRINT_ERR("No DTS node\n");
        return -ENODEV;
    }

#if HX9031A_TEST_ON_MTK_DEMO
    ret = of_property_read_u32(dt_node, "tyhx,nirq-gpio", &hx9031a_pdata.irq_gpio);
    if(ret < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#else
    hx9031a_pdata.irq_gpio = of_get_named_gpio_flags(dt_node, "tyhx,nirq-gpio", 0, NULL);
    if(hx9031a_pdata.irq_gpio < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#endif

    PRINT_INF("hx9031a_pdata.irq_gpio=%d\n", hx9031a_pdata.irq_gpio);

    hx9031a_pdata.channel_used_flag = 0x0F;
    ret = of_property_read_u32(dt_node, "tyhx,channel-flag", &hx9031a_pdata.channel_used_flag);//客户配置：有效通道标志位：9031A最大传入0x0F，9023E最大传入0x07
    if(ret < 0) {
        PRINT_ERR("\"tyhx,channel-flag\" is not set in DT\n");
        return -1;
    }
    if(hx9031a_pdata.channel_used_flag > ((1 << HX9031A_CH_NUM) - 1)) {
        PRINT_ERR("the max value of channel_used_flag is 0x%X\n", ((1 << HX9031A_CH_NUM) - 1));
        return -1;
    }
    PRINT_INF("hx9031a_pdata.channel_used_flag=0x%X\n", hx9031a_pdata.channel_used_flag);

    return 0;
}

static int hx9031a_gpio_init(void)
{
    int ret = -1;
    if (gpio_is_valid(hx9031a_pdata.irq_gpio)) {
        ret = gpio_request(hx9031a_pdata.irq_gpio, "hx9031a_irq_gpio");
        if (ret < 0) {
            PRINT_ERR("gpio_request failed. ret=%d\n", ret);
            return ret;
        }
        ret = gpio_direction_input(hx9031a_pdata.irq_gpio);
        if (ret < 0) {
            PRINT_ERR("gpio_direction_input failed. ret=%d\n", ret);
            gpio_free(hx9031a_pdata.irq_gpio);
            return ret;
        }
    } else {
        PRINT_ERR("Invalid gpio num\n");
        return -1;
    }

    hx9031a_pdata.irq = gpio_to_irq(hx9031a_pdata.irq_gpio);
    PRINT_INF("hx9031a_pdata.irq_gpio=%d hx9031a_pdata.irq=%d\n", hx9031a_pdata.irq_gpio, hx9031a_pdata.irq);
    return 0;
}

static void hx9031a_gpio_deinit(void)
{
    ENTER;
    gpio_free(hx9031a_pdata.irq_gpio);
}

static void hx9031a_power_on(uint8_t on)
{
    if(on) {
        //TODO: 用户自行填充
        PRINT_INF("power on\n");
    } else {
        //TODO: 用户自行填充
        PRINT_INF("power off\n");
    }
}

#if 0
static int hx9031a_threshold_int_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    ret = hx9031a_read(RA_INTERRUPT_CFG_0x39, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
        return -1;
    }

    if(ch_id >= HX9031A_CH_NUM) {
        PRINT_ERR("channel index over range !!!(ch_id=%d, en=%d)\n", ch_id, en);
        return -1;
    }

    if(0 == en) {
        buf[0] &= ~(1 << (ch_id + 0));
        buf[0] &= ~(1 << (ch_id + 4));
        ret = hx9031a_write(RA_INTERRUPT_CFG_0x39, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_%d threshold int disabled\n", ch_id);
    } else {
        buf[0] |= (1 << (ch_id + 0));
        buf[0] |= (1 << (ch_id + 4));
        ret = hx9031a_write(RA_INTERRUPT_CFG_0x39, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_%d threshold int enabled\n", ch_id);
    }

    return 0;
}
#endif

#if 1
static int hx9031a_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = 0;
    uint8_t tx_buf[1] = {0};

    if(ch_enable_status > 0) {
        if(1 == en) {
            ch_enable_status |= (1 << ch_id);
            //hx9031a_threshold_int_en(ch_id, en);
            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            ch_enable_status &= ~(1 << ch_id);
            //hx9031a_threshold_int_en(ch_id, en);
            if(0 == ch_enable_status) {
                tx_buf[0] = 0x00;
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
                if (1 == hx9031a_alg_dynamic_threshold_en) {
                    hx9031a_alg_dynamic_threshold_init(3);
                }
#endif
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
                cancel_delayed_work_sync(&(hx9031a_pdata.hw_monitor_work));
                PRINT_INF("ch_%d disabled, all channels disabled\n", ch_id);
            } else {
                PRINT_INF("ch_%d disabled\n", ch_id);
            }
        }
    } else {
        if(1 == en) {
            hx9031a_pdata.prox_state_reg = 0;
            hx9031a_pdata.prox_state_cmp = 0;
            hx9031a_pdata.prox_state_reg_pre = 0;
            hx9031a_pdata.prox_state_cmp_pre = 0;
            hx9031a_pdata.prox_state_far_to_near_flag = 0;
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
            if (1 == hx9031a_alg_dynamic_threshold_en) {
                hx9031a_alg_dynamic_threshold_init(3);
                tx_buf[0] = hx9031a_pdata.channel_used_flag;
                tx_buf[0] |= (0x1 << 3); //ch3 enabled
            } else {
                tx_buf[0] = hx9031a_pdata.channel_used_flag;
            }
#else
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
            tx_buf[0] = hx9031a_pdata.channel_used_flag;
#endif
            ret = hx9031a_write(RA_CH_NUM_CFG_0x24, tx_buf, 1);
            if(0 != ret) {
                PRINT_ERR("hx9031a_write failed\n");
                return -1;
            }
            ch_enable_status |= (1 << ch_id);
            //hx9031a_threshold_int_en(ch_id, en);

            if(1 == hx9031a_hw_monitor_en_flag) {
                schedule_delayed_work(&hx9031a_pdata.hw_monitor_work, msecs_to_jiffies(10000));
            }

            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            PRINT_INF("all channels disabled already\n");
        }
    }

    return ret;
}

#else

static int hx9031a_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};
    uint8_t tx_buf[1] = {0};

    ret = hx9031a_read(RA_CH_NUM_CFG_0x24, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
        return -1;
    }
    ch_enable_status = rx_buf[0];

    if(ch_id >= HX9031A_CH_NUM) {
        PRINT_ERR("channel index over range !!!ch_enable_status=0x%02X (ch_id=%d, en=%d)\n", ch_enable_status, ch_id, en);
        return -1;
    }

    if(1 == en) {
        ch_enable_status |= (1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9031a_write(RA_CH_NUM_CFG_0x24, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
        msleep(10);
    } else {
        en = 0;
        ch_enable_status &= ~(1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9031a_write(RA_CH_NUM_CFG_0x24, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
    }
    return 0;
}
#endif

/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 start*/
static int hx9031a_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
    int ret = -1;
    int ii = 0;

    mutex_lock(&hx9031a_ch_en_mutex);
    for (ii = 0; ii < ARRAY_SIZE(hx9031a_channels); ii++) {
        if (strcmp(sensors_cdev->name, hx9031a_channels[ii].name) == 0) {
            if (1 == enable) {
                PRINT_INF("enable ch_%d(name:%s)\n", ii, sensors_cdev->name);
                ret = hx9031a_ch_en(ii, 1);
                if(0 != ret) {
                    PRINT_ERR("hx9031a_ch_en failed\n");
                    mutex_unlock(&hx9031a_ch_en_mutex);
                    return -1;
                }
                hx9031a_channels[ii].enabled = true;
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(&hx9031a_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
#endif
                /* Tab A8 code for AX6300DEV-3809 by xiongxiaoliang at 2021/12/16 start */
                hx9031a_sample();
                hx9031a_get_prox_state(data_diff, HX9031A_CH_NUM);
                /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
                if (!g_onoff_flag) {
                    PRINT_ERR("%s:hx9031 onoff sar off\n", __func__);
                    mutex_unlock(&hx9031a_ch_en_mutex);
                    return 0;
                }
                /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
                if (anfr_status == 1) {
                    PRINT_ERR("lc_enable\n");
                    input_report_abs(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, 0);
                    input_report_rel(hx9031a_channels[ii].hx9031a_input_dev, REL_X, g_anfr_event);
                    input_sync(hx9031a_channels[ii].hx9031a_input_dev);
                } else {
                    if (((hx9031a_pdata.prox_state_reg >> ii) & 0x1) == 0) {
                        PRINT_ERR("lc_handover_far_3\n");
                        hx9031a_channels[ii].state = IDLE;
                    } else {
                        PRINT_ERR("lc_handover_near_3\n");
                        hx9031a_channels[ii].state = PROXACTIVE;
                    }
                    input_report_abs(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, ((hx9031a_pdata.prox_state_reg >> ii) & 0x1) ?  0 : 5);
                    input_report_rel(hx9031a_channels[ii].hx9031a_input_dev, REL_X, g_noanfr_event);
                    input_sync(hx9031a_channels[ii].hx9031a_input_dev);
                }
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
                /* Tab A8 code for AX6300DEV-3809 by xiongxiaoliang at 2021/12/16 end */
            } else if (0 == enable) {
                PRINT_INF("disable ch_%d(name:%s)\n", ii, sensors_cdev->name);
                ret = hx9031a_ch_en(ii, 0);
                if(0 != ret) {
                    PRINT_ERR("hx9031a_ch_en failed\n");
                    mutex_unlock(&hx9031a_ch_en_mutex);
                    return -1;
                }
                hx9031a_channels[ii].enabled = false;
#ifdef CONFIG_PM_WAKELOCKS
                __pm_wakeup_event(&hx9031a_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9031a_wake_lock, HZ * 1);
#endif
                /* Tab A8 code for AX6300DEV-3809 by xiongxiaoliang at 2021/12/16 start */
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
                input_report_abs(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, -1);
                input_report_rel(hx9031a_channels[ii].hx9031a_input_dev, REL_X, g_noanfr_event);
                /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
                /* Tab A8 code for AX6300DEV-3809 by xiongxiaoliang at 2021/12/16 end */
                input_sync(hx9031a_channels[ii].hx9031a_input_dev);
            } else {
                PRINT_ERR("unknown enable symbol\n");
            }
        }
    }
    mutex_unlock(&hx9031a_ch_en_mutex);

    return 0;
}
/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 end*/
//alg start===========================================================================
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
static int hx9031a_alg_ref_ch_drdy_int_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    ret = hx9031a_read(RA_CALI_DIFF_CFG_0x3b, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
        return -1;
    }

    if(ch_id >= HX9031A_CH_NUM) {
        PRINT_ERR("channel index over range !!!(ch_id=%d, en=%d)\n", ch_id, en);
        return -1;
    }

    if(0 == en) {
        buf[0] &= ~(1 << (ch_id + 4));
        ret = hx9031a_write(RA_CALI_DIFF_CFG_0x3b, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_%d drdy int disabled by alg\n", ch_id);
    } else {
        buf[0] |= (1 << (ch_id + 4));
        ret = hx9031a_write(RA_CALI_DIFF_CFG_0x3b, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
        PRINT_INF("ch_%d drdy int enabled by alg\n", ch_id);
    }

    return 0;
}

static int hx9031a_alg_dynamic_threshold_init(uint8_t ref_ch_id)
{
    int ii = 0;

    ENTER;
    hx9031a_alg_ref_ch_drdy_int_en(ref_ch_id, 0);
    hx9031a_alg_ref_ch_drdy_int_en_flag = 0;
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        hx9031a_alg_ch_thres_ini[ii] = 0;
        hx9031a_alg_ch_thres_drift[ii] = 0;
        hx9031a_set_thres_near(ii, hx9031a_alg_ch_thres_backup[ii].thr_near);
        hx9031a_set_thres_far(ii, hx9031a_alg_ch_thres_backup[ii].thr_far);
        PRINT_INF("ch_%d threshold: near=%d, far=%d\n", ii, hx9031a_ch_thres[ii].thr_near, hx9031a_ch_thres[ii].thr_far);
    }
    return 0;
}

static int hx9031a_alg_dynamic_threshold_adjust(uint8_t ref_ch_id)//ch3 == ref_ch_id
{
    int ii = 0;

    ENTER;
    if((hx9031a_pdata.prox_state_far_to_near_flag & hx9031a_pdata.channel_used_flag) > 0) {
        if(0 == hx9031a_alg_ref_ch_drdy_int_en_flag) {
            hx9031a_alg_ref_ch_drdy_int_en(ref_ch_id, 1);
            hx9031a_alg_ref_ch_drdy_int_en_flag = 1;
        }

        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            if (hx9031a_pdata.prox_state_far_to_near_flag >> ii & 0x1) {
                hx9031a_alg_ch_thres_ini[ii] = data_diff[ref_ch_id];
            }
        }
    }

    if(1 == hx9031a_alg_ref_ch_drdy_int_en_flag) {
        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            hx9031a_alg_ch_thres_drift[ii] = data_diff[ref_ch_id] - hx9031a_alg_ch_thres_ini[ii];
        }

        if((hx9031a_alg_ch_thres_drift[2] >= hx9031a_alg_ch_thres_drift_min[0])
           && (hx9031a_alg_ch_thres_drift[2] <= hx9031a_alg_ch_thres_drift_max[0])) {
            hx9031a_ch_thres[2].thr_near = hx9031a_alg_ch_thres_backup[2].thr_near + hx9031a_alg_ch_thres_drift_offset[0];
            hx9031a_ch_thres[2].thr_far = hx9031a_alg_ch_thres_backup[2].thr_far + hx9031a_alg_ch_thres_drift_offset[0];
        }

        if((hx9031a_alg_ch_thres_drift[1] >= hx9031a_alg_ch_thres_drift_min[1])
           && (hx9031a_alg_ch_thres_drift[1] <= hx9031a_alg_ch_thres_drift_max[1])) {
            hx9031a_ch_thres[1].thr_near = hx9031a_alg_ch_thres_backup[1].thr_near - hx9031a_alg_ch_thres_drift[1]/2;
            hx9031a_ch_thres[1].thr_far = hx9031a_alg_ch_thres_backup[1].thr_far- hx9031a_alg_ch_thres_drift[1]/2;
        }

        if((hx9031a_alg_ch_thres_drift[1] >= hx9031a_alg_ch_thres_drift_min[2])
           && (hx9031a_alg_ch_thres_drift[1] < hx9031a_alg_ch_thres_drift_max[2])) {
            hx9031a_ch_thres[1].thr_near = hx9031a_alg_ch_thres_backup[1].thr_near + hx9031a_alg_ch_thres_drift_offset[2];
            hx9031a_ch_thres[1].thr_far = hx9031a_alg_ch_thres_backup[1].thr_far + hx9031a_alg_ch_thres_drift_offset[2];
        }

        if((hx9031a_alg_ch_thres_drift[0] >= hx9031a_alg_ch_thres_drift_min[3])
           && (hx9031a_alg_ch_thres_drift[0] <= hx9031a_alg_ch_thres_drift_max[3])) {
            hx9031a_ch_thres[0].thr_near = hx9031a_alg_ch_thres_backup[0].thr_near + hx9031a_alg_ch_thres_drift_offset[3];
            hx9031a_ch_thres[0].thr_far = hx9031a_alg_ch_thres_backup[0].thr_far + hx9031a_alg_ch_thres_drift_offset[3];
        }

        for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
            if (hx9031a_pdata.prox_state_reg >> ii & 0x1) {
                hx9031a_set_thres_near(ii, hx9031a_ch_thres[ii].thr_near);
                hx9031a_set_thres_far(ii, hx9031a_ch_thres[ii].thr_far);
            } else {
                hx9031a_set_thres_near(ii, hx9031a_alg_ch_thres_backup[ii].thr_near);
                hx9031a_set_thres_far(ii, hx9031a_alg_ch_thres_backup[ii].thr_far);
                /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
                if (1 == ii) {
                    hx9031a_manual_offset_calibration(ii);
                }
                /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
            }
            PRINT_INF("ch_%d threshold: near=%d, far=%d\n", ii, hx9031a_ch_thres[ii].thr_near, hx9031a_ch_thres[ii].thr_far);
        }

        if (0 == (hx9031a_pdata.prox_state_reg & hx9031a_pdata.channel_used_flag)) {
            hx9031a_alg_dynamic_threshold_init(3);
        }
    }
    return 0;
}
#endif
//alg end===========================================================================

static int hx9031a_reg_reinitialize(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t chip_select = 0;
    uint8_t tx_buf[1] = {0};

    ENTER;
    hx9031a_register_init();
    chip_select = hx9031a_get_board_info();
    hx9031a_ch_cfg(chip_select);
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
    hx9031a_data_switch(HX9031A_OUTPUT_LP_BL);
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/

    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        hx9031a_set_thres_near(ii, hx9031a_ch_thres[ii].thr_near);
        hx9031a_set_thres_far(ii, hx9031a_ch_thres[ii].thr_far);
    }

    if(ch_enable_status > 0) {
        tx_buf[0] = hx9031a_pdata.channel_used_flag;
        ret = hx9031a_write(RA_CH_NUM_CFG_0x24, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_write failed\n");
            return -1;
        }
    }
    return 0;
}

static int hx9031a_state_monitoring(uint8_t addr, uint8_t val)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};

    if(0 == hx9031a_hw_monitor_en_flag) {
        PRINT_INF("hw monitor is disabled\n");
        return 0;
    }

    ret = hx9031a_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
        return -1;
    }

    if(val != rx_buf[0]) {
        PRINT_ERR("chip restarted abnormally! do reg reinitialize!\n");
        hx9031a_reg_reinitialize();
    } else {
        PRINT_INF("OK\n");
    }

    return 0 ;
}

static void hx9031a_polling_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9031a_ch_en_mutex);
    hx9031a_state_monitoring(hx9031a_monitor_addr, hx9031a_monitor_val);
    hx9031a_sample();
    hx9031a_get_prox_state(data_diff, HX9031A_CH_NUM);
    //hx9031a_int_clear();
    hx9031a_input_report();

    if(1 == hx9031a_polling_enable)
        schedule_delayed_work(&hx9031a_pdata.polling_work, msecs_to_jiffies(hx9031a_polling_period_ms));
    mutex_unlock(&hx9031a_ch_en_mutex);
    return;
}

static void hx9031a_hw_monitor_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9031a_ch_en_mutex);
    hx9031a_state_monitoring(hx9031a_monitor_addr, hx9031a_monitor_val);
    schedule_delayed_work(&hx9031a_pdata.hw_monitor_work, msecs_to_jiffies(10000));
    mutex_unlock(&hx9031a_ch_en_mutex);
    return;
}

static irqreturn_t hx9031a_irq_handler(int irq, void *pvoid)
{
    ENTER;
    mutex_lock(&hx9031a_ch_en_mutex);
    if(1 == hx9031a_irq_from_suspend_flag) {
        hx9031a_irq_from_suspend_flag = 0;
        PRINT_INF("delay 50ms for waiting the i2c controller enter working mode\n");
        msleep(50);
    }
    hx9031a_state_monitoring(hx9031a_monitor_addr, hx9031a_monitor_val);
    hx9031a_sample();
    hx9031a_get_prox_state(data_diff, HX9031A_CH_NUM);
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
    if (1 == hx9031a_alg_dynamic_threshold_en) {
        hx9031a_alg_dynamic_threshold_adjust(3);
    }
#endif
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
    hx9031a_input_report();
    mutex_unlock(&hx9031a_ch_en_mutex);
    return IRQ_HANDLED;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test begin
static ssize_t hx9031a_raw_data_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *p = buf;
    int ii = 0;

    ENTER;
    hx9031a_sample();
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        p += snprintf(p, PAGE_SIZE, "ch[%d]: DIFF=%-8d, RAW=%-8d, OFFSET=%-8d, BL=%-8d, LP=%-8d\n",
                      ii, data_diff[ii], data_raw[ii], data_offset_dac[ii], data_bl[ii], data_lp[ii]);
    }
    return (p - buf);
}

static ssize_t hx9031a_register_write_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    unsigned int val = 0;
    uint8_t addr = 0;
    uint8_t tx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
        PRINT_ERR("must input two HEX numbers: aa,bb (aa: reg_address, bb: value_to_be_set)\n");
        return -EINVAL;
    }

    addr = (uint8_t)reg_address;
    tx_buf[0] = (uint8_t)val;

    ret = hx9031a_write(addr, tx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }

    PRINT_INF("WRITE:Reg0x%02X=0x%02X\n", addr, tx_buf[0]);
    return count;
}

static ssize_t hx9031a_register_read_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    uint8_t addr = 0;
    uint8_t rx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x", &reg_address) != 1) {
        PRINT_ERR("must input a HEX number\n");
        return -EINVAL;
    }
    addr = (uint8_t)reg_address;

    ret = hx9031a_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    PRINT_INF("READ:Reg0x%02X=0x%02X\n", addr, rx_buf[0]);
    return count;
}
/* Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/8 start */
/*Tab A8 code for SR-AX6300-01-282 by mayuhang at 2021/8/19 start*/
static ssize_t hx9031a_channel_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int value[2] = {0};

    ENTER;
    if (sscanf(buf, "%d %d", &value[0], &value[1]) != 2)
    {
        PRINT_INF("enable format error\n");
        return -EINVAL;
    }
    PRINT_INF("enable:%d %d\n",value[0],value[1]);
    if(value[0] == 176)
    {
        if(value[1] == 0)
        {
            hx9031a_set_enable(&hx9031a_channels[1].hx9031a_sensors_classdev, 0);
        }else if(value[1] > 0)
        {
            hx9031a_set_enable(&hx9031a_channels[1].hx9031a_sensors_classdev, 1);
        }else
        {
            PRINT_INF("enable number value error\n");
            return -EINVAL;
        }
    }
    else if(value[0] == 177)
    {
        if(value[1] == 0)
        {
            hx9031a_set_enable(&hx9031a_channels[2].hx9031a_sensors_classdev, 0);
        }else if(value[1] > 0)
        {
            hx9031a_set_enable(&hx9031a_channels[2].hx9031a_sensors_classdev, 1);
        }else
        {
            PRINT_INF("enable number value error\n");
            return -EINVAL;
        }
    }
    else if(value[0] == 178)
    {
        if(value[1] == 0)
        {
            hx9031a_set_enable(&hx9031a_channels[0].hx9031a_sensors_classdev, 0);
        }else if(value[1] > 0)
        {
            hx9031a_set_enable(&hx9031a_channels[0].hx9031a_sensors_classdev, 1);
        }else
        {
            PRINT_INF("enable number value error\n");
            return -EINVAL;
        }
    }

    return count;
}
/* Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/8 end */
static ssize_t hx9031a_channel_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    ENTER;
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        if (hx9031a_pdata.channel_used_flag >> ii & 0x01) {
            PRINT_INF("hx9031a_channels[%d].enabled=%d\n", ii, hx9031a_channels[ii].enabled);
            p += snprintf(p, PAGE_SIZE, "hx9031a_channels[%d].enabled=%d\n", ii, hx9031a_channels[ii].enabled);
        }
    }

    return (p - buf);
}
/*Tab A8 code for SR-AX6300-01-282 by mayuhang at 2021/8/19 end*/
static ssize_t hx9031a_manual_offset_calibration_show(struct class *class, struct class_attribute *attr, char *buf)
{
    hx9031a_read_offset_dac();
    return sprintf(buf, "OFFSET_DAC: %8d, %8d, %8d, %8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3]);
}

static ssize_t hx9031a_manual_offset_calibration_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned long val;
    uint8_t ch_id = 0;

    ENTER;
    if (kstrtoul(buf, 10, &val)) {
        PRINT_ERR("Invalid Argument\n");
        return -EINVAL;
    }
    ch_id = (uint8_t)val;

    if (ch_id < HX9031A_CH_NUM)
        hx9031a_manual_offset_calibration(ch_id);
    else
        PRINT_ERR(" \"echo ch_id > calibrate\" to do a manual calibrate(ch_id is a channel num (0~%d)\n", HX9031A_CH_NUM);
    return count;
}

static ssize_t hx9031a_prox_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_DBG("prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9031a_ch_near_state:%d-%d-%d-%d, far_to_near_flag=0x%02X\n",
              hx9031a_pdata.prox_state_reg,
              hx9031a_pdata.prox_state_cmp,
              hx9031a_ch_near_state[3],
              hx9031a_ch_near_state[2],
              hx9031a_ch_near_state[1],
              hx9031a_ch_near_state[0],
              hx9031a_pdata.prox_state_far_to_near_flag);

    return sprintf(buf, "prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9031a_ch_near_state:%d-%d-%d-%d, far_to_near_flag=0x%02X\n",
                   hx9031a_pdata.prox_state_reg,
                   hx9031a_pdata.prox_state_cmp,
                   hx9031a_ch_near_state[3],
                   hx9031a_ch_near_state[2],
                   hx9031a_ch_near_state[1],
                   hx9031a_ch_near_state[0],
                   hx9031a_pdata.prox_state_far_to_near_flag);
}
/* Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/8 start */
static ssize_t hx9031a_polling_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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
        hx9031a_polling_period_ms = value;
        if(1 == hx9031a_polling_enable) {
            PRINT_INF("polling is already enabled!, no need to do enable again!, just update the polling period\n");
            goto exit;
        }

        hx9031a_polling_enable = 1;
        hx9031a_disable_irq(hx9031a_pdata.irq);

        PRINT_INF("polling started! period=%dms\n", hx9031a_polling_period_ms);
        schedule_delayed_work(&hx9031a_pdata.polling_work, msecs_to_jiffies(hx9031a_polling_period_ms));
    } else {
        if(0 == hx9031a_polling_enable) {
            PRINT_INF("polling is already disabled!, no need to do again!\n");
            goto exit;
        }
        hx9031a_polling_period_ms = 0;
        hx9031a_polling_enable = 0;
        PRINT_INF("polling stoped!\n");

        cancel_delayed_work(&hx9031a_pdata.polling_work);
        hx9031a_enable_irq(hx9031a_pdata.irq);
    }

exit:
    return count;
}
/* Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/8 end */
static ssize_t hx9031a_batch_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int value[2] = {0};

    ENTER;
    if (sscanf(buf, "%d %d", &value[0], &value[1]) != 2)
    {
        PRINT_INF("enable format error\n");
        return -EINVAL;
    }
    return 0;
}
static ssize_t hx9031a_polling_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031a_polling_enable=%d hx9031a_polling_period_ms=%d\n", hx9031a_polling_enable, hx9031a_polling_period_ms);
    return sprintf(buf, "hx9031a_polling_enable=%d hx9031a_polling_period_ms=%d\n", hx9031a_polling_enable, hx9031a_polling_period_ms);
}

static ssize_t hx9031a_loglevel_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("tyhx_log_level=%d\n", tyhx_log_level);
    return sprintf(buf, "tyhx_log_level=%d\n", tyhx_log_level);
}

static ssize_t hx9031a_loglevel_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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

static ssize_t hx9031a_output_switch_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031a_output_switch=%d\n", hx9031a_output_switch);
    return sprintf(buf, "hx9031a_output_switch=%d\n", hx9031a_output_switch);
}

static ssize_t hx9031a_output_switch_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9031a_data_switch((HX9031A_OUTPUT_RAW_DIFF == value) ? HX9031A_OUTPUT_RAW_DIFF : HX9031A_OUTPUT_LP_BL);
    PRINT_INF("set hx9031a_output_switch=%d\n", hx9031a_output_switch);
    return count;
}

static ssize_t hx9031a_threshold_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int ch = 0;
    unsigned int thr_near = 0;
    unsigned int thr_far = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d", &ch, &thr_near, &thr_far) != 3) {
        PRINT_ERR("please input 3 numbers in DEC: ch,thr_near,thr_far (eg: 0,500,300)\n");
        return -EINVAL;
    }

    if(ch >= HX9031A_CH_NUM || thr_near > (0x03FF * 32) || thr_far > thr_near) {
        PRINT_ERR("input value over range! (valid value: ch=%d, thr_near=%d, thr_far=%d)\n", ch, thr_near, thr_far);
        return -EINVAL;
    }

    thr_near = (thr_near / 32) * 32;
    thr_far = (thr_far / 32) * 32;

    PRINT_INF("set threshold: ch=%d, thr_near=%d, thr_far=%d\n", ch, thr_near, thr_far);
    hx9031a_set_thres_near(ch, thr_near);
    hx9031a_set_thres_far(ch, thr_far);

    return count;
}

static ssize_t hx9031a_threshold_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        hx9031a_get_thres_near(ii);
        hx9031a_get_thres_far(ii);
        PRINT_INF("ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9031a_ch_thres[ii].thr_near, hx9031a_ch_thres[ii].thr_far);
        p += snprintf(p, PAGE_SIZE, "ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9031a_ch_thres[ii].thr_near, hx9031a_ch_thres[ii].thr_far);
    }

    return (p - buf);
}

static ssize_t hx9031a_dump_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ret = -1;
    int ii = 0;
    char *p = buf;
    uint8_t rx_buf[1] = {0};

    for(ii = 0; ii < ARRAY_SIZE(hx9031a_reg_init_list); ii++) {
        ret = hx9031a_read(hx9031a_reg_init_list[ii].addr, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9031a_read failed\n");
        }
        PRINT_INF("0x%02X=0x%02X\n", hx9031a_reg_init_list[ii].addr, rx_buf[0]);
        p += snprintf(p, PAGE_SIZE, "0x%02X=0x%02X\n", hx9031a_reg_init_list[ii].addr, rx_buf[0]);
    }

    p += snprintf(p, PAGE_SIZE, "driver version:%s\n", HX9031A_DRIVER_VER);
    return (p - buf);
}

static ssize_t hx9031a_offset_dac_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    hx9031a_read_offset_dac();

    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        PRINT_INF("data_offset_dac[%d]=%dpF\n", ii, data_offset_dac[ii] * 58 / 1000);
        p += snprintf(p, PAGE_SIZE, "ch[%d]=%dpF ", ii, data_offset_dac[ii] * 58 / 1000);
    }
    p += snprintf(p, PAGE_SIZE, "\n");

    return (p - buf);
}

static ssize_t hx9031a_reinitialize_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    if(1 == value) {
        PRINT_INF("start reg reinitialize\n");
        hx9031a_reg_reinitialize();
    }
    return count;
}

static ssize_t hx9031a_hw_monitor_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031a_hw_monitor_en_flag=%d\n", hx9031a_hw_monitor_en_flag);
    return sprintf(buf, "hx9031a_hw_monitor_en_flag=%d\n", hx9031a_hw_monitor_en_flag);
}

static ssize_t hx9031a_hw_monitor_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9031a_hw_monitor_en_flag = (0 == value) ? 0 : 1;

    if(1 == hx9031a_hw_monitor_en_flag) {
        cancel_delayed_work_sync(&(hx9031a_pdata.hw_monitor_work));
        schedule_delayed_work(&hx9031a_pdata.hw_monitor_work, msecs_to_jiffies(10000));
    } else {
        cancel_delayed_work_sync(&(hx9031a_pdata.hw_monitor_work));
    }

    PRINT_INF("set hx9031a_hw_monitor_en_flag=%d\n", hx9031a_hw_monitor_en_flag);
    return count;
}
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
static ssize_t hx9031a_drift_min_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int temp[4] = {0};
    int ii = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d,%d", &temp[0],
               &temp[1],
               &temp[2],
               &temp[3]) != 4) {
        PRINT_ERR("please input 4 numbers in DEC:(eg: -300,500,300,800)\n");
        return -EINVAL;
    }

    for(ii = 0; ii < 4; ii++) {
        hx9031a_alg_ch_thres_drift_min[ii] = temp[ii];
        PRINT_INF("hx9031a_alg_ch_thres_drift_min[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_min[ii]);
    }

    return count;
}

static ssize_t hx9031a_drift_min_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < 4; ii++) {
        PRINT_INF("hx9031a_alg_ch_thres_drift_min[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_min[ii]);
        p += snprintf(p, PAGE_SIZE, "hx9031a_alg_ch_thres_drift_min[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_min[ii]);
    }

    return (p - buf);
}

static ssize_t hx9031a_drift_max_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int temp[4] = {0};
    int ii = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d,%d", &temp[0],
               &temp[1],
               &temp[2],
               &temp[3]) != 4) {
        PRINT_ERR("please input 4 numbers in DEC:(eg: -300,500,300,800)\n");
        return -EINVAL;
    }

    for(ii = 0; ii < 4; ii++) {
        hx9031a_alg_ch_thres_drift_max[ii] = temp[ii];
        PRINT_INF("hx9031a_alg_ch_thres_drift_max[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_max[ii]);
    }

    return count;
}

static ssize_t hx9031a_drift_max_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < 4; ii++) {
        PRINT_INF("hx9031a_alg_ch_thres_drift_max[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_max[ii]);
        p += snprintf(p, PAGE_SIZE, "hx9031a_alg_ch_thres_drift_max[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_max[ii]);
    }

    return (p - buf);
}

static ssize_t hx9031a_drift_offset_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int temp[4] = {0};
    int ii = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d,%d", &temp[0],
               &temp[1],
               &temp[2],
               &temp[3]) != 4) {
        PRINT_ERR("please input 4 numbers in DEC:(eg: -300,500,300,800)\n");
        return -EINVAL;
    }

    for(ii = 0; ii < 4; ii++) {
        hx9031a_alg_ch_thres_drift_offset[ii] = temp[ii];
        PRINT_INF("hx9031a_alg_ch_thres_drift_offset[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_offset[ii]);
    }

    return count;
}

static ssize_t hx9031a_drift_offset_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < 4; ii++) {
        PRINT_INF("hx9031a_alg_ch_thres_drift_offset[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_offset[ii]);
        p += snprintf(p, PAGE_SIZE, "hx9031a_alg_ch_thres_drift_offset[%d]=%d\n", ii, hx9031a_alg_ch_thres_drift_offset[ii]);
    }

    return (p - buf);
}

static ssize_t hx9031a_threshold_backup_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int ch = 0;
    unsigned int thr_near = 0;
    unsigned int thr_far = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d", &ch, &thr_near, &thr_far) != 3) {
        PRINT_ERR("please input 3 numbers in DEC: ch,thr_near,thr_far (eg: 0,500,300)\n");
        return -EINVAL;
    }

    if(ch >= HX9031A_CH_NUM || thr_near > (0x03FF * 32) || thr_far > thr_near) {
        PRINT_ERR("input value over range! (valid value: ch=%d, thr_near=%d, thr_far=%d)\n", ch, thr_near, thr_far);
        return -EINVAL;
    }

    thr_near = (thr_near / 32) * 32;
    thr_far = (thr_far / 32) * 32;

    PRINT_INF("set threshold of backup: ch=%d, thr_near=%d, thr_far=%d\n", ch, thr_near, thr_far);
    hx9031a_alg_ch_thres_backup[ch].thr_far = thr_far;
    hx9031a_alg_ch_thres_backup[ch].thr_near = thr_near;

    return count;
}

static ssize_t hx9031a_threshold_backup_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        PRINT_INF("ch_%d threshold of backup: near=%-8d, far=%-8d\n",
                  ii, hx9031a_alg_ch_thres_backup[ii].thr_near, hx9031a_alg_ch_thres_backup[ii].thr_far);
        p += snprintf(p, PAGE_SIZE, "ch_%d threshold of backup: near=%-8d, far=%-8d\n",
                      ii, hx9031a_alg_ch_thres_backup[ii].thr_near, hx9031a_alg_ch_thres_backup[ii].thr_far);
    }

    return (p - buf);
}
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
static ssize_t hx9031a_alg_dynamic_threshold_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9031a_alg_dynamic_threshold_en=%d\n", hx9031a_alg_dynamic_threshold_en);
    return sprintf(buf, "hx9031a_alg_dynamic_threshold_en=%d\n", hx9031a_alg_dynamic_threshold_en);
}

static ssize_t hx9031a_alg_dynamic_threshold_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9031a_alg_dynamic_threshold_en = (0 == value) ? 0 : 1;
    hx9031a_alg_dynamic_threshold_init(3);

    PRINT_INF("set hx9031a_alg_dynamic_threshold_en=%d\n", hx9031a_alg_dynamic_threshold_en);
    PRINT_INF("Warning!!! all channels must be disabled before you set the hx9031a_alg_dynamic_threshold_en flag!\n");
    return count;
}
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_raw_data = __ATTR(raw_data, 0664, hx9031a_raw_data_show, NULL);
static struct class_attribute class_attr_reg_write = __ATTR(reg_write,  0664, NULL, hx9031a_register_write_store);
static struct class_attribute class_attr_reg_read = __ATTR(reg_read, 0664, NULL, hx9031a_register_read_store);
static struct class_attribute class_attr_channel_en = __ATTR(enable, 0664, hx9031a_channel_en_show, hx9031a_channel_en_store);
static struct class_attribute class_attr_calibrate = __ATTR(calibrate, 0664, hx9031a_manual_offset_calibration_show, hx9031a_manual_offset_calibration_store);
static struct class_attribute class_attr_prox_state = __ATTR(prox_state, 0664, hx9031a_prox_state_show, NULL);
static struct class_attribute class_attr_polling_period = __ATTR(polling_period, 0664, hx9031a_polling_show, hx9031a_polling_store);
static struct class_attribute class_attr_batch = __ATTR(batch, 0664, NULL, hx9031a_batch_store);
static struct class_attribute class_attr_threshold = __ATTR(threshold, 0664, hx9031a_threshold_show, hx9031a_threshold_store);
static struct class_attribute class_attr_loglevel = __ATTR(loglevel, 0664, hx9031a_loglevel_show, hx9031a_loglevel_store);
static struct class_attribute class_attr_output_switch = __ATTR(output_switch, 0664, hx9031a_output_switch_show, hx9031a_output_switch_store);
static struct class_attribute class_attr_dump = __ATTR(dump, 0664, hx9031a_dump_show, NULL);
static struct class_attribute class_attr_offset_dac = __ATTR(offset_dac, 0664, hx9031a_offset_dac_show, NULL);
static struct class_attribute class_attr_reinitialize = __ATTR(reinitialize,  0664, NULL, hx9031a_reinitialize_store);
static struct class_attribute class_attr_hw_monitor_en = __ATTR(hw_monitor_en, 0664, hx9031a_hw_monitor_en_show, hx9031a_hw_monitor_en_store);
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
static struct class_attribute class_attr_drift_min = __ATTR(drift_min, 0664, hx9031a_drift_min_show, hx9031a_drift_min_store);
static struct class_attribute class_attr_drift_max = __ATTR(drift_max, 0664, hx9031a_drift_max_show, hx9031a_drift_max_store);
static struct class_attribute class_attr_drift_offset = __ATTR(drift_offset, 0664, hx9031a_drift_offset_show, hx9031a_drift_offset_store);
static struct class_attribute class_attr_threshold_backup = __ATTR(threshold_backup, 0664, hx9031a_threshold_backup_show, hx9031a_threshold_backup_store);
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
static struct class_attribute class_attr_alg_dynamic_threshold_en = __ATTR(alg_dynamic_threshold_en, 0664, hx9031a_alg_dynamic_threshold_en_show, hx9031a_alg_dynamic_threshold_en_store);
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
#endif

static struct attribute *hx9031a_class_attrs[] = {
    &class_attr_raw_data.attr,
    &class_attr_reg_write.attr,
    &class_attr_reg_read.attr,
    &class_attr_channel_en.attr,
    &class_attr_calibrate.attr,
    &class_attr_prox_state.attr,
    &class_attr_polling_period.attr,
    &class_attr_threshold.attr,
    &class_attr_loglevel.attr,
    &class_attr_output_switch.attr,
    &class_attr_dump.attr,
    &class_attr_offset_dac.attr,
    &class_attr_reinitialize.attr,
    &class_attr_hw_monitor_en.attr,
    &class_attr_batch.attr,
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
    &class_attr_drift_min.attr,
    &class_attr_drift_max.attr,
    &class_attr_drift_offset.attr,
    &class_attr_threshold_backup.attr,
    /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
    &class_attr_alg_dynamic_threshold_en.attr,
    /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
#endif
    NULL,
};
ATTRIBUTE_GROUPS(hx9031a_class);
#else
static struct class_attribute hx9031a_class_attributes[] = {
    __ATTR(raw_data, 0664, hx9031a_raw_data_show, NULL),
    __ATTR(reg_write,  0664, NULL, hx9031a_register_write_store),
    __ATTR(reg_read, 0664, NULL, hx9031a_register_read_store),
    __ATTR(channel_en, 0664, hx9031a_channel_en_show, hx9031a_channel_en_store),
    __ATTR(calibrate, 0664, hx9031a_manual_offset_calibration_show, hx9031a_manual_offset_calibration_store),
    __ATTR(prox_state, 0664, hx9031a_prox_state_show, NULL),
    __ATTR(polling_period, 0664, hx9031a_polling_show, hx9031a_polling_store),
    __ATTR(threshold, 0664, hx9031a_threshold_show, hx9031a_threshold_store),
    __ATTR(loglevel, 0664, hx9031a_loglevel_show, hx9031a_loglevel_store),
    __ATTR(output_switch, 0664, hx9031a_output_switch_show, hx9031a_output_switch_store),
    __ATTR(dump, 0664, hx9031a_dump_show, NULL),
    __ATTR(offset_dac, 0664, hx9031a_offset_dac_show, NULL),
    __ATTR(reinitialize,  0664, NULL, hx9031a_reinitialize_store),
    __ATTR(hw_monitor_en, 0664, hx9031a_hw_monitor_en_show, hx9031a_hw_monitor_en_store),
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
    __ATTR(drift_min, 0664, hx9031a_drift_min_show, hx9031a_drift_min_store),
    __ATTR(drift_max, 0664, hx9031a_drift_max_show, hx9031a_drift_max_store),
    __ATTR(drift_offset, 0664, hx9031a_drift_offset_show, hx9031a_drift_offset_store),
    __ATTR(threshold_backup, 0664, hx9031a_threshold_backup_show, hx9031a_threshold_backup_store),
    /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
    __ATTR(alg_dynamic_threshold_en, 0664, hx9031a_alg_dynamic_threshold_en_show, hx9031a_alg_dynamic_threshold_en_store),
    /* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
#endif
    __ATTR_NULL,
};
#endif

struct class hx9031a_class = {
        .name = "sar",
        .owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups = hx9031a_class_groups,
#else
        .class_attrs = hx9031a_class_attributes,
#endif
    };
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test end
/*Tab A8 code for SR-AX6300-01-80 by mayuhang at 2021/8/21 start*/
static void sar_calibration_func(struct work_struct *work)
{
    int ret = 0;
    uint8_t buf[1] = {0};
    PRINT_ERR("hx9031a_calibration in\n");
    ENTER;
    ret = hx9031a_read(RA_OFFSET_CALI_CTRL_0xc2, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_read failed\n");
    }

    buf[0] |= 0x70;
    ret = hx9031a_write(RA_OFFSET_CALI_CTRL_0xc2, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9031a_write failed\n");
    }
}
/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 start*/
/*
Name:sdcard_calibration_func
Author:mayuhang
Date:2021/10/21
Param:nb,event,v
Return:0
Purpose:sdcard insert callback to run calibration
*/
int sdcard_calibration_func(struct notifier_block *nb, unsigned long event, void *v)
{
    PRINT_INF("sdcard_calibration_func entry\n");
    if ((event == SDCARD_INSERT_EVENT) && (sar_work != NULL)) {
        queue_work(sar_work, &sar_enable_work);
    } else {
        PRINT_INF("event or sar_work error\n");
    }
    return 0;
}

/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
ssize_t hx9031a_set_onoff(const char *buf, size_t count)
{
    if (!strncmp(buf, "1", 1)) {
        g_onoff_flag = true;
        hx9031a_onoff_enablereport();
        pr_err("%s :set %d on success", __func__, g_onoff_flag);
    } else if (!strncmp(buf, "0", 1)) {
        g_onoff_flag = false;
        if (hx9031a_channels[0].enabled || hx9031a_channels[1].enabled || hx9031a_channels[2].enabled) {
            input_report_abs(hx9031a_channels[0].hx9031a_input_dev, ABS_DISTANCE, 5);
            input_report_abs(hx9031a_channels[1].hx9031a_input_dev, ABS_DISTANCE, 5);
            input_report_abs(hx9031a_channels[2].hx9031a_input_dev, ABS_DISTANCE, 5);
            input_sync(hx9031a_channels[0].hx9031a_input_dev);
            input_sync(hx9031a_channels[1].hx9031a_input_dev);
            input_sync(hx9031a_channels[2].hx9031a_input_dev);
        }
    } else {
        pr_err("Invalid");
    }
    return count;
}

ssize_t hx9031a_get_onoff(char *buf)
{
    return snprintf(buf, 8,"%d\n",g_onoff_flag);
}
/*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/


/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 end*/
void sar_usb_callback_init(void)
{
    sar_work = create_singlethread_workqueue("hx_sar_calibration_wq");

    if (!sar_work) {
        PRINT_INF("create_singlethread_workqueue failed\n");
        return;
    }

    INIT_WORK(&sar_enable_work, sar_calibration_func);
}
/*Tab A8 code for SR-AX6300-01-80 by mayuhang at 2021/8/21 end*/
/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 start*/
static struct notifier_block sdcard_block = {
    .notifier_call = sdcard_calibration_func,
};
/*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 end*/
/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 start*/
static int hx9031a_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ii = 0;
    int ret = 0;
    uint8_t chip_select = 0;
    struct hx9031a_channel_info *channel_p = NULL;

    ENTER;

    PRINT_INF("i2c address from DTS is 0x%02X", client->addr);
    client->addr = 0x28;
    PRINT_INF("i2c address from driver is 0x%02X", client->addr);

    if (!i2c_check_functionality(to_i2c_adapter(client->dev.parent), I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        PRINT_ERR("i2c_check_functionality failed\n");
        ret = -EIO;
        goto failed_i2c_check_functionality;
    }

    i2c_set_clientdata(client, (&hx9031a_pdata));
    hx9031a_i2c_client = client;
    hx9031a_pdata.pdev = &client->dev;
    client->dev.platform_data = (&hx9031a_pdata);

//{begin =============================================
    ret = hx9031a_parse_dt(&client->dev);//yasin: power, irq, regs
    if (ret) {
        PRINT_ERR("hx9031a_parse_dt failed\n");
        ret = -ENODEV;
        goto failed_parse_dt;
    }

    client->irq = hx9031a_pdata.irq;

    ret = hx9031a_gpio_init();
    if (ret) {
        PRINT_ERR("hx9031a_gpio_init failed\n");
        ret = -1;
        goto failed_gpio_init;
    }

    hx9031a_power_on(1);
//}end =============================================================

    if(hx9031a_id_check() < 0) {
        PRINT_ERR("hx9031_id_check failed\n");
        ret = -1;
        goto failed_id_check;
    }

    hx9031a_register_init();
    chip_select = hx9031a_get_board_info();
    hx9031a_ch_cfg(chip_select);
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 start*/
    hx9031a_data_switch(HX9031A_OUTPUT_LP_BL);
    /*Tab A8_T code for SR-AX6301A-01-112 by lichang at 2022/10/18 end*/
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 start */
#if HX9031A_ALG_COMPILE_EN
/* Tab A8 code for AX6300DEV-735 by mayuhang at 2021/9/23 end */
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        hx9031a_alg_ch_thres_backup[ii].thr_far = hx9031a_ch_thres[ii].thr_far;
        hx9031a_alg_ch_thres_backup[ii].thr_near = hx9031a_ch_thres[ii].thr_near;
    }
    hx9031a_alg_dynamic_threshold_init(3);
#else
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        hx9031a_set_thres_near(ii, hx9031a_ch_thres[ii].thr_near);
        hx9031a_set_thres_far(ii, hx9031a_ch_thres[ii].thr_far);
    }
#endif

    for (ii = 0; ii < ARRAY_SIZE(hx9031a_channels); ii++) {
        if (hx9031a_pdata.channel_used_flag >> ii & 0x01) {
            hx9031a_channels[ii].used = true;
            hx9031a_channels[ii].state = IDLE;

            hx9031a_channels[ii].hx9031a_input_dev = input_allocate_device();
            if (!hx9031a_channels[ii].hx9031a_input_dev) {
                PRINT_ERR("input_allocate_device failed\n");
                ret = -ENOMEM;
                goto failed_allocate_input_dev;
            }

            hx9031a_channels[ii].hx9031a_input_dev->name = hx9031a_channels[ii].name;
            /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 start*/
            if(ii == 0)
            {
#ifdef HQ_FACTORY_BUILD
                __set_bit(EV_KEY, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                __set_bit(KEY_SAR1_FAR, hx9031a_channels[ii].hx9031a_input_dev->keybit);
                __set_bit(KEY_SAR1_CLOSE, hx9031a_channels[ii].hx9031a_input_dev->keybit);
#else
                __set_bit(EV_ABS, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                input_set_abs_params(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, -1, 100, 0, 0);
                input_set_capability(hx9031a_channels[ii].hx9031a_input_dev, EV_REL, REL_X);
#endif
            }
            if(ii == 1)
            {
#ifdef HQ_FACTORY_BUILD
                __set_bit(EV_KEY, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                __set_bit(KEY_SAR2_FAR, hx9031a_channels[ii].hx9031a_input_dev->keybit);
                __set_bit(KEY_SAR2_CLOSE, hx9031a_channels[ii].hx9031a_input_dev->keybit);
#else
                __set_bit(EV_ABS, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                input_set_abs_params(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, -1, 100, 0, 0);
                input_set_capability(hx9031a_channels[ii].hx9031a_input_dev, EV_REL, REL_X);
#endif
            }
            if(ii == 2)
            {
#ifdef HQ_FACTORY_BUILD
                __set_bit(EV_KEY, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                __set_bit(KEY_SAR3_FAR, hx9031a_channels[ii].hx9031a_input_dev->keybit);
                __set_bit(KEY_SAR3_CLOSE, hx9031a_channels[ii].hx9031a_input_dev->keybit);
#else
                __set_bit(EV_ABS, hx9031a_channels[ii].hx9031a_input_dev->evbit);
                input_set_abs_params(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, -1, 100, 0, 0);
                input_set_capability(hx9031a_channels[ii].hx9031a_input_dev, EV_REL, REL_X);
#endif
            }
            /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 end*/
            ret = input_register_device(hx9031a_channels[ii].hx9031a_input_dev);

            input_report_abs(hx9031a_channels[ii].hx9031a_input_dev, ABS_DISTANCE, -1);
            input_report_rel(hx9031a_channels[ii].hx9031a_input_dev, REL_X, g_noanfr_event);
            input_sync(hx9031a_channels[ii].hx9031a_input_dev);

            hx9031a_channels[ii].hx9031a_sensors_classdev.sensors_enable = hx9031a_set_enable;
            hx9031a_channels[ii].hx9031a_sensors_classdev.sensors_poll_delay = NULL;
            hx9031a_channels[ii].hx9031a_sensors_classdev.name = hx9031a_channels[ii].name;
            hx9031a_channels[ii].hx9031a_sensors_classdev.vendor = "HX9031A";
            hx9031a_channels[ii].hx9031a_sensors_classdev.version = 1;
            hx9031a_channels[ii].hx9031a_sensors_classdev.type = SENSOR_TYPE_CAPSENSE;
            hx9031a_channels[ii].hx9031a_sensors_classdev.max_range = "5";
            hx9031a_channels[ii].hx9031a_sensors_classdev.resolution = "5.0";
            hx9031a_channels[ii].hx9031a_sensors_classdev.sensor_power = "3";
            hx9031a_channels[ii].hx9031a_sensors_classdev.min_delay = 0;
            hx9031a_channels[ii].hx9031a_sensors_classdev.fifo_reserved_event_count = 0;
            hx9031a_channels[ii].hx9031a_sensors_classdev.fifo_max_event_count = 0;
            hx9031a_channels[ii].hx9031a_sensors_classdev.delay_msec = 100;
            hx9031a_channels[ii].hx9031a_sensors_classdev.enabled = 0;
            hx9031a_channels[ii].enabled = false;

            ret = sensors_classdev_register(&hx9031a_channels[ii].hx9031a_input_dev->dev, &hx9031a_channels[ii].hx9031a_sensors_classdev);
            if (ret < 0) {
                PRINT_ERR("create %d cap sensor_class  file failed (%d)\n", ii, ret);
            }
        }
    }
    /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 start*/
    sar_func.set_onoff = hx9031a_set_onoff;
    sar_func.get_onoff = hx9031a_get_onoff;
    /*Tab A8_U code for SR-AX6300U-01-1 by wentao at 2023/7/26 end*/
    spin_lock_init(&hx9031a_pdata.lock);
    INIT_DELAYED_WORK(&hx9031a_pdata.polling_work, hx9031a_polling_work_func);
    /*Tab A8 code for SR-AX6300-01-80 by mayuhang at 2021/8/21 start*/
    sar_usb_callback_init();
    /*Tab A8 code for SR-AX6300-01-80 by mayuhang at 2021/8/21 end*/
    INIT_DELAYED_WORK(&hx9031a_pdata.hw_monitor_work, hx9031a_hw_monitor_work_func);
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_init(&hx9031a_wake_lock, "hx9031a_wakelock");
#else
    wake_lock_init(&hx9031a_wake_lock, WAKE_LOCK_SUSPEND, "hx9031a_wakelock");
#endif

    ret = request_threaded_irq(hx9031a_pdata.irq, NULL, hx9031a_irq_handler,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                               hx9031a_pdata.pdev->driver->name, (&hx9031a_pdata));
    if(ret < 0) {
        PRINT_ERR("request_irq failed irq=%d ret=%d\n", hx9031a_pdata.irq, ret);
        goto failed_request_irq;
    }
    enable_irq_wake(hx9031a_pdata.irq);//enable irq wakeup PM
    hx9031a_irq_en_flag = 1;//irq is enabled after request by default
    /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 start*/
    //debug sys fs
    ret = class_register(&hx9031a_class);
    if (ret < 0) {
        PRINT_ERR("class_register failed\n");
        goto failed_class_register;
    }
    /*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 end*/
/*Tab A8 code for SR-AX6300-01-282 by mayuhang at 2021/8/19 start*/
#if HX9031A_TEST_ON_MTK_DEMO
    PRINT_INF("enable all channels for test\n");
    for(ii = 0; ii < HX9031A_CH_NUM; ii++) {
        if (hx9031a_pdata.channel_used_flag >> ii & 0x01) {
            hx9031a_set_enable(&hx9031a_channels[ii].hx9031a_sensors_classdev, 1);
        }
    }
#endif
    /*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 start*/
    raw_notifier_chain_register(&sdcard_hx9031_notify, &sdcard_block);
    /*Tab A8 code for AX6300DEV-1840 by mayuhang at 2021/10/21 end*/
/*Tab A8 code for SR-AX6300-01-282 by mayuhang at 2021/8/19 end*/
    /*Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/6 start*/
    sar_name = "hx9031a";
    /*Tab A8 code for SR-AX6300-01-66 by mayuhang at 2021/9/6 end*/
    PRINT_INF("probe success\n");
    return 0;
/*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 start*/
failed_class_register:
    free_irq(hx9031a_pdata.irq, (&hx9031a_pdata));
failed_request_irq:
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_trash(&hx9031a_wake_lock);
#else
    wake_lock_destroy(&hx9031a_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9031a_pdata.polling_work));
    for (ii = 0; ii < ARRAY_SIZE(hx9031a_channels); ii++) {
        channel_p = &(hx9031a_channels[ii]);
        if (channel_p->used == true) {
            sensors_classdev_unregister(&(channel_p->hx9031a_sensors_classdev));
            input_unregister_device(channel_p->hx9031a_input_dev);
        }
    }
failed_allocate_input_dev:
failed_id_check:
    hx9031a_power_on(0);
    hx9031a_gpio_deinit();
failed_gpio_init:
failed_parse_dt:
failed_i2c_check_functionality:
    PRINT_ERR("probe failed\n");
/*Tab A8 code for SR-AX6300-01-81 by mayuhang at 2021/8/12 end*/
    return ret;
}
/*Tab A8_T code for AX6300TDEV-642 by xiongxiaoliang at 2023/06/20 end*/

static int hx9031a_remove(struct i2c_client *client)
{
    struct hx9031a_channel_info *channel_p = NULL;
    int ii = 0;

    ENTER;
    cancel_delayed_work_sync(&(hx9031a_pdata.hw_monitor_work));
    class_unregister(&hx9031a_class);
    free_irq(hx9031a_pdata.irq, (&hx9031a_pdata));
#ifdef CONFIG_PM_WAKELOCKS
    wakeup_source_trash(&hx9031a_wake_lock);
#else
    wake_lock_destroy(&hx9031a_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9031a_pdata.polling_work));
    for (ii = 0; ii < ARRAY_SIZE(hx9031a_channels); ii++) {
        channel_p = &(hx9031a_channels[ii]);
        if (channel_p->used == true) {
            sensors_classdev_unregister(&(channel_p->hx9031a_sensors_classdev));
            input_unregister_device(channel_p->hx9031a_input_dev);
        }
    }
    hx9031a_power_on(0);
    hx9031a_gpio_deinit();
    return 0;
}

static int hx9031a_suspend(struct device *dev)
{
    ENTER;
    hx9031a_irq_from_suspend_flag = 1;
    return 0;
}

static int hx9031a_resume(struct device *dev)
{
    ENTER;
    hx9031a_irq_from_suspend_flag = 0;
    hx9031a_state_monitoring(hx9031a_monitor_addr, hx9031a_monitor_val);
    return 0;
}
/*Tab A8 code for SR-AX6300-01-82 by mayuhang at 2021/9/15 end*/
static struct i2c_device_id hx9031a_i2c_id_table[] = {
    { HX9031A_DRIVER_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, hx9031a_i2c_id_table);
#ifdef CONFIG_OF
static struct of_device_id hx9031a_of_match_table[] = {
#if HX9031A_TEST_ON_MTK_DEMO
    {.compatible = "mediatek,sar"},
#else
    {.compatible = "tyhx,hx9031a"},
#endif
    { },
};
#else
#define hx9031a_of_match_table NULL
#endif

static const struct dev_pm_ops hx9031a_pm_ops = {
    .suspend = hx9031a_suspend,
    .resume = hx9031a_resume,
};

static struct i2c_driver hx9031a_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = HX9031A_DRIVER_NAME,
        .of_match_table = hx9031a_of_match_table,
        .pm = &hx9031a_pm_ops,
    },
    .id_table = hx9031a_i2c_id_table,
    .probe = hx9031a_probe,
    .remove = hx9031a_remove,
};

static int __init hx9031a_module_init(void)
{
    ENTER;
    PRINT_INF("driver version:%s\n", HX9031A_DRIVER_VER);
    return i2c_add_driver(&hx9031a_i2c_driver);
}

static void __exit hx9031a_module_exit(void)
{
    ENTER;
    i2c_del_driver(&hx9031a_i2c_driver);
}

module_init(hx9031a_module_init);
module_exit(hx9031a_module_exit);
MODULE_AUTHOR("Yasin Lee <yasin.lee.x@gmail.com><yasin.lee@tianyihexin.com>");
MODULE_DESCRIPTION("Driver for NanJingTianYiHeXin HX9031A & HX9023E Cap Sense");
MODULE_ALIAS("sar driver");
MODULE_LICENSE("GPL");
