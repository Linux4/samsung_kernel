#ifndef __HX9035_H
#define __HX9035_H
#include "tyhx_log.h"

//{======================================== hx9035 registers Begin

#define HX9035_REG_PAGE 0x00 //address for page select

#define PAGE0_01_DEVICE_ID                      0x0001
#define PAGE0_02_VERION_ID                      0x0002
#define PAGE0_03_CH_NUM_CFG                     0x0003
#define PAGE0_04_INT_STATE_RD0                  0x0004
#define PAGE0_05_INT_STATE_RD1                  0x0005
#define PAGE0_06_INT_STATE_RD2                  0x0006
#define PAGE0_07_INT_STATE_RD3                  0x0007
#define PAGE0_08_INT_STATE_RD4                  0x0008
#define PAGE0_09_INT_STATE_RD5                  0x0009
#define PAGE0_0A_INT_STATE_RD6                  0x000A
#define PAGE0_0B_INT_STATE_RD7                  0x000B
#define PAGE0_0C_INT_STATE_RD8                  0x000C
#define PAGE0_0D_RAW_BL_CH0_0                   0x000D
#define PAGE0_0E_RAW_BL_CH0_1                   0x000E
#define PAGE0_0F_RAW_BL_CH0_2                   0x000F
#define PAGE0_10_RAW_BL_CH1_0                   0x0010
#define PAGE0_11_RAW_BL_CH1_1                   0x0011
#define PAGE0_12_RAW_BL_CH1_2                   0x0012
#define PAGE0_13_RAW_BL_CH2_0                   0x0013
#define PAGE0_14_RAW_BL_CH2_1                   0x0014
#define PAGE0_15_RAW_BL_CH2_2                   0x0015
#define PAGE0_16_RAW_BL_CH3_0                   0x0016
#define PAGE0_17_RAW_BL_CH3_1                   0x0017
#define PAGE0_18_RAW_BL_CH3_2                   0x0018
#define PAGE0_19_RAW_BL_CH4_0                   0x0019
#define PAGE0_1A_RAW_BL_CH4_1                   0x001A
#define PAGE0_1B_RAW_BL_CH4_2                   0x001B
#define PAGE0_1C_RAW_BL_CH5_0                   0x001C
#define PAGE0_1D_RAW_BL_CH5_1                   0x001D
#define PAGE0_1E_RAW_BL_CH5_2                   0x001E
#define PAGE0_1F_RAW_BL_CH6_0                   0x001F
#define PAGE0_20_RAW_BL_CH6_1                   0x0020
#define PAGE0_21_RAW_BL_CH6_2                   0x0021
#define PAGE0_22_RAW_BL_CH7_0                   0x0022
#define PAGE0_23_RAW_BL_CH7_1                   0x0023
#define PAGE0_24_RAW_BL_CH7_2                   0x0024
#define PAGE0_25_LP_DIFF_CH0_0                  0x0025
#define PAGE0_26_LP_DIFF_CH0_1                  0x0026
#define PAGE0_27_LP_DIFF_CH0_2                  0x0027
#define PAGE0_28_LP_DIFF_CH1_0                  0x0028
#define PAGE0_29_LP_DIFF_CH1_1                  0x0029
#define PAGE0_2A_LP_DIFF_CH1_2                  0x002A
#define PAGE0_2B_LP_DIFF_CH2_0                  0x002B
#define PAGE0_2C_LP_DIFF_CH2_1                  0x002C
#define PAGE0_2D_LP_DIFF_CH2_2                  0x002D
#define PAGE0_2E_LP_DIFF_CH3_0                  0x002E
#define PAGE0_2F_LP_DIFF_CH3_1                  0x002F
#define PAGE0_30_LP_DIFF_CH3_2                  0x0030
#define PAGE0_31_LP_DIFF_CH4_0                  0x0031
#define PAGE0_32_LP_DIFF_CH4_1                  0x0032
#define PAGE0_33_LP_DIFF_CH4_2                  0x0033
#define PAGE0_34_LP_DIFF_CH5_0                  0x0034
#define PAGE0_35_LP_DIFF_CH5_1                  0x0035
#define PAGE0_36_LP_DIFF_CH5_2                  0x0036
#define PAGE0_37_LP_DIFF_CH6_0                  0x0037
#define PAGE0_38_LP_DIFF_CH6_1                  0x0038
#define PAGE0_39_LP_DIFF_CH6_2                  0x0039
#define PAGE0_3A_LP_DIFF_CH7_0                  0x003A
#define PAGE0_3B_LP_DIFF_CH7_1                  0x003B
#define PAGE0_3C_LP_DIFF_CH7_2                  0x003C
#define PAGE0_3D_GLOBAL_CFG0                    0x003D
#define PAGE0_3E_TS_CFG0                        0x003E
#define PAGE0_3F_TS_CFG1                        0x003F
#define PAGE0_40_PRF_CFG                        0x0040
#define PAGE0_41_CH10_SCAN_FACTOR               0x0041
#define PAGE0_42_CH32_SCAN_FACTOR               0x0042
#define PAGE0_43_CH54_SCAN_FACTOR               0x0043
#define PAGE0_44_CH76_SCAN_FACTOR               0x0044
#define PAGE0_45_CH10_DOZE_FACTOR               0x0045
#define PAGE0_46_CH32_DOZE_FACTOR               0x0046
#define PAGE0_47_CH54_DOZE_FACTOR               0x0047
#define PAGE0_48_CH76_DOZE_FACTOR               0x0048
#define PAGE0_49_CH10_PROX_FACTOR               0x0049
#define PAGE0_4D_TC_GLOBAL_CFG0                 0x004D
#define PAGE0_4E_TC_NOSR_CFG0                   0x004E
#define PAGE0_4F_TC_NOSR_CFG1                   0x004F
#define PAGE0_50_TC_NOSR_CFG2                   0x0050
#define PAGE0_51_TC_NOSR_CFG3                   0x0051
#define PAGE0_52_TC_AVG_CFG0                    0x0052
#define PAGE0_53_TC_AVG_CFG1                    0x0053
#define PAGE0_54_TC_AVG_CFG2                    0x0054
#define PAGE0_55_TC_AVG_CFG3                    0x0055
#define PAGE0_56_OFFSET_CALI_CTRL0              0x0056
#define PAGE0_57_OFFSET_CALI_CTRL1              0x0057
#define PAGE0_58_OFFSET_DAC0_0                  0x0058
#define PAGE0_59_OFFSET_DAC0_1                  0x0059
#define PAGE0_5A_OFFSET_DAC1_0                  0x005A
#define PAGE0_5B_OFFSET_DAC1_1                  0x005B
#define PAGE0_5C_OFFSET_DAC2_0                  0x005C
#define PAGE0_5D_OFFSET_DAC2_1                  0x005D
#define PAGE0_5E_OFFSET_DAC3_0                  0x005E
#define PAGE0_5F_OFFSET_DAC3_1                  0x005F
#define PAGE0_60_OFFSET_DAC4_0                  0x0060
#define PAGE0_61_OFFSET_DAC4_1                  0x0061
#define PAGE0_62_OFFSET_DAC5_0                  0x0062
#define PAGE0_63_OFFSET_DAC5_1                  0x0063
#define PAGE0_64_OFFSET_DAC6_0                  0x0064
#define PAGE0_65_OFFSET_DAC6_1                  0x0065
#define PAGE0_66_OFFSET_DAC7_0                  0x0066
#define PAGE0_67_OFFSET_DAC7_1                  0x0067
#define PAGE0_68_CH0_CFG_0                      0x0068
#define PAGE0_69_CH0_CFG_1                      0x0069
#define PAGE0_6A_CH1_CFG_0                      0x006A
#define PAGE0_6B_CH1_CFG_1                      0x006B
#define PAGE0_6C_CH2_CFG_0                      0x006C
#define PAGE0_6D_CH2_CFG_1                      0x006D
#define PAGE0_6E_CH3_CFG_0                      0x006E
#define PAGE0_6F_CH3_CFG_1                      0x006F
#define PAGE0_70_CH4_CFG_0                      0x0070
#define PAGE0_71_CH4_CFG_1                      0x0071
#define PAGE0_72_CH5_CFG_0                      0x0072
#define PAGE0_73_CH5_CFG_1                      0x0073
#define PAGE0_74_CH6_CFG_0                      0x0074
#define PAGE0_75_CH6_CFG_1                      0x0075
#define PAGE0_76_CH7_CFG_0                      0x0076
#define PAGE0_77_CH7_CFG_1                      0x0077
#define PAGE0_78_RANGE_CFG_0                    0x0078
#define PAGE0_79_RANGE_CFG_1                    0x0079
#define PAGE0_7A_RANGE_CFG_2                    0x007A
#define PAGE0_7B_RANGE_CFG_3                    0x007B
#define PAGE0_7C_DAC_SWAP_CFG                   0x007C
#define PAGE0_7D_SAMPLE_SWAP_CFG                0x007D
#define PAGE0_7E_HUGE_OFFSET_CFG                0x007E
#define PAGE0_7F_INT_CAP_CFG                    0x007F
#define PAGE0_80_SHIELD_CFG                     0x0080
#define PAGE0_83_AMP_CHOP_CFG                   0x0083
#define PAGE0_84_INTER_CIN_REF_EN               0x0084
#define PAGE0_85_INTER_DAC_REF_EN               0x0085
#define PAGE0_86_NEG_GAIN_EN                    0x0086
#define PAGE0_87_CH0_NEG_GAIN                   0x0087
#define PAGE0_88_CH1_NEG_GAIN                   0x0088
#define PAGE0_89_CH2_NEG_GAIN                   0x0089
#define PAGE0_8A_CH3_NEG_GAIN                   0x008A
#define PAGE0_8B_CH4_NEG_GAIN                   0x008B
#define PAGE0_8C_CH5_NEG_GAIN                   0x008C
#define PAGE0_8D_CH6_NEG_GAIN                   0x008D
#define PAGE0_8E_CH7_NEG_GAIN                   0x008E
#define PAGE0_8F_OFFSET_GAIN_CFG0               0x008F
#define PAGE0_90_OFFSET_GAIN_CFG1               0x0090
#define PAGE0_91_OFFSET_GAIN_CFG2               0x0091
#define PAGE0_92_OFFSET_GAIN_CFG3               0x0092
#define PAGE0_93_DSP_CONFIG_CTRL0               0x0093
#define PAGE0_94_DSP_CONFIG_CTRL1               0x0094
#define PAGE0_95_DSP_CONFIG_CTRL2               0x0095
#define PAGE0_96_DSP_CONFIG_CTRL3               0x0096
#define PAGE0_97_DSP_CONFIG_CTRL4               0x0097
#define PAGE0_98_DSP_CONFIG_CTRL5               0x0098
#define PAGE0_99_DSP_CONFIG_CTRL6               0x0099
#define PAGE0_9A_DSP_CONFIG_CTRL7               0x009A
#define PAGE0_9B_DSP_CONFIG_CTRL8               0x009B
#define PAGE0_9C_DSP_CONFIG_CTRL9               0x009C
#define PAGE0_9D_CAP_INI_CFG0                   0x009D
#define PAGE0_9E_CAP_INI_CFG1                   0x009E
#define PAGE0_9F_CAP_INI_RD                     0x009F
#define PAGE0_A0_INTERRUPT_CFG0                 0x00A0
#define PAGE0_A1_INTERRUPT_CFG1                 0x00A1
#define PAGE0_A2_INTERRUPT_CFG2                 0x00A2
#define PAGE0_A3_INTERRUPT_CFG3                 0x00A3
#define PAGE0_A4_INTERRUPT_CFG4                 0x00A4
#define PAGE0_A5_INTERRUPT_CFG5                 0x00A5
#define PAGE0_A6_INTERRUPT_CFG6                 0x00A6
#define PAGE0_A7_INTERRUPT_CFG7                 0x00A7
#define PAGE0_A8_PROX_STATUS                    0x00A8
#define PAGE0_A9_INT_WIDTH_CFG0                 0x00A9
#define PAGE0_AA_INT_WIDTH_CFG1                 0x00AA
#define PAGE0_AB_PROX_INT_CFG                   0x00AB
#define PAGE0_AC_BL_UP_CFG0                     0x00AC
#define PAGE0_AD_BL_UP_CFG1                     0x00AD
#define PAGE0_AE_BL_UP_CFG2                     0x00AE
#define PAGE0_AF_BL_UP_CFG3                     0x00AF
#define PAGE0_B0_BL_DN_CFG0                     0x00B0
#define PAGE0_B1_BL_DN_CFG1                     0x00B1
#define PAGE0_B2_BL_DN_CFG2                     0x00B2
#define PAGE0_B3_BL_DN_CFG3                     0x00B3
#define PAGE0_B4_NDLY_CFG                       0x00B4
#define PAGE0_B5_LP_ALP_CFG0                    0x00B5
#define PAGE0_B6_LP_ALP_CFG1                    0x00B6
#define PAGE0_B7_LP_ALP_CFG2                    0x00B7
#define PAGE0_B8_LP_ALP_CFG3                    0x00B8
#define PAGE0_B9_CH0_COE_CFG_0                  0x00B9
#define PAGE0_BB_CH1_COE_CFG_0                  0x00BB
#define PAGE0_BD_CH2_COE_CFG_0                  0x00BD
#define PAGE0_BF_CH3_COE_CFG_0                  0x00BF
#define PAGE0_C1_CH4_COE_CFG_0                  0x00C1
#define PAGE0_C3_CH5_COE_CFG_0                  0x00C3
#define PAGE0_C5_CH6_COE_CFG_0                  0x00C5
#define PAGE0_C7_CH7_COE_CFG_0                  0x00C7
#define PAGE0_C9_PROX_HIGH_THRES1_CFG0          0x00C9
#define PAGE0_CA_PROX_HIGH_THRES1_CFG1          0x00CA
#define PAGE0_CB_PROX_HIGH_THRES1_CFG2          0x00CB
#define PAGE0_CC_PROX_HIGH_THRES1_CFG3          0x00CC
#define PAGE0_CD_PROX_HIGH_THRES1_CFG4          0x00CD
#define PAGE0_CE_PROX_HIGH_THRES1_CFG5          0x00CE
#define PAGE0_CF_PROX_HIGH_THRES1_CFG6          0x00CF
#define PAGE0_D0_PROX_HIGH_THRES1_CFG7          0x00D0
#define PAGE0_D1_PROX_HIGH_THRES2_CFG0          0x00D1
#define PAGE0_D2_PROX_HIGH_THRES2_CFG1          0x00D2
#define PAGE0_D3_PROX_HIGH_THRES2_CFG2          0x00D3
#define PAGE0_D4_PROX_HIGH_THRES2_CFG3          0x00D4
#define PAGE0_D5_PROX_HIGH_THRES2_CFG4          0x00D5
#define PAGE0_D6_PROX_HIGH_THRES2_CFG5          0x00D6
#define PAGE0_D7_PROX_HIGH_THRES2_CFG6          0x00D7
#define PAGE0_D8_PROX_HIGH_THRES2_CFG7          0x00D8
#define PAGE0_D9_PROX_HIGH_THRES3_CFG0          0x00D9
#define PAGE0_DA_PROX_HIGH_THRES3_CFG1          0x00DA
#define PAGE0_DB_PROX_HIGH_THRES3_CFG2          0x00DB
#define PAGE0_DC_PROX_HIGH_THRES3_CFG3          0x00DC
#define PAGE0_DD_PROX_HIGH_THRES3_CFG4          0x00DD
#define PAGE0_DE_PROX_HIGH_THRES3_CFG5          0x00DE
#define PAGE0_DF_PROX_HIGH_THRES3_CFG6          0x00DF
#define PAGE0_E0_PROX_HIGH_THRES3_CFG7          0x00E0
#define PAGE0_E1_DITHER_CFG                     0x00E1
#define PAGE0_E2_SAMPLE_NUM_0                   0x00E2
#define PAGE0_E3_SAMPLE_NUM_1                   0x00E3
#define PAGE0_E4_INTEGRATION_NUM_0              0x00E4
#define PAGE0_E5_INTEGRATION_NUM_1              0x00E5
#define PAGE0_E6_CH0_SAMP_CFG                   0x00E6
#define PAGE0_E9_TEMP_ALPHA_CFG0                0x00E9
#define PAGE0_ED_LPOUT_DELTA_CFG                0x00ED
#define PAGE0_EF_INT_STATE_RD9                  0x00EF
#define PAGE0_F0_SEGMENT_TEMP_COMP_DIS_CFG      0x00F0
#define PAGE0_F1_RD_LOCK_CFG                    0x00F1
#define PAGE0_F2_PROX_STATUS1                   0x00F2
#define PAGE0_F3_PROX_STATUS2                   0x00F3
#define PAGE0_F4_PROX_STATUS3                   0x00F4
#define PAGE0_F9_PROX_THRES_EN_CFG              0x00F9
#define PAGE0_FA_PRESS_CALI_EN_CFG              0x00FA
#define PAGE0_FB_PRESS_SEL_CFG0                 0x00FB
#define PAGE0_FF_PRESS_SEL_CFG4                 0x00FF
    
