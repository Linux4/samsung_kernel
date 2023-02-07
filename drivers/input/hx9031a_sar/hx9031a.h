#ifndef __HX9031A_H
#define __HX9031A_H
#include "tyhx_log.h"

#define HX9031A_ON_BOARD 1
#define HX9023E_ON_BOARD 0

#define HX9031A_DRIVER_NAME "hx9031a" //i2c addr: HX9023E=0x28, HX9031A=0x28
#define HX9031A_CH_NUM 4
#define HX9031A_MAX_XFER_SIZE 32

#define HX9031A_ODR_MS 50

#define HX9031A_DATA_LOCK 1
#define HX9031A_DATA_UNLOCK 0

#define HX9031A_OUTPUT_RAW_DIFF 1
#define HX9031A_OUTPUT_LP_BL 0

#define CH_DATA_2BYTES 2
#define CH_DATA_3BYTES 3
#define CH_DATA_BYTES_MAX CH_DATA_3BYTES

#define HX9031A_ID_CHECK_COUNT 5
#define HX90XX_CHIP_ID_9031A 0x1D //23E和31A都是1D

#define IDLE 0 //远离
#define PROXACTIVE 1 //较近
#define BODYACTIVE 2 //更近

//寄存器列表
#define RA_GLOBAL_CTRL0_0x00              0x00
#define RA_GLOBAL_CTRL1_0x01              0x01
#define RA_PRF_CFG_0x02                   0x02
#define RA_CH0_CFG_7_0_0x03               0x03
#define RA_CH0_CFG_9_8_0x04               0x04
#define RA_CH1_CFG_7_0_0x05               0x05
#define RA_CH1_CFG_9_8_0x06               0x06
#define RA_CH2_CFG_7_0_0x07               0x07
#define RA_CH2_CFG_9_8_0x08               0x08
#define RA_CH3_CFG_7_0_0x09               0x09
#define RA_CH3_CFG_9_8_0x0A               0x0A
#define RA_RANGE_7_0_0x0D                 0x0D
#define RA_RANGE_9_8_0x0E                 0x0E
#define RA_TEMP_SENCE_CFG_0x0F            0x0F
#define RA_AVG0_NOSR0_CFG_0x10            0x10
#define RA_NOSR12_CFG_0x11                0x11
#define RA_NOSR43_CFG_0x12                0x12
#define RA_AVG12_CFG_0x13                 0x13
#define RA_AVG34_CFG_0x14                 0x14
#define RA_OFFSET_DAC0_7_0_0x15           0x15
#define RA_OFFSET_DAC0_9_8_0x16           0x16
#define RA_OFFSET_DAC1_7_0_0x17           0x17
#define RA_OFFSET_DAC1_9_8_0x18           0x18
#define RA_OFFSET_DAC2_7_0_0x19           0x19
#define RA_OFFSET_DAC2_9_8_0x1a           0x1a
#define RA_OFFSET_DAC3_7_0_0x1b           0x1b
#define RA_OFFSET_DAC3_9_8_0x1c           0x1c
#define RA_SAMPLE_NUM_7_0_0x1f            0x1f
#define RA_SAMPLE_NUM_9_8_0x20            0x20
#define RA_INTEGRATION_NUM_7_0_0x21       0x21
#define RA_INTEGRATION_NUM_9_8_0x22       0x22
#define RA_CH_NUM_CFG_0x24                0x24
#define RA_DAC_SWAP_CFG_0x25              0x25
#define RA_SAMPLE_SWAP_CFG_0x26           0x26
#define RA_HUGE_OFFSET_CFG_0x27           0x27
#define RA_MOD_RST_CFG_0x28               0x28
#define RA_LP_ALP_1_0_CFG_0x2a            0x2a
#define RA_LP_ALP_3_2_CFG_0x2b            0x2b
#define RA_UP_ALP_1_0_CFG_0x2c            0x2c
#define RA_UP_ALP_3_2_CFG_0x2d            0x2d
#define RA_DN_UP_ALP_0_4_CFG_0x2e         0x2e
#define RA_DN_ALP_2_1_CFG_0x2f            0x2f
#define RA_DN_ALP_4_3_CFG_0x30            0x30
#define RA_INT_CAP_CFG_0x31               0x31
#define RA_AMP_BIAS_CFG_0x32              0x32
#define RA_NDL_DLY_4_CFG_0x33             0x33
#define RA_FORCE_NO_UP_CFG_0x35           0x35
#define RA_CIC_MODE_CFG_0x37              0x37
#define RA_RAW_BL_RD_CFG_0x38             0x38
#define RA_INTERRUPT_CFG_0x39             0x39
#define RA_CALI_DIFF_CFG_0x3b             0x3b
#define RA_DITHER_CFG_0x3C                0x3C
#define RA_ANALOG_MEM0_WRDATA_7_0_0x40    0x40
#define RA_ANALOG_MEM0_WRDATA_23_16_0x42  0x42
#define RA_ANALOG_MOD_AMP_COMP_0x50       0x50
#define RA_ID_REG_0x60                    0x60
#define RA_TEMP_DATA_7_0_0x62             0x62
#define RA_VOL_TEMP_9_8_0x63              0x63
#define RA_FLAG_RD_0x66                   0x66
#define RA_CONV_TIMEOUT_CNT_0x6a          0x6a
#define RA_PROX_STATUS_0x6b               0x6b
#define RA_PROX_INT_HIGH_CFG_0x6c         0x6c
#define RA_PROX_INT_LOW_CFG_0x6d          0x6d
#define RA_CAP_INI_CFG_0x6e               0x6e
#define RA_INT_WIDTH_CFG0_0x6f            0x6f
#define RA_INT_WIDTH_CFG1_0x70            0x70
#define RA_PROX_HIGH_DIFF_CFG_CH0_0_0x80  0x80
#define RA_PROX_HIGH_DIFF_CFG_CH0_1_0x81  0x81
#define RA_PROX_HIGH_DIFF_CFG_CH1_0_0x82  0x82
#define RA_PROX_HIGH_DIFF_CFG_CH1_1_0x83  0x83
#define RA_PROX_HIGH_DIFF_CFG_CH2_0_0x84  0x84
#define RA_PROX_HIGH_DIFF_CFG_CH2_1_0x85  0x85
#define RA_PROX_HIGH_DIFF_CFG_CH3_0_0x86  0x86
#define RA_PROX_HIGH_DIFF_CFG_CH3_1_0x87  0x87
#define RA_PROX_LOW_DIFF_CFG_CH0_0_0x88   0x88
#define RA_PROX_LOW_DIFF_CFG_CH0_1_0x89   0x89
#define RA_PROX_LOW_DIFF_CFG_CH1_0_0x8a   0x8a
#define RA_PROX_LOW_DIFF_CFG_CH1_1_0x8b   0x8b
#define RA_PROX_LOW_DIFF_CFG_CH2_0_0x8c   0x8c
#define RA_PROX_LOW_DIFF_CFG_CH2_1_0x8d   0x8d
#define RA_PROX_LOW_DIFF_CFG_CH3_0_0x8e   0x8e
#define RA_PROX_LOW_DIFF_CFG_CH3_1_0x8f   0x8f
#define RA_BL_OUT_CH0_0_0x90              0x90
#define RA_BL_OUT_CH0_1_0x91              0x91
#define RA_BL_OUT_CH0_2_0x92              0x92
#define RA_BL_OUT_CH1_0_0x93              0x93
#define RA_BL_OUT_CH1_1_0x94              0x94
#define RA_BL_OUT_CH1_2_0x95              0x95
#define RA_BL_OUT_CH2_0_0x96              0x96
#define RA_BL_OUT_CH2_1_0x97              0x97
#define RA_BL_OUT_CH2_2_0x98              0x98
#define RA_BL_OUT_CH3_0_0x99              0x99
#define RA_BL_OUT_CH3_1_0x9a              0x9a
#define RA_BL_OUT_CH3_2_0x9b              0x9b
#define RA_LP_OUT_CH0_0_0xa0              0xa0
#define RA_LP_OUT_CH0_1_0xa1              0xa1
#define RA_LP_OUT_CH0_2_0xa2              0xa2
#define RA_LP_OUT_CH1_0_0xa3              0xa3
#define RA_LP_OUT_CH1_1_0xa4              0xa4
#define RA_LP_OUT_CH1_2_0xa5              0xa5
#define RA_LP_OUT_CH2_0_0xa6              0xa6
#define RA_LP_OUT_CH2_1_0xa7              0xa7
#define RA_LP_OUT_CH2_2_0xa8              0xa8
#define RA_LP_OUT_CH3_0_0xa9              0xa9
#define RA_LP_OUT_CH3_1_0xaa              0xaa
#define RA_LP_OUT_CH3_2_0xab              0xab
#define RA_RAW_DATA_CH0_0_0xb0            0xb0
#define RA_RAW_DATA_CH0_1_0xb1            0xb1
#define RA_RAW_DATA_CH0_2_0xb2            0xb2
#define RA_RAW_DATA_CH1_0_0xb3            0xb3
#define RA_RAW_DATA_CH1_1_0xb4            0xb4
#define RA_RAW_DATA_CH1_2_0xb5            0xb5
#define RA_RAW_DATA_CH2_0_0xb6            0xb6
#define RA_RAW_DATA_CH2_1_0xb7            0xb7
#define RA_RAW_DATA_CH2_2_0xb8            0xb8
#define RA_RAW_DATA_CH3_0_0xb9            0xb9
#define RA_RAW_DATA_CH3_1_0xba            0xba
#define RA_RAW_DATA_CH3_2_0xbb            0xbb
#define RA_CH0_SAMP_CFG_0xbf              0xbf
#define RA_CH10_SCAN_FACTOR_0xc0          0xc0
#define RA_CH32_SCAN_FACTOR_0xc1          0xc1
#define RA_OFFSET_CALI_CTRL_0xc2          0xc2
#define RA_DSP_CONFIG_CTRL0_0xc3          0xc3
#define RA_CH10_DOZE_FACTOR_0xc4          0xc4
#define RA_CH32_DOZE_FACTOR_0xc5          0xc5
#define RA_CH10_PROX_FACTOR_0xc6          0xc6
#define RA_CH32_PROX_FACTOR_0xc7          0xc7
#define RA_DSP_CONFIG_CTRL1_0xc8          0xc8
#define RA_DSP_CONFIG_CTRL2_0xc9          0xc9
#define RA_DSP_CONFIG_CTRL3_0xca          0xca
#define RA_DEC_DATA0_0xcb                 0xcb
#define RA_DEC_DATA1_0xcc                 0xcc
#define RA_DEC_DATA2_0xcd                 0xcd
#define RA_DEC_DATA3_0xce                 0xce
#define RA_DEC_DATA4_0xcf                 0xcf
#define RA_CAP_INI_CH0_0_0xe0             0xe0
#define RA_CAP_INI_CH0_1_0xe1             0xe1
#define RA_CAP_INI_CH1_0_0xe2             0xe2
#define RA_CAP_INI_CH1_1_0xe3             0xe3
#define RA_CAP_INI_CH2_0_0xe4             0xe4
#define RA_CAP_INI_CH2_1_0xe5             0xe5
#define RA_CAP_INI_CH3_0_0xe6             0xe6
#define RA_CAP_INI_CH3_1_0xe7             0xe7
#define RA_RAW_BL_CH0_0_0xe8              0xe8
#define RA_RAW_BL_CH0_1_0xe9              0xe9
#define RA_RAW_BL_CH0_2_0xea              0xea
#define RA_RAW_BL_CH1_0_0xeb              0xeb
#define RA_RAW_BL_CH1_1_0xec              0xec
#define RA_RAW_BL_CH1_2_0xed              0xed
#define RA_RAW_BL_CH2_0_0xee              0xee
#define RA_RAW_BL_CH2_1_0xef              0xef
#define RA_RAW_BL_CH2_2_0xf0              0xf0
#define RA_RAW_BL_CH3_0_0xf1              0xf1
#define RA_RAW_BL_CH3_1_0xf2              0xf2
#define RA_RAW_BL_CH3_2_0xf3              0xf3
#define RA_LP_DIFF_CH0_0_0xf4             0xf4
#define RA_LP_DIFF_CH0_1_0xf5             0xf5
#define RA_LP_DIFF_CH0_2_0xf6             0xf6
#define RA_LP_DIFF_CH1_0_0xf7             0xf7
#define RA_LP_DIFF_CH1_1_0xf8             0xf8
#define RA_LP_DIFF_CH1_2_0xf9             0xf9
#define RA_LP_DIFF_CH2_0_0xfa             0xfa
#define RA_LP_DIFF_CH2_1_0xfb             0xfb
#define RA_LP_DIFF_CH2_2_0xfc             0xfc
#define RA_LP_DIFF_CH3_0_0xfd             0xfd
#define RA_LP_DIFF_CH3_1_0xfe             0xfe
#define RA_LP_DIFF_CH3_2_0xff             0xff

