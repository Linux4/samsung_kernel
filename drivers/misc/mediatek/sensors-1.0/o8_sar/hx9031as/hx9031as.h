#ifndef __HX9031AS_H
#define __HX9031AS_H
#include "tyhx_log.h"

#define HX9023S_ON_BOARD 0
#define HX9031AS_ON_BOARD 1
#define HX9031AS_CHIP_SELECT HX9031AS_ON_BOARD
#define HX9031AS_DRIVER_NAME "hx9031as"    //i2c addr: HX9031AS=0x28/0x2A

#define TYHX_DELAY_US(x) msleep((x)/1000)
#define TYHX_DELAY_MS(x) msleep(x)
/*A06 code for SR-AL7160A-01-174  by suyurui at 2024/04/02 start*/
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 start*/
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 start*/
#ifdef HQ_FACTORY_BUILD
#define HX9031AS_FACTORY_NODE
#else
#define HX9031AS_USER_NODE
#endif
/*A06 code for SR-AL7160A-01-188 by jiashixian at 20240420 end*/
/*A06 code for SR-AL7160A-01-183 by huofudong at 20240409 end*/
/*A06 code for SR-AL7160A-01-174  by suyurui at 2024/04/02 end*/
/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 start*/
#define HX9031AS_TEST_CHS_EN 1      //Turn on and open all channels
#define HX9031AS_REPORT_EVKEY 1     //Select the type of reported event 1==EV_KEY, 0==EV_ABS
/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 end*/
#define HX9031AS_TEST_ON_MTK_DEMO_XY6761 0//Set 1 only during debugging on the 6761 development board

#define HX9031AS_CH_NUM 5
/*A06 code for AL7160A-417 jiashixian at 20240506 start*/
#define HX9031AS_CH_USED 0x1F
/*A06 code for AL7160A-417 jiashixian at 20240506 end*/
#define HX9031AS_MAX_XFER_SIZE 32

#define HX9031AS_ODR_MS 200

#define HX9031AS_DATA_LOCK 1
#define HX9031AS_DATA_UNLOCK 0

#define CH_DATA_2BYTES 2
#define CH_DATA_3BYTES 3
#define CH_DATA_BYTES_MAX CH_DATA_3BYTES

#define HX9031AS_ID_CHECK_COUNT 2
#define HX9031AS_CHIP_ID 0x1D

#define IDLE 0          //far
#define PROXACTIVE 1    //near
#define BODYACTIVE 2    //closer

