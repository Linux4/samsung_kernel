/*************************************************************
*                                                            *
*  Driver for NanJingTianYiHeXin HX9023S Cap Sensor         *
*                                                            *
*************************************************************/
/*
                       客制化配置说明

1. dts配置：仅供参考！！！
    sar_hx9023s@28 {
            compatible = "tyhx,hx9023s";
            i2c_num = <3>;
            i2c_addr = <0x28 0 0 0>;
            interrupt-parent = <&pio>;
            interrupts = <5 IRQ_TYPE_EDGE_FALLING 5 0>;
            tyhx,irq-gpio = <&pio 5 0x0>;
            tyhx,irq-flags = <2>;           // 1:rising, 2:falling;
            tyhx,channel-flag = <0x07>;     //必要，每个channel对应一个input设备，每个bit对应一个channel（channel）。如0x7代表开启0，1，2通道
            //tyhx,power-supply-type =<1>;  //可选，如何供电，如果有的话，客户自行移植
            status = "okay";
    };

2. 填充上下电函数：static void hx9023s_power_on(uint8_t on) 如果是常供电，可不用配置此项

3. 根据实际硬件电路形式，FAE协助确认通道配置函数中的cs和channel映射关系：static void hx9023s_ch_cfg(void)

4. 根据实际需要，在dtsi文件的对应节点中和dts解析函数static int hx9023s_parse_dt(struct device *dev)中添加你需要的其他属性信息，
   如其他gpio和regulator等，参考dts配置中的中断gpio号"tyhx,irq-gpio"和通道数配置"tyhx,channel-flag"是必要的。
   更多的gpio配置，请放在 static int hx9023s_gpio_init(void) 和 static void hx9023s_gpio_deinit(void) 中
*/

#define HX9023S_DRIVER_VER "Change-Id 004" //20230627 update by liuling

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#define POWER_ENABLE    1
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch

#define USER_TEST 1

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
#if 0
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h> //4.19的内核上必须走这个，所以需要将代码中的所有宏判断 #ifdef CONFIG_PM_WAKELOCKS 全部替换成 #if 0
#endif
#include "hx9023s.h"
// + wt Adaptive sensor hal, wangjianlong, 20230413, start
#include <linux/sensors.h>
#include <linux/hardware_info.h>
// - wt Adaptive sensor hal, wangjianlong, 20230413, end
#define HX9023S_TEST_CHS_EN 0

static struct i2c_client *hx9023s_i2c_client = NULL;
static struct hx9023s_platform_data hx9023s_pdata;
static uint8_t ch_enable_status = 0x00;
static uint8_t hx9023s_polling_enable = 0;
static int hx9023s_polling_period_ms = 0;

static int32_t data_raw[HX9023S_CH_NUM] = {0};
static int32_t data_diff[HX9023S_CH_NUM] = {0};
static int32_t data_lp[HX9023S_CH_NUM] = {0};
static int32_t data_bl[HX9023S_CH_NUM] = {0};
static uint16_t data_offset_dac[HX9023S_CH_NUM] = {0};


static struct hx9023s_near_far_threshold hx9023s_ch_thres[HX9023S_CH_NUM] = {
/*+S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, Modify the sar trigger distance*/
    {.thr_near = 288, .thr_far = 256}, //ch0
    {.thr_near = 96, .thr_far = 64},
    {.thr_near = 32736, .thr_far = 32736},
/*-S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, Modify the sar trigger distance*/
};

static uint8_t hx9023s_ch_near_state[HX9023S_CH_NUM] = {0};  //只有远近
static volatile uint8_t hx9023s_irq_from_suspend_flag = 0;
static volatile uint8_t hx9023s_irq_en_flag = 1;
static volatile uint8_t hx9023s_data_accuracy = 16;

static DEFINE_MUTEX(hx9023s_i2c_rw_mutex);
static DEFINE_MUTEX(hx9023s_ch_en_mutex);
static DEFINE_MUTEX(hx9023s_cali_mutex);

#if 0
static struct wakeup_source hx9023s_wake_lock;
#else
static struct wake_lock hx9023s_wake_lock;
#endif

enum {
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
    SAR_STATE_NEAR = 1,
    SAR_STATE_FAR,
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
};


//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^通用读写 START
//从指定起始寄存器开始，连续写入count个值
static int linux_common_i2c_write(struct i2c_client *client, uint8_t *reg_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[HX9023S_MAX_XFER_SIZE + 1] = {0};
    struct i2c_msg msg[1];

    if(count > HX9023S_MAX_XFER_SIZE) {
        count = HX9023S_MAX_XFER_SIZE;
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

//从指定起始寄存器开始，连续读取count个值
static int linux_common_i2c_read(struct i2c_client *client, uint8_t *reg_addr, uint8_t *rxbuf, int count)
{
    int ret = -1;
    int ii = 0;
    struct i2c_msg msg[2];

    if(count > HX9023S_MAX_XFER_SIZE) {
        count = HX9023S_MAX_XFER_SIZE;
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
static int hx9023s_read(uint8_t addr, uint8_t *rxbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9023s_i2c_rw_mutex);
    ret = linux_common_i2c_read(hx9023s_i2c_client, &addr, rxbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_read failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9023s_i2c_rw_mutex);
    return ret;
}

//return 0 for success, return -1 for errors.
static int hx9023s_write(uint8_t addr, uint8_t *txbuf, int count)
{
    int ret = -1;

    mutex_lock(&hx9023s_i2c_rw_mutex);
    ret = linux_common_i2c_write(hx9023s_i2c_client, &addr, txbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9023s_i2c_rw_mutex);
    return ret;
}
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^通用读写 END

static void hx9023s_disable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(1 == hx9023s_irq_en_flag) {
        disable_irq_nosync(hx9023s_pdata.irq);
        hx9023s_irq_en_flag = 0;
        PRINT_DBG("irq_%d is disabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is disabled already!\n", irq);
    }
}

static void hx9023s_enable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("wrong irq number!\n");
        return;
    }

    if(0 == hx9023s_irq_en_flag) {
        enable_irq(hx9023s_pdata.irq);
        hx9023s_irq_en_flag = 1;
        PRINT_DBG("irq_%d is enabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is enabled already!\n", irq);
    }
}

static void hx9023s_data_lock(uint8_t lock_flag)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};

    if(HX9023S_DATA_LOCK == lock_flag) {
        ret = hx9023s_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_read failed\n");
        }

        rx_buf[0] = rx_buf[0] | 0x10;

        ret = hx9023s_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("rx_buf[0]=0x%02X,hx9023s_write failed\n", rx_buf[0]);
        }
    } else if(HX9023S_DATA_UNLOCK == lock_flag) {
        ret = hx9023s_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_read failed\n");
        }

        rx_buf[0] = rx_buf[0] & 0xE7;

        ret = hx9023s_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("rx_buf[0]=0x%02X,hx9023s_write failed\n", rx_buf[0]);
        }
    } else {
        PRINT_ERR("ERROR!!! hx9023s_data_lock wrong para. now do data unlock!\n");
        ret = hx9023s_read(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_read failed\n");
        }

        rx_buf[0] = rx_buf[0] & 0xE7;

        ret = hx9023s_write(RW_C8_DSP_CONFIG_CTRL1, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("rx_buf[0]=0x%02X,hx9023s_write failed\n", rx_buf[0]);
        }
    }
}

static int hx9023s_id_check(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t device_id = 0;
    uint8_t rxbuf[1] = {0};

    for(ii = 0; ii < HX9023S_ID_CHECK_COUNT; ii++) {
        ret = hx9023s_read(RO_60_DEVICE_ID, rxbuf, 1);
        if(ret < 0) {
            PRINT_ERR("i2c read error\n");
            continue;
        }
        device_id = rxbuf[0];
        if((HX9023S_CHIP_ID == device_id))
            break;
        else
            continue;
    }

    if(HX9023S_CHIP_ID == device_id) {
        PRINT_INF("success! device_id=0x%02X(HX9023S)\n", device_id);
        return 0;
    } else {
        PRINT_ERR("failed! device_id=0x%02X(UNKNOW_CHIP_ID)\n", device_id);
        return -1;
    }
}