struct hx9031a_near_far_threshold {
    int16_t thr_near;
    int16_t thr_far;
};

struct hx9031a_addr_val_pair {
    uint8_t addr;
    uint8_t val;
};

struct hx9031a_channel_info {
    char *name;
    bool enabled;//该通道是否已经打开
    bool used;   //该通道是否有效，如：对于9023E ch3是无效的，此时used应该为0
    int state;
    struct sensors_classdev hx9031a_sensors_classdev;
    struct input_dev *hx9031a_input_dev;
};

struct hx9031a_platform_data {
    struct device *pdev;
    struct delayed_work polling_work;
    struct delayed_work hw_monitor_work;
	#ifdef ANFR_TEST
	struct delayed_work report_ch0_work;
    struct delayed_work report_ch1_work;
    struct delayed_work report_ch2_work;
	#endif
    spinlock_t lock;
    uint32_t channel_used_flag;
    int irq;      //irq number
    int irq_gpio; //gpio numver
    char irq_disabled;
    uint32_t prox_state_reg;//寄存器显示的prox状态
    uint32_t prox_state_cmp;//data和阈值比较后的prox状态
    uint32_t prox_state_reg_pre;
    uint32_t prox_state_cmp_pre;
    uint32_t prox_state_far_to_near_flag;
    uint32_t grip_sensor_ch;	
	uint32_t grip_sensor_sub_ch;
	uint32_t grip_sensor_wifi_ch;
	#ifdef ANFR_TEST
	uint32_t back_cap[3];
	bool ps_is_present;
	bool hx_first_boot;
	int interrupt_count;
	#endif
};

