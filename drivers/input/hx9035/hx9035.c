/*************************************************************
*                                                            *
*  Driver for NanJingTianYiHeXin HX9035 & HX9036 Cap Sense *
*                                                            *
*************************************************************/
/*
                       客制化配置
1. 将宏定义 HX9035_TEST_ON_MTK_DEMO 设置为 0

2. 填充上下电函数：static void hx9035_power_on(uint8_t on) 如果是常供电，可不用配置此项

3. 根据实际硬件电路形式，补充通道配置函数中的cs和channel映射关系：static void hx9035_ch_cfg(uint8_t chip_select)

4. 根据实际需要，在dtsi文件的对应节点中和dts解析函数static int hx9035_parse_dt(struct device *dev)中添加你需要的其他属性信息，
   如其他gpio和regulator等，参考dts配置中的中断gpio号"tyhx,irq-gpio"和通道数配置"tyhx,channel-flag"是必要的。
   更多的gpio配置，请放在 static int hx9035_gpio_init(void) 和 static void hx9035_gpio_deinit(void) 中
5. version information:
    V002 bypass CS0~CS1 compile error;
    V003: add threshold modify function
    V004: add offset_CS
    V005
    V006
    V007: add bl<-2048 recali
*/

#define HX9035_TEST_ON_MTK_DEMO 0 //客户平台上务必将此宏定义设置为0
#define HX9035_TEST_ON_MTK_DEMO_XY6761 0 //客户平台上务必将此宏定义设置为0
#define HX9035_HW_MONITOR_EN 0    //默认关闭HW monitor
#define HX9035_CHANNEL_ALL_EN 0 //open all channel default
#define HX9035_DRIVER_VER "Change-Id 009"
#ifdef WT_COMPILE_FACTORY_VERSION
#define SAR_IN_FRANCE 0
#else
#define SAR_IN_FRANCE 1
#endif

#define USE_USB 0
#define POWER_ENABLE 1

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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/irqdomain.h>
#include <linux/irq.h>
#include <linux/sensors.h>
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
#include <linux/pm_wakeup.h>
#else
#include <linux/wakelock.h> //在闻泰4.19的内核上必须走这个，所以需要将代码中的所有宏判断 #ifdef CONFIG_PM_WAKELOCKS 全部替换成 #if 0
#endif
#include "hx9035.h"
#include <linux/hardware_info.h>

#if USE_USB
#if SAR_IN_FRANCE
#include <linux/usb.h>
#include <linux/notifier.h>
#include <linux/power_supply.h>
#endif
#endif

static struct i2c_client *hx9035_i2c_client = NULL;
static struct hx9035_platform_data hx9035_pdata;
static uint16_t hx9035_monitor_addr = PAGE0_03_CH_NUM_CFG;
static uint8_t hx9035_monitor_val = 0;
static uint8_t ch_enable_status = 0x00;
static uint8_t hx9035_polling_enable = 0;
static int hx9035_polling_period_ms = 0;

static int hx9035_thres_alg_en = 1;
static int hx9035_channel1_diff = 2000;
static int hx9035_channel2_diff = 2000;
static int hx9035_alg_flag = 0;

static int32_t data_raw[HX9035_CH_NUM] = {0};
static int32_t data_diff[HX9035_CH_NUM] = {0};
static int32_t data_lp[HX9035_CH_NUM] = {0};
static int32_t data_bl[HX9035_CH_NUM] = {0};
static int32_t data_offset_dac[HX9035_CH_NUM] = {0};
static int32_t data_offset_csn[HX9035_CH_NUM] = {0};

static uint16_t hx9035_threshold_near_num[] = 
{
    128,  144,  160,  176,  192,  208,  224,  240,  256,  320,   384,   448,   512, 640, 768, 896, 1024, 1280, 1536, 1792, 
    2048, 2560, 3072, 3584, 4096, 5120, 6144, 7168, 8192, 12288, 16384, 24576, 120, 112, 104, 96,  88,   80,   72,   64, 
    60,   56,   52,   48,   44,   40,   38,   36,   34,   32,    30,    28,    26,  24,  22,  20,  18,   16,   14,   13, 
    12, 10, 8
};

static int16_t hx9035_threshold_far_num[] = {500, 750, 875, 1000};

static struct hx9035_near_far_threshold hx9035_ch_thres1_default_hx9035[HX9035_CH_NUM] = {
    {.thr_near = 176, .thr_far = 1}, //ch0
    {.thr_near = 96, .thr_far = 1},
    {.thr_near = 96, .thr_far = 1},
    {.thr_near = 24576, .thr_far = 1},
    {.thr_near = 24576, .thr_far = 1}, 
    {.thr_near = 24576, .thr_far = 1},
    {.thr_near = 24576, .thr_far = 1},
    {.thr_near = 24576, .thr_far = 1},
};

static struct hx9035_near_far_threshold *hx9035_ch_thres1 = NULL;

static uint16_t hx9035_reg_init_list_size = 0;

static uint8_t hx9035_ch_near_state[HX9035_CH_NUM] = {0};  //0,1,2 三个状态
static uint8_t hx9035_ch_last_state[HX9035_CH_NUM] = {0};
static volatile uint8_t hx9035_irq_from_suspend_flag = 0;
static volatile uint8_t hx9035_irq_en_flag = 1;
static volatile uint8_t hx9035_hw_monitor_en_flag = HX9035_HW_MONITOR_EN;
static volatile uint8_t hx9035_output_switch = HX9035_OUTPUT_LP_BL;
static volatile uint8_t hx9035_data_accuracy = 16;

#if SAR_IN_FRANCE
#define MAX_INT_COUNT   20
static int32_t enable_data[HX9035_CH_NUM] = {1, 1, 1};
#define PDATA_GET(index)  hx9035_pdata.ch##index##_backgrand_cap
static int cali_test_index = 1;
#endif

static DEFINE_MUTEX(hx9035_i2c_rw_mutex);
static DEFINE_MUTEX(hx9035_ch_en_mutex);
static DEFINE_MUTEX(hx9035_cali_mutex);

//#ifdef CONFIG_PM_WAKELOCKS
#if 0
static struct wakeup_source hx9035_wake_lock;
#else
static struct wake_lock hx9035_wake_lock;
#endif

static void hx9035_manual_offset_calibration_1_ch(uint8_t ch_id);

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^通用读写 START
//从指定起始寄存器开始，连续写入count个值
static int linux_common_i2c_write(struct i2c_client *client, uint8_t *reg_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[HX9035_MAX_XFER_SIZE + 1] = {0};
    struct i2c_msg msg[1];

    if(count > HX9035_MAX_XFER_SIZE) {
        count = HX9035_MAX_XFER_SIZE;
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

    if(count > HX9035_MAX_XFER_SIZE) {
        count = HX9035_MAX_XFER_SIZE;
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
static int hx9035_read(uint16_t page_addr, uint8_t *rxbuf, int count)
{
    int ret = -1;
    uint8_t reg_page = HX9035_REG_PAGE;
    uint8_t page_id = (uint8_t)(page_addr >> 8);
    uint8_t addr = (uint8_t)page_addr;

    mutex_lock(&hx9035_i2c_rw_mutex);

    ret = linux_common_i2c_write(hx9035_i2c_client, &reg_page, &page_id, 1);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

    ret = linux_common_i2c_read(hx9035_i2c_client, &addr, rxbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_read failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9035_i2c_rw_mutex);
    return ret;
}

//return 0 for success, return -1 for errors.
static int hx9035_write(uint16_t page_addr, uint8_t *txbuf, int count)
{
    int ret = -1;
    uint8_t reg_page = HX9035_REG_PAGE;
    uint8_t page_id = (uint8_t)(page_addr >> 8);
    uint8_t addr = (uint8_t)page_addr;

    mutex_lock(&hx9035_i2c_rw_mutex);

    ret = linux_common_i2c_write(hx9035_i2c_client, &reg_page, &page_id, 1);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

    ret = linux_common_i2c_write(hx9035_i2c_client, &addr, txbuf, count);
    if(0 != ret) {
        PRINT_ERR("linux_common_i2c_write failed\n");
        goto exit;
    }

exit:
    mutex_unlock(&hx9035_i2c_rw_mutex);
    return ret;
}
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^通用读写 END

static void hx9035_disable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("invalid irq number!\n");
        return;
    }

    if(1 == hx9035_irq_en_flag) {
        disable_irq_nosync(hx9035_pdata.irq);
        hx9035_irq_en_flag = 0;
        PRINT_DBG("irq_%d is disabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is disabled already!\n", irq);
    }
}

static void hx9035_enable_irq(unsigned int irq)
{
    if(0 == irq) {
        PRINT_ERR("invalid irq number!\n");
        return;
    }

    if(0 == hx9035_irq_en_flag) {
        enable_irq(hx9035_pdata.irq);
        hx9035_irq_en_flag = 1;
        PRINT_DBG("irq_%d is enabled!\n", irq);
    } else {
        PRINT_ERR("irq_%d is enabled already!\n", irq);
    }
}

static void hx9035_data_lock(uint8_t lock_flag)
{
    int ret = -1;
    uint8_t buf[1] = {0};

    if(HX9035_DATA_LOCK == lock_flag) {
        buf[0] = 0x01;
        ret = hx9035_write(PAGE0_F1_RD_LOCK_CFG, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
        }
    } else if(HX9035_DATA_UNLOCK == lock_flag) {
        buf[0] = 0x00;
        ret = hx9035_write(PAGE0_F1_RD_LOCK_CFG, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
        }
    } else {
        PRINT_ERR("ERROR!!! hx9035_data_lock wrong para. now do data unlock!\n");
        buf[0] = 0x00;
        ret = hx9035_write(PAGE0_F1_RD_LOCK_CFG, buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
        }
    }
}

#if SAR_IN_FRANCE
#if USE_USB
static int ps_get_state(struct power_supply *psy, bool *present)
{
    union power_supply_propval pval = { 0 };
    int retval;
    static bool ac_is_present;
    static bool usb_is_present;

    retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_ONLINE,
			&pval);
    if (retval) {
        PRINT_ERR("%s psy get property failed\n", psy->desc->name);
        return retval;
    }

    if (!strncmp(psy->desc->name, "ac", strlen("ac"))) {
        ac_is_present = (pval.intval) ? true : false;
        PRINT_INF("ac_is_present = %d\n", ac_is_present);
    }

    if (!strncmp(psy->desc->name, "usb", strlen("usb"))) {
        usb_is_present = (pval.intval) ? true : false;
        PRINT_INF("usb_is_present = %d\n", usb_is_present);
    }

    *present = (ac_is_present || usb_is_present) ? true : false;
    PRINT_INF("ps is %s\n", (*present) ? "present" : "not present");
    return 0;
}
#endif
#endif

static int hx9035_id_check(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t device_id = 0;
    uint8_t version_id = 0;
    uint8_t buf[2] = {0};

    for(ii = 0; ii < HX9035_ID_CHECK_COUNT; ii++) {
        ret = hx9035_read(PAGE0_01_DEVICE_ID, buf, 2);
        if(ret < 0) {
            PRINT_ERR("i2c read error\n");
            continue;
        }
        device_id = buf[0];
        version_id = buf[1];
        if((HX9035_CHIP_ID == device_id))
            break;
        else
            continue;
    }

    if(HX9035_CHIP_ID == device_id) {
        PRINT_INF("success! device_id=0x%02X version_id=0x%02X (HX9036 or HX9035)\n", device_id, version_id);
        return 0;
    } else {
        PRINT_ERR("failed! device_id=0x%02X version_id=0x%02X (UNKNOW_CHIP_ID)\n", device_id, version_id);
        return -1;
    }
}