#define PAGE1_07_ANALOG_MEM0_WRDATA_7_0         0x0107
#define PAGE1_08_ANALOG_MEM0_WRDATA_15_8        0x0108
#define PAGE1_09_ANALOG_MEM0_WRDATA_23_16       0x0109
#define PAGE1_0A_ANALOG_MEM0_WRDATA_31_24       0x010A
#define PAGE1_0F_ANALOG_PWE_PULSE_CYCLE7_0      0x010F
#define PAGE1_10_ANALOG_PWE_PULSE_CYCLE12_8     0x0110
#define PAGE1_11_ANALOG_MEM_GLOBAL_CTRL0        0x0111
#define PAGE1_12_ANALOG_MEM_GLOBAL_CTRL1        0x0112
#define PAGE1_13_DEBUG_MEM_ADC_FSM              0x0113
#define PAGE1_16_REG_TO_ANA2                    0x0116
#define PAGE1_17_REG_TO_ANA3                    0x0117
#define PAGE1_18_REG_TO_ANA4                    0x0118
#define PAGE1_19_REG_TO_ANA5                    0x0119
#define PAGE1_1A_REG_TO_ANA6                    0x011A
#define PAGE1_1B_REG_TO_ANA7                    0x011B
#define PAGE1_1C_REG_TO_ANA8                    0x011C
#define PAGE1_1D_REG_TO_ANA9                    0x011D
#define PAGE1_1E_REG_TO_ANAA                    0x011E
#define PAGE1_1F_REG_TO_ANAB                    0x011F
#define PAGE1_20_TC_FSM                         0x0120
#define PAGE1_21_FLAG_RD                        0x0121
#define PAGE1_22_DEC_DATA0                      0x0122
#define PAGE1_23_DEC_DATA1                      0x0123
#define PAGE1_24_DEC_DATA2                      0x0124
#define PAGE1_25_DEC_DATA3                      0x0125
#define PAGE1_27_RAW_DATA_CH0_0                 0x0127
#define PAGE1_28_RAW_DATA_CH0_1                 0x0128
#define PAGE1_29_RAW_DATA_CH0_2                 0x0129
#define PAGE1_2B_RAW_DATA_CH1_0                 0x012B
#define PAGE1_2C_RAW_DATA_CH1_1                 0x012C
#define PAGE1_2D_RAW_DATA_CH1_2                 0x012D
#define PAGE1_2F_RAW_DATA_CH2_0                 0x012F
#define PAGE1_30_RAW_DATA_CH2_1                 0x0130
#define PAGE1_31_RAW_DATA_CH2_2                 0x0131
#define PAGE1_33_RAW_DATA_CH3_0                 0x0133
#define PAGE1_34_RAW_DATA_CH3_1                 0x0134
#define PAGE1_35_RAW_DATA_CH3_2                 0x0135
#define PAGE1_37_RAW_DATA_CH4_0                 0x0137
#define PAGE1_38_RAW_DATA_CH4_1                 0x0138
#define PAGE1_39_RAW_DATA_CH4_2                 0x0139
#define PAGE1_3B_RAW_DATA_CH5_0                 0x013B
#define PAGE1_3C_RAW_DATA_CH5_1                 0x013C
#define PAGE1_3D_RAW_DATA_CH5_2                 0x013D
#define PAGE1_3F_RAW_DATA_CH6_0                 0x013F
#define PAGE1_40_RAW_DATA_CH6_1                 0x0140
#define PAGE1_41_RAW_DATA_CH6_2                 0x0141
#define PAGE1_43_RAW_DATA_CH7_0                 0x0143
#define PAGE1_44_RAW_DATA_CH7_1                 0x0144
#define PAGE1_45_RAW_DATA_CH7_2                 0x0145
#define PAGE1_47_LP_OUT_CH0_0                   0x0147
#define PAGE1_48_LP_OUT_CH0_1                   0x0148
#define PAGE1_49_LP_OUT_CH0_2                   0x0149
#define PAGE1_4A_LP_OUT_CH1_0                   0x014A
#define PAGE1_4B_LP_OUT_CH1_1                   0x014B
#define PAGE1_4C_LP_OUT_CH1_2                   0x014C
#define PAGE1_4D_LP_OUT_CH2_0                   0x014D
#define PAGE1_4E_LP_OUT_CH2_1                   0x014E
#define PAGE1_4F_LP_OUT_CH2_2                   0x014F
#define PAGE1_50_LP_OUT_CH3_0                   0x0150
#define PAGE1_51_LP_OUT_CH3_1                   0x0151
#define PAGE1_52_LP_OUT_CH3_2                   0x0152
#define PAGE1_53_LP_OUT_CH4_0                   0x0153
#define PAGE1_54_LP_OUT_CH4_1                   0x0154
#define PAGE1_55_LP_OUT_CH4_2                   0x0155
#define PAGE1_56_LP_OUT_CH5_0                   0x0156
#define PAGE1_57_LP_OUT_CH5_1                   0x0157
#define PAGE1_58_LP_OUT_CH5_2                   0x0158
#define PAGE1_59_LP_OUT_CH6_0                   0x0159
#define PAGE1_5A_LP_OUT_CH6_1                   0x015A
#define PAGE1_5B_LP_OUT_CH6_2                   0x015B
#define PAGE1_5C_LP_OUT_CH7_0                   0x015C
#define PAGE1_5D_LP_OUT_CH7_1                   0x015D
#define PAGE1_5E_LP_OUT_CH7_2                   0x015E
#define PAGE1_5F_BL_OUT_CH0_0                   0x015F
#define PAGE1_60_BL_OUT_CH0_1                   0x0160
#define PAGE1_61_BL_OUT_CH0_2                   0x0161
#define PAGE1_62_BL_OUT_CH1_0                   0x0162
#define PAGE1_63_BL_OUT_CH1_1                   0x0163
#define PAGE1_64_BL_OUT_CH1_2                   0x0164
#define PAGE1_65_BL_OUT_CH2_0                   0x0165
#define PAGE1_66_BL_OUT_CH2_1                   0x0166
#define PAGE1_67_BL_OUT_CH2_2                   0x0167
#define PAGE1_68_BL_OUT_CH3_0                   0x0168
#define PAGE1_69_BL_OUT_CH3_1                   0x0169
#define PAGE1_6A_BL_OUT_CH3_2                   0x016A
#define PAGE1_6B_BL_OUT_CH4_0                   0x016B
#define PAGE1_6C_BL_OUT_CH4_1                   0x016C
#define PAGE1_6D_BL_OUT_CH4_2                   0x016D
#define PAGE1_6E_BL_OUT_CH5_0                   0x016E
#define PAGE1_6F_BL_OUT_CH5_1                   0x016F
#define PAGE1_70_BL_OUT_CH5_2                   0x0170
#define PAGE1_71_BL_OUT_CH6_0                   0x0171
#define PAGE1_72_BL_OUT_CH6_1                   0x0172
#define PAGE1_73_BL_OUT_CH6_2                   0x0173
#define PAGE1_74_BL_OUT_CH7_0                   0x0174
#define PAGE1_75_BL_OUT_CH7_1                   0x0175
#define PAGE1_76_BL_OUT_CH7_2                   0x0176
#define PAGE1_77_CAP_INI_CH0_0                  0x0177
#define PAGE1_78_CAP_INI_CH0_1                  0x0178
#define PAGE1_79_CAP_INI_CH0_2                  0x0179
#define PAGE1_7B_CAP_INI_CH1_0                  0x017B
#define PAGE1_7C_CAP_INI_CH1_1                  0x017C
#define PAGE1_7D_CAP_INI_CH1_2                  0x017D
#define PAGE1_7F_CAP_INI_CH2_0                  0x017F
#define PAGE1_80_CAP_INI_CH2_1                  0x0180
#define PAGE1_81_CAP_INI_CH2_2                  0x0181
#define PAGE1_83_CAP_INI_CH3_0                  0x0183
#define PAGE1_84_CAP_INI_CH3_1                  0x0184
#define PAGE1_85_CAP_INI_CH3_2                  0x0185
#define PAGE1_87_CAP_INI_CH4_0                  0x0187
#define PAGE1_88_CAP_INI_CH4_1                  0x0188
#define PAGE1_89_CAP_INI_CH4_2                  0x0189
#define PAGE1_8B_CAP_INI_CH5_0                  0x018B
#define PAGE1_8C_CAP_INI_CH5_1                  0x018C
#define PAGE1_8D_CAP_INI_CH5_2                  0x018D
#define PAGE1_8F_CAP_INI_CH6_0                  0x018F
#define PAGE1_90_CAP_INI_CH6_1                  0x0190
#define PAGE1_91_CAP_INI_CH6_2                  0x0191
#define PAGE1_93_CAP_INI_CH7_0                  0x0193
#define PAGE1_94_CAP_INI_CH7_1                  0x0194
#define PAGE1_95_CAP_INI_CH7_2                  0x0195
#define PAGE1_97_LP_TEMP_DATA_0                 0x0197
#define PAGE1_98_LP_TEMP_DATA_1                 0x0198
#define PAGE1_A0_LP_OUT_DELTA_THRES_CH0_CFG0    0x01A0
#define PAGE1_A1_LP_OUT_DELTA_THRES_CH0_CFG1    0x01A1
#define PAGE1_A3_LP_OUT_DELTA_THRES_CH1_CFG0    0x01A3
#define PAGE1_A4_LP_OUT_DELTA_THRES_CH1_CFG1    0x01A4
#define PAGE1_A6_LP_OUT_DELTA_THRES_CH2_CFG0    0x01A6
#define PAGE1_A7_LP_OUT_DELTA_THRES_CH2_CFG1    0x01A7
#define PAGE1_A9_LP_OUT_DELTA_THRES_CH3_CFG0    0x01A9
#define PAGE1_AA_LP_OUT_DELTA_THRES_CH3_CFG1    0x01AA
#define PAGE1_AC_LP_OUT_DELTA_THRES_CH4_CFG0    0x01AC
#define PAGE1_AD_LP_OUT_DELTA_THRES_CH4_CFG1    0x01AD
#define PAGE1_AF_LP_OUT_DELTA_THRES_CH5_CFG0    0x01AF
#define PAGE1_B0_LP_OUT_DELTA_THRES_CH5_CFG1    0x01B0
#define PAGE1_B2_LP_OUT_DELTA_THRES_CH6_CFG0    0x01B2
#define PAGE1_B3_LP_OUT_DELTA_THRES_CH6_CFG1    0x01B3
#define PAGE1_B5_LP_OUT_DELTA_THRES_CH7_CFG0    0x01B5
#define PAGE1_B6_LP_OUT_DELTA_THRES_CH7_CFG1    0x01B6
#define PAGE1_C0_BL_IN_NO_UP_NUM_SEL0           0x01C0
#define PAGE1_C1_BL_IN_NO_UP_NUM_SEL1           0x01C1
#define PAGE1_C2_BL_IN_NO_UP_NUM_SEL2           0x01C2
#define PAGE1_C3_BL_IN_NO_UP_NUM_SEL3           0x01C3
#define PAGE1_C4_LP_SIGN_THRES_EN               0x01C4
#define PAGE1_C5_LP_SIGN_THRES_CH0              0x01C5
#define PAGE1_CD_PROX_THRES_CFG0                0x01CD
#define PAGE1_CE_PROX_THRES_CFG1                0x01CE
#define PAGE1_CF_BL_ALPHA_UP_DN_SEL             0x01CF
#define PAGE1_E1_PROX_LOW_THRES1_CFG0           0x01E1
#define PAGE1_E2_PROX_LOW_THRES1_CFG1           0x01E2
#define PAGE1_E3_PROX_LOW_THRES1_CFG2           0x01E3
#define PAGE1_E4_PROX_LOW_THRES1_CFG3           0x01E4
#define PAGE1_E5_PROX_LOW_THRES1_CFG4           0x01E5
#define PAGE1_E6_PROX_LOW_THRES1_CFG5           0x01E6
#define PAGE1_E7_PROX_LOW_THRES1_CFG6           0x01E7
#define PAGE1_E8_PROX_LOW_THRES1_CFG7           0x01E8
    