//Register List
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
#define RW_0A_CH3_CFG_9_8                    0x0A
#define RW_0B_CH4_CFG_7_0                    0x0B
#define RW_0C_CH4_CFG_9_8                    0x0C
#define RW_0D_RANGE_7_0                      0x0D
#define RW_0E_RANGE_9_8                      0x0E
#define RW_0F_RANGE_18_16                    0x0F
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
#define RW_1A_OFFSET_DAC2_9_8                0x1A
#define RW_1B_OFFSET_DAC3_7_0                0x1B
#define RW_1C_OFFSET_DAC3_9_8                0x1C
#define RW_1D_OFFSET_DAC4_7_0                0x1D
#define RW_1E_OFFSET_DAC4_9_8                0x1E
#define RW_1F_SAMPLE_NUM_7_0                 0x1F
#define RW_20_SAMPLE_NUM_9_8                 0x20
#define RW_21_INTEGRATION_NUM_7_0            0x21
#define RW_22_INTEGRATION_NUM_9_8            0x22
#define RW_23_GLOBAL_CTRL2                   0x23
#define RW_24_CH_NUM_CFG                     0x24
#define RW_25_DAC_SWAP_CFG                   0x25
#define RW_28_MOD_RST_CFG                    0x28
#define RW_29_LP_ALP_4_CFG                   0x29
#define RW_2A_LP_ALP_1_0_CFG                 0x2A
#define RW_2B_LP_ALP_3_2_CFG                 0x2B
#define RW_2C_UP_ALP_1_0_CFG                 0x2C
#define RW_2D_UP_ALP_3_2_CFG                 0x2D
#define RW_2E_DN_UP_ALP_0_4_CFG              0x2E
#define RW_2F_DN_ALP_2_1_CFG                 0x2F
#define RW_30_DN_ALP_4_3_CFG                 0x30
#define RW_31_INT_CAP_CFG                    0x31
#define RW_33_NDL_DLY_4_CFG                  0x33
#define RW_35_FORCE_NO_UP_CFG                0x35
#define RW_38_RAW_BL_RD_CFG                  0x38
#define RW_39_INTERRUPT_CFG                  0x39
#define RW_3A_INTERRUPT_CFG1                 0x3A
#define RW_3B_CALI_DIFF_CFG                  0x3B
#define RW_3C_DITHER_CFG                     0x3C
#define RW_40_ANALOG_MEM0_WRDATA_7_0         0x40
#define RW_41_ANALOG_MEM0_WRDATA_15_8        0x41
#define RW_42_ANALOG_MEM0_WRDATA_23_16       0x42
#define RW_43_ANALOG_MEM0_WRDATA_31_24       0x43
#define RW_48_ANALOG_PWE_PULSE_CYCLE7_0      0x48
#define RW_49_ANALOG_PWE_PULSE_CYCLE12_8     0x49
#define RW_4A_ANALOG_MEM_GLOBAL_CTRL         0x4A
#define RO_4B_DEBUG_MEM_ADC_FSM              0x4B
#define RW_4C_ANALOG_MEM_GLOBAL_CTRL1        0x4C
#define RO_5F_VERION_ID                      0x5F
#define RO_60_DEVICE_ID                      0x60
#define RO_61_TC_FSM                         0x61
#define RO_66_FLAG_RD                        0x66
#define RO_6A_CONV_TIMEOUT_CNT               0x6A
#define RO_6B_PROX_STATUS                    0x6B
#define RW_6C_PROX_INT_HIGH_CFG              0x6C
#define RW_6D_PROX_INT_LOW_CFG               0x6D
#define RW_6E_CAP_INI_CFG                    0x6E
#define RW_6F_INT_WIDTH_CFG0                 0x6F
#define RW_70_INT_WIDTH_CFG1                 0x70
#define RO_71_INT_STATE_RD0                  0x71
#define RO_72_INT_STATE_RD1                  0x72
#define RO_73_INT_STATE_RD2                  0x73
#define RO_74_INT_STATE_RD3                  0x74
#define RW_80_PROX_HIGH_DIFF_CFG_CH0_0       0x80
#define RW_81_PROX_HIGH_DIFF_CFG_CH0_1       0x81
/*A06 code for AL7160A-417 | AL7160A-1534 jiashixian at 20240524 start*/
#define RW_82_PROX_HIGH_DIFF_CFG_CH1_0       0x82
/*A06 code for AL7160A-417 | AL7160A-1534 jiashixian at 20240524 end*/
#define RW_83_PROX_HIGH_DIFF_CFG_CH1_1       0x83
#define RW_84_PROX_HIGH_DIFF_CFG_CH2_0       0x84
#define RW_85_PROX_HIGH_DIFF_CFG_CH2_1       0x85
#define RW_86_PROX_HIGH_DIFF_CFG_CH3_0       0x86
#define RW_87_PROX_HIGH_DIFF_CFG_CH3_1       0x87
#define RW_88_PROX_LOW_DIFF_CFG_CH0_0        0x88
#define RW_89_PROX_LOW_DIFF_CFG_CH0_1        0x89
/*A06 code for AL7160A-417 | AL7160A-1534 jiashixian at 20240524 start*/
#define RW_8A_PROX_LOW_DIFF_CFG_CH1_0        0x8A
/*A06 code for AL7160A-417 | AL7160A-1534 jiashixian at 20240524 end*/
#define RW_8B_PROX_LOW_DIFF_CFG_CH1_1        0x8B
#define RW_8C_PROX_LOW_DIFF_CFG_CH2_0        0x8C
#define RW_8D_PROX_LOW_DIFF_CFG_CH2_1        0x8D
#define RW_8E_PROX_LOW_DIFF_CFG_CH3_0        0x8E
#define RW_8F_PROX_LOW_DIFF_CFG_CH3_1        0x8F
#define RW_9E_PROX_HIGH_DIFF_CFG_CH4_0       0x9E
#define RW_9F_PROX_HIGH_DIFF_CFG_CH4_1       0x9F
#define RW_A2_PROX_LOW_DIFF_CFG_CH4_0        0xA2
#define RW_A3_PROX_LOW_DIFF_CFG_CH4_1        0xA3
#define RW_91_DSP_CONFIG_CTRL4               0x91
#define RW_93_DSP_CONFIG_CTRL6               0x93
#define RW_94_DSP_CONFIG_CTRL7               0x94
#define RW_95_DSP_CONFIG_CTRL8               0x95
#define RW_96_DSP_CONFIG_CTRL9               0x96
#define RW_97_DSP_CONFIG_CTRL10              0x97
#define RW_98_DSP_CONFIG_CTRL11              0x98
#define RW_A0_LP_OUT_DELTA_THRES_CH1_CFG0    0xA0
#define RW_A1_LP_OUT_DELTA_THRES_CH1_CFG1    0xA1
#define RW_A4_LP_OUT_DELTA_THRES_CH3_CFG0    0xA4
#define RW_A5_LP_OUT_DELTA_THRES_CH3_CFG1    0xA5
#define RW_A6_LP_OUT_DELTA_THRES_CH4_CFG0    0xA6
#define RW_A7_LP_OUT_DELTA_THRES_CH4_CFG1    0xA7
#define RW_A8_PROX_THRES_SHIFT_CFG0          0xA8
#define RW_A9_PROX_THRES_SHIFT_CFG1          0xA9
#define RW_AA_PROX_THRES_SHIFT_CFG2          0xAA
#define RW_AB_PROX_THRES_SHIFT_CFG3          0xAB
#define RW_AC_PROX_THRES_SHIFT_CFG4          0xAC
#define RW_AD_BL_IN_NO_UP_NUM_SEL0           0xAD
#define RW_AE_BL_IN_NO_UP_NUM_SEL1           0xAE
#define RW_AF_BL_IN_NO_UP_NUM_SEL2           0xAF
#define RW_B2_BL_ALPHA_UP_DN_SEL             0xB2
#define RW_BF_CH0_SAMP_CFG                   0xBF
#define RW_C0_CH10_SCAN_FACTOR               0xC0
#define RW_C1_CH32_SCAN_FACTOR               0xC1
#define RW_C2_OFFSET_CALI_CTRL               0xC2
#define RW_90_OFFSET_CALI_CTRL1              0x90
#define RW_C3_DSP_CONFIG_CTRL0               0xC3
#define RW_92_DSP_CONFIG_CTRL5               0x92
#define RW_C4_CH10_DOZE_FACTOR               0xC4
#define RW_C5_CH32_DOZE_FACTOR               0xC5
#define RW_C6_CH10_PROX_FACTOR               0xC6
#define RW_C7_CH4_FACTOR_CTRL                0xC7
#define RW_C8_DSP_CONFIG_CTRL1               0xC8
#define RW_C9_DSP_CONFIG_CTRL2               0xC9
#define RW_CA_DSP_CONFIG_CTRL3               0xCA
#define RO_CB_DEC_DATA0                      0xCB
#define RO_CC_DEC_DATA1                      0xCC
#define RO_CD_DEC_DATA2                      0xCD
#define RO_CE_DEC_DATA3                      0xCE
#define RO_E0_CAP_INI_CH0_0                  0xE0
#define RO_E1_CAP_INI_CH0_1                  0xE1
#define RO_99_CAP_INI_CH0_2                  0x99
#define RO_E2_CAP_INI_CH1_0                  0xE2
#define RO_E3_CAP_INI_CH1_1                  0xE3
#define RO_9A_CAP_INI_CH1_2                  0x9A
#define RO_E4_CAP_INI_CH2_0                  0xE4
#define RO_E5_CAP_INI_CH2_1                  0xE5
#define RO_9B_CAP_INI_CH2_2                  0x9B
#define RO_E6_CAP_INI_CH3_0                  0xE6
#define RO_E7_CAP_INI_CH3_1                  0xE7
#define RO_9C_CAP_INI_CH3_2                  0x9C
#define RO_B3_CAP_INI_CH4_0                  0xB3
#define RO_B4_CAP_INI_CH4_1                  0xB4
#define RO_9D_CAP_INI_CH4_2                  0x9D
#define RO_E8_RAW_BL_CH0_0                   0xE8
#define RO_E9_RAW_BL_CH0_1                   0xE9
#define RO_EA_RAW_BL_CH0_2                   0xEA
#define RO_EB_RAW_BL_CH1_0                   0xEB
#define RO_EC_RAW_BL_CH1_1                   0xEC
#define RO_ED_RAW_BL_CH1_2                   0xED
#define RO_EE_RAW_BL_CH2_0                   0xEE
#define RO_EF_RAW_BL_CH2_1                   0xEF
#define RO_F0_RAW_BL_CH2_2                   0xF0
#define RO_F1_RAW_BL_CH3_0                   0xF1
#define RO_F2_RAW_BL_CH3_1                   0xF2
#define RO_F3_RAW_BL_CH3_2                   0xF3
#define RO_B5_RAW_BL_CH4_0                   0xB5
#define RO_B6_RAW_BL_CH4_1                   0xB6
#define RO_B7_RAW_BL_CH4_2                   0xB7
#define RO_F4_LP_DIFF_CH0_0                  0xF4
#define RO_F5_LP_DIFF_CH0_1                  0xF5
#define RO_F6_LP_DIFF_CH0_2                  0xF6
#define RO_F7_LP_DIFF_CH1_0                  0xF7
#define RO_F8_LP_DIFF_CH1_1                  0xF8
#define RO_F9_LP_DIFF_CH1_2                  0xF9
#define RO_FA_LP_DIFF_CH2_0                  0xFA
#define RO_FB_LP_DIFF_CH2_1                  0xFB
#define RO_FC_LP_DIFF_CH2_2                  0xFC
#define RO_FD_LP_DIFF_CH3_0                  0xFD
#define RO_FE_LP_DIFF_CH3_1                  0xFE
#define RO_FF_LP_DIFF_CH3_2                  0xFF
#define RO_B8_LP_DIFF_CH4_0                  0xB8
#define RO_B9_LP_DIFF_CH4_1                  0xB9
#define RO_BA_LP_DIFF_CH4_2                  0xBA
#define RW_50_REG_TO_ANA2                    0x50
#define RW_51_REG_TO_ANA3                    0x51
#define RW_52_REG_TO_ANA4                    0x52
#define RW_53_REG_TO_ANA5                    0x53
#define RW_82_REG_TO_ANA6                    0x82