static void hx9023s_ch_cfg(void)
{
    int ret = -1;
    int ii = 0;
    uint16_t ch_cfg = 0;
    uint8_t cfg[HX9023S_CH_NUM * 2] = {0};

    uint8_t CS0 = 0;
    uint8_t CS1 = 0;
    uint8_t CS2 = 0;
    uint8_t NA = 0;
    uint8_t CH0_POS = 0;
    uint8_t CH0_NEG = 0;
    uint8_t CH1_POS = 0;
    uint8_t CH1_NEG = 0;
    uint8_t CH2_POS = 0;
    uint8_t CH2_NEG = 0;

    ENTER;

    //CS引脚配置在寄存器列表中的映射关系：这个映射表存在的目的是CS号在寄存器列表中并没有顺序排列 ┓(´∀`)┏
    CS0 = 0;
    CS1 = 1;
    CS2 = 2;
    NA = 10;
    PRINT_INF("HX9023S_ON_BOARD\n");

    //TODO:通道客制化配置开始 =================================================
    //每个通道的正负极都可以连接到任何一个CS。按需配置。
    CH0_POS = CS0;  //MAIN CS0+ CS2-
    CH0_NEG = CS2;
    CH1_POS = CS1;  //WCN CS1+
    CH1_NEG = NA;
    CH2_POS = CS2;
    CH2_NEG = NA;
    //TODO:通道客制化配置结束 ===============================================

    ch_cfg = (uint16_t)((0x03 << (CH0_POS * 2)) + (0x02 << (CH0_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH1_POS * 2)) + (0x02 << (CH1_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH2_POS * 2)) + (0x02 << (CH2_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ret = hx9023s_write(RW_03_CH0_CFG_7_0, cfg, HX9023S_CH_NUM * 2);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
}

static void hx9023s_reg_init(void)
{
    int ii = 0;
    int ret = -1;

    while(ii < (int)ARRAY_SIZE(hx9023s_reg_init_list)) {
        ret = hx9023s_write(hx9023s_reg_init_list[ii].addr, &hx9023s_reg_init_list[ii].val, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_write failed\n");
        }
        ii++;
    }
}

static void hx9023s_read_offset_dac(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9023S_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    uint32_t data[HX9023S_CH_NUM] = {0};

    hx9023s_data_lock(HX9023S_DATA_LOCK);
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;
    ret = hx9023s_read(RW_15_OFFSET_DAC0_7_0, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0xFFF;//12位
            data_offset_dac[ii] = data[ii];
        }
    }
    hx9023s_data_lock(HX9023S_DATA_UNLOCK);

    PRINT_DBG("OFFSET_DAC, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2]);
}


static void hx9023s_manual_offset_calibration(void)
{
    int ret = -1;
    uint8_t buf[2] = {0};

    mutex_lock(&hx9023s_cali_mutex);
    ret = hx9023s_read(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }
    ret = hx9023s_read(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }

    buf[0] |= 0xF0;
    buf[1] |= 0x10;

    ret = hx9023s_write(RW_C2_OFFSET_CALI_CTRL, &buf[0], 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    ret = hx9023s_write(RW_90_OFFSET_CALI_CTRL1, &buf[1], 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }

    PRINT_INF("channels will calibrate in next convert cycle (ODR=%dms)\n", HX9023S_ODR_MS);
    mutex_unlock(&hx9023s_cali_mutex);
}

static int16_t hx9023s_get_thres_near(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[2] = {0};

    ret = hx9023s_read(RW_80_PROX_HIGH_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
    hx9023s_ch_thres[ch].thr_near = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;

    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }

    return hx9023s_ch_thres[ch].thr_near;
}

static int16_t hx9023s_get_thres_far(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[2] = {0};

    ret = hx9023s_read(RW_88_PROX_LOW_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);
    hx9023s_ch_thres[ch].thr_far = (buf[0] + ((buf[1] & 0x03) << 8)) * 32;

    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }

    return hx9023s_ch_thres[ch].thr_far;
}

static int16_t hx9023s_set_thres_near(uint8_t ch, int16_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9023s_ch_thres[ch].thr_near = (val & 0x03FF) * 32;
 
    ret = hx9023s_write(RW_80_PROX_HIGH_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);

    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }

    PRINT_INF("hx9023s_ch_thres[%d].thr_near=%d\n", ch, hx9023s_ch_thres[ch].thr_near);
    return hx9023s_ch_thres[ch].thr_near;
}

static int16_t hx9023s_set_thres_far(uint8_t ch, int16_t val)
{
    int ret = -1;
    uint8_t buf[2];

    val /= 32;
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0x03;
    hx9023s_ch_thres[ch].thr_far = (val & 0x03FF) * 32;

    ret = hx9023s_write(RW_88_PROX_LOW_DIFF_CFG_CH0_0 + (ch * CH_DATA_2BYTES), buf, 2);

    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }

    PRINT_INF("hx9023s_ch_thres[%d].thr_far=%d\n", ch, hx9023s_ch_thres[ch].thr_far);
    return hx9023s_ch_thres[ch].thr_far;
}

static void hx9023s_get_prox_state(int32_t *data_compare, uint8_t ch_num)
{
    int ret = -1;
    int ii = 0;
    int16_t near = 0;
    int16_t far = 0;
    uint8_t buf[1] = {0};

    hx9023s_pdata.prox_state_reg = 0;
    hx9023s_pdata.prox_state_cmp = 0;
    for(ii = 0; ii < ch_num; ii++) {
        hx9023s_ch_near_state[ii] = 0;
    }

    ret = hx9023s_read(RO_6B_PROX_STATUS, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }
    hx9023s_pdata.prox_state_reg = buf[0];

    for(ii = 0; ii < ch_num; ii++) {
        near = hx9023s_get_thres_near(ii);
        far = hx9023s_get_thres_far(ii);
        if(data_compare[ii] >= near) {
            hx9023s_ch_near_state[ii] = 1;//near
            hx9023s_pdata.prox_state_cmp |= (1 << ii);
        } else if(data_compare[ii] <= far) {
            hx9023s_ch_near_state[ii] = 0;//far
        }
    }

    //====================================================================
    for(ii = 0; ii < HX9023S_CH_NUM; ii++) { //标记每个通道由远离变为接近
        if((0 == ((hx9023s_pdata.prox_state_reg_pre >> ii) & 0x1)) && (1 == ((hx9023s_pdata.prox_state_reg >> ii) & 0x1))) {
            hx9023s_pdata.prox_state_far_to_near_flag |= (1 << ii);
        } else {
            hx9023s_pdata.prox_state_far_to_near_flag &= ~(1 << ii);
        }
    }
    hx9023s_pdata.prox_state_reg_pre = hx9023s_pdata.prox_state_reg;//保存上次的接近状态
    hx9023s_pdata.prox_state_cmp_pre = hx9023s_pdata.prox_state_cmp;//保存上次的接近状态
    //====================================================================

    PRINT_INF("prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9023s_ch_near_state ch0_2:%d-%d-%d, far_to_near_flag=0x%02X\n",
              hx9023s_pdata.prox_state_reg,
              hx9023s_pdata.prox_state_cmp,
              hx9023s_ch_near_state[0],
              hx9023s_ch_near_state[1],
              hx9023s_ch_near_state[2],
              hx9023s_pdata.prox_state_far_to_near_flag);
}

static void hx9023s_sample_16(void)//输出raw bl diff lp offse五组数据，默认lp,bl(diff=lp-bl)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9023S_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    uint8_t val_38_buf[1] = {0};
    int32_t data[HX9023S_CH_NUM] = {0};

    hx9023s_data_lock(HX9023S_DATA_LOCK);
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;

    ret = hx9023s_read(RO_E8_RAW_BL_CH0_0, rx_buf, bytes_all_channels);
    ret |= hx9023s_read(RW_38_RAW_BL_RD_CFG, val_38_buf, 1);

    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));//24位数据舍去低8位，保留高16位，下同
            if(((val_38_buf[0] >> ii) & 0x10) == 0x10){
                 data_raw[ii] = 0;
                 data_bl[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
            } else if(((val_38_buf[0] >> ii) & 0x10) == 0x00){
                 data_raw[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                 data_bl[ii] = 0;
            }
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;
    ret = hx9023s_read(RO_F4_LP_DIFF_CH0_0, rx_buf, bytes_all_channels);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 8) | (rx_buf[ii * bytes_per_channel + 1]));
            if(((val_38_buf[0] >> ii) & 0x01) == 0x01){
                   data_diff[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                   data_lp[ii] = 0;
            } else if(((val_38_buf[0] >> ii) & 0x01) == 0x00){
                   data_lp[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                   if(((val_38_buf[0] >> ii) & 0x10) == 0x10){
                      data_diff[ii] = data_lp[ii] - data_bl[ii];
                   } else{
                     data_diff[ii] = 0;
                   }
            }
        }
    }

    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;
    ret = hx9023s_read(RW_15_OFFSET_DAC0_7_0, rx_buf, bytes_all_channels);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = (data[ii] & 0xFFF);//12位
            data_offset_dac[ii] = data[ii];
        }
    }
    //====================================================================================================
    hx9023s_data_lock(HX9023S_DATA_UNLOCK);
}