static struct hx9031a_channel_info hx9031a_channels[] = {
    {
        .name = "hx9031a_ch0",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9031a_ch1",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9031a_ch2",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9031a_ch3",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9031a_ch4",
        .enabled = false,
        .used = false,
    },
};

static struct hx9031a_addr_val_pair hx9031a_reg_init_list_hx9023e[] = {
    {RA_CH_NUM_CFG_0x24,               0x00}, //close channel.
    {RA_GLOBAL_CTRL1_0x01,             0x82}, //open chop
    {RA_GLOBAL_CTRL0_0x00,             0x40}, //open dither
    {RA_PRF_CFG_0x02,                  0x0f}, //重要！！此处修改后要同步修改宏定义 HX9031A_ODR_MS; 0x0f:50ms
    {RA_RANGE_7_0_0x0D,                0x23}, //range ch1 ch0 1.25p
    {RA_RANGE_9_8_0x0E,                0x00}, //range ch3 1.25p ch2 1.25pf
    {RA_AVG0_NOSR0_CFG_0x10,           0x15}, //ch0 osr 512 avg 1 warm 16.
    {RA_NOSR12_CFG_0x11,               0x55}, //ch2 osr 512; ch1 osr 512
    {RA_NOSR43_CFG_0x12,               0x10}, //ch3 osr 512
    {RA_AVG12_CFG_0x13,                0x00}, //ch1 and ch2 avg 1
    {RA_AVG34_CFG_0x14,                0x00}, //ch3 avg 1
    {RA_SAMPLE_NUM_7_0_0x1f,           0x65}, //sample frequency 0x96: 31K. 0x4B 62K, 0x3a 83K
    {RA_INTEGRATION_NUM_7_0_0x21,      0x65}, //sample frequency 0x96: 31K. 0x4B 62K, 0x3a 83K
    {RA_MOD_RST_CFG_0x28,              0xf5}, //change offset cali delay
    {RA_LP_ALP_1_0_CFG_0x2a,           0x22}, //ch1 LP alpha 4. ch0 LP alpha 4
    {RA_LP_ALP_3_2_CFG_0x2b,           0x02}, //ch3 LP alpha 4. ch2 LP alpha 4
    {RA_UP_ALP_1_0_CFG_0x2c,           0x88}, //ch1 ch0 BL UP alpha 128
    {RA_UP_ALP_3_2_CFG_0x2d,           0x88}, //ch3 ch2 BL UP alpha 128
    {RA_DN_UP_ALP_0_4_CFG_0x2e,        0x10}, //ch0 BL DN alpha 4
    {RA_DN_ALP_2_1_CFG_0x2f,           0x11}, //ch2 ch1 BL DN alpha 4
    {RA_DN_ALP_4_3_CFG_0x30,           0x01}, //ch3 BL DN alpha 4
    {RA_RAW_BL_RD_CFG_0x38,            0xf0}, //7:4 bl lp
    {RA_INTERRUPT_CFG_0x39,            0xFF}, //开启所有通道的低阈值中断、开启所有通道的高阈值中断，每个通道对应一个bit
    {RA_ANALOG_MEM0_WRDATA_7_0_0x40,   0x57}, //write default value.
    {RA_CALI_DIFF_CFG_0x3b,            0x01}, //cali diff 16384 bit7:4 int_conversion_ready
    {RA_ANALOG_MOD_AMP_COMP_0x50,      0xb5}, //increase amp current
    {RA_PROX_INT_HIGH_CFG_0x6c,        0x04}, //prox int high count.  接近中断防抖
    {RA_PROX_INT_LOW_CFG_0x6d,         0x04}, //prox int low count.  远离中断防抖
    {RA_CAP_INI_CFG_0x6e,              0x00}, //close cap ini calibration