#define PAGE2_09_TBASE_CFG4_0                   0x0209
#define PAGE2_0A_TBASE_CFG4_1                   0x020A
#define PAGE2_1A_KBASE4_CH0_CFG0                0x021A
#define PAGE2_1D_KBASE5_CH0_CFG0                0x021D
#define PAGE2_32_KBASE4_CH1_CFG0                0x0232
#define PAGE2_35_KBASE5_CH1_CFG0                0x0235
#define PAGE2_4A_KBASE4_CH2_CFG0                0x024A
#define PAGE2_4D_KBASE5_CH2_CFG0                0x024D
#define PAGE2_62_KBASE4_CH3_CFG0                0x0262
#define PAGE2_65_KBASE5_CH3_CFG0                0x0265
#define PAGE2_7A_KBASE4_CH4_CFG0                0x027A
#define PAGE2_7D_KBASE5_CH4_CFG0                0x027D
#define PAGE2_92_KBASE4_CH5_CFG0                0x0292
#define PAGE2_95_KBASE5_CH5_CFG0                0x0295
#define PAGE2_AA_KBASE4_CH6_CFG0                0x02AA
#define PAGE2_AD_KBASE5_CH6_CFG0                0x02AD
#define PAGE2_C2_KBASE4_CH7_CFG0                0x02C2
#define PAGE2_C5_KBASE5_CH7_CFG0                0x02C5
#define PAGE2_F0_CH_CFG_ADD0                    0x02F0
#define PAGE2_F1_CH_CFG_ADD1                    0x02F1
#define PAGE2_F2_CH_CFG_ADD2                    0x02F2
#define PAGE2_F3_CH_CFG_ADD3                    0x02F3
#define PAGE2_F4_CH_CFG_ADD4                    0x02F4
#define PAGE2_F5_CH_CFG_ADD5                    0x02F5
#define PAGE2_F6_CH_CFG_ADD6                    0x02F6
#define PAGE2_F7_CH_CFG_ADD7                    0x02F7
#define PAGE2_F8_PRES_RST_CFG0                  0x02F8
#define PAGE2_F9_PRES_RST_CFG1                  0x02F9
#define PAGE2_FA_PRES_RST_CFG2                  0x02FA
#define PAGE2_FC_CH_SUB_CFG                     0x02FC