static void hx9023s_sample_24(void)//输出raw bl diff lp offse五组数据，默认lp,bl(diff=lp-bl)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9023S_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    uint8_t val_38_buf[1] = {0};
    int32_t data[HX9023S_CH_NUM] = {0};

    hx9023s_data_lock(HX9023S_DATA_LOCK);
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;

    ret = hx9023s_read(RO_E8_RAW_BL_CH0_0, rx_buf, bytes_all_channels);
    ret |= hx9023s_read(RW_38_RAW_BL_RD_CFG, val_38_buf, 1);

    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
                data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 16) | (rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
                if(((val_38_buf[0] >> ii) & 0x10) == 0x10){
                     data_raw[ii] = 0;
                     data_bl[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                } else if(((val_38_buf[0] >> ii) & 0x10) == 0x00){
                     data_raw[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                     data_bl[ii] = 0;
                }
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;
    ret = hx9023s_read(RO_F4_LP_DIFF_CH0_0, rx_buf, bytes_all_channels);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
                data[ii] = ((rx_buf[ii * bytes_per_channel + 2] << 16) | (rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
                if(((val_38_buf[0] >> ii) & 0x01) == 0x01){
                       data_diff[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                       data_lp[ii] = 0;
                } else if(((val_38_buf[0] >> ii) & 0x01) == 0x00){
                       data_lp[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                       if(((val_38_buf[0] >> ii) & 0x10) == 0x10){
                          data_diff[ii] = data_lp[ii] - data_bl[ii];
                       } else{
                          data_diff[ii] = 0;
                       }
                }
        }
    }

    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9023S_CH_NUM * bytes_per_channel;
    ret = hx9023s_read(RW_15_OFFSET_DAC0_7_0, rx_buf, bytes_all_channels);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }
    if(0 == ret) {
        for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = (data[ii] & 0xFFF);//12位
            data_offset_dac[ii] = data[ii];
        }
    }
    //====================================================================================================
    hx9023s_data_lock(HX9023S_DATA_UNLOCK);
}

static void hx9023s_sample(void)
{
    if(24 == hx9023s_data_accuracy) {
        hx9023s_sample_24();
    } else {
        hx9023s_sample_16();
    }
    PRINT_DBG("accuracy=%d\n", hx9023s_data_accuracy);
    PRINT_DBG("DIFF  , %-8d, %-8d, %-8d\n", data_diff[0], data_diff[1], data_diff[2]);
    PRINT_DBG("RAW   , %-8d, %-8d, %-8d\n", data_raw[0], data_raw[1], data_raw[2]);
    PRINT_DBG("OFFSET, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2]);
    PRINT_DBG("BL    , %-8d, %-8d, %-8d\n", data_bl[0], data_bl[1], data_bl[2]);
    PRINT_DBG("LP    , %-8d, %-8d, %-8d\n", data_lp[0], data_lp[1], data_lp[2]);
}

#if HX9023S_REPORT_EVKEY
static void hx9023s_input_report_key(void)
{
    int ii = 0;
    uint8_t touch_state = 0;

    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if (false == hx9023s_pdata.chs_info[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9023s_pdata.chs_info[ii].name);
            continue;
        }
        if (false == hx9023s_pdata.chs_info[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9023s_pdata.chs_info[ii].name);
            continue;
        }

        //上报数据时采用何种状态判断依据，以下3选1
        //touch_state = hx9023s_ch_near_state[ii];
        touch_state = (hx9023s_pdata.prox_state_reg >> ii) & 0x1;
        //touch_state = (hx9023s_pdata.prox_state_cmp >> ii) & 0x1;

        if (PROXACTIVE == touch_state) {
            if (hx9023s_pdata.chs_info[ii].state == PROXACTIVE)
                PRINT_DBG("%s already PROXACTIVE, nothing report\n", hx9023s_pdata.chs_info[ii].name);
            else {
#if 0
                __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif
                input_event(hx9023s_pdata.input_dev_key, EV_KEY, hx9023s_pdata.chs_info[ii].keycode, PROXACTIVE);
                hx9023s_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9023s_pdata.chs_info[ii].name);
            }
        } else if (IDLE == touch_state) {
            if (hx9023s_pdata.chs_info[ii].state == IDLE)
                PRINT_DBG("%s already released, nothing report\n", hx9023s_pdata.chs_info[ii].name);
            else {
#if 0
                __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif
                input_event(hx9023s_pdata.input_dev_key, EV_KEY, hx9023s_pdata.chs_info[ii].keycode, IDLE);
                hx9023s_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9023s_pdata.chs_info[ii].name);
            }
        } else {
            PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
        }
    }
    input_sync(hx9023s_pdata.input_dev_key);
}

#else

static void hx9023s_input_report_abs(void)
{
    int ii = 0;
    uint8_t touch_state = 0;

    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if (false == hx9023s_pdata.chs_info[ii].enabled) {
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9023s_pdata.chs_info[ii].name);
            continue;
        }
        if (false == hx9023s_pdata.chs_info[ii].used) {
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9023s_pdata.chs_info[ii].name);
            continue;
        }

        //上报数据时采用何种状态判断依据，以下3选1
        //touch_state = hx9023s_ch_near_state[ii];
        touch_state = (hx9023s_pdata.prox_state_reg >> ii) & 0x1;
        //touch_state = (hx9023s_pdata.prox_state_cmp >> ii) & 0x1;

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
        if(hx9023s_pdata.power_state){
            if (hx9023s_pdata.chs_info[ii].state == IDLE)
                PRINT_DBG("[power_enable]:%s already released, nothing report\n", hx9023s_pdata.chs_info[ii].name);
            else{
                wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_MISC, SAR_STATE_FAR);
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_sync(hx9023s_pdata.chs_info[ii].input_dev_abs);
                hx9023s_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("[power_enable]:%s report released\n", hx9023s_pdata.chs_info[ii].name);
            }
            continue;
        }
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
        if (PROXACTIVE == touch_state) {
            if (hx9023s_pdata.chs_info[ii].state == PROXACTIVE)
                PRINT_DBG("%s already PROXACTIVE, nothing report\n", hx9023s_pdata.chs_info[ii].name);
            else {
#if 0
                __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_MISC, SAR_STATE_NEAR);
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_sync(hx9023s_pdata.chs_info[ii].input_dev_abs);
                hx9023s_pdata.chs_info[ii].state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE\n", hx9023s_pdata.chs_info[ii].name);
            }
        } else if (IDLE == touch_state) {
            if (hx9023s_pdata.chs_info[ii].state == IDLE)
                PRINT_DBG("%s already released, nothing report\n", hx9023s_pdata.chs_info[ii].name);
            else {
#if 0
                __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_MISC, SAR_STATE_FAR);
                input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_sync(hx9023s_pdata.chs_info[ii].input_dev_abs);
                hx9023s_pdata.chs_info[ii].state = IDLE;
                PRINT_DBG("%s report released\n", hx9023s_pdata.chs_info[ii].name);
            }
        } else {
            PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
        }
    }
}
#endif

