#ifndef __HX9023S_H
#define __HX9023S_H
#include "tyhx_log.h"

#define HX9023S_REPORT_EVKEY 0            //选择上报事件类型report 1:EV_KEY 0:EV_ABS
#if !HX9023S_REPORT_EVKEY
#include <linux/sensors.h>
#endif

#define HX9023S_TEST_ON_MTK_DEMO_XY6761 0 //仅在6761开发板调试时置1,客户平台务必置0
//#define HX9023S_TEST_CHS_EN 1             //开机打开所有通道

#define HX9023S_DRIVER_NAME "hx9023s" //i2c addr: HX9023S=0x2A | 0x28

#define HX9023S_CH_NUM 3
#define HX9023S_MAX_XFER_SIZE 32

#define HX9023S_ODR_MS 50

#define HX9023S_DATA_LOCK 1
#define HX9023S_DATA_UNLOCK 0

#define CH_DATA_2BYTES 2
#define CH_DATA_3BYTES 3
#define CH_DATA_BYTES_MAX CH_DATA_3BYTES

#define HX9023S_ID_CHECK_COUNT 2
#define HX9023S_CHIP_ID 0x1D //chip_id是0x1D

#define IDLE 0 //远离
#define PROXACTIVE 1 //接近

//寄存器列表
#define RW_00_GLOBAL_CTRL0                   0x00
#define RW_01_GLOBAL_CTRL1                   0x01
#define RW_02_PRF_CFG                        0x02
#define RW_03_CH0_CFG_7_0                    0x03
#define RW_04_CH0_CFG_9_8                    0x04
#define RW_05_CH1_CFG_7_0                    0x05
#define RW_06_CH1_CFG_9_8                    0x06
#define RW_07_CH2_CFG_7_0                    0x07
#define RW_08_CH2_CFG_9_8                    0x08
#define RW_09_CH3_CFG_7_0                    0x09
#define RW_0A_CH3_CFG_9_8                    0x0a
#define RW_0B_CH4_CFG_7_0                    0x0b
#define RW_0C_CH4_CFG_9_8                    0x0c
#define RW_0D_RANGE_7_0                      0x0d
#define RW_0E_RANGE_9_8                      0x0e
#define RW_0F_RANGE_18_16                    0x0f
#define RW_10_AVG0_NOSR0_CFG                 0x10
#define RW_11_NOSR12_CFG                     0x11
#define RW_12_NOSR34_CFG                     0x12
#define RW_13_AVG12_CFG                      0x13
#define RW_14_AVG34_CFG                      0x14
#define RW_15_OFFSET_DAC0_7_0                0x15
#define RW_16_OFFSET_DAC0_9_8                0x16
#define RW_17_OFFSET_DAC1_7_0                0x17
#define RW_18_OFFSET_DAC1_9_8                0x18
#define RW_19_OFFSET_DAC2_7_0                0x19
#define RW_1A_OFFSET_DAC2_9_8                0x1a
#define RW_1B_OFFSET_DAC3_7_0                0x1b
#define RW_1C_OFFSET_DAC3_9_8                0x1c
#define RW_1D_OFFSET_DAC4_7_0                0x1d
#define RW_1E_OFFSET_DAC4_9_8                0x1e
#define RW_1F_SAMPLE_NUM_7_0                 0x1f
#define RW_20_SAMPLE_NUM_9_8                 0x20
#define RW_21_INTEGRATION_NUM_7_0            0x21
#define RW_22_INTEGRATION_NUM_9_8            0x22
#define RW_23_GLOBAL_CTRL2                   0x23
#define RW_24_CH_NUM_CFG                     0x24
#define RW_25_DAC_SWAP_CFG                   0x25
#define RW_28_MOD_RST_CFG                    0x28
#define RW_29_LP_ALP_4_CFG                   0x29
#define RW_2A_LP_ALP_1_0_CFG                 0x2a
#define RW_2B_LP_ALP_3_2_CFG                 0x2b
#define RW_2C_UP_ALP_1_0_CFG                 0x2c
#define RW_2D_UP_ALP_3_2_CFG                 0x2d
#define RW_2E_DN_UP_ALP_0_4_CFG              0x2e
#define RW_2F_DN_ALP_2_1_CFG                 0x2f
#define RW_30_DN_ALP_4_3_CFG                 0x30
#define RW_31_INT_CAP_CFG                    0x31
#define RW_33_NDL_DLY_4_CFG                  0x33
#define RW_35_FORCE_NO_UP_CFG                0x35
#define RW_38_RAW_BL_RD_CFG                  0x38
#define RW_39_INTERRUPT_CFG                  0x39
#define RW_3A_INTERRUPT_CFG1                 0x3a
#define RW_3B_CALI_DIFF_CFG                  0x3b
#define RW_3C_DITHER_CFG                     0x3c
#define RW_40_ANALOG_MEM0_WRDATA_7_0         0x40
#define RW_41_ANALOG_MEM0_WRDATA_15_8        0x41
#define RW_42_ANALOG_MEM0_WRDATA_23_16       0x42
#define RW_43_ANALOG_MEM0_WRDATA_31_24       0x43
#define RW_48_ANALOG_PWE_PULSE_CYCLE7_0      0x48
#define RW_49_ANALOG_PWE_PULSE_CYCLE12_8     0x49
#define RW_4A_ANALOG_MEM_GLOBAL_CTRL         0x4a
#define RO_4B_DEBUG_MEM_ADC_FSM              0x4b
#define RW_4C_ANALOG_MEM_GLOBAL_CTRL1        0x4c
#define RO_5F_VERION_ID                      0x5f
#define RO_60_DEVICE_ID                      0x60
#define RO_61_TC_FSM                         0x61
#define RO_66_FLAG_RD                        0x66
#define RO_6A_CONV_TIMEOUT_CNT               0x6a
#define RO_6B_PROX_STATUS                    0x6b
#define RW_6C_PROX_INT_HIGH_CFG              0x6c
#define RW_6D_PROX_INT_LOW_CFG               0x6d
#define RW_6E_CAP_INI_CFG                    0x6e
#define RW_6F_INT_WIDTH_CFG0                 0x6f
#define RW_70_INT_WIDTH_CFG1                 0x70
#define RO_71_INT_STATE_RD0                  0x71
#define RO_72_INT_STATE_RD1                  0x72
#define RO_73_INT_STATE_RD2                  0x73
#define RO_74_INT_STATE_RD3                  0x74
#define RW_80_PROX_HIGH_DIFF_CFG_CH0_0       0x80
#define RW_81_PROX_HIGH_DIFF_CFG_CH0_1       0x81
#define RW_82_PROX_HIGH_DIFF_CFG_CH1_0       0x82
#define RW_83_PROX_HIGH_DIFF_CFG_CH1_1       0x83
#define RW_84_PROX_HIGH_DIFF_CFG_CH2_0       0x84
#define RW_85_PROX_HIGH_DIFF_CFG_CH2_1       0x85
#define RW_86_PROX_HIGH_DIFF_CFG_CH3_0       0x86
#define RW_87_PROX_HIGH_DIFF_CFG_CH3_1       0x87
#define RW_88_PROX_LOW_DIFF_CFG_CH0_0        0x88
#define RW_89_PROX_LOW_DIFF_CFG_CH0_1        0x89
#define RW_8A_PROX_LOW_DIFF_CFG_CH1_0        0x8a
#define RW_8B_PROX_LOW_DIFF_CFG_CH1_1        0x8b
#define RW_8C_PROX_LOW_DIFF_CFG_CH2_0        0x8c
#define RW_8D_PROX_LOW_DIFF_CFG_CH2_1        0x8d
#define RW_8E_PROX_LOW_DIFF_CFG_CH3_0        0x8e
#define RW_8F_PROX_LOW_DIFF_CFG_CH3_1        0x8f
#define RW_9E_PROX_HIGH_DIFF_CFG_CH4_0       0x9e
#define RW_9F_PROX_HIGH_DIFF_CFG_CH4_1       0x9f
#define RW_A2_PROX_LOW_DIFF_CFG_CH4_0        0xa2
#define RW_A3_PROX_LOW_DIFF_CFG_CH4_1        0xa3
#define RW_91_DSP_CONFIG_CTRL4               0x91
#define RW_93_DSP_CONFIG_CTRL6               0x93
#define RW_94_DSP_CONFIG_CTRL7               0x94
#define RW_95_DSP_CONFIG_CTRL8               0x95
#define RW_96_DSP_CONFIG_CTRL9               0x96
#define RW_97_DSP_CONFIG_CTRL10              0x97
#define RW_98_DSP_CONFIG_CTRL11              0x98
#define RW_A0_LP_OUT_DELTA_THRES_CH1_CFG0    0xa0
#define RW_A1_LP_OUT_DELTA_THRES_CH1_CFG1    0xa1
#define RW_A4_LP_OUT_DELTA_THRES_CH3_CFG0    0xa4
#define RW_A5_LP_OUT_DELTA_THRES_CH3_CFG1    0xa5
#define RW_A6_LP_OUT_DELTA_THRES_CH4_CFG0    0xa6
#define RW_A7_LP_OUT_DELTA_THRES_CH4_CFG1    0xa7
#define RW_A8_PROX_THRES_SHIFT_CFG0          0xa8
#define RW_A9_PROX_THRES_SHIFT_CFG1          0xa9
#define RW_AA_PROX_THRES_SHIFT_CFG2          0xaa
#define RW_AB_PROX_THRES_SHIFT_CFG3          0xab
#define RW_AC_PROX_THRES_SHIFT_CFG4          0xac
#define RW_AD_BL_IN_NO_UP_NUM_SEL0           0xad
#define RW_AE_BL_IN_NO_UP_NUM_SEL1           0xae
#define RW_AF_BL_IN_NO_UP_NUM_SEL2           0xaf
#define RW_B2_BL_ALPHA_UP_DN_SEL             0xb2
#define RW_BF_CH0_SAMP_CFG                   0xbf
#define RW_C0_CH10_SCAN_FACTOR               0xc0
#define RW_C1_CH32_SCAN_FACTOR               0xc1
#define RW_C2_OFFSET_CALI_CTRL               0xc2
#define RW_90_OFFSET_CALI_CTRL1              0x90
#define RW_C3_DSP_CONFIG_CTRL0               0xc3
#define RW_92_DSP_CONFIG_CTRL5               0x92
#define RW_C4_CH10_DOZE_FACTOR               0xc4
#define RW_C5_CH32_DOZE_FACTOR               0xc5
#define RW_C6_CH10_PROX_FACTOR               0xc6
#define RW_C7_CH4_FACTOR_CTRL                0xc7
#define RW_C8_DSP_CONFIG_CTRL1               0xc8
#define RW_C9_DSP_CONFIG_CTRL2               0xc9
#define RW_CA_DSP_CONFIG_CTRL3               0xca
#define RO_CB_DEC_DATA0                      0xcb
#define RO_CC_DEC_DATA1                      0xcc
#define RO_CD_DEC_DATA2                      0xcd
#define RO_CE_DEC_DATA3                      0xce
#define RO_E0_CAP_INI_CH0_0                  0xe0
#define RO_E1_CAP_INI_CH0_1                  0xe1
#define RO_99_CAP_INI_CH0_2                  0x99
#define RO_E2_CAP_INI_CH1_0                  0xe2
#define RO_E3_CAP_INI_CH1_1                  0xe3
#define RO_9A_CAP_INI_CH1_2                  0x9a
#define RO_E4_CAP_INI_CH2_0                  0xe4
#define RO_E5_CAP_INI_CH2_1                  0xe5
#define RO_9B_CAP_INI_CH2_2                  0x9b
#define RO_E6_CAP_INI_CH3_0                  0xe6
#define RO_E7_CAP_INI_CH3_1                  0xe7
#define RO_9C_CAP_INI_CH3_2                  0x9C
#define RO_B3_CAP_INI_CH4_0                  0xb3
#define RO_B4_CAP_INI_CH4_1                  0xb4
#define RO_9D_CAP_INI_CH4_2                  0x9d
#define RO_E8_RAW_BL_CH0_0                   0xe8
#define RO_E9_RAW_BL_CH0_1                   0xe9
#define RO_EA_RAW_BL_CH0_2                   0xea
#define RO_EB_RAW_BL_CH1_0                   0xeb
#define RO_EC_RAW_BL_CH1_1                   0xec
#define RO_ED_RAW_BL_CH1_2                   0xed
#define RO_EE_RAW_BL_CH2_0                   0xee
#define RO_EF_RAW_BL_CH2_1                   0xef
#define RO_F0_RAW_BL_CH2_2                   0xf0
#define RO_F1_RAW_BL_CH3_0                   0xf1
#define RO_F2_RAW_BL_CH3_1                   0xf2
#define RO_F3_RAW_BL_CH3_2                   0xf3
#define RO_B5_RAW_BL_CH4_0                   0xb5
#define RO_B6_RAW_BL_CH4_1                   0xb6
#define RO_B7_RAW_BL_CH4_2                   0xb7
#define RO_F4_LP_DIFF_CH0_0                  0xf4
#define RO_F5_LP_DIFF_CH0_1                  0xf5
#define RO_F6_LP_DIFF_CH0_2                  0xf6
#define RO_F7_LP_DIFF_CH1_0                  0xf7
#define RO_F8_LP_DIFF_CH1_1                  0xf8
#define RO_F9_LP_DIFF_CH1_2                  0xf9
#define RO_FA_LP_DIFF_CH2_0                  0xfa
#define RO_FB_LP_DIFF_CH2_1                  0xfb
#define RO_FC_LP_DIFF_CH2_2                  0xfc
#define RO_FD_LP_DIFF_CH3_0                  0xfd
#define RO_FE_LP_DIFF_CH3_1                  0xfe
#define RO_FF_LP_DIFF_CH3_2                  0xff
#define RO_B8_LP_DIFF_CH4_0                  0xb8
#define RO_B9_LP_DIFF_CH4_1                  0xb9
#define RO_BA_LP_DIFF_CH4_2                  0xba
#define RW_50_REG_TO_ANA2                    0x50
#define RW_51_REG_TO_ANA3                    0x51
#define RW_52_REG_TO_ANA4                    0x52
#define RW_53_REG_TO_ANA5                    0x53
#define RW_82_REG_TO_ANA6                    0x82