/*      Chip ID     */
#define ID_VALUE_REV3                       0x2E
//============================================hx9035 registers END}


#define HX9035_DRIVER_NAME "hx9035" //i2c addr: HX9036=0x28, HX9035=0x28
#define HX9035_CH_NUM 8
#define HX9035_MAX_XFER_SIZE 32

#define HX9035_ODR_MS 200

#define HX9035_DATA_LOCK 1
#define HX9035_DATA_UNLOCK 0

#define HX9035_OUTPUT_RAW_DIFF 1
#define HX9035_OUTPUT_LP_BL 0

#define CH_DATA_2BYTES 2
#define CH_DATA_3BYTES 3
#define CH_DATA_BYTES_MAX CH_DATA_3BYTES

#define HX9035_ID_CHECK_COUNT 5
#define HX9035_CHIP_ID ID_VALUE_REV3

#define IDLE 0 //远离
#define PROXACTIVE 1 //较近


/*struct hx9035_near_far_threshold {
    int16_t near1; //较近
    int16_t near2; //很近
    int16_t near1_adj;
    int16_t near2_adj;
}; */

struct hx9035_near_far_threshold {
    int16_t thr_near;
    int16_t thr_far;
};

struct hx9035_addr_val_pair {
    uint16_t addr;
    uint8_t val;
};