// 必要DTS属性：
// "tyhx,irq-gpio"      必要！中断对应的gpio number
// "tyhx,channel-flag"  必要！每个bit对应一个used channel,如0xF代表0,1,2,3通道可用
static int hx9023s_parse_dt(struct device *dev)
{
    int ret = -1;
    struct device_node *dt_node = dev->of_node;

    if (NULL == dt_node) {
        PRINT_ERR("No DTS node\n");
        return -ENODEV;
    }

#if HX9023S_TEST_ON_MTK_DEMO_XY6761
    ret = of_property_read_u32(dt_node, "xy6761-fake-irq-gpio", &hx9023s_pdata.irq_gpio);
    if(ret < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#else
    hx9023s_pdata.irq_gpio = of_get_named_gpio_flags(dt_node, "tyhx,irq-gpio", 0, NULL);
    if(hx9023s_pdata.irq_gpio < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#endif

    PRINT_INF("hx9023s_pdata.irq_gpio=%d\n", hx9023s_pdata.irq_gpio);

    hx9023s_pdata.channel_used_flag = 0x07;
    ret = of_property_read_u32(dt_node, "tyhx,channel-flag", &hx9023s_pdata.channel_used_flag);//客户配置：有效通道标志位：9023S最大传入0x1F
    if(ret < 0) {
        PRINT_ERR("\"tyhx,channel-flag\" is not set in DT\n");
        return -1;
    }
    if(hx9023s_pdata.channel_used_flag > ((1 << HX9023S_CH_NUM) - 1)) {
        PRINT_ERR("the max value of channel_used_flag is 0x%X\n", ((1 << HX9023S_CH_NUM) - 1));
        return -1;
    }
    PRINT_INF("hx9023s_pdata.channel_used_flag=0x%X\n", hx9023s_pdata.channel_used_flag);

    return 0;
}

static int hx9023s_gpio_init(void)
{
    int ret = -1;
    if (gpio_is_valid(hx9023s_pdata.irq_gpio)) {
        ret = gpio_request(hx9023s_pdata.irq_gpio, "hx9023s_irq_gpio");
        if (ret < 0) {
            PRINT_ERR("gpio_request failed. ret=%d\n", ret);
            return ret;
        }
        ret = gpio_direction_input(hx9023s_pdata.irq_gpio);
        if (ret < 0) {
            PRINT_ERR("gpio_direction_input failed. ret=%d\n", ret);
            gpio_free(hx9023s_pdata.irq_gpio);
            return ret;
        }
    } else {
        PRINT_ERR("Invalid gpio num\n");
        return -1;
    }

    hx9023s_pdata.irq = gpio_to_irq(hx9023s_pdata.irq_gpio);
    PRINT_INF("hx9023s_pdata.irq_gpio=%d hx9023s_pdata.irq=%d\n", hx9023s_pdata.irq_gpio, hx9023s_pdata.irq);
    return 0;
}

static void hx9023s_gpio_deinit(void)
{
    ENTER;
    gpio_free(hx9023s_pdata.irq_gpio);
}

static void hx9023s_power_on(uint8_t on)
{
    if(on) {
        //TODO: 用户自行填充
        PRINT_INF("power on\n");
    } else {
        //TODO: 用户自行填充
        PRINT_INF("power off\n");
    }
}

//+ add function to open and close sar channels separately for wt factory test need, liuling, 20230511, start
#if HX9023S_TEST_CHS_EN //通道测试时用全开全关策略
//- add function to open and close sar channels separately for wt factory test need, liuling, 20230511, end
static int hx9023s_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = 0;
    uint8_t tx_buf[1] = {0};

    if(ch_enable_status > 0) {
        if(1 == en) {
            ch_enable_status |= (1 << ch_id);
            //hx9023s_threshold_int_en(ch_id, en);
            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            ch_enable_status &= ~(1 << ch_id);
            //hx9023s_threshold_int_en(ch_id, en);
            if(0 == ch_enable_status) {
                tx_buf[0] = 0x00;
                ret = hx9023s_write(RW_24_CH_NUM_CFG, tx_buf, 1);
                if(0 != ret) {
                    PRINT_ERR("hx9023s_write failed\n");
                    return -1;
                }
                PRINT_INF("ch_%d disabled, all channels disabled\n", ch_id);
            } else {
                PRINT_INF("ch_%d disabled\n", ch_id);
            }
        }
    } else {
        if(1 == en) {
            hx9023s_pdata.prox_state_reg = 0;
            hx9023s_pdata.prox_state_cmp = 0;
            hx9023s_pdata.prox_state_reg_pre = 0;
            hx9023s_pdata.prox_state_cmp_pre = 0;
            hx9023s_pdata.prox_state_far_to_near_flag = 0;

            tx_buf[0] = hx9023s_pdata.channel_used_flag;
            ret = hx9023s_write(RW_24_CH_NUM_CFG, tx_buf, 1);
            if(0 != ret) {
                PRINT_ERR("hx9023s_write failed\n");
                return -1;
            }
            ch_enable_status |= (1 << ch_id);
            //hx9023s_threshold_int_en(ch_id, en);
            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            PRINT_INF("all channels disabled already\n");
        }
    }

    return ret;
}
//+ add function to open and close sar channels separately for wt factory test need, liuling, 20230511, start
#else

static int hx9023s_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};
    uint8_t tx_buf[1] = {0};

    ret = hx9023s_read(RW_24_CH_NUM_CFG, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
        return -1;
    }
    ch_enable_status = rx_buf[0];

    if(ch_id >= HX9023S_CH_NUM) {
        PRINT_ERR("channel index over range !!!ch_enable_status=0x%02X (ch_id=%d, en=%d)\n", ch_enable_status, ch_id, en);
        return -1;
    }

    if(1 == en) {
        if(0 == ch_enable_status) { //开启第一个ch
            hx9023s_pdata.prox_state_reg = 0;
            hx9023s_pdata.prox_state_cmp = 0;
            hx9023s_pdata.prox_state_reg_pre = 0;
            hx9023s_pdata.prox_state_cmp_pre = 0;
            hx9023s_pdata.prox_state_far_to_near_flag = 0;
        }

        ch_enable_status |= (1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9023s_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
        msleep(HX9023S_ODR_MS);
    } else {
        en = 0;
        ch_enable_status &= ~(1 << ch_id);
        if(0 == (ch_enable_status & 0x07)) { //关闭最后一个ch
        }

        tx_buf[0] = ch_enable_status;
        ret = hx9023s_write(RW_24_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
    }
    return 0;
}
#endif
//- add function to open and close sar channels separately for wt factory test need, liuling, 20230511, end

static int hx9023s_ch_en_hal(uint8_t ch_id, uint8_t enable)//yasin: for upper layer
{
    int ret = -1;

    mutex_lock(&hx9023s_ch_en_mutex);
    if (1 == enable) {
        PRINT_INF("enable ch_%d(name:%s)\n", ch_id, hx9023s_pdata.chs_info[ch_id].name);
        ret = hx9023s_ch_en(ch_id, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_ch_en failed\n");
            mutex_unlock(&hx9023s_ch_en_mutex);
            return -1;
        }
        hx9023s_pdata.chs_info[ch_id].state = IDLE;
        hx9023s_pdata.chs_info[ch_id].enabled = true;

#if 0
        __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
        wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif

#if HX9023S_REPORT_EVKEY
        input_event(hx9023s_pdata.input_dev_key, EV_KEY, hx9023s_pdata.chs_info[ch_id].keycode, IDLE);
        input_sync(hx9023s_pdata.input_dev_key);
#else
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_report_rel(hx9023s_pdata.chs_info[ch_id].input_dev_abs, REL_MISC, SAR_STATE_FAR);
        input_report_rel(hx9023s_pdata.chs_info[ch_id].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_sync(hx9023s_pdata.chs_info[ch_id].input_dev_abs);
#endif
    } else if (0 == enable) {
        PRINT_INF("disable ch_%d(name:%s)\n", ch_id, hx9023s_pdata.chs_info[ch_id].name);
        ret = hx9023s_ch_en(ch_id, 0);
        if(0 != ret) {
            PRINT_ERR("hx9023s_ch_en failed\n");
            mutex_unlock(&hx9023s_ch_en_mutex);
            return -1;
        }
        hx9023s_pdata.chs_info[ch_id].state = IDLE;
        hx9023s_pdata.chs_info[ch_id].enabled = false;

#if 0
        __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
        wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif

#if HX9023S_REPORT_EVKEY
        input_event(hx9023s_pdata.input_dev_key, EV_KEY, hx9023s_pdata.chs_info[ch_id].keycode, -1);
        input_sync(hx9023s_pdata.input_dev_key);
#else
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_report_rel(hx9023s_pdata.chs_info[ch_id].input_dev_abs, REL_MISC, -1);
        input_report_rel(hx9023s_pdata.chs_info[ch_id].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_sync(hx9023s_pdata.chs_info[ch_id].input_dev_abs);
#endif
    } else {
        PRINT_ERR("unknown enable symbol\n");
    }
    mutex_unlock(&hx9023s_ch_en_mutex);

    return 0;
}


static void hx9023s_polling_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9023s_ch_en_mutex);
    hx9023s_sample();
    hx9023s_get_prox_state(data_diff, HX9023S_CH_NUM);

#if HX9023S_REPORT_EVKEY
    hx9023s_input_report_key();
#else
    hx9023s_input_report_abs();
#endif

    if(1 == hx9023s_polling_enable)
        schedule_delayed_work(&hx9023s_pdata.polling_work, msecs_to_jiffies(hx9023s_polling_period_ms));
    mutex_unlock(&hx9023s_ch_en_mutex);
    return;
}

static irqreturn_t hx9023s_irq_handler(int irq, void *pvoid)
{
    ENTER;
    mutex_lock(&hx9023s_ch_en_mutex);
    if(1 == hx9023s_irq_from_suspend_flag) {
        hx9023s_irq_from_suspend_flag = 0;
        PRINT_INF("delay 50ms for waiting the i2c controller enter working mode\n");
        msleep(50);//如果从suspend被中断唤醒，该延时确保i2c控制器也从休眠唤醒并进入工作状态
    }
    hx9023s_sample();
    hx9023s_get_prox_state(data_diff, HX9023S_CH_NUM);

#if HX9023S_REPORT_EVKEY
    hx9023s_input_report_key();
#else
    hx9023s_input_report_abs();
#endif

    mutex_unlock(&hx9023s_ch_en_mutex);
    return IRQ_HANDLED;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test begin
static ssize_t hx9023s_raw_data_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *p = buf;
    int ii = 0;

    ENTER;
    hx9023s_sample();
    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        p += snprintf(p, PAGE_SIZE, "ch[%d]: DIFF=%-8d, RAW=%-8d, OFFSET=%-8d, BL=%-8d, LP=%-8d\n",
                      ii, data_diff[ii], data_raw[ii], data_offset_dac[ii], data_bl[ii], data_lp[ii]);
    }
    return (p - buf);//返回实际放到buf中的实际字符个数
}

static ssize_t hx9023s_reg_write_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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

    ret = hx9023s_write(addr, tx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_write failed\n");
    }

    PRINT_INF("WRITE:Reg0x%02X=0x%02X\n", addr, tx_buf[0]);
    return count;
}

static ssize_t hx9023s_reg_read_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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

    ret = hx9023s_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9023s_read failed\n");
    }

    PRINT_INF("READ:Reg0x%02X=0x%02X\n", addr, rx_buf[0]);
    return count;
}

