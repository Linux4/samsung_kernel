#ifndef __CX25890H_HEADER__
#define __CX25890H_HEADER__

#include <linux/regulator/consumer.h>
#include <linux/extcon-provider.h>
#include <linux/module.h>

#define CONFIG_CX25890H_ENABLE_HVDCP


/* Register 00h */
#define CX25890H_REG_00      		0x00
#define CX25890H_ENHIZ_MASK		    0x80
#define CX25890H_ENHIZ_SHIFT		    7
#define CX25890H_HIZ_ENABLE          1
#define CX25890H_HIZ_DISABLE         0
#define CX25890H_ENILIM_MASK		    0x40
#define CX25890H_ENILIM_SHIFT		6
#define CX25890H_ENILIM_ENABLE       1
#define CX25890H_ENILIM_DISABLE      0

#define CX25890H_IINLIM_MASK		    0x3F
#define CX25890H_IINLIM_SHIFT		0
#define CX25890H_IINLIM_BASE         100
#define CX25890H_IINLIM_LSB          50

/* Register 01h */
#define CX25890H_REG_01		    	0x01
#define CX25890H_BHOT_MASK           0xC0
#define CX25890H_BHOT_SHIFT          6
#define CX25890H_BCOLD_MASK          0x20
#define CX25890H_BCOLD_SHIFT         5
#define CX25890H_VINDPMOS_MASK       0x1F
#define CX25890H_VINDPMOS_SHIFT      0

#define CX25890H_VINDPMOS_BASE       0
#define CX25890H_VINDPMOS_LSB        100


/* Register 0x02 */
#define CX25890H_REG_02              0x02
#define CX25890H_CONV_START_MASK      0x80
#define CX25890H_CONV_START_SHIFT     7
#define CX25890H_CONV_START           1
#define CX25890H_CONV_RATE_MASK       0x40
#define CX25890H_CONV_RATE_SHIFT      6
#define CX25890H_ADC_CONTINUE_ENABLE  1
#define CX25890H_ADC_CONTINUE_DISABLE 0

#define CX25890H_BOOST_FREQ_MASK      0x20
#define CX25890H_BOOST_FREQ_SHIFT     5
#define CX25890H_BOOST_FREQ_1500K     0
#define CX25890H_BOOST_FREQ_500K      0

#define CX25890H_ICOEN_MASK          0x10
#define CX25890H_ICOEN_SHIFT         4
#define CX25890H_ICO_ENABLE          1
#define CX25890H_ICO_DISABLE         0
#define CX25890H_HVDCPEN_MASK        0x08
#define CX25890H_HVDCPEN_SHIFT       3
#define CX25890H_HVDCP_ENABLE        1
#define CX25890H_HVDCP_DISABLE       0
/*
#define CX25890H_MAXCEN_MASK         0x04
#define CX25890H_MAXCEN_SHIFT        2
#define CX25890H_MAXC_ENABLE         1
#define CX25890H_MAXC_DISABLE        0
*/

#define CX25890H_FORCE_DPDM_MASK     0x02
#define CX25890H_FORCE_DPDM_SHIFT    1
#define CX25890H_FORCE_DPDM          1
#define CX25890H_AUTO_DPDM_EN_MASK   0x01
#define CX25890H_AUTO_DPDM_EN_SHIFT  0
#define CX25890H_AUTO_DPDM_ENABLE    1
#define CX25890H_AUTO_DPDM_DISABLE   0


/* Register 0x03*/
#define CX25890H_REG_03              0x03
#define CX25890H_BAT_LOADEN_MASK     0x80
#define CX25890H_BAT_LOAEN_SHIFT     7
#define CX25890H_WDT_RESET_MASK      0x40
#define CX25890H_WDT_RESET_SHIFT     6
#define CX25890H_WDT_RESET           1

#define CX25890H_OTG_CONFIG_MASK     0x20
#define CX25890H_OTG_CONFIG_SHIFT    5
#define CX25890H_OTG_ENABLE          1
#define CX25890H_OTG_DISABLE         0

