/* SPDX-License-Identifier: GPL-2.0*/
/*
 * Copyright (c) 2022 Southchip Semiconductor Technology(Shanghai) Co., Ltd.
 */
#ifndef __SC8960X_HEADER__
#define __SC8960X_HEADER__

/* Register 00h */
#define SC8960X_REG_00      		0x00
#define REG00_ENHIZ_MASK		    0x80
#define REG00_ENHIZ_SHIFT		    7
#define	REG00_HIZ_ENABLE			1
#define	REG00_HIZ_DISABLE			0

#define	REG00_STAT_CTRL_MASK		0x60
#define REG00_STAT_CTRL_SHIFT		5
#define	REG00_STAT_CTRL_STAT		0
#define	REG00_STAT_CTRL_DISABLE		3

#define REG00_IINLIM_MASK		    0x1F
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_LSB			100
#define	REG00_IINLIM_BASE			100

/* Register 01h */
#define SC8960X_REG_01		    	0x01
#define REG01_PFM_DIS_MASK	      	0x80
#define	REG01_PFM_DIS_SHIFT			7
#define	REG01_PFM_ENABLE			0
#define	REG01_PFM_DISABLE			1

#define REG01_WDT_RESET_MASK		0x40
#define REG01_WDT_RESET_SHIFT		6
#define REG01_WDT_RESET				1

#define	REG01_OTG_CONFIG_MASK		0x20
#define	REG01_OTG_CONFIG_SHIFT		5
#define	REG01_OTG_ENABLE			1
#define	REG01_OTG_DISABLE			0

#define REG01_CHG_CONFIG_MASK     	0x10
#define REG01_CHG_CONFIG_SHIFT    	4
#define REG01_CHG_DISABLE        	0
#define REG01_CHG_ENABLE         	1

#define REG01_SYS_MINV_MASK       	0x0E
#define REG01_SYS_MINV_SHIFT      	1

#define	REG01_MIN_VBAT_SEL_MASK		0x01
#define	REG01_MIN_VBAT_SEL_SHIFT	0
#define	REG01_MIN_VBAT_2P8V			0
#define	REG01_MIN_VBAT_2P5V			1


/* Register 0x02*/
#define SC8960X_REG_02              0x02
#define	REG02_BOOST_LIM_MASK		0x80
#define	REG02_BOOST_LIM_SHIFT		7
#define	REG02_BOOST_LIM_0P5A		0
#define	REG02_BOOST_LIM_1P2A		1

#define REG02_ICHG_MASK           	0x3F
#define REG02_ICHG_SHIFT          	0
#define REG02_ICHG_BASE           	0
#define REG02_ICHG_LSB            	60

/* Register 0x03*/
#define SC8960X_REG_03              0x03
#define REG03_IPRECHG_MASK        	0xF0
#define REG03_IPRECHG_SHIFT       	4
#define REG03_IPRECHG_BASE        	60
#define REG03_IPRECHG_LSB         	60

#define REG03_ITERM_MASK          	0x0F
#define REG03_ITERM_SHIFT         	0
#define REG03_ITERM_BASE          	60
#define REG03_ITERM_LSB           	60


/* Register 0x04*/
#define SC8960X_REG_04              0x04
#define REG04_VREG_MASK           	0xF8
#define REG04_VREG_SHIFT          	3
#define REG04_VREG_BASE           	3847
#define REG04_VREG_LSB            	32

#define	REG04_TOPOFF_TIMER_MASK		0x06
#define	REG04_TOPOFF_TIMER_SHIFT	1
#define	REG04_TOPOFF_TIMER_DISABLE	0
#define	REG04_TOPOFF_TIMER_15M		1
#define	REG04_TOPOFF_TIMER_30M		2
#define	REG04_TOPOFF_TIMER_45M		3


#define REG04_VRECHG_MASK         	0x01
#define REG04_VRECHG_SHIFT        	0
#define REG04_VRECHG_100MV        	0
#define REG04_VRECHG_200MV        	1

/* Register 0x05*/
#define SC8960X_REG_05             	0x05
#define REG05_EN_TERM_MASK        	0x80
#define REG05_EN_TERM_SHIFT       	7
#define REG05_TERM_ENABLE         	1
#define REG05_TERM_DISABLE        	0

#define REG05_WDT_MASK            	0x30
#define REG05_WDT_SHIFT           	4
#define REG05_WDT_DISABLE         	0
#define REG05_WDT_40S             	1
#define REG05_WDT_80S             	2
#define REG05_WDT_160S            	3
#define REG05_WDT_BASE            	0
#define REG05_WDT_LSB             	40
/* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 start */#define REG06_VINDPM_HV_VAL			0x0F // 8.4V#define REG06_VINDPM_HV_LOW			8000 // 8V#define REG06_VINDPM_HV_TOP			9000 // 9V/* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 end */
#define REG05_EN_TIMER_MASK       	0x08
#define REG05_EN_TIMER_SHIFT      	3
#define REG05_CHG_TIMER_ENABLE    	1
#define REG05_CHG_TIMER_DISABLE   	0