struct hx9035_channel_info {
    char *name;
    bool enabled;//该通道是否已经打开
    bool used;   //该通道是否有效，如：对于9036 ch3是无效的，此时used应该为0
    int state;
    struct sensors_classdev hx9035_sensors_classdev;
    struct input_dev *hx9035_input_dev;
};

struct hx9035_platform_data {
    struct device *pdev;
    struct delayed_work polling_work;
    struct delayed_work hw_monitor_work;
    spinlock_t lock;
    uint32_t channel_used_flag;
    int irq;      //irq number
    int irq_gpio; //gpio numver
    char irq_disabled;
    uint32_t prox_state_reg;
    uint32_t prox_state_cmp;
    uint32_t prox_state_reg_pre;
    uint32_t prox_state_cmp_pre;
    uint32_t prox_state_far_to_near_flag;
    uint32_t grip_sensor_ch;
    uint32_t grip_sensor_sub_ch;
    uint32_t grip_sensor_wifi_ch;
#if SAR_IN_FRANCE
#if USE_USB
    struct notifier_block ps_notif;
    bool ps_is_present;
#endif
    bool sar_first_boot;
    int interrupt_count;
    int16_t ch0_backgrand_cap;
    int16_t ch1_backgrand_cap;
    int16_t ch2_backgrand_cap;
    int user_test;
    bool anfr_export_exit;
#endif
#if POWER_ENABLE
    int power_state;
#endif
#if defined(CONFIG_SENSORS)
	bool skip_data;
#endif
};