static ssize_t hx9023s_channel_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ch_id = 0;
    int en = 0;

    ENTER;
    if (sscanf(buf, "%d,%d", &ch_id, &en) != 2) {
        PRINT_ERR("please input two DEC numbers: ch_id,en (ch_id: channel number, en: 1=enable, 0=disable)\n");
        return -EINVAL;
    }

    if((ch_id >= HX9023S_CH_NUM) || (ch_id < 0)) {
        PRINT_ERR("channel number out of range, the effective number is 0~%d\n", HX9023S_CH_NUM - 1);
        return -EINVAL;
    }

    if ((hx9023s_pdata.channel_used_flag >> ch_id) & 0x01) {
        hx9023s_ch_en_hal(ch_id, (en > 0) ? 1 : 0);
    } else {
        PRINT_ERR("ch_%d is unused, you can not enable or disable an unused channel\n", ch_id);
    }

    return count;
}

static ssize_t hx9023s_channel_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    ENTER;
    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if ((hx9023s_pdata.channel_used_flag >> ii) & 0x1) {
            PRINT_INF("hx9023s_pdata.chs_info[%d].enabled=%d\n", ii, hx9023s_pdata.chs_info[ii].enabled);
            p += snprintf(p, PAGE_SIZE, "hx9023s_pdata.chs_info[%d].enabled=%d\n", ii, hx9023s_pdata.chs_info[ii].enabled);
        }
    }

    return (p - buf);
}

static ssize_t hx9023s_manual_offset_calibration_show(struct class *class, struct class_attribute *attr, char *buf)
{
    hx9023s_read_offset_dac();
    return sprintf(buf, "OFFSET_DAC, %-8d, %-8d, %-8d\n", data_offset_dac[0], data_offset_dac[1], data_offset_dac[2]);
}

static ssize_t hx9023s_manual_offset_calibration_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned long val;

    ENTER;
    if (kstrtoul(buf, 10, &val)) {
        PRINT_ERR("Invalid Argument\n");
        return -EINVAL;
    }

    if (0 != val)
        hx9023s_manual_offset_calibration();
    else
        PRINT_ERR("echo 1 to calibrate\n");
    return count;
}

static ssize_t hx9023s_prox_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_DBG("prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9023s_ch_near_state ch0_2:%d-%d-%d, far_to_near_flag=0x%02X\n",
              hx9023s_pdata.prox_state_reg,
              hx9023s_pdata.prox_state_cmp,
              hx9023s_ch_near_state[0],
              hx9023s_ch_near_state[1],
              hx9023s_ch_near_state[2],
              hx9023s_pdata.prox_state_far_to_near_flag);

    return sprintf(buf, "prox_state_reg=0x%02X, prox_state_cmp=0x%02X, hx9023s_ch_near_state ch0_2:%d-%d-%d, far_to_near_flag=0x%02X\n",
                   hx9023s_pdata.prox_state_reg,
                   hx9023s_pdata.prox_state_cmp,
                   hx9023s_ch_near_state[0],
                   hx9023s_ch_near_state[1],
                   hx9023s_ch_near_state[2],
                   hx9023s_pdata.prox_state_far_to_near_flag);
}

static ssize_t hx9023s_polling_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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
        hx9023s_polling_period_ms = value;
        if(1 == hx9023s_polling_enable) {
            PRINT_INF("polling is already enabled!, no need to do enable again!, just update the polling period\n");
            goto exit;
        }

        hx9023s_polling_enable = 1;
        hx9023s_disable_irq(hx9023s_pdata.irq);//关闭中断，并停止中断底半部对应的工作队列

        PRINT_INF("polling started! period=%dms\n", hx9023s_polling_period_ms);
        schedule_delayed_work(&hx9023s_pdata.polling_work, msecs_to_jiffies(hx9023s_polling_period_ms));
    } else {
        if(0 == hx9023s_polling_enable) {
            PRINT_INF("polling is already disabled!, no need to do again!\n");
            goto exit;
        }
        hx9023s_polling_period_ms = 0;
        hx9023s_polling_enable = 0;
        PRINT_INF("polling stoped!\n");

        cancel_delayed_work(&hx9023s_pdata.polling_work);//停止polling对应的工作队列，并重新开启中断模式
        hx9023s_enable_irq(hx9023s_pdata.irq);
    }

exit:
    return count;
}

static ssize_t hx9023s_polling_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9023s_polling_enable=%d hx9023s_polling_period_ms=%d\n", hx9023s_polling_enable, hx9023s_polling_period_ms);
    return sprintf(buf, "hx9023s_polling_enable=%d hx9023s_polling_period_ms=%d\n", hx9023s_polling_enable, hx9023s_polling_period_ms);
}

static ssize_t hx9023s_loglevel_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("tyhx_log_level=%d\n", tyhx_log_level);
    return sprintf(buf, "tyhx_log_level=%d\n", tyhx_log_level);
}

static ssize_t hx9023s_loglevel_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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

static ssize_t hx9023s_accuracy_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9023s_data_accuracy=%d\n", hx9023s_data_accuracy);
    return sprintf(buf, "hx9023s_data_accuracy=%d\n", hx9023s_data_accuracy);
}

static ssize_t hx9023s_accuracy_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9023s_data_accuracy = (24 == value) ? 24 : 16;
    PRINT_INF("set hx9023s_data_accuracy=%d\n", hx9023s_data_accuracy);
    return count;
}

static ssize_t hx9023s_threshold_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int ch = 0;
    unsigned int thr_near = 0;
    unsigned int thr_far = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d", &ch, &thr_near, &thr_far) != 3) {
        PRINT_ERR("please input 3 numbers in DEC: ch,thr_near,thr_far (eg: 0,500,300)\n");
        return -EINVAL;
    }

    if(ch >= HX9023S_CH_NUM || thr_near > (0x03FF * 32) || thr_far > thr_near) {
        PRINT_ERR("input value over range! (valid value: ch=%d, thr_near=%d, thr_far=%d)\n", ch, thr_near, thr_far);
        return -EINVAL;
    }

    thr_near = (thr_near / 32) * 32;
    thr_far = (thr_far / 32) * 32;

    PRINT_INF("set threshold: ch=%d, thr_near=%d, thr_far=%d\n", ch, thr_near, thr_far);
    hx9023s_set_thres_near(ch, thr_near);
    hx9023s_set_thres_far(ch, thr_far);

    return count;
}

static ssize_t hx9023s_threshold_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        hx9023s_get_thres_near(ii);
        hx9023s_get_thres_far(ii);
        PRINT_INF("ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9023s_ch_thres[ii].thr_near, hx9023s_ch_thres[ii].thr_far);
        p += snprintf(p, PAGE_SIZE, "ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9023s_ch_thres[ii].thr_near, hx9023s_ch_thres[ii].thr_far);
    }

    return (p - buf);
}

static ssize_t hx9023s_dump_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ret = -1;
    int ii = 0;
    char *p = buf;
    uint8_t rx_buf[1] = {0};

    for(ii = 0; ii < (int)ARRAY_SIZE(hx9023s_reg_init_list); ii++) {
        ret = hx9023s_read(hx9023s_reg_init_list[ii].addr, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9023s_read failed\n");
        }
        PRINT_INF("0x%02X=0x%02X\n", hx9023s_reg_init_list[ii].addr, rx_buf[0]);
        p += snprintf(p, PAGE_SIZE, "0x%02X=0x%02X\n", hx9023s_reg_init_list[ii].addr, rx_buf[0]);
    }

    p += snprintf(p, PAGE_SIZE, "driver version:%s\n", HX9023S_DRIVER_VER);
    return (p - buf);
}

static ssize_t hx9023s_offset_dac_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    hx9023s_read_offset_dac();

    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        PRINT_INF("data_offset_dac[%d]=%dpF\n", ii, data_offset_dac[ii] * 58 / 1000);
        p += snprintf(p, PAGE_SIZE, "ch[%d]=%dpF ", ii, data_offset_dac[ii] * 58 / 1000);
    }
    p += snprintf(p, PAGE_SIZE, "\n");

    return (p - buf);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_raw_data = __ATTR(raw_data, 0664, hx9023s_raw_data_show, NULL);
static struct class_attribute class_attr_reg_write = __ATTR(reg_write,  0664, NULL, hx9023s_reg_write_store);
static struct class_attribute class_attr_reg_read = __ATTR(reg_read, 0664, NULL, hx9023s_reg_read_store);
static struct class_attribute class_attr_channel_en = __ATTR(channel_en, 0664, hx9023s_channel_en_show, hx9023s_channel_en_store);
static struct class_attribute class_attr_calibrate = __ATTR(calibrate, 0664, hx9023s_manual_offset_calibration_show, hx9023s_manual_offset_calibration_store);
static struct class_attribute class_attr_prox_state = __ATTR(prox_state, 0664, hx9023s_prox_state_show, NULL);
static struct class_attribute class_attr_polling_period = __ATTR(polling_period, 0664, hx9023s_polling_show, hx9023s_polling_store);
static struct class_attribute class_attr_threshold = __ATTR(threshold, 0664, hx9023s_threshold_show, hx9023s_threshold_store);
static struct class_attribute class_attr_loglevel = __ATTR(loglevel, 0664, hx9023s_loglevel_show, hx9023s_loglevel_store);
static struct class_attribute class_attr_accuracy = __ATTR(accuracy, 0664, hx9023s_accuracy_show, hx9023s_accuracy_store);
static struct class_attribute class_attr_dump = __ATTR(dump, 0664, hx9023s_dump_show, NULL);
static struct class_attribute class_attr_offset_dac = __ATTR(offset_dac, 0664, hx9023s_offset_dac_show, NULL);