    {RA_PROX_HIGH_DIFF_CFG_CH0_0_0x80, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH0_1_0x81, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH1_0_0x82, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH1_1_0x83, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH2_0_0x84, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH2_1_0x85, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH3_0_0x86, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH3_1_0x87, 0x00},

    {RA_PROX_LOW_DIFF_CFG_CH0_0_0x88,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH0_1_0x89,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH1_0_0x8a,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH1_1_0x8b,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH2_0_0x8c,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH2_1_0x8d,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH3_0_0x8e,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH3_1_0x8f,  0x00},

    {RA_CH0_SAMP_CFG_0xbf,             0x00},
    {RA_CH10_SCAN_FACTOR_0xc0,         0x00}, // 0x32: ch1 odr * 8 ch0 odr * 4.
    {RA_CH32_SCAN_FACTOR_0xc1,         0x00}, // 0x70: ch3 odr * 128 (samle 1 per 128 period).
    {RA_OFFSET_CALI_CTRL_0xc2,         0x00},
    {RA_DSP_CONFIG_CTRL0_0xc3,         0xF0}, //baseline cali en
    {RA_DSP_CONFIG_CTRL3_0xca,         0x00}, //lp up threshold 0.

    {RA_CH0_CFG_7_0_0x03,              0x00}, //channel config
    {RA_CH0_CFG_9_8_0x04,              0x00},
    {RA_CH1_CFG_7_0_0x05,              0x00},
    {RA_CH1_CFG_9_8_0x06,              0x00},
    {RA_CH2_CFG_7_0_0x07,              0x00},
    {RA_CH2_CFG_9_8_0x08,              0x00},
    {RA_CH3_CFG_7_0_0x09,              0x00},
    {RA_CH3_CFG_9_8_0x0A,              0x00},