static struct hx9035_channel_info hx9035_channels[] = {
    {
        .name = "grip_sensor",
        .enabled = false,
        .used = false,
    },
    {
        .name = "grip_sensor_sub",
        .enabled = false,
        .used = false,
    },
    {
        .name = "grip_sensor_wifi",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9035_ch3",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9035_ch4",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9035_ch5",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9035_ch6",
        .enabled = false,
        .used = false,
    },
    {
        .name = "hx9035_ch7",
        .enabled = false,
        .used = false,
    },
};

static struct hx9035_addr_val_pair hx9035_reg_init_list[] = {
    {PAGE0_03_CH_NUM_CFG,                0x00},
    {PAGE0_40_PRF_CFG,                   0x17},
    {PAGE0_41_CH10_SCAN_FACTOR,          0x00},
    {PAGE0_42_CH32_SCAN_FACTOR,          0x00},
    {PAGE0_43_CH54_SCAN_FACTOR,          0x00},
    {PAGE0_44_CH76_SCAN_FACTOR,          0x00},
    {PAGE0_45_CH10_DOZE_FACTOR,          0x00},
    {PAGE0_46_CH32_DOZE_FACTOR,          0x00},
    {PAGE0_47_CH54_DOZE_FACTOR,          0x00},
    {PAGE0_48_CH76_DOZE_FACTOR,          0x00},
    