struct hx9023s_near_far_threshold {
    int16_t thr_near;
    int16_t thr_far;
};

struct hx9023s_addr_val_pair {
    uint8_t addr;
    uint8_t val;
};

struct hx9023s_channel_info {
    char name[20];
    bool enabled;
    bool used;
    int state;
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
    int type;
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
#if !HX9023S_REPORT_EVKEY
    struct sensors_classdev classdev;
#endif
    struct input_dev *input_dev_abs;
    int keycode;
};

struct hx9023s_platform_data {
    struct device *pdev;
    struct delayed_work polling_work;
    spinlock_t lock;
    struct input_dev *input_dev_key;
    struct hx9023s_channel_info *chs_info;
    uint32_t channel_used_flag;
    int irq;      //irq number
    int irq_gpio; //gpio numver
    char irq_disabled;
    uint32_t prox_state_reg;//寄存器显示的prox状态
    uint32_t prox_state_cmp;//data和阈值比较后的prox状态
    uint32_t prox_state_reg_pre;//寄存器显示的prox状态（上次的）
    uint32_t prox_state_cmp_pre;//data和阈值比较后的prox状态（上次的）
    uint32_t prox_state_far_to_near_flag;//标记每个通道由远离变为接近
//+S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
#if POWER_ENABLE
    int power_state;
#endif
//-S96818AA1-2208, liuling3.wt,ADD, 2023/05/15, add sar power reduction control switch
};