    //{RA_CH_NUM_CFG_0x24,              0x0F}, //open channel.
};

static struct hx9031a_addr_val_pair hx9031a_reg_init_list_hx9031a[] = {
    {RA_CH_NUM_CFG_0x24,               0x00}, //close channel.
    {RA_GLOBAL_CTRL1_0x01,             0x82}, //open chop
    {RA_GLOBAL_CTRL0_0x00,             0x40}, //open dither
    {RA_PRF_CFG_0x02,                  0x0f}, //重要！！此处修改后要同步修改宏定义 HX9031A_ODR_MS; 0x0f:50ms
    {RA_RANGE_7_0_0x0D,                0x23}, //range ch1 ch0 1.25p
    {RA_RANGE_9_8_0x0E,                0x00}, //range ch3 1.25p ch2 1.25pf
    {RA_AVG0_NOSR0_CFG_0x10,           0x15}, //ch0 osr 512 avg 1 warm 16.
    {RA_NOSR12_CFG_0x11,               0x55}, //ch2 osr 512; ch1 osr 512
    {RA_NOSR43_CFG_0x12,               0x10}, //ch3 osr 512
    {RA_AVG12_CFG_0x13,                0x00}, //ch1 and ch2 avg 1
    {RA_AVG34_CFG_0x14,                0x00}, //ch3 avg 1
    {RA_SAMPLE_NUM_7_0_0x1f,           0x65}, //sample frequency 0x96: 31K. 0x4B 62K, 0x3a 83K
    {RA_INTEGRATION_NUM_7_0_0x21,      0x65}, //sample frequency 0x96: 31K. 0x4B 62K, 0x3a 83K
    {RA_MOD_RST_CFG_0x28,              0xf5}, //change offset cali delay
    {RA_LP_ALP_1_0_CFG_0x2a,           0x22}, //ch1 LP alpha 4. ch0 LP alpha 4
    {RA_LP_ALP_3_2_CFG_0x2b,           0x02}, //ch3 LP alpha 4. ch2 LP alpha 4
    {RA_UP_ALP_1_0_CFG_0x2c,           0x88}, //ch1 ch0 BL UP alpha 128
    {RA_UP_ALP_3_2_CFG_0x2d,           0x88}, //ch3 ch2 BL UP alpha 128
    {RA_DN_UP_ALP_0_4_CFG_0x2e,        0x10}, //ch0 BL DN alpha 4
    {RA_DN_ALP_2_1_CFG_0x2f,           0x11}, //ch2 ch1 BL DN alpha 4
    {RA_DN_ALP_4_3_CFG_0x30,           0x01}, //ch3 BL DN alpha 4
    {RA_RAW_BL_RD_CFG_0x38,            0xf0}, //7:4 bl lp
    {RA_INTERRUPT_CFG_0x39,            0xFF}, //开启所有通道的低阈值中断、开启所有通道的高阈值中断，每个通道对应一个bit
    {RA_ANALOG_MEM0_WRDATA_7_0_0x40,   0x57}, //write default value.
    #ifdef ANFR_TEST
    {RA_CALI_DIFF_CFG_0x3b,            0x01}, //cali diff 16384 bit7:4 int_conversion_ready
    #else
	{RA_CALI_DIFF_CFG_0x3b,            0x07}, //cali diff 16384 bit7:4 int_conversion_ready
	#endif
    {RA_ANALOG_MOD_AMP_COMP_0x50,      0xb5}, //increase amp current
    {RA_PROX_INT_HIGH_CFG_0x6c,        0x04}, //prox int high count.  接近中断防抖
    {RA_PROX_INT_LOW_CFG_0x6d,         0x04}, //prox int low count.  远离中断防抖
    #ifdef ANFR_TEST
    {RA_CAP_INI_CFG_0x6e,              0x02}, //close cap ini calibration
	#else 
	{RA_CAP_INI_CFG_0x6e,              0x00}, //close cap ini calibration
	#endif
    {RA_PROX_HIGH_DIFF_CFG_CH0_0_0x80, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH0_1_0x81, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH1_0_0x82, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH1_1_0x83, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH2_0_0x84, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH2_1_0x85, 0x00},
    {RA_PROX_HIGH_DIFF_CFG_CH3_0_0x86, 0x80}, //high int 0x3FF*32
    {RA_PROX_HIGH_DIFF_CFG_CH3_1_0x87, 0x00},