#define REG05_CHG_TIMER_MASK      	0x04
#define REG05_CHG_TIMER_SHIFT     	2
#define REG05_CHG_TIMER_5HOURS    	0
#define REG05_CHG_TIMER_10HOURS   	1

#define	REG05_TREG_MASK				0x02
#define	REG05_TREG_SHIFT			1
#define	REG05_TREG_90C				0
#define	REG05_TREG_110C				1

#define REG05_JEITA_ISET_MASK     	0x01
#define REG05_JEITA_ISET_SHIFT    	0
#define REG05_JEITA_ISET_50PCT    	0
#define REG05_JEITA_ISET_20PCT    	1


/* Register 0x06*/
#define SC8960X_REG_06              0x06
#define	REG06_OVP_MASK				0xC0
#define	REG06_OVP_SHIFT				0x6
#define	REG06_OVP_5P8V				0
#define	REG06_OVP_6P4V				1
#define	REG06_OVP_11V				2
#define	REG06_OVP_14P2V				3

#define	REG06_BOOSTV_MASK			0x30
#define	REG06_BOOSTV_SHIFT			4
#define	REG06_BOOSTV_4P7V			0
#define	REG06_BOOSTV_4P9V			1
#define	REG06_BOOSTV_5P1V			2
#define	REG06_BOOSTV_5P3V			3

#define	REG06_VINDPM_MASK			0x0F
#define	REG06_VINDPM_SHIFT			0
#define	REG06_VINDPM_BASE			3900
#define	REG06_VINDPM_LSB			100
/* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 start */
#define REG06_VINDPM_HV_VAL			0x0F // 8.4V
#define REG06_VINDPM_HV_LOW			8000 // 8V
#define REG06_VINDPM_HV_TOP			9000 // 9V
/* hs14 code for AL6528A-1072 by zhangzhihao at 2023/1/18 end */
/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
#define REG06_MIN_VINDPM_VAL                    0x07 // 4.6V
#define REG06_MIN_VINDPM_THRES                  5000
/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */


/* Register 0x07*/
#define SC8960X_REG_07              0x07
#define REG07_FORCE_DPDM_MASK     	0x80
#define REG07_FORCE_DPDM_SHIFT    	7
#define REG07_FORCE_DPDM          	1

#define REG07_TMR2X_EN_MASK       	0x40
#define REG07_TMR2X_EN_SHIFT      	6
#define REG07_TMR2X_ENABLE        	1
#define REG07_TMR2X_DISABLE       	0

#define REG07_BATFET_DIS_MASK     	0x20
#define REG07_BATFET_DIS_SHIFT    	5
#define REG07_BATFET_OFF          	1
#define REG07_BATFET_ON          	0

#define REG07_JEITA_VSET_MASK     	0x10
#define REG07_JEITA_VSET_SHIFT    	4
#define REG07_JEITA_VSET_4100     	0
#define REG07_JEITA_VSET_VREG     	1

#define	REG07_BATFET_DLY_MASK		0x08
#define	REG07_BATFET_DLY_SHIFT		3
#define	REG07_BATFET_DLY_0S			0
#define	REG07_BATFET_DLY_10S		1

#define	REG07_BATFET_RST_EN_MASK	0x04
#define	REG07_BATFET_RST_EN_SHIFT	2
#define	REG07_BATFET_RST_DISABLE	0
#define	REG07_BATFET_RST_ENABLE		1

#define	REG07_VDPM_BAT_TRACK_MASK	0x03
#define	REG07_VDPM_BAT_TRACK_SHIFT 	0
#define	REG07_VDPM_BAT_TRACK_DISABLE	0
#define	REG07_VDPM_BAT_TRACK_200MV	1
#define	REG07_VDPM_BAT_TRACK_250MV	2
#define	REG07_VDPM_BAT_TRACK_300MV	3

/* Register 0x08*/
#define SC8960X_REG_08              0x08
#define REG08_VBUS_STAT_MASK        0xE0
#define REG08_VBUS_STAT_SHIFT       5
#define REG08_VBUS_TYPE_NONE	    0
#define REG08_VBUS_TYPE_SDP	        1
#define REG08_VBUS_TYPE_CDP	        2
#define REG08_VBUS_TYPE_DCP	        3
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#define REG08_VBUS_TYPE_HVDCP	    4
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */
#define REG08_VBUS_TYPE_UNKNOWN	    5
#define REG08_VBUS_TYPE_NON_STD	    6
#define REG08_VBUS_TYPE_OTG	        7

