/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef SD7601_H_
#define SD7601_H_
/* +S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */
enum {
	EVENT_EOC,
	EVENT_RECHARGE,
	CHARGER_DEV_NOTIFY_EOC,
	CHARGER_DEV_NOTIFY_RECHG,
};
/* -S96818AA1-6688, zhouxiaopeng2.wt, MODIFY, 20230629, UI_soc reaches 100% after charging stops */

#define DEFAULT_ILIMIT     3000
#define DEFAULT_VLIMIT		4600
#define DEFAULT_CC			2000
#define DEFAULT_CV			4400
#define DEFAULT_IPRECHG		540
#define DEFAULT_ITERM		180
#define DEFAULT_VSYS_MIN	3500
#define DEFAULT_RECHG		100

//do not include ADC registers in SD7601AD
#define SD7601_REG_NUM 17

#define SD7601_R00		0x00
#define SD7601_R01		0x01
#define SD7601_R02		0x02
#define SD7601_R03		0x03
#define SD7601_R04		0x04
#define SD7601_R05		0x05
#define SD7601_R06		0x06
#define SD7601_R07		0x07
#define SD7601_R08		0x08
#define SD7601_R09		0x09
#define SD7601_R0A		0x0A
#define SD7601_R0B		0x0B
#define SD7601_R0C		0x0C
#define SD7601_R0D		0x0D
#define SD7601_R0E		0x0E
#define SD7601_R0F		0x0F
#define SD7601_R10		0x10

#define SD7601_COMPINENT_ID		0x0100
#define SD7601D_COMPINENT_ID		0x01A3
#define SD7601AD_COMPINENT_ID		0x02A1
#define SD7601AD_COMPINENT_ID2   0x02A2

/* CON0 */
#define CON0_EN_HIZ_MASK	0x1
#define CON0_EN_HIZ_SHIFT	7

#define CON0_EN_ICHG_MON_MASK	0x3
#define CON0_EN_ICHG_MON_SHIFT	5

#define CON0_IINLIM_MASK	0x1F
#define CON0_IINLIM_SHIFT	0

#define INPUT_CURRT_STEP	((uint32_t)100)
#define INPUT_CURRT_MAX		((uint32_t)3200)
#define INPUT_CURRT_MIN		((uint32_t)100)

/* CON1 */
#define CON1_WD_MASK		0x1
#define CON1_WD_SHIFT		6

#define CON1_OTG_CONFIG_MASK		0x1
#define CON1_OTG_CONFIG_SHIFT		5

#define CON1_CHG_CONFIG_MASK		0x1
#define CON1_CHG_CONFIG_SHIFT		4

#define CON1_SYS_V_LIMIT_MASK		0x7
#define CON1_SYS_V_LIMIT_SHIFT		1
#define CON1_SYS_V_LIMIT_MIN		((uint32_t)3000)
#define CON1_SYS_V_LIMIT_MAX		((uint32_t)3700)
#define CON1_SYS_V_LIMIT_STEP		((uint32_t)100)

#define CON1_MIN_VBAT_SEL_MASK		0x1
#define CON1_MIN_VBAT_SEL_SHIFT		0

#define SYS_VOL_STEP		((uint32_t)100)
#define SYS_VOL_MIN		((uint32_t)3000)
#define SYS_VOL_MAX		((uint32_t)3700)

/* CON2 */
#define CON2_BOOST_ILIM_MASK		0x1
#define CON2_BOOST_ILIM_SHIFT		7

#define CON2_ICHG_MASK		0x3F
#define CON2_ICHG_SHIFT		0
#define ICHG_CURR_STEP		((uint32_t)60)
#define ICHG_CURR_MIN		((uint32_t)0)
#define ICHG_CURR_MAX		((uint32_t)3000)

/* CON3 */
#define CON3_IPRECHG_MASK	0xF
#define CON3_IPRECHG_SHIFT	4
#define IPRECHG_CURRT_STEP	((uint32_t)60)
#define IPRECHG_CURRT_MAX	((uint32_t)960)
#define IPRECHG_CURRT_MIN	((uint32_t)60)