#define CX25890H_CHG_CONFIG_MASK     0x10
#define CX25890H_CHG_CONFIG_SHIFT    4
#define CX25890H_CHG_ENABLE          1
#define CX25890H_CHG_DISABLE         0


#define CX25890H_SYS_MINV_MASK       0x0E
#define CX25890H_SYS_MINV_SHIFT      1

#define CX25890H_SYS_MINV_BASE       3000
#define CX25890H_SYS_MINV_LSB        100


/* Register 0x04*/
#define CX25890H_REG_04              0x04
#define CX25890H_EN_PUMPX_MASK       0x80
#define CX25890H_EN_PUMPX_SHIFT      7
#define CX25890H_PUMPX_ENABLE        1
#define CX25890H_PUMPX_DISABLE       0
#define CX25890H_ICHG_MASK           0x7F
#define CX25890H_ICHG_SHIFT          0
#define CX25890H_ICHG_BASE           0
#define CX25890H_ICHG_LSB            64


/* Register 0x05*/
#define CX25890H_REG_05              0x05
#define CX25890H_IPRECHG_MASK        0xF0
#define CX25890H_IPRECHG_SHIFT       4
#define CX25890H_ITERM_MASK          0x0F
#define CX25890H_ITERM_SHIFT         0
#define CX25890H_IPRECHG_BASE        64
#define CX25890H_IPRECHG_LSB         64
#define CX25890H_ITERM_BASE          64
#define CX25890H_ITERM_LSB           64
#define CX25890H_ITERM_MAX           1024
#define CX25890H_ITERM_MIN           64


/* Register 0x06*/
#define CX25890H_REG_06              0x06
#define CX25890H_VREG_MASK           0xFC
#define CX25890H_VREG_SHIFT          2
#define CX25890H_BATLOWV_MASK        0x02
#define CX25890H_BATLOWV_SHIFT       1
#define CX25890H_BATLOWV_2800MV      0
#define CX25890H_BATLOWV_3000MV      1
#define CX25890H_VRECHG_MASK         0x01
#define CX25890H_VRECHG_SHIFT        0
#define CX25890H_VRECHG_100MV        0
#define CX25890H_VRECHG_200MV        1
#define CX25890H_VREG_BASE           3840
#define CX25890H_VREG_LSB            16

/* Register 0x07*/
#define CX25890H_REG_07              0x07
#define CX25890H_EN_TERM_MASK        0x80
#define CX25890H_EN_TERM_SHIFT       7
#define CX25890H_TERM_ENABLE         1
#define CX25890H_TERM_DISABLE        0

#define CX25890H_WDT_MASK            0x30
#define CX25890H_WDT_SHIFT           4
#define CX25890H_WDT_DISABLE         0
#define CX25890H_WDT_40S             1
#define CX25890H_WDT_80S             2
#define CX25890H_WDT_160S            3
#define CX25890H_WDT_BASE            0
#define CX25890H_WDT_LSB             40

#define CX25890H_EN_TIMER_MASK       0x08
#define CX25890H_EN_TIMER_SHIFT      3

#define CX25890H_CHG_TIMER_ENABLE    1
#define CX25890H_CHG_TIMER_DISABLE   0

#define CX25890H_CHG_TIMER_MASK      0x06
#define CX25890H_CHG_TIMER_SHIFT     1
#define CX25890H_CHG_TIMER_5HOURS    0
#define CX25890H_CHG_TIMER_8HOURS    1
#define CX25890H_CHG_TIMER_12HOURS   2
#define CX25890H_CHG_TIMER_20HOURS   3

#define CX25890H_JEITA_ISET_MASK     0x01
#define CX25890H_JEITA_ISET_SHIFT    0
#define CX25890H_JEITA_ISET_50PCT    0
#define CX25890H_JEITA_ISET_20PCT    1


