#ifndef __CX2560X_HEADER__
#define __CX2560X_HEADER__


#include <linux/power_supply.h>


/* Register 00h */
#define CX2560X_REG_00      		0x00
#define REG00_ENHIZ_MASK		    0x80
#define REG00_ENHIZ_SHIFT		    7
#define	REG00_HIZ_ENABLE			1
#define	REG00_HIZ_DISABLE			0

#define	REG00_DPDM_DIS_MASK			0x40
#define REG00_DPDM_DIS_SHIFT		6
#define	REG00_DPDM_DIS_ENABLE		0
#define	REG00_DPDM_DIS_DISABLE		1

#define	REG00_STAT_CTRL_MASK		0x20
#define REG00_STAT_CTRL_SHIFT		5
#define	REG00_STAT_ENABLE			0
#define	REG00_STAT_DISABLE			1


#define REG00_IINLIM_MASK		    0x1F
#define REG00_IINLIM_SHIFT			0
#define	REG00_IINLIM_LSB			100
#define	REG00_IINLIM_BASE			100
#define	REG00_IINLIM_MAX			3200

/* Register 01h */
#define CX2560X_REG_01		    	0x01
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
#define CX2560X_REG_02              0x02
#define	REG02_BOOST_LIM_MASK		0x80
#define	REG02_BOOST_LIM_SHIFT		7
#define	REG02_BOOST_LIM_0P5A		0
#define	REG02_BOOST_LIM_1P2A		1

#define	REG02_Q1_FULLON_MASK		0x40
#define	REG02_Q1_FULLON_SHIFT		6
#define	REG02_Q1_FULLON_ENABLE		1
#define	REG02_Q1_FULLON_DISABLE		0

#define REG02_ICHG_MASK           	0x3F
#define REG02_ICHG_SHIFT          	0
#define REG02_ICHG_BASE           	0
#define REG02_ICHG_LSB            	60

/* Register 0x03*/
#define CX2560X_REG_03              0x03
#define REG03_IPRECHG_MASK        	0xF0
#define REG03_IPRECHG_SHIFT       	4
#define REG03_IPRECHG_BASE        	52
#define REG03_IPRECHG_LSB         	52
#define REG03_IPRECHG_MAX			676

#define REG03_ITERM_MASK          	0x0F
#define REG03_ITERM_SHIFT         	0
#define REG03_ITERM_BASE          	60
#define REG03_ITERM_MAX				780
#define REG03_ITERM_LSB           	60


/* Register 0x04*/
#define CX2560X_REG_04              0x04
#define REG04_VREG_MASK           	0xF8
#define REG04_VREG_SHIFT          	3
#define REG04_VREG_BASE           	3856
#define REG04_VREG_MAX           	4624
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
#define CX2560X_REG_05             	0x05
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
#define	REG05_TREG_100C				0
#define	REG05_TREG_120C				1

#define REG05_JEITA_ISET_MASK     	0x01
#define REG05_JEITA_ISET_SHIFT    	0
#define REG05_JEITA_ISET_50PCT    	0
#define REG05_JEITA_ISET_20PCT    	1


/* Register 0x06*/
#define CX2560X_REG_06              0x06
#define	REG06_OVP_MASK				0xC0
#define	REG06_OVP_SHIFT				6
#define	REG06_OVP_5P5V				0
#define	REG06_OVP_6P5V				1
#define	REG06_OVP_10P5V				2
#define	REG06_OVP_14P0V				3

#define	REG06_BOOSTV_MASK			0x30
#define	REG06_BOOSTV_SHIFT			4
#define	REG06_BOOSTV_4P87V			0
#define	REG06_BOOSTV_4P998V			1
#define	REG06_BOOSTV_5P126V			2
#define	REG06_BOOSTV_5P254V			3

#define	REG06_VINDPM_MASK			0x0F
#define	REG06_VINDPM_SHIFT			0
#define	REG06_VINDPM_BASE			3900
#define	REG06_VINDPM_LSB			100

/* Register 0x07*/
#define CX2560X_REG_07              0x07
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
#define REG07_JEITA_VSET_MNUS_200MV 0
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
#define CX2560X_REG_08              0x08
#define REG08_VBUS_STAT_MASK      0xE0           
#define REG08_VBUS_STAT_SHIFT     5
#define REG08_VBUS_TYPE_NONE	  0
#define REG08_VBUS_TYPE_USB_SDP   1
#define REG08_VBUS_TYPE_USB_CDP   2
#define REG08_VBUS_TYPE_USB_DCP   3
#define REG08_VBUS_TYPE_UNKNOWN   5
#define REG08_VBUS_TYPE_NON_STANDARD 6
#define REG08_VBUS_TYPE_OTG       7

#define REG08_CHRG_STAT_MASK      0x18
#define REG08_CHRG_STAT_SHIFT     3
#define REG08_CHRG_STAT_IDLE      0
#define REG08_CHRG_STAT_PRECHG    1
#define REG08_CHRG_STAT_FASTCHG   2
#define REG08_CHRG_STAT_CHGDONE   3

