/*
 * SPDX-License-Identifier: GPL-2.0-only.
 * Copyright(c) Bigmtech, 2021. All rights reserved.
 *
 */

#ifndef SD76030_H_
#define SD76030_H_

#define DEFAULT_ILIMIT     3000
#define DEFAULT_VLIMIT		4500
#define DEFAULT_CC			2000
#define DEFAULT_CV			4400
#define DEFAULT_IPRECHG		540
#define DEFAULT_ITERM		180
#define DEFAULT_VSYS_MIN	3500
#define DEFAULT_RECHG		100
#define DEFAULT_V_WAKEUP   2900

#define SD76030_REG_NUM 17

#define SD76030_R00		0x00
#define SD76030_R01		0x01
#define SD76030_R02		0x02
#define SD76030_R03		0x03
#define SD76030_R04		0x04
#define SD76030_R05		0x05
#define SD76030_R06		0x06
#define SD76030_R07		0x07
#define SD76030_R08		0x08
#define SD76030_R09		0x09
#define SD76030_R0A		0x0A
#define SD76030_R0B		0x0B
#define SD76030_R0C		0x0C
#define SD76030_R0D		0x0D
#define SD76030_R0E		0x0E
#define SD76030_R0F		0x0F
#define SD76030_R10		0x10

#define SD76030_COMPINENT_ID	0x03A1

//#define SD76030_TEMPERATURE_ENABLE
#define SD76030_R_PULL   5230     //5.23K
#define SD76030_R_DOWN   30100    //30.1K
#define TEMPERATURE_DATA_NUM 28
typedef struct tag_one_latitude_data {
   int32_t x;//
   int32_t y;//
} one_latitude_data_t;

/* CON0 */
#define CON0_EN_HIZ_MASK	0x1
#define CON0_EN_HIZ_SHIFT	7

#define CON0_EN_ICHG_MON_MASK	0x3
#define CON0_EN_ICHG_MON_SHIFT	5

#define CON0_IINLIM_MASK	0x1F
#define CON0_IINLIM_SHIFT	0

#define INPUT_CURRT_STEP	((uint32_t)100)
#define INPUT_CURRT_MAX	((uint32_t)3200)
#define INPUT_CURRT_MIN	((uint32_t)100)

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
#define ICHG_CURR_MAX		((uint32_t)3780)

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

/*CON11*/
#define SD76030_R11		      0x11
#define SD76030_R12		      0x12
#define SD76030_R13		      0x13
#define SD76030_R14		      0x14
#define SD76030_R15		      0x15
#define SD76030_R16		      0x16
#define SD76030_R17		      0x17

#define PROFILE_CHG_VOTER		"PROFILE_CHG_VOTER"
#define MAIN_SET_VOTER			"MAIN_SET_VOTER"
#define BMS_FC_VOTER			"BMS_FC_VOTER"
#define DCP_CHG_VOTER			"DCP_CHG_VOTER"

#define SD76030_MAX_ICL 		3000
#define SD76030_MAX_FCC 		3780

#define CHG_FCC_CURR_MAX		3600
#define CHG_ICL_CURR_MAX		2020
#define DCP_ICL_CURR_MAX		2000

#define CHG_CDP_CURR_MAX        1500
#define CHG_SDP_CURR_MAX        500
#define CHG_AFC_CURR_MAX        3000
#define CHG_DCP_CURR_MAX        2000

#define CON11_START_ADC_MASK  0x01
#define CON11_START_ADC_SHIFT 7
#define CON11_CONV_RATE_MASK  0x01
#define CON11_CONV_RATE_SHIFT 6

#define AFC_QUICK_CHARGE_POWER_CODE          0x46
#define AFC_COMMIN_CHARGE_POWER_CODE         0x08


#define SD76030_ADC_CHANNEL_START SD76030_R12
enum sd76030_adc_channel
{
   error_channel= -1,
   vbat_channel =  0,
   vsys_channel =  1,
   tbat_channel =  2,
   vbus_channel =  3,
   ibat_channel =  4,
   ibus_channel =  5,
   max_channel,
};