    {PAGE0_4E_TC_NOSR_CFG0,              0x33}, 
    {PAGE0_4F_TC_NOSR_CFG1,              0x03},
    {PAGE0_50_TC_NOSR_CFG2,              0x00},
    {PAGE0_51_TC_NOSR_CFG3,              0x00},
    {PAGE0_52_TC_AVG_CFG0,               0x44},
    {PAGE0_53_TC_AVG_CFG1,               0x05},
    {PAGE0_54_TC_AVG_CFG2,               0x00},
    {PAGE0_55_TC_AVG_CFG3,               0x00}, 

    {PAGE0_78_RANGE_CFG_0,               0x35}, //ch1 ch0 ： (N+1)* 1.25pF 
    {PAGE0_79_RANGE_CFG_1,               0x31}, //ch3 ch2
    {PAGE0_7A_RANGE_CFG_2,               0x33}, //ch5 ch4
    {PAGE0_7B_RANGE_CFG_3,               0x33}, //ch7 ch6 json config available last line.

    {PAGE0_83_AMP_CHOP_CFG,              0xFF}, //ch7 ch6 json config available last line.
    
    {PAGE0_9A_DSP_CONFIG_CTRL7,          0x00}, // 1: diff_data; 0: lp
    {PAGE0_9B_DSP_CONFIG_CTRL8,          0xFF}, // 1: bl; 0: raw
    {PAGE0_9C_DSP_CONFIG_CTRL9,          0x10}, // high 4 byte (n+1)*-2048
    