#define CON3_ITERM_MASK		0x7
#define CON3_ITERM_SHIFT	0
#define EOC_CURRT_STEP		((uint32_t)60)
#define EOC_CURRT_MAX		((uint32_t)480)
#define EOC_CURRT_MIN		((uint32_t)60)

/* CON4 */
#define CON4_VREG_MASK		0x1F
#define CON4_VREG_SHIFT		3

#define CON4_TOPOFF_TIME_MASK		0x3
#define CON4_TOPOFF_TIME_SHIFT		1

#define CON4_VRECHG_MASK   	0x1
#define CON4_VRECHG_SHIFT  	0

//cv
#define VREG_VOL_STEP		((uint32_t)32)
#define VREG_VOL_MIN 		((uint32_t)3856)
#define VREG_VOL_MAX		((uint32_t)4624)
//rechg vol
#define VRCHG_VOL_STEP		((uint32_t)100)
#define VRCHG_VOL_MIN		((uint32_t)100)
#define VRCHG_VOL_MAX		((uint32_t)200)

/* CON5 */
#define CON5_EN_TERM_CHG_MASK   	0x1
#define CON5_EN_TERM_CHG_SHIFT  	7

#define CON5_WTG_TIM_SET_MASK		0x3
#define CON5_WTG_TIM_SET_SHIFT  	4

#define CON5_EN_TIMER_MASK   		0x1
#define CON5_EN_TIMER_SHIFT  		3

#define CON5_SET_CHG_TIM_MASK   	0x1
#define CON5_SET_CHG_TIM_SHIFT  	2

#define CON5_TREG_MASK 		0x1
#define CON5_TREG_SHIFT 	1

#define CON5_JEITA_ISET_MASK   		0x1
#define CON5_JEITA_ISET_SHIFT 		0

/* CON6 */
#define CON6_OVP_MASK 		0x3
#define CON6_OVP_SHIFT 		6
#define OVP_VOL_MIN		((uint32_t)5500)
#define OVP_VOL_MAX		((uint32_t)14000)

#define CON6_BOOST_VLIM_MASK 		0x3
#define CON6_BOOST_VLIM_SHIFT 		4
#define BOOST_VOL_STEP		((uint32_t)150)
#define BOOST_VOL_MIN		((uint32_t)4850)
#define BOOST_VOL_MAX		((uint32_t)5300)

#define CON6_VINDPM_MASK   	0xF
#define CON6_VINDPM_SHIFT  	0
#define VINDPM_VOL_STEP		((uint32_t)100)
#define VINDPM_VOL_MIN		((uint32_t)3900)
#define VINDPM_VOL_MAX		((uint32_t)5400)

/* CON7 */
#define FORCE_IINDET_MASK 	0x1
#define FORCE_IINDET_SHIFT 	7

#define TMR2X_MASK 		0x1
#define TMR2X_SHIFT 		6

#define BATFET_DIS_MASK 	0x1
#define BATFET_DIS_SHIFT 	5

#define JEITA_VSET_MASK 	0x1
#define JEITA_VSET_SHIFT	 4

#define BATFET_DLY_MASK	 	0x1
#define BATFET_DLY_SHIFT 	3

#define BATFET_RST_EN_MASK 	0x1
#define BATFET_RST_EN_SHIFT 	2

#define VDPM_BAT_TRACK_MASK 	0x1
#define VDPM_BAT_TRACK_SHIFT 	0

/* CON8 */
#define CON8_VBUS_STAT_MASK   	0x7
#define CON8_VBUS_STAT_SHIFT  	5

#define CON8_CHRG_STAT_MASK   	0x3
#define CON8_CHRG_STAT_SHIFT  	3

#define CON8_PG_STAT_MASK   	0x1
#define CON8_PG_STAT_SHIFT  	2

#define CON8_THM_STAT_MASK   	0x1
#define CON8_THM_STAT_SHIFT  	1

#define CON8_VSYS_STAT_MASK   	0x1
#define CON8_VSYS_STAT_SHIFT 	 0

/* CON9 */
#define CON9_WATG_STAT_MASK   	0x1
#define CON9_WATG_STAT_SHIFT  	7