static struct hx9023s_channel_info hx9023_channels[] = {
    {
        .name = "grip_sensor",
        .enabled = false,
        .used = false,
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
        .type = 65560,
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
    },
    {
        .name = "grip_sensor_wifi",
        .enabled = false,
        .used = false,
/* +S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
        .type = 65575,
/* -S96818AA1-1936, liuling3.wt,ADD, 2023/06/25,  add sar sensor type*/
    },
//+S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
    {
        .name = "ch0_refer",
        .enabled = false,
        .used = false,
    },
//-S96818AA1-1936, liuling3.wt,ADD, 2023/05/17, add sar reference channel switch
};

static struct hx9023s_addr_val_pair hx9023s_reg_init_list[] = {
    
    {RW_24_CH_NUM_CFG,                 0x00},
    {RW_00_GLOBAL_CTRL0,               0x00},
    {RW_23_GLOBAL_CTRL2,               0x00},

/*+S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, update parameter configuration*/
    {RW_02_PRF_CFG,                    0x14},
    {RW_0D_RANGE_7_0,                  0x23},
/*-S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, update parameter configuration*/
    {RW_0E_RANGE_9_8,                  0x00},

/*+S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, update parameter configuration*/
    {RW_10_AVG0_NOSR0_CFG,             0x70},
    {RW_11_NOSR12_CFG,                 0x04},
    {RW_12_NOSR34_CFG,                 0x00},
    {RW_13_AVG12_CFG,                  0x03},
    {RW_14_AVG34_CFG,                  0x00},