    {RA_PROX_LOW_DIFF_CFG_CH0_0_0x88,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH0_1_0x89,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH1_0_0x8a,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH1_1_0x8b,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH2_0_0x8c,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH2_1_0x8d,  0x00},
    {RA_PROX_LOW_DIFF_CFG_CH3_0_0x8e,  0x60}, //low int 0x2FF*32
    {RA_PROX_LOW_DIFF_CFG_CH3_1_0x8f,  0x00},

    {RA_CH0_SAMP_CFG_0xbf,             0x00},
    {RA_CH10_SCAN_FACTOR_0xc0,         0x00}, // 0x32: ch1 odr * 8 ch0 odr * 4.
    {RA_CH32_SCAN_FACTOR_0xc1,         0x00}, // 0x70: ch3 odr * 128 (samle 1 per 128 period).
    {RA_OFFSET_CALI_CTRL_0xc2,         0x00},
    {RA_DSP_CONFIG_CTRL0_0xc3,         0xF0}, //baseline cali en
    {RA_DSP_CONFIG_CTRL3_0xca,         0x00}, //lp up threshold 0.

    {RA_CH0_CFG_7_0_0x03,              0x00}, //channel config
    {RA_CH0_CFG_9_8_0x04,              0x00},
    {RA_CH1_CFG_7_0_0x05,              0x00},
    {RA_CH1_CFG_9_8_0x06,              0x00},
    {RA_CH2_CFG_7_0_0x07,              0x00},
    {RA_CH2_CFG_9_8_0x08,              0x00},
    {RA_CH3_CFG_7_0_0x09,              0x00},
    {RA_CH3_CFG_9_8_0x0A,              0x00},