#define REG08_VBUS_TYPE_USB         1
#define REG08_VBUS_TYPE_ADAPTER     3
#define REG08_VBUS_TYPE_OTG         7

#define REG08_CHRG_STAT_MASK        0x18
#define REG08_CHRG_STAT_SHIFT       3
#define REG08_CHRG_STAT_IDLE        0
#define REG08_CHRG_STAT_PRECHG      1
#define REG08_CHRG_STAT_FASTCHG     2
#define REG08_CHRG_STAT_CHGDONE     3

#define REG08_PG_STAT_MASK          0x04
#define REG08_PG_STAT_SHIFT         2
#define REG08_POWER_GOOD            1

#define REG08_THERM_STAT_MASK       0x02
#define REG08_THERM_STAT_SHIFT      1

#define REG08_VSYS_STAT_MASK        0x01
#define REG08_VSYS_STAT_SHIFT       0
#define REG08_IN_VSYS_STAT          1


/* Register 0x09*/
#define SC8960X_REG_09              0x09
#define REG09_FAULT_WDT_MASK        0x80
#define REG09_FAULT_WDT_SHIFT       7
#define REG09_FAULT_WDT             1

#define REG09_FAULT_BOOST_MASK      0x40
#define REG09_FAULT_BOOST_SHIFT     6

#define REG09_FAULT_CHRG_MASK       0x30
#define REG09_FAULT_CHRG_SHIFT      4
#define REG09_FAULT_CHRG_NORMAL     0
#define REG09_FAULT_CHRG_INPUT      1
#define REG09_FAULT_CHRG_THERMAL    2
#define REG09_FAULT_CHRG_TIMER      3

#define REG09_FAULT_BAT_MASK        0x08
#define REG09_FAULT_BAT_SHIFT       3
#define	REG09_FAULT_BAT_OVP		    1

#define REG09_FAULT_NTC_MASK        0x07
#define REG09_FAULT_NTC_SHIFT       0
#define	REG09_FAULT_NTC_NORMAL		0
#define REG09_FAULT_NTC_WARM        2
#define REG09_FAULT_NTC_COOL        3
#define REG09_FAULT_NTC_COLD        5
#define REG09_FAULT_NTC_HOT         6


/* Register 0x0A */
#define SC8960X_REG_0A              0x0A
#define	REG0A_VBUS_GD_MASK			0x80
#define	REG0A_VBUS_GD_SHIFT			7
#define	REG0A_VBUS_GD				1

#define	REG0A_VINDPM_STAT_MASK		0x40
#define	REG0A_VINDPM_STAT_SHIFT		6
#define	REG0A_VINDPM_ACTIVE			1

#define	REG0A_IINDPM_STAT_MASK		0x20
#define	REG0A_IINDPM_STAT_SHIFT		5
#define	REG0A_IINDPM_ACTIVE			1

#define	REG0A_TOPOFF_ACTIVE_MASK	0x08
#define	REG0A_TOPOFF_ACTIVE_SHIFT	3
#define	REG0A_TOPOFF_ACTIVE			1

#define	REG0A_ACOV_STAT_MASK		0x04
#define	REG0A_ACOV_STAT_SHIFT		2
#define	REG0A_ACOV_ACTIVE			1

#define	REG0A_VINDPM_INT_MASK		0x02
#define	REG0A_VINDPM_INT_SHIFT		1
#define	REG0A_VINDPM_INT_ENABLE		0
#define	REG0A_VINDPM_INT_DISABLE	1

#define	REG0A_IINDPM_INT_MASK		0x01
#define	REG0A_IINDPM_INT_SHIFT		0
#define	REG0A_IINDPM_INT_ENABLE		0
#define	REG0A_IINDPM_INT_DISABLE	1

#define	REG0A_INT_MASK_MASK			0x03
#define	REG0A_INT_MASK_SHIFT		0


#define	SC8960X_REG_0B				0x0B
#define	REG0B_REG_RESET_MASK		0x80
#define	REG0B_REG_RESET_SHIFT		7
#define	REG0B_REG_RESET				1

#define REG0B_PN_MASK             	0x78
#define REG0B_PN_SHIFT            	3

#define REG0B_DEV_REV_MASK        	0x03
#define REG0B_DEV_REV_SHIFT       	0

#define SC8960X_REG_0C                  0x0C
#define REG0C_JEITA_COOL_ISET2_MASK   0x80
#define REG0C_JEITA_COOL_ISET2_SHIFT  7

#define REG0C_JEITA_WARM_VSET2_MASK   0x40
#define REG0C_JEITA_WARM_VSET2_SHIFT  6

#define REG0C_JEITA_WARM_ISET_MASK    0x30
#define REG0C_JEITA_WARM_ISET_SHIFT   4

#define REG0C_JEITA_COOL_TEMP_MASK    0x0C
#define REG0C_JEITA_COOL_TEMP_SHIFT   2