#define CON9_BOOST_STAT_MASK   	0x1
#define CON9_BOOST_STAT_SHIFT  	6

#define CON9_CHRG_FAULT_MASK   	0x3
#define CON9_CHRG_FAULT_SHIFT  	4

#define CON9_BAT_STAT_MASK   	0x1
#define CON9_BAT_STAT_SHIFT  	3

#define CON9_NTC_STAT_MASK   	0x7
#define CON9_NTC_STAT_SHIFT  	0

/* CONA */
#define CONA_VBUS_GD_MASK   	0x1
#define CONA_VBUS_GD_SHIFT  	7

#define CONA_VINDPM_STAT_MASK   	0x1
#define CONA_VINDPM_STAT_SHIFT  	6

#define CONA_IDPM_STAT_MASK   	0x1
#define CONA_IDPM_STAT_SHIFT  	5

#define CONA_TOPOFF_ACTIVE_MASK   	0x1
#define CONA_TOPOFF_ACTIVE_SHIFT  	3

#define CONA_ACOV_STAT_MASK  	0x1
#define CONA_ACOV_STAT_SHIFT  	2

#define	CONA_VINDPM_INT_MASK	  	0x1
#define	CONA_VINDPM_INT_SHIFT	1

#define	CONA_IINDPM_INT_MASK	  	0x1
#define	CONA_IINDPM_INT_SHIFT	   	0

/*CONB*/
#define CONB_REG_RST_MASK   	0x1
#define CONB_REG_RST_SHIFT  	7

#define CONB_PN_MASK				0x0F
#define CONB_PN_SHIFT			3

#define CONB_DEV_REV_MASK		0x03
#define CONB_DEV_REV_SHIFT		0

/*CONC*/
#define CONC_EN_HVDCP_MASK   	0x1
#define CONC_EN_HVDCP_SHIFT  	7

#define CONC_DP_VOLT_MASK   	0x3
#define CONC_DP_VOLT_SHIFT  	2

#define CONC_DM_VOLT_MASK   	0x3
#define CONC_DM_VOLT_SHIFT  	0

#define CONC_DP_DM_MASK       0xF
#define CONC_DP_DM_SHIFT      0

#define CONC_DP_DM_VOL_HIZ    0
#define CONC_DP_DM_VOL_0P0    1
#define CONC_DP_DM_VOL_0P6    2
#define CONC_DP_DM_VOL_3P3    3

/*COND*/
#define COND_CV_SP_MASK   	0x1
#define COND_CV_SPP_SHIFT  	6

#define COND_BAT_COMP_MASK   	0x7
#define COND_BAT_COMP_SHIFT  	3

#define COND_COMP_MAX_MASK   	0x7
#define COND_COMP_MAX_SHIFT  	0

/*CON10*/
#define CON10_HV_VINDPM_MASK   	0x1
#define CON10_HV_VINDPM_SHIFT  	7

#define CON10_VLIMIT_MASK   	0x7F
#define CON10_VLIMIT_SHIFT  	0
#define VILIMIT_VOL_STEP	((uint32_t)100)
#define VILIMIT_VOL_MAX 	((uint32_t)12000)
#define VILIMIT_VOL_MIN		((uint32_t)3900)

//for SD7601AD only
/*CON11*/
#define SD7601_R11		      0x11
#define SD7601_R12		      0x12
#define SD7601_R13		      0x13
#define SD7601_R14		      0x14
#define SD7601_R15		      0x15
#define SD7601_R16		      0x16
#define SD7601_R17		      0x17

#define CON11_START_ADC_MASK  0x01
#define CON11_START_ADC_SHIFT 7
#define CON11_CONV_RATE_MASK  0x01
#define CON11_CONV_RATE_SHIFT 6

#define SD7601_ADC_CHANNEL_START SD7601_R12
enum sd7601_adc_channel{
   error_channel= -1,
   vbat_channel =  0,
   vsys_channel =  1,
   tbat_channel =  2,
   vbus_channel =  3,
   ibat_channel =  4,
   ibus_channel =  5,
   max_channel,
};

#endif // _sd7601_SW_H_