static void hx9035_ch_cfg(void)
{
    int ret = -1;
    int ii = 0;
    uint16_t ch_cfg = 0;
    uint8_t cfg[HX9035_CH_NUM * 2] = {0};

    uint8_t CS0 = 0;
    uint8_t CS1 = 1;
    uint8_t CS2 = 2;
    uint8_t CS3 = 3;
    uint8_t CS4 = 4;
    uint8_t CS5 = 5;
    uint8_t CS6 = 6;
    uint8_t CS7 = 7;
    uint8_t NA = 10;
    

    uint8_t CH0_POS = CS0; //MAIN cs0+, cs1-
    uint8_t CH0_NEG = CS1;
    uint8_t CH1_POS = CS6; //ANT4 cs6+, cs5-
    uint8_t CH1_NEG = CS5;
    uint8_t CH2_POS = CS4; //WCN cs4+, cs7-
    uint8_t CH2_NEG = CS7;
    uint8_t CH3_POS = CS1; //MAIN neg
    uint8_t CH3_NEG = NA;
    uint8_t CH4_POS = CS5; //ANT4 neg
    uint8_t CH4_NEG = NA;
    uint8_t CH5_POS = CS7; //WCN neg
    uint8_t CH5_NEG = NA;
    uint8_t CH6_POS = NA; //Bypass comile error
    uint8_t CH6_NEG = NA; //Bypass comile error
    uint8_t CH7_POS = NA;
    uint8_t CH7_NEG = NA;

#if HX9035_TEST_ON_MTK_DEMO
    CH0_POS = CS0;
    CH0_NEG = NA;
    CH1_POS = CS1;
    CH1_NEG = NA;
    CH2_POS = CS2;
    CH2_NEG = NA;
    CH3_POS = CS4;
    CH3_NEG = NA;
    CH4_POS = CS5;
    CH4_NEG = NA;
    CH5_POS = CS6;
    CH5_NEG = NA;
    CH6_POS = CS7;
    CH6_NEG = NA;
    CH7_POS = NA;
    CH7_NEG = NA;
#endif

    if(0) {
    PRINT_DBG("CS7~0,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n", CS7, CS6, CS5, CS4,CS3, CS2, CS1, CS0);
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

    ch_cfg = (uint16_t)((0x03 << (CH4_POS * 2)) + (0x02 << (CH4_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH5_POS * 2)) + (0x02 << (CH5_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH6_POS * 2)) + (0x02 << (CH6_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ch_cfg = (uint16_t)((0x03 << (CH7_POS * 2)) + (0x02 << (CH7_NEG * 2)));
    cfg[ii++] = (uint8_t)(ch_cfg);
    cfg[ii++] = (uint8_t)(ch_cfg >> 8);

    ret = hx9035_write(PAGE0_68_CH0_CFG_0, cfg, HX9035_CH_NUM * 2);
    if(0 != ret) {
        PRINT_ERR("hx9035_write failed\n");
    }
}



static void hx9035_register_init(void)
{
    int ii = 0;
    int ret = -1;
    
    hx9035_reg_init_list_size = ARRAY_SIZE(hx9035_reg_init_list);
    while(ii < hx9035_reg_init_list_size) {
        //PRINT_DBG("WRITE:Reg0x%02X=0x%02X\n", hx9035_reg_init_list[ii].addr, hx9035_reg_init_list[ii].val);
        ret = hx9035_write(hx9035_reg_init_list[ii].addr, &hx9035_reg_init_list[ii].val, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
        }

        if(hx9035_monitor_addr == hx9035_reg_init_list[ii].addr) {
            hx9035_monitor_val = hx9035_reg_init_list[ii].val;
        }

        ii++;
    }
}

static void hx9035_capcoe_init(void)
{
    int ii = 0;
    int ret = -1;
    uint8_t buf0[1] = {0x02};
    uint8_t buf1[4] = {0};
    uint8_t rx_buf[4] = {0};
    uint8_t check_val = 0;

    ret = hx9035_read(PAGE1_07_ANALOG_MEM0_WRDATA_7_0, rx_buf, 4);
    PRINT_DBG("REG0x0107 = 0x%02X  REG0x0108 = 0x%02X  REG0x0109 = 0x%02X  REG0x010a = 0x%02X ",
                rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);

    check_val = (rx_buf[3] & 0x3F);

    if(check_val < 22 || check_val > 42 ){
        PRINT_DBG("REG CHANGE!");
        ret = hx9035_write(PAGE0_3D_GLOBAL_CFG0, &buf0[0], 1);
        buf1[0] = rx_buf[0];
        buf1[1] = rx_buf[1];
        buf1[2] = rx_buf[2];
        buf1[3] = 0xA0;

        for(ii = 0; ii < 4; ii++){
        ret = hx9035_write(PAGE1_07_ANALOG_MEM0_WRDATA_7_0 + ii, &buf1[ii], 1);
            if(0 != ret) {
                PRINT_ERR("hx9035_write failed\n");
                }
            }
        ret = hx9035_read(PAGE1_07_ANALOG_MEM0_WRDATA_7_0, rx_buf, 4);
        PRINT_DBG("REG0x0107 = 0x%02X  REG0x0108 = 0x%02X  REG0x0109 = 0x%02X  REG0x010a = 0x%02X ",
                    rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
        }
}


static void hx9035_read_offset_dac(void)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t rx_buf[HX9035_CH_NUM * CH_DATA_BYTES_MAX] = {0};
    int32_t data[HX9035_CH_NUM] = {0};

    hx9035_data_lock(HX9035_DATA_LOCK);
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_58_OFFSET_DAC0_0, rx_buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((rx_buf[ii * bytes_per_channel + 1] << 8) | (rx_buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0x1FFF;//13位
            data_offset_dac[ii] = data[ii];
        }
    }
    hx9035_data_lock(HX9035_DATA_UNLOCK);

    PRINT_DBG("OFFSET,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3],
              data_offset_dac[4], data_offset_dac[5], data_offset_dac[6], data_offset_dac[7]);
}

static void hx9035_manual_offset_calibration_1_ch(uint8_t ch_id)//手动校准指定的某个channel一次
{
    int ret = 0;
    uint8_t buf[1] = {0};

    mutex_lock(&hx9035_cali_mutex);
    ret = hx9035_read(PAGE0_57_OFFSET_CALI_CTRL1, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
    }

    buf[0] |= (1 << ch_id);
    ret = hx9035_write(PAGE0_57_OFFSET_CALI_CTRL1, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_write failed\n");
    }
    PRINT_INF("ch_%d will calibrate in next convert cycle (ODR=%dms)\n", ch_id, HX9035_ODR_MS);//因此不能在一个ord周期内连续两次调用本函数！
    msleep(HX9035_ODR_MS);
    mutex_unlock(&hx9035_cali_mutex);
}


static void hx9035_get_prox_state(void)
{
    int ret = -1;
    uint8_t ii = 0;
    uint8_t prox_status = 0;
    uint8_t prox_buf[2] = {0};
#if 0
    uint8_t buf[3] = {0};
    ret = hx9035_read(PAGE0_F2_PROX_STATUS1, buf, 3);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
    }

    PRINT_DBG("INT_REG (class 1~3) 0xF2~0xF4: 0x%.2x 0x%.2x 0x%.2x\n", buf[0], buf[1], buf[2]);
#endif

    PRINT_INF("hx9035_thres_alg_en = %d, alg_flag = %d\n", hx9035_thres_alg_en, hx9035_alg_flag);
    if(hx9035_thres_alg_en == 0 || hx9035_alg_flag == 0){
        ret = hx9035_read(PAGE0_A8_PROX_STATUS, &prox_status, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_read failed\n");
        }
        PRINT_INF("hx9035 read 0x00a8 =  0x%.2X\n", prox_status);
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            hx9035_ch_near_state[ii] = ((prox_status >> ii) & 0x01);
        }
    }
    else if(hx9035_thres_alg_en == 1 && hx9035_alg_flag == 1){
        ret = hx9035_read(PAGE0_F2_PROX_STATUS1, prox_buf, 2);
        if(0 != ret) {
            PRINT_ERR("hx9035_read failed\n");
        }
        PRINT_INF("hx9035 read 0x00f2 = 0x%.2X, 0x00f3 = 0x%.2X\n", prox_buf[0], prox_buf[1]);
        hx9035_ch_near_state[0] = prox_buf[1] & 0x01;
        for(ii = 1; ii < HX9035_CH_NUM; ii++) {
            hx9035_ch_near_state[ii] = ((prox_buf[0] >> ii) & 0x01);
        }
    }

    PRINT_DBG("hx9035_ch_near_state[7~0]:%d%d%d%d%d%d%d%d \n",
              hx9035_ch_near_state[7],
              hx9035_ch_near_state[6],
              hx9035_ch_near_state[5],
              hx9035_ch_near_state[4],
              hx9035_ch_near_state[3],
              hx9035_ch_near_state[2],
              hx9035_ch_near_state[1],
              hx9035_ch_near_state[0]
              );
}


//因为raw data 和bl data共用寄存器，lp data和diff data共用寄存器，该函数用来切换输出数据。
//0x9a	RA_DSP_CONFIG_CTRL7 rw	0xff	7:0 lp_diff_sel 0：lp_data 1:diff_data
//0x9b	RA_DSP_CONFIG_CTRL8 rw	0xff	7:0 raw_bl_sel	0: raw_data 1:bl_data
static void hx9035_data_switch(uint8_t output_switch)
{
    int ret = -1;
    uint8_t buf[2] = {0};

    if(HX9035_OUTPUT_RAW_DIFF == output_switch) { //raw & diff
        hx9035_output_switch = HX9035_OUTPUT_RAW_DIFF;
        buf[0] = 0xFF;
        buf[1] = 0x00;
        PRINT_INF("output data: raw & diff\n");
    } else { //lp & bl
        hx9035_output_switch = HX9035_OUTPUT_LP_BL;
        buf[0] = 0x00;
        buf[1] = 0xFF;
        PRINT_INF("output data: lp & bl\n");
    }

    ret = hx9035_write(PAGE0_9A_DSP_CONFIG_CTRL7, buf, 2);
    if(0 != ret) {
        PRINT_ERR("hx9035_write failed\n");
    }
}


static void hx9035_sample_high16(void)//输出raw bl diff lp offse五组数据，默认diff和raw (diff=lp-bl)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t buf[HX9035_CH_NUM * CH_DATA_BYTES_MAX] = {0};//设置为可能的最大buf
    int32_t data[HX9035_CH_NUM] = {0};
    uint8_t thres2;

    hx9035_data_lock(HX9035_DATA_LOCK);
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_0D_RAW_BL_CH0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 2] << 8) | (buf[ii * bytes_per_channel + 1])); //高16位数据
            if(HX9035_OUTPUT_RAW_DIFF == hx9035_output_switch) {
                data_raw[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                data_bl[ii] = 0;
            } else {
                data_raw[ii] = 0;
                data_bl[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
            }
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_25_LP_DIFF_CH0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 2] << 8) | (buf[ii * bytes_per_channel + 1])); //高16位数据
            if(HX9035_OUTPUT_RAW_DIFF == hx9035_output_switch) {
                data_diff[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                data_lp[ii] = 0;
            } else {
                data_lp[ii] = (data[ii] > 0x7FFF) ? (data[ii] - (0xFFFF + 1)) : data[ii]; //补码转换为原码
                data_diff[ii] = data_lp[ii] - data_bl[ii];
            }

        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_58_OFFSET_DAC0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 1] << 8) | (buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0x1FFF;//13位
            data_offset_dac[ii] = data[ii];
        }
    }
    //====================================================================================================
    hx9035_data_lock(HX9035_DATA_UNLOCK);
    if((data_diff[1] > hx9035_channel1_diff) || (data_diff[2] > hx9035_channel2_diff)){
        hx9035_alg_flag = 1;
        ret = hx9035_read(PAGE0_D1_PROX_HIGH_THRES2_CFG0, &thres2, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_read failed\n");
        }
        PRINT_DBG("thres2  ,%-8d\n",hx9035_threshold_near_num[thres2]);
        if(hx9035_thres_alg_en == 1 && data_diff[0] < hx9035_threshold_near_num[thres2]) data_diff[0] = data_diff[0] / 3;
    }
    else hx9035_alg_flag = 0;
    PRINT_DBG("DIFF  ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_diff[0], data_diff[1], data_diff[2], data_diff[3],
              data_diff[4], data_diff[5], data_diff[6], data_diff[7]);
    PRINT_DBG("RAW   ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_raw[0], data_raw[1], data_raw[2], data_raw[3],
              data_raw[4], data_raw[5], data_raw[6], data_raw[7]);
    PRINT_DBG("OFFSET,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3],
              data_offset_dac[4], data_offset_dac[5], data_offset_dac[6], data_offset_dac[7]);
    PRINT_DBG("BL    ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_bl[0], data_bl[1], data_bl[2], data_bl[3],
              data_bl[4], data_bl[5], data_bl[6], data_bl[7]);
    PRINT_DBG("LP    ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_lp[0], data_lp[1], data_lp[2], data_lp[3],
              data_lp[4], data_lp[5], data_lp[6], data_lp[7]);
}