static struct attribute *hx9023s_class_attrs[] = {
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
ATTRIBUTE_GROUPS(hx9023s_class);
#else
static struct class_attribute hx9023s_class_attributes[] = {
    __ATTR(raw_data, 0664, hx9023s_raw_data_show, NULL),
    __ATTR(reg_write,  0664, NULL, hx9023s_reg_write_store),
    __ATTR(reg_read, 0664, NULL, hx9023s_reg_read_store),
    __ATTR(channel_en, 0664, hx9023s_channel_en_show, hx9023s_channel_en_store),
    __ATTR(calibrate, 0664, hx9023s_manual_offset_calibration_show, hx9023s_manual_offset_calibration_store),
    __ATTR(prox_state, 0664, hx9023s_prox_state_show, NULL),
    __ATTR(polling_period, 0664, hx9023s_polling_show, hx9023s_polling_store),
    __ATTR(threshold, 0664, hx9023s_threshold_show, hx9023s_threshold_store),
    __ATTR(loglevel, 0664, hx9023s_loglevel_show, hx9023s_loglevel_store),
    __ATTR(accuracy, 0664, hx9023s_accuracy_show, hx9023s_accuracy_store),
    __ATTR(dump, 0664, hx9023s_dump_show, NULL),
    __ATTR(offset_dac, 0664, hx9023s_offset_dac_show, NULL),
    __ATTR_NULL,
};
#endif

struct class hx9023s_class = {
        .name = "hx9023s",
        .owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups = hx9023s_class_groups,
#else
        .class_attrs = hx9023s_class_attributes,
#endif
    };
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test end
// + wt add Adaptive ss sensor hal, wangjianlong 20230413, start
static ssize_t show_enable(struct device *dev,
						   struct device_attribute *attr,
						   char *buf)
{
	int status = 0;
	char *p = buf;
    struct input_dev* temp_input_dev;

	temp_input_dev = container_of(dev,struct input_dev,dev);
    PRINT_INF("%s: dev->name:%s\n", __func__, temp_input_dev->name);

    for (size_t index = 0; index < HX9023S_CH_NUM; index++) {
        if (!strcmp(temp_input_dev->name, hx9023_channels[index].name)) {
            if (true == hx9023s_pdata.chs_info[index].enabled)
                status = 1;
            break;
        }
    }
    p += snprintf(p, PAGE_SIZE, "%d", status);
    return (p-buf);
}
static ssize_t store_enable(struct device *dev,
							struct device_attribute *attr,
							const char *buf, size_t count)
{
    struct input_dev* temp_input_dev;
    bool val = simple_strtol(buf, NULL, 10) ? 1 : 0;

    temp_input_dev = container_of(dev, struct input_dev, dev);
    PRINT_INF("%s: dev->name:%s:%s\n", __func__, temp_input_dev->name, buf);

    mutex_lock(&hx9023s_ch_en_mutex);
    for (size_t index = 0; index < HX9023S_CH_NUM; index++) {
        if (!strcmp(temp_input_dev->name, hx9023_channels[index].name)) {
            if (0 != hx9023s_ch_en(index, val)) {
                PRINT_ERR("hx9023_ch%d_en failed\n",  index);
                mutex_unlock(&hx9023s_ch_en_mutex);
                return -1;
            }
//+S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
            if (0 == index) {
                if (0 != hx9023s_ch_en(index + 2, val)) {
                    PRINT_ERR("hx9023_ch%d_en failed\n", index+2);
                    mutex_unlock(&hx9023s_ch_en_mutex);
                    return -1;
                }
            }
//-S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
#if 0
            __pm_wakeup_event(&hx9023s_wake_lock, 1000);
#else
            wake_lock_timeout(&hx9023s_wake_lock, HZ * 1);
#endif

            if (val) {
                hx9023s_pdata.chs_info[index].state = IDLE;
                hx9023s_pdata.chs_info[index].enabled = true;
//+S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
                if (0 == index) {
                    hx9023s_pdata.chs_info[index + 2].enabled = true;
                }
//-S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_report_rel(hx9023s_pdata.chs_info[index].input_dev_abs, REL_MISC, SAR_STATE_FAR);
                input_report_rel(hx9023s_pdata.chs_info[index].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_sync(hx9023s_pdata.chs_info[index].input_dev_abs);
            } else {
                PRINT_INF("name:%s: disable\n", hx9023_channels[index].name);
                hx9023s_pdata.chs_info[index].state = IDLE;
                hx9023s_pdata.chs_info[index].enabled = false;
//+S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
                if (0 == index) {
                    hx9023s_pdata.chs_info[index + 2].enabled = false;
                }
//-S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_report_rel(hx9023s_pdata.chs_info[index].input_dev_abs, REL_MISC, SAR_STATE_FAR);
                input_report_rel(hx9023s_pdata.chs_info[index].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
                input_sync(hx9023s_pdata.chs_info[index].input_dev_abs);
            }
        }
    }
    mutex_unlock(&hx9023s_ch_en_mutex);
    return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);
// - wt add Adaptive ss sensor hal, wangjianlong 20230413, end

// + wt factory app test need, wangjianlong 20230413, start
static ssize_t cap_diff_dump_show(int ch, char *buf)
{
    ssize_t len = 0;

    hx9023s_read_offset_dac();
    len += snprintf(buf+len, PAGE_SIZE-len,
                    "CH%d_background_cap=%d;", ch, data_offset_dac[ch] * 58 / 1000);
    hx9023s_sample();
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d", ch, data_diff[ch]);
    
    return len;

}
static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                       char *buf)
{
    return cap_diff_dump_show(0, buf);    
}

static ssize_t ch1_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    return cap_diff_dump_show(1, buf);
}

static ssize_t ch2_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                       char *buf)
{
    return cap_diff_dump_show(2, buf);
}

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
	return sprintf(buf, "%d\n", hx9023s_pdata.power_state);
}
static ssize_t power_enable_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;

    ret = kstrtoint(buf, 10, &hx9023s_pdata.power_state);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
    }
    return count;
}
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch

#if USER_TEST
static ssize_t user_test_store(struct class *class,
                               struct class_attribute *attr,
                               const char *buf, size_t count)
{
    int ret;
    int val = 0;
    ret = kstrtoint(buf, 10, &val);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
    }

    PRINT_INF("hx9023s user_test val = %d\n", val);
    if (val) {
        hx9023s_manual_offset_calibration();
    }
    return count;
}
#endif

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch1_cap_diff_dump = __ATTR(ch1_cap_diff_dump, 0664, ch1_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0664, ch2_cap_diff_dump_show, NULL);
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if USER_TEST
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, NULL, user_test_store);
#endif

static struct attribute *hx9023_capsense_attrs[] = {
    &class_attr_ch0_cap_diff_dump.attr,
    &class_attr_ch1_cap_diff_dump.attr,
    &class_attr_ch2_cap_diff_dump.attr,
//+ add function to open and close sar channels separately for wt factory test need, liuling, 20230511, start
    &class_attr_channel_en.attr,
//- add function to open and close sar channels separately for wt factory test need, liuling, 20230511, end
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if USER_TEST
    &class_attr_user_test.attr,
#endif
    //reserve
    NULL,
};
ATTRIBUTE_GROUPS(hx9023_capsense);

static struct class capsense_class = {
    .name           = "capsense",
    .owner          = THIS_MODULE,
    .class_groups   = hx9023_capsense_groups,
};
// - wt factory app test need, wangjianlong 20230413, end

#if HX9023S_REPORT_EVKEY
static int hx9023s_input_init_key(struct i2c_client *client)
{
    int ii = 0;
    int ret = -1;

    hx9023s_pdata.chs_info = devm_kzalloc(&client->dev,
                                           sizeof(struct hx9023s_channel_info) * HX9023S_CH_NUM,
                                           GFP_KERNEL);
    if (NULL == hx9023s_pdata.chs_info) {
        PRINT_ERR("devm_kzalloc failed\n");
        ret = -ENOMEM;
        goto failed_devm_kzalloc;
    }

    hx9023s_pdata.input_dev_key = input_allocate_device();
    if (NULL == hx9023s_pdata.input_dev_key) {
        PRINT_ERR("input_allocate_device failed\n");
        ret = -ENOMEM;
        goto failed_input_allocate_device;
    }

    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        snprintf(hx9023s_pdata.chs_info[ii].name,
                 sizeof(hx9023s_pdata.chs_info[ii].name),
                 "hx9023s_key_ch%d",
                 ii);
        PRINT_DBG("name of ch_%d:\"%s\"\n", ii, hx9023s_pdata.chs_info[ii].name);
        hx9023s_pdata.chs_info[ii].used = false;
        hx9023s_pdata.chs_info[ii].enabled = false;
        hx9023s_pdata.chs_info[ii].keycode = KEY_1 + ii;
        if ((hx9023s_pdata.channel_used_flag >> ii) & 0x1) {
            hx9023s_pdata.chs_info[ii].used = true;
            hx9023s_pdata.chs_info[ii].state = IDLE;
            __set_bit(hx9023s_pdata.chs_info[ii].keycode, hx9023s_pdata.input_dev_key->keybit);
        }
    }

    hx9023s_pdata.input_dev_key->name = HX9023S_DRIVER_NAME;
    __set_bit(EV_KEY, hx9023s_pdata.input_dev_key->evbit);
    ret = input_register_device(hx9023s_pdata.input_dev_key);
    if(ret) {
        PRINT_ERR("input_register_device failed\n");
        goto failed_input_register_device;
    }

    PRINT_INF("input init success\n");
    return ret;