/* Register 0x08*/
#define CX25890H_REG_08              0x08
#define CX25890H_BAT_COMP_MASK       0xE0
#define CX25890H_BAT_COMP_SHIFT      5
#define CX25890H_VCLAMP_MASK         0x1C
#define CX25890H_VCLAMP_SHIFT        2
#define CX25890H_TREG_MASK           0x03
#define CX25890H_TREG_SHIFT          0
#define CX25890H_TREG_60C            0
#define CX25890H_TREG_80C            1
#define CX25890H_TREG_100C           2
#define CX25890H_TREG_120C           3

#define CX25890H_BAT_COMP_BASE       0
#define CX25890H_BAT_COMP_LSB        20
#define CX25890H_VCLAMP_BASE         0
#define CX25890H_VCLAMP_LSB          32


/* Register 0x09*/
#define CX25890H_REG_09              0x09
#define CX25890H_FORCE_ICO_MASK      0x80
#define CX25890H_FORCE_ICO_SHIFT     7
#define CX25890H_FORCE_ICO           1
#define CX25890H_TMR2X_EN_MASK       0x40
#define CX25890H_TMR2X_EN_SHIFT      6
#define CX25890H_BATFET_DIS_MASK     0x20
#define CX25890H_BATFET_DIS_SHIFT    5
#define CX25890H_BATFET_OFF          1

#define CX25890H_JEITA_VSET_MASK     0x10
#define CX25890H_JEITA_VSET_SHIFT    4
#define CX25890H_JEITA_VSET_N150MV   0
#define CX25890H_JEITA_VSET_VREG     1
#define CX25890H_BATFET_DLY_MASK     0x08
#define CX25890H_BATFET_DLY_SHIFT    3
#define CX25890H_BATFET_DLY_OFF      0
#define CX25890H_BATFET_DLY_ON       1
#define CX25890H_BATFET_RST_EN_MASK  0x04
#define CX25890H_BATFET_RST_EN_SHIFT 2
#define CX25890H_BATFET_RST_ENABLE	 1
#define CX25890H_BATFET_RST_DISABLE	 0
#define CX25890H_PUMPX_UP_MASK       0x02
#define CX25890H_PUMPX_UP_SHIFT      1
#define CX25890H_PUMPX_UP            1
#define CX25890H_PUMPX_DOWN_MASK     0x01
#define CX25890H_PUMPX_DOWN_SHIFT    0
#define CX25890H_PUMPX_DOWN          1


/* Register 0x0A*/
#define CX25890H_REG_0A              0x0A
#define CX25890H_BOOSTV_MASK         0xF0
#define CX25890H_BOOSTV_SHIFT        4
#define CX25890H_BOOSTV_BASE         4550
#define CX25890H_BOOSTV_LSB          64

#define CX25890H_BOOSTV_SET_4850MV        0x00
#define CX25890H_BOOSTV_SET_5000MV        0x01
#define CX25890H_BOOSTV_SET_5150MV        0x02
#define CX25890H_BOOSTV_SET_5300MV        0x03

#define CX25890H_BOOST_LIM_MASK      0x07
#define CX25890H_BOOST_LIM_SHIFT     0
#define CX25890H_BOOST_LIM_500MA     0x00
#define CX25890H_BOOST_LIM_750MA     0x01
#define CX25890H_BOOST_LIM_1200MA    0x02
#define CX25890H_BOOST_LIM_1400MA    0x03
#define CX25890H_BOOST_LIM_1650MA    0x04
#define CX25890H_BOOST_LIM_1875MA    0x05
#define CX25890H_BOOST_LIM_2150MA    0x06
#define CX25890H_BOOST_LIM_2450MA    0x07


/* Register 0x0B*/
#define CX25890H_REG_0B              0x0B
#define CX25890H_VBUS_STAT_MASK      0xE0
#define CX25890H_VBUS_STAT_SHIFT     5
#define CX25890H_CHRG_STAT_MASK      0x18
#define CX25890H_CHRG_STAT_SHIFT     3
#define CX25890H_CHRG_STAT_IDLE      0
#define CX25890H_CHRG_STAT_PRECHG    1
#define CX25890H_CHRG_STAT_FASTCHG   2
#define CX25890H_CHRG_STAT_CHGDONE   3