static void hx9035_sample_24(void)//输出raw bl diff lp offse五组数据，默认diff和raw (diff=lp-bl)
{
    int ret = -1;
    int ii = 0;
    uint8_t bytes_per_channel = 0;
    uint8_t bytes_all_channels = 0;
    uint8_t buf[HX9035_CH_NUM * CH_DATA_BYTES_MAX] = {0};//设置为可能的最大buf
    int32_t data[HX9035_CH_NUM] = {0};
    uint8_t thres2;

    hx9035_data_lock(HX9035_DATA_LOCK);
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_0D_RAW_BL_CH0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 2] << 16) | (buf[ii * bytes_per_channel + 1] << 8) | (buf[ii * bytes_per_channel])); //24位数据
            if(HX9035_OUTPUT_RAW_DIFF == hx9035_output_switch) {
                data_raw[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                data_bl[ii] = 0;
            } else {
                data_raw[ii] = 0;
                data_bl[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
            }
        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_3BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_25_LP_DIFF_CH0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 2] << 16) | (buf[ii * bytes_per_channel + 1] << 8) | (buf[ii * bytes_per_channel])); //24位数据
            if(HX9035_OUTPUT_RAW_DIFF == hx9035_output_switch) {
                data_diff[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                data_lp[ii] = 0;
            } else {
                data_lp[ii] = (data[ii] > 0x7FFFFF) ? (data[ii] - (0xFFFFFF + 1)) : data[ii]; //补码转换为原码
                data_diff[ii] = data_lp[ii] - data_bl[ii];
            }

        }
    }
    //====================================================================================================
    bytes_per_channel = CH_DATA_2BYTES;
    bytes_all_channels = HX9035_CH_NUM * bytes_per_channel;
    ret = hx9035_read(PAGE0_58_OFFSET_DAC0_0, buf, bytes_all_channels);
    if(0 == ret) {
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            data[ii] = ((buf[ii * bytes_per_channel + 1] << 8) | (buf[ii * bytes_per_channel]));
            data[ii] = data[ii] & 0x1FFF;//13位
            data_offset_dac[ii] = data[ii];
        }
    }
    //====================================================================================================
    hx9035_data_lock(HX9035_DATA_UNLOCK);
    if((data_diff[1] > hx9035_channel1_diff) || (data_diff[2] > hx9035_channel2_diff)){
        hx9035_alg_flag = 1;
        ret = hx9035_read(PAGE0_D1_PROX_HIGH_THRES2_CFG0, &thres2, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_read failed\n");
        }
        PRINT_DBG("thres2*256  ,%-8d\n",hx9035_threshold_near_num[thres2]*256);
        if(hx9035_thres_alg_en == 1 && data_diff[0] < (hx9035_threshold_near_num[thres2]*256)) data_diff[0] = data_diff[0] / 3;
    }
    else hx9035_alg_flag = 0;
    PRINT_DBG("DIFF  ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_diff[0], data_diff[1], data_diff[2], data_diff[3],
              data_diff[4], data_diff[5], data_diff[6], data_diff[7]);
    PRINT_DBG("RAW   ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_raw[0], data_raw[1], data_raw[2], data_raw[3],
              data_raw[4], data_raw[5], data_raw[6], data_raw[7]);
    PRINT_DBG("OFFSET,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3],
              data_offset_dac[4], data_offset_dac[5], data_offset_dac[6], data_offset_dac[7]);
    PRINT_DBG("BL    ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_bl[0], data_bl[1], data_bl[2], data_bl[3],
              data_bl[4], data_bl[5], data_bl[6], data_bl[7]);
    PRINT_DBG("LP    ,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
              data_lp[0], data_lp[1], data_lp[2], data_lp[3],
              data_lp[4], data_lp[5], data_lp[6], data_lp[7]);
}

static void hx9035_sample(void)
{
    if(24 == hx9035_data_accuracy) {
        hx9035_sample_24();
    } else if(16 == hx9035_data_accuracy) {
        hx9035_sample_high16();
    } 
}

static void hx9035_input_report(void)
{
    int ii = 0;
    uint8_t touch_state = 0;
    int num_channels = 0;
    struct input_dev *input = NULL;
    struct hx9035_channel_info *channel_p  = NULL;
#if SAR_IN_FRANCE
    int16_t ch0_result = 0;
    int16_t ch1_result = 0;
    int16_t ch2_result = 0;
#endif

    num_channels = ARRAY_SIZE(hx9035_channels);

    if (unlikely(NULL == hx9035_channels)) {
        PRINT_ERR("hx9035_channels==NULL!!!\n");
        return;
    }
#if SAR_IN_FRANCE
    if (hx9035_pdata.sar_first_boot) {
        hx9035_read_offset_dac();
        ch0_result = data_offset_dac[0];
        ch1_result = data_offset_dac[1];
        ch2_result = data_offset_dac[2];
        PRINT_INF("SAR ch0_result: %d, ch1_result: %d, ch2_result: %d\n", ch0_result, ch1_result, ch2_result);
#if USE_USB
        PRINT_INF("SAR hx9035_pdata.ps_is_present=%d\n",hx9035_pdata.ps_is_present);
#endif
        PRINT_INF("SAR hx9035_pdata.sar_first_boot=%d\n",hx9035_pdata.sar_first_boot);
      }
#endif

#if defined(CONFIG_SENSORS)
    if (hx9035_pdata.skip_data == true) {
        PRINT_INF("%s - skip grip event\n", __func__);
        return;
    }
#endif

    for (ii = 0; ii < num_channels; ii++) {
        if (false == hx9035_channels[ii].enabled) { //enabled表示该通道已经被开启。
            PRINT_DBG("ch_%d(name:%s) is disabled, nothing report\n", ii, hx9035_channels[ii].name);
            continue;
        }
        if (false == hx9035_channels[ii].used) { //used表示通道有效，但此刻该通道并不一定enable
            PRINT_DBG("ch_%d(name:%s) is unused, nothing report\n", ii, hx9035_channels[ii].name);
            continue;
        }

        channel_p = &hx9035_channels[ii];
        if (NULL == channel_p) {
            PRINT_ERR("ch_%d is NULL!!!\n", ii);
            return;
        }

        input = channel_p->hx9035_input_dev;
        if (NULL == input) {
            PRINT_ERR("ch_%d input device is NULL!!!\n", ii);
            return;
        }

#if SAR_IN_FRANCE
		if (MAX_INT_COUNT >= hx9035_pdata.interrupt_count) {
        	if(hx9035_ch_last_state[ii] != hx9035_ch_near_state[ii]){
            	PRINT_ERR("ch_%d state changed!!!\n", ii);
                hx9035_pdata.interrupt_count++;
            }
            hx9035_ch_last_state[ii] = hx9035_ch_near_state[ii];
        }
        PRINT_INF("SAR hx9035_pdata.interrupt_count=%d\n",hx9035_pdata.interrupt_count);
        if (hx9035_pdata.sar_first_boot) {
            if (hx9035_pdata.interrupt_count >= 5 && hx9035_pdata.interrupt_count < 15) {
                PRINT_INF("hx9035_ch_near_state[2] = %d", hx9035_ch_near_state[2]);
                if (hx9035_ch_near_state[2] == IDLE && cali_test_index == 1) {
                    hx9035_manual_offset_calibration_1_ch(2);
                    PRINT_INF("hx9035:interrupt_count:%d, wifi sar cali", hx9035_pdata.interrupt_count);
                    cali_test_index = 0;
                    PRINT_INF("hx9035:interrupt_count:%d", hx9035_pdata.interrupt_count);
                }
            } else if(hx9035_pdata.interrupt_count == 15 && cali_test_index == 1) {
                    PRINT_INF("hx9035:interrupt_count:15, wifi sar cali");
                    hx9035_manual_offset_calibration_1_ch(2);
                    PRINT_INF("hx9035:interrupt_count:%d", hx9035_pdata.interrupt_count);
            }
            if (hx9035_pdata.sar_first_boot && (hx9035_pdata.interrupt_count < MAX_INT_COUNT) &&   \
                   ((ch0_result - hx9035_pdata.ch0_backgrand_cap) >= -7) &&   \
                   ((ch1_result - hx9035_pdata.ch1_backgrand_cap) >= -7) &&   \
                   ((ch2_result - hx9035_pdata.ch2_backgrand_cap) >= -5)) {
                PRINT_INF("force %s State=Near\n", hx9035_channels[ii].name);
                input_report_rel(input, REL_MISC, 1);
                input_report_rel(input, REL_X, 1);
                input_sync(input);
                channel_p->state = PROXACTIVE;
                continue;
            }
            PRINT_INF("exit force input near mode!!!\n");
            hx9035_pdata.sar_first_boot = false;
        }
#endif

#if POWER_ENABLE
        if(hx9035_pdata.power_state){
            if (channel_p->state == IDLE)
                PRINT_DBG("%s already released, nothing report\n", channel_p->name);
            else{
                wake_lock_timeout(&hx9035_wake_lock, HZ * 1);   
                input_report_rel(input, REL_MISC, 2);
                input_report_rel(input, REL_X, 2);
                input_sync(input);
                channel_p->state = IDLE;
                PRINT_DBG("%s report released\n", channel_p->name);
            }
        }
        else{
#endif
            touch_state = hx9035_ch_near_state[ii];

            if (PROXACTIVE == touch_state) {
#if SAR_IN_FRANCE
                if (channel_p->state == PROXACTIVE && hx9035_pdata.anfr_export_exit == false)
#else
                if (channel_p->state == PROXACTIVE)
#endif
                    PRINT_DBG("%s already PROXACTIVE, nothing report\n", channel_p->name);
                else {
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
                    __pm_wakeup_event(&hx9035_wake_lock, 1000);
#else
                    wake_lock_timeout(&hx9035_wake_lock, HZ * 1);
#endif
#if defined(CONFIG_SENSORS)
                if (hx9035_pdata.skip_data == true) {
                    PRINT_INF("%s - skip grip event\n", __func__);
                } else {
#endif
                    input_report_rel(input, REL_MISC, 1);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
#if defined(CONFIG_SENSORS)
                }
#endif
                channel_p->state = PROXACTIVE;
                PRINT_DBG("%s report PROXACTIVE(15mm)\n", channel_p->name);
            }
        } else if (IDLE == touch_state) {
#if SAR_IN_FRANCE
            if (channel_p->state == IDLE && hx9035_pdata.anfr_export_exit == false)
#else
            if (channel_p->state == IDLE)
#endif
                PRINT_DBG("%s already released, nothing report\n", channel_p->name);
            else {
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
                    __pm_wakeup_event(&hx9035_wake_lock, 1000);
#else
                    wake_lock_timeout(&hx9035_wake_lock, HZ * 1);
#endif
#if defined(CONFIG_SENSORS)
                if (hx9035_pdata.skip_data == true) {
                    PRINT_INF("%s - skip grip event\n", __func__);
                } else {
#endif
                    input_report_rel(input, REL_MISC, 2);
                    input_report_rel(input, REL_X, 2);
                    input_sync(input);
#if defined(CONFIG_SENSORS)
                }
#endif
                channel_p->state = IDLE;
                PRINT_DBG("%s report released\n", channel_p->name);
            }
        } else {
            PRINT_ERR("unknow touch state! touch_state=%d\n", touch_state);
        }
#if POWER_ENABLE
        }
#endif
    }
}