struct hx9031as_near_far_threshold {
    int32_t thr_near;
    int32_t thr_far;
};

struct hx9031as_addr_val_pair {
    uint8_t addr;
    uint8_t val;
};
/*A06 code for SR-AL7160A-01-185 |  SR-AL7160A-01-183 by huofudong at 20240409 start*/
struct hx9031as_channel_info {
    char name[20];
    bool enabled;
    bool used;
    int state;
    struct input_dev *input_dev_abs;
    struct input_dev *input_key;
    int keycode;
    int keycode_far;
    int keycode_close;
    char phase_status;
};

struct hx9031as_platform_data {
    struct device *pdev;
    struct delayed_work polling_work;
    struct input_dev *input_dev_key;
    struct hx9031as_channel_info *chs_info;
    uint32_t channel_used_flag;
    int irq;
    int irq_gpio;
    char irq_disabled;
    uint32_t prox_state_reg;  //The prox status displayed in the register
    bool sel_bl[HX9031AS_CH_NUM];
    bool sel_raw[HX9031AS_CH_NUM];
    bool sel_diff[HX9031AS_CH_NUM];
    bool sel_lp[HX9031AS_CH_NUM];
    char channel_status;
    uint32_t fw_num;
    uint8_t chs_en_flag;
    uint8_t cali_en_flag;
    uint8_t device_id;
    uint8_t version_id;
    /*A06 code for SR-AL7160A-01-188  by jiashixian at 20240420 start*/
    #ifdef HX9031AS_USER_NODE
    struct device *factory_device;
    struct device *factory_device_sub;
    struct device *factory_device_wifi;
    #endif
    /*A06 code for SR-AL7160A-01-188  by jiashixian at 20240420 end*/
};
/*A06 code for SR-AL7160A-01-185 |  SR-AL7160A-01-183 by huofudong at 20240409 end*/
/*A06 code for AL7160A-417 jiashixian at 20240506 start*/
/*A06 code for AL7160A-3673 | AL7160A-3805 jiashixian at 20240705 start*/
static struct hx9031as_addr_val_pair hx9031as_reg_init_list[] = {
    {RW_24_CH_NUM_CFG,                 0x00},
    {RW_00_GLOBAL_CTRL0,               0x00},
    {RW_23_GLOBAL_CTRL2,               0x00},