#define CX25890H_PG_STAT_MASK        0x04
#define CX25890H_PG_STAT_SHIFT       2
#define CX25890H_SDP_STAT_MASK       0x02
#define CX25890H_SDP_STAT_SHIFT      1
#define CX25890H_VSYS_STAT_MASK      0x01
#define CX25890H_VSYS_STAT_SHIFT     0


/* Register 0x0C*/
#define CX25890H_REG_0C              0x0c
#define CX25890H_FAULT_WDT_MASK      0x80
#define CX25890H_FAULT_WDT_SHIFT     7
#define CX25890H_FAULT_BOOST_MASK    0x40
#define CX25890H_FAULT_BOOST_SHIFT   6
#define CX25890H_FAULT_CHRG_MASK     0x30
#define CX25890H_FAULT_CHRG_SHIFT    4
#define CX25890H_FAULT_CHRG_NORMAL   0
#define CX25890H_FAULT_CHRG_INPUT    1
#define CX25890H_FAULT_CHRG_THERMAL  2
#define CX25890H_FAULT_CHRG_TIMER    3

#define CX25890H_FAULT_BAT_MASK      0x08
#define CX25890H_FAULT_BAT_SHIFT     3
#define CX25890H_FAULT_NTC_MASK      0x07
#define CX25890H_FAULT_NTC_SHIFT     0
#define CX25890H_FAULT_NTC_TSCOLD    1
#define CX25890H_FAULT_NTC_TSHOT     2

#define CX25890H_FAULT_NTC_WARM      2
#define CX25890H_FAULT_NTC_COOL      3
#define CX25890H_FAULT_NTC_COLD      5
#define CX25890H_FAULT_NTC_HOT       6


/* Register 0x0D*/
#define CX25890H_REG_0D              0x0D
#define CX25890H_FORCE_VINDPM_MASK   0x80
#define CX25890H_FORCE_VINDPM_SHIFT  7
#define CX25890H_FORCE_VINDPM_ENABLE 1
#define CX25890H_FORCE_VINDPM_DISABLE 0
#define CX25890H_VINDPM_MASK         0x7F
#define CX25890H_VINDPM_SHIFT        0

#define CX25890H_VINDPM_BASE         2600
#define CX25890H_VINDPM_LSB          100


/* Register 0x0E*/
#define CX25890H_REG_0E              0x0E
#define CX25890H_THERM_STAT_MASK     0x80
#define CX25890H_THERM_STAT_SHIFT    7
#define CX25890H_BATV_MASK           0x7F
#define CX25890H_BATV_SHIFT          0
#define CX25890H_BATV_BASE           2304
#define CX25890H_BATV_LSB            20


/* Register 0x0F*/
#define CX25890H_REG_0F              0x0F
#define CX25890H_ACOV_TH0_MASK		 0x80
#define CX25890H_ACOV_TH0_SHIFT		 7
#define CX25890H_SYSV_MASK           0x7F
#define CX25890H_SYSV_SHIFT          0
#define CX25890H_SYSV_BASE           2304
#define CX25890H_SYSV_LSB            20


/* Register 0x10*/
#define CX25890H_REG_10              0x10
#define CX25890H_ACOV_TH1_MASK		 0x80
#define CX25890H_ACOV_TH1_SHIFT		 7
#define CX25890H_TSPCT_MASK          0x7F
#define CX25890H_TSPCT_SHIFT         0
#define CX25890H_TSPCT_BASE          21
#define CX25890H_TSPCT_LSB           465//should be 0.465,kernel does not support float