#if SAR_IN_FRANCE
#if USE_USB
static int ps_notify_callback(struct notifier_block *self,
                                unsigned long event, void *p)
{
    struct power_supply *psy = p;
    bool present;
    int retval;
    PRINT_INF("SAR psy->desc->name is %s",psy->desc->name);

    if (PSY_EVENT_PROP_CHANGED == event
            && psy && psy->desc->get_property && psy->desc->name &&
            (!strncmp(psy->desc->name, "usb", strlen("usb")) || !strncmp(psy->desc->name, "ac", strlen("ac")))) {
        PRINT_INF("SAR ps notification: event = %lu\n", event);
        retval = ps_get_state(psy, &present);
        if (retval) {
           PRINT_ERR("SAR psy get property failed\n");
           return retval;
        }

        if (present == hx9035_pdata.ps_is_present) {
            PRINT_INF("SAR ps present state not change\n");
            return 0;
        }

        hx9035_pdata.ps_is_present = present;
        PRINT_INF("SAR hx9035_pdata.ps_is_present = %d\n", hx9035_pdata.ps_is_present);
        if (hx9035_pdata.sar_first_boot && hx9035_pdata.ps_is_present) {
           hx9035_input_report();
           //need add
        }
    }
    return 0;
}
#endif
#endif

// 必要DTS属性：
// "tyhx,irq-gpio"     必要！中断对应的gpio number
// "tyhx,channel-flag"  必要！每个channel对应一个input设备，每个bit对应一个channel（channel）。如0xF代表开启0，1，2，3通道
static int hx9035_parse_dt(struct device *dev)
{
    int ret = -1;
    struct device_node *dt_node = dev->of_node;

    if (NULL == dt_node) {
        PRINT_ERR("No DTS node\n");
        return -ENODEV;
    }

#if HX9035_TEST_ON_MTK_DEMO_XY6761
    ret = of_property_read_u32(dt_node, "xy6761-fake-irq-gpio", &hx9035_pdata.irq_gpio);
    if(ret < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#else
    hx9035_pdata.irq_gpio = of_get_named_gpio_flags(dt_node, "tyhx,irq-gpio", 0, NULL);
    if(hx9035_pdata.irq_gpio < 0) {
        PRINT_ERR("failed to get irq_gpio from DT\n");
        return -1;
    }
#endif

    PRINT_INF("hx9035_pdata.irq_gpio=%d\n", hx9035_pdata.irq_gpio);

    hx9035_pdata.channel_used_flag = 0xFF;
    ret = of_property_read_u32(dt_node, "tyhx,channel-flag", &hx9035_pdata.channel_used_flag);//客户配置：有效通道标志位：hx9035最大传入0xFF(表示8通道)
    if(ret < 0) {
        PRINT_ERR("\"tyhx,channel-flag\" is not set in DT\n");
        return -1;
    }
    if(hx9035_pdata.channel_used_flag > ((1 << HX9035_CH_NUM) - 1)) {
        PRINT_ERR("the max value of channel_used_flag is 0x%X\n", ((1 << HX9035_CH_NUM) - 1));
        return -1;
    }
    PRINT_INF("hx9035_pdata.channel_used_flag=0x%X\n", hx9035_pdata.channel_used_flag);
    
    hx9035_pdata.grip_sensor_ch = 0;
    hx9035_pdata.grip_sensor_sub_ch = 0;
    hx9035_pdata.grip_sensor_wifi_ch = 0;
    of_property_read_u32(dt_node, "grip-sensor", &hx9035_pdata.grip_sensor_ch);
    of_property_read_u32(dt_node, "grip-sensor-sub", &hx9035_pdata.grip_sensor_sub_ch);
    of_property_read_u32(dt_node, "grip-sensor-wifi", &hx9035_pdata.grip_sensor_wifi_ch);

    return 0;
}

static int hx9035_gpio_init(void)
{
    int ret = -1;
    if (gpio_is_valid(hx9035_pdata.irq_gpio)) {
        ret = gpio_request(hx9035_pdata.irq_gpio, "hx9035_irq_gpio");
        if (ret < 0) {
            PRINT_ERR("gpio_request failed. ret=%d\n", ret);
            return ret;
        }
        ret = gpio_direction_input(hx9035_pdata.irq_gpio);
        if (ret < 0) {
            PRINT_ERR("gpio_direction_input failed. ret=%d\n", ret);
            gpio_free(hx9035_pdata.irq_gpio);
            return ret;
        }
    } else {
        PRINT_ERR("Invalid gpio num\n");
        return -1;
    }

    return 0;
}

static void hx9035_gpio_deinit(void)
{
    ENTER;
    gpio_free(hx9035_pdata.irq_gpio);
}

static void hx9035_power_on(uint8_t on)
{
    if(on) {
        //TODO: 用户自行填充
        PRINT_INF("power on\n");
    } else {
        //TODO: 用户自行填充
        PRINT_INF("power off\n");
    }
}


#if 1
static int hx9035_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = 0;
    uint8_t tx_buf[1] = {0};

    if(ch_enable_status > 0) {
        if(1 == en) {
            ch_enable_status |= (1 << ch_id);
            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            ch_enable_status &= ~(1 << ch_id);
            if(0 == ch_enable_status) {
                tx_buf[0] = 0x00;
                ret = hx9035_write(PAGE0_03_CH_NUM_CFG, tx_buf, 1);
                if(0 != ret) {
                    PRINT_ERR("hx9035_write failed\n");
                    return -1;
                }
                if(1 == hx9035_hw_monitor_en_flag) {
                    cancel_delayed_work_sync(&(hx9035_pdata.hw_monitor_work));
                }
                PRINT_INF("ch_%d disabled, all channels disabled\n", ch_id);
            } else {
                PRINT_INF("ch_%d disabled\n", ch_id);
            }
        }
    } else {
        if(1 == en) {
            tx_buf[0] = hx9035_pdata.channel_used_flag;
            ret = hx9035_write(PAGE0_03_CH_NUM_CFG, tx_buf, 1);
            if(0 != ret) {
                PRINT_ERR("hx9035_write failed\n");
                return -1;
            }
            ch_enable_status |= (1 << ch_id);

            if(1 == hx9035_hw_monitor_en_flag) {
                schedule_delayed_work(&hx9035_pdata.hw_monitor_work, msecs_to_jiffies(10000));
            }

            PRINT_INF("ch_%d enabled\n", ch_id);
        } else {
            PRINT_INF("all channels disabled already\n");
        }
    }

    return ret;
}

#else

static int hx9035_ch_en(uint8_t ch_id, uint8_t en)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};
    uint8_t tx_buf[1] = {0};

    ret = hx9035_read(R2_PAGE0_RA_CH_NUM_CFG_0x03, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
        return -1;
    }
    ch_enable_status = rx_buf[0];

    if(ch_id >= HX9035_CH_NUM) {
        PRINT_ERR("channel index over range !!!ch_enable_status=0x%02X (ch_id=%d, en=%d)\n", ch_enable_status, ch_id, en);
        return -1;
    }

    if(1 == en) {
        ch_enable_status |= (1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9035_write(R2_PAGE0_RA_CH_NUM_CFG_0x03, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
        msleep(10);
    } else {
        en = 0;
        ch_enable_status &= ~(1 << ch_id);
        tx_buf[0] = ch_enable_status;
        ret = hx9035_write(R2_PAGE0_RA_CH_NUM_CFG_0x03, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
            return -1;
        }
        PRINT_INF("ch_enable_status=0x%02X (ch_%d=%d)\n", ch_enable_status, ch_id, en);
    }
    return 0;
}
#endif

static int hx9035_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
    int ret = -1;
    int ii = 0;

    mutex_lock(&hx9035_ch_en_mutex);
    for (ii = 0; ii < ARRAY_SIZE(hx9035_channels); ii++) {
        if (strcmp(sensors_cdev->sensor_name, hx9035_channels[ii].name) == 0) {
            if (1 == enable) {
                PRINT_INF("enable ch_%d(name:%s)\n", ii, sensors_cdev->sensor_name);
                ret = hx9035_ch_en(ii, 1);
                if(0 != ret) {
                    PRINT_ERR("hx9035_ch_en failed\n");
                    mutex_unlock(&hx9035_ch_en_mutex);
                    return -1;
                }
                hx9035_channels[ii].state = IDLE;
                hx9035_channels[ii].enabled = true;
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
                __pm_wakeup_event(&hx9035_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9035_wake_lock, HZ * 1);
#endif
#if defined(CONFIG_SENSORS)
                if (hx9035_pdata.skip_data == true) {
                    PRINT_INF("%s - skip grip event\n", __func__);
                } else {
#endif
                    input_report_rel(hx9035_channels[ii].hx9035_input_dev, REL_MISC, 2);
                    input_report_rel(hx9035_channels[ii].hx9035_input_dev, REL_X, 2);
                    input_sync(hx9035_channels[ii].hx9035_input_dev);
                    //hx9035_manual_offset_calibration_1_ch(ii);//enable(en:0x24)的时候会自动校准，故此处不需要。
#if defined(CONFIG_SENSORS)
                }
#endif
            } else if (0 == enable) {
                PRINT_INF("disable ch_%d(name:%s)\n", ii, sensors_cdev->sensor_name);
                ret = hx9035_ch_en(ii, 0);
                if(0 != ret) {
                    PRINT_ERR("hx9035_ch_en failed\n");
                    mutex_unlock(&hx9035_ch_en_mutex);
                    return -1;
                }
                hx9035_channels[ii].state = IDLE;
                hx9035_channels[ii].enabled = false;
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
                __pm_wakeup_event(&hx9035_wake_lock, 1000);
#else
                wake_lock_timeout(&hx9035_wake_lock, HZ * 1);
#endif
#if defined(CONFIG_SENSORS)
                if (hx9035_pdata.skip_data == true) {
                    PRINT_INF("%s - skip grip event\n", __func__);
                } else {
#endif
                    input_report_rel(hx9035_channels[ii].hx9035_input_dev, REL_MISC, 2);
                    input_sync(hx9035_channels[ii].hx9035_input_dev);
#if defined(CONFIG_SENSORS)
                }
#endif
            } else {
                PRINT_ERR("unknown enable symbol\n");
            }
        }
    }
    mutex_unlock(&hx9035_ch_en_mutex);

    return 0;
}