#define REG0C_JEITA_WARM_TEMP_MASK    0x03
#define REG0C_JEITA_WARM_TEMP_SHIFT   0


#define SC8960X_REG_0D                  0x0D
#define REG0D_VBAT_REG_FT_MASK        0xC0
#define REG0D_VBAT_REG_FT_SHIFT       6
#define REG0D_VREG_FT_DEFAULT			0
#define REG0D_VREG_FT_INC8MV			1
#define REG0D_VREG_FT_INC16MV			2
#define REG0D_VREG_FT_INC24MV			3

#define REG0D_BOOST_NTC_HOT_TEMP_MASK   0x30
#define REG0D_BOOST_NTC_HOT_TEMP_SHIFT  4

#define REG0D_BOOST_NTC_COOL_TEMP_MASK  0x08
#define REG0D_BOOST_NTC_COOL_TEMP_SHIFT 3

#define REG0D_BOOSTV3_MASK            0x04
#define REG0D_BOOSTV3_SHIFT           2

#define REG0D_BOOSTV0_MASK            0x02
#define REG0D_BOOSTV0_SHIFT           1

#define REG0D_ISHORT_MASK             0x01
#define REG0D_ISHORT_SHIFT            0


#define SC8960X_REG_0E                  0x0E
#define REG0E_VTC_MASK                0x80
#define REG0E_VTC_SHIFT               7

#define REG0E_INPUT_DET_DONE_MASK     0x40
#define REG0E_INPUT_DET_DONE_SHIFT    6

#define REG0E_AUTO_DPDM_EN_MASK       0x20
#define REG0E_AUTO_DPDM_EN_SHIFT      5

#define REG0E_BUCK_FREQ_MASK          0x10
#define REG0E_BUCK_FREQ_SHIFT         4

#define REG0E_BOOST_FREQ_MASK         0x08
#define REG0E_BOOST_FREQ_SHIFT        3

#define REG0E_VSYSOVP_MASK            0x06
#define REG0E_VSYSOVP_SHIFT           1

#define REG0E_NTC_DIS_MASK            0x01
#define REG0E_NTC_DIS_SHIFT           0

/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 start */
#define SC8960X_REG_7D                0x7D
#define REG7D_KEY1                    0x36
#define REG7D_KEY2                    0x48
#define REG7D_KEY3                    0x54
#define REG7D_KEY4                    0x4C

/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
#define SC8960X_REG_80                0x80
#define REG80_DPDM_SINK_MASK          0x02
#define REG80_DPDM_SINK_HIGH          0
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */

#define SC8960X_REG_81                0x81
#define REG81_HVDCP_EN_MASK           0x02
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
#define REG81_DP_DRIVE_MASK           0xE0
#define REG81_DM_DRIVE_MASK           0x1C
/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 start */
#define REG81_DP_DRIVE_06V            0x02
/* hs14 code for AL6528A-1090 by shanxinkai at 2023/02/10 end */
#define REG81_DP_DRIVE_SHIFT          5
#define REG81_DM_DRIVE_SHIFT          2
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
#define REG81_HVDCP_EN_SHIFT          1
#define REG81_HVDCP_ENABLE            1
#define REG81_HVDCP_DISABLE           0
/* hs14 code for SR-AL6528A-01-321 by gaozhengwei at 2022/09/22 end */

/* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 start */
#define SC8960X_REG_7F                0x7F
#define REG7F_KEY1                    0x5A
#define REG7F_KEY2                    0x68
#define REG7F_KEY3                    0x65
#define REG7F_KEY4                    0x6E
#define REG7F_KEY5                    0x67
#define REG7F_KEY6                    0x4C
#define REG7F_KEY7                    0x69
#define REG7F_KEY8                    0x6E

#define REG0E_VAL(val)                \
        (0xB2 - (val & 0x80))

#define SC8960X_REG_92                0x92
#define REG92_PFM_VAL                 0x71

#define SC8960X_REG_93                0x93
#define REG93_VAL(val)                \
        (0x19 + ((val & 0x80) >> 3))

#define SC8960X_REG_94                0x94
#define REG94_VAL                     0x10

#define SC8960X_REG_96                0x96
#define REG96_VAL(val)                \
        (0x09 - ((val & 0x80) >> 7))

#define SC8960X_REG_99                0x99
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 start */
#define REG_99_BC12_enable            0x44
#define REG_99_BC12_disable           0xC4
/* hs14 code for AL6528ADEU-2065|AL6528ADEU-2066 by shanxinkai at 2022/11/16 end */
#define REG99_VAL(val)                \
        (0x4C - ((val & 0x80) >> 4))

#define SC8960X_REG_B8                0xB8
/* hs14 code for AL6528ADEU-28 by gaozhengwei at 2022/09/29 end */

#endif


