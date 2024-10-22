/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */
#ifndef _MT6681_H_
#define _MT6681_H_

#include <linux/mfd/mt6681-private.h>

/* AUDENC_ANA_CON16: */
#define RG_AUD_MICBIAS1_LOWP_EN (1 << MT6681_RG_AUDMICBIAS1LOWPEN_SHIFT)
/* AUDENC_ANA_CON18: */
#define RG_ACCDET_MODE_ANA11_MODE1 (0x000F)
#define RG_ACCDET_MODE_ANA11_MODE2 (0x008F)
#define RG_ACCDET_MODE_ANA11_MODE6 (0x008F)

/* ------Register_AUXADC_REG  Bit Define------ */
/* AUXADC_ADC5:  Auxadc CH5 read data */
#define AUXADC_DATA_RDY_CH5 (1 << 15)
#define AUXADC_DATA_PROCEED_CH5 (0 << 15)
#define AUXADC_DATA_MASK (0x0FFF)

/* AUXADC_RQST0_SET:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_SET (1 << 5)
/* AUXADC_RQST0_CLR:  Auxadc CH5 request, relevant 0x07EC */
#define AUXADC_RQST_CH5_CLR (1 << 5)

/* -----Register_EFUSE_REG  Bit Define-------- */
#define ACCDET_CALI_MASK0 (0xFF)
#define ACCDET_CALI_MASK1 (0xFF << 8)
#define ACCDET_CALI_MASK2 (0xFF)
#define ACCDET_CALI_MASK3 (0xFF << 8)
#define ACCDET_CALI_MASK4 (0xFF)
/* -----Register_ACCDET_REG  Bit Define------- */
#define ACCDET_EINT1_IRQ_CLR_B11 (0x01 << MT6681_ACCDET_EINT1_IRQ_CLR_SHIFT)
#define ACCDET_EINT0_IRQ_CLR_B10 (0x01 << MT6681_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_EINT_IRQ_CLR_B10_11 (0x03 << MT6681_ACCDET_EINT0_IRQ_CLR_SHIFT)
#define ACCDET_IRQ_CLR_B8 (0x01 << MT6681_ACCDET_IRQ_CLR_SHIFT)
#define ACCDET_EINT1_IRQ_B3 (0x01 << MT6681_ACCDET_EINT1_IRQ_SHIFT)
#define ACCDET_EINT0_IRQ_B2 (0x01 << MT6681_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_EINT_IRQ_B2_B3 (0x03 << MT6681_ACCDET_EINT0_IRQ_SHIFT)
#define ACCDET_IRQ_B0 (0x01 << MT6681_ACCDET_IRQ_SHIFT)
/* ACCDET_CON25: RO, accdet FSM state,etc.*/
#define ACCDET_STATE_MEM_IN_OFFSET (MT6681_ACCDET_MEM_IN_SHIFT)
#define ACCDET_STATE_AB_MASK (0x03)
#define ACCDET_STATE_AB_00 (0x00)
#define ACCDET_STATE_AB_01 (0x01)
#define ACCDET_STATE_AB_10 (0x02)
#define ACCDET_STATE_AB_11 (0x03)
/* ACCDET_CON19 */
#define ACCDET_EINT0_STABLE_VAL                                                \
	((1 << MT6681_ACCDET_DA_STABLE_SHIFT)                                  \
	 | (1 << MT6681_ACCDET_EINT0_EN_STABLE_SHIFT)                          \
	 | (1 << MT6681_ACCDET_EINT0_CMPEN_STABLE_SHIFT)                       \
	 | (1 << MT6681_ACCDET_EINT0_CEN_STABLE_SHIFT))
#define ACCDET_EINT1_STABLE_VAL                                                \
	((1 << MT6681_ACCDET_DA_STABLE_SHIFT)                                  \
	 | (1 << MT6681_ACCDET_EINT1_EN_STABLE_SHIFT)                          \
	 | (1 << MT6681_ACCDET_EINT1_CMPEN_STABLE_SHIFT)                       \
	 | (1 << MT6681_ACCDET_EINT1_CEN_STABLE_SHIFT))