#define REG08_PG_STAT_MASK        0x04
#define REG08_PG_STAT_SHIFT       2
#define REG08_POWER_GOOD          1
#define REG08_POWER_NOT_GOOD      0

#define REG08_THERM_STAT_MASK     0x02
#define REG08_THERM_STAT_SHIFT    1
#define REG08_THERM_NORMAL        0
#define REG08_THERM_IN_REG        1

#define REG08_VSYS_STAT_MASK      0x01
#define REG08_VSYS_STAT_SHIFT     0
#define REG08_IN_VSYSMIN_STAT     1
#define REG08_NOT_IN_VSYSMIN_STAT 0


/* Register 0x09*/
#define CX2560X_REG_09              0x09
#define REG09_FAULT_WDT_MASK      0x80
#define REG09_FAULT_WDT_SHIFT     7
#define REG09_FAULT_WDT           1

#define REG09_FAULT_BOOST_MASK    0x40
#define REG09_FAULT_BOOST_SHIFT   6
#define REG09_FAULT_BOOST         1

#define REG09_FAULT_CHRG_MASK     0x30
#define REG09_FAULT_CHRG_SHIFT    4
#define REG09_FAULT_CHRG_NORMAL   0
#define REG09_FAULT_CHRG_INPUT    1
#define REG09_FAULT_CHRG_THERMAL  2
#define REG09_FAULT_CHRG_TIMER    3

#define REG09_FAULT_BAT_MASK      0x08
#define REG09_FAULT_BAT_SHIFT     3
#define	REG09_FAULT_BAT_OVP		1

#define REG09_FAULT_NTC_MASK      0x07
#define REG09_FAULT_NTC_SHIFT     0
#define	REG09_FAULT_NTC_NORMAL	  0
#define REG09_FAULT_NTC_WARM      2
#define REG09_FAULT_NTC_COOL      3
#define REG09_FAULT_NTC_COLD      5
#define REG09_FAULT_NTC_HOT       6


/* Register 0x0A */
#define CX2560X_REG_0A              0x0A
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


/* Register 0x0B */
#define	CX2560X_REG_0B				0x0B
#define	REG0B_REG_RESET_MASK		0x80
#define	REG0B_REG_RESET_SHIFT		7
#define	REG0B_REG_RESET				1

#define REG0B_PN_MASK             	0x78
#define REG0B_PN_SHIFT            	3
#define REG0B_PN_CX25600        0
#define REG0B_PN_CX25600D       1
#define REG0B_PN_CX25601        2
#define REG0B_PN_CX25601D       7

#define REG0B_DEV_REV_MASK        	0x03
#define REG0B_DEV_REV_SHIFT       	0

/* Register 0x0C */
#define CX2560X_REG_0C				0x0C
#define REG0C_BOOST_FREQ_MASK       0x80
#define REG0C_BOOST_FREQ_SHIFT      7
#define REG0C_BOOST_FREQ_1P5M       0
#define REG0C_BOOST_FREQ_500K       1

#define REG0C_BCOLD_MASK        0x40
#define REG0C_BCOLD_SHIFT       6
#define REG0C_BCOLD_77PCT       0
#define REG0C_BCOLD_80PCT       1

#define REG0C_BHOT_MASK         0x30
#define REG0C_BHOT_SHIFT        4
#define REG0C_BHOT_34P75PCT     0
#define REG0C_BHOT_37P75PCT     1
#define REG0C_BHOT_31P25PCT     2
#define REG0C_BHOT_DISABLE      3

#define REG0C_ICO_EN_MASK       0x01
#define REG0C_ICO_EN_SHIFT      0
#define REG0C_ICO_DISABLE       0
#define REG0C_ICO_ENABLE        1

/* Register 0x0D */
#define CX2560X_REG_0D				0x0D
#define REG0D_FORCE_ICO_MASK        0x80
#define REG0D_FORCE_ICO_SHIFT       7
#define REG0D_FORCE_ICO             1

#define REG0D_ICO_OPTIMIZED_MASK    0x40
#define REG0D_ICO_OPTIMIZED_SHIFT   6
#define REG0D_IN_ICO                0
#define REG0D_ICO_DONE              1

#define REG0D_IDPM_LIM_MASK     0x3F
#define REG0D_IDPM_LIM_SHIFT    0
#define REG0D_IDPM_LIM_BASE     100
#define REG0D_IDPM_LIM_LSB      50

/* Register 0x0E */
#define CX2560X_REG_0E				0x0E
#define REG0E_VREG_MASK         0xFE
#define REG0E_VREG_SHIFT        1
#define REG0E_VREG_BASE         3856
#define REG0E_VREG_LSB          8

#define REG0E_BAT_LOADEN_MASK   0x01
#define REG0E_BAT_LOADEN_SHIFT  0
#define REG0E_BAT_LOAD_ENABLE   1
#define REG0E_BAT_LOAD_DISABLE  0