static ssize_t show_enable(struct device *dev,
                           struct device_attribute *attr,
                           char *buf)
{
    int status = 0;
    char *p = buf;
    struct input_dev* temp_input_dev;
    
    temp_input_dev = container_of(dev,struct input_dev,dev);
    PRINT_INF("%s: dev->name:%s\n", __func__, temp_input_dev->name);

    for (size_t i = 0; i < ARRAY_SIZE(hx9035_channels); i++) {
        if (!strcmp(temp_input_dev->name, hx9035_channels[i].name)) {
            if (true == hx9035_channels[i].enabled)
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
    mutex_lock(&hx9035_ch_en_mutex);
    PRINT_INF("%s: dev->name:%s:%s\n", __func__, temp_input_dev->name, buf);

    for (size_t i = 0; i < ARRAY_SIZE(hx9035_channels); i++) {
        if (!strcmp(temp_input_dev->name, hx9035_channels[i].name)) {
            if (0 != hx9035_ch_en(i, val)) {
                PRINT_ERR("hx9035_ch_en failed\n");
                mutex_unlock(&hx9035_ch_en_mutex);
                return -1;
            }
            if (val) {
                hx9035_channels[i].state = IDLE;
                hx9035_channels[i].enabled = true;
                PRINT_INF("name:%s: enable\n", hx9035_channels[i].name);
                input_report_rel(hx9035_channels[i].hx9035_input_dev, REL_MISC, 2);
                input_report_rel(hx9035_channels[i].hx9035_input_dev, REL_X, 2);
                input_sync(hx9035_channels[i].hx9035_input_dev);
#if SAR_IN_FRANCE
                if (hx9035_pdata.sar_first_boot && enable_data[i]) {
                    mdelay(20);
                    hx9035_read_offset_dac();
                    PDATA_GET(0) = data_offset_dac[0]; enable_data[0] = 0;
                    PDATA_GET(1) = data_offset_dac[1]; enable_data[1] = 0;
                    PDATA_GET(2) = data_offset_dac[2]; enable_data[2] = 0;
                    PRINT_INF("ch0:backgrand_cap%d", data_offset_dac[0]);
                    PRINT_INF("ch1:backgrand_cap%d", data_offset_dac[1]);
                    PRINT_INF("ch2:backgrand_cap%d", data_offset_dac[2]);
                }
#endif
            } else {
                PRINT_INF("name:%s: disable\n", hx9035_channels[i].name);
                hx9035_channels[i].enabled = false;
                input_report_rel(hx9035_channels[i].hx9035_input_dev, REL_MISC, 2);
                input_sync(hx9035_channels[i].hx9035_input_dev);
                hx9035_channels[i].state = IDLE;
            }
            break;
        }
    }

    mutex_unlock(&hx9035_ch_en_mutex);
    return count;
}
static DEVICE_ATTR(enable, 0660, show_enable, store_enable);

static int hx9035_reg_reinitialize(void)
{
    int ret = -1;
    uint8_t tx_buf[1] = {0};

    ENTER;
    hx9035_register_init();//寄存器初始化,如果需要，也可以使用chip_select做芯片区分
    hx9035_ch_cfg();//通道配置
    hx9035_data_switch(hx9035_output_switch);//数据输出默认选择raw 和 diff

    if(ch_enable_status > 0) {
        tx_buf[0] = hx9035_pdata.channel_used_flag;
        ret = hx9035_write(PAGE0_03_CH_NUM_CFG, tx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_write failed\n");
            return -1;
        }
    }
    return 0;
}

static int hx9035_state_monitoring(uint8_t addr, uint8_t val)
{
    int ret = -1;
    uint8_t rx_buf[1] = {0};

    if(0 == hx9035_hw_monitor_en_flag) {
        return 0;
    }

    ret = hx9035_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
        return -1;
    }

    if(val != rx_buf[0]) {
        PRINT_ERR("chip restarted abnormally! do reg reinitialize!\n");
        hx9035_reg_reinitialize();
    } else {
        PRINT_INF("OK\n");
    }

    return 0 ;
}

static void hx9035_polling_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9035_ch_en_mutex);
    hx9035_state_monitoring(hx9035_monitor_addr, hx9035_monitor_val);
    hx9035_sample();
    hx9035_get_prox_state();
    hx9035_input_report();
    if(1 == hx9035_polling_enable)
        schedule_delayed_work(&hx9035_pdata.polling_work, msecs_to_jiffies(hx9035_polling_period_ms));
    mutex_unlock(&hx9035_ch_en_mutex);
    return;
}

static void hx9035_hw_monitor_work_func(struct work_struct *work)
{
    ENTER;
    mutex_lock(&hx9035_ch_en_mutex);
    hx9035_state_monitoring(hx9035_monitor_addr, hx9035_monitor_val);
    schedule_delayed_work(&hx9035_pdata.hw_monitor_work, msecs_to_jiffies(10000));
    mutex_unlock(&hx9035_ch_en_mutex);
    return;
}

static irqreturn_t hx9035_irq_handler(int irq, void *pvoid)
{
    ENTER;
    mutex_lock(&hx9035_ch_en_mutex);
    if(1 == hx9035_irq_from_suspend_flag) {
        hx9035_irq_from_suspend_flag = 0;
        PRINT_INF("delay 50ms for waiting the i2c controller enter working mode\n");
        msleep(50);//如果从suspend被中断唤醒，该延时确保i2c控制器也从休眠唤醒并进入工作状态
    }

    PRINT_INF("hx9035_irq_handler");
    hx9035_state_monitoring(hx9035_monitor_addr, hx9035_monitor_val);
    hx9035_sample();
    hx9035_get_prox_state();
    hx9035_input_report();
    mutex_unlock(&hx9035_ch_en_mutex);
    return IRQ_HANDLED;
}

//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test begin
static ssize_t hx9035_raw_data_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *p = buf;
    int ii = 0;

    ENTER;
    hx9035_sample();
    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        p += snprintf(p, PAGE_SIZE, "ch[%d]: DIFF=%-8d, RAW=%-8d, OFFSET=%-8d, BL=%-8d, LP=%-8d\n",
                      ii, data_diff[ii], data_raw[ii], data_offset_dac[ii], data_bl[ii], data_lp[ii]);
    }
    return (p - buf);//返回实际放到buf中的实际字符个数
}

static ssize_t hx9035_register_write_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    unsigned int val = 0;
    uint16_t addr = 0;
    uint8_t tx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
        PRINT_ERR("please input two HEX numbers: aaaa,bb (aaaa: reg_address(16bit), bb: value_to_be_set(8bit)\n");
        return -EINVAL;
    }

    addr = (uint16_t)reg_address;
    tx_buf[0] = (uint8_t)val;

    ret = hx9035_write(addr, tx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_write failed\n");
    }

    PRINT_INF("WRITE:Reg0x%04X=0x%02X\n", addr, tx_buf[0]);
    return count;
}

static ssize_t hx9035_register_read_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    unsigned int reg_address = 0;
    uint16_t addr = 0;
    uint8_t rx_buf[1] = {0};

    ENTER;
    if (sscanf(buf, "%x", &reg_address) != 1) {
        PRINT_ERR("please input a HEX number of 16bit for reg_addr\n");
        return -EINVAL;
    }
    addr = (uint16_t)reg_address;

    ret = hx9035_read(addr, rx_buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
    }

    PRINT_INF("READ:Reg0x%04X=0x%02X\n", addr, rx_buf[0]);
    return count;
}

static ssize_t hx9035_channel_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ch_id = 0;
    int en = 0;

    ENTER;
    if (sscanf(buf, "%d,%d", &ch_id, &en) != 2) {
        PRINT_ERR("please input two DEC numbers: ch_id,en (ch_id: channel number, en: 1=enable, 0=disable)\n");
        return -EINVAL;
    }

    if((ch_id >= HX9035_CH_NUM) || (ch_id < 0)) {
        PRINT_ERR("channel number out of range, the effective number is 0~%d\n", HX9035_CH_NUM - 1);
        return -EINVAL;
    }

    if ((hx9035_pdata.channel_used_flag >> ch_id) & 0x01) {
        hx9035_set_enable(&hx9035_channels[ch_id].hx9035_sensors_classdev, (en > 0) ? 1 : 0);
    } else {
        PRINT_ERR("ch_%d is unused, you can not enable or disable an unused channel\n", ch_id);
    }

    return count;
}

static ssize_t hx9035_channel_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    ENTER;
    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        if ((hx9035_pdata.channel_used_flag >> ii) & 0x1) {
            PRINT_INF("hx9035_channels[%d].enabled=%d\n", ii, hx9035_channels[ii].enabled);
            p += snprintf(p, PAGE_SIZE, "hx9035_channels[%d].enabled=%d\n", ii, hx9035_channels[ii].enabled);
        }
    }

    return (p - buf);
}

static ssize_t hx9035_manual_offset_calibration_show(struct class *class, struct class_attribute *attr, char *buf)
{
    hx9035_read_offset_dac();
    return sprintf(buf, "OFFSET,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d,%-8d\n",
                   data_offset_dac[0], data_offset_dac[1], data_offset_dac[2], data_offset_dac[3],
                   data_offset_dac[4], data_offset_dac[5], data_offset_dac[6], data_offset_dac[7]);
}

static ssize_t hx9035_manual_offset_calibration_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ii = 0;
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
        for(ii = 0; ii < HX9035_CH_NUM; ii++) {
            if ((hx9035_pdata.channel_used_flag >> ii) & 0x1) {
                mdelay(20);
                hx9035_manual_offset_calibration_1_ch(ii);
            }
        }
        return count;
    }

    if (ch_id < HX9035_CH_NUM)
        hx9035_manual_offset_calibration_1_ch(ch_id);
    else
        PRINT_ERR(" \"echo ch_id > calibrate\" to do a manual calibrate(ch_id is a channel num (0~%d)\n", HX9035_CH_NUM);
    return count;
}

static ssize_t hx9035_prox_state_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_DBG("hx9035_ch_near_state:%d-%d-%d-%d-%d-%d-%d-%d\n",
              hx9035_ch_near_state[7],
              hx9035_ch_near_state[6],
              hx9035_ch_near_state[5],
              hx9035_ch_near_state[4],
              hx9035_ch_near_state[3],
              hx9035_ch_near_state[2],
              hx9035_ch_near_state[1],
              hx9035_ch_near_state[0]);

    return sprintf(buf, "hx9035_ch_near_state:%d-%d-%d-%d-%d-%d-%d-%d\n",
                   hx9035_ch_near_state[7],
                   hx9035_ch_near_state[6],
                   hx9035_ch_near_state[5],
                   hx9035_ch_near_state[4],
                   hx9035_ch_near_state[3],
                   hx9035_ch_near_state[2],
                   hx9035_ch_near_state[1],
                   hx9035_ch_near_state[0]);
}

static ssize_t hx9035_polling_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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
        hx9035_polling_period_ms = value;
        if(1 == hx9035_polling_enable) {
            PRINT_INF("polling is already enabled!, no need to do enable again!, just update the polling period\n");
            goto exit;
        }

        hx9035_polling_enable = 1;
        hx9035_disable_irq(hx9035_pdata.irq);

        PRINT_INF("polling started! period=%dms\n", hx9035_polling_period_ms);
        schedule_delayed_work(&hx9035_pdata.polling_work, msecs_to_jiffies(hx9035_polling_period_ms));
    } else {
        if(0 == hx9035_polling_enable) {
            PRINT_INF("polling is already disabled!, no need to do again!\n");
            goto exit;
        }
        hx9035_polling_period_ms = 0;
        hx9035_polling_enable = 0;
        PRINT_INF("polling stoped!\n");

        cancel_delayed_work(&hx9035_pdata.polling_work);//停止polling对应的工作队列，并重新开启中断模式
        hx9035_enable_irq(hx9035_pdata.irq);
    }

exit:
    return count;
}

static ssize_t hx9035_polling_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9035_polling_enable=%d hx9035_polling_period_ms=%d\n", hx9035_polling_enable, hx9035_polling_period_ms);
    return sprintf(buf, "hx9035_polling_enable=%d hx9035_polling_period_ms=%d\n", hx9035_polling_enable, hx9035_polling_period_ms);
}

static ssize_t hx9035_loglevel_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("tyhx_log_level=%d\n", tyhx_log_level);
    return sprintf(buf, "tyhx_log_level=%d\n", tyhx_log_level);
}