    {PAGE0_A1_INTERRUPT_CFG1,            0x00}, //INT_RDY_EN
    {PAGE0_A2_INTERRUPT_CFG2,            0x07}, //INT_PROX_LOW1_EN
    {PAGE0_A3_INTERRUPT_CFG3,            0x01}, //INT_PROX_LOW2_EN
    {PAGE0_A5_INTERRUPT_CFG5,            0x07}, //INT_PROX_HIGH1_EN
    {PAGE0_A6_INTERRUPT_CFG6,            0x01}, //INT_PROX_HIGH2_EN
    
    {PAGE0_AB_PROX_INT_CFG,              0x11}, //7:4 PROX_INT_HIGH_NUM  3:0 PROX_INT_LOW_NUM   
    {PAGE0_AC_BL_UP_CFG0,                0x88},
    {PAGE0_AD_BL_UP_CFG1,                0x88},
    {PAGE0_AE_BL_UP_CFG2,                0x88},
    {PAGE0_AF_BL_UP_CFG3,                0x88},
    {PAGE0_B0_BL_DN_CFG0,                0x11},
    {PAGE0_B1_BL_DN_CFG1,                0x11},
    {PAGE0_B2_BL_DN_CFG2,                0x11},
    {PAGE0_B3_BL_DN_CFG3,                0x11},

#ifdef WT_COMPILE_FACTORY_VERSION
    {PAGE0_B5_LP_ALP_CFG0,               0x00},
    {PAGE0_B6_LP_ALP_CFG1,               0x00},
    {PAGE0_B7_LP_ALP_CFG2,               0x00},
    {PAGE0_B8_LP_ALP_CFG3,               0x00},
#else
    {PAGE0_B5_LP_ALP_CFG0,               0x22},
    {PAGE0_B6_LP_ALP_CFG1,               0x22},
    {PAGE0_B7_LP_ALP_CFG2,               0x22},
    {PAGE0_B8_LP_ALP_CFG3,               0x22},
#endif
                        
    {PAGE0_C9_PROX_HIGH_THRES1_CFG0,     0x03}, //PROX_HIGH_THRES1
    {PAGE0_CA_PROX_HIGH_THRES1_CFG1,     0x23},
    {PAGE0_CB_PROX_HIGH_THRES1_CFG2,     0x23},
    {PAGE0_CC_PROX_HIGH_THRES1_CFG3,     0x1F},
    {PAGE0_CD_PROX_HIGH_THRES1_CFG4,     0x1F},
    {PAGE0_CE_PROX_HIGH_THRES1_CFG5,     0x1F},
    {PAGE0_CF_PROX_HIGH_THRES1_CFG6,     0x1F},
    {PAGE0_D0_PROX_HIGH_THRES1_CFG7,     0x1F},
    {PAGE0_D1_PROX_HIGH_THRES2_CFG0,     0x09},//PROX_HIGH_THRES2
    {PAGE0_D2_PROX_HIGH_THRES2_CFG1,     0x1F},
    {PAGE0_D3_PROX_HIGH_THRES2_CFG2,     0x1F},
    {PAGE0_D4_PROX_HIGH_THRES2_CFG3,     0x1F},
    {PAGE0_D5_PROX_HIGH_THRES2_CFG4,     0x1F},
    {PAGE0_D6_PROX_HIGH_THRES2_CFG5,     0x1F},
    {PAGE0_D7_PROX_HIGH_THRES2_CFG6,     0x1F},
    {PAGE0_D8_PROX_HIGH_THRES2_CFG7,     0x1F},
    
    {PAGE0_E1_DITHER_CFG,                0x01}, //freq
    {PAGE0_E2_SAMPLE_NUM_0,              0x60},
    {PAGE0_E3_SAMPLE_NUM_1,              0x00},
    {PAGE0_E4_INTEGRATION_NUM_0,         0x60},
    {PAGE0_E5_INTEGRATION_NUM_1,         0x00},
    {PAGE0_ED_LPOUT_DELTA_CFG,           0xFF},  //1: close usefilter

    {PAGE1_E1_PROX_LOW_THRES1_CFG0,      0x01},
    {PAGE1_E2_PROX_LOW_THRES1_CFG1,      0x01},
    {PAGE1_E3_PROX_LOW_THRES1_CFG2,      0x01},
    {PAGE1_E4_PROX_LOW_THRES1_CFG3,      0x01},
    {PAGE1_E5_PROX_LOW_THRES1_CFG4,      0x01},
    {PAGE1_E6_PROX_LOW_THRES1_CFG5,      0x01},
    {PAGE1_E7_PROX_LOW_THRES1_CFG6,      0x01},
    {PAGE1_E8_PROX_LOW_THRES1_CFG7,      0x01},

    {PAGE1_16_REG_TO_ANA2,               0xc0},//CS/INT复用配置
    {PAGE1_17_REG_TO_ANA3,               0x00},//配置CS3引脚为INT功能
    {PAGE1_1C_REG_TO_ANA8,               0xc0},
};
#endif