    {RW_02_PRF_CFG,                    0x15},
    {RW_0D_RANGE_7_0,                  0x11},
    {RW_0E_RANGE_9_8,                  0x02},
    {RW_0F_RANGE_18_16,                0x00},

    {RW_10_AVG0_NOSR0_CFG,             0x1C},
    {RW_11_NOSR12_CFG,                 0x33},
    {RW_12_NOSR34_CFG,                 0x33},
    {RW_13_AVG12_CFG,                  0x44},
    {RW_14_AVG34_CFG,                  0x44},

    {RW_1F_SAMPLE_NUM_7_0,             0x20},
    {RW_21_INTEGRATION_NUM_7_0,        0x20},

    {RW_2A_LP_ALP_1_0_CFG,             0x33},
    {RW_2B_LP_ALP_3_2_CFG,             0x33},
    {RW_29_LP_ALP_4_CFG,               0x03},
    {RW_2C_UP_ALP_1_0_CFG,             0x88},
    {RW_2D_UP_ALP_3_2_CFG,             0x88},
    {RW_2E_DN_UP_ALP_0_4_CFG,          0x18},
    {RW_2F_DN_ALP_2_1_CFG,             0x11},
    {RW_30_DN_ALP_4_3_CFG,             0x11},

    {RW_38_RAW_BL_RD_CFG,              0xF0},
    {RW_39_INTERRUPT_CFG,              0xFF},
    {RW_3A_INTERRUPT_CFG1,             0x3B},
    {RW_3B_CALI_DIFF_CFG,              0x07},
    {RW_3C_DITHER_CFG,                 0x21},
    {RW_6C_PROX_INT_HIGH_CFG,          0x01},
    {RW_6D_PROX_INT_LOW_CFG,           0x01},