static ssize_t hx9035_loglevel_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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

static ssize_t hx9035_accuracy_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9035_data_accuracy=%d\n", hx9035_data_accuracy);
    return sprintf(buf, "hx9035_data_accuracy=%d\n", hx9035_data_accuracy);
}

static ssize_t hx9035_accuracy_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9035_data_accuracy = (24 == value) ? 24 : 16;
    PRINT_INF("set hx9035_data_accuracy=%d\n", hx9035_data_accuracy);
    return count;
}

static ssize_t hx9035_output_switch_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9035_output_switch=%d\n", hx9035_output_switch);
    return sprintf(buf, "hx9035_output_switch=%d\n", hx9035_output_switch);
}

static ssize_t hx9035_output_switch_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9035_data_switch((HX9035_OUTPUT_RAW_DIFF == value) ? HX9035_OUTPUT_RAW_DIFF : HX9035_OUTPUT_LP_BL);
    PRINT_INF("set hx9035_output_switch=%d\n", hx9035_output_switch);
    return count;
}

static int16_t hx9035_get_thres1_near(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[1] = {0};

    ret = hx9035_read(PAGE0_C9_PROX_HIGH_THRES1_CFG0 + ch, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
    }

    hx9035_ch_thres1[ch].thr_near = hx9035_threshold_near_num[buf[0]];
    return hx9035_ch_thres1[ch].thr_near;
}

static int16_t hx9035_get_thres1_far(uint8_t ch)
{
    int ret = 0;
    uint8_t buf[1] = {0};

    ret = hx9035_read(PAGE1_E1_PROX_LOW_THRES1_CFG0 + ch, buf, 1);
    if(0 != ret) {
        PRINT_ERR("hx9035_read failed\n");
    }

    hx9035_ch_thres1[ch].thr_far =  hx9035_ch_thres1[ch].thr_near * hx9035_threshold_far_num[buf[0]] / 1000;
    return hx9035_ch_thres1[ch].thr_far;
}

static int16_t hx9035_set_thres1_near(uint8_t ch, int16_t val)
{
    int ret = -1;
    int ii = 0;
    uint8_t buf[1] = {0};
    bool threshold_flag = false;

    for(ii = 0; ii < 63; ii++){
        if(hx9035_threshold_near_num[ii] == val){
            threshold_flag = true;
            buf[0] = ii & 0x3F;
            ret = hx9035_write(PAGE0_C9_PROX_HIGH_THRES1_CFG0 + ch, buf, 1);//6位！！！
            if(0 != ret) {
                PRINT_ERR("hx9035_write failed\n");
            }
            break;
        }
    }

    if (threshold_flag) {
        PRINT_INF("hx9035_ch_thres[%d].thr_near=%d\n", ch, hx9035_ch_thres1[ch].thr_near);
        return hx9035_ch_thres1[ch].thr_near;
    } else 
        return 0;
}


static ssize_t hx9035_threshold1_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ii = 0;
    bool threshold_flag = false;
    unsigned int ch = 0;
    unsigned int thr_near = 0;

    ENTER;
    if (sscanf(buf, "%d,%d", &ch, &thr_near) != 2) {
        PRINT_ERR("please input 2 numbers in DEC: ch,thr_near(eg: 0,512)\n");
        return -EINVAL;
    }

    for(ii = 0; ii < 63; ii++){
        if(hx9035_threshold_near_num[ii] == thr_near){
            threshold_flag = true;
        }
    }
    
    if(ch >= HX9035_CH_NUM || !threshold_flag) {
        PRINT_ERR("input value error! (valid value: ch=%d, thr_near=%d)\n", ch, thr_near);
        return -EINVAL;
    }

    PRINT_INF("set threshold1: ch=%d, thr_near1=0x%d\n", ch, thr_near);
    hx9035_set_thres1_near(ch, thr_near);

    return count;
}

static ssize_t hx9035_threshold1_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        hx9035_get_thres1_near(ii);
        hx9035_get_thres1_far(ii);
        PRINT_INF("ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9035_ch_thres1[ii].thr_near, hx9035_ch_thres1[ii].thr_far);
        p += snprintf(p, PAGE_SIZE, "ch_%d threshold: near=%-8d, far=%-8d\n", ii, hx9035_ch_thres1[ii].thr_near, hx9035_ch_thres1[ii].thr_far);
    }

    return (p - buf);
}

static ssize_t hx9035_thres_alg_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    unsigned int en = 0;
    unsigned int diff1 = 0;
    unsigned int diff2 = 0;

    ENTER;
    if (sscanf(buf, "%d,%d,%d", &en, &diff1, &diff2) != 3) {
        PRINT_ERR("please input 3 numbers in DEC: en, diff1_thres, diff2_thres(eg: 0,1000,1000)\n");
        return -EINVAL;//??
    }

     hx9035_thres_alg_en = en;
     hx9035_channel1_diff = diff1;
     hx9035_channel2_diff = diff2;

    PRINT_INF("set prox_status2_en: %d, channel1_diff = %d, channel2_diff = %d\n",
                    hx9035_thres_alg_en, hx9035_channel1_diff, hx9035_channel2_diff);

    return count;
}


static ssize_t hx9035_thres_alg_show(struct class *class, struct class_attribute *attr, char *buf)
{
    char *p = buf;

    PRINT_INF("set prox_status2_en: %d, channel1_diff = %d, channel2_diff = %d\n",
                    hx9035_thres_alg_en, hx9035_channel1_diff, hx9035_channel2_diff);

    p += snprintf(p, PAGE_SIZE, "en %d diff_thr1 =%d, diff_thr2=%d\n",
                    hx9035_thres_alg_en, hx9035_channel1_diff, hx9035_channel2_diff);

    return (p - buf);
}

static ssize_t hx9035_dump_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ret = -1;
    int ii = 0;
    char *p = buf;
    uint8_t rx_buf[1] = {0};
    
    hx9035_reg_init_list_size = ARRAY_SIZE(hx9035_reg_init_list);
    for(ii = 0; ii < hx9035_reg_init_list_size; ii++) {
        ret = hx9035_read(hx9035_reg_init_list[ii].addr, rx_buf, 1);
        if(0 != ret) {
            PRINT_ERR("hx9035_read failed\n");
        }
        PRINT_INF("0x%04X=0x%02X\n", hx9035_reg_init_list[ii].addr, rx_buf[0]);
        p += snprintf(p, PAGE_SIZE, "0x%04X=0x%02X\n", hx9035_reg_init_list[ii].addr, rx_buf[0]);
    }

    p += snprintf(p, PAGE_SIZE, "driver version:%s\n", HX9035_DRIVER_VER);
    return (p - buf);
}

static ssize_t hx9035_offset_dac_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    hx9035_read_offset_dac();

    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        PRINT_INF("data_offset_dac[%d]=%dpF\n", ii, data_offset_dac[ii] * 26 / 1000);
        p += snprintf(p, PAGE_SIZE, "ch[%d]=%dpF ", ii, data_offset_dac[ii] * 26 / 1000);
    }
    p += snprintf(p, PAGE_SIZE, "\n");

    return (p - buf);
}

static ssize_t hx9035_offset_csn_show(struct class *class, struct class_attribute *attr, char *buf)
{
    int ii = 0;
    char *p = buf;

    hx9035_read_offset_dac();
    
    data_offset_csn[0] = data_offset_dac[0] + data_offset_dac[3];
    data_offset_csn[1] = data_offset_dac[3];
    data_offset_csn[2] = 0;
    data_offset_csn[3] = 0;
    data_offset_csn[4] = data_offset_dac[2] + data_offset_dac[5];
    data_offset_csn[5] = data_offset_dac[4];
    data_offset_csn[6] = data_offset_dac[1] + data_offset_dac[4];
    data_offset_csn[7] = data_offset_dac[5];

    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        PRINT_INF("data_offset_csn[%d]=%dpF\n", ii, data_offset_csn[ii] * 26 / 1000);
        p += snprintf(p, PAGE_SIZE, "cs[%d]=%dpF ", ii, data_offset_csn[ii] * 26 / 1000);
    }
    p += snprintf(p, PAGE_SIZE, "\n");

    return (p - buf);
}


static ssize_t hx9035_reinitialize_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
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
        hx9035_reg_reinitialize();
    }
    return count;
}

static ssize_t hx9035_hw_monitor_en_show(struct class *class, struct class_attribute *attr, char *buf)
{
    PRINT_INF("hx9035_hw_monitor_en_flag=%d\n", hx9035_hw_monitor_en_flag);
    return sprintf(buf, "hx9035_hw_monitor_en_flag=%d\n", hx9035_hw_monitor_en_flag);
}

static ssize_t hx9035_hw_monitor_en_store(struct class *class, struct class_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    int value = 0;

    ret = kstrtoint(buf, 10, &value);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
        return count;
    }

    hx9035_hw_monitor_en_flag = (0 == value) ? 0 : 1;

    if(1 == hx9035_hw_monitor_en_flag) {
        cancel_delayed_work_sync(&(hx9035_pdata.hw_monitor_work));
        schedule_delayed_work(&hx9035_pdata.hw_monitor_work, msecs_to_jiffies(10000));
    } else {
        cancel_delayed_work_sync(&(hx9035_pdata.hw_monitor_work));
    }

    PRINT_INF("set hx9035_hw_monitor_en_flag=%d\n", hx9035_hw_monitor_en_flag);
    return count;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
static struct class_attribute class_attr_raw_data = __ATTR(raw_data, 0664, hx9035_raw_data_show, NULL);
static struct class_attribute class_attr_reg_write = __ATTR(reg_write,  0664, NULL, hx9035_register_write_store);
static struct class_attribute class_attr_reg_read = __ATTR(reg_read, 0664, NULL, hx9035_register_read_store);
static struct class_attribute class_attr_channel_en = __ATTR(channel_en, 0664, hx9035_channel_en_show, hx9035_channel_en_store);
static struct class_attribute class_attr_calibrate = __ATTR(calibrate, 0664, hx9035_manual_offset_calibration_show, hx9035_manual_offset_calibration_store);
static struct class_attribute class_attr_prox_state = __ATTR(prox_state, 0664, hx9035_prox_state_show, NULL);
static struct class_attribute class_attr_polling_period = __ATTR(polling_period, 0664, hx9035_polling_show, hx9035_polling_store);
static struct class_attribute class_attr_loglevel = __ATTR(loglevel, 0664, hx9035_loglevel_show, hx9035_loglevel_store);
static struct class_attribute class_attr_accuracy = __ATTR(accuracy, 0664, hx9035_accuracy_show, hx9035_accuracy_store);
static struct class_attribute class_attr_output_switch = __ATTR(output_switch, 0664, hx9035_output_switch_show, hx9035_output_switch_store);
static struct class_attribute class_attr_dump = __ATTR(dump, 0664, hx9035_dump_show, NULL);
static struct class_attribute class_attr_offset_dac = __ATTR(offset_dac, 0664, hx9035_offset_dac_show, NULL);
static struct class_attribute class_attr_offset_csn = __ATTR(offset_csn, 0664, hx9035_offset_csn_show, NULL);
static struct class_attribute class_attr_reinitialize = __ATTR(reinitialize,  0664, NULL, hx9035_reinitialize_store);
static struct class_attribute class_attr_hw_monitor_en = __ATTR(hw_monitor_en, 0664, hx9035_hw_monitor_en_show, hx9035_hw_monitor_en_store);
static struct class_attribute class_attr_threshold = __ATTR(threshold, 0664, hx9035_threshold1_show, hx9035_threshold1_store);
static struct class_attribute class_attr_thres_alg = __ATTR(thres_alg, 0664, hx9035_thres_alg_show, hx9035_thres_alg_store);