/* Register 0x0F */
#define CX2560X_REG_0F				0x0F
#define REG0F_TREG_MASK         0xC0
#define REG0F_TREG_SHIFT        6
#define REG0F_TREG_60C          0
#define REG0F_TREG_80C          1
#define REG0F_TREG_100C         2
#define REG0F_TREG_120C         3

#define REG0F_BAT_COMP_MASK     0x38
#define REG0F_BAT_COMP_SHIFT    3
#define REG0F_BAT_COMP_BASE     0
#define REG0F_BAT_COMP_LSB      20

#define REG0F_VCLAMP_MASK       0x07
#define REG0F_VCLAMP_SHIFT      0
#define REG0F_VCLAMP_BASE       0
#define REG0F_VCLAMP_LSB        32

/* Register 0x10 */
#define CX2560X_REG_10				0x10
#define REG10_EN_12V_MASK       0x80
#define REG10_EN_12V_SHIFT      7
#define REG10_EN_HVDCP_12V      1
#define REG10_EN_HVDCP_9V       0

#define REG10_DP_DAC_MASK       0x70
#define REG10_DP_DAC_SHIFT      4
#define REG10_DP_DAC_HIZ        0
#define REG10_DP_DAC_0V         1
#define REG10_DP_DAC_0P6V       2
#define REG10_DP_DAC_1P2V       3
#define REG10_DP_DAC_2V         4
#define REG10_DP_DAC_2P7V       5
#define REG10_DP_DAC_3P3V       6

#define REG10_HVDCP_EN_MASK     0x08
#define REG10_HVDCP_EN_SHIFT    3
#define REG10_HVDCP_ENABLE      1
#define REG10_HVDCP_DISABLE     0

#define REG10_DM_DAC_MASK       0x07
#define REG10_DM_DAC_SHIFT      0
#define REG10_DM_DAC_HIZ        0
#define REG10_DM_DAC_0V         1
#define REG10_DM_DAC_0P6V       2
#define REG10_DM_DAC_1P2V       3
#define REG10_DM_DAC_2V         4
#define REG10_DM_DAC_2P7V       5
#define REG10_DM_DAC_3P3V       6

/* Register 0x11 */
#define CX2560X_REG_11			0x11
#define REG11_VINDPM_MASK       0x7F
#define REG11_VINDPM_SHIFT      0
#define REG11_VINDPM_BASE       3900
#define REG11_VINDPM_MAX       14200
#define REG11_VINDPM_LSB        100


enum charg_stat {
	CHRG_STAT_NOT_CHARGING,
	CHRG_STAT_PRE_CHARGINT,
	CHRG_STAT_FAST_CHARGING,
	CHRG_STAT_CHARGING_TERM,
};

enum wt_charger_type {
	CHARGER_UNKNOWN = 0,
	STANDARD_HOST,          /* USB : 450mA */
	CHARGING_HOST,
	NONSTANDARD_CHARGER,    /* AC : 450mA~1A */
	STANDARD_CHARGER,       /* AC : ~1A */
	APPLE_2_4A_CHARGER, /* 2.4A apple charger */
	APPLE_2_1A_CHARGER, /* 2.1A apple charger */
	APPLE_1_0A_CHARGER, /* 1A apple charger */
	APPLE_0_5A_CHARGER, /* 0.5A apple charger */
	SAMSUNG_CHARGER,
	WIRELESS_CHARGER,
};


enum vboost {
	BOOSTV_4870 = 4870,
	BOOSTV_4998 = 4998,
	BOOSTV_5126 = 5126,
	BOOSTV_5254 = 5254,
};

enum vac_ovp {
	VAC_OVP_5500 = 5500,
	VAC_OVP_6500 = 6500,
	VAC_OVP_10500 = 10500,
	VAC_OVP_14000 = 14000,
};

enum iboost {
	BOOSTI_500 = 500,
	BOOSTI_1200 = 1200,
};

enum pmic_iio_channels {
        VBUS_VOLTAGE,
};

static const char * const pmic_iio_chan_name[] = {
	[VBUS_VOLTAGE] = "vbus_dect",
};

struct cx2560x_state {
    u32 vbus_stat;
    u32 chrg_stat;
    u32 pg_stat;
    u32 therm_stat;
    u32 vsys_stat;
    u32 wdt_fault;
    u32 boost_fault;
    u32 chrg_fault;
    u32 bat_fault;
    u32 ntc_fault;
	bool online;
	bool vbus_gd;
};

struct cx2560x_charge_param {
	int vlim;
	int ilim;
	int ichg;
	int vreg;
};

enum stat_ctrl {
	STAT_CTRL_STAT,
	STAT_CTRL_ICHG,
	STAT_CTRL_INDPM,
	STAT_CTRL_DISABLE,
};

struct cx2560x_platform_data {
	struct cx2560x_charge_param usb;
	int iprechg;
	int iterm;

	enum stat_ctrl statctrl;
	enum vboost boostv;	// options are 4850,
	enum iboost boosti; // options are 500mA, 1200mA
	enum vac_ovp vac_ovp;

};

#endif