    {RW_80_PROX_HIGH_DIFF_CFG_CH0_0,   0x40},
    {RW_81_PROX_HIGH_DIFF_CFG_CH0_1,   0x00},
    {RW_82_PROX_HIGH_DIFF_CFG_CH1_0,   0x6C},
    {RW_83_PROX_HIGH_DIFF_CFG_CH1_1,   0x00},
    {RW_84_PROX_HIGH_DIFF_CFG_CH2_0,   0x40},
    {RW_85_PROX_HIGH_DIFF_CFG_CH2_1,   0x00},
    {RW_86_PROX_HIGH_DIFF_CFG_CH3_0,   0x07},
    {RW_87_PROX_HIGH_DIFF_CFG_CH3_1,   0x00},
    {RW_9E_PROX_HIGH_DIFF_CFG_CH4_0,   0x0F},
    {RW_9F_PROX_HIGH_DIFF_CFG_CH4_1,   0x00},
    {RW_88_PROX_LOW_DIFF_CFG_CH0_0,    0x20},
    {RW_89_PROX_LOW_DIFF_CFG_CH0_1,    0x00},
    {RW_8A_PROX_LOW_DIFF_CFG_CH1_0,    0x6B},
    {RW_8B_PROX_LOW_DIFF_CFG_CH1_1,    0x00},
    {RW_8C_PROX_LOW_DIFF_CFG_CH2_0,    0x20},
    {RW_8D_PROX_LOW_DIFF_CFG_CH2_1,    0x00},
    {RW_8E_PROX_LOW_DIFF_CFG_CH3_0,    0x06},
    {RW_8F_PROX_LOW_DIFF_CFG_CH3_1,    0x00},
    {RW_A2_PROX_LOW_DIFF_CFG_CH4_0,    0x0E},
    {RW_A3_PROX_LOW_DIFF_CFG_CH4_1,    0x00},

    {RW_A8_PROX_THRES_SHIFT_CFG0,      0x00},
    {RW_A9_PROX_THRES_SHIFT_CFG1,      0x00},
    {RW_AA_PROX_THRES_SHIFT_CFG2,      0x00},
    {RW_AB_PROX_THRES_SHIFT_CFG3,      0x00},
    {RW_AC_PROX_THRES_SHIFT_CFG4,      0x00},

    {RW_C0_CH10_SCAN_FACTOR,           0x00},
    {RW_C1_CH32_SCAN_FACTOR,           0x00},
    {RW_C4_CH10_DOZE_FACTOR,           0x00},
    {RW_C5_CH32_DOZE_FACTOR,           0x00},
    {RW_C7_CH4_FACTOR_CTRL,            0x00},
    {RW_C8_DSP_CONFIG_CTRL1,           0x46},
    {RW_CA_DSP_CONFIG_CTRL3,           0x00},

    {RW_98_DSP_CONFIG_CTRL11,          0x15},
    {RW_A0_LP_OUT_DELTA_THRES_CH1_CFG0,0x03},
    {RW_A4_LP_OUT_DELTA_THRES_CH3_CFG0,0x03},
    {RW_AD_BL_IN_NO_UP_NUM_SEL0,       0x50},
    {RW_AE_BL_IN_NO_UP_NUM_SEL1,       0x50},
    {RW_95_DSP_CONFIG_CTRL8,           0x7F},
};

#endif
/*A06 code for AL7160A-3673 | AL7160A-3805 jiashixian at 20240705 end*/
/*A06 code for AL7160A-417 jiashixian at 20240506 end*/
/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 start*/
#define KEY_SAR1_CLOSE          0x279
#define KEY_SAR1_FAR            0x27a

#define KEY_SAR2_CLOSE          0x27b
#define KEY_SAR2_FAR            0x27c

#define KEY_SAR3_CLOSE          0x27d
#define KEY_SAR3_FAR            0x27e



/*A06 code for SR-AL7160A-01-185 by huofudong at 20240326 end*/