static struct attribute *hx9035_class_attrs[] = {
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
    &class_attr_output_switch.attr,
    &class_attr_dump.attr,
    &class_attr_offset_dac.attr,
    &class_attr_offset_csn.attr,
    &class_attr_reinitialize.attr,
    &class_attr_hw_monitor_en.attr,
    &class_attr_thres_alg.attr,
    NULL,
};
ATTRIBUTE_GROUPS(hx9035_class);
#else
static struct class_attribute hx9035_class_attributes[] = {
    __ATTR(raw_data, 0664, hx9035_raw_data_show, NULL),
    __ATTR(reg_write,  0664, NULL, hx9035_register_write_store),
    __ATTR(reg_read, 0664, NULL, hx9035_register_read_store),
    __ATTR(channel_en, 0664, hx9035_channel_en_show, hx9035_channel_en_store),
    __ATTR(calibrate, 0664, hx9035_manual_offset_calibration_show, hx9035_manual_offset_calibration_store),
    __ATTR(prox_state, 0664, hx9035_prox_state_show, NULL),
    __ATTR(polling_period, 0664, hx9035_polling_show, hx9035_polling_store),
    __ATTR(loglevel, 0664, hx9035_loglevel_show, hx9035_loglevel_store),
    __ATTR(accuracy, 0664, hx9035_accuracy_show, hx9035_accuracy_store),
    __ATTR(output_switch, 0664, hx9035_output_switch_show, hx9035_output_switch_store),
    __ATTR(dump, 0664, hx9035_dump_show, NULL),
    __ATTR(offset_dac, 0664, hx9035_offset_dac_show, NULL),
    __ATTR(offset_csn, 0664, hx9035_offset_csn_show, NULL),
    __ATTR(reinitialize,  0664, NULL, hx9035_reinitialize_store),
    __ATTR(hw_monitor_en, 0664, hx9035_hw_monitor_en_show, hx9035_hw_monitor_en_store),
    __ATTR(thres_alg, 0664, hx9035_thres_alg_show, hx9035_thres_alg_store),
    __ATTR_NULL,
};
#endif

#if defined(CONFIG_SENSORS)
static ssize_t hx9035_onoff_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", !hx9035_pdata.skip_data);
}

static ssize_t hx9035_onoff_store(struct device *dev,
        struct device_attribute *attr, const char *buf, size_t count)
{
    u8 val;
    int ret;

    ret = kstrtou8(buf, 2, &val);
    if (ret) {
        PRINT_ERR("%s - Invalid Argument\n", __func__);
        return ret;
    }

    if (val == 0) {
        hx9035_pdata.skip_data = true;
        if (hx9035_channels[0].enabled || hx9035_channels[1].enabled || hx9035_channels[2].enabled) {
            input_report_rel(hx9035_channels[0].hx9035_input_dev, REL_MISC, 2);                 
            input_sync(hx9035_channels[0].hx9035_input_dev);
            input_report_rel(hx9035_channels[1].hx9035_input_dev, REL_MISC, 2); 
            input_sync(hx9035_channels[1].hx9035_input_dev);
            input_report_rel(hx9035_channels[2].hx9035_input_dev, REL_MISC, 2); 
            input_sync(hx9035_channels[2].hx9035_input_dev);
        }
    } else {
        hx9035_pdata.skip_data = false;
    }

    PRINT_INF("%s -%u\n", __func__, val);
    return count;
}

static int hx9035_grip_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    input_report_rel(hx9035_channels[0].hx9035_input_dev, REL_MAX, val);
    input_sync(hx9035_channels[0].hx9035_input_dev);

    PRINT_INF("%s val=%u\n", __func__, val);
    return 0;
}

static int hx9035_grip_wifi_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    input_report_rel(hx9035_channels[2].hx9035_input_dev, REL_MAX, val);
    input_sync(hx9035_channels[2].hx9035_input_dev);

    PRINT_INF("%s -%u\n", __func__, val);
    return 0;
}

static int hx9035_grip_sub_flush(struct sensors_classdev *sensors_cdev,unsigned char val)
{
    input_report_rel(hx9035_channels[1].hx9035_input_dev, REL_MAX, val);
    input_sync(hx9035_channels[1].hx9035_input_dev);

    PRINT_INF("%s -%u\n", __func__, val);
    return 0;
}

static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
        hx9035_onoff_show, hx9035_onoff_store);
#endif

struct class hx9035_class = {
        .name = "hx9035",
        .owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0)
        .class_groups = hx9035_class_groups,
#else
        .class_attrs = hx9035_class_attributes,
#endif
    };
//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^sysfs for test end
static ssize_t cap_diff_dump_show(int ch, char *buf)
{
    ssize_t len = 0;
    int channel_nu = ch ? ch + 1 : ch;

    hx9035_read_offset_dac();
    len += snprintf(buf+len, PAGE_SIZE-len,
                    "CH%d_background_cap=%d;", channel_nu, data_offset_dac[ch] * 26 / 1000);
    len += snprintf(buf+len, PAGE_SIZE-len,
                    "CH%d_refer_channel_cap=%d;", channel_nu, data_offset_dac[ch + 3] * 26 / 1000);
    hx9035_sample();
    len += snprintf(buf+len, PAGE_SIZE-len, "CH%d_diff=%d", channel_nu, data_diff[ch]);

    return len;
}

static ssize_t ch0_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    ssize_t len = 0;

    len = cap_diff_dump_show(0, buf);

    return len;
}

static ssize_t ch2_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    ssize_t len = 0;

    len = cap_diff_dump_show(1, buf);

    return len;
}
static ssize_t ch3_cap_diff_dump_show(struct class *class,
                                      struct class_attribute *attr,
                                      char *buf)
{
    ssize_t len = 0;

    len = cap_diff_dump_show(2, buf);

    return len;
}
#if POWER_ENABLE
static ssize_t power_enable_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    return sprintf(buf, "%d\n", hx9035_pdata.power_state); 
}
static ssize_t power_enable_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;
    
    ret = kstrtoint(buf, 10, &hx9035_pdata.power_state);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
    }
    return count;
}
#endif

#if SAR_IN_FRANCE
void exit_anfr_func(void )
{
    if (hx9035_pdata.sar_first_boot) {
        hx9035_pdata.sar_first_boot = false;
        hx9035_pdata.anfr_export_exit = true;
        mutex_lock(&hx9035_ch_en_mutex);
        hx9035_state_monitoring(hx9035_monitor_addr, hx9035_monitor_val);
        hx9035_sample();
        hx9035_get_prox_state();
        hx9035_input_report();
        mutex_unlock(&hx9035_ch_en_mutex);
        hx9035_pdata.anfr_export_exit = false;
        PRINT_INF("hx9035:exit force input near mode, report sar state once!\n");
    }
}
EXPORT_SYMBOL(exit_anfr_func);


static ssize_t user_test_show(struct class *class,
                                 struct class_attribute *attr,
                                 char *buf)
{
    return sprintf(buf, "%d\n", hx9035_pdata.user_test);
}
static ssize_t user_test_store(struct class *class,
                                  struct class_attribute *attr,
                                  const char *buf, size_t count)
{
    int ret = -1;
 
    ret = kstrtoint(buf, 10, &hx9035_pdata.user_test);
    if (0 != ret) {
        PRINT_ERR("kstrtoint failed\n");
    }
	PRINT_INF("hx9035_pdata.user_test:%d", hx9035_pdata.user_test);
    if (hx9035_pdata.user_test) {
	    hx9035_pdata.sar_first_boot = false;
        PRINT_INF("hx9035_pdata.sar_first_boot:%d", hx9035_pdata.sar_first_boot);
        PRINT_INF("hx9035:user_test:%d, exit force input near mode!!!\n", hx9035_pdata.user_test);
        for(size_t ii = 0; ii < HX9035_CH_NUM; ii++) {
            if ((hx9035_pdata.channel_used_flag >> ii) & 0x1) {
                //mdelay(20);
                hx9035_manual_offset_calibration_1_ch(ii);
            }
        }
    }
    return count;
}
#endif