/* Register 0x11*/
#define CX25890H_REG_11              0x11
#define CX25890H_VBUS_GD_MASK        0x80
#define CX25890H_VBUS_GD_SHIFT       7
#define CX25890H_VBUSV_MASK          0x7F
#define CX25890H_VBUSV_SHIFT         0
#define CX25890H_VBUSV_BASE          2600
#define CX25890H_VBUSV_LSB           100


/* Register 0x12*/
#define CX25890H_REG_12              0x12
#define CX25890H_ICHGR_MASK          0x7F
#define CX25890H_ICHGR_SHIFT         0
#define CX25890H_ICHGR_BASE          0
#define CX25890H_ICHGR_LSB           50


/* Register 0x13*/
#define CX25890H_REG_13              0x13
#define CX25890H_VDPM_STAT_MASK      0x80
#define CX25890H_VDPM_STAT_SHIFT     7
#define CX25890H_IDPM_STAT_MASK      0x40
#define CX25890H_IDPM_STAT_SHIFT     6
#define CX25890H_IDPM_LIM_MASK       0x3F
#define CX25890H_IDPM_LIM_SHIFT      0
#define CX25890H_IDPM_LIM_BASE       100
#define CX25890H_IDPM_LIM_LSB        50


/* Register 0x14*/
#define CX25890H_REG_14              0x14
#define CX25890H_RESET_MASK          0x80
#define CX25890H_RESET_SHIFT         7
#define CX25890H_RESET               1
#define CX25890H_ICO_OPTIMIZED_MASK  0x40
#define CX25890H_ICO_OPTIMIZED_SHIFT 6
#define CX25890H_PN_MASK             0x38
#define CX25890H_PN_SHIFT            3
#define CX25890H_TS_PROFILE_MASK     0x04
#define CX25890H_TS_PROFILE_SHIFT    2
#define CX25890H_DEV_REV_MASK        0x03
#define CX25890H_DEV_REV_SHIFT       0

/* Register 0x15*/
#define CX25890H_REG_15              0x15
#define CX25890H_DP_VSET_MASK        0xE0
#define CX25890H_DP_VSET_SHIFT       5
#define CX25890H_DM_VSET_MASK        0x1C
#define CX25890H_DM_VSET_SHIFT       2



#define CX25890H_REGISTER_MAX		0x15

#define CX25890H_OVP_5000mV 5000
#define CX25890H_OVP_6000mV 6000
#define CX25890H_OVP_8000mV 8000
#define CX25890H_OVP_10000mV 10000

#define CX25890H_BOOSTV_4850mV 4850
#define CX25890H_BOOSTV_4998mV 4998
#define CX25890H_BOOSTV_5000mV 5000
#define CX25890H_BOOSTV_5126mV 5126
#define CX25890H_BOOSTV_5150mV 5150
#define CX25890H_BOOSTV_5300mV 5300
#define CX25890H_BOOSTV_5318mV 5318


#define CX25890H_VINDPM_3900mV 3900
#define CX25890H_VINDPM_5400mV 5400
#define CX25890H_VINDPM_5900mV 5900
#define CX25890H_VINDPM_7500mV 7500
#define CX25890H_VINDPM_10500mV 10500

#define DEFAULT_PRECHG_CURR	240
#define DEFAULT_TERM_CURR	240
#define DEFAULT_CHG_CURR	500

#define CX25890H_VBUS_NONSTAND_1000MA 1200
#define CX25890H_VBUS_NONSTAND_1500MA 1600
#define CX25890H_VBUS_NONSTAND_2000MA 2000

#define BC12_FLOAT_CHECK_MAX 1

/*+P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/
#define AFC_DETECT_TIME   25
#define QC_DETECT_TIME    10
/*-P231130-06621 liwei19.wt 20231218,reduce the number of AFC and QC identification*/

enum cx25890h_part_no {
	CX25890H = 0x03,
};

enum cx25890h_chrs_status {
	CX25890H_NOT_CHARGING = 0,
	CX25890H_PRE_CHARGE,
	CX25890H_FAST_CHARGING,
	CX25890H_CHARGE_DONE,
};