failed_input_register_device:
    input_free_device(hx9023s_pdata.input_dev_key);
failed_input_allocate_device:
    devm_kfree(&client->dev, hx9023s_pdata.chs_info);
failed_devm_kzalloc:
    PRINT_ERR("hx9023s_input_init_key failed\n");
    return ret;
}

static void hx9023s_input_deinit_key(struct i2c_client *client)
{
    ENTER;
    input_unregister_device(hx9023s_pdata.input_dev_key);
    input_free_device(hx9023s_pdata.input_dev_key);
    devm_kfree(&client->dev, hx9023s_pdata.chs_info);
}

#else

static int hx9023s_input_init_abs(struct i2c_client *client)
{
    int ii = 0;
    int jj = 0;
    int ret = -1;

    hx9023s_pdata.chs_info = devm_kzalloc(&client->dev,
                                           sizeof(struct hx9023s_channel_info) * HX9023S_CH_NUM,
                                           GFP_KERNEL);
    if (NULL == hx9023s_pdata.chs_info) {
        PRINT_ERR("devm_kzalloc failed\n");
        ret = -ENOMEM;
        goto failed_devm_kzalloc;
    }

    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
// - wt add Adaptive ss sensor hal, wangjianlong, 20230414, start
        snprintf(hx9023s_pdata.chs_info[ii].name,
                 sizeof(hx9023s_pdata.chs_info[ii].name),
                 hx9023_channels[ii].name);

// - wt add Adaptive ss sensor hal, wangjianlong, 20230414, end
        PRINT_DBG("name of ch_%d:\"%s\"\n", ii, hx9023s_pdata.chs_info[ii].name);
        hx9023s_pdata.chs_info[ii].used = false;
        hx9023s_pdata.chs_info[ii].enabled = false;

        hx9023s_pdata.chs_info[ii].input_dev_abs = input_allocate_device();
        if (NULL == hx9023s_pdata.chs_info[ii].input_dev_abs) {
            PRINT_ERR("input_allocate_device failed, ii=%d\n", ii);
            ret = -ENOMEM;
            goto failed_input_allocate_device;
        }

        hx9023s_pdata.chs_info[ii].input_dev_abs->name = hx9023s_pdata.chs_info[ii].name;
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        __set_bit(EV_REL, hx9023s_pdata.chs_info[ii].input_dev_abs->evbit);
        input_set_capability(hx9023s_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_MISC);
        __set_bit(EV_REL, hx9023s_pdata.chs_info[ii].input_dev_abs->evbit);
        input_set_capability(hx9023s_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_X);
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        __set_bit(EV_REL, hx9023s_pdata.chs_info[ii].input_dev_abs->evbit);
        input_set_capability(hx9023s_pdata.chs_info[ii].input_dev_abs, EV_REL, REL_MAX);
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */


        ret = input_register_device(hx9023s_pdata.chs_info[ii].input_dev_abs);
        if (ret) {
            PRINT_ERR("input_register_device failed, ii=%d\n", ii);
            goto failed_input_register_device;
        }
		
// + wt add Adaptive ss sensor hal, wangjianlong, 20230413, start
		if (0 != device_create_file(&hx9023s_pdata.chs_info[ii].input_dev_abs->dev, &dev_attr_enable)) {
			PRINT_INF("%s attribute ENABLE create fail\n", hx9023s_pdata.chs_info[ii].input_dev_abs->name);
		}
// - wt add Adaptive ss sensor hal, wangjianlong, 20230413, end

/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_MISC, -1);
        input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_X, 2);
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        input_sync(hx9023s_pdata.chs_info[ii].input_dev_abs);

        if ((hx9023s_pdata.channel_used_flag >> ii) & 0x1) {
            hx9023s_pdata.chs_info[ii].used = true;
            hx9023s_pdata.chs_info[ii].state = IDLE;
        }
    }

    PRINT_INF("input init success\n");
    return ret;

failed_input_register_device:
    for(jj = ii - 1; jj >= 0; jj--) {
        input_unregister_device(hx9023s_pdata.chs_info[jj].input_dev_abs);
    }
    ii++;
failed_input_allocate_device:
    for(jj = ii - 1; jj >= 0; jj--) {
        input_free_device(hx9023s_pdata.chs_info[jj].input_dev_abs);
    }
    devm_kfree(&client->dev, hx9023s_pdata.chs_info);
failed_devm_kzalloc:
    PRINT_ERR("hx9023s_input_init_abs failed\n");
    return ret;
}

static void hx9023s_input_deinit_abs(struct i2c_client *client)
{
    int ii = 0;

    ENTER;
    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        input_unregister_device(hx9023s_pdata.chs_info[ii].input_dev_abs);
        input_free_device(hx9023s_pdata.chs_info[ii].input_dev_abs);
    }
    devm_kfree(&client->dev, hx9023s_pdata.chs_info);
}

static int hx9023s_ch_en_classdev(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
    int ii = 0;

    ENTER;
    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if (strcmp(sensors_cdev->name, hx9023s_pdata.chs_info[ii].name) == 0) {
            hx9023s_ch_en_hal(ii, enable);
        }
    }
    return 0;
}

/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
static int hx9023s_flush_classdev(struct sensors_classdev *sensors_cdev, unsigned char flush)
{
    int ii = 0;

    ENTER;
    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if (sensors_cdev->type == hx9023_channels[ii].type) {
            input_report_rel(hx9023s_pdata.chs_info[ii].input_dev_abs, REL_MAX, flush);
            input_sync(hx9023s_pdata.chs_info[ii].input_dev_abs);
            break;
        }
    }
    return 0;
}
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/

static int hx9023s_classdev_init(void)
{
    int ii = 0;
    int jj = 0;
    int ret = -1;
    int type = 0;

    ENTER;
    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        type = ii ? SENSOR_TYPE_SAR_WCN : SENSOR_TYPE_SAR_MIAN ;
        hx9023s_pdata.chs_info[ii].classdev.sensors_enable = hx9023s_ch_en_classdev;
        hx9023s_pdata.chs_info[ii].classdev.sensors_poll_delay = NULL;
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        hx9023s_pdata.chs_info[ii].classdev.sensors_flush = hx9023s_flush_classdev;
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add flush node for ss sensor hal*/
        hx9023s_pdata.chs_info[ii].classdev.name = "HX9031";
        hx9023s_pdata.chs_info[ii].classdev.sensor_name = hx9023_channels[ii].name;
        hx9023s_pdata.chs_info[ii].classdev.name = "HX9031";
        hx9023s_pdata.chs_info[ii].classdev.vendor = HX9023S_DRIVER_NAME;
        hx9023s_pdata.chs_info[ii].classdev.version = 1;
        hx9023s_pdata.chs_info[ii].classdev.type = type;
        hx9023s_pdata.chs_info[ii].classdev.max_range = "5";
        hx9023s_pdata.chs_info[ii].classdev.resolution = "5.0";
        hx9023s_pdata.chs_info[ii].classdev.sensor_power = "3";
        hx9023s_pdata.chs_info[ii].classdev.min_delay = 0;
        hx9023s_pdata.chs_info[ii].classdev.fifo_reserved_event_count = 0;
        hx9023s_pdata.chs_info[ii].classdev.fifo_max_event_count = 0;
        hx9023s_pdata.chs_info[ii].classdev.delay_msec = 100;
        hx9023s_pdata.chs_info[ii].classdev.enabled = 0;

        ret = sensors_classdev_register(&hx9023s_pdata.chs_info[ii].input_dev_abs->dev,
                                        &hx9023s_pdata.chs_info[ii].classdev);
/* +S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        hx9023s_pdata.chs_info[ii].classdev.sensor_name = "HX9031";
/* -S96818AA1-6209, liuling3.wt,ADD, 2023/06/08, wt Adaptive sensor new hal */
        if (ret) {
            PRINT_ERR("sensors_classdev_register failed, ii=%d\n", ii);
            for(jj = ii - 1; jj >= 0; jj--) {
                sensors_classdev_unregister(&(hx9023s_pdata.chs_info[jj].classdev));
            }
            break;
        }
    }
    return ret;
}