static struct class_attribute class_attr_ch0_cap_diff_dump = __ATTR(ch0_cap_diff_dump, 0664, ch0_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch2_cap_diff_dump = __ATTR(ch2_cap_diff_dump, 0664, ch2_cap_diff_dump_show, NULL);
static struct class_attribute class_attr_ch3_cap_diff_dump = __ATTR(ch3_cap_diff_dump, 0664, ch3_cap_diff_dump_show, NULL);
#if POWER_ENABLE
static struct class_attribute class_attr_power_enable = __ATTR(power_enable, 0664, power_enable_show, power_enable_store);
#endif
#if SAR_IN_FRANCE
static struct class_attribute class_attr_user_test = __ATTR(user_test, 0664, user_test_show, user_test_store);
#endif

static struct attribute *hx9035_capsense_attrs[] = {
    &class_attr_ch0_cap_diff_dump.attr,
    &class_attr_ch2_cap_diff_dump.attr,
    &class_attr_ch3_cap_diff_dump.attr,
#if POWER_ENABLE
    &class_attr_power_enable.attr,
#endif
#if SAR_IN_FRANCE
    &class_attr_user_test.attr,
#endif
    NULL,
};
ATTRIBUTE_GROUPS(hx9035_capsense);

static struct class capsense_class = {
    .name           = "capsense",
    .owner          = THIS_MODULE,
    .class_groups = hx9035_capsense_groups,
};

static int hx9035_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int type = 0;
    int ii = 0;
    int ret = 0;
    struct hx9035_channel_info *channel_p = NULL;
    
    ENTER;
    PRINT_INF("client->addr=0x%02X\n", client->addr);
    if (!i2c_check_functionality(to_i2c_adapter(client->dev.parent), I2C_FUNC_SMBUS_READ_WORD_DATA)) {
        PRINT_ERR("i2c_check_functionality failed\n");
        ret = -EIO;
        goto failed_i2c_check_functionality;
    }

    i2c_set_clientdata(client, (&hx9035_pdata));
    hx9035_i2c_client = client;
    hx9035_pdata.pdev = &client->dev;
    client->dev.platform_data = (&hx9035_pdata);

//{begin =============================================需要客户自行配置dts属性和实现上电相关内容
    ret = hx9035_parse_dt(&client->dev);//yasin: power, irq, regs
    if (ret) {
        PRINT_ERR("hx9035_parse_dt failed\n");
        ret = -ENODEV;
        goto failed_parse_dt;
    }

    ret = hx9035_gpio_init();
    if (ret) {
        PRINT_ERR("hx9035_gpio_init failed\n");
        ret = -1;
        goto failed_gpio_init;
    }

    hx9035_pdata.irq = gpio_to_irq(hx9035_pdata.irq_gpio);
    client->irq = hx9035_pdata.irq;
    PRINT_INF("hx9035_pdata.irq_gpio=%d hx9035_pdata.irq=%d\n", hx9035_pdata.irq_gpio, hx9035_pdata.irq);

    hx9035_power_on(1);
//}end =============================================================

    ret = hx9035_id_check();
    if(0 != ret) {
        PRINT_ERR("hx9035_id_check failed\n");
        goto failed_id_check;
    }

    hx9035_register_init();//寄存器初始化,如果需要，也可以使用chip_select做芯片区分
    hx9035_capcoe_init();
    hx9035_ch_cfg();//通道配置
    hx9035_data_switch(hx9035_output_switch);//数据输出默认选择raw 和 diff
	hx9035_ch_thres1 = hx9035_ch_thres1_default_hx9035;
	for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        hx9035_set_thres1_near(ii, hx9035_ch_thres1[ii].thr_near);
    }

    for (ii = 0; ii < ARRAY_SIZE(hx9035_channels); ii++) {
        if ((hx9035_pdata.channel_used_flag >> ii) & 0x1) { //used flag的值来自dts，表示通道有效性，每一位表示一个通道。
            hx9035_channels[ii].used = true;
            hx9035_channels[ii].state = IDLE;

            hx9035_channels[ii].hx9035_input_dev = input_allocate_device();
            if (!hx9035_channels[ii].hx9035_input_dev) {
                PRINT_ERR("input_allocate_device failed\n");
                ret = -ENOMEM;
                goto failed_allocate_input_dev;
            }

            hx9035_channels[ii].hx9035_input_dev->name = hx9035_channels[ii].name;
            
            if (ii == hx9035_pdata.grip_sensor_ch) {
                type = SENSOR_TYPE_SAR_MIAN;
                hx9035_channels[ii].hx9035_input_dev->name = "grip_sensor";
            } else if (ii == hx9035_pdata.grip_sensor_sub_ch) {
                type = SENSOR_TYPE_SAR_SUB;
                hx9035_channels[ii].hx9035_input_dev->name = "grip_sensor_sub";
            } else if (ii == hx9035_pdata.grip_sensor_wifi_ch) {
                type = SENSOR_TYPE_SAR_WCN;
                hx9035_channels[ii].hx9035_input_dev->name = "grip_sensor_wifi";
            }
            PRINT_INF("hx9035_input_dev name is %s\n",hx9035_channels[ii].hx9035_input_dev->name);

            input_set_capability(hx9035_channels[ii].hx9035_input_dev, EV_REL, REL_MISC);
            input_set_capability(hx9035_channels[ii].hx9035_input_dev, EV_REL, REL_MAX);
            __set_bit(EV_REL, hx9035_channels[ii].hx9035_input_dev->evbit);
            input_set_capability(hx9035_channels[ii].hx9035_input_dev, EV_REL, REL_X);

            ret = input_register_device(hx9035_channels[ii].hx9035_input_dev);
            if (ii <= 2) {
                if (0 != device_create_file(&hx9035_channels[ii].hx9035_input_dev->dev, &dev_attr_enable)) {
                    PRINT_INF("%s attribute ENABLE create fail\n", hx9035_channels[ii].hx9035_input_dev->name);
                }
            }

            input_report_rel(hx9035_channels[ii].hx9035_input_dev, REL_MISC, 2); 
            input_sync(hx9035_channels[ii].hx9035_input_dev);

            hx9035_channels[ii].hx9035_sensors_classdev.sensors_enable = hx9035_set_enable;
            hx9035_channels[ii].hx9035_sensors_classdev.sensors_poll_delay = NULL;
            hx9035_channels[ii].hx9035_sensors_classdev.sensor_name = hx9035_channels[ii].hx9035_input_dev->name;
            hx9035_channels[ii].hx9035_sensors_classdev.vendor = "TianYiHeXin";
            hx9035_channels[ii].hx9035_sensors_classdev.name = "HX9031";
            hx9035_channels[ii].hx9035_sensors_classdev.version = 1;
            hx9035_channels[ii].hx9035_sensors_classdev.type = type;
            hx9035_channels[ii].hx9035_sensors_classdev.max_range = "5";
            hx9035_channels[ii].hx9035_sensors_classdev.resolution = "5.0";
            hx9035_channels[ii].hx9035_sensors_classdev.sensor_power = "3";
            hx9035_channels[ii].hx9035_sensors_classdev.min_delay = 0;
            hx9035_channels[ii].hx9035_sensors_classdev.fifo_reserved_event_count = 0;
            hx9035_channels[ii].hx9035_sensors_classdev.fifo_max_event_count = 0;
            hx9035_channels[ii].hx9035_sensors_classdev.delay_msec = 100;
            hx9035_channels[ii].hx9035_sensors_classdev.enabled = 0;
            hx9035_channels[ii].enabled = false;

#if defined(CONFIG_SENSORS)
            if (ii == hx9035_pdata.grip_sensor_ch) {
                hx9035_channels[ii].hx9035_sensors_classdev.sensors_flush = hx9035_grip_flush;
            } else if (ii == hx9035_pdata.grip_sensor_sub_ch) {
                hx9035_channels[ii].hx9035_sensors_classdev.sensors_flush = hx9035_grip_sub_flush;
            } else if (ii == hx9035_pdata.grip_sensor_wifi_ch) {
                hx9035_channels[ii].hx9035_sensors_classdev.sensors_flush = hx9035_grip_wifi_flush;
            }
#endif

            ret = sensors_classdev_register(&hx9035_channels[ii].hx9035_input_dev->dev, &hx9035_channels[ii].hx9035_sensors_classdev);
            if (ret < 0) {
                PRINT_ERR("create %d cap sensor_class  file failed (%d)\n", ii, ret);
            }
        }
    }

#if defined(CONFIG_SENSORS)
    PRINT_INF("grip_sensor onoff created\n");
    device_create_file(hx9035_channels[0].hx9035_sensors_classdev.dev, &dev_attr_onoff);
#endif

    spin_lock_init(&hx9035_pdata.lock);
    INIT_DELAYED_WORK(&hx9035_pdata.polling_work, hx9035_polling_work_func);
    INIT_DELAYED_WORK(&hx9035_pdata.hw_monitor_work, hx9035_hw_monitor_work_func);
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
    wakeup_source_init(&hx9035_wake_lock, "hx9035_wakelock");
#else
    wake_lock_init(&hx9035_wake_lock, WAKE_LOCK_SUSPEND, "hx9035_wakelock");
#endif

    ret = request_threaded_irq(hx9035_pdata.irq, NULL, hx9035_irq_handler,
                               IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_NO_SUSPEND,
                               hx9035_pdata.pdev->driver->name, (&hx9035_pdata));
    if(ret < 0) {
        PRINT_ERR("request_irq failed irq=%d ret=%d\n", hx9035_pdata.irq, ret);
        goto failed_request_irq;
    }
    enable_irq_wake(hx9035_pdata.irq);//enable irq wakeup PM
    hx9035_irq_en_flag = 1;//irq is enabled after request by default

    //debug sys fs
    ret = class_register(&hx9035_class);
    if (ret < 0) {
        PRINT_ERR("class_register failed\n");
        goto failed_class_register;
    }

    // for WT factory
    ret = class_register(&capsense_class);
    if (ret < 0) {
        PRINT_ERR("register capsense class failed (%d)\n", &capsense_class);
        return ret;
    }
    PRINT_INF("capsense_class has been registered\n");

#if POWER_ENABLE
    hx9035_pdata.power_state = 0;
#endif

#if SAR_IN_FRANCE
    hx9035_pdata.sar_first_boot = true;
    hx9035_pdata.user_test  = 0;
#if USE_USB
    hx9035_pdata.ps_notif.notifier_call = ps_notify_callback;
    ret = power_supply_reg_notifier(&hx9035_pdata.ps_notif);
    if (ret < 0) {
        PRINT_ERR("Unable to register ps_notifier: %d\n", ret);
        goto failed_ps_notifier;
    }
#endif
#endif

#if (HX9035_TEST_ON_MTK_DEMO || HX9035_CHANNEL_ALL_EN)
    PRINT_INF("enable all channels for test\n");
    for(ii = 0; ii < HX9035_CH_NUM; ii++) {
        if ((hx9035_pdata.channel_used_flag >> ii) & 0x01) {
            hx9035_set_enable(&hx9035_channels[ii].hx9035_sensors_classdev, 1);
        }
    }
#endif

    PRINT_INF("probe success\n");
    hardwareinfo_set_prop(HARDWARE_SAR, "hx9036");
    return 0;

#if USE_USB
#if SAR_IN_FRANCE
failed_ps_notifier:
    power_supply_unreg_notifier(&hx9035_pdata.ps_notif);
#endif
#endif

failed_class_register:
    free_irq(hx9035_pdata.irq, (&hx9035_pdata));
failed_request_irq:
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
    wakeup_source_trash(&hx9035_wake_lock);
#else
    wake_lock_destroy(&hx9035_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9035_pdata.polling_work));
    for (ii = 0; ii < ARRAY_SIZE(hx9035_channels); ii++) {
        channel_p = &(hx9035_channels[ii]);
        if (channel_p->used == true) {
            sensors_classdev_unregister(&(channel_p->hx9035_sensors_classdev));
            input_unregister_device(channel_p->hx9035_input_dev);
        }
    }
failed_allocate_input_dev:
failed_id_check:
    hx9035_power_on(0);
    hx9035_gpio_deinit();
failed_gpio_init:
failed_parse_dt:
failed_i2c_check_functionality:
    PRINT_ERR("probe failed\n");
    return ret;
}

static int hx9035_remove(struct i2c_client *client)
{
    struct hx9035_channel_info *channel_p = NULL;
    int ii = 0;

    ENTER;
    cancel_delayed_work_sync(&(hx9035_pdata.hw_monitor_work));
    class_unregister(&hx9035_class);
    free_irq(hx9035_pdata.irq, (&hx9035_pdata));
//#ifdef CONFIG_PM_WAKELOCKS
#if 0
    wakeup_source_trash(&hx9035_wake_lock);
#else
    wake_lock_destroy(&hx9035_wake_lock);
#endif
    cancel_delayed_work_sync(&(hx9035_pdata.polling_work));
    for (ii = 0; ii < ARRAY_SIZE(hx9035_channels); ii++) {
        channel_p = &(hx9035_channels[ii]);
        if (channel_p->used == true) {
            sensors_classdev_unregister(&(channel_p->hx9035_sensors_classdev));
            input_unregister_device(channel_p->hx9035_input_dev);
        }
    }
    hx9035_power_on(0);
    hx9035_gpio_deinit();
    return 0;
}

static int hx9035_suspend(struct device *dev)
{
    ENTER;
    hx9035_irq_from_suspend_flag = 1;
    return 0;
}

static int hx9035_resume(struct device *dev)
{
    ENTER;
    hx9035_irq_from_suspend_flag = 0;
    hx9035_state_monitoring(hx9035_monitor_addr, hx9035_monitor_val);
    return 0;
}

static struct i2c_device_id hx9035_i2c_id_table[] = {
    {HX9035_DRIVER_NAME, 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, hx9035_i2c_id_table);
#ifdef CONFIG_OF
static struct of_device_id hx9035_of_match_table[] = {
#if HX9035_TEST_ON_MTK_DEMO_XY6761
    {.compatible = "mediatek,sar_hx9035"},
#else
    {.compatible = "tyhx,hx9035"},
#endif
    { },
};
#else
#define hx9035_of_match_table NULL
#endif

static const struct dev_pm_ops hx9035_pm_ops = {
    .suspend = hx9035_suspend,
    .resume = hx9035_resume,
};

static struct i2c_driver hx9035_i2c_driver = {
    .driver = {
        .owner = THIS_MODULE,
        .name = HX9035_DRIVER_NAME,
        .of_match_table = hx9035_of_match_table,
        .pm = &hx9035_pm_ops,
    },
    .id_table = hx9035_i2c_id_table,
    .probe = hx9035_probe,
    .remove = hx9035_remove,
};

static int __init hx9035_module_init(void)
{
    ENTER;
    PRINT_INF("driver version:%s\n", HX9035_DRIVER_VER);
    return i2c_add_driver(&hx9035_i2c_driver);
}

static void __exit hx9035_module_exit(void)
{
    ENTER;
    i2c_del_driver(&hx9035_i2c_driver);
}

rootfs_initcall(hx9035_module_init);
module_exit(hx9035_module_exit);

MODULE_AUTHOR("Yasin Lee <yasin.lee.x@gmail.com><yasin.lee@tianyihexin.com>");
MODULE_DESCRIPTION("Driver for NanJingTianYiHeXin HX9035 & HX9036 Cap Sense");
MODULE_ALIAS("sar driver");
MODULE_LICENSE("GPL");