/* hw gain */
static const uint32_t kHWGainMap[] = {
	0x00000, // -64.0 dB (mute)
	0x00173, // -63.0 dB
	0x001A0, // -62.0 dB
	0x001D3, // -61.0 dB
	0x0020C, // -60.0 dB
	0x0024C, // -59.0 dB
	0x00294, // -58.0 dB
	0x002E4, // -57.0 dB
	0x0033E, // -56.0 dB
	0x003A4, // -55.0 dB
	0x00416, // -54.0 dB
	0x00495, // -53.0 dB
	0x00524, // -52.0 dB
	0x005C5, // -51.0 dB
	0x00679, // -50.0 dB
	0x00744, // -49.0 dB
	0x00827, // -48.0 dB
	0x00925, // -47.0 dB
	0x00A43, // -46.0 dB
	0x00B84, // -45.0 dB
	0x00CEC, // -44.0 dB
	0x00E7F, // -43.0 dB
	0x01044, // -42.0 dB
	0x01240, // -41.0 dB
	0x0147A, // -40.0 dB
	0x016FA, // -39.0 dB
	0x019C8, // -38.0 dB
	0x01CED, // -37.0 dB
	0x02075, // -36.0 dB
	0x0246B, // -35.0 dB
	0x028DC, // -34.0 dB
	0x02DD9, // -33.0 dB
	0x03371, // -32.0 dB
	0x039B8, // -31.0 dB
	0x040C3, // -30.0 dB
	0x048AA, // -29.0 dB
	0x05188, // -28.0 dB
	0x05B7B, // -27.0 dB
	0x066A4, // -26.0 dB
	0x0732A, // -25.0 dB
	0x08138, // -24.0 dB
	0x090FC, // -23.0 dB
	0x0A2AD, // -22.0 dB
	0x0B687, // -21.0 dB
	0x0CCCC, // -20.0 dB
	0x0E5CA, // -19.0 dB
	0x101D3, // -18.0 dB
	0x12149, // -17.0 dB
	0x14496, // -16.0 dB
	0x16C31, // -15.0 dB
	0x198A1, // -14.0 dB
	0x1CA7D, // -13.0 dB
	0x2026F, // -12.0 dB
	0x24134, // -11.0 dB
	0x287A2, // -10.0 dB
	0x2D6A8, // -9.0 dB
	0x32F52, // -8.0 dB
	0x392CE, // -7.0 dB
	0x4026E, // -6.0 dB
	0x47FAC, // -5.0 dB
	0x50C33, // -4.0 dB
	0x5A9DF, // -3.0 dB
	0x65AC8, // -2.0 dB
	0x72148, // -1.0 dB
	0x80000, //  0.0 dB
	0x8F9E4, // 1.0 dB
	0xA1248, // 2.0 dB
	0xB4CE4, // 3.0 dB
	0xCADDC, // 4.0 dB
	0xE39EA, // 5.0 dB
	0xFF64A, // 6.0 dB
	0x11E8E2, // 7.0 dB
	0x141856, // 8.0 dB
	0x168C08, // 9.0 dB
	0x194C54, // 10.0 dB
	0x1C6290, // 11.0 dB
	0x1FD93E, // 12.0 dB
	0x23BC16, // 13.0 dB
	0x28184C, // 14.0 dB
	0x2CFCC2, // 15.0 dB
	0x3279FE, // 16.0 dB
	0x38A2B6, // 17.0 dB
	0x3F8BDA, // 18.0 dB
	0x474CD0, // 19.0 dB
	0x500000, // 20.0 dB
	0x59C2F2, // 21.0 dB
	0x64B6C6, // 22.0 dB
	0x7100C0, // 23.0 dB
	0x7ECA98, // 24.0 dB
	0x8E432E, // 25.0 dB
};

#endif /* end _MT6681_H_ */