    {RW_1F_SAMPLE_NUM_7_0,             0x40},
    {RW_21_INTEGRATION_NUM_7_0,        0x40},

    {RW_2A_LP_ALP_1_0_CFG,             0x33},
    {RW_2B_LP_ALP_3_2_CFG,             0x33},
/*-S96818AA1-1936, liuling3.wt,ADD, 2023/05/22, update parameter configuration*/
    {RW_2C_UP_ALP_1_0_CFG,             0x88},
    {RW_2D_UP_ALP_3_2_CFG,             0x88},
    {RW_2E_DN_UP_ALP_0_4_CFG,          0x18},
    {RW_2F_DN_ALP_2_1_CFG,             0x11},

    {RW_38_RAW_BL_RD_CFG,              0xF0},
    {RW_39_INTERRUPT_CFG,              0xFF},
    {RW_3B_CALI_DIFF_CFG,              0x07},
    {RW_6C_PROX_INT_HIGH_CFG,          0x01},
    {RW_6D_PROX_INT_LOW_CFG,           0x01},

    {RW_80_PROX_HIGH_DIFF_CFG_CH0_0,   0x40},
    {RW_81_PROX_HIGH_DIFF_CFG_CH0_1,   0x00},
    {RW_82_PROX_HIGH_DIFF_CFG_CH1_0,   0x40},
    {RW_83_PROX_HIGH_DIFF_CFG_CH1_1,   0x00},
    {RW_84_PROX_HIGH_DIFF_CFG_CH2_0,   0x40},
    {RW_85_PROX_HIGH_DIFF_CFG_CH2_1,   0x00},
    {RW_88_PROX_LOW_DIFF_CFG_CH0_0,    0x20},
    {RW_89_PROX_LOW_DIFF_CFG_CH0_1,    0x00},
    {RW_8A_PROX_LOW_DIFF_CFG_CH1_0,    0x20},
    {RW_8B_PROX_LOW_DIFF_CFG_CH1_1,    0x00},
    {RW_8C_PROX_LOW_DIFF_CFG_CH2_0,    0x20},
    {RW_8D_PROX_LOW_DIFF_CFG_CH2_1,    0x00},

    {RW_A8_PROX_THRES_SHIFT_CFG0,      0x00},
    {RW_A9_PROX_THRES_SHIFT_CFG1,      0x00},
    {RW_AA_PROX_THRES_SHIFT_CFG2,      0x00},

    {RW_C0_CH10_SCAN_FACTOR,           0x00},
    {RW_C1_CH32_SCAN_FACTOR,           0x00},
    {RW_C4_CH10_DOZE_FACTOR,           0x00},
    {RW_C5_CH32_DOZE_FACTOR,           0x00},
    {RW_C8_DSP_CONFIG_CTRL1,           0x00},
    {RW_CA_DSP_CONFIG_CTRL3,           0x00},
};

#endif