enum cx25890h_ovp {
	CX25890H_OVP_5500mV,
	CX25890H_OVP_6500mV,
	CX25890H_OVP_10500mV,
	CX25890H_OVP_14000mV,		
};

enum cx25890h_vbus_type {
	CX25890H_VBUS_NONE,
	CX25890H_VBUS_USB_SDP,
	CX25890H_VBUS_USB_CDP,
	CX25890H_VBUS_USB_DCP,
	CX25890H_VBUS_AHVDCP,
	CX25890H_VBUS_UNKNOWN,
	CX25890H_VBUS_NONSTAND,
	CX25890H_VBUS_OTG,
	CX25890H_VBUS_TYPE_NUM,
	CX25890H_VBUS_USB_HVDCP,
	CX25890H_VBUS_USB_HVDCP3,
	CX25890H_VBUS_USB_AFC,
	CX25890H_VBUS_USB_PE,
	CX25890H_VBUS_NONSTAND_1A,
	CX25890H_VBUS_NONSTAND_1P5A,
	CX25890H_VBUS_NONSTAND_2A,
};

static const unsigned int cx25890h_extcon_cable[] = {
	EXTCON_USB,
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

struct cx25890h_wakeup_source {
	struct wakeup_source	*source;
	unsigned long		disabled;
};

struct cx25890h_config {
	bool	enable_auto_dpdm;
/*	bool	enable_12v;*/

	int	charge_voltage;
	int	charge_current;

	bool enable_term;
	int	term_current;

	int prechg_current;

	bool enable_ico;
	bool use_absolute_vindpm;
	bool disable_hvdcp;
	bool enable_ir_compensation;
};

struct cx25890h_device {
	struct device *dev;
	struct i2c_client *client;

	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**afc_ext_iio_chans;
	struct iio_channel	**wtchg_ext_iio_chans;
	struct iio_channel	**pmic_ext_iio_chans;
	struct iio_channel	**qg_batt_ext_iio_chans;

	struct cx25890h_wakeup_source	cx25890h_wake_source;
	struct cx25890h_wakeup_source	cx25890h_apsd_wake_source;

	struct regulator	*dpdm_reg;
	bool			dpdm_enabled;

	struct cx25890h_config  cfg;
	struct delayed_work irq_work;
	struct delayed_work monitor_work;
	struct delayed_work rerun_apsd_work;
	struct delayed_work detect_work;
	struct delayed_work check_adapter_work;

	enum cx25890h_part_no part_no;
	int revision;
	bool is_cx25890h;

	unsigned int status;
	int usb_type;
	int vbus_type;
	int pre_vbus_type;

	int bc12_done;
	int usb_online;
	int pre_usb_online;
	int apsd_rerun;
	int bc12_cnt;
	int hvdcp_det;
	int init_detect;
	int bc12_float_check;

	int vbus_good;
	bool vindpm_stat;

	int chg_status;
	bool enabled;

	int chg_en_gpio;
	int irq_gpio;
	int otg_en_gpio;

	int pulse_cnt;

	bool hiz_mode;

	//+ bug, tankaikun@wt, add 20220701, charger bring up
	struct extcon_dev	*extcon;
	int notify_usb_flag;
	//- bug, tankaikun@wt, add 20220701, charger bring up

	struct mutex	resume_complete_lock;
	int resume_completed;

	//+bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	unsigned long dynamic_adjust_charge_update_timer;
	int final_cc;
	int final_cv;
	int tune_cv;
	int adjust_cnt;
	//-bug816469,tankaikun.wt,add,2023/1/30, IR compensation
	int afc_type;

};

int cx25890h_get_vbus_volt(struct cx25890h_device *cx_chg,  int *val);
int cx25890h_get_vbat_volt(struct cx25890h_device *cx_chg,  int *val);
int cx25890h_get_ibat_curr(struct cx25890h_device *cx_chg,  int *val);

MODULE_LICENSE("GPL v2");

#endif