static void hx9023s_classdev_deinit(void)
{
    int ii = 0;

    ENTER;
    for (ii = 0; ii < HX9023S_CH_NUM; ii++) {
        sensors_classdev_unregister(&(hx9023s_pdata.chs_info[ii].classdev));
    }
}
#endif

//+ distinguish sar driver for Australia, liuling, 20230419, start
static bool sar_get_boardid(void)
{
	char board_id[64];
	char *br_ptr;
	char *br_ptr_e;
	bool board_is_la = false;

	memset(board_id, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.board_id=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get board id */
		if (br_ptr_e != 0) {
			strncpy(board_id, br_ptr + 21,
					br_ptr_e - br_ptr - 21);
			board_id[br_ptr_e - br_ptr - 21] = '\0';
		}

		printk("hx9023s board_id = %s ", board_id);
        /* if it is LA board */
		if ((!strncmp(board_id, "S96818CA1",strlen("S96818CA1")))) {
		     board_is_la = true;
		}
	} else
        board_is_la = false;
	return board_is_la;

}
//- distinguish sar driver for Australia, liuling, 20230419, start



static int hx9023s_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int ii = 0;
    int ret = 0;

    PRINT_INF("i2c address:0x%02X\n", client->addr);
    if (!i2c_check_functionality(to_i2c_adapter(client->dev.parent), I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        PRINT_ERR("i2c_check_functionality failed\n");
        ret = -EIO;
        goto failed_i2c_check_functionality;
    }
    i2c_set_clientdata(client, &hx9023s_pdata);
    hx9023s_i2c_client = client;
    hx9023s_pdata.pdev = &client->dev;
    client->dev.platform_data = &hx9023s_pdata;

//{begin =============================================需要客户自行配置dts属性和实现上电相关内容
    ret = hx9023s_parse_dt(&client->dev);//yasin: power, irq, regs
    if (ret) {
        PRINT_ERR("hx9023s_parse_dt failed\n");
        ret = -ENODEV;
        goto failed_parse_dt;
    }

    ret = hx9023s_gpio_init();
    if (ret) {
        PRINT_ERR("hx9023s_gpio_init failed\n");
        ret = -1;
        goto failed_gpio_init;
    }

    client->irq = hx9023s_pdata.irq;
    hx9023s_power_on(1);
//}end =============================================================

    ret = hx9023s_id_check();
    if(0 != ret) {
        PRINT_INF("hx9023s_id_check failed, retry\n");
        if(0x28 == client->addr)
            client->addr = 0x2A;
        else
            client->addr = 0x28;
        PRINT_INF("i2c address:0x%02X\n", client->addr);
        ret = hx9023s_id_check();
        if(0 != ret) {
            PRINT_ERR("hx9023s_id_check failed\n");
            goto failed_id_check;
        }
    }

    hx9023s_reg_init();
    hx9023s_ch_cfg();

    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        hx9023s_set_thres_near(ii, hx9023s_ch_thres[ii].thr_near);
        hx9023s_set_thres_far(ii, hx9023s_ch_thres[ii].thr_far);
    }

    spin_lock_init(&hx9023s_pdata.lock);
    INIT_DELAYED_WORK(&hx9023s_pdata.polling_work, hx9023s_polling_work_func);
#if 0
    wakeup_source_init(&hx9023s_wake_lock, "hx9023s_wakelock");
#else
    wake_lock_init(&hx9023s_wake_lock, WAKE_LOCK_SUSPEND, "hx9023s_wakelock");
#endif

#if HX9023S_REPORT_EVKEY
    ret = hx9023s_input_init_key(client);
    if(0 != ret) {
        PRINT_ERR("hx9023s_input_init_key failed\n");
        goto failed_input_init;
    }
#else
    ret = hx9023s_input_init_abs(client);
    if(0 != ret) {
        PRINT_ERR("hx9023s_input_init_abs failed\n");
        goto failed_input_init;
    }

    ret = hx9023s_classdev_init();
    if(0 != ret) {
        PRINT_ERR("hx9023s_input_init_abs failed\n");
        goto failed_classdev_init;
    }
#endif

    ret = class_register(&hx9023s_class);//debug fs path:/sys/class/hx9023s/*
    if (ret < 0) {
        PRINT_ERR("class_register failed\n");
        goto failed_class_register;
    }

// + wt Adaptive factory app, wangjianlong, 20230413, start
    ret = class_register(&capsense_class);
    if (ret < 0) {
        PRINT_ERR("register capsense class failed (%d)\n", &capsense_class);
        return ret;
    }
// - wt Adaptive factory app, wangjianlong, 20230413, end

//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    hx9023s_pdata.power_state = 0;
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch

    ret = request_threaded_irq(hx9023s_pdata.irq, NULL, hx9023s_irq_handler,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                               hx9023s_pdata.pdev->driver->name, (&hx9023s_pdata));
    if(ret < 0) {
        PRINT_ERR("request_irq failed irq=%d ret=%d\n", hx9023s_pdata.irq, ret);
        goto failed_request_irq;
    }
    enable_irq_wake(hx9023s_pdata.irq);//enable irq wakeup PM
    hx9023s_irq_en_flag = 1;//irq is enabled after request by default

#if HX9023S_TEST_CHS_EN //enable channels for test
    PRINT_INF("enable all chs for test\n");
    for(ii = 0; ii < HX9023S_CH_NUM; ii++) {
        if ((hx9023s_pdata.channel_used_flag >> ii) & 0x1) {
            hx9023s_ch_en_hal(ii, 1);
        }
    }
#endif

    PRINT_INF("probe success\n");
    hardwareinfo_set_prop(HARDWARE_SAR, "hx9023s_sar");
    return 0;

failed_request_irq:
    class_unregister(&hx9023s_class);
failed_class_register:
#if HX9023S_REPORT_EVKEY
    hx9023s_input_deinit_key(client);
#else
    hx9023s_classdev_deinit();
failed_classdev_init:
    hx9023s_input_deinit_abs(client);
#endif
failed_input_init:
#if 0
    wakeup_source_trash(&hx9023s_wake_lock);
#else
    wake_lock_destroy(&hx9023s_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9023s_pdata.polling_work));
failed_id_check:
    hx9023s_power_on(0);
    hx9023s_gpio_deinit();
failed_gpio_init:
failed_parse_dt:
failed_i2c_check_functionality:
    PRINT_ERR("probe failed\n");
    return ret;
}

static int hx9023s_remove(struct i2c_client *client)
{
    ENTER;
    free_irq(hx9023s_pdata.irq, &hx9023s_pdata);
    class_unregister(&hx9023s_class);
#if HX9023S_REPORT_EVKEY
    hx9023s_input_deinit_key(client);
#else
    hx9023s_classdev_deinit();
    hx9023s_input_deinit_abs(client);
#endif
#if 0
    wakeup_source_trash(&hx9023s_wake_lock);
#else
    wake_lock_destroy(&hx9023s_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9023s_pdata.polling_work));
    hx9023s_power_on(0);
    hx9023s_gpio_deinit();
    return 0;
}

static int hx9023s_suspend(struct device *dev)
{
    ENTER;
    hx9023s_irq_from_suspend_flag = 1;
    return 0;
}

static int hx9023s_resume(struct device *dev)
{
    ENTER;
    hx9023s_irq_from_suspend_flag = 0;
    return 0;
}

static struct i2c_device_id hx9023s_i2c_id_table[] = {
    { HX9023S_DRIVER_NAME, 0 },
    { }
};

MODULE_DEVICE_TABLE(i2c, hx9023s_i2c_id_table);
#ifdef CONFIG_OF
static struct of_device_id hx9023s_of_match_table[] = {
#if HX9023S_TEST_ON_MTK_DEMO_XY6761
    {.compatible = "mediatek,sar_hx9023s"},
#else
    {.compatible = "tyhx,hx9023s"},
#endif
    { },
};
#else
#define hx9023s_of_match_table NULL
#endif

static const struct dev_pm_ops hx9023s_pm_ops = {
    .suspend = hx9023s_suspend,
    .resume = hx9023s_resume,
};

static struct i2c_driver hx9023s_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = HX9023S_DRIVER_NAME,
        .of_match_table = hx9023s_of_match_table,
        .pm = &hx9023s_pm_ops,
    },
    .id_table = hx9023s_i2c_id_table,
    .probe = hx9023s_probe,
    .remove = hx9023s_remove,
};

static int __init hx9023s_module_init(void)
{
    ENTER;
    PRINT_INF("driver version:%s\n", HX9023S_DRIVER_VER);

    return i2c_add_driver(&hx9023s_i2c_driver);
}

static void __exit hx9023s_module_exit(void)
{
    ENTER;
    i2c_del_driver(&hx9023s_i2c_driver);
}

module_init(hx9023s_module_init);
module_exit(hx9023s_module_exit);

MODULE_AUTHOR("Yasin Lee <yasin.lee.x@gmail.com><yasin.lee@tianyihexin.com>");
MODULE_DESCRIPTION("Driver for NanJingTianYiHeXin HX9023S Cap Sensor");
MODULE_ALIAS("sar driver");
MODULE_LICENSE("GPL");