const uint32_t sd76030_extcon_cables[] =
{
//	EXTCON_USB,
//	EXTCON_CHG_USB_SDP,
//	EXTCON_CHG_USB_CDP,
//	EXTCON_CHG_USB_DCP,
//	EXTCON_CHG_USB_FAST,
//	EXTCON_NONE,
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

enum sd76030_port_stat
{
	SD76030_PORTSTAT_NO_INPUT = 0,
	SD76030_PORTSTAT_SDP,
	SD76030_PORTSTAT_CDP,
	SD76030_PORTSTAT_DCP,
	SD76030_PORTSTAT_HVDCP,
	SD76030_PORTSTAT_UNKNOWN,
	SD76030_PORTSTAT_NON_STANDARD,
	SD76030_PORTSTAT_OTG,
	SD76030_PORTSTAT_MAX,
};

enum sd76030_iio_type {
	DS28E16,
	BMS,
	NOPMI,
};

struct sd76030_chip
{
	struct device *dev;
	struct i2c_client * client;
	struct mutex i2c_rw_lock;
	struct mutex adc_lock;
	struct mutex irq_lock;
	const char *chg_dev_name;
	struct delayed_work sd76030_work;

	struct extcon_dev *edev;
	struct mutex bc12_lock;
	atomic_t vbus_gd;
	bool attach;
	enum sd76030_port_stat port;
	struct power_supply *psy;
	struct power_supply * sd76030_psy;
	//enum power_supply_type chg_type;
	enum power_supply_type  chg_type;
	int psy_usb_type;
	struct delayed_work psy_dwork;

	int charge_afc;

	struct regulator_desc sd76030_otg_rdesc;
	struct regulator_init_data *init_data;
	struct regulator_dev* sd76030_reg_dev;
	struct power_supply_config chg_cfg;
	struct power_supply_desc chg_desc;
#ifdef TRIGGER_CHARGE_TYPE_DETECTION
	wait_queue_head_t bc12_en_req;
	atomic_t bc12_en_req_cnt;
	struct task_struct *bc12_en_kthread;
#endif
	int32_t component_id;
	int32_t irq_gpio;
	int32_t irq;
	int32_t ce_gpio;
	uint32_t chg_state;
	uint32_t ilimit_ma;
	uint32_t vlimit_mv;
	uint32_t cc_ma;
	uint32_t cv_mv;
	uint32_t pre_ma;
	uint32_t eoc_ma;
	uint32_t rechg_mv;
	uint32_t vsysmin_mv;
	uint32_t target_hv;

	uint32_t trickle_charger_mv;
	bool 	is_trickle_charging;
	struct regulator *dpdm_reg;
	bool dpdm_enabled;
	struct mutex dpdm_lock;
	struct power_supply * sd76030_usb_port_descpsy;

	//typec / otg
	struct delayed_work tcpc_dwork;
	struct tcpc_device *tcpc;
	struct notifier_block tcp_nb;
	struct power_supply	*usb_psy;
	/* longcheer add iio start */
	struct iio_dev  		*indio_dev;
	struct iio_chan_spec	*iio_chan;
	struct iio_channel		*int_iio_chans;
	struct iio_channel		**ds_ext_iio_chans;
	struct iio_channel		**fg_ext_iio_chans;
	struct iio_channel		**nopmi_chg_ext_iio_chans;

	struct votable			*fcc_votable;
	struct votable			*fv_votable;
	struct votable			*usb_icl_votable;
	struct votable			*chg_dis_votable;
	/* longcheer add iio end */

	//afc
	struct delayed_work hvdcp_dwork;
};

/*set cv func*/
extern int32_t sd76030_set_vreg_volt(struct sd76030_chip *chip, uint32_t vreg_chg_vol);
/*set ichg func*/
extern int32_t sd76030_set_ichg_current(struct sd76030_chip *chip, uint32_t ichg);
/*set ilimit func*/
extern int32_t sd76030_set_input_curr_lim(struct sd76030_chip *chip, uint32_t ilimit_ma);
/*get charger status func*/
extern int32_t sd76030_get_charger_status(struct sd76030_chip *chip);
extern int afc_set_voltage_workq(unsigned int afc_code);
#endif // _sd76030_SW_H_