    //{RA_CH_NUM_CFG_0x24,              0x0F}, //open channel.
};
#if 0
static struct sensors_classdev sensors_capsensor_ch0_cdev = {
    .name = "grip_sensor",
    .vendor = "TianYiHeXin",
    .version = 1,
    .type = SENSOR_TYPE_TYHX_CAPSENSE,
    .max_range = "5",
    .resolution = "5.0",
    .sensor_power = "3",
    .min_delay = 0,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 100,
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_capsensor_ch2_cdev = {
    .name = "grip_sensor_sub",
    .vendor = "TianYiHeXin",
    .version = 1,
    .type = SENSOR_TYPE_TYHX_CAPSENSE,
    .max_range = "5",
    .resolution = "5.0",
    .sensor_power = "3",
    .min_delay = 0,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 100,
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
};

static struct sensors_classdev sensors_capsensor_ch3_cdev = {
    .name = "grip_sensor_wifi",
    .vendor = "TianYiHeXin",
    .version = 1,
    .type = SENSOR_TYPE_TYHX_CAPSENSE,
    .max_range = "5",
    .resolution = "5.0",
    .sensor_power = "3",
    .min_delay = 0,
    .fifo_reserved_event_count = 0,
    .fifo_max_event_count = 0,
    .enabled = 0,
    .delay_msec = 100,
    .sensors_enable = NULL,
    .sensors_poll_delay = NULL,
};
#endif
#endif
